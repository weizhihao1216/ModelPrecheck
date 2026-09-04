#include "PrecheckSummary.h"
#include <sstream>
#include <utility>

namespace {

void tally(PrecheckSummaryBoard& board, TestItemState state) {
    switch (state) {
    case TestItemState::Pass: ++board.passCount; break;
    case TestItemState::Fail: ++board.failCount; break;
    case TestItemState::Warn: ++board.warnCount; break;
    case TestItemState::NotRun: ++board.notRunCount; break;
    case TestItemState::Skipped: ++board.skippedCount; break;
    }
}

void pushItem(PrecheckSummaryBoard& board, TestItemResult item) {
    if (item.reason.empty()) {
        switch (item.state) {
        case TestItemState::Pass: item.reason = "检查通过"; break;
        case TestItemState::NotRun: item.reason = "尚未执行该项检测"; break;
        case TestItemState::Skipped: item.reason = "条件不足，已跳过"; break;
        default: break;
        }
    }
    if (item.consequence.empty()) {
        switch (item.state) {
        case TestItemState::Pass:
            item.consequence = "该项对集成无明显额外风险";
            break;
        case TestItemState::NotRun:
        case TestItemState::Skipped:
            item.consequence = "尚未覆盖，集成时风险未知，建议补测";
            break;
        default:
            break;
        }
    }
    tally(board, item.state);
    board.items.push_back(std::move(item));
}

void normalizeItemDefaults(TestItemResult& item) {
    if (item.reason.empty()) {
        switch (item.state) {
        case TestItemState::Pass: item.reason = "检查通过"; break;
        case TestItemState::NotRun: item.reason = "尚未执行该项检测"; break;
        case TestItemState::Skipped: item.reason = "条件不足，已跳过"; break;
        default: break;
        }
    }
    if (item.consequence.empty()) {
        switch (item.state) {
        case TestItemState::Pass:
            item.consequence = "该项对集成无明显额外风险";
            break;
        case TestItemState::NotRun:
        case TestItemState::Skipped:
            item.consequence = "尚未覆盖，集成时风险未知，建议补测";
            break;
        case TestItemState::Fail:
            item.consequence = "可能导致集成失败或运行时异常";
            break;
        case TestItemState::Warn:
            item.consequence = "存在隐患，建议修复后再集成";
            break;
        }
    }
}

void recountBoard(PrecheckSummaryBoard& board) {
    board.passCount = board.failCount = board.warnCount = 0;
    board.notRunCount = board.skippedCount = 0;
    for (const auto& item : board.items)
        tally(board, item.state);
}

} // namespace

