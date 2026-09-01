#ifndef REPORT_GENERATOR_H
#define REPORT_GENERATOR_H

#include <string>
#include "PeAnalyzer.h"
#include "HeaderAnalyzer.h"
#include "LibAnalyzer.h"
#include "DllLoader.h"
#include "PerfProfiler.h"
#include "FunctionalVerifier.h"
#include "ConcurrencyTester.h"
#include "PackageScanner.h"

struct CombinedPrecheckReport {
    std::string dllPath;
    std::string headerPath;
    std::string libPath;
    std::string buildConfig; // "Release" or "Debug"
    std::string timestamp;
    
    PeAnalysisReport peReport;
    HeaderAnalysisReport headerReport;
    LibAnalysisReport libReport;
    HeaderExportConsistency consistencyReport;
    LoadResult loadReport;
    PerfProfileReport perfReport;
    TrajectoryVerificationReport trajReport;
    ConcurrencyTestReport multiModelReport;
    ConcurrencyTestReport multiThreadReport;

    bool overallPass = false;
};

struct DualBuildPrecheckReport {
    std::string modelName;   // 型号名称（多型号会话时按型号划分）
    std::string packageDir;
    std::string timestamp;
    ModelPackageFiles packageFiles;

    std::vector<HeaderAnalysisReport> headerReports;
    std::vector<LibAnalysisReport> libReports;
    std::vector<CombinedPrecheckReport> dllReports;

    HeaderExportConsistency consistencyReport;

    int passedHeaderCount = 0;
    int passedLibCount = 0;
    int passedDllCount = 0;

    bool overallPass = false;
};

// 多型号一键预检总报告（按型号分章节）
struct FleetSessionReport {
    std::string timestamp;
    std::vector<DualBuildPrecheckReport> modelReports;
    ConcurrencyTestReport multiModelReport;
    ConcurrencyTestReport multiThreadReport;
    PerfProfileReport perfReport;
    bool overallPass = false;
};

class ReportGenerator {
public:
    static std::string GenerateHtml(const CombinedPrecheckReport& report);
    static std::string GenerateDualBuildHtml(const DualBuildPrecheckReport& dualReport);
    static std::string GenerateFleetHtml(const FleetSessionReport& fleetReport);
    static bool SaveReportToFile(const CombinedPrecheckReport& report, const std::string& outputPath);
    static bool SaveDualReportToFile(const DualBuildPrecheckReport& dualReport, const std::string& outputPath);
    static bool SaveFleetReportToFile(const FleetSessionReport& fleetReport, const std::string& outputPath);
};

#endif // REPORT_GENERATOR_H
