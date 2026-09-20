#ifndef QML_APP_CONTROLLER_H
#define QML_APP_CONTROLLER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <vector>

#include "core/SessionStore.h"

class QThread;
class UserCodeHarness;

class QmlAppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList models READ models NOTIFY dashboardChanged)
    Q_PROPERTY(int modelCount READ modelCount NOTIFY dashboardChanged)
    Q_PROPERTY(int compiledCount READ compiledCount NOTIFY dashboardChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY dashboardChanged)
    Q_PROPERTY(int readiness READ readiness NOTIFY dashboardChanged)
    Q_PROPERTY(QString nextActionTitle READ nextActionTitle NOTIFY dashboardChanged)
    Q_PROPERTY(QString nextActionDetail READ nextActionDetail NOTIFY dashboardChanged)
    Q_PROPERTY(QString sessionLabel READ sessionLabel NOTIFY dashboardChanged)
    Q_PROPERTY(int selectedModelIndex READ selectedModelIndex NOTIFY modelDetailsChanged)
    Q_PROPERTY(QString selectedModelName READ selectedModelName NOTIFY modelDetailsChanged)
    Q_PROPERTY(QString selectedPackageDir READ selectedPackageDir NOTIFY modelDetailsChanged)
    Q_PROPERTY(QString selectedUserMain READ selectedUserMain NOTIFY modelDetailsChanged)
    Q_PROPERTY(QVariantList selectedRandomVars READ selectedRandomVars NOTIFY modelDetailsChanged)
    Q_PROPERTY(QString configurationMessage READ configurationMessage NOTIFY modelDetailsChanged)
    Q_PROPERTY(QVariantList selectedPackageContents READ selectedPackageContents NOTIFY modelDetailsChanged)
    Q_PROPERTY(bool selectedPackageLayoutValid READ selectedPackageLayoutValid NOTIFY modelDetailsChanged)
    Q_PROPERTY(QString selectedPackageSummary READ selectedPackageSummary NOTIFY modelDetailsChanged)
    Q_PROPERTY(int selectedMultiObjectCount READ selectedMultiObjectCount NOTIFY multiObjectChanged)
    Q_PROPERTY(int selectedMultiObjectSteps READ selectedMultiObjectSteps NOTIFY multiObjectChanged)
    Q_PROPERTY(double selectedMultiObjectDt READ selectedMultiObjectDt NOTIFY multiObjectChanged)
    Q_PROPERTY(double selectedMultiObjectTolerance READ selectedMultiObjectTolerance NOTIFY multiObjectChanged)
    Q_PROPERTY(int selectedMultiObjectSchedule READ selectedMultiObjectSchedule NOTIFY multiObjectChanged)
    Q_PROPERTY(bool selectedMultiObjectCompiled READ selectedMultiObjectCompiled NOTIFY multiObjectChanged)
    Q_PROPERTY(QString multiObjectMessage READ multiObjectMessage NOTIFY multiObjectChanged)
    Q_PROPERTY(bool multiObjectBusy READ multiObjectBusy NOTIFY multiObjectChanged)
    Q_PROPERTY(QVariantList multiObjectResultItems READ multiObjectResultItems NOTIFY multiObjectChanged)
    Q_PROPERTY(QVariantList multiObjectTrajectories READ multiObjectTrajectories NOTIFY multiObjectChanged)
    Q_PROPERTY(QString multiObjectVerdict READ multiObjectVerdict NOTIFY multiObjectChanged)
    Q_PROPERTY(QString multiObjectSummary READ multiObjectSummary NOTIFY multiObjectChanged)
    Q_PROPERTY(bool hasMultiObjectResult READ hasMultiObjectResult NOTIFY multiObjectChanged)
    Q_PROPERTY(double multiObjectMaxDeviation READ multiObjectMaxDeviation NOTIFY multiObjectChanged)
    Q_PROPERTY(double multiObjectMaxFrameMs READ multiObjectMaxFrameMs NOTIFY multiObjectChanged)
    Q_PROPERTY(double multiObjectMemoryDeltaMB READ multiObjectMemoryDeltaMB NOTIFY multiObjectChanged)
    Q_PROPERTY(QVariantList fleetMultiObjectModels READ fleetMultiObjectModels NOTIFY multiObjectChanged)
    Q_PROPERTY(bool multiObjectResultIsFleet READ multiObjectResultIsFleet NOTIFY multiObjectChanged)
    Q_PROPERTY(bool compileBusy READ compileBusy NOTIFY compileStateChanged)
    /// 等候进度：0.0~1.0 表示确定进度，负值表示不确定（转圈/往返条）。
    Q_PROPERTY(double busyProgress READ busyProgress NOTIFY busyProgressChanged)
    Q_PROPERTY(QVariantList resultItems READ resultItems NOTIFY resultsChanged)
    Q_PROPERTY(int resultPassCount READ resultPassCount NOTIFY resultsChanged)
    Q_PROPERTY(int resultWarnCount READ resultWarnCount NOTIFY resultsChanged)
    Q_PROPERTY(int resultFailCount READ resultFailCount NOTIFY resultsChanged)
    Q_PROPERTY(int resultPendingCount READ resultPendingCount NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList resultModels READ resultModels NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList resultConcurrency READ resultConcurrency NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap resultHeaderConflicts READ resultHeaderConflicts NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList resultMultiObject READ resultMultiObject NOTIFY resultsChanged)
    Q_PROPERTY(QString resultTimestamp READ resultTimestamp NOTIFY resultsChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY resultsChanged)
    Q_PROPERTY(bool resultOverallPass READ resultOverallPass NOTIFY resultsChanged)
    Q_PROPERTY(QString cachedReportPath READ cachedReportPath NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList reportHistory READ reportHistory NOTIFY resultsChanged)
    Q_PROPERTY(int reportHistoryCount READ reportHistoryCount NOTIFY resultsChanged)
    Q_PROPERTY(bool precheckRunning READ precheckRunning NOTIFY precheckStateChanged)
    Q_PROPERTY(QString precheckProgress READ precheckProgress NOTIFY precheckStateChanged)
    Q_PROPERTY(QStringList logLines READ logLines NOTIFY logsChanged)
    Q_PROPERTY(int logCount READ logCount NOTIFY logsChanged)
    Q_PROPERTY(QString lastLogLine READ lastLogLine NOTIFY logsChanged)
    Q_PROPERTY(QString precheckError READ precheckError NOTIFY precheckStateChanged)
    Q_PROPERTY(int perfSteps READ perfSteps NOTIFY settingsChanged)
    Q_PROPERTY(double perfHz READ perfHz NOTIFY settingsChanged)
    Q_PROPERTY(int perfMemCapMB READ perfMemCapMB NOTIFY settingsChanged)
    Q_PROPERTY(int threadCount READ threadCount NOTIFY settingsChanged)
    Q_PROPERTY(bool toolchainReady READ toolchainReady NOTIFY settingsChanged)
    Q_PROPERTY(QString toolchainPath READ toolchainPath NOTIFY settingsChanged)
    Q_PROPERTY(bool specialtyBusy READ specialtyBusy NOTIFY specialtyChanged)
    Q_PROPERTY(QString specialtyMessage READ specialtyMessage NOTIFY specialtyChanged)
    Q_PROPERTY(QVariantMap specialtyPerformance READ specialtyPerformance NOTIFY specialtyChanged)
    Q_PROPERTY(QVariantList specialtyTrajectory READ specialtyTrajectory NOTIFY specialtyChanged)
    Q_PROPERTY(QVariantMap specialtyConcurrency READ specialtyConcurrency NOTIFY specialtyChanged)
    Q_PROPERTY(QVariantMap specialtyStaticReport READ specialtyStaticReport NOTIFY specialtyChanged)
    Q_PROPERTY(bool restorePending READ restorePending NOTIFY restoreStateChanged)
    Q_PROPERTY(int restoreModelCount READ restoreModelCount NOTIFY restoreStateChanged)
    Q_PROPERTY(QString restoreSavedAt READ restoreSavedAt NOTIFY restoreStateChanged)

