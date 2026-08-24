#ifndef PERF_PROFILER_H
#define PERF_PROFILER_H

#include <QThread>
#include <QObject>
#include <QMetaType>
#include <vector>
#include <string>
#include "DllLoader.h"

struct PerfSample {
    int stepIndex;
    double timeMs;
    double memoryMB;
};

struct PerfProfileReport {
    int totalSteps = 0;
    int completedSteps = 0;
    double targetHz = 50.0;
    double frameBudgetMs = 20.0; // 1000.0 / targetHz

    double minTimeMs = 0.0;
    double maxTimeMs = 0.0;
    double avgTimeMs = 0.0;
    double jitterMs = 0.0;       // Standard deviation

    double initialMemoryMB = 0.0;
    double finalMemoryMB = 0.0;
    double memoryDeltaMB = 0.0;
    double memoryLeakRateMBPer10k = 0.0;

    std::string realtimeVerdict; // "PASS", "WARNING", "FAIL"
    bool encounteredException = false;
    std::string exceptionLog;

    std::vector<PerfSample> samples;
};

Q_DECLARE_METATYPE(PerfProfileReport)

class PerfProfilerWorker : public QObject {
    Q_OBJECT
public:
    PerfProfilerWorker(DllLoader* loader, const WeaponModelParams& params, int totalSteps, double targetHz);
    ~PerfProfilerWorker();

public slots:
    void process();

signals:
    void progressUpdated(int currentStep, int totalSteps, double timeMs, double memoryMB);
    void sampleAdded(int stepIndex, double timeMs, double memoryMB);
    void finished(const PerfProfileReport& report);
    void logMessage(const QString& msg);

private:
    DllLoader* m_pLoader;
    WeaponModelParams m_params;
    int m_totalSteps;
    double m_targetHz;
};

#endif // PERF_PROFILER_H
