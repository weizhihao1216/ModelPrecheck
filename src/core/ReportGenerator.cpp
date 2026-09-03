#include "ReportGenerator.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

std::string ReportGenerator::GenerateHtml(const CombinedPrecheckReport& report) {
    std::stringstream html;

    std::string badgeColor = "#a6adc8"; // gray = partial / not fully run
    std::string verdictText = "PARTIAL";
    const bool headerRanTop = !report.headerPath.empty();
    const bool libRanTop = !report.libPath.empty();
    const bool peRanTop = !report.peReport.filePath.empty() || !report.dllPath.empty();
    const bool loadRanTop = report.loadReport.isLoaded
        || !report.loadReport.errorLog.empty()
        || report.loadReport.exceptionCode != 0
        || report.loadReport.boundSymbolCount > 0
        || report.loadReport.missingSymbolCount > 0;
    const bool perfRanTop = !report.perfReport.realtimeVerdict.empty();
    if (headerRanTop || libRanTop || peRanTop || loadRanTop || perfRanTop
        || !report.multiThreadReport.verdict.empty() || !report.multiModelReport.verdict.empty()) {
        bool anyFail = false;
        if (headerRanTop
            && (!report.headerReport.overallPass || !report.headerConflictReport.overallPass)) anyFail = true;
        if (libRanTop && !report.libReport.overallPass) anyFail = true;
        if (peRanTop && !report.peReport.overallPass) anyFail = true;
        if (loadRanTop && !report.loadReport.isLoaded) anyFail = true;
        if (perfRanTop && report.perfReport.realtimeVerdict == "FAIL") anyFail = true;
        if (report.multiThreadReport.verdict == "FAIL") anyFail = true;
        if (report.multiModelReport.verdict == "FAIL") anyFail = true;
        if (anyFail) {
            badgeColor = "#f38ba8";
            verdictText = "FAIL";
        } else {
            badgeColor = "#a6e3a1";
            verdictText = "PASS";
        }
        if (perfRanTop && report.perfReport.realtimeVerdict == "WARNING" && verdictText != "FAIL") {
            badgeColor = "#f9e2af";
            verdictText = "WARNING";
        }
    } else {
        verdictText = "N/A";
    }

    html << "<!DOCTYPE html>\n<html>\n<head>\n"
         << "<meta charset=\"utf-8\">\n"
         << "<title>第三方武器模型DLL集成预检报告</title>\n"
         << "<style>\n"
         << "  body { font-family: 'Segoe UI', Microsoft YaHei, sans-serif; margin: 0; padding: 20px; background-color: #1e1e2e; color: #cdd6f4; }\n"
         << "  .container { max-width: 1000px; margin: 0 auto; background: #181825; padding: 30px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.5); }\n"
         << "  h1, h2, h3 { color: #89b4fa; border-bottom: 1px solid #313244; padding-bottom: 8px; }\n"
         << "  .badge { display: inline-block; padding: 6px 16px; border-radius: 20px; color: #11111b; font-weight: bold; font-size: 18px; text-align: center; }\n"
         << "  table { width: 100%; border-collapse: collapse; margin: 15px 0; background: #11111b; border-radius: 8px; overflow: hidden; }\n"
         << "  th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #313244; }\n"
         << "  th { background-color: #313244; color: #89b4fa; font-weight: 600; }\n"
         << "  tr:hover { background-color: #1e1e2e; }\n"
         << "  .pass { color: #a6e3a1; font-weight: bold; }\n"
         << "  .warn { color: #f9e2af; font-weight: bold; }\n"
         << "  .fail { color: #f38ba8; font-weight: bold; }\n"
         << "  .card { background: #313244; padding: 15px; border-radius: 8px; margin-bottom: 15px; }\n"
         << "  .log-box { background: #11111b; padding: 12px; font-family: Consolas, monospace; font-size: 13px; max-height: 250px; overflow-y: auto; border: 1px solid #45475a; border-radius: 6px; }\n"
         << "</style>\n</head>\n<body>\n";

    html << "<div class=\"container\">\n"
         << "  <div style=\"display: flex; justify-content: space-between; align-items: center;\">\n"
         << "    <div>\n"
         << "      <h1>第三方武器模型 DLL 集成预检报告</h1>\n"
         << "      <p style=\"color: #a6adc8;\">目标文件: <code>" << report.dllPath << "</code></p>\n"
         << "      <p style=\"color: #a6adc8;\">生成时间: " << report.timestamp << "</p>\n"
         << "    </div>\n"
         << "    <div>\n"
         << "      <span class=\"badge\" style=\"background-color: " << badgeColor << ";\">" << verdictText << "</span>\n"
         << "    </div>\n"
         << "  </div>\n\n";

     // Summary Table
    html << "  <h2>1. 预检综合判定矩阵</h2>\n"
         << "  <table>\n"
         << "    <tr><th>测试维度</th><th>关键指标</th><th>测试结果</th><th>判定状态</th></tr>\n";

    if (!report.headerPath.empty()) {
        const bool headerPass = report.headerReport.overallPass
            && report.headerConflictReport.overallPass;
        html << "    <tr><td>1. 头文件(.h)规范预检</td><td>编码校验/extern \"C\"/接口原型</td><td>编码: "
             << report.headerReport.encoding << " | extern \"C\": " << (report.headerReport.hasExternC ? "有" : "无")
             << " | 重名: " << report.headerConflictReport.duplicateTypeCount
             << " | ODR: " << report.headerConflictReport.odrConflictCount
             << " | 命名空间污染: " << report.headerConflictReport.namespacePollutionCount
             << "</td><td class=\"" << (headerPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
    } else {
        html << "    <tr><td>1. 头文件(.h)规范预检</td><td>编码校验/extern \"C\"/接口原型</td>"
             << "<td>未执行预检</td><td class=\"warn\">N/A</td></tr>\n";
    }

    if (!report.libPath.empty()) {
        html << "    <tr><td>2. LIB 库(.lib)规范预检</td><td>COFF架构/导入库类型/符号匹配</td><td>"
             << report.libReport.architecture << " / " << report.libReport.libType
             << "</td><td class=\"" << (report.libReport.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
    } else {
        html << "    <tr><td>2. LIB 库(.lib)规范预检</td><td>COFF架构/导入库类型/符号匹配</td>"
             << "<td>未执行预检</td><td class=\"warn\">N/A</td></tr>\n";
    }

    // PE / Load / Perf: distinguish "not run" from real FAIL
    const bool peRan = !report.peReport.filePath.empty() || !report.dllPath.empty();
    const bool pePass = peRan && report.peReport.overallPass;
    const bool loadRan = report.loadReport.isLoaded
        || !report.loadReport.errorLog.empty()
        || report.loadReport.exceptionCode != 0
        || report.loadReport.boundSymbolCount > 0
        || report.loadReport.missingSymbolCount > 0;
    const bool perfRan = !report.perfReport.realtimeVerdict.empty();
    const bool trajectoryRan = report.trajReport.totalDataPoints > 0;
    const bool multiModelRan = !report.multiModelReport.verdict.empty();
    const bool multiThreadRan = !report.multiThreadReport.verdict.empty();
    const bool multiObjectRan = !report.multiObjectReport.verdict.empty();

    auto statusCell = [](bool ran, bool pass) -> std::string {
        if (!ran) return "warn\">N/A";
        return pass ? "pass\">PASS" : "fail\">FAIL";
    };

    html << "    <tr><td>3. DLL 文件与依赖检查</td><td>架构匹配/运行库类型/依赖文件完整性</td><td>"
         << (peRan ? (report.peReport.architecture + " / " + report.peReport.crtLinkage) : std::string("未执行一键预检"))
         << "</td><td class=\"" << statusCell(peRan, pePass) << "</td></tr>\n"
         << "    <tr><td>4. DLL 接口与加载检查</td><td>导出接口/安全加载/异常保护</td><td>";
    if (!loadRan) {
        html << "未执行一键预检</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        const bool interfaceLoadPass = report.loadReport.isLoaded
            && report.loadReport.missingSymbolCount == 0;
        html << (report.loadReport.isLoaded ? "加载成功" : "加载失败")
             << "，缺少接口: " << report.loadReport.missingSymbolCount << " 个</td><td class=\""
             << statusCell(true, interfaceLoadPass) << "</td></tr>\n";
    }
    html << "    <tr><td>5. UserMain 性能压测</td><td>平均/最大耗时 ( Budget: " << report.perfReport.frameBudgetMs << " ms)</td><td>";
    if (!perfRan) {
        html << "未执行压测</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        html << "Avg: " << report.perfReport.avgTimeMs << " ms, Max: " << report.perfReport.maxTimeMs << " ms</td><td class=\""
             << (report.perfReport.realtimeVerdict == "PASS" ? "pass\">PASS"
                : (report.perfReport.realtimeVerdict == "WARNING" ? "warn\">WARNING" : "fail\">FAIL"))
             << "</td></tr>\n";
    }
    html << "    <tr><td>6. 内存泄露监测</td><td>每 10k 次 UserMain 内存增长</td><td>";
    if (!perfRan) {
        html << "未执行压测</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        html << report.perfReport.memoryLeakRateMBPer10k << " MB / 10k</td><td class=\""
             << (report.perfReport.memoryLeakRateMBPer10k < 5.0 ? "pass\">PASS" : "warn\">WARN") << "</td></tr>\n";
    }
    html << "    <tr><td>7. 运行轨迹检查</td><td>经纬度路径点采集</td><td>"
         << (trajectoryRan ? (std::to_string(report.trajReport.totalDataPoints) + " 个路径点") : "未执行")
         << "</td><td class=\"" << statusCell(trajectoryRan, report.trajReport.overallPass)
         << "</td></tr>\n";
    html << "    <tr><td>8. 多型号并行</td><td>多路径头文件/DLL 按数量并发</td><td>"
         << (multiModelRan ? report.multiModelReport.summary : "未执行")
         << "</td><td class=\""
         << (!multiModelRan ? "warn\">N/A"
            : (report.multiModelReport.verdict == "PASS" ? "pass\">PASS"
               : (report.multiModelReport.verdict == "WARNING" ? "warn\">WARNING" : "fail\">FAIL")))
         << "</td></tr>\n"
         << "    <tr><td>9. 多线程稳定性</td><td>并行多线程 UserMain</td><td>"
         << (multiThreadRan ? report.multiThreadReport.summary : "未执行")
         << "</td><td class=\""
         << (!multiThreadRan ? "warn\">N/A"
            : (report.multiThreadReport.verdict == "PASS" ? "pass\">PASS"
               : (report.multiThreadReport.verdict == "WARNING" ? "warn\">WARNING" : "fail\">FAIL")))
         << "</td></tr>\n";
    html << "    <tr><td>10. 单线程多对象测试</td><td>逐对象基线/单线程交错/状态串扰</td><td>"
         << (multiObjectRan ? report.multiObjectReport.summary
                            : (report.multiObjectConfigured ? "映射 Harness 尚未生成或未执行"
                                                            : "未完成多对象接口映射"))
         << "</td><td class=\""
         << (!multiObjectRan ? "warn\">N/A"
             : (report.multiObjectReport.verdict == "PASS" ? "pass\">PASS" : "fail\">FAIL"))
         << "</td></tr>\n"
         << "  </table>\n\n";

    // Header File Section
    if (!report.headerPath.empty()) {
        html << "  <h2>2. 头文件 (.h) 接口规范预检分析</h2>\n"
             << "  <div class=\"card\">\n"
             << "    <p><b>头文件路径:</b> <code>" << report.headerPath << "</code></p>\n"
             << "    <p><b>文本编码格式:</b> " << report.headerReport.encoding << "</p>\n"
             << "    <p><b>extern \"C\" 保护:</b> " << (report.headerReport.hasExternC ? "<span class=\"pass\">已包含</span>" : "<span class=\"fail\">未检测到</span>") << "</p>\n"
             << "    <p><b>__declspec 动态库宏:</b> " << (report.headerReport.hasDeclspec ? "<span class=\"pass\">包含</span>" : "无") << "</p>\n"
             << "    <p><b>结构体/类型重名:</b> " << report.headerConflictReport.duplicateTypeCount
             << " | <b>ODR 冲突:</b> " << report.headerConflictReport.odrConflictCount
             << " | <b>命名空间污染风险:</b> " << report.headerConflictReport.namespacePollutionCount
             << "</p>\n  </div>\n";
        if (!report.headerConflictReport.issues.empty()) {
            html << "  <table><tr><th>类型</th><th>级别</th><th>符号</th><th>说明</th></tr>\n";
            for (const auto& issue : report.headerConflictReport.issues) {
                html << "  <tr><td>" << issue.category << "</td><td class=\""
                     << (issue.severity == "FAIL" ? "fail" : "warn") << "\">"
                     << issue.severity << "</td><td>" << issue.symbol
                     << "</td><td>" << issue.detail << "</td></tr>\n";
            }
            html << "  </table>\n";
        }
        html << "\n";
    }

    // LIB Library Section
    if (!report.libPath.empty()) {
        html << "  <h2>3. LIB 库 (.lib) 静态结构预检分析</h2>\n"
             << "  <div class=\"card\">\n"
             << "    <p><b>LIB 库路径:</b> <code>" << report.libPath << "</code></p>\n"
             << "    <p><b>目标 CPU 架构:</b> " << report.libReport.architecture << "</p>\n"
             << "    <p><b>归档库鉴定类型:</b> " << report.libReport.libType << "</p>\n"
             << "  </div>\n\n";
    }

    // Section 4: PE Analysis
    html << "  <h2>4. 静态 PE 结构与依赖库分析</h2>\n"
         << "  <div class=\"card\">\n"
         << "    <p><b>目标 CPU 架构:</b> " << report.peReport.architecture << " (宿主框架: x64)</p>\n"
         << "    <p><b>CRT 链接模式:</b> " << report.peReport.crtLinkage << "</p>\n"
         << "    <p><b>缺失依赖项数量:</b> " << report.peReport.missingDependencyCount << " 个</p>\n"
         << "  </div>\n"
         << "  <h3>4.1 依赖库扫描明细 (Import Directory)</h3>\n"
         << "  <table>\n"
         << "    <tr><th>依赖 DLL 名称</th><th>解析状态</th><th>系统 / 物理路径</th></tr>\n";
    for (const auto& dep : report.peReport.importedDlls) {
        html << "    <tr><td>" << dep.name << "</td><td class=\"" << (dep.found ? "pass\">FOUND" : "fail\">MISSING")
             << "</td><td><code>" << dep.resolvedPath << "</code></td></tr>\n";
    }
    html << "  </table>\n\n";

    // Section 5: Export Symbols
    html << "  <h3>4.2 导出接口函数比对 (Export Directory)</h3>\n"
         << "  <table>\n"
         << "    <tr><th>导出函数名</th><th>Ordinal 序号</th><th>接口匹配状态</th></tr>\n";
    for (const auto& exp : report.peReport.exportedSymbols) {
        html << "    <tr><td><code>" << exp.name << "</code></td><td>" << exp.ordinal << "</td><td class=\""
             << (exp.isRequiredInterface ? "pass\">Core Interface" : "pass\">Optional") << "</td></tr>\n";
    }
    html << "  </table>\n\n";

    // Section 6: Performance Profiling (UserMain)
    html << "  <h2>5. 性能压力（UserMain 重复执行）</h2>\n"
         << "  <div class=\"card\">\n"
         << "    <p><b>UserMain 执行次数:</b> " << report.perfReport.completedSteps << " / " << report.perfReport.totalSteps << "</p>\n"
         << "    <p><b>单次最小耗时:</b> " << report.perfReport.minTimeMs << " ms</p>\n"
         << "    <p><b>单次最大耗时:</b> " << report.perfReport.maxTimeMs << " ms</p>\n"
         << "    <p><b>单次平均耗时:</b> " << report.perfReport.avgTimeMs << " ms</p>\n"
         << "    <p><b>耗时抖动 (StdDev):</b> " << report.perfReport.jitterMs << " ms</p>\n"
         << "    <p><b>压测内存增量:</b> " << report.perfReport.memoryDeltaMB << " MB</p>\n"
         << "  </div>\n\n";

    auto emitConcCard = [&](const char* title, const ConcurrencyTestReport& cr) {
        html << "  <h2>" << title << "</h2>\n"
             << "  <div class=\"card\">\n"
             << "    <p><b>数量:</b> " << cr.workerCount << "</p>\n"
             << "    <p><b>成功 / 用户失败 / 异常:</b> " << cr.successCount
             << " / " << cr.userFailCount
             << " / " << cr.exceptionCount << "</p>\n"
             << "    <p><b>判定:</b> <span class=\""
             << (cr.verdict == "PASS" ? "pass" : (cr.verdict == "WARNING" ? "warn" : "fail"))
             << "\">" << (cr.verdict.empty() ? "N/A" : cr.verdict) << "</span></p>\n"
             << "    <p>" << cr.summary << "</p>\n"
             << "  </div>\n\n";
    };
    emitConcCard("6. 多型号并行（多路径 DLL × 各型号数量）", report.multiModelReport);
    emitConcCard("7. 多线程稳定性（并行 UserMain）", report.multiThreadReport);

    html << "  <h2>8. 单线程多对象基线/交错测试</h2>\n"
         << "  <div class=\"card\">\n";
    if (!multiObjectRan) {
        html << "    <p class=\"warn\">"
             << (report.multiObjectConfigured ? "映射 Harness 尚未生成或未执行"
                                              : "未完成多对象接口映射")
             << "</p>\n";
    } else {
        html << "    <p><b>对象数/步数:</b> " << report.multiObjectReport.objectCount
             << " / " << report.multiObjectReport.stepCount << "</p>\n"
             << "    <p><b>最大位置偏差:</b> " << report.multiObjectReport.maxPositionDeviation
             << "（容差 " << report.multiObjectReport.tolerance << "）</p>\n"
             << "    <p><b>异常/状态串扰:</b> " << report.multiObjectReport.exceptionCount
             << " / " << report.multiObjectReport.interferenceCount << "</p>\n"
             << "    <p><b>最大单帧耗时:</b> " << report.multiObjectReport.maxFrameTimeMs
             << " ms | <b>内存变化:</b> " << report.multiObjectReport.memoryDeltaMB << " MB</p>\n"
             << "    <p class=\"" << (report.multiObjectReport.verdict == "PASS" ? "pass" : "fail")
             << "\">" << report.multiObjectReport.verdict << " — "
             << report.multiObjectReport.summary << "</p>\n";
    }
    html << "  </div>\n\n";

    // Section Logs
    html << "  <h2>9. 预检过程日志追踪</h2>\n"
         << "  <div class=\"log-box\">\n";
    for (const auto& logMsg : report.headerReport.logMessages) {
        html << "    <div>[Header] " << logMsg << "</div>\n";
    }
    for (const auto& logMsg : report.libReport.logMessages) {
        html << "    <div>[LIB] " << logMsg << "</div>\n";
    }
    for (const auto& logMsg : report.peReport.logMessages) {
        html << "    <div>[PE] " << logMsg << "</div>\n";
    }
    for (const auto& logMsg : report.multiModelReport.logMessages) {
        html << "    <div>[MultiModel] " << logMsg << "</div>\n";
    }
    for (const auto& logMsg : report.multiThreadReport.logMessages) {
        html << "    <div>[MultiThread] " << logMsg << "</div>\n";
    }
    for (const auto& logMsg : report.multiObjectReport.logMessages) {
        html << "    <div>[MultiObject] " << logMsg << "</div>\n";
    }
    if (!report.perfReport.exceptionLog.empty()) {
        html << "    <div class=\"fail\">" << report.perfReport.exceptionLog << "</div>\n";
    }
    html << "  </div>\n";

    html << "</div>\n</body>\n</html>\n";
    return html.str();
}

bool ReportGenerator::SaveReportToFile(const CombinedPrecheckReport& report, const std::string& outputPath) {
    std::string html = GenerateHtml(report);
    std::ofstream file(outputPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << html;
    file.close();
    return true;
}

std::string ReportGenerator::GenerateDualBuildHtml(const DualBuildPrecheckReport& dualReport) {
    std::stringstream html;

    std::string badgeColor = dualReport.overallPass ? "#a6e3a1" : "#f38ba8";
    std::string verdictText = dualReport.overallPass ? "PASS" : "FAIL";

    html << "<!DOCTYPE html>\n<html>\n<head>\n"
         << "<meta charset=\"utf-8\">\n"
         << "<title>三方武器模型库 Release & Debug 双版本集成预检报告</title>\n"
         << "<style>\n"
         << "  body { font-family: 'Segoe UI', Microsoft YaHei, sans-serif; margin: 0; padding: 20px; background-color: #1e1e2e; color: #cdd6f4; }\n"
         << "  .container { max-width: 1050px; margin: 0 auto; background: #181825; padding: 30px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.5); }\n"
         << "  h1, h2, h3 { color: #89b4fa; border-bottom: 1px solid #313244; padding-bottom: 8px; }\n"
         << "  .badge { display: inline-block; padding: 6px 16px; border-radius: 20px; color: #11111b; font-weight: bold; font-size: 18px; text-align: center; }\n"
         << "  table { width: 100%; border-collapse: collapse; margin: 15px 0; background: #11111b; border-radius: 8px; overflow: hidden; }\n"
         << "  th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #313244; }\n"
         << "  th { background-color: #313244; color: #89b4fa; font-weight: 600; }\n"
         << "  tr:hover { background-color: #1e1e2e; }\n"
         << "  .pass { color: #a6e3a1; font-weight: bold; }\n"
         << "  .warn { color: #f9e2af; font-weight: bold; }\n"
         << "  .fail { color: #f38ba8; font-weight: bold; }\n"
         << "  .card { background: #313244; padding: 15px; border-radius: 8px; margin-bottom: 15px; }\n"
         << "  .log-box { background: #11111b; padding: 12px; font-family: Consolas, monospace; font-size: 13px; max-height: 250px; overflow-y: auto; border: 1px solid #45475a; border-radius: 6px; }\n"
         << "</style>\n</head>\n<body>\n";

    html << "<div class=\"container\">\n"
         << "  <div style=\"display: flex; justify-content: space-between; align-items: center;\">\n"
         << "    <div>\n"
         << "      <h1>三方武器模型包 (Release & Debug 双版本) 预检报告</h1>\n"
         << "      <p style=\"color: #a6adc8;\">型号: <b>" << (dualReport.modelName.empty() ? "(未命名)" : dualReport.modelName) << "</b></p>\n"
         << "      <p style=\"color: #a6adc8;\">模型包路径: <code>" << dualReport.packageDir << "</code></p>\n"
         << "      <p style=\"color: #a6adc8;\">生成时间: " << dualReport.timestamp << "</p>\n"
         << "    </div>\n"
         << "    <div>\n"
         << "      <span class=\"badge\" style=\"background-color: " << badgeColor << ";\">" << verdictText << "</span>\n"
         << "    </div>\n"
         << "  </div>\n\n";

    // Summary Overview Table
    html << "  <h2>1. 全文件预检判定概览</h2>\n"
         << "  <table>\n"
         << "    <tr><th>文件类别</th><th>全包发现数量</th><th>通过检验数量</th><th>总体状态判定</th></tr>\n"
         << "    <tr><td>1. C/C++ 头文件 (.h / .hpp)</td><td>" << dualReport.headerReports.size() << " 个</td><td>" << dualReport.passedHeaderCount << " 个</td><td class=\"" << (dualReport.passedHeaderCount == dualReport.headerReports.size() && !dualReport.headerReports.empty() ? "pass\">PASS" : "fail\">FAIL / N/A") << "</td></tr>\n"
         << "    <tr><td>2. LIB 库文件 (.lib)</td><td>" << dualReport.libReports.size() << " 个</td><td>" << dualReport.passedLibCount << " 个</td><td class=\"" << (dualReport.passedLibCount == dualReport.libReports.size() && !dualReport.libReports.empty() ? "pass\">PASS" : "fail\">FAIL / N/A") << "</td></tr>\n"
         << "    <tr><td>3. DLL 动态库文件 (.dll)</td><td>" << dualReport.dllReports.size() << " 个</td><td>" << dualReport.passedDllCount << " 个</td><td class=\"" << (dualReport.passedDllCount == dualReport.dllReports.size() && !dualReport.dllReports.empty() ? "pass\">PASS" : "fail\">FAIL / N/A") << "</td></tr>\n"
         << "  </table>\n\n";

    // Section 2: Header Files List
    html << "  <h2>2. C/C++ 头文件全清单预检结果</h2>\n";
    if (!dualReport.headerReports.empty()) {
        html << "  <table>\n"
             << "    <tr><th>头文件路径</th><th>文本编码</th><th>extern \"C\" 保护</th><th>提取接口函数数</th><th>预检判定</th></tr>\n";
        for (const auto& hRep : dualReport.headerReports) {
            html << "    <tr><td><code>" << hRep.filePath << "</code></td>"
                 << "<td>" << hRep.encoding << "</td>"
                 << "<td>" << (hRep.hasExternC ? "有" : "无") << "</td>"
                 << "<td>" << hRep.declaredFunctions.size() << " 个</td>"
                 << "<td class=\"" << (hRep.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
        }
        html << "  </table>\n\n";
    } else {
        html << "  <p>未发现头文件。</p>\n";
    }

    // Section 3: LIB Files List
    html << "  <h2>3. LIB 库全清单预检结果</h2>\n";
    if (!dualReport.libReports.empty()) {
        html << "  <table>\n"
             << "    <tr><th>LIB 库文件路径</th><th>目标 CPU 架构</th><th>库类型区分</th><th>预检判定</th></tr>\n";
        for (const auto& lRep : dualReport.libReports) {
            html << "    <tr><td><code>" << lRep.filePath << "</code></td>"
                 << "<td>" << lRep.architecture << "</td>"
                 << "<td>" << lRep.libType << "</td>"
                 << "<td class=\"" << (lRep.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
        }
        html << "  </table>\n\n";
    } else {
        html << "  <p>未发现 LIB 库文件。</p>\n";
    }

    // Section 4: DLL Files List
    html << "  <h2>4. DLL 动态库全清单与推演预检结果</h2>\n";
    if (!dualReport.dllReports.empty()) {
        html << "  <table>\n"
             << "    <tr><th>DLL 文件路径</th><th>版本配置</th><th>CPU 架构与 CRT 模式</th><th>SEH 加载</th><th>推演平均耗时</th><th>预检判定</th></tr>\n";
        for (const auto& dRep : dualReport.dllReports) {
            html << "    <tr><td><code>" << dRep.dllPath << "</code></td>"
                 << "<td>" << dRep.buildConfig << "</td>"
                 << "<td>" << dRep.peReport.architecture << " / " << dRep.peReport.crtLinkage << "</td>"
                 << "<td class=\"" << (dRep.loadReport.isLoaded ? "pass\">PASS" : "fail\">FAIL") << "</td>"
                 << "<td>" << dRep.perfReport.avgTimeMs << " ms</td>"
                 << "<td class=\"" << (dRep.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
        }
        html << "  </table>\n\n";
    } else {
        html << "  <p>未发现 DLL 动态库文件。</p>\n";
    }

    // Section 5: Consistency Verification
    const auto& cReport = dualReport.consistencyReport;
    html << "  <h2>5. 全包接口声明与导出符号一致性校验</h2>\n"
         << "  <div class=\"card\">\n"
         << "    <p><b>全包提取到头文件声明:</b> " << cReport.declaredInHeader.size() << " 个</p>\n"
         << "    <p><b>全包二进制导出符号:</b> " << cReport.exportedInBinary.size() << " 个</p>\n"
         << "    <p><b>接口匹配一致率:</b> <span class=\"" << (cReport.isFullyConsistent ? "pass" : "warn") << "\">" << cReport.consistencyRatio << "%</span></p>\n";

    if (!cReport.matchedFunctions.empty()) {
        html << "    <p><b>✅ 匹配一致的函数原型 (" << cReport.matchedFunctions.size() << " 个):</b> ";
        for (size_t i = 0; i < cReport.matchedFunctions.size(); ++i) {
            html << "<code>" << cReport.matchedFunctions[i] << "</code>" << (i + 1 < cReport.matchedFunctions.size() ? ", " : "");
        }
        html << "</p>\n";
    }

    if (!cReport.declaredButNotExported.empty()) {
        html << "    <p class=\"fail\"><b>⚠️ 头文件声明但二进制未导出 (" << cReport.declaredButNotExported.size() << " 个，可能引发 LNK2019 链接错误):</b> ";
        for (size_t i = 0; i < cReport.declaredButNotExported.size(); ++i) {
            html << "<code>" << cReport.declaredButNotExported[i] << "</code>" << (i + 1 < cReport.declaredButNotExported.size() ? ", " : "");
        }
        html << "</p>\n";
    }
    html << "  </div>\n\n";

    // Section 6: Logs
    html << "  <h2>6. 预检过程综合日志</h2>\n"
         << "  <div class=\"log-box\">\n";
    for (const auto& logMsg : dualReport.packageFiles.scanLog) {
        html << "    <div>[Scan] " << logMsg << "</div>\n";
    }
    for (const auto& hRep : dualReport.headerReports) {
        for (const auto& logMsg : hRep.logMessages) {
            html << "    <div>[Header] " << logMsg << "</div>\n";
        }
    }
    for (const auto& lRep : dualReport.libReports) {
        for (const auto& logMsg : lRep.logMessages) {
            html << "    <div>[LIB] " << logMsg << "</div>\n";
        }
    }
    for (const auto& dRep : dualReport.dllReports) {
        for (const auto& logMsg : dRep.peReport.logMessages) {
            html << "    <div>[DLL-PE] " << logMsg << "</div>\n";
        }
    }
    html << "  </div>\n";

    html << "</div>\n</body>\n</html>\n";
    return html.str();
}

bool ReportGenerator::SaveDualReportToFile(const DualBuildPrecheckReport& dualReport, const std::string& outputPath) {
    std::string html = GenerateDualBuildHtml(dualReport);
    std::ofstream file(outputPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << html;
    file.close();
    return true;
}

std::string ReportGenerator::GenerateFleetHtml(const FleetSessionReport& fleetReport) {
    std::stringstream html;

    int headerTotal = 0, headerPass = 0, libTotal = 0, libPass = 0;
    int duplicateTypes = 0, odrConflicts = 0, namespaceRisks = 0;
    bool headerConflictsPass = true;
    int dllTotal = 0, dllPePass = 0, dllLoadPass = 0, missingExports = 0;
    bool peRan = false;
    for (const auto& m : fleetReport.modelReports) {
        headerTotal += static_cast<int>(m.headerReports.size());
        headerPass += m.passedHeaderCount;
        libTotal += static_cast<int>(m.libReports.size());
        libPass += m.passedLibCount;
        for (const auto& d : m.dllReports) {
            ++dllTotal;
            if (d.peReport.overallPass) ++dllPePass;
            if (d.loadReport.isLoaded) ++dllLoadPass;
            missingExports += d.loadReport.missingSymbolCount;
        }
    }
    duplicateTypes = fleetReport.crossModelHeaderConflictReport.duplicateTypeCount;
    odrConflicts = fleetReport.crossModelHeaderConflictReport.odrConflictCount;
    namespaceRisks = fleetReport.crossModelHeaderConflictReport.namespacePollutionCount;
    headerConflictsPass = fleetReport.crossModelHeaderConflictReport.overallPass;
    peRan = dllTotal > 0;
    const bool perfRan = !fleetReport.perfReport.realtimeVerdict.empty();
    const bool multiModelRan = !fleetReport.multiModelReport.verdict.empty();
    const bool multiThreadRan = !fleetReport.multiThreadReport.verdict.empty();
    int multiObjectConfigured = 0;
    int multiObjectCompiled = 0;
    int multiObjectTested = 0;
    int multiObjectPassed = 0;
    for (const auto& model : fleetReport.multiObjectReports) {
        if (model.configured) ++multiObjectConfigured;
        if (model.harnessCompiled) ++multiObjectCompiled;
        if (!model.report.verdict.empty()) {
            ++multiObjectTested;
            if (model.report.verdict == "PASS") ++multiObjectPassed;
        }
    }
    const bool multiObjectRan = multiObjectTested > 0;
    const bool packageRan = headerTotal > 0 || libTotal > 0 || peRan;

    bool anyFail = peRan && (dllPePass < dllTotal || dllLoadPass < dllTotal);
    if (headerTotal > 0 && headerPass < headerTotal) anyFail = true;
    if (!headerConflictsPass) anyFail = true;
    if (libTotal > 0 && libPass < libTotal) anyFail = true;
    if (perfRan && fleetReport.perfReport.realtimeVerdict == "FAIL") anyFail = true;
    if (fleetReport.trajectoryModelsTested > 0
        && fleetReport.trajectoryModelsPassed != fleetReport.trajectoryModelsTested) anyFail = true;
    if (fleetReport.multiModelReport.verdict == "FAIL") anyFail = true;
    if (fleetReport.multiThreadReport.verdict == "FAIL") anyFail = true;
    if (multiObjectRan && multiObjectPassed < multiObjectTested) anyFail = true;
    if (!fleetReport.overallPass && peRan) anyFail = true;

    std::string badgeColor = anyFail ? "#f38ba8" : (packageRan || perfRan || multiModelRan || multiThreadRan || multiObjectRan ? "#a6e3a1" : "#a6adc8");
    std::string verdictText = anyFail ? "FAIL" : (packageRan || perfRan || multiModelRan || multiThreadRan || multiObjectRan ? "PASS" : "N/A");

    auto statusCell = [](bool ran, bool pass) -> std::string {
        if (!ran) return "warn\">N/A";
        return pass ? "pass\">PASS" : "fail\">FAIL";
    };

    html << "<!DOCTYPE html>\n<html>\n<head>\n"
         << "<meta charset=\"utf-8\">\n"
         << "<title>多型号武器模型预检总报告</title>\n"
         << "<style>\n"
         << "  body { font-family: 'Segoe UI', Microsoft YaHei, sans-serif; margin: 0; padding: 20px; background-color: #1e1e2e; color: #cdd6f4; }\n"
         << "  .container { max-width: 1100px; margin: 0 auto; background: #181825; padding: 30px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.5); }\n"
         << "  h1, h2, h3 { color: #89b4fa; border-bottom: 1px solid #313244; padding-bottom: 8px; }\n"
         << "  .badge { display: inline-block; padding: 6px 16px; border-radius: 20px; color: #11111b; font-weight: bold; font-size: 18px; }\n"
         << "  table { width: 100%; border-collapse: collapse; margin: 15px 0; background: #11111b; border-radius: 8px; overflow: hidden; }\n"
         << "  th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #313244; }\n"
         << "  th { background-color: #313244; color: #89b4fa; font-weight: 600; }\n"
         << "  .pass { color: #a6e3a1; font-weight: bold; }\n"
         << "  .fail { color: #f38ba8; font-weight: bold; }\n"
         << "  .warn { color: #f9e2af; font-weight: bold; }\n"
         << "  .card { background: #313244; padding: 15px; border-radius: 8px; margin-bottom: 15px; }\n"
         << "  .model-section { border: 1px solid #45475a; border-radius: 10px; padding: 16px; margin: 24px 0; background: #1e1e2e; }\n"
         << "  .log-box { background: #11111b; padding: 12px; font-family: Consolas, monospace; font-size: 13px; max-height: 320px; overflow-y: auto; border: 1px solid #45475a; border-radius: 6px; }\n"
         << "</style>\n</head>\n<body>\n<div class=\"container\">\n";

    html << "  <div style=\"display:flex;justify-content:space-between;align-items:center;\">\n"
         << "    <div>\n"
         << "      <h1>多型号武器模型预检总报告</h1>\n"
         << "      <p style=\"color:#a6adc8;\">型号数量: " << fleetReport.modelReports.size() << "</p>\n"
         << "      <p style=\"color:#a6adc8;\">生成时间: " << fleetReport.timestamp << "</p>\n"
         << "    </div>\n"
         << "    <span class=\"badge\" style=\"background-color:" << badgeColor << ";\">" << verdictText << "</span>\n"
         << "  </div>\n\n";

    // —— 综合判定矩阵（与单报告一致，始终存在）——
    html << "  <h2>1. 预检综合判定矩阵</h2>\n"
         << "  <table>\n"
         << "    <tr><th>测试维度</th><th>关键指标</th><th>测试结果</th><th>判定状态</th></tr>\n";

    html << "    <tr><td>1. 头文件(.h)规范预检</td><td>编码/extern \"C\"/接口原型</td><td>"
         << (headerTotal > 0 ? (std::to_string(headerPass) + "/" + std::to_string(headerTotal) + " 通过")
                             : "未发现头文件")
         << "；重名 " << duplicateTypes << "，ODR " << odrConflicts
         << "，命名空间污染风险 " << namespaceRisks
         << "</td><td class=\"" << statusCell(headerTotal > 0,
                                               headerPass == headerTotal && headerConflictsPass)
         << "</td></tr>\n";

    html << "    <tr><td>2. LIB 库(.lib)规范预检</td><td>COFF架构/库类型/接口符号</td><td>"
         << (libTotal > 0 ? (std::to_string(libPass) + "/" + std::to_string(libTotal) + " 通过")
                          : "未发现 LIB 文件")
         << "</td><td class=\"" << statusCell(libTotal > 0, libPass == libTotal)
         << "</td></tr>\n";

    html << "    <tr><td>3. DLL 文件与依赖检查</td><td>架构匹配/运行库类型/依赖文件完整性</td><td>";
    if (!peRan) {
        html << "未执行一键预检</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        html << "全型号 DLL " << dllPePass << "/" << dllTotal << " 通过 PE</td><td class=\""
             << statusCell(true, dllPePass == dllTotal && dllTotal > 0) << "</td></tr>\n";
    }

    html << "    <tr><td>4. DLL 接口与加载检查</td><td>导出接口/安全加载/异常保护</td><td>";
    if (!peRan) {
        html << "未执行一键预检</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        const bool pass = missingExports == 0 && dllLoadPass == dllTotal;
        html << "加载成功 " << dllLoadPass << "/" << dllTotal
             << "，缺少接口 " << missingExports << " 个</td><td class=\""
             << statusCell(true, pass) << "</td></tr>\n";
    }

    html << "    <tr><td>5. UserMain 性能压测</td><td>平均/最大耗时 ( Budget: "
         << fleetReport.perfReport.frameBudgetMs << " ms)</td><td>";
    if (!perfRan) {
        html << "未执行压测</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        html << "Avg: " << fleetReport.perfReport.avgTimeMs << " ms, Max: "
             << fleetReport.perfReport.maxTimeMs << " ms</td><td class=\""
             << (fleetReport.perfReport.realtimeVerdict == "PASS" ? "pass\">PASS"
                : (fleetReport.perfReport.realtimeVerdict == "WARNING" ? "warn\">WARNING" : "fail\">FAIL"))
             << "</td></tr>\n";
    }

    html << "    <tr><td>6. 内存泄露监测</td><td>每 10k 次 UserMain 内存增长</td><td>";
    if (!perfRan) {
        html << "未执行压测</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        html << fleetReport.perfReport.memoryLeakRateMBPer10k << " MB / 10k</td><td class=\""
             << (fleetReport.perfReport.memoryLeakRateMBPer10k < 5.0 ? "pass\">PASS" : "warn\">WARN")
             << "</td></tr>\n";
    }

    const bool trajectoryRan = fleetReport.trajectoryModelsTested > 0;
    html << "    <tr><td>7. 运行轨迹检查</td><td>经纬度路径点采集</td><td>"
         << (trajectoryRan
             ? (std::to_string(fleetReport.trajectoryModelsPassed) + "/"
                + std::to_string(fleetReport.trajectoryModelsTested) + " 个型号通过")
             : "未执行")
         << "</td><td class=\""
         << statusCell(trajectoryRan,
                       trajectoryRan
                       && fleetReport.trajectoryModelsPassed == fleetReport.trajectoryModelsTested)
         << "</td></tr>\n";

    html << "    <tr><td>8. 多型号并行</td><td>多路径头文件/DLL 按数量并发</td><td>"
         << (multiModelRan ? fleetReport.multiModelReport.summary : "未执行")
         << "</td><td class=\""
         << (!multiModelRan ? "warn\">N/A"
            : (fleetReport.multiModelReport.verdict == "PASS" ? "pass\">PASS" : "fail\">FAIL"))
         << "</td></tr>\n";

    html << "    <tr><td>9. 多线程稳定性</td><td>并行多线程 UserMain</td><td>"
         << (multiThreadRan ? fleetReport.multiThreadReport.summary : "未执行")
         << "</td><td class=\""
         << (!multiThreadRan ? "warn\">N/A"
            : (fleetReport.multiThreadReport.verdict == "PASS" ? "pass\">PASS" : "fail\">FAIL"))
         << "</td></tr>\n";
    html << "    <tr><td>10. 单线程多对象测试</td><td>逐对象基线/单线程交错/状态串扰</td><td>";
    if (!multiObjectRan) {
        html << (multiObjectConfigured == 0
            ? "未完成多对象接口映射"
            : ("已完成映射 " + std::to_string(multiObjectConfigured)
               + " 个，Harness 已验证 " + std::to_string(multiObjectCompiled) + " 个，未执行"))
             << "</td><td class=\"warn\">N/A</td></tr>\n";
    } else {
        html << multiObjectPassed << "/" << multiObjectTested
             << " 个已执行型号通过</td><td class=\""
             << statusCell(true, multiObjectPassed == multiObjectTested)
             << "</td></tr>\n";
    }
    html
         << "  </table>\n\n";

    html << "  <h2>2. 型号总览</h2>\n  <table>\n"
         << "    <tr><th>型号</th><th>路径</th><th>头文件通过</th><th>LIB 通过</th><th>DLL 通过</th><th>判定</th></tr>\n";
    for (const auto& m : fleetReport.modelReports) {
        html << "    <tr><td>" << (m.modelName.empty() ? "-" : m.modelName) << "</td>"
             << "<td><code>" << m.packageDir << "</code></td>"
             << "<td>" << m.passedHeaderCount << "/" << m.headerReports.size() << "</td>"
             << "<td>" << m.passedLibCount << "/" << m.libReports.size() << "</td>"
             << "<td>" << m.passedDllCount << "/" << m.dllReports.size() << "</td>"
             << "<td class=\"" << (m.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
    }
    html << "  </table>\n\n";

    html << "  <h2>3. 头文件规范检查结果</h2>\n"
         << "  <table><tr><th>型号</th><th>头文件</th><th>编码</th><th>extern \"C\"</th><th>判定</th></tr>\n";
    for (const auto& m : fleetReport.modelReports) {
        for (const auto& header : m.headerReports) {
            html << "  <tr><td>" << m.modelName << "</td><td><code>" << header.filePath
                 << "</code></td><td>" << header.encoding << "</td><td>"
                 << (header.hasExternC ? "有" : "无") << "</td><td class=\""
                 << (header.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
        }
    }
    if (headerTotal == 0) html << "  <tr><td colspan=\"5\">未发现头文件</td></tr>\n";
    html << "  </table>\n\n";
    html << "  <h3>结构体重名、ODR 与命名空间污染检查</h3>\n"
         << "  <table><tr><th>型号</th><th>类型</th><th>级别</th><th>符号</th><th>涉及文件</th><th>说明</th></tr>\n";
    int conflictRows = 0;
    for (const auto& issue : fleetReport.crossModelHeaderConflictReport.issues) {
        std::string files;
        for (size_t i = 0; i < issue.files.size(); ++i) {
            if (i) files += "<br>";
            files += issue.files[i];
        }
        html << "  <tr><td>跨型号/包级</td><td>" << issue.category
             << "</td><td class=\"" << (issue.severity == "FAIL" ? "fail" : "warn")
             << "\">" << issue.severity << "</td><td>" << issue.symbol
             << "</td><td><code>" << files << "</code></td><td>" << issue.detail
             << "</td></tr>\n";
        ++conflictRows;
    }
    if (conflictRows == 0) html << "  <tr><td colspan=\"6\">未发现冲突或污染风险</td></tr>\n";
    html << "  </table>\n\n";

    html << "  <h2>4. LIB 库文件检查结果</h2>\n"
         << "  <table><tr><th>型号</th><th>LIB 文件</th><th>架构</th><th>库类型</th><th>判定</th></tr>\n";
    for (const auto& m : fleetReport.modelReports) {
        for (const auto& lib : m.libReports) {
            html << "  <tr><td>" << m.modelName << "</td><td><code>" << lib.filePath
                 << "</code></td><td>" << lib.architecture << "</td><td>" << lib.libType
                 << "</td><td class=\"" << (lib.overallPass ? "pass\">PASS" : "fail\">FAIL")
                 << "</td></tr>\n";
        }
    }
    if (libTotal == 0) html << "  <tr><td colspan=\"5\">未发现 LIB 文件</td></tr>\n";
    html << "  </table>\n\n";

    // 按型号划分：DLL 文件结构与依赖
    html << "  <h2>5. DLL 文件与依赖检查</h2>\n";
    for (size_t i = 0; i < fleetReport.modelReports.size(); ++i) {
        const auto& m = fleetReport.modelReports[i];
        html << "  <div class=\"model-section\">\n"
             << "    <h3>型号: " << (m.modelName.empty() ? "(未命名)" : m.modelName) << "</h3>\n"
             << "    <p>路径: <code>" << m.packageDir << "</code></p>\n";

        if (m.dllReports.empty()) {
            html << "    <p class=\"warn\">该型号包内未发现 DLL</p>\n";
        }
        for (const auto& d : m.dllReports) {
            html << "    <div class=\"card\">\n"
                 << "      <p><b>DLL:</b> <code>" << d.dllPath << "</code> [" << d.buildConfig << "]</p>\n"
                 << "      <p>架构: " << d.peReport.architecture
                 << " | CRT: " << d.peReport.crtLinkage
                 << " | 文件检查: <span class=\""
                 << (d.peReport.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</span></p>\n";

            html << "      <p><b>导入依赖</b></p>\n<table>\n"
                 << "      <tr><th>依赖 DLL</th><th>状态</th><th>解析路径</th></tr>\n";
            for (const auto& dep : d.peReport.importedDlls) {
                html << "      <tr><td>" << dep.name << "</td><td class=\""
                     << (dep.found ? "pass\">找到" : "fail\">缺失") << "</td><td><code>"
                     << dep.resolvedPath << "</code></td></tr>\n";
            }
            if (d.peReport.importedDlls.empty()) {
                html << "      <tr><td colspan=\"3\">无</td></tr>\n";
            }
            html << "      </table>\n    </div>\n";
        }
        html << "  </div>\n";
    }

    html << "  <h2>6. DLL 接口与加载检查</h2>\n";
    for (const auto& m : fleetReport.modelReports) {
        html << "  <div class=\"model-section\">\n"
             << "    <h3>型号: " << (m.modelName.empty() ? "(未命名)" : m.modelName) << "</h3>\n";
        if (m.dllReports.empty()) {
            html << "    <p class=\"warn\">该型号包内未发现 DLL</p>\n";
        }
        for (const auto& d : m.dllReports) {
            html << "    <div class=\"card\">\n"
                 << "      <p><b>DLL:</b> <code>" << d.dllPath << "</code></p>\n"
                 << "      <p>加载: <span class=\""
                 << (d.loadReport.isLoaded ? "pass\">PASS" : "fail\">FAIL")
                 << "</span> | 已绑定接口: " << d.loadReport.boundSymbolCount
                 << " | 缺失接口: " << d.loadReport.missingSymbolCount << "</p>\n"
                 << "      <table><tr><th>导出函数</th><th>Ordinal</th></tr>\n";
            for (const auto& exp : d.peReport.exportedSymbols) {
                html << "      <tr><td>" << exp.name << "</td><td>" << exp.ordinal << "</td></tr>\n";
            }
            if (d.peReport.exportedSymbols.empty()) {
                html << "      <tr><td colspan=\"2\">无</td></tr>\n";
            }
            html << "      </table>\n    </div>\n";
        }
        html << "  </div>\n";
    }

    // 7. 性能压测明细
    html << "  <h2>7. 性能与内存压力（UserMain 重复执行）</h2>\n"
         << "  <div class=\"card\">\n";
    if (!perfRan) {
        html << "    <p class=\"warn\">未执行压测</p>\n";
    } else {
        html << "    <p><b>UserMain 执行次数:</b> " << fleetReport.perfReport.completedSteps
             << " / " << fleetReport.perfReport.totalSteps << "</p>\n"
             << "    <p><b>单次最小耗时:</b> " << fleetReport.perfReport.minTimeMs << " ms</p>\n"
             << "    <p><b>单次最大耗时:</b> " << fleetReport.perfReport.maxTimeMs << " ms</p>\n"
             << "    <p><b>单次平均耗时:</b> " << fleetReport.perfReport.avgTimeMs << " ms</p>\n"
             << "    <p><b>耗时抖动 (StdDev):</b> " << fleetReport.perfReport.jitterMs << " ms</p>\n"
             << "    <p><b>压测内存增量:</b> " << fleetReport.perfReport.memoryDeltaMB << " MB</p>\n"
             << "    <p><b>实时性判定:</b> <span class=\""
             << (fleetReport.perfReport.realtimeVerdict == "PASS" ? "pass"
                : (fleetReport.perfReport.realtimeVerdict == "WARNING" ? "warn" : "fail"))
             << "\">" << fleetReport.perfReport.realtimeVerdict << "</span></p>\n";
    }
    html << "  </div>\n\n";

    auto emitConcSection = [&](const char* title, const ConcurrencyTestReport& cr, bool ran) {
        html << "  <h2>" << title << "</h2>\n"
             << "  <div class=\"card\">\n";
        if (!ran) {
            html << "    <p class=\"warn\">未执行</p>\n  </div>\n\n";
            return;
        }
        html << "    <p><b>数量:</b> " << cr.workerCount << "</p>\n"
             << "    <p><b>成功 / 用户失败 / 异常:</b> " << cr.successCount
             << " / " << cr.userFailCount
             << " / " << cr.exceptionCount << "</p>\n"
             << "    <p><b>判定:</b> <span class=\""
             << (cr.verdict == "PASS" ? "pass" : (cr.verdict == "WARNING" ? "warn" : "fail"))
             << "\">" << cr.verdict << "</span></p>\n"
             << "    <p>" << cr.summary << "</p>\n"
             << "  </div>\n";

        if (!cr.threadResults.empty()) {
            const bool isMultiModel = (cr.mode == ConcurrencyTestMode::MultiModel);
            html << "  <table>\n    <tr><th>"
                 << (isMultiModel ? "型号[实例]" : "线程")
                 << "</th><th>随机参数</th><th>返回码</th><th>SEH</th><th>详情</th></tr>\n";
            for (const auto& tr : cr.threadResults) {
                html << "    <tr><td>";
                if (isMultiModel) {
                    html << tr.modelName << "[" << tr.instanceId << "]";
                } else {
                    html << "#" << tr.threadId;
                }
                html << "</td><td>" << tr.randomSummary
                     << "</td><td>" << tr.userReturnCode
                     << "</td><td class=\"" << (tr.exceptionOccurred ? "fail\">异常" : "pass\">正常")
                     << "</td><td>" << tr.errorLog << "</td></tr>\n";
            }
            html << "  </table>\n\n";
        } else {
            html << "\n";
        }
    };

    emitConcSection("8. 多型号并行（多路径 DLL × 各型号数量）",
                    fleetReport.multiModelReport, multiModelRan);
    emitConcSection("9. 多线程稳定性（并行 UserMain）",
                    fleetReport.multiThreadReport, multiThreadRan);

    html << "  <h2>10. 单线程多对象基线/交错测试</h2>\n";
    if (fleetReport.multiObjectReports.empty()) {
        html << "  <div class=\"card\"><p class=\"warn\">未完成多对象接口映射</p></div>\n";
    }
    for (const auto& model : fleetReport.multiObjectReports) {
        html << "  <div class=\"model-section\"><h3>型号: " << model.modelName << "</h3>\n";
        if (model.configured) {
            const auto& mapping = model.mappingProfile;
            html << "    <div class=\"card\"><p><b>DLL:</b> <code>"
                 << mapping.dllPath << "</code></p><p><b>头文件:</b> <code>"
                 << mapping.headerPath << "</code></p><p><b>生命周期映射:</b> "
                 << mapping.createFunction.functionName << " → "
                 << mapping.initFunction.functionName << " → "
                 << mapping.stepFunction.functionName << " → "
                 << mapping.destroyFunction.functionName
                 << "</p><p><b>输出字段:</b> lat=" << mapping.latitudeField
                 << "，lon=" << mapping.longitudeField
                 << " | ABI 验证: " << (mapping.abiValidated ? "PASS" : "N/A")
                 << "</p></div>\n";
        }
        if (!model.configured) {
            html << "    <p class=\"warn\">N/A — 未完成多对象接口映射</p>\n";
        } else if (!model.harnessCompiled || model.report.verdict.empty()) {
            html << "    <p class=\"warn\">N/A — 映射 Harness 尚未生成、ABI 未验证或未执行</p>\n";
        } else {
            const auto& report = model.report;
            html << "    <p>对象数: " << report.objectCount << " | 步数: " << report.stepCount
                 << " | 最大偏差: " << report.maxPositionDeviation
                 << " | 容差: " << report.tolerance
                 << " | 最大单帧: " << report.maxFrameTimeMs << " ms"
                 << " | 内存变化: " << report.memoryDeltaMB << " MB</p>\n"
                 << "    <p class=\"" << (report.verdict == "PASS" ? "pass" : "fail")
                 << "\">" << report.verdict << " — " << report.summary << "</p>\n"
                 << "    <table><tr><th>对象</th><th>基线点</th><th>交错点</th>"
                 << "<th>最大偏差</th><th>返回码(基线/交错)</th><th>SEH/异常位置</th><th>说明</th></tr>\n";
            for (const auto& object : report.objectResults) {
                html << "    <tr><td>Object #" << object.objectId << "</td><td>"
                     << object.baselineTrajectory.size() << "</td><td>"
                     << object.interleavedTrajectory.size() << "</td><td>"
                     << object.maxPositionDeviation << "</td><td>"
                     << object.baselineReturnCode << " / " << object.interleavedReturnCode
                     << "</td><td class=\"" << (object.exceptionOccurred ? "fail" : "pass")
                     << "\">" << (object.exceptionOccurred
                         ? ("0x" + std::to_string(object.exceptionCode)
                            + " step=" + std::to_string(object.faultStep))
                         : "无")
                     << "</td><td>" << object.detail << "</td></tr>\n";
            }
            html << "    </table>\n";
        }
        html << "  </div>\n";
    }

    // 11. 日志
    html << "  <h2>11. 预检过程日志追踪</h2>\n"
         << "  <div class=\"log-box\">\n";
    bool anyLog = false;
    for (const auto& m : fleetReport.modelReports) {
        const std::string tag = m.modelName.empty() ? "Model" : m.modelName;
        for (const auto& hRep : m.headerReports) {
            for (const auto& logMsg : hRep.logMessages) {
                html << "    <div>[" << tag << "/Header] " << logMsg << "</div>\n";
                anyLog = true;
            }
        }
        for (const auto& lRep : m.libReports) {
            for (const auto& logMsg : lRep.logMessages) {
                html << "    <div>[" << tag << "/LIB] " << logMsg << "</div>\n";
                anyLog = true;
            }
        }
        for (const auto& dRep : m.dllReports) {
            for (const auto& logMsg : dRep.peReport.logMessages) {
                html << "    <div>[" << tag << "/PE] " << logMsg << "</div>\n";
                anyLog = true;
            }
            if (!dRep.loadReport.errorLog.empty()) {
                html << "    <div>[" << tag << "/Load] " << dRep.loadReport.errorLog << "</div>\n";
                anyLog = true;
            }
        }
        for (const auto& logMsg : m.packageFiles.scanLog) {
            html << "    <div>[" << tag << "/Scan] " << logMsg << "</div>\n";
            anyLog = true;
        }
    }
    for (const auto& logMsg : fleetReport.crossModelHeaderConflictReport.logMessages) {
        html << "    <div>[HeaderConflict] " << logMsg << "</div>\n";
        anyLog = true;
    }
    for (const auto& logMsg : fleetReport.multiModelReport.logMessages) {
        html << "    <div>[MultiModel] " << logMsg << "</div>\n";
        anyLog = true;
    }
    for (const auto& logMsg : fleetReport.multiThreadReport.logMessages) {
        html << "    <div>[MultiThread] " << logMsg << "</div>\n";
        anyLog = true;
    }
    for (const auto& model : fleetReport.multiObjectReports) {
        for (const auto& logMsg : model.report.logMessages) {
            html << "    <div>[" << model.modelName << "/MultiObject] "
                 << logMsg << "</div>\n";
            anyLog = true;
        }
    }
    if (!fleetReport.perfReport.exceptionLog.empty()) {
        html << "    <div class=\"fail\">[Perf] " << fleetReport.perfReport.exceptionLog << "</div>\n";
        anyLog = true;
    }
    if (!anyLog) {
        html << "    <div class=\"warn\">暂无日志（请先执行一键预检 / 压测 / 并行测试）</div>\n";
    }
    html << "  </div>\n";

    html << "</div>\n</body>\n</html>\n";
    return html.str();
}

bool ReportGenerator::SaveFleetReportToFile(const FleetSessionReport& fleetReport, const std::string& outputPath) {
    std::string html = GenerateFleetHtml(fleetReport);
    std::ofstream file(outputPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << html;
    file.close();
    return true;
}