const TestItemResult* PrecheckSummaryBoard::findById(const std::string& id) const {
    for (const auto& item : items) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

PrecheckSummaryBoard PrecheckSummary::MakeSkeletonBoard() {
    PrecheckSummaryBoard board;
    const char* ids[][2] = {
        {"build_config", "Release / Debug 构建产物"},
        {"header", "头文件规范检查"},
        {"header_conflict", "跨型号头文件冲突"},
        {"lib", "LIB 库文件检查"},
        {"dll_pe", "DLL 文件与依赖检查"},
        {"dll_load", "DLL 接口与加载检查"},
        {"perf", "UserMain 性能压测"},
        {"memory", "内存泄漏监测"},
        {"trajectory", "运行轨迹检查"},
        {"multimodel", "多型号并行"},
        {"multithread", "多线程稳定性"},
        {"multiobject", "单线程多对象"},
    };
    for (const auto& row : ids) {
        TestItemResult item;
        item.id = row[0];
        item.name = row[1];
        item.state = TestItemState::NotRun;
        pushItem(board, item);
    }
    return board;
}

void PrecheckSummary::UpsertItem(PrecheckSummaryBoard& board, TestItemResult item) {
    normalizeItemDefaults(item);
    bool replaced = false;
    for (auto& existing : board.items) {
        if (existing.id == item.id) {
            existing = std::move(item);
            replaced = true;
            break;
        }
    }
    if (!replaced)
        board.items.push_back(std::move(item));
    recountBoard(board);
}

BuildConfigCapability PrecheckSummary::EvaluateBuildConfig(const ModelPackageFiles& pkg,
                                                           const std::string& modelName) {
    BuildConfigCapability cap;
    cap.modelName = modelName;
    cap.releaseDllCount = static_cast<int>(pkg.releaseDllFiles.size());
    cap.releaseLibCount = static_cast<int>(pkg.releaseLibFiles.size());
    cap.debugDllCount = static_cast<int>(pkg.debugDllFiles.size());
    cap.debugLibCount = static_cast<int>(pkg.debugLibFiles.size());

    cap.canUseRelease = cap.releaseDllCount > 0;
    cap.canLinkRelease = cap.releaseLibCount > 0;
    cap.canUseDebug = cap.debugDllCount > 0;
    cap.canLinkDebug = cap.debugLibCount > 0;
    cap.canCompileDebug = cap.canUseDebug && cap.canLinkDebug;

    if (cap.canUseRelease && cap.canLinkRelease) {
        cap.releaseVerdict = "PASS";
        cap.releaseSummary =
            "可编译/链接 Release：发现 Release DLL "
            + std::to_string(cap.releaseDllCount) + " 个、Release LIB "
            + std::to_string(cap.releaseLibCount) + " 个";
    } else if (cap.canUseRelease) {
        cap.releaseVerdict = "WARN";
        cap.releaseSummary =
            "可加载 Release DLL（" + std::to_string(cap.releaseDllCount)
            + " 个），但缺少 Release .lib";
        cap.notes.push_back("建议在 lib/ 提供与 Release DLL 匹配的导入库");
    } else {
        cap.releaseVerdict = "FAIL";
        cap.releaseSummary = "未发现 Release DLL";
    }

    if (cap.canCompileDebug) {
        cap.debugVerdict = "PASS";
        cap.debugSummary =
            "可编译 Debug：发现 Debug DLL " + std::to_string(cap.debugDllCount)
            + " 个、Debug LIB " + std::to_string(cap.debugLibCount) + " 个";
    } else {
        cap.debugVerdict = "FAIL";
        if (!cap.canUseDebug && !cap.canLinkDebug) {
            cap.debugSummary = "未提供 Debug DLL 与 Debug 链接库（.lib）";
        } else if (!cap.canLinkDebug) {
            cap.debugSummary =
                "有 Debug DLL（" + std::to_string(cap.debugDllCount)
                + "），但缺少 Debug .lib";
        } else {
            cap.debugSummary =
                "有 Debug .lib（" + std::to_string(cap.debugLibCount)
                + "），但缺少 Debug DLL";
        }
        cap.notes.push_back("请仅在 Release 配置下集成，或向厂家索取 Debug 库");
    }

    return cap;
}

std::string PrecheckSummary::StateLabel(TestItemState state) {
    switch (state) {
    case TestItemState::Pass: return "通过";
    case TestItemState::Fail: return "未通过";
    case TestItemState::Warn: return "警告";
    case TestItemState::NotRun: return "未测试";
    case TestItemState::Skipped: return "已跳过";
    }
    return "未知";
}

std::string PrecheckSummary::StateCssClass(TestItemState state) {
    switch (state) {
    case TestItemState::Pass: return "pass";
    case TestItemState::Fail: return "fail";
    case TestItemState::Warn: return "warn";
    case TestItemState::NotRun:
    case TestItemState::Skipped: return "muted";
    }
    return "muted";
}

PrecheckSummaryBoard PrecheckSummary::BuildFromFleet(const FleetSessionReport& fleet) {
    PrecheckSummaryBoard board;

    int headerTotal = 0, headerPass = 0;
    int libTotal = 0, libPass = 0;
    int dllTotal = 0, dllPePass = 0, dllLoadPass = 0, missingDeps = 0;
    bool anyPackage = false;
    bool allReleaseOk = true;
    bool anyDebugFail = false;
    bool anyReleaseFail = false;
    std::string buildReason;

    for (const auto& model : fleet.modelReports) {
        anyPackage = true;
        BuildConfigCapability cap = EvaluateBuildConfig(model.packageFiles, model.modelName);
        if (cap.releaseVerdict == "FAIL") {
            allReleaseOk = false;
            anyReleaseFail = true;
        } else if (cap.releaseVerdict == "WARN") {
            allReleaseOk = false;
        }
        if (cap.debugVerdict != "PASS") anyDebugFail = true;
        board.buildConfigs.push_back(cap);

        if (!buildReason.empty()) buildReason += "；";
        buildReason += (model.modelName.empty() ? std::string("(未命名)") : model.modelName)
            + " [Release:" + cap.releaseVerdict + " / Debug:" + cap.debugVerdict + "]";

        headerTotal += static_cast<int>(model.headerReports.size());
        headerPass += model.passedHeaderCount;
        libTotal += static_cast<int>(model.libReports.size());
        libPass += model.passedLibCount;
        dllTotal += static_cast<int>(model.dllReports.size());
        dllPePass += model.passedDllCount;
        for (const auto& d : model.dllReports) {
            if (d.loadReport.isLoaded) ++dllLoadPass;
            missingDeps += d.peReport.missingDependencyCount;
        }
    }

    {
        TestItemResult item;
        item.id = "build_config";
        item.name = "Release / Debug 构建产物";
        if (!anyPackage) {
            item.state = TestItemState::NotRun;
            item.reason = "尚未扫描型号包";
            item.consequence = "无法判断能否编译 Release 或 Debug";
        } else if (anyReleaseFail) {
            item.state = TestItemState::Fail;
            item.reason = buildReason + "。存在型号缺少 Release DLL。";
            item.consequence = "无法完成 Release 集成加载与链接";
        } else if (anyDebugFail) {
            item.state = TestItemState::Warn;
            item.reason = buildReason + "。Release 可用，但 Debug 产物不完整。";
            item.consequence = "无法编译 Debug 版本；工程切到 Debug 会链接失败（仅能按 Release 集成）";
        } else if (!allReleaseOk) {
            item.state = TestItemState::Warn;
            item.reason = buildReason + "。Release DLL 存在但部分缺少 .lib。";
            item.consequence = "可加载 DLL，但本地/主程序链接导入库时可能报错";
        } else {
            item.state = TestItemState::Pass;
            item.reason = buildReason + "。Release 与 Debug 产物齐全。";
            item.consequence = "Release / Debug 均可编译集成";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "header";
        item.name = "头文件规范检查";
        if (headerTotal == 0) {
            item.state = anyPackage ? TestItemState::Warn : TestItemState::NotRun;
            item.reason = anyPackage ? "未发现头文件" : "尚未执行检测";
            item.consequence = "无法核对接口声明，后续编译与符号一致性风险升高";
        } else if (headerPass == headerTotal) {
            item.state = TestItemState::Pass;
            item.reason = std::to_string(headerPass) + "/" + std::to_string(headerTotal)
                + " 个头文件通过（编码 / extern \"C\" / 导出声明）";
        } else {
            item.state = TestItemState::Fail;
            item.reason = std::to_string(headerPass) + "/" + std::to_string(headerTotal)
                + " 通过；存在编码、linkage 或导出声明问题";
            item.consequence = "编译失败、C++ 名字修饰不匹配或接口无法正确导出";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "header_conflict";
        item.name = "跨型号头文件冲突";
        const auto& conflict = fleet.crossModelHeaderConflictReport;
        if (!anyPackage) {
            item.state = TestItemState::NotRun;
            item.reason = "尚未执行检测";
            item.consequence = "多型号合库时的类型冲突风险未知";
        } else if (fleet.modelReports.size() < 2) {
            item.state = TestItemState::Skipped;
            item.reason = "仅 1 个型号，无需跨型号冲突检测";
            item.consequence = "暂无跨型号冲突风险";
        } else if (conflict.overallPass) {
            item.state = TestItemState::Pass;
            item.reason = "未发现跨型号类型/ODR/命名空间冲突";
        } else {
            item.state = TestItemState::Fail;
            item.reason = "发现跨型号头文件冲突（重名类型 / ODR / 命名空间污染）";
            item.consequence = "多型号合进同一进程后可能偶发崩溃或数据错乱";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "lib";
        item.name = "LIB 库文件检查";
        if (libTotal == 0) {
            item.state = anyPackage ? TestItemState::Warn : TestItemState::NotRun;
            item.reason = anyPackage ? "未发现 LIB 文件" : "尚未执行检测";
            item.consequence = "工程链接导入库可能失败，仅能依赖隐式加载 DLL";
        } else if (libPass == libTotal) {
            item.state = TestItemState::Pass;
            item.reason = std::to_string(libPass) + "/" + std::to_string(libTotal)
                + " 个 LIB 架构/类型/符号检查通过";
        } else {
            item.state = TestItemState::Fail;
            item.reason = std::to_string(libPass) + "/" + std::to_string(libTotal)
                + " 通过；存在架构不匹配或符号异常";
            item.consequence = "链接报错，或链接到错误架构的库";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "dll_pe";
        item.name = "DLL 文件与依赖检查";
        if (!anyPackage || dllTotal == 0) {
            item.state = TestItemState::NotRun;
            item.reason = "尚未执行检测或未发现 DLL";
            item.consequence = "依赖与架构风险未知";
        } else if (dllPePass == dllTotal && missingDeps == 0) {
            item.state = TestItemState::Pass;
            item.reason = std::to_string(dllPePass) + "/" + std::to_string(dllTotal)
                + " PE 通过，依赖完整";
        } else if (dllPePass == dllTotal) {
            item.state = TestItemState::Warn;
            item.reason = "PE 通过，但仍有缺失依赖 " + std::to_string(missingDeps) + " 项";
            item.consequence = "换机或换主程序版本后可能加载失败、表现不一致";
        } else {
            item.state = TestItemState::Fail;
            item.reason = std::to_string(dllPePass) + "/" + std::to_string(dllTotal)
                + " 通过；缺失依赖 " + std::to_string(missingDeps) + " 项";
            item.consequence = "LoadLibrary 失败，或因 CRT/依赖不匹配导致启动异常";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "dll_load";
        item.name = "DLL 接口与加载检查";
        if (!anyPackage || dllTotal == 0) {
            item.state = TestItemState::NotRun;
            item.reason = "尚未执行检测或未发现 DLL";
            item.consequence = "能否安全加载与绑定接口未知";
        } else if (dllLoadPass == dllTotal) {
            item.state = TestItemState::Pass;
            item.reason = std::to_string(dllLoadPass) + "/" + std::to_string(dllTotal)
                + " 加载成功";
        } else {
            item.state = TestItemState::Fail;
            item.reason = std::to_string(dllLoadPass) + "/" + std::to_string(dllTotal)
                + " 加载成功（失败常与依赖、授权或 CRT 有关）";
            item.consequence = "主程序无法加载模型，或初始化阶段即崩溃";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "perf";
        item.name = "UserMain 性能压测";
        if (fleet.perfReport.realtimeVerdict.empty()) {
            item.state = TestItemState::Skipped;
            item.reason = "未编译型号或未执行压测";
            item.consequence = "实时性是否满足帧预算未知";
        } else if (fleet.perfReport.realtimeVerdict == "PASS") {
            item.state = TestItemState::Pass;
            item.reason = "实时性 PASS；Avg "
                + std::to_string(fleet.perfReport.avgTimeMs) + " ms，Max "
                + std::to_string(fleet.perfReport.maxTimeMs) + " ms";
        } else if (fleet.perfReport.realtimeVerdict == "WARNING") {
            item.state = TestItemState::Warn;
            item.reason = "实时性 WARNING；Max "
                + std::to_string(fleet.perfReport.maxTimeMs) + " ms 接近或超过预算";
            item.consequence = "高负载或加速推演时可能掉帧、超时";
        } else {
            item.state = TestItemState::Fail;
            item.reason = "实时性 FAIL；Max "
                + std::to_string(fleet.perfReport.maxTimeMs) + " ms 超出帧预算";
            item.consequence = "加速推演或高频率 Step 时可能卡顿、超时甚至崩溃";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "memory";
        item.name = "内存泄漏监测";
        if (fleet.perfReport.realtimeVerdict.empty()) {
            item.state = TestItemState::Skipped;
            item.reason = "未编译型号或未执行监测";
            item.consequence = "长时间运行内存增长风险未知";
        } else if (fleet.perfReport.memoryLeakRateMBPer10k < 5.0) {
            item.state = TestItemState::Pass;
            item.reason = std::to_string(fleet.perfReport.memoryLeakRateMBPer10k)
                + " MB / 10k 次调用，低于阈值";
        } else {
            item.state = TestItemState::Warn;
            item.reason = std::to_string(fleet.perfReport.memoryLeakRateMBPer10k)
                + " MB / 10k 次调用，增长偏高";
            item.consequence = "长时间仿真可能内存持续上涨，最终 OOM 或不稳定";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "trajectory";
        item.name = "运行轨迹检查";
        if (fleet.trajectoryModelsTested <= 0) {
            item.state = TestItemState::Skipped;
            item.reason = "未编译或未采集到轨迹点";
            item.consequence = "无法核对经纬度输出是否有效";
        } else if (fleet.trajectoryModelsPassed == fleet.trajectoryModelsTested) {
            item.state = TestItemState::Pass;
            item.reason = std::to_string(fleet.trajectoryModelsPassed) + "/"
                + std::to_string(fleet.trajectoryModelsTested) + " 型号轨迹采集成功";
        } else {
            item.state = TestItemState::Fail;
            item.reason = std::to_string(fleet.trajectoryModelsPassed) + "/"
                + std::to_string(fleet.trajectoryModelsTested)
                + " 成功；失败常见于未调用 RecordTrajectoryPoint 或运行异常";
            item.consequence = "态势/轨迹显示异常，或推演过程中返回错误/SEH";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "multimodel";
        item.name = "多型号并行";
        if (fleet.multiModelReport.verdict.empty()) {
            item.state = TestItemState::Skipped;
            item.reason = "需要已编译型号；本次未执行";
            item.consequence = "多型号同场景干扰风险未知";
        } else if (fleet.multiModelReport.verdict == "PASS") {
            item.state = TestItemState::Pass;
            item.reason = fleet.multiModelReport.summary.empty()
                ? "多型号并行测试通过" : fleet.multiModelReport.summary;
        } else if (fleet.multiModelReport.verdict == "WARNING") {
            item.state = TestItemState::Warn;
            item.reason = fleet.multiModelReport.summary;
            item.consequence = "同厂家多模型同场景可能出现数据异常或偶发失败";
        } else {
            item.state = TestItemState::Fail;
            item.reason = fleet.multiModelReport.summary.empty()
                ? "多型号并行测试失败" : fleet.multiModelReport.summary;
            item.consequence = "同厂家不同模型同场景冲突、崩溃或内存破坏";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "multithread";
        item.name = "多线程稳定性";
        if (fleet.multiThreadReport.verdict.empty()) {
            item.state = TestItemState::Skipped;
            item.reason = "需要已编译型号；本次未执行";
            item.consequence = "多线程调度下的崩溃风险未知";
        } else if (fleet.multiThreadReport.verdict == "PASS") {
            item.state = TestItemState::Pass;
            item.reason = fleet.multiThreadReport.summary.empty()
                ? "多线程 UserMain 测试通过" : fleet.multiThreadReport.summary;
        } else if (fleet.multiThreadReport.verdict == "WARNING") {
            item.state = TestItemState::Warn;
            item.reason = fleet.multiThreadReport.summary;
            item.consequence = "主程序开启多线程后可能偶发失败";
        } else {
            item.state = TestItemState::Fail;
            item.reason = fleet.multiThreadReport.summary.empty()
                ? "多线程测试失败/检测到 SEH" : fleet.multiThreadReport.summary;
            item.consequence = "主程序勾选多线程后模型崩溃";
        }
        pushItem(board, item);
    }

    {
        TestItemResult item;
        item.id = "multiobject";
        item.name = "单线程多对象";
        int configured = 0, tested = 0, passed = 0, failed = 0;
        for (const auto& mo : fleet.multiObjectReports) {
            if (mo.configured) ++configured;
            if (!mo.report.verdict.empty()) {
                ++tested;
                if (mo.report.verdict == "PASS") ++passed;
                else if (mo.report.verdict == "FAIL") ++failed;
            }
        }
        if (configured == 0) {
            item.state = TestItemState::Skipped;
            item.reason = "未配置多对象 Harness（可选）";
            item.consequence = "多实例隔离能力未验证";
        } else if (tested == 0) {
            item.state = TestItemState::Skipped;
            item.reason = "已配置但未编译/未验证多对象 Harness";
            item.consequence = "同一武器多枚实例的稳定性未知";
        } else if (failed > 0) {
            item.state = TestItemState::Fail;
            item.reason = std::to_string(passed) + "/" + std::to_string(tested)
                + " 通过；存在串扰、返回码异常或 SEH";
            item.consequence = "同一武器发射多枚时崩溃或状态互相干扰";
        } else {
            item.state = TestItemState::Pass;
            item.reason = std::to_string(passed) + "/" + std::to_string(tested)
                + " 基线/交错测试通过";
        }
        pushItem(board, item);
    }

    return board;
}

std::string PrecheckSummary::ToHtmlSection(const PrecheckSummaryBoard& board,
                                           bool includeBuildConfigRow) {
    std::ostringstream html;
    int pass = 0, fail = 0, warn = 0, pending = 0;
    for (const auto& item : board.items) {
        if (!includeBuildConfigRow && item.id == "build_config") continue;
        switch (item.state) {
        case TestItemState::Pass: ++pass; break;
        case TestItemState::Fail: ++fail; break;
        case TestItemState::Warn: ++warn; break;
        case TestItemState::NotRun:
        case TestItemState::Skipped: ++pending; break;
        }
    }

    html << "  <h2>1. 测试项总览</h2>\n"
         << "  <p>通过 <span class=\"pass\">" << pass
         << "</span> · 未通过 <span class=\"fail\">" << fail
         << "</span> · 警告 <span class=\"warn\">" << warn
         << "</span> · 未测试/跳过 <span class=\"muted\" style=\"font-weight:bold;\">" << pending
         << "</span></p>\n"
         << "  <table>\n"
         << "    <tr><th>测试项</th><th>状态</th><th>具体原因</th><th>可能导致的情况</th></tr>\n";

    for (const auto& item : board.items) {
        if (!includeBuildConfigRow && item.id == "build_config") continue;
        html << "    <tr><td>" << item.name << "</td><td class=\""
             << StateCssClass(item.state) << "\">" << StateLabel(item.state)
             << "</td><td>" << item.reason << "</td><td>" << item.consequence
             << "</td></tr>\n";
    }
    html << "  </table>\n\n";
    return html.str();
}
