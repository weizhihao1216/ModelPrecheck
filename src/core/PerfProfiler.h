#ifndef PERF_PROFILER_H
#define PERF_PROFILER_H

#include <QObject>
#include <QMetaType>
#include <vector>
#include <string>
#include "UserCodeHarness.h"

struct PerfSample {
    int stepIndex = 0;   // UserMain 第几次执行（从 0 起）
    double timeMs = 0.0;
    double memoryMB = 0.0;
};

struct PerfProfileReport {
    int totalSteps = 0;       // 计划执行 UserMain 次数
    int completedSteps = 0;
    double targetHz = 50.0;
    double frameBudgetMs = 20.0;

    double minTimeMs = 0.0;
    double maxTimeMs = 0.0;
    double avgTimeMs = 0.0;
    double jitterMs = 0.0;

    double initialMemoryMB = 0.0;
    double finalMemoryMB = 0.0;
    double memoryDeltaMB = 0.0;
    double memoryLeakRateMBPer10k = 0.0;

    std::string realtimeVerdict; // PASS / WARNING / FAIL
    bool encounteredException = false;
    std::string exceptionLog;

    std::vector<PerfSample> samples;
};

Q_DECLARE_METATYPE(PerfProfileReport)

// 通过反复调用已编译的 UserMain 做性能压测（串行、尽快连跑）
class PerfProfilerWorker : public QObject {
    Q_OBJECT
public:
    PerfProfilerWorker(UserCodeHarness* harness, int totalRuns, double targetHz, uint32_t randomSeed = 1);
    ~PerfProfilerWorker();

public slots:
    void process();

signals:
    void progressUpdated(int currentStep, int totalSteps, double timeMs, double memoryMB);
    void sampleAdded(int stepIndex, double timeMs, double memoryMB);
    void finished(const PerfProfileReport& report);
    void logMessage(const QString& msg);

private:
    UserCodeHarness* m_pHarness;
    int m_totalRuns;
    double m_targetHz;
    uint32_t m_randomSeed;
};

#endif // PERF_PROFILER_H
