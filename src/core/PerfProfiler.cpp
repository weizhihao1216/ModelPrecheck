#include "PerfProfiler.h"
#include <chrono>
#include <cmath>
#include <numeric>
#include <QDebug>
#include "../utils/MemoryUtils.h"

PerfProfilerWorker::PerfProfilerWorker(DllLoader* loader, const WeaponModelParams& params, int totalSteps, double targetHz)
    : m_pLoader(loader)
    , m_params(params)
    , m_totalSteps(totalSteps)
    , m_targetHz(targetHz) {
}

PerfProfilerWorker::~PerfProfilerWorker() {
}

void PerfProfilerWorker::process() {
    PerfProfileReport report;
    report.totalSteps = m_totalSteps;
    report.targetHz = m_targetHz;
    report.frameBudgetMs = 1000.0 / (m_targetHz > 0 ? m_targetHz : 50.0);

    if (!m_pLoader || !m_pLoader->IsLoaded()) {
        report.realtimeVerdict = "FAIL";
        report.exceptionLog = "DLL is not loaded or loader is null.";
        emit finished(report);
        return;
    }

    // Call Model_Init
    int initRes = 0;
    std::string errStr;
    emit logMessage("INFO: Initializing model for performance profiling...");
    if (!m_pLoader->CallInit(m_params, initRes, errStr)) {
        report.realtimeVerdict = "FAIL";
        report.encounteredException = true;
        report.exceptionLog = "Init Failed: " + errStr;
        emit logMessage("ERROR: " + QString::fromStdString(errStr));
        emit finished(report);
        return;
    }

    ProcessMemoryStats initialMem = MemoryUtils::GetCurrentProcessMemory();
    report.initialMemoryMB = MemoryUtils::BytesToMB(initialMem.workingSetBytes);

    std::vector<double> timeSamples;
    timeSamples.reserve(m_totalSteps);

    WeaponModelOutput outData;
    double totalTimeMs = 0.0;
    double minT = 1e9;
    double maxT = 0.0;

    int sampleInterval = m_totalSteps / 100;
    if (sampleInterval < 1) sampleInterval = 1;

    for (int i = 0; i < m_totalSteps; ++i) {
        auto tStart = std::chrono::high_resolution_clock::now();

        int stepRes = 0;
        bool stepOk = m_pLoader->CallStep(outData, stepRes, errStr);

        auto tEnd = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        if (!stepOk) {
            report.encounteredException = true;
            report.exceptionLog = "Step Exception at index " + std::to_string(i) + ": " + errStr;
            emit logMessage("ERROR: " + QString::fromStdString(report.exceptionLog));
            break;
        }

        timeSamples.push_back(elapsedMs);
        totalTimeMs += elapsedMs;
        if (elapsedMs < minT) minT = elapsedMs;
        if (elapsedMs > maxT) maxT = elapsedMs;

        report.completedSteps++;

        // Periodic sample reporting & real-time chart streaming
        if (i % sampleInterval == 0 || i == m_totalSteps - 1) {
            ProcessMemoryStats currentMem = MemoryUtils::GetCurrentProcessMemory();
            double curMB = MemoryUtils::BytesToMB(currentMem.workingSetBytes);
            PerfSample s;
            s.stepIndex = i;
            s.timeMs = elapsedMs;
            s.memoryMB = curMB;
            report.samples.push_back(s);

            emit sampleAdded(i, elapsedMs, curMB);
            emit progressUpdated(i + 1, m_totalSteps, elapsedMs, curMB);
        }
    }

    ProcessMemoryStats finalMem = MemoryUtils::GetCurrentProcessMemory();
    report.finalMemoryMB = MemoryUtils::BytesToMB(finalMem.workingSetBytes);
    report.memoryDeltaMB = report.finalMemoryMB - report.initialMemoryMB;

    if (report.completedSteps > 0) {
        report.avgTimeMs = totalTimeMs / report.completedSteps;
        report.minTimeMs = minT;
        report.maxTimeMs = maxT;

        // Calculate StdDev (Jitter)
        double varianceSum = 0.0;
        for (double t : timeSamples) {
            varianceSum += (t - report.avgTimeMs) * (t - report.avgTimeMs);
        }
        report.jitterMs = std::sqrt(varianceSum / report.completedSteps);

        // Memory growth per 10,000 steps
        report.memoryLeakRateMBPer10k = (report.memoryDeltaMB / report.completedSteps) * 10000.0;
    }

    // Verdict calculation
    if (report.encounteredException) {
        report.realtimeVerdict = "FAIL";
    } else if (report.avgTimeMs > report.frameBudgetMs) {
        report.realtimeVerdict = "FAIL";
    } else if (report.maxTimeMs > report.frameBudgetMs) {
        report.realtimeVerdict = "WARNING";
    } else {
        report.realtimeVerdict = "PASS";
    }

    emit logMessage(QString("INFO: Profiling finished. Completed %1/%2 steps. Avg: %3 ms, Max: %4 ms, Verdict: %5")
        .arg(report.completedSteps)
        .arg(report.totalSteps)
        .arg(report.avgTimeMs, 0, 'f', 4)
        .arg(report.maxTimeMs, 0, 'f', 4)
        .arg(QString::fromStdString(report.realtimeVerdict)));

    emit finished(report);
}
