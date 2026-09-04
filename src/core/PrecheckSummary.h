#ifndef PRECHECK_SUMMARY_H
#define PRECHECK_SUMMARY_H

#include <string>
#include <vector>
#include "PackageScanner.h"
#include "ReportGenerator.h"

enum class TestItemState {
    Pass,
    Fail,
    Warn,
    NotRun,   // 尚未检测
    Skipped   // 条件不足跳过（如未编译）
};

struct BuildConfigCapability {
    std::string modelName;
    int releaseDllCount = 0;
    int releaseLibCount = 0;
    int debugDllCount = 0;
    int debugLibCount = 0;

    bool canUseRelease = false;
    bool canLinkRelease = false;
    bool canUseDebug = false;
    bool canLinkDebug = false;
    bool canCompileDebug = false;

    std::string releaseVerdict; // PASS / WARN / FAIL
    std::string debugVerdict;
    std::string releaseSummary;
    std::string debugSummary;
    std::vector<std::string> notes;
};

struct TestItemResult {
    std::string id;           // 稳定键，用于导航页绑定
    std::string name;         // 显示名
    TestItemState state = TestItemState::NotRun;
    std::string reason;       // 具体原因
    std::string consequence;  // 可能导致的情况
};

struct PrecheckSummaryBoard {
    std::vector<BuildConfigCapability> buildConfigs;
    std::vector<TestItemResult> items;
    int passCount = 0;
    int failCount = 0;
    int warnCount = 0;
    int notRunCount = 0;
    int skippedCount = 0;

    const TestItemResult* findById(const std::string& id) const;
};

class PrecheckSummary {
public:
    static BuildConfigCapability EvaluateBuildConfig(const ModelPackageFiles& pkg,
                                                     const std::string& modelName = {});
    static PrecheckSummaryBoard BuildFromFleet(const FleetSessionReport& fleet);
    static std::string StateLabel(TestItemState state);
    static std::string StateCssClass(TestItemState state);
    static std::string ToHtmlSection(const PrecheckSummaryBoard& board,
                                     bool includeBuildConfigRow = false);
};

#endif // PRECHECK_SUMMARY_H
