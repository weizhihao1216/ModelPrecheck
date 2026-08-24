#include "ReportGenerator.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

std::string ReportGenerator::GenerateHtml(const CombinedPrecheckReport& report) {
    std::stringstream html;

    std::string badgeColor = "#a6e3a1"; // Green
    std::string verdictText = "PASS";
    if (!report.overallPass) {
        if (report.perfReport.realtimeVerdict == "WARNING" || !report.trajReport.warnings.empty()) {
            badgeColor = "#f9e2af"; // Yellow
            verdictText = "WARNING";
        }
        if (!report.peReport.overallPass || !report.loadReport.isLoaded || report.perfReport.realtimeVerdict == "FAIL" || !report.trajReport.overallPass) {
            badgeColor = "#f38ba8"; // Red
            verdictText = "FAIL";
        }
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
        html << "    <tr><td>1. 头文件(.h)规范预检</td><td>编码校验/extern \"C\"/接口原型</td><td>编码: "
             << report.headerReport.encoding << " | extern \"C\": " << (report.headerReport.hasExternC ? "有" : "无")
             << "</td><td class=\"" << (report.headerReport.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
    }

    if (!report.libPath.empty()) {
        html << "    <tr><td>2. LIB 库(.lib)规范预检</td><td>COFF架构/导入库类型/符号匹配</td><td>"
             << report.libReport.architecture << " / " << report.libReport.libType
             << "</td><td class=\"" << (report.libReport.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n";
    }

    html << "    <tr><td>3. DLL 静态 PE 检查</td><td>架构匹配/CRT类型/依赖库完整性</td><td>" 
         << report.peReport.architecture << " / " << report.peReport.crtLinkage << "</td><td class=\"" 
         << (report.peReport.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n"
         << "    <tr><td>4. 导出接口校验</td><td>Init / Step / Destroy / Info</td><td>缺少导出: " 
         << report.peReport.missingExportCount << " 个</td><td class=\"" 
         << (report.peReport.missingExportCount == 0 ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n"
         << "    <tr><td>5. 动态加载与SEH</td><td>LoadLibrary & Exception Protection</td><td>" 
         << (report.loadReport.isLoaded ? "成功加载" : "加载失败") << "</td><td class=\"" 
         << (report.loadReport.isLoaded ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n"
         << "    <tr><td>6. 步进性能评估</td><td>平均/最大耗时 ( Budget: " << report.perfReport.frameBudgetMs << " ms)</td><td>Avg: " 
         << report.perfReport.avgTimeMs << " ms, Max: " << report.perfReport.maxTimeMs << " ms</td><td class=\"" 
         << (report.perfReport.realtimeVerdict == "PASS" ? "pass\">PASS" : (report.perfReport.realtimeVerdict == "WARNING" ? "warn\">WARNING" : "fail\">FAIL")) << "</td></tr>\n"
         << "    <tr><td>7. 内存泄露监测</td><td>10,000 步内存增长 rate</td><td>" 
         << report.perfReport.memoryLeakRateMBPer10k << " MB / 10k steps</td><td class=\"" 
         << (report.perfReport.memoryLeakRateMBPer10k < 5.0 ? "pass\">PASS" : "warn\">WARN") << "</td></tr>\n"
         << "    <tr><td>8. 轨迹与坐标系逻辑</td><td>NaN检测/范围限制/角度单位</td><td>NaN点: " 
         << report.trajReport.nanOrInfCount << " | 跳变: " << report.trajReport.positionJumpCount << "</td><td class=\"" 
         << (report.trajReport.overallPass ? "pass\">PASS" : "fail\">FAIL") << "</td></tr>\n"
         << "  </table>\n\n";

    // Header File Section
    if (!report.headerPath.empty()) {
        html << "  <h2>2. 头文件 (.h) 接口规范预检分析</h2>\n"
             << "  <div class=\"card\">\n"
             << "    <p><b>头文件路径:</b> <code>" << report.headerPath << "</code></p>\n"
             << "    <p><b>文本编码格式:</b> " << report.headerReport.encoding << "</p>\n"
             << "    <p><b>extern \"C\" 保护:</b> " << (report.headerReport.hasExternC ? "<span class=\"pass\">已包含</span>" : "<span class=\"fail\">未检测到</span>") << "</p>\n"
             << "    <p><b>__declspec 动态库宏:</b> " << (report.headerReport.hasDeclspec ? "<span class=\"pass\">包含</span>" : "无") << "</p>\n"
             << "  </div>\n\n";
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

    // Section 6: Performance Profiling
    html << "  <h2>5. 性能压力与内存泄露统计</h2>\n"
         << "  <div class=\"card\">\n"
         << "    <p><b>压测总步数:</b> " << report.perfReport.completedSteps << " / " << report.perfReport.totalSteps << "</p>\n"
         << "    <p><b>单步最小耗时:</b> " << report.perfReport.minTimeMs << " ms</p>\n"
         << "    <p><b>单步最大耗时:</b> " << report.perfReport.maxTimeMs << " ms</p>\n"
         << "    <p><b>单步平均耗时:</b> " << report.perfReport.avgTimeMs << " ms</p>\n"
         << "    <p><b>耗时抖动 (StdDev):</b> " << report.perfReport.jitterMs << " ms</p>\n"
         << "    <p><b>静态内存占用 (Working Set):</b> " << report.loadReport.initialMemoryDeltaKB << " KB</p>\n"
         << "    <p><b>压测内存增量:</b> " << report.perfReport.memoryDeltaMB << " MB</p>\n"
         << "  </div>\n\n";

    // Section 7: Functional Verification
    html << "  <h2>6. 运动轨迹与坐标单位逻辑校验</h2>\n"
         << "  <div class=\"card\">\n"
         << "    <p><b>轨迹数据点数:</b> " << report.trajReport.totalDataPoints << "</p>\n"
         << "    <p><b>角度单位评估:</b> " << report.trajReport.unitCheckLog << "</p>\n"
         << "    <p><b>异常跳变点数:</b> " << report.trajReport.positionJumpCount << "</p>\n"
         << "  </div>\n\n";

    // Section 8: Logs
    html << "  <h2>7. 预检过程日志追踪</h2>\n"
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
