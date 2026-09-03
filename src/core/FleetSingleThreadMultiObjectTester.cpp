#include "FleetSingleThreadMultiObjectTester.h"
#include "MultiObjectHarness.h"
#include "../utils/MemoryUtils.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <random>
#include <sstream>

namespace {

void recordFail(MultiObjectResult& result, int returnCode, unsigned long exceptionCode,
                int step, bool baseline, const char* phase) {
    if (baseline) result.baselineReturnCode = returnCode;
    else result.interleavedReturnCode = returnCode;
    result.exceptionOccurred = exceptionCode != 0;
    result.exceptionCode = exceptionCode;
    result.faultStep = step;
    result.detail = phase;
}

} // namespace

FleetMultiObjectTestReport FleetSingleThreadMultiObjectTester::Run(
    const FleetMultiObjectTestConfig& config) {
    FleetMultiObjectTestReport report;
    report.stepCount = config.stepCount;
    report.tolerance = config.tolerance;
    report.usedSingleThread = true;
    report.modelCount = static_cast<int>(config.models.size());

    if (config.models.size() < 2) {
        report.verdict = "FAIL";
        report.summary = "跨型号对象交错至少需要 2 个型号参与";
        report.logMessages.push_back("FAIL: " + report.summary);
        return report;
    }
    if (config.stepCount < 1 || config.stepDt <= 0.0) {
        report.verdict = "FAIL";
        report.summary = "步数与 dt 必须大于 0";
        report.logMessages.push_back("FAIL: " + report.summary);
        return report;
    }

    struct Slot {
        MultiObjectHarness* harness = nullptr;
        int modelIndex = 0;
        int localObjectId = 0;
        std::string modelName;
        int fleetResultIndex = -1;
        void* live = nullptr;
    };

    std::vector<Slot> slots;
    for (size_t mi = 0; mi < config.models.size(); ++mi) {
        const FleetMultiObjectModelSpec& spec = config.models[mi];
        if (!spec.harness || !spec.harness->IsLoaded()) {
            report.verdict = "FAIL";
            report.summary = "型号「" + spec.modelName + "」多对象 Harness 未加载";
            report.logMessages.push_back("FAIL: " + report.summary);
            return report;
        }
        if (!spec.harness->SupportsObjectSession()) {
            report.verdict = "FAIL";
            report.summary = "型号「" + spec.modelName
                + "」Harness 不支持对象级调度，请重新编译多对象 Harness";
            report.logMessages.push_back("FAIL: " + report.summary);
            return report;
        }
        if (spec.objectCount < 1) {
            report.verdict = "FAIL";
            report.summary = "型号「" + spec.modelName + "」对象数至少为 1";
            report.logMessages.push_back("FAIL: " + report.summary);
            return report;
        }
        for (int oid = 0; oid < spec.objectCount; ++oid) {
            Slot slot;
            slot.harness = spec.harness;
            slot.modelIndex = static_cast<int>(mi);
            slot.localObjectId = oid;
            slot.modelName = spec.modelName;
            slot.fleetResultIndex = static_cast<int>(report.objectResults.size());
            slots.push_back(slot);

            FleetMultiObjectResult item;
            item.globalObjectId = static_cast<int>(slots.size() - 1);
            item.modelIndex = slot.modelIndex;
            item.localObjectId = oid;
            item.modelName = spec.modelName;
            item.detail.objectId = oid;
            report.objectResults.push_back(item);
        }
    }
    report.totalObjectCount = static_cast<int>(slots.size());
    if (report.totalObjectCount < 2) {
        report.verdict = "FAIL";
        report.summary = "跨型号交错对象总数至少为 2";
        report.logMessages.push_back("FAIL: " + report.summary);
        return report;
    }

    report.logMessages.push_back(
        "INFO: 跨型号对象交错：型号数=" + std::to_string(report.modelCount)
        + "，对象总数=" + std::to_string(report.totalObjectCount)
        + "，步数=" + std::to_string(config.stepCount));

    const ProcessMemoryStats before = MemoryUtils::GetCurrentProcessMemory();

    // ---- Per-model prepare + baseline (isolated) ----
    for (size_t mi = 0; mi < config.models.size(); ++mi) {
        const FleetMultiObjectModelSpec& spec = config.models[mi];
        const uint32_t seed = config.randomSeed + static_cast<uint32_t>(mi) * 97u;
        const RandomValueBlob values = spec.harness->Sample(seed, spec.objectCount);
        std::string error;
        if (!spec.harness->PrepareObjectSession(values, spec.objectCount, config.stepDt, error)) {
            report.verdict = "FAIL";
            report.summary = "型号「" + spec.modelName + "」会话准备失败: " + error;
            report.logMessages.push_back("FAIL: " + report.summary);
            return report;
        }
        for (Slot& slot : slots) {
            if (slot.modelIndex != static_cast<int>(mi)) continue;
            FleetMultiObjectResult& item =
                report.objectResults[static_cast<size_t>(slot.fleetResultIndex)];
            unsigned long seh = 0;
            void* object = spec.harness->CreateLiveObject(slot.localObjectId, &seh);
            if (!object) {
                recordFail(item.detail, -1, seh, -1, true, "基线创建对象失败");
                continue;
            }
            const int initRc = spec.harness->InitLiveObject(object, slot.localObjectId, &seh);
            if (initRc != 0 || seh != 0) {
                recordFail(item.detail, initRc, seh, -1, true, "基线初始化失败");
                spec.harness->DestroyLiveObject(object, slot.localObjectId, &seh);
                continue;
            }
            for (int step = 0; step < config.stepCount; ++step) {
                double lat = 0.0, lon = 0.0;
                const int stepRc = spec.harness->StepLiveObject(
                    object, slot.localObjectId, step, &lat, &lon, &seh);
                if (stepRc != 0 || seh != 0) {
                    recordFail(item.detail, stepRc, seh, step, true, "基线步进失败");
                    break;
                }
                item.detail.baselineTrajectory.push_back({ lat, lon });
            }
            spec.harness->DestroyLiveObject(object, slot.localObjectId, &seh);
            if (seh != 0 && item.detail.detail.empty())
                recordFail(item.detail, item.detail.baselineReturnCode, seh, -1, true,
                           "基线销毁对象失败");
        }
    }

    // ---- Interleaved session: recreate all, then shared schedule ----
    for (size_t mi = 0; mi < config.models.size(); ++mi) {
        const FleetMultiObjectModelSpec& spec = config.models[mi];
        const uint32_t seed = config.randomSeed + static_cast<uint32_t>(mi) * 97u;
        const RandomValueBlob values = spec.harness->Sample(seed, spec.objectCount);
        std::string error;
        if (!spec.harness->PrepareObjectSession(values, spec.objectCount, config.stepDt, error)) {
            report.verdict = "FAIL";
            report.summary = "型号「" + spec.modelName + "」交错会话准备失败: " + error;
            report.logMessages.push_back("FAIL: " + report.summary);
            return report;
        }
        for (Slot& slot : slots) {
            if (slot.modelIndex != static_cast<int>(mi)) continue;
            FleetMultiObjectResult& item =
                report.objectResults[static_cast<size_t>(slot.fleetResultIndex)];
            unsigned long seh = 0;
            slot.live = spec.harness->CreateLiveObject(slot.localObjectId, &seh);
            if (!slot.live) {
                recordFail(item.detail, -1, seh, -1, false, "交错创建对象失败");
                continue;
            }
            const int initRc = spec.harness->InitLiveObject(
                slot.live, slot.localObjectId, &seh);
            if (initRc != 0 || seh != 0) {
                recordFail(item.detail, initRc, seh, -1, false, "交错初始化失败");
            }
        }
    }

    std::vector<int> order(slots.size());
    std::iota(order.begin(), order.end(), 0);
    if (config.schedule == MultiObjectSchedule::Reverse)
        std::reverse(order.begin(), order.end());
    std::mt19937 random(config.randomSeed ^ 0xA5A5u);

    for (int step = 0; step < config.stepCount; ++step) {
        if (config.schedule == MultiObjectSchedule::DeterministicRandom)
            std::shuffle(order.begin(), order.end(), random);
        const auto frameStart = std::chrono::high_resolution_clock::now();
        for (int globalId : order) {
            Slot& slot = slots[static_cast<size_t>(globalId)];
            FleetMultiObjectResult& item =
                report.objectResults[static_cast<size_t>(slot.fleetResultIndex)];
            if (!slot.live || item.detail.interleavedReturnCode != 0
                || item.detail.exceptionOccurred) continue;
            double lat = 0.0, lon = 0.0;
            unsigned long seh = 0;
            const int stepRc = slot.harness->StepLiveObject(
                slot.live, slot.localObjectId, step, &lat, &lon, &seh);
            if (stepRc != 0 || seh != 0) {
                recordFail(item.detail, stepRc, seh, step, false, "交错步进失败");
                continue;
            }
            item.detail.interleavedTrajectory.push_back({ lat, lon });
        }
        const auto frameEnd = std::chrono::high_resolution_clock::now();
        report.maxFrameTimeMs = (std::max)(
            report.maxFrameTimeMs,
            std::chrono::duration<double, std::milli>(frameEnd - frameStart).count());
    }

    for (Slot& slot : slots) {
        if (!slot.live) continue;
        unsigned long seh = 0;
        slot.harness->DestroyLiveObject(slot.live, slot.localObjectId, &seh);
        FleetMultiObjectResult& item =
            report.objectResults[static_cast<size_t>(slot.fleetResultIndex)];
        if (seh != 0 && !item.detail.exceptionOccurred) {
            recordFail(item.detail, item.detail.interleavedReturnCode, seh, -1, false,
                       "交错销毁对象失败");
        }
        slot.live = nullptr;
    }

    const ProcessMemoryStats after = MemoryUtils::GetCurrentProcessMemory();
    report.memoryDeltaMB = MemoryUtils::BytesToMB(after.workingSetBytes)
                         - MemoryUtils::BytesToMB(before.workingSetBytes);

    for (auto& item : report.objectResults) {
        MultiObjectResult& result = item.detail;
        const size_t count = (std::min)(result.baselineTrajectory.size(),
                                        result.interleavedTrajectory.size());
        for (size_t i = 0; i < count; ++i) {
            const double dLat = result.baselineTrajectory[i].lat
                              - result.interleavedTrajectory[i].lat;
            const double dLon = result.baselineTrajectory[i].lon
                              - result.interleavedTrajectory[i].lon;
            result.maxPositionDeviation = (std::max)(
                result.maxPositionDeviation,
                std::sqrt(dLat * dLat + dLon * dLon));
        }
        report.maxPositionDeviation = (std::max)(
            report.maxPositionDeviation, result.maxPositionDeviation);

        const bool complete =
            static_cast<int>(result.baselineTrajectory.size()) == config.stepCount
            && static_cast<int>(result.interleavedTrajectory.size()) == config.stepCount;
        const bool failed = result.exceptionOccurred
            || result.baselineReturnCode != 0
            || result.interleavedReturnCode != 0
            || !complete;
        if (result.detail.empty()) {
            if (result.baselineTrajectory.size() != result.interleavedTrajectory.size())
                result.detail = "基线与交错轨迹点数不一致";
            else if (result.maxPositionDeviation > config.tolerance)
                result.detail = "跨型号交错偏离基线，疑似跨型号状态串扰";
            else
                result.detail = "跨型号状态隔离正常";
        }
        if (result.exceptionOccurred) ++report.exceptionCount;
        if (!failed && result.maxPositionDeviation <= config.tolerance)
            ++report.completedObjects;
        else if (!failed)
            ++report.interferenceCount;

        std::ostringstream line;
        line << "FleetObject#" << item.globalObjectId
             << " model=" << item.modelName
             << " local#" << item.localObjectId
             << " baseline=" << result.baselineTrajectory.size()
             << " interleaved=" << result.interleavedTrajectory.size()
             << " maxDeviation=" << result.maxPositionDeviation
             << " detail=" << result.detail;
        report.logMessages.push_back(line.str());
    }

    if (report.exceptionCount > 0) {
        report.verdict = "FAIL";
        report.summary = "跨型号对象交错发生异常，异常对象="
            + std::to_string(report.exceptionCount);
    } else if (report.interferenceCount > 0
               || report.completedObjects != report.totalObjectCount) {
        report.verdict = "FAIL";
        report.summary = "检测到跨型号状态串扰或执行不完整，偏差对象="
            + std::to_string(report.interferenceCount);
    } else {
        report.verdict = "PASS";
        report.summary = "各型号单独基线与跨型号单线程交错结果一致，型号数="
            + std::to_string(report.modelCount)
            + "，对象总数=" + std::to_string(report.totalObjectCount);
    }
    report.logMessages.push_back("INFO: " + report.summary);
    return report;
}
