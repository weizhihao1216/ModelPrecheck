#include "QmlPrecheckRunner.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include "core/ConcurrencyTester.h"
#include "core/DllLoader.h"
#include "core/FleetSingleThreadMultiObjectTester.h"
#include "core/FunctionalVerifier.h"
#include "core/HeaderAnalyzer.h"
#include "core/LibAnalyzer.h"
#include "core/MultiObjectHarness.h"
#include "core/PackageScanner.h"
#include "core/PeAnalyzer.h"
#include "core/PerfProfiler.h"
#include "core/PrecheckSummary.h"
#include "core/ReportGenerator.h"
#include "core/SingleThreadMultiObjectTester.h"
#include "core/UserCodeHarness.h"

namespace {

QString qstr(const std::string& value) { return QString::fromUtf8(value.c_str()); }
std::string u8(const QString& value) { return value.toUtf8().toStdString(); }

QString displayName(const SessionModelSnapshot& model, int index) {
    return model.name.trimmed().isEmpty()
        ? QStringLiteral("型号 %1").arg(index + 1) : model.name.trimmed();
}

std::string pickHeader(const ModelPackageFiles& package, const std::string& dllPath) {
    if (package.allHeaderFiles.empty()) return {};
    const QString base = QFileInfo(qstr(dllPath)).completeBaseName().toLower();
    for (const std::string& header : package.allHeaderFiles) {
        const QString headerBase = QFileInfo(qstr(header)).completeBaseName().toLower();
        if (headerBase.contains(base) || base.contains(headerBase)
            || headerBase == QStringLiteral("weaponmodel")) {
            return header;
        }
    }
    return package.allHeaderFiles.front();
}

CombinedPrecheckReport inspectDll(const std::string& dllPath,
                                  const std::string& headerPath,
                                  const std::string& configuration,
                                  const std::string& packageDir,
                                  const InterfaceMapping& mapping,
                                  bool skipLoad) {
    CombinedPrecheckReport report;
    report.dllPath = dllPath;
    report.headerPath = headerPath;
    report.buildConfig = configuration;
    report.timestamp = u8(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    const std::vector<std::string> searchPaths = {
        packageDir,
        PackageScanner::ModelsDirectory(packageDir),
        PackageScanner::LibDirectory(packageDir),
        PackageScanner::IncludeDirectory(packageDir)
    };
    report.peReport = PeAnalyzer::AnalyzeDll(dllPath, searchPaths, {});
    std::vector<std::string> exports;
    for (const ExportedSymbolInfo& symbol : report.peReport.exportedSymbols)
        exports.push_back(symbol.name);
    if (!headerPath.empty()) {
        report.headerReport = HeaderAnalyzer::AnalyzeHeader(headerPath);
        report.consistencyReport = HeaderAnalyzer::VerifyConsistency(
            report.headerReport.declaredFunctions, exports);
    }
    if (skipLoad) {
        report.loadReport.isLoaded = false;
        report.loadReport.errorLog =
            "SKIP: Release 宿主不加载 Debug DLL；Debug 库仅执行 PE 静态检查";
        report.overallPass = report.peReport.overallPass;
    } else {
        DllLoader loader;
        report.loadReport = loader.Load(dllPath, mapping);
        report.overallPass = report.peReport.overallPass && report.loadReport.isLoaded;
        loader.Unload();
    }
    return report;
}

DualBuildPrecheckReport inspectModel(const SessionModelSnapshot& model, int index) {
    DualBuildPrecheckReport result;
    result.modelName = u8(displayName(model, index));
    result.packageDir = u8(model.packageDir);
    result.timestamp = u8(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (model.packageDir.trimmed().isEmpty() || !QDir(model.packageDir).exists())
        return result;

    result.packageFiles = PackageScanner::ScanPackageDirectory(result.packageDir);
    const BuildConfigCapability build =
        PrecheckSummary::EvaluateBuildConfig(result.packageFiles, result.modelName);
    result.releaseBuildOk = (build.releaseVerdict == "PASS" || build.releaseVerdict == "WARN")
        && build.canUseRelease;
    result.debugBuildOk = build.canCompileDebug;
    result.releaseBuildSummary = build.releaseSummary;
    result.debugBuildSummary = build.debugSummary;

    std::vector<std::string> declarations;
    std::vector<std::string> exports;
    InterfaceMapping mapping = InterfaceMapping::DefaultSingleton();
    bool hasSuggestedMapping = false;
    for (const std::string& path : result.packageFiles.allHeaderFiles) {
        HeaderAnalysisReport header = HeaderAnalyzer::AnalyzeHeader(path);
        if (header.overallPass) ++result.passedHeaderCount;
        declarations.insert(declarations.end(), header.declaredFunctions.begin(),
                            header.declaredFunctions.end());
        if (!hasSuggestedMapping && !header.suggestedMapping.entries.empty()) {
            mapping = header.suggestedMapping;
            hasSuggestedMapping = true;
        }
        result.headerReports.push_back(std::move(header));
    }
    result.headerConflictReport =
        HeaderAnalyzer::AnalyzeHeaderSet(result.packageFiles.allHeaderFiles);
    for (const std::string& path : result.packageFiles.allLibFiles) {
        LibAnalysisReport library = LibAnalyzer::AnalyzeLib(path);
        if (library.overallPass) ++result.passedLibCount;
        exports.insert(exports.end(), library.foundSymbols.begin(), library.foundSymbols.end());
        result.libReports.push_back(std::move(library));
    }

    int loadedReleaseDlls = 0;
    for (const std::string& path : result.packageFiles.allDllFiles) {
        const bool debug = std::find(result.packageFiles.debugDllFiles.begin(),
                                     result.packageFiles.debugDllFiles.end(), path)
            != result.packageFiles.debugDllFiles.end();
        CombinedPrecheckReport dll = inspectDll(
            path, pickHeader(result.packageFiles, path), debug ? "Debug" : "Release",
            result.packageDir, mapping, debug);
        dll.headerConflictReport = result.headerConflictReport;
        if (dll.overallPass) ++result.passedDllCount;
        if (!debug && dll.loadReport.isLoaded) ++loadedReleaseDlls;
        for (const ExportedSymbolInfo& symbol : dll.peReport.exportedSymbols)
            exports.push_back(symbol.name);
        result.dllReports.push_back(std::move(dll));
    }
    result.consistencyReport = HeaderAnalyzer::VerifyConsistency(declarations, exports);

    const bool headersPass = result.packageFiles.allHeaderFiles.empty()
        || result.passedHeaderCount == static_cast<int>(result.packageFiles.allHeaderFiles.size());
    const bool librariesPass = result.packageFiles.allLibFiles.empty()
        || result.passedLibCount == static_cast<int>(result.packageFiles.allLibFiles.size());
    const bool dllsPass = !result.packageFiles.releaseDllFiles.empty()
        ? loadedReleaseDlls == static_cast<int>(result.packageFiles.releaseDllFiles.size())
        : (result.packageFiles.allDllFiles.empty()
           || result.passedDllCount == static_cast<int>(result.packageFiles.allDllFiles.size()));
    result.overallPass = result.packageFiles.layoutValid() && headersPass && librariesPass
        && dllsPass && result.headerConflictReport.overallPass;
    return result;
}

void applyRuntimeReports(FleetSessionReport& fleet, const SessionSnapshot& snapshot,
                         const QmlPrecheckRunner::ProgressCallback& progress) {
    std::vector<std::unique_ptr<UserCodeHarness>> harnesses;
    std::vector<int> indexes;
    for (int i = 0; i < static_cast<int>(snapshot.models.size()); ++i) {
        const SessionModelSnapshot& model = snapshot.models[static_cast<size_t>(i)];
        if (model.lastUserHarnessDll.isEmpty() || !QFileInfo::exists(model.lastUserHarnessDll))
            continue;
        std::unique_ptr<UserCodeHarness> harness(new UserCodeHarness());
        std::string error;
        if (!harness->LoadCompiledDll(u8(model.lastUserHarnessDll), error)) continue;
        harness->SetEnabledRandomVars(model.randomVars);
        harness->SetStepParams(model.multiObjectSteps, model.multiObjectDt);
        indexes.push_back(i);
        harnesses.push_back(std::move(harness));
    }

    bool firstPerf = true;
    double weightedTime = 0.0;
    const int runs = qBound(1, snapshot.perfSteps, 100);
    for (int h = 0; h < static_cast<int>(harnesses.size()); ++h) {
        const int modelIndex = indexes[static_cast<size_t>(h)];
        if (progress) progress(QStringLiteral("性能与轨迹：%1")
            .arg(displayName(snapshot.models[static_cast<size_t>(modelIndex)], modelIndex)));
        PerfProfileReport perf;
        PerfProfilerWorker worker(harnesses[static_cast<size_t>(h)].get(), runs,
                                  snapshot.perfHz, static_cast<uint32_t>(modelIndex + 1),
                                  snapshot.perfMemCapMB);
        QObject::connect(&worker, &PerfProfilerWorker::finished,
                         [&perf](const PerfProfileReport& value) { perf = value; });
        worker.process();
        if (firstPerf) {
            fleet.perfReport = perf;
            weightedTime = perf.avgTimeMs * perf.completedSteps;
            firstPerf = false;
        } else {
            PerfProfileReport& total = fleet.perfReport;
            weightedTime += perf.avgTimeMs * perf.completedSteps;
            total.totalSteps += perf.totalSteps;
            total.completedSteps += perf.completedSteps;
            total.minTimeMs = (std::min)(total.minTimeMs, perf.minTimeMs);
            total.maxTimeMs = (std::max)(total.maxTimeMs, perf.maxTimeMs);
            total.jitterMs = (std::max)(total.jitterMs, perf.jitterMs);
            total.memoryDeltaMB += perf.memoryDeltaMB;
            total.memoryLeakRateMBPer10k = (std::max)(total.memoryLeakRateMBPer10k,
                                                       perf.memoryLeakRateMBPer10k);
            total.encounteredException = total.encounteredException || perf.encounteredException;
            if (perf.realtimeVerdict == "FAIL"
                || (perf.realtimeVerdict == "WARNING" && total.realtimeVerdict == "PASS"))
                total.realtimeVerdict = perf.realtimeVerdict;
            if (total.completedSteps > 0) total.avgTimeMs = weightedTime / total.completedSteps;
        }

        UserCodeHarness* harness = harnesses[static_cast<size_t>(h)].get();
        harness->SetTrajectoryCapture(true);
        const RandomValueBlob values = harness->Sample(static_cast<uint32_t>(1000 + modelIndex));
        int returnCode = 0;
        bool seh = false;
        std::string error;
        const bool ran = harness->RunOnce(values, &returnCode, &seh, error);
        harness->SetTrajectoryCapture(false);
        std::vector<TrajectorySample> samples;
        if (ran) harness->FetchTrajectory(samples);
        std::vector<WeaponModelOutput> history;
        history.reserve(samples.size());
        for (const TrajectorySample& sample : samples) {
            WeaponModelOutput output{};
            output.lat = sample.lat;
            output.lon = sample.lon;
            history.push_back(output);
        }
        TrajectoryVerificationReport trajectory = FunctionalVerifier::VerifyTrajectory(history);
        trajectory.overallPass = trajectory.overallPass && ran && !seh && returnCode == 0
            && !samples.empty();
        ++fleet.trajectoryModelsTested;
        if (trajectory.overallPass) ++fleet.trajectoryModelsPassed;
        if (modelIndex < static_cast<int>(fleet.modelReports.size())) {
            for (CombinedPrecheckReport& dll : fleet.modelReports[static_cast<size_t>(modelIndex)].dllReports) {
                dll.perfReport = perf;
                dll.trajReport = trajectory;
            }
        }
        // 性能与轨迹阶段结论：日志中可看到每个型号的 PASS / FAIL。
        if (progress) progress(QStringLiteral("性能与轨迹结果：%1 — 性能 %2 · 轨迹 %3")
            .arg(displayName(snapshot.models[static_cast<size_t>(modelIndex)], modelIndex))
            .arg(perf.realtimeVerdict.empty() ? QStringLiteral("未执行")
                                              : qstr(perf.realtimeVerdict))
            .arg(trajectory.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")));
    }

    if (harnesses.empty()) return;
    if (progress) progress(QStringLiteral("多线程与多型号并行检查"));
    ConcurrencyTestConfig multi;
    multi.mode = ConcurrencyTestMode::MultiModel;
    for (int h = 0; h < static_cast<int>(harnesses.size()); ++h) {
        const int modelIndex = indexes[static_cast<size_t>(h)];
        MultiModelSpec spec;
        spec.harness = harnesses[static_cast<size_t>(h)].get();
        spec.count = qBound(1, snapshot.models[static_cast<size_t>(modelIndex)].instanceCount, 64);
        spec.modelName = u8(displayName(snapshot.models[static_cast<size_t>(modelIndex)], modelIndex));
        multi.models.push_back(spec);
    }
    fleet.multiModelReport = ConcurrencyTester::RunMultiModel(multi);

    ConcurrencyTestReport threads;
    threads.mode = ConcurrencyTestMode::MultiThread;
    threads.modelTypeCount = static_cast<int>(harnesses.size());
    threads.multiThreadSafe = true;
    threads.verdict = "PASS";
    for (int h = 0; h < static_cast<int>(harnesses.size()); ++h) {
        ConcurrencyTestConfig config;
        config.mode = ConcurrencyTestMode::MultiThread;
        config.count = qBound(1, snapshot.threadCount, 64);
        ConcurrencyTestReport current = ConcurrencyTester::Run(*harnesses[static_cast<size_t>(h)], config);
        threads.workerCount += current.workerCount;
        threads.successCount += current.successCount;
        threads.userFailCount += current.userFailCount;
        threads.exceptionCount += current.exceptionCount;
        threads.crashed = threads.crashed || current.crashed;
        threads.multiThreadSafe = threads.multiThreadSafe && current.multiThreadSafe;
        for (ConcurrencyThreadResult item : current.threadResults) {
            const int modelIndex = indexes[static_cast<size_t>(h)];
            item.modelName = u8(displayName(snapshot.models[static_cast<size_t>(modelIndex)], modelIndex));
            threads.threadResults.push_back(std::move(item));
        }
        if (current.verdict == "FAIL"
            || (current.verdict == "WARNING" && threads.verdict == "PASS"))
            threads.verdict = current.verdict;
    }
    threads.summary = "已完成全部已编译型号的多线程 UserMain 测试";
    fleet.multiThreadReport = std::move(threads);

    // 多线程与多型号并行阶段结论
    if (progress) progress(QStringLiteral("多线程测试结果 — %1（成功 %2 · 用户失败 %3 · 异常 %4）")
        .arg(qstr(fleet.multiThreadReport.verdict))
        .arg(fleet.multiThreadReport.successCount)
        .arg(fleet.multiThreadReport.userFailCount)
        .arg(fleet.multiThreadReport.exceptionCount));
    if (progress) progress(QStringLiteral("多型号并行结果 — %1")
        .arg(qstr(fleet.multiModelReport.verdict)));
}

void applyMultiObjectReports(FleetSessionReport& fleet, const SessionSnapshot& snapshot,
                             const QmlPrecheckRunner::ProgressCallback& progress) {
    std::vector<std::unique_ptr<MultiObjectHarness>> loaded;
    std::vector<int> loadedIndexes;
    for (int i = 0; i < static_cast<int>(snapshot.models.size()); ++i) {
        const SessionModelSnapshot& model = snapshot.models[static_cast<size_t>(i)];
        if (progress) progress(QStringLiteral("单线程多对象：%1").arg(displayName(model, i)));
        ModelMultiObjectReport named;
        named.modelName = u8(displayName(model, i));
        named.configured = !model.userMainBody.trimmed().isEmpty();
        std::unique_ptr<MultiObjectHarness> harness(new MultiObjectHarness());
        std::string error;
        named.harnessCompiled = !model.lastUserHarnessDll.isEmpty()
            && QFileInfo::exists(model.lastUserHarnessDll)
            && harness->LoadCompiledDll(u8(model.lastUserHarnessDll), error);
        if (named.harnessCompiled) {
            harness->SetEnabledRandomVars(model.randomVars);
            MultiObjectTestConfig config;
            config.objectCount = qMax(1, model.multiObjectCount);
            config.stepCount = qMax(1, model.multiObjectSteps);
            config.stepDt = model.multiObjectDt;
            config.tolerance = model.multiObjectTolerance;
            config.schedule = static_cast<MultiObjectSchedule>(model.multiObjectSchedule);
            config.randomSeed = static_cast<uint32_t>(20260902 + i);
            named.report = SingleThreadMultiObjectTester::Run(
                *harness, config, harness->Sample(config.randomSeed, config.objectCount));
            loadedIndexes.push_back(i);
            loaded.push_back(std::move(harness));
        } else {
            named.report.summary = named.configured
                ? "对象代码尚未编译或无法加载" : "未完成对象代码配置";
        }
        fleet.multiObjectReports.push_back(std::move(named));
        // 单线程多对象阶段结论：未编译 / 未配置的型号同样给出结论行。
        const ModelMultiObjectReport& stored = fleet.multiObjectReports.back();
        if (progress) progress(QStringLiteral("单线程多对象结果：%1 — %2")
            .arg(displayName(model, i))
            .arg(stored.report.verdict.empty()
                 ? (stored.configured ? QStringLiteral("未执行") : QStringLiteral("未配置"))
                 : qstr(stored.report.verdict)));
    }
    if (loaded.size() < 2) return;
    if (progress) progress(QStringLiteral("跨型号对象交错检查"));
    FleetMultiObjectTestConfig config;
    config.randomSeed = 20260903u;
    for (int h = 0; h < static_cast<int>(loaded.size()); ++h) {
        const SessionModelSnapshot& model = snapshot.models[static_cast<size_t>(loadedIndexes[static_cast<size_t>(h)])];
        if (!loaded[static_cast<size_t>(h)]->SupportsObjectSession()) continue;
        FleetMultiObjectModelSpec spec;
        spec.harness = loaded[static_cast<size_t>(h)].get();
        spec.modelName = u8(displayName(model, loadedIndexes[static_cast<size_t>(h)]));
        spec.objectCount = qMax(1, model.multiObjectCount);
        config.models.push_back(spec);
    }
    if (config.models.size() >= 2)
        fleet.fleetMultiObjectReport = FleetSingleThreadMultiObjectTester::Run(config);
    if (progress && config.models.size() >= 2)
        progress(QStringLiteral("跨型号交错结果 — %1")
            .arg(qstr(fleet.fleetMultiObjectReport.verdict)));
}

QVariantMap concurrencyMap(const QString& title, const QString& description,
                           const ConcurrencyTestReport& report) {
    QVariantMap row;
    row.insert(QStringLiteral("title"), title);
    row.insert(QStringLiteral("description"), description);
    row.insert(QStringLiteral("verdict"), qstr(report.verdict));
    row.insert(QStringLiteral("summary"), qstr(report.summary));
    row.insert(QStringLiteral("workerCount"), report.workerCount);
    row.insert(QStringLiteral("modelTypeCount"), report.modelTypeCount);
    row.insert(QStringLiteral("successCount"), report.successCount);
    row.insert(QStringLiteral("exceptionCount"), report.exceptionCount);
    row.insert(QStringLiteral("userFailCount"), report.userFailCount);
    QVariantList workers;
    for (const ConcurrencyThreadResult& item : report.threadResults) {
        QVariantMap worker;
        worker.insert(QStringLiteral("threadId"), item.threadId);
        worker.insert(QStringLiteral("modelName"), qstr(item.modelName));
        worker.insert(QStringLiteral("instanceId"), item.instanceId);
        worker.insert(QStringLiteral("returnCode"), item.userReturnCode);
        worker.insert(QStringLiteral("exception"), item.exceptionOccurred);
        worker.insert(QStringLiteral("userFail"), item.userReportedFail);
        worker.insert(QStringLiteral("randomSummary"), qstr(item.randomSummary));
        worker.insert(QStringLiteral("error"), qstr(item.errorLog));
        workers.append(worker);
    }
    row.insert(QStringLiteral("workers"), workers);
    return row;
}

void buildPayload(const FleetSessionReport& fleet, QmlPrecheckResult& output) {
    const PrecheckSummaryBoard board = PrecheckSummary::BuildFromFleet(fleet);
    output.passCount = board.passCount;
    output.warnCount = board.warnCount;
    output.failCount = board.failCount;
    output.pendingCount = board.notRunCount + board.skippedCount;
    output.timestamp = qstr(fleet.timestamp);
    output.overallPass = fleet.overallPass;
    for (const TestItemResult& item : board.items) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), qstr(item.id));
        row.insert(QStringLiteral("name"), qstr(item.name));
        row.insert(QStringLiteral("state"), qstr(PrecheckSummary::StateLabel(item.state)));
        row.insert(QStringLiteral("reason"), qstr(item.reason));
        row.insert(QStringLiteral("consequence"), qstr(item.consequence));
        output.items.append(row);
    }

    for (const DualBuildPrecheckReport& model : fleet.modelReports) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), qstr(model.modelName));
        row.insert(QStringLiteral("packageDir"), qstr(model.packageDir));
        row.insert(QStringLiteral("overallPass"), model.overallPass);
        row.insert(QStringLiteral("layoutValid"), model.packageFiles.layoutValid());
        row.insert(QStringLiteral("headerCount"), static_cast<int>(model.packageFiles.allHeaderFiles.size()));
        row.insert(QStringLiteral("passedHeaderCount"), model.passedHeaderCount);
        row.insert(QStringLiteral("libCount"), static_cast<int>(model.packageFiles.allLibFiles.size()));
        row.insert(QStringLiteral("passedLibCount"), model.passedLibCount);
        row.insert(QStringLiteral("dllCount"), static_cast<int>(model.packageFiles.allDllFiles.size()));
        row.insert(QStringLiteral("passedDllCount"), model.passedDllCount);
        row.insert(QStringLiteral("releaseBuildOk"), model.releaseBuildOk);
        row.insert(QStringLiteral("debugBuildOk"), model.debugBuildOk);
        row.insert(QStringLiteral("releaseSummary"), qstr(model.releaseBuildSummary));
        row.insert(QStringLiteral("debugSummary"), qstr(model.debugBuildSummary));
        QVariantList headers;
        for (const HeaderAnalysisReport& header : model.headerReports) {
            QVariantMap value;
            value.insert(QStringLiteral("name"), QFileInfo(qstr(header.filePath)).fileName());
            value.insert(QStringLiteral("path"), qstr(header.filePath));
            value.insert(QStringLiteral("encoding"), qstr(header.encoding));
            value.insert(QStringLiteral("externC"), header.hasExternC);
            value.insert(QStringLiteral("declspec"), header.hasDeclspec);
            value.insert(QStringLiteral("packDirective"), header.hasPackDirective);
            value.insert(QStringLiteral("pass"), header.overallPass);
            value.insert(QStringLiteral("apiStyle"), qstr(header.apiStyleDescription));
            QVariantList headerMessages;
            headerMessages.append(QStringLiteral("编码 ") + qstr(header.encoding));
            headerMessages.append(qstr(header.apiStyleDescription));
            value.insert(QStringLiteral("messages"), headerMessages);
            QVariantList functions;
            for (const std::string& function : header.declaredFunctions) functions.append(qstr(function));
            value.insert(QStringLiteral("functions"), functions);
            headers.append(value);
        }
        row.insert(QStringLiteral("headers"), headers);
        QVariantList libraries;
        for (const LibAnalysisReport& library : model.libReports) {
            QVariantMap value;
            value.insert(QStringLiteral("name"), QFileInfo(qstr(library.filePath)).fileName());
            value.insert(QStringLiteral("path"), qstr(library.filePath));
            value.insert(QStringLiteral("architecture"), qstr(library.architecture));
            value.insert(QStringLiteral("type"), qstr(library.libType));
            value.insert(QStringLiteral("pass"), library.overallPass);
            // Release / Debug 分开标注，便于按构建配置查看明细
            const bool debugLib = std::find(model.packageFiles.debugLibFiles.begin(),
                                            model.packageFiles.debugLibFiles.end(),
                                            library.filePath) != model.packageFiles.debugLibFiles.end();
            value.insert(QStringLiteral("configuration"),
                         debugLib ? QStringLiteral("Debug") : QStringLiteral("Release"));
            QVariantList libMessages;
            libMessages.append(qstr(library.architecture));
            libMessages.append(qstr(library.libType));
            value.insert(QStringLiteral("messages"), libMessages);
            QVariantList found;
            for (const std::string& symbol : library.foundSymbols) found.append(qstr(symbol));
            value.insert(QStringLiteral("foundSymbols"), found);
            QVariantList missing;
            for (const std::string& symbol : library.missingSymbols) missing.append(qstr(symbol));
            value.insert(QStringLiteral("missingSymbols"), missing);
            libraries.append(value);
        }
        row.insert(QStringLiteral("libraries"), libraries);
        row.insert(QStringLiteral("consistencyRatio"), model.consistencyReport.consistencyRatio);
        row.insert(QStringLiteral("consistencyPass"), model.consistencyReport.isFullyConsistent);
        QVariantList declaredOnly;
        for (const std::string& symbol : model.consistencyReport.declaredButNotExported) declaredOnly.append(qstr(symbol));
        row.insert(QStringLiteral("declaredOnly"), declaredOnly);
        QVariantList exportedOnly;
        for (const std::string& symbol : model.consistencyReport.exportedButNotDeclared) exportedOnly.append(qstr(symbol));
        row.insert(QStringLiteral("exportedOnly"), exportedOnly);
        QVariantList dlls;
        for (const CombinedPrecheckReport& dll : model.dllReports) {
            QVariantMap value;
            value.insert(QStringLiteral("name"), QFileInfo(qstr(dll.dllPath)).fileName());
            value.insert(QStringLiteral("path"), qstr(dll.dllPath));
            value.insert(QStringLiteral("configuration"), qstr(dll.buildConfig));
            value.insert(QStringLiteral("architecture"), qstr(dll.peReport.architecture));
            value.insert(QStringLiteral("crt"), qstr(dll.peReport.crtLinkage));
            value.insert(QStringLiteral("pePass"), dll.peReport.overallPass);
            value.insert(QStringLiteral("missingDependencies"), dll.peReport.missingDependencyCount);
            value.insert(QStringLiteral("missingExports"), dll.peReport.missingExportCount);
            value.insert(QStringLiteral("loaded"), dll.loadReport.isLoaded);
            value.insert(QStringLiteral("loadError"), qstr(dll.loadReport.errorLog));
            value.insert(QStringLiteral("boundSymbols"), dll.loadReport.boundSymbolCount);
            value.insert(QStringLiteral("missingSymbols"), dll.loadReport.missingSymbolCount);
            QVariantList dependencies;
            for (const ImportedDllInfo& dependency : dll.peReport.importedDlls) {
                QVariantMap dependencyValue;
                dependencyValue.insert(QStringLiteral("name"), qstr(dependency.name));
                dependencyValue.insert(QStringLiteral("found"), dependency.found);
                dependencyValue.insert(QStringLiteral("path"), qstr(dependency.resolvedPath));
                dependencies.append(dependencyValue);
            }
            value.insert(QStringLiteral("dependencies"), dependencies);
            QVariantList exportedSymbols;
            for (const ExportedSymbolInfo& symbol : dll.peReport.exportedSymbols) {
                QVariantMap symbolValue;
                symbolValue.insert(QStringLiteral("name"), qstr(symbol.name));
                symbolValue.insert(QStringLiteral("ordinal"), static_cast<uint>(symbol.ordinal));
                symbolValue.insert(QStringLiteral("required"), symbol.isRequiredInterface);
                exportedSymbols.append(symbolValue);
            }
            value.insert(QStringLiteral("exportedSymbols"), exportedSymbols);
            value.insert(QStringLiteral("trajectoryPass"), dll.trajReport.overallPass);
            value.insert(QStringLiteral("trajectoryPoints"), dll.trajReport.totalDataPoints);
            value.insert(QStringLiteral("trajectoryNanCount"), dll.trajReport.nanOrInfCount);
            value.insert(QStringLiteral("trajectoryJumpCount"), dll.trajReport.positionJumpCount);
            value.insert(QStringLiteral("trajectoryOutOfBoundsCount"), dll.trajReport.outOfBoundsCount);
            value.insert(QStringLiteral("trajectoryUnit"), dll.trajReport.isDegreeUnit ? QStringLiteral("经纬度（度）") : QStringLiteral("疑似弧度"));
            value.insert(QStringLiteral("trajectoryUnitLog"), qstr(dll.trajReport.unitCheckLog));
            QVariantList trajectoryMessages;
            for (const std::string& message : dll.trajReport.warnings) trajectoryMessages.append(qstr(message));
            for (const std::string& message : dll.trajReport.errors) trajectoryMessages.append(qstr(message));
            value.insert(QStringLiteral("trajectoryMessages"), trajectoryMessages);
            value.insert(QStringLiteral("perfVerdict"), qstr(dll.perfReport.realtimeVerdict));
            value.insert(QStringLiteral("perfAverageMs"), dll.perfReport.avgTimeMs);
            value.insert(QStringLiteral("perfMaximumMs"), dll.perfReport.maxTimeMs);
            value.insert(QStringLiteral("perfJitterMs"), dll.perfReport.jitterMs);
            value.insert(QStringLiteral("memoryDeltaMB"), dll.perfReport.memoryDeltaMB);
            value.insert(QStringLiteral("memoryLeakRate"), dll.perfReport.memoryLeakRateMBPer10k);
            value.insert(QStringLiteral("initialMemoryMB"), dll.perfReport.initialMemoryMB);
            value.insert(QStringLiteral("finalMemoryMB"), dll.perfReport.finalMemoryMB);
            QVariantList perfSamples;
            for (const PerfSample& sample : dll.perfReport.samples) {
                QVariantMap point;
                point.insert(QStringLiteral("step"), sample.stepIndex);
                point.insert(QStringLiteral("timeMs"), sample.timeMs);
                point.insert(QStringLiteral("memoryMB"), sample.memoryMB);
                perfSamples.append(point);
            }
            value.insert(QStringLiteral("perfSamples"), perfSamples);
            QVariantList trajectory;
            const size_t count = (std::min)(dll.trajReport.latList.size(), dll.trajReport.lonList.size());
            for (size_t i = 0; i < count; ++i) {
                QVariantMap point;
                point.insert(QStringLiteral("x"), dll.trajReport.lonList[i]);
                point.insert(QStringLiteral("y"), dll.trajReport.latList[i]);
                trajectory.append(point);
            }
            value.insert(QStringLiteral("trajectory"), trajectory);
            dlls.append(value);
        }
        row.insert(QStringLiteral("dlls"), dlls);
        output.models.append(row);
    }

    output.concurrency.append(concurrencyMap(
        QStringLiteral("多型号并行"),
        QStringLiteral("多个型号在同一进程中并行运行，检查跨 DLL 干扰"),
        fleet.multiModelReport));
    output.concurrency.append(concurrencyMap(
        QStringLiteral("多线程稳定性"),
        QStringLiteral("每个型号并行执行 UserMain，检查线程安全与异常"),
        fleet.multiThreadReport));

    output.headerConflicts.insert(QStringLiteral("overallPass"), fleet.crossModelHeaderConflictReport.overallPass);
    output.headerConflicts.insert(QStringLiteral("duplicateTypeCount"), fleet.crossModelHeaderConflictReport.duplicateTypeCount);
    output.headerConflicts.insert(QStringLiteral("odrConflictCount"), fleet.crossModelHeaderConflictReport.odrConflictCount);
    output.headerConflicts.insert(QStringLiteral("namespacePollutionCount"), fleet.crossModelHeaderConflictReport.namespacePollutionCount);
    QVariantList issues;
    for (const HeaderConflictIssue& issue : fleet.crossModelHeaderConflictReport.issues) {
        QVariantMap value;
        value.insert(QStringLiteral("category"), qstr(issue.category));
        value.insert(QStringLiteral("severity"), qstr(issue.severity));
        value.insert(QStringLiteral("symbol"), qstr(issue.symbol));
        value.insert(QStringLiteral("detail"), qstr(issue.detail));
        QVariantList files;
        for (const std::string& file : issue.files) files.append(qstr(file));
        value.insert(QStringLiteral("files"), files);
        issues.append(value);
    }
    output.headerConflicts.insert(QStringLiteral("issues"), issues);

    for (const ModelMultiObjectReport& named : fleet.multiObjectReports) {
        const MultiObjectTestReport& report = named.report;
        QVariantMap row;
        row.insert(QStringLiteral("scope"), QStringLiteral("model"));
        row.insert(QStringLiteral("title"), qstr(named.modelName));
        row.insert(QStringLiteral("configured"), named.configured);
        row.insert(QStringLiteral("compiled"), named.harnessCompiled);
        row.insert(QStringLiteral("verdict"), qstr(report.verdict));
        row.insert(QStringLiteral("summary"), qstr(report.summary));
        row.insert(QStringLiteral("modelCount"), 1);
        row.insert(QStringLiteral("objectCount"), report.objectCount);
        row.insert(QStringLiteral("stepCount"), report.stepCount);
        row.insert(QStringLiteral("completedObjects"), report.completedObjects);
        row.insert(QStringLiteral("interferenceCount"), report.interferenceCount);
        row.insert(QStringLiteral("exceptionCount"), report.exceptionCount);
        row.insert(QStringLiteral("tolerance"), report.tolerance);
        row.insert(QStringLiteral("maxDeviation"), report.maxPositionDeviation);
        row.insert(QStringLiteral("maxFrameMs"), report.maxFrameTimeMs);
        row.insert(QStringLiteral("memoryDeltaMB"), report.memoryDeltaMB);
        QVariantList objects;
        for (const MultiObjectResult& item : report.objectResults) {
            QVariantMap object;
            object.insert(QStringLiteral("modelName"), qstr(named.modelName));
            object.insert(QStringLiteral("objectId"), item.objectId);
            object.insert(QStringLiteral("baselineReturn"), item.baselineReturnCode);
            object.insert(QStringLiteral("interleavedReturn"), item.interleavedReturnCode);
            object.insert(QStringLiteral("exception"), item.exceptionOccurred);
            object.insert(QStringLiteral("exceptionCode"), static_cast<qulonglong>(item.exceptionCode));
            object.insert(QStringLiteral("faultStep"), item.faultStep);
            object.insert(QStringLiteral("deviation"), item.maxPositionDeviation);
            object.insert(QStringLiteral("detail"), qstr(item.detail));
            objects.append(object);
        }
        row.insert(QStringLiteral("objects"), objects);
        output.multiObject.append(row);
    }
    const FleetMultiObjectTestReport& fleetMo = fleet.fleetMultiObjectReport;
    if (!fleetMo.verdict.empty() || fleetMo.totalObjectCount > 0) {
        QVariantMap row;
        row.insert(QStringLiteral("scope"), QStringLiteral("fleet"));
        row.insert(QStringLiteral("title"), QStringLiteral("跨型号对象交错"));
        row.insert(QStringLiteral("configured"), true);
        row.insert(QStringLiteral("compiled"), true);
        row.insert(QStringLiteral("verdict"), qstr(fleetMo.verdict));
        row.insert(QStringLiteral("summary"), qstr(fleetMo.summary));
        row.insert(QStringLiteral("modelCount"), fleetMo.modelCount);
        row.insert(QStringLiteral("objectCount"), fleetMo.totalObjectCount);
        row.insert(QStringLiteral("stepCount"), fleetMo.stepCount);
        row.insert(QStringLiteral("completedObjects"), fleetMo.completedObjects);
        row.insert(QStringLiteral("interferenceCount"), fleetMo.interferenceCount);
        row.insert(QStringLiteral("exceptionCount"), fleetMo.exceptionCount);
        row.insert(QStringLiteral("tolerance"), fleetMo.tolerance);
        row.insert(QStringLiteral("maxDeviation"), fleetMo.maxPositionDeviation);
        row.insert(QStringLiteral("maxFrameMs"), fleetMo.maxFrameTimeMs);
        row.insert(QStringLiteral("memoryDeltaMB"), fleetMo.memoryDeltaMB);
        QVariantList objects;
        for (const FleetMultiObjectResult& named : fleetMo.objectResults) {
            QVariantMap object;
            object.insert(QStringLiteral("modelName"), qstr(named.modelName));
            object.insert(QStringLiteral("objectId"), named.localObjectId);
            object.insert(QStringLiteral("baselineReturn"), named.detail.baselineReturnCode);
            object.insert(QStringLiteral("interleavedReturn"), named.detail.interleavedReturnCode);
            object.insert(QStringLiteral("exception"), named.detail.exceptionOccurred);
            object.insert(QStringLiteral("exceptionCode"), static_cast<qulonglong>(named.detail.exceptionCode));
            object.insert(QStringLiteral("faultStep"), named.detail.faultStep);
            object.insert(QStringLiteral("deviation"), named.detail.maxPositionDeviation);
            object.insert(QStringLiteral("detail"), qstr(named.detail.detail));
            objects.append(object);
        }
        row.insert(QStringLiteral("objects"), objects);
        output.multiObject.append(row);
    }
}

} // namespace