public:
    explicit QmlAppController(QObject* parent = nullptr);
    ~QmlAppController() override;

    QVariantList models() const { return m_models; }
    int modelCount() const { return m_models.size(); }
    int compiledCount() const { return m_compiledCount; }
    int pendingCount() const { return qMax(0, modelCount() - m_compiledCount); }
    int readiness() const { return m_readiness; }
    QString nextActionTitle() const { return m_nextActionTitle; }
    QString nextActionDetail() const { return m_nextActionDetail; }
    QString sessionLabel() const { return m_sessionLabel; }
    int selectedModelIndex() const { return m_selectedModelIndex; }
    QString selectedModelName() const;
    QString selectedPackageDir() const;
    QString selectedUserMain() const;
    QVariantList selectedRandomVars() const;
    QString configurationMessage() const { return m_configurationMessage; }
    QVariantList selectedPackageContents() const;
    bool selectedPackageLayoutValid() const;
    QString selectedPackageSummary() const;
    int selectedMultiObjectCount() const;
    int selectedMultiObjectSteps() const;
    double selectedMultiObjectDt() const;
    double selectedMultiObjectTolerance() const;
    int selectedMultiObjectSchedule() const;
    bool selectedMultiObjectCompiled() const;
    QString multiObjectMessage() const { return m_multiObjectMessage; }
    bool multiObjectBusy() const { return m_multiObjectBusy; }
    QVariantList multiObjectResultItems() const { return m_multiObjectResultItems; }
    QVariantList multiObjectTrajectories() const { return m_multiObjectTrajectories; }
    QString multiObjectVerdict() const { return m_multiObjectVerdict; }
    QString multiObjectSummary() const { return m_multiObjectSummary; }
    bool hasMultiObjectResult() const { return !m_multiObjectResultItems.isEmpty(); }
    double multiObjectMaxDeviation() const { return m_multiObjectMaxDeviation; }
    double multiObjectMaxFrameMs() const { return m_multiObjectMaxFrameMs; }
    double multiObjectMemoryDeltaMB() const { return m_multiObjectMemoryDeltaMB; }
    QVariantList fleetMultiObjectModels() const;
    bool multiObjectResultIsFleet() const { return m_multiObjectResultIsFleet; }
    bool compileBusy() const { return m_compileBusy; }
    double busyProgress() const { return m_busyProgress; }
    QVariantList resultItems() const { return m_resultItems; }
    int resultPassCount() const { return m_resultPassCount; }
    int resultWarnCount() const { return m_resultWarnCount; }
    int resultFailCount() const { return m_resultFailCount; }
    int resultPendingCount() const { return m_resultPendingCount; }
    QVariantList resultModels() const { return m_resultModels; }
    QVariantList resultConcurrency() const { return m_resultConcurrency; }
    QVariantMap resultHeaderConflicts() const { return m_resultHeaderConflicts; }
    QVariantList resultMultiObject() const { return m_resultMultiObject; }
    QString resultTimestamp() const { return m_resultTimestamp; }
    bool hasResult() const { return !m_resultItems.isEmpty(); }
    bool resultOverallPass() const { return m_resultOverallPass; }
    QString cachedReportPath() const { return m_cachedReportPath; }
    QVariantList reportHistory() const { return m_reportHistory; }
    int reportHistoryCount() const { return m_reportHistory.size(); }
    bool precheckRunning() const { return m_precheckRunning; }
    QString precheckProgress() const { return m_precheckProgress; }
    QStringList logLines() const { return m_logLines; }
    int logCount() const { return m_logLines.size(); }
    QString lastLogLine() const { return m_logLines.isEmpty() ? QString() : m_logLines.last(); }
    QString precheckError() const { return m_precheckError; }
    int perfSteps() const { return m_snapshot.perfSteps; }
    double perfHz() const { return m_snapshot.perfHz; }
    int perfMemCapMB() const { return m_snapshot.perfMemCapMB; }
    int threadCount() const { return m_snapshot.threadCount; }
    bool toolchainReady() const { return !m_toolchainPath.isEmpty(); }
    QString toolchainPath() const { return m_toolchainPath; }
    bool specialtyBusy() const { return m_specialtyBusy; }
    QString specialtyMessage() const { return m_specialtyMessage; }
    QVariantMap specialtyPerformance() const { return m_specialtyPerformance; }
    QVariantList specialtyTrajectory() const { return m_specialtyTrajectory; }
    QVariantMap specialtyConcurrency() const { return m_specialtyConcurrency; }
    QVariantMap specialtyStaticReport() const { return m_specialtyStaticReport; }
    bool restorePending() const { return m_restorePending; }
    int restoreModelCount() const { return static_cast<int>(m_restoreSnapshot.models.size()); }
    QString restoreSavedAt() const {
        return m_restoreSnapshot.savedAt.isValid()
            ? m_restoreSnapshot.savedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString();
    }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void runPrecheck();
    Q_INVOKABLE void openReport();
    Q_INVOKABLE void exportReport();
    Q_INVOKABLE void selectModel(int modelIndex);
    Q_INVOKABLE void addModelInQml();
    Q_INVOKABLE void removeSelectedModel();
    Q_INVOKABLE void browseSelectedPackage();
    Q_INVOKABLE void setHeaderSelected(const QString& path, bool selected);
    Q_INVOKABLE void selectAllPackageHeaders(bool selected);
    Q_INVOKABLE void saveSelectedUserMain(const QString& code);
    Q_INVOKABLE void compileSelectedModel(const QString& code);
    Q_INVOKABLE void compileAllModelsInQml(const QString& currentCode);
    Q_INVOKABLE void addRandomVariable();
    Q_INVOKABLE void updateRandomVariable(int row, bool enabled, const QString& name,
                                          const QString& type, double minimum,
                                          double maximum);
    Q_INVOKABLE void removeRandomVariable(int row);
    // 步数/步长属于“型号与代码”页的运行参数（运行期生效，无需重新编译）。
    Q_INVOKABLE void saveStepSettings(int steps, double dt);
    // 多对象专项：对象数、判定容差与调度顺序在专项测试弹框中设置。
    Q_INVOKABLE void saveMultiObjectConfiguration(int objectCount,
                                                   double tolerance, int schedule);
    Q_INVOKABLE void runSelectedMultiObject(int objectCount,
                                             double tolerance, int schedule);
    Q_INVOKABLE void initializeFleetMultiObjectSelection();
    Q_INVOKABLE void updateFleetMultiObjectModel(int modelIndex, bool selected,
                                                  int objectCount);
    Q_INVOKABLE void runFleetMultiObject(double tolerance, int schedule);
    Q_INVOKABLE void openCachedReport();
    Q_INVOKABLE void openReportAt(int index);
    Q_INVOKABLE void exportReportAt(int index);
    Q_INVOKABLE void openReportsFolder();
    Q_INVOKABLE void clearLogs();
    Q_INVOKABLE void exportLogs();
    Q_INVOKABLE void saveRuntimeSettings(int perfSteps, double perfHz,
                                          int perfMemCapMB, int threadCount);
    Q_INVOKABLE void resetRuntimeSettings();
    Q_INVOKABLE void attachCppHighlighter(QObject* quickTextDocument);
    /** 从当前型号头文件提取结构体 / 类 / 枚举 / typedef / 函数名，供代码编辑器补全。 */
    Q_INVOKABLE QStringList selectedHeaderSymbols() const;
    Q_INVOKABLE void runSelectedPerformanceTest();
    Q_INVOKABLE void runSelectedTrajectoryTest();
    Q_INVOKABLE void runSelectedMultiThreadTest();
    Q_INVOKABLE void runMultiModelParallelTest(const QVariantList& modelIndexes);
    Q_INVOKABLE void runSelectedStaticChecks();
    Q_INVOKABLE void updateModelInstanceCount(int modelIndex, int count);
    Q_INVOKABLE void acceptSessionRestore();
    Q_INVOKABLE void skipSessionRestore();

