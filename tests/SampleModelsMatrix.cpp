// Matrix smoke for dist/sample_models: static + Release Load + short lifecycle.
// Build: cmake --build build --config Release --target SampleModelsMatrix
#include "../src/core/PackageScanner.h"
#include "../src/core/HeaderAnalyzer.h"
#include "../src/core/LibAnalyzer.h"
#include "../src/core/PeAnalyzer.h"
#include "../src/core/DllLoader.h"
#include "../src/core/PrecheckSummary.h"
#include "../src/utils/QtEncoding.h"
#include "../src/utils/MemoryUtils.h"
#include "../src/utils/SehHelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <iostream>

namespace {

WeaponModelParams MakeParams(uint32_t seed) {
    WeaponModelParams p{};
    p.init_lat = 30.0 + (seed % 10) * 0.1;
    p.init_lon = 110.0 + (seed % 10) * 0.1;
    p.init_alt = 1000.0;
    p.init_speed = 300.0;
    p.init_heading = 45.0;
    p.init_pitch = 0.0;
    p.init_roll = 0.0;
    p.step_dt = 0.02;
    return p;
}

std::string PickReleaseDll(const ModelPackageFiles& pkg) {
    if (!pkg.releaseDllFiles.empty()) return pkg.releaseDllFiles.front();
    for (const auto& d : pkg.allDllFiles) {
        QString low = qUtf8(d).toLower();
        QFileInfo fi(qUtf8(d));
        if (low.contains("\\debug\\") || low.contains("/debug/")) continue;
        if (fi.completeBaseName().endsWith(QLatin1Char('d'), Qt::CaseInsensitive)
            && fi.completeBaseName().size() > 1) {
            // likely *d.dll debug — skip if a non-d sibling exists
            continue;
        }
        return d;
    }
    return pkg.allDllFiles.empty() ? std::string() : pkg.allDllFiles.front();
}

struct Row {
    QString name;
    QString buildCfg;
    QString pe;
    QString load;
    QString life;
    QString dualCreate;
    QString notes;
};

QString Yn(bool ok) { return ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"); }

Row TestPackage(const QString& pkgDir) {
    Row row;
    row.name = QDir(pkgDir).dirName();
    ModelPackageFiles pkg = PackageScanner::ScanPackageDirectory(qToUtf8(pkgDir));
    const BuildConfigCapability cap =
        PrecheckSummary::EvaluateBuildConfig(pkg, qToUtf8(row.name));
    row.buildCfg = QStringLiteral("R=%1 D=%2")
        .arg(qUtf8(cap.releaseVerdict), qUtf8(cap.debugVerdict));

    int hdrOk = 0;
    for (const auto& h : pkg.allHeaderFiles) {
        if (HeaderAnalyzer::AnalyzeHeader(h).overallPass) ++hdrOk;
    }
    HeaderConflictReport setRep = HeaderAnalyzer::AnalyzeHeaderSet(pkg.allHeaderFiles);

    int libOk = 0;
    for (const auto& l : pkg.allLibFiles) {
        if (LibAnalyzer::AnalyzeLib(l).overallPass) ++libOk;
    }

    int peOk = 0;
    std::string crt;
    for (const auto& d : pkg.allDllFiles) {
        PeAnalysisReport pe = PeAnalyzer::AnalyzeDll(d, { qToUtf8(pkgDir) }, {});
        if (pe.overallPass) ++peOk;
        if (crt.empty()) crt = pe.crtLinkage;
    }
    row.pe = QStringLiteral("%1/%2 crt=%3")
        .arg(peOk).arg(pkg.allDllFiles.size()).arg(qUtf8(crt));

    const std::string dll = PickReleaseDll(pkg);
    if (dll.empty()) {
        row.load = QStringLiteral("N/A");
        row.life = QStringLiteral("N/A");
        row.dualCreate = QStringLiteral("N/A");
        row.notes = QStringLiteral("no dll");
        return row;
    }

    DllLoader loader;
    LoadResult lr = loader.Load(dll, InterfaceMapping::DefaultHandleBased());
    row.load = lr.isLoaded
        ? QStringLiteral("PASS bound=%1").arg(lr.boundSymbolCount)
        : QStringLiteral("FAIL %1").arg(qDecodeLog(lr.errorLog).left(80));

    if (!lr.isLoaded) {
        row.life = QStringLiteral("SKIP");
        row.dualCreate = QStringLiteral("SKIP");
        row.notes = QStringLiteral("hdrPass=%1/%2 setPass=%3 lib=%4/%5")
            .arg(hdrOk).arg(pkg.allHeaderFiles.size())
            .arg(setRep.overallPass ? 1 : 0)
            .arg(libOk).arg(pkg.allLibFiles.size());
        return row;
    }

    CallContext ctx;
    ctx.params = MakeParams(1);
    ctx.handleSlots.assign(4, nullptr);
    std::string err;
    const bool lifeOk = loader.RunLifecycle(ctx, 8, err);
    row.life = lifeOk
        ? QStringLiteral("PASS")
        : QStringLiteral("FAIL %1").arg(qDecodeLog(err).left(80));

    // Dual Create on one module (multi-instance smoke).
    loader.Unload();
    lr = loader.Load(dll, InterfaceMapping::DefaultHandleBased());
    bool dualOk = false;
    QString dualDetail;
    if (lr.isLoaded) {
        FnModelCreate createFn = loader.GetCreateFn();
        FnModelDestroyEx destroyFn = loader.GetDestroyExFn();
        if (!createFn) {
            dualDetail = QStringLiteral("no Create export");
        } else {
            ModelHandle h1 = nullptr;
            ModelHandle h2 = nullptr;
            DWORD e1 = 0, e2 = 0;
            const bool c1 = SafeCallCreate(createFn, &h1, &e1) && h1;
            const bool c2 = SafeCallCreate(createFn, &h2, &e2) && h2;
            dualOk = c1 && c2 && h1 != h2 && e1 == 0 && e2 == 0;
            dualDetail = QStringLiteral("c1=%1 c2=%2 seh1=0x%3 seh2=0x%4 same=%5")
                .arg(c1 ? 1 : 0).arg(c2 ? 1 : 0)
                .arg(e1, 0, 16).arg(e2, 0, 16)
                .arg(h1 && h1 == h2 ? 1 : 0);
            if (destroyFn) {
                DWORD de = 0;
                if (h1) SafeCallDestroyEx(destroyFn, h1, &de);
                if (h2) SafeCallDestroyEx(destroyFn, h2, &de);
            }
        }
    }
    row.dualCreate = dualOk ? QStringLiteral("PASS ") + dualDetail
                            : QStringLiteral("FAIL ") + dualDetail;

    // Light multithread lifecycle (may not catch silent races).
    if (row.name.startsWith(QStringLiteral("ThreadUnsafe"))
        || row.name.startsWith(QStringLiteral("TcPass"))) {
        std::atomic<int> fail{0};
        std::atomic<int> seh{0};
        auto worker = [&](int id) {
            DllLoader thr;
            if (!thr.Load(dll, InterfaceMapping::DefaultHandleBased()).isLoaded) {
                ++fail;
                return;
            }
            CallContext tctx;
            tctx.params = MakeParams(static_cast<uint32_t>(10 + id));
            tctx.handleSlots.assign(4, nullptr);
            std::string terr;
            if (!thr.RunLifecycle(tctx, 20, terr)) {
                ++fail;
                if (terr.find("SEH") != std::string::npos) ++seh;
            }
        };
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) threads.emplace_back(worker, i);
        for (auto& t : threads) t.join();
        row.notes += QStringLiteral(" mt4 fail=%1 seh=%2;")
            .arg(fail.load()).arg(seh.load());
    }

    if (row.name.startsWith(QStringLiteral("MemLeak"))) {
        DllLoader ml;
        if (ml.Load(dll, InterfaceMapping::DefaultHandleBased()).isLoaded) {
            const auto before = MemoryUtils::GetCurrentProcessMemory();
            CallContext mctx;
            mctx.params = MakeParams(9);
            mctx.handleSlots.assign(4, nullptr);
            std::string merr;
            ml.RunLifecycle(mctx, 200, merr);
            const auto after = MemoryUtils::GetCurrentProcessMemory();
            const double dMB = (after.privateUsageBytes > before.privateUsageBytes)
                ? (after.privateUsageBytes - before.privateUsageBytes) / (1024.0 * 1024.0)
                : (after.workingSetBytes - before.workingSetBytes) / (1024.0 * 1024.0);
            row.notes += QStringLiteral(" memDelta~%1MB@200step;").arg(dMB, 0, 'f', 2);
        }
    }

    row.notes += QStringLiteral(" hdr=%1/%2 set=%3 lib=%4/%5")
        .arg(hdrOk).arg(pkg.allHeaderFiles.size())
        .arg(setRep.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
        .arg(libOk).arg(pkg.allLibFiles.size());
    loader.Unload();
    return row;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QString root = QCoreApplication::applicationDirPath() + "/sample_models";
    if (argc >= 2) root = QString::fromLocal8Bit(argv[1]);

    QTextStream out(stdout);
    out.setCodec("UTF-8");
    out << "ROOT=" << root << endl;
    out << "time=" << QDateTime::currentDateTime().toString(Qt::ISODate) << endl;

    QDir dir(root);
    if (!dir.exists()) {
        out << "FAIL: missing root" << endl;
        return 2;
    }

    // Cross-header pair checks
    auto pairConflict = [&](const QString& a, const QString& b) {
        std::vector<std::string> paths;
        for (const QString& name : { a, b }) {
            ModelPackageFiles pkg =
                PackageScanner::ScanPackageDirectory(qToUtf8(dir.filePath(name)));
            paths.insert(paths.end(), pkg.allHeaderFiles.begin(), pkg.allHeaderFiles.end());
        }
        HeaderConflictReport r = HeaderAnalyzer::AnalyzeHeaderSet(paths);
        out << "PAIR " << a << "+" << b
            << " conflictPass=" << (r.overallPass ? "PASS" : "FAIL")
            << " dup=" << r.duplicateTypeCount
            << " odr=" << r.odrConflictCount
            << " ns=" << r.namespacePollutionCount << endl;
    };

    out << "==== PAIR HEADER CONFLICTS ====" << endl;
    pairConflict(QStringLiteral("HeaderClash_A"), QStringLiteral("HeaderClash_B"));
    pairConflict(QStringLiteral("TcPass_A"), QStringLiteral("TcPass_C"));
    pairConflict(QStringLiteral("TcPass_A"), QStringLiteral("TcPass_B"));

    out << "==== PER PACKAGE ====" << endl;
    out << "name\tbuild\tpe\tload\tlife\tdualCreate\tnotes" << endl;

    QString csvPath = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("sample_models_matrix.csv"));
    // Prefer writing next to repo dist if root is under dist
    if (root.contains(QStringLiteral("sample_models"))) {
        QDir r(root);
        r.cdUp();
        csvPath = r.filePath(QStringLiteral("sample_models_matrix.csv"));
    }
    QFile csv(csvPath);
    csv.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream csvOut(&csv);
    csvOut.setCodec("UTF-8");
    csvOut << "name,build,pe,load,life,dualCreate,notes\n";

    const QFileInfoList pkgs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& fi : pkgs) {
        Row row = TestPackage(fi.absoluteFilePath());
        out << row.name << '\t' << row.buildCfg << '\t' << row.pe << '\t'
            << row.load << '\t' << row.life << '\t' << row.dualCreate << '\t'
            << row.notes << endl;
        out.flush();
        auto esc = [](QString s) {
            s.replace('"', "\"\"");
            if (s.contains(',') || s.contains('"') || s.contains('\n'))
                return QStringLiteral("\"%1\"").arg(s);
            return s;
        };
        csvOut << esc(row.name) << ',' << esc(row.buildCfg) << ',' << esc(row.pe) << ','
               << esc(row.load) << ',' << esc(row.life) << ',' << esc(row.dualCreate) << ','
               << esc(row.notes) << '\n';
    }
    csvOut.flush();
    out << "CSV=" << csvPath << endl;
    out << "ALL DONE" << endl;
    return 0;
}
