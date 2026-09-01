#ifndef CONCURRENCY_TESTER_H
#define CONCURRENCY_TESTER_H

#include <QObject>
#include <QMetaType>
#include <string>
#include <vector>
#include "UserCodeHarness.h"

enum class ConcurrencyTestMode {
    MultiThread = 0,  // 单型号：N 线程各跑一次完整 UserMain
    MultiModel = 1    // 多型号：各型号按配置数量并行一起跑
};

struct MultiModelSpec {
    UserCodeHarness* harness = nullptr;
    int count = 1;
    std::string modelName;
};

struct ConcurrencyTestConfig {
    ConcurrencyTestMode mode = ConcurrencyTestMode::MultiThread;
    int count = 4;                          // MultiThread：线程数
    std::vector<MultiModelSpec> models;     // MultiModel：各型号 harness + 数量
    uint32_t randomSeed = 1;
};

struct ConcurrencyThreadResult {
    int threadId = 0;
    int modelIndex = 0;
    int instanceId = 0;
    std::string modelName;
    int userReturnCode = 0;
    bool exceptionOccurred = false;
    bool userReportedFail = false;
    std::string randomSummary;
    std::string errorLog;
};

struct ConcurrencyTestReport {
    ConcurrencyTestMode mode = ConcurrencyTestMode::MultiThread;
    int workerCount = 0;
    int modelTypeCount = 0;
    bool crashed = false;
    int exceptionCount = 0;
    int userFailCount = 0;
    int successCount = 0;
    bool multiThreadSafe = false;
    bool multiModelOk = false;
    std::string verdict;
    std::string summary;
    std::vector<std::string> logMessages;
    std::vector<ConcurrencyThreadResult> threadResults;
};

Q_DECLARE_METATYPE(ConcurrencyTestReport)

class ConcurrencyTester {
public:
    static ConcurrencyTestReport Run(UserCodeHarness& harness, const ConcurrencyTestConfig& config);
    static ConcurrencyTestReport RunMultiModel(const ConcurrencyTestConfig& config);
};

class ConcurrencyTestWorker : public QObject {
    Q_OBJECT
public:
    // MultiThread：传入单个 harness；MultiModel：harness 可为空，使用 config.models
    ConcurrencyTestWorker(UserCodeHarness* harness, const ConcurrencyTestConfig& config);

public slots:
    void process();

signals:
    void finished(const ConcurrencyTestReport& report);
    void logMessage(const QString& msg);

private:
    UserCodeHarness* m_pHarness;
    ConcurrencyTestConfig m_config;
};

#endif // CONCURRENCY_TESTER_H