QmlPrecheckResult QmlPrecheckRunner::Run(const SessionSnapshot& snapshot,
                                         const ProgressCallback& progress) {
    QmlPrecheckResult output;
    if (snapshot.models.empty()) {
        output.error = QStringLiteral("请先添加至少一个型号。");
        return output;
    }
    FleetSessionReport fleet;
    fleet.timestamp = u8(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    int passedModels = 0;
    for (int i = 0; i < static_cast<int>(snapshot.models.size()); ++i) {
        const SessionModelSnapshot& model = snapshot.models[static_cast<size_t>(i)];
        if (progress) progress(QStringLiteral("静态检查：%1（%2/%3）")
            .arg(displayName(model, i))
            .arg(i + 1).arg(snapshot.models.size()));
        DualBuildPrecheckReport report = inspectModel(model, i);
        if (report.overallPass) ++passedModels;
        // 每个型号的静态检查结论单独成行，日志中可直接看到 PASS / FAIL。
        if (progress) progress(QStringLiteral("静态检查结果：%1 — %2")
            .arg(displayName(model, i))
            .arg(report.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL")));
        fleet.modelReports.push_back(std::move(report));
    }
    std::vector<std::string> allHeaders;
    for (const DualBuildPrecheckReport& model : fleet.modelReports)
        allHeaders.insert(allHeaders.end(), model.packageFiles.allHeaderFiles.begin(),
                          model.packageFiles.allHeaderFiles.end());
    fleet.crossModelHeaderConflictReport = HeaderAnalyzer::AnalyzeHeaderSet(allHeaders);
    applyRuntimeReports(fleet, snapshot, progress);
    applyMultiObjectReports(fleet, snapshot, progress);

    fleet.overallPass = passedModels == static_cast<int>(snapshot.models.size());
    if (fleet.perfReport.realtimeVerdict == "FAIL"
        || fleet.multiModelReport.verdict == "FAIL"
        || fleet.multiThreadReport.verdict == "FAIL"
        || (fleet.trajectoryModelsTested > 0
            && fleet.trajectoryModelsPassed != fleet.trajectoryModelsTested)
        || !fleet.crossModelHeaderConflictReport.overallPass)
        fleet.overallPass = false;
    for (const ModelMultiObjectReport& report : fleet.multiObjectReports) {
        if (report.configured && report.harnessCompiled && report.report.verdict == "FAIL")
            fleet.overallPass = false;
    }
    if (fleet.fleetMultiObjectReport.verdict == "FAIL") fleet.overallPass = false;

    // 一键预检总体结论：日志中直接给出 PASS / FAIL。
    if (progress) progress(QStringLiteral("预检总体结论：%1（静态检查通过型号 %2/%3 · 轨迹 %4/%5）")
        .arg(fleet.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
        .arg(passedModels).arg(snapshot.models.size())
        .arg(fleet.trajectoryModelsPassed).arg(fleet.trajectoryModelsTested));

    if (progress) progress(QStringLiteral("正在生成检查报告"));
    const QString directory = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session"));
    QDir().mkpath(directory);
    output.reportPath = QDir(directory).filePath(QStringLiteral("last_precheck_report.html"));
    if (!ReportGenerator::SaveFleetReportToFile(
            fleet, u8(QDir::toNativeSeparators(output.reportPath)))) {
        output.error = QStringLiteral("检查已完成，但报告文件保存失败。");
    }
    buildPayload(fleet, output);
    return output;
}
