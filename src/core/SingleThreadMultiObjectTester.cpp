#include "SingleThreadMultiObjectTester.h"
#include "MultiObjectHarness.h"
#include <algorithm>
#include <sstream>

MultiObjectTestReport SingleThreadMultiObjectTester::Run(
    MultiObjectHarness& harness,
    const MultiObjectTestConfig& config,
    const RandomValueBlob& values) {
    MultiObjectTestReport report;
    report.objectCount = config.objectCount;
    report.stepCount = config.stepCount;
    report.tolerance = config.tolerance;
    report.usedSingleThread = true;

    if (!harness.IsLoaded()) {
        report.verdict = "FAIL";
        report.summary = "多对象 Harness 尚未编译";
        report.logMessages.push_back("FAIL: " + report.summary);
        return report;
    }
    if (config.objectCount < 2 || config.stepCount < 1 || config.stepDt <= 0.0) {
        report.verdict = "FAIL";
        report.summary = "对象数量至少为 2，步数和时间步长必须大于 0";
        report.logMessages.push_back("FAIL: " + report.summary);
        return report;
    }

    std::string error;
    if (!harness.Run(config, values, report, error)) {
        report.verdict = "FAIL";
        report.summary = error;
        report.logMessages.push_back("FAIL: " + error);
        return report;
    }

    for (const auto& object : report.objectResults) {
        report.maxPositionDeviation =
            (std::max)(report.maxPositionDeviation, object.maxPositionDeviation);
        const bool complete =
            static_cast<int>(object.baselineTrajectory.size()) == config.stepCount
            && static_cast<int>(object.interleavedTrajectory.size()) == config.stepCount;
        const bool failed = object.exceptionOccurred
            || object.baselineReturnCode != 0
            || object.interleavedReturnCode != 0
            || !complete;
        if (object.exceptionOccurred) ++report.exceptionCount;
        if (!failed && object.maxPositionDeviation <= config.tolerance) {
            ++report.completedObjects;
        } else if (!failed) {
            ++report.interferenceCount;
        }
        std::ostringstream detail;
        detail << "Object#" << object.objectId
               << " baseline=" << object.baselineTrajectory.size()
               << " interleaved=" << object.interleavedTrajectory.size()
               << " maxDeviation=" << object.maxPositionDeviation
               << " detail=" << object.detail;
        report.logMessages.push_back(detail.str());
    }

    if (report.exceptionCount > 0) {
        report.verdict = "FAIL";
        report.summary = "单线程多对象运行发生异常，异常对象="
            + std::to_string(report.exceptionCount);
    } else if (report.interferenceCount > 0
               || report.completedObjects != report.objectCount) {
        report.verdict = "FAIL";
        report.summary = "检测到对象状态串扰或执行不完整，异常偏差对象="
            + std::to_string(report.interferenceCount);
    } else {
        report.verdict = "PASS";
        report.summary = "单独基线与单线程交错运行结果一致，对象数="
            + std::to_string(report.objectCount);
    }
    report.logMessages.push_back("INFO: " + report.summary);
    return report;
}
