#include "PerfProfiler.h"
#include <chrono>
#include <cmath>
#include "../utils/MemoryUtils.h"
#include "../utils/QtEncoding.h"

PerfProfilerWorker::PerfProfilerWorker(UserCodeHarness* harness, int totalRuns, double targetHz,
                                       uint32_t randomSeed, double maxMemoryDeltaMB)
    : m_pHarness(harness)
    , m_totalRuns(totalRuns)
    , m_targetHz(targetHz)
    , m_randomSeed(randomSeed)
    , m_maxMemoryDeltaMB(maxMemoryDeltaMB) {
}

PerfProfilerWorker::~PerfProfilerWorker() = default;

void PerfProfilerWorker::process() {
    PerfProfileReport report;
    report.totalSteps = m_totalRuns > 0 ? m_totalRuns : 1;
    report.targetHz = m_targetHz > 0 ? m_targetHz : 50.0;
    report.frameBudgetMs = 1000.0 / report.targetHz;

    if (!m_pHarness || !m_pHarness->IsLoaded()) {
        report.realtimeVerdict = "FAIL";
        report.exceptionLog = "用户 UserMain 未编译/未加载，请先在「用户代码」页编译";
        emit logMessage("ERROR: " + qUtf8(report.exceptionLog));
        emit finished(report);
        return;
    }

    emit logMessage(QString("INFO: 性能压测开始 — 串行尽快执行 UserMain × %1，单次实时性预算 %2 ms（不按 Hz 等待）")
        .arg(report.totalSteps)
        .arg(report.frameBudgetMs, 0, 'f', 3));

    ProcessMemoryStats initialMem = MemoryUtils::GetCurrentProcessMemory();
    report.initialMemoryMB = MemoryUtils::BytesToMB(initialMem.workingSetBytes);

    std::vector<double> timeSamples;
    timeSamples.reserve(static_cast<size_t>(report.totalSteps));

    double totalTimeMs = 0.0;
    double minT = 1e9;
    double maxT = 0.0;

    int sampleInterval = report.totalSteps / 100;
    if (sampleInterval < 1) sampleInterval = 1;
    // Cap Working Set growth so intentional leak samples cannot hang the UI / OOM.
    // <=0 disables the cap (user-configurable; UI default 256 MB).
    const double maxMemoryDeltaMB = m_maxMemoryDeltaMB;
    const bool memoryCapEnabled = maxMemoryDeltaMB > 0.0;
    constexpr int kMemoryCheckEvery = 25;

    if (memoryCapEnabled) {
        emit logMessage(QStringLiteral("INFO: 工作集增长上限 %1 MB（超过则提前结束压测）")
            .arg(maxMemoryDeltaMB, 0, 'f', 0));
    } else {
        emit logMessage(QStringLiteral("INFO: 未设置工作集增长上限（将跑满计划次数）"));
    }

    for (int i = 0; i < report.totalSteps; ++i) {
        uint32_t seed = m_randomSeed + static_cast<uint32_t>(i) * 9973u;
        RandomValueBlob blob = m_pHarness->Sample(seed);

        int userRet = 0;
        bool seh = false;
        std::string err;

        auto t0 = std::chrono::high_resolution_clock::now();
        bool ok = m_pHarness->RunOnce(blob, &userRet, &seh, err);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (!ok || seh) {
            report.encounteredException = true;
            report.exceptionLog = "UserMain 第 " + std::to_string(i) + " 次执行异常: " + err;
            emit logMessage("ERROR: " + qDecodeLog(report.exceptionLog));
            break;
        }
        if (userRet != 0) {
            report.encounteredException = true;
            report.exceptionLog = "UserMain 第 " + std::to_string(i) + " 次返回非 0: " + std::to_string(userRet)
                + " [" + blob.summary + "]";
            emit logMessage("ERROR: " + qUtf8(report.exceptionLog));
            break;
        }

        timeSamples.push_back(elapsedMs);
        totalTimeMs += elapsedMs;
        if (elapsedMs < minT) minT = elapsedMs;
        if (elapsedMs > maxT) maxT = elapsedMs;
        report.completedSteps++;

        const bool sampleNow = (i % sampleInterval == 0) || (i == report.totalSteps - 1);
        const bool memCheckNow = (report.completedSteps % kMemoryCheckEvery == 0) || sampleNow;
        if (sampleNow || memCheckNow) {
            ProcessMemoryStats curMem = MemoryUtils::GetCurrentProcessMemory();
            double curMB = MemoryUtils::BytesToMB(curMem.workingSetBytes);
            if (sampleNow) {
                PerfSample s;
                s.stepIndex = i;
                s.timeMs = elapsedMs;
                s.memoryMB = curMB;
                report.samples.push_back(s);
                emit sampleAdded(i, elapsedMs, curMB);
                emit progressUpdated(i + 1, report.totalSteps, elapsedMs, curMB);
            }
            if (memoryCapEnabled && curMB - report.initialMemoryMB >= maxMemoryDeltaMB) {
                report.abortedDueToMemory = true;
                emit logMessage(QString(
                    "WARN: 工作集已增长超过 %1 MB（当前 %2 MB），提前结束压测以避免卡死/OOM；"
                    "已完成 %3/%4 次，按已完成次数推算泄漏率")
                    .arg(maxMemoryDeltaMB, 0, 'f', 0)
                    .arg(curMB, 0, 'f', 1)
                    .arg(report.completedSteps)
                    .arg(report.totalSteps));
                break;
            }
        }
    }

    ProcessMemoryStats finalMem = MemoryUtils::GetCurrentProcessMemory();
    report.finalMemoryMB = MemoryUtils::BytesToMB(finalMem.workingSetBytes);
    report.memoryDeltaMB = report.finalMemoryMB - report.initialMemoryMB;

    if (report.completedSteps > 0) {
        report.avgTimeMs = totalTimeMs / report.completedSteps;
        report.minTimeMs = minT;
        report.maxTimeMs = maxT;
        double varianceSum = 0.0;
        for (double t : timeSamples) {
            varianceSum += (t - report.avgTimeMs) * (t - report.avgTimeMs);
        }
        report.jitterMs = std::sqrt(varianceSum / report.completedSteps);
        report.memoryLeakRateMBPer10k = (report.memoryDeltaMB / report.completedSteps) * 10000.0;
    }

    if (report.encounteredException) {
        report.realtimeVerdict = "FAIL";
    } else if (report.avgTimeMs > report.frameBudgetMs) {
        report.realtimeVerdict = "FAIL";
    } else if (report.maxTimeMs > report.frameBudgetMs) {
        report.realtimeVerdict = "WARNING";
    } else {
        report.realtimeVerdict = "PASS";
    }

    emit logMessage(QString("INFO: 压测结束 %1/%2 次 UserMain，Avg=%3 ms，内存Δ=%4 MB，"
                            "泄漏率=%5 MB/10k%6，判定=%7")
        .arg(report.completedSteps)
        .arg(report.totalSteps)
        .arg(report.avgTimeMs, 0, 'f', 4)
        .arg(report.memoryDeltaMB, 0, 'f', 2)
        .arg(report.memoryLeakRateMBPer10k, 0, 'f', 2)
        .arg(report.abortedDueToMemory ? QStringLiteral("（因内存提前中止）") : QString())
        .arg(qUtf8(report.realtimeVerdict)));

    emit finished(report);
}