signals:
    void dashboardChanged();
    void modelDetailsChanged();
    void compileStateChanged();
    void busyProgressChanged();
    void resultsChanged();
    void precheckStateChanged();
    void precheckFinished();
    void logsChanged();
    void settingsChanged();
    void multiObjectChanged();
    void specialtyChanged();
    void restoreStateChanged();

private:
    bool saveSnapshot();
    SessionModelSnapshot* selectedSnapshotModel();
    const SessionModelSnapshot* selectedSnapshotModel() const;
    void rebuildDashboardFromSnapshot();
    CompileResult compileSnapshotModel(SessionModelSnapshot& model, int modelIndex);
    // 一键预检前自动编译：缺少有效 DLL 但已配置模型包与对象代码的型号会先行编译。
    void ensureModelsCompiledForPrecheck();
    void invalidateSelectedCompilation(const QString& message);
    void acceptPrecheckSummary(const QVariantList& items, int passCount,
                               int warnCount, int failCount, int pendingCount,
                               const QString& timestamp, bool overallPass,
                               const QString& cachedReportPath);
    void acceptPrecheckModelDetails(const QVariantList& models);
    void acceptPrecheckConcurrencyDetails(const QVariantList& reports);
    void acceptPrecheckHeaderConflicts(const QVariantMap& report);
    void acceptPrecheckMultiObjectDetails(const QVariantList& reports);
    void loadResultSnapshot();
    void saveResultSnapshot() const;
    void loadReportHistory();
    void saveReportHistory() const;
    void archiveCurrentReport();
    bool prepareSpecialtyHarness();
    void finishSpecialtyThread(QThread* thread);

    QVariantList m_models;
    int m_compiledCount = 0;
    int m_readiness = 0;
    QString m_nextActionTitle;
    QString m_nextActionDetail;
    QString m_sessionLabel;
    SessionSnapshot m_snapshot;
    bool m_hasSnapshot = false;
    int m_selectedModelIndex = -1;
    QString m_configurationMessage;
    bool m_compileBusy = false;
    double m_busyProgress = -1.0;

    /** 更新等候进度；0~1 为确定进度，负值为不确定。 */
    void setBusyProgress(double value);
    /** 追加一条带时间戳的运行日志，并裁剪到最近 1000 条。 */
    void appendLog(const QString& text);
    QVariantList m_resultItems;
    QVariantList m_resultModels;
    QVariantList m_resultConcurrency;
    QVariantMap m_resultHeaderConflicts;
    QVariantList m_resultMultiObject;
    int m_resultPassCount = 0;
    int m_resultWarnCount = 0;
    int m_resultFailCount = 0;
    int m_resultPendingCount = 0;
    QString m_resultTimestamp;
    bool m_resultOverallPass = false;
    QString m_cachedReportPath;
    QVariantList m_reportHistory;
    bool m_precheckRunning = false;
    QString m_precheckProgress;
    QStringList m_logLines;
    QString m_precheckError;
    bool m_receivedResultCurrentRun = false;
    QString m_toolchainPath;
    QString m_multiObjectMessage;
    bool m_multiObjectBusy = false;
    QVariantList m_multiObjectResultItems;
    QVariantList m_multiObjectTrajectories;
    QString m_multiObjectVerdict;
    QString m_multiObjectSummary;
    double m_multiObjectMaxDeviation = 0.0;
    double m_multiObjectMaxFrameMs = 0.0;
    double m_multiObjectMemoryDeltaMB = 0.0;
    QSet<int> m_fleetMultiObjectSelection;
    QHash<int, int> m_fleetMultiObjectCounts;
    bool m_multiObjectResultIsFleet = false;
    bool m_specialtyBusy = false;
    QString m_specialtyMessage;
    QVariantMap m_specialtyPerformance;
    QVariantList m_specialtyTrajectory;
    QVariantMap m_specialtyConcurrency;
    QVariantMap m_specialtyStaticReport;
    QThread* m_specialtyThread = nullptr;
    QThread* m_precheckThread = nullptr;
    std::unique_ptr<UserCodeHarness> m_specialtyHarness;
    std::vector<std::unique_ptr<UserCodeHarness>> m_specialtyFleetHarnesses;
    SessionSnapshot m_restoreSnapshot;
    bool m_restorePending = false;
};

#endif // QML_APP_CONTROLLER_H
