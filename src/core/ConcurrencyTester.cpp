#include "ConcurrencyTester.h"
#include "../utils/QtEncoding.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>

static ConcurrencyThreadResult RunOneUserMain(UserCodeHarness& harness, int threadId,
                                             int modelIndex, int instanceId,
                                             const std::string& modelName,
                                             const RandomValueBlob& blob) {
    ConcurrencyThreadResult tr;
    tr.threadId = threadId;
    tr.modelIndex = modelIndex;
    tr.instanceId = instanceId;
    tr.modelName = modelName;
    tr.randomSummary = blob.summary;

    int userRet = 0;
    bool seh = false;
    std::string err;
    if (!harness.RunOnce(blob, &userRet, &seh, err)) {
        tr.exceptionOccurred = true;
        tr.errorLog = err;
        return tr;
    }
    tr.userReturnCode = userRet;
    if (userRet != 0) {
        tr.userReportedFail = true;
        tr.errorLog = "UserMain 返回非 0: " + std::to_string(userRet);
    }
    return tr;
}

static void FinalizeReport(ConcurrencyTestReport& report) {
    report.crashed = report.exceptionCount > 0;
    report.multiThreadSafe = (report.mode == ConcurrencyTestMode::MultiThread)
        ? (report.exceptionCount == 0) : false;
    report.multiModelOk = (report.mode == ConcurrencyTestMode::MultiModel)
        ? (report.exceptionCount == 0 && report.userFailCount == 0) : false;

    if (report.exceptionCount > 0) {
        report.verdict = "FAIL";
        report.summary = (report.mode == ConcurrencyTestMode::MultiModel)
            ? "多型号并行执行 UserMain 时发生 SEH 崩溃/硬件异常"
            : "多线程执行 UserMain 时发生 SEH 崩溃/硬件异常";
    } else if (report.userFailCount > 0) {
        report.verdict = "FAIL";
        report.summary = "部分 UserMain 返回失败码（无崩溃）";
    } else {
        report.verdict = "PASS";
        if (report.mode == ConcurrencyTestMode::MultiModel) {
            report.summary = "多型号并行完成（型号数=" + std::to_string(report.modelTypeCount)
                + ", 总实例=" + std::to_string(report.workerCount) + "）";
        } else {
            report.summary = "全部线程成功完成 UserMain（线程数="
                + std::to_string(report.workerCount) + "）";
        }
    }

    report.logMessages.push_back("INFO: 成功=" + std::to_string(report.successCount)
        + " 用户失败=" + std::to_string(report.userFailCount)
        + " 异常=" + std::to_string(report.exceptionCount)
        + " 判定=" + report.verdict);
}

ConcurrencyTestReport ConcurrencyTester::Run(UserCodeHarness& harness, const ConcurrencyTestConfig& config) {
    ConcurrencyTestReport report;
    report.mode = ConcurrencyTestMode::MultiThread;
    report.workerCount = config.count > 0 ? config.count : 1;

    if (!harness.IsLoaded()) {
        report.verdict = "FAIL";
        report.summary = "用户 UserMain 未编译/未加载，请先在「用户代码」页编译";
        report.logMessages.push_back("FAIL: " + report.summary);
        return report;
    }

    report.logMessages.push_back("INFO: 多线程测试 — 同时启动 " + std::to_string(report.workerCount)
        + " 个线程，各跑一次完整 UserMain");

    std::vector<ConcurrencyThreadResult> results(static_cast<size_t>(report.workerCount));
    std::atomic<bool> anyCrash{ false };

    auto workerFn = [&](int id) {
        uint32_t seed = config.randomSeed + static_cast<uint32_t>(id) * 9973u
            + static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count() & 0xffff);
        RandomValueBlob blob = harness.Sample(seed);
        results[static_cast<size_t>(id)] = RunOneUserMain(harness, id, 0, id, "default", blob);
        if (results[static_cast<size_t>(id)].exceptionOccurred) {
            anyCrash = true;
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(report.workerCount));
    for (int i = 0; i < report.workerCount; ++i) {
        threads.emplace_back(workerFn, i);
    }
    for (auto& t : threads) t.join();

    report.threadResults = results;
    report.crashed = anyCrash.load();
    for (const auto& tr : results) {
        if (tr.exceptionOccurred) report.exceptionCount++;
        else if (tr.userReportedFail) report.userFailCount++;
        else report.successCount++;

        if (!tr.errorLog.empty()) {
            report.logMessages.push_back("Thread#" + std::to_string(tr.threadId)
                + " [" + tr.randomSummary + "] " + tr.errorLog);
        } else {
            report.logMessages.push_back("Thread#" + std::to_string(tr.threadId)
                + " OK [" + tr.randomSummary + "] ret=" + std::to_string(tr.userReturnCode));
        }
    }

    FinalizeReport(report);
    return report;
}

