// Smoke: run PackageScanner + Header/Lib/PE analyzers on all dist/sample_models
// without Qt UI. Build: cmake --build build --config Release --target StaticPrecheckSmoke
#include "../src/core/PackageScanner.h"
#include "../src/core/HeaderAnalyzer.h"
#include "../src/core/LibAnalyzer.h"
#include "../src/core/PeAnalyzer.h"
#include "../src/utils/QtEncoding.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>
#include <iostream>
#include <string>
#include <vector>

static void AnalyzePackage(const QString& pkgDir, QTextStream& out) {
    out << "==== " << pkgDir << endl;
    out.flush();
    ModelPackageFiles pkg = PackageScanner::ScanPackageDirectory(qToUtf8(pkgDir));
    out << "  headers=" << pkg.allHeaderFiles.size()
        << " libs=" << pkg.allLibFiles.size()
        << " dlls=" << pkg.allDllFiles.size() << endl;
    out.flush();

    for (const auto& h : pkg.allHeaderFiles) {
        out << "  HDR " << qUtf8(h) << endl;
        out.flush();
        (void)HeaderAnalyzer::AnalyzeHeader(h);
    }
    out << "  HDR-SET..." << endl;
    out.flush();
    (void)HeaderAnalyzer::AnalyzeHeaderSet(pkg.allHeaderFiles);

    for (const auto& l : pkg.allLibFiles) {
        out << "  LIB " << qUtf8(l) << endl;
        out.flush();
        (void)LibAnalyzer::AnalyzeLib(l);
    }
    for (const auto& d : pkg.allDllFiles) {
        out << "  PE  " << qUtf8(d) << endl;
        out.flush();
        std::vector<std::string> search;
        search.push_back(qToUtf8(pkgDir));
        (void)PeAnalyzer::AnalyzeDll(d, search, {});
    }
    out << "  OK " << QDir(pkgDir).dirName() << endl;
    out.flush();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    out.setCodec("UTF-8");

    QString root = QCoreApplication::applicationDirPath() + "/sample_models";
    if (argc >= 2) root = QString::fromLocal8Bit(argv[1]);
    out << "Root: " << root << endl;

    QDir dir(root);
    if (!dir.exists()) {
        out << "FAIL: root missing" << endl;
        return 2;
    }

    const QFileInfoList pkgs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : pkgs) {
        AnalyzePackage(fi.absoluteFilePath(), out);
    }

    // Cross-model header set like one-click precheck
    std::vector<std::string> allHeaders;
    for (const QFileInfo& fi : pkgs) {
        ModelPackageFiles pkg =
            PackageScanner::ScanPackageDirectory(qToUtf8(fi.absoluteFilePath()));
        allHeaders.insert(allHeaders.end(),
                          pkg.allHeaderFiles.begin(), pkg.allHeaderFiles.end());
    }
    out << "CROSS headers=" << allHeaders.size() << endl;
    out.flush();
    (void)HeaderAnalyzer::AnalyzeHeaderSet(allHeaders);
    out << "ALL DONE" << endl;
    return 0;
}