ConcurrencyTestReport ConcurrencyTester::RunMultiModel(const ConcurrencyTestConfig& config) {
    ConcurrencyTestReport report;
    report.mode = ConcurrencyTestMode::MultiModel;
    report.modelTypeCount = static_cast<int>(config.models.size());

    struct Job {
        UserCodeHarness* harness;
        int modelIndex;
        int instanceId;
        std::string modelName;
    };
    std::vector<Job> jobs;
    for (size_t mi = 0; mi < config.models.size(); ++mi) {
        const MultiModelSpec& spec = config.models[mi];
        if (!spec.harness || !spec.harness->IsLoaded()) {
            report.verdict = "FAIL";
            report.summary = "型号未编译/未加载: " + (spec.modelName.empty() ? ("#" + std::to_string(mi)) : spec.modelName);
            report.logMessages.push_back("FAIL: " + report.summary);
            return report;
        }
        int n = spec.count > 0 ? spec.count : 1;
        for (int k = 0; k < n; ++k) {
            Job j;
            j.harness = spec.harness;
            j.modelIndex = static_cast<int>(mi);
            j.instanceId = k;
            j.modelName = spec.modelName.empty() ? ("model" + std::to_string(mi)) : spec.modelName;
            jobs.push_back(j);
        }
    }

    report.workerCount = static_cast<int>(jobs.size());
    if (jobs.empty()) {
        report.verdict = "FAIL";
        report.summary = "未配置任何型号实例（请添加型号并设置数量）";
        report.logMessages.push_back("FAIL: " + report.summary);
        return report;
    }

    {
        std::ostringstream info;
        info << "INFO: 多型号并行 — 型号数=" << report.modelTypeCount << " 总实例=" << report.workerCount << " [";
        for (size_t mi = 0; mi < config.models.size(); ++mi) {
            if (mi) info << ", ";
            info << config.models[mi].modelName << "×" << config.models[mi].count;
        }
        info << "]";
        report.logMessages.push_back(info.str());
    }

    std::vector<ConcurrencyThreadResult> results(jobs.size());
    std::atomic<bool> anyCrash{ false };

    auto workerFn = [&](int jobId) {
        const Job& job = jobs[static_cast<size_t>(jobId)];
        uint32_t seed = config.randomSeed
            + static_cast<uint32_t>(job.modelIndex) * 100003u
            + static_cast<uint32_t>(job.instanceId) * 9973u
            + static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count() & 0xffff);
        RandomValueBlob blob = job.harness->Sample(seed);
        results[static_cast<size_t>(jobId)] = RunOneUserMain(
            *job.harness, jobId, job.modelIndex, job.instanceId, job.modelName, blob);
        if (results[static_cast<size_t>(jobId)].exceptionOccurred) {
            anyCrash = true;
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(jobs.size());
    for (int i = 0; i < static_cast<int>(jobs.size()); ++i) {
        threads.emplace_back(workerFn, i);
    }
    for (auto& t : threads) t.join();

    report.threadResults = results;
    report.crashed = anyCrash.load();
    for (const auto& tr : results) {
        if (tr.exceptionOccurred) report.exceptionCount++;
        else if (tr.userReportedFail) report.userFailCount++;
        else report.successCount++;

        std::string tag = tr.modelName + "[" + std::to_string(tr.instanceId) + "]";
        if (!tr.errorLog.empty()) {
            report.logMessages.push_back(tag + " [" + tr.randomSummary + "] " + tr.errorLog);
        } else {
            report.logMessages.push_back(tag + " OK [" + tr.randomSummary + "] ret="
                + std::to_string(tr.userReturnCode));
        }
    }

    FinalizeReport(report);
    return report;
}

ConcurrencyTestWorker::ConcurrencyTestWorker(UserCodeHarness* harness, const ConcurrencyTestConfig& config)
    : m_pHarness(harness)
    , m_config(config) {
}

void ConcurrencyTestWorker::process() {
    const bool isMulti = (m_config.mode == ConcurrencyTestMode::MultiModel);
    emit logMessage(QString("INFO: ") + (isMulti ? "多型号并行测试开始..." : "多线程（并行）测试开始..."));

    ConcurrencyTestReport report;
    if (isMulti) {
        report = ConcurrencyTester::RunMultiModel(m_config);
    } else {
        if (!m_pHarness) {
            report.verdict = "FAIL";
            report.summary = "Harness 为空";
            emit finished(report);
            return;
        }
        report = ConcurrencyTester::Run(*m_pHarness, m_config);
    }

    for (const auto& msg : report.logMessages) {
        emit logMessage(qDecodeLog(msg));
    }
    emit finished(report);
}
