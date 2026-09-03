#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QTabWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTextBrowser>
#include <QListWidget>
#include <QThread>
#include <QStringList>
#include <memory>
#include <vector>
#include <functional>

#include "../core/PeAnalyzer.h"
#include "../core/HeaderAnalyzer.h"
#include "../core/LibAnalyzer.h"
#include "../core/DllLoader.h"
#include "../core/PerfProfiler.h"
#include "../core/ConcurrencyTester.h"
#include "../core/UserCodeHarness.h"
#include "../core/MultiObjectHarness.h"
#include "../core/ReportGenerator.h"
#include "../core/PackageScanner.h"

#include "LogConsoleWidget.h"
#include "CppCodeEditor.h"

class ChartViewerWidget;
class TrajectoryViewWidget;
class BusyOverlayWidget;
class QScrollArea;

struct FleetModelEntry {
    QString name;
    QString packageDir;
    QStringList headerPaths;
    QString userMainBody;
    QString userMultiObjectBody;
    std::vector<RandomVarDef> randomVars;
    int instanceCount = 1;
    std::shared_ptr<UserCodeHarness> harness;
    QString multiObjectHeaderPath;
    QString multiObjectDllPath;
    QString multiObjectSourcePath;
    QString multiObjectLibPath;
    QString multiObjectAdapterCode;
    int multiObjectCount = 3;
    int multiObjectSteps = 100;
    double multiObjectDt = 0.02;
    double multiObjectTolerance = 1e-8;
    MultiObjectSchedule multiObjectSchedule = MultiObjectSchedule::Forward;
    std::shared_ptr<MultiObjectHarness> multiObjectHarness;
    MultiObjectTestReport multiObjectReport;
    InterfaceMappingProfile multiObjectMapping;
    InterfaceHeaderSchema multiObjectSchema;
    QString status = QStringLiteral("未编译");
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void showBusyOverlay(const QString& text);
    void hideBusyOverlay();
    void setBusyOverlayText(const QString& text);
    void runBusyBlocking(const std::function<void()>& work);

private slots:
    void runFullPrecheck();
    void runStressTestOnly();
    void runTrajectoryPreview();
    void runMultiModelTest();
    void runMultiThreadTest();
    void onMultiObjectModelChanged(int index);
    void browseMultiObjectDll();
    void analyzeMultiObjectInterface();
    void validateMultiObjectMapping();
    void onMultiObjectFunctionChanged();
    void browseMultiObjectHeader();
    void browseMultiObjectSource();
    void browseMultiObjectLib();
    void compileMultiObjectAdapter();
    void runMultiObjectTest();
    void onMultiObjectResultSelectionChanged();
    void runDllFileCheckOnly();
    void runDllLoadCheckOnly();
    void runHeaderCheckOnly();
    void runLibCheckOnly();
    void exportReport();

    void addModel();
    void removeModel();
    void onModelSelectionChanged();
    void browseCurrentModelPackage();
    void refreshCurrentModelHeaders();
    void compileCurrentModel();
    void compileAllModels();
    void addRandomVarRow();
    void removeRandomVarRow();
    void onFleetCountChanged(int value);
    void onTestNavigationChanged(int row);

    void onPerfProfileProgress(int step, int total, double timeMs, double memMB);
    void onPerfProfileFinished(const PerfProfileReport& report);
    void onConcurrencyFinished(const ConcurrencyTestReport& report);
    void logMessage(const QString& msg);

private:
    void applyDarkStyle();
    void updateStatusBadges();
    void updatePeView(const PeAnalysisReport& pe);
    void updateHeaderConflictView(const HeaderConflictReport& report);
    void updateResultTable(QTableWidget* table, const ConcurrencyTestReport& report);
    void refreshModelListUi();
    void refreshModelSelectors();
    void refreshFleetCountTable();
    void refreshPeSelectors();
    void onPeModelChanged(int index);
    void onPeDllChanged(int index);
    void onLoadModelChanged(int index);
    void onLoadDllChanged(int index);
    void fillDllSelector(QComboBox* modelCombo, QComboBox* dllCombo);
    void fillPackageFileSelector(QComboBox* modelCombo, QComboBox* fileCombo,
                                 const QStringList& nameFilters);
    const CombinedPrecheckReport* findDllReport(int modelIndex, const QString& dllPath) const;
    void refreshReportBrowser();
    void updateWorkflowUi();
    void saveEditorsToCurrentModel();
    void loadEditorsFromModel(int index);
    void setEditorsEnabled(bool on);
    bool isModelPathValid(const FleetModelEntry& entry) const;
    bool isModelConfigured(const FleetModelEntry& entry) const;
    bool isModelCompiled(const FleetModelEntry& entry) const;
    bool allModelsCompiled() const;
    int currentModelIndex() const;
    int selectedTestModelIndex(QComboBox* combo) const;
    FleetModelEntry* selectedTestModel(QComboBox* combo);
    bool requireSelectedModelCompiled(QComboBox* combo, int switchToTab);
    void startConcurrencyWorker(UserCodeHarness* harness, const ConcurrencyTestConfig& cfg,
                                const QString& busyText = QString());
    void refreshCodeEditorCompletions();
    const CombinedPrecheckReport* selectedPeDllReport() const;
    UserHarnessConfig buildHarnessConfig(const FleetModelEntry& entry, int index) const;
    UserHarnessConfig buildUserMultiObjectConfig(const FleetModelEntry& entry, int index) const;
    MultiObjectHarnessConfig buildMultiObjectHarnessConfig(
        const FleetModelEntry& entry, int index) const;
    void saveMultiObjectEditor();
    void loadMultiObjectEditor(int modelIndex);
    void updateMultiObjectResultView(const MultiObjectTestReport& report);
    bool isMultiObjectConfigured(const FleetModelEntry& entry) const;
    bool prepareMultiObjectHarness(FleetModelEntry& entry, int modelIndex);
    void rebuildMultiObjectMappingTables();
    void saveMultiObjectMappingFromUi();
    QString multiObjectProfilePath(const FleetModelEntry& entry) const;
    DualBuildPrecheckReport precheckOneModel(const FleetModelEntry& entry);
    CombinedPrecheckReport runBuildPrecheck(const std::string& dllPath, const std::string& headerPath,
                                            const std::string& libPath, const std::string& buildConfig,
                                            const std::string& packageDir);
    std::string pickHeaderForDll(const ModelPackageFiles& pkg, const std::string& dllPath) const;
    static std::vector<RandomVarDef> DefaultRandomVars();
    static void FillRandomVarTable(QTableWidget* table, const std::vector<RandomVarDef>& vars);
    static std::vector<RandomVarDef> ReadRandomVarTable(QTableWidget* table);

    QPushButton* m_btnRunPrecheck;
    QPushButton* m_btnExportReport;
    QLabel* m_lblHeaderStatus;
    QLabel* m_lblLibStatus;
    QLabel* m_lblDllStatus;
    QLabel* m_lblWorkflowSummary;
    std::vector<QLabel*> m_workflowSteps;
    QTabWidget* m_pCentralTabs;
    QListWidget* m_listTestNavigation;
    QScrollArea* m_workflowScroll;

    QComboBox* m_comboHeaderModel;
    QComboBox* m_comboHeaderFile;
    QPushButton* m_btnCheckHeader;
    QLabel* m_lblHeaderResult;
    QTableWidget* m_tblHeaderFunctions;
    QLabel* m_lblHeaderConflictSummary;
    QTableWidget* m_tblHeaderConflicts;
    QComboBox* m_comboLibModel;
    QComboBox* m_comboLibFile;
    QPushButton* m_btnCheckLib;
    QLabel* m_lblLibResult;
    QTableWidget* m_tblLibSymbols;

    // Tab0 PE
    QComboBox* m_comboPeModel;
    QComboBox* m_comboPeDll;
    QPushButton* m_btnCheckDllFile;
    QComboBox* m_comboLoadModel;
    QComboBox* m_comboLoadDll;
    QPushButton* m_btnCheckDllLoad;
    QLabel* m_lblPePageHint;
    QLabel* m_lblLoadPageHint;
    QLabel* m_lblPeArch;
    QLabel* m_lblPeCrt;
    QLabel* m_lblLoadStatus;
    QLabel* m_lblLoadApi;
    QTableWidget* m_tblImports;
    QTableWidget* m_tblExports;

    // Tab1 models + UserMain
    QListWidget* m_listModels;
    QPushButton* m_btnAddModel;
    QPushButton* m_btnRemoveModel;
    QLineEdit* m_editModelName;
    QLineEdit* m_editModelPackage;
    QLabel* m_lblLicenseHint;
    QLabel* m_lblPathStageStatus;
    QLabel* m_emptyWorkflowPanel;
    QWidget* m_modelSetupPanel;
    QWidget* m_modelDetailPanel;
    QWidget* m_testSection;
    QLabel* m_testSectionTitle;
    QPushButton* m_btnBrowseModelPackage;
    QPushButton* m_btnRefreshModelHeaders;
    QListWidget* m_listHarnessHeaders;
    CppCodeEditor* m_editUserMain;
    QTableWidget* m_tblRandomVars;
    QPushButton* m_btnAddRandomVar;
    QPushButton* m_btnRemoveRandomVar;
    QPushButton* m_btnCompileCurrent;
    QPushButton* m_btnCompileAll;
    QLabel* m_lblHarnessStatus;

    // Tab2 stress
    QComboBox* m_comboStressModel;
    QSpinBox* m_spnSteps;
    QComboBox* m_comboHz;
    QPushButton* m_btnRunStress;
    QPushButton* m_btnRunTrajectory;
    QWidget* m_perfOptionsPanel;
    QWidget* m_trajectoryPanel;
    QLabel* m_lblPerfPageHint;
    QLabel* m_lblPerfSummary;
    QLabel* m_lblTrajOut;
    ChartViewerWidget* m_pChartViewer;
    TrajectoryViewWidget* m_pTrajectoryView;
    QTableWidget* m_tblTrajectoryPoints;

    // Tab3 multi-model
    QTableWidget* m_tblFleetCounts;
    QLabel* m_lblMultiModelPageHint;
    QPushButton* m_btnRunMultiModel;
    QLabel* m_lblMultiModelSummary;
    QTableWidget* m_tblMultiModelResults;

    // Tab4 multi-thread
    QComboBox* m_comboThreadModel;
    QLabel* m_lblMultiThreadPageHint;
    QSpinBox* m_spnThreadCount;
    QPushButton* m_btnRunMultiThread;
    QLabel* m_lblMultiThreadSummary;
    QTableWidget* m_tblMultiThreadResults;

    // Single-thread interleaved multi-object
    QComboBox* m_comboMultiObjectModel;
    QLabel* m_lblMultiObjectPageHint;
    CppCodeEditor* m_editUserMultiObject;
    QWidget* m_multiObjectLegacyPanel;
    QLineEdit* m_editMultiObjectDll;
    QComboBox* m_comboMultiObjectHeader;
    QComboBox* m_comboMultiObjectLib;
    QComboBox* m_comboMultiObjectCreate;
    QComboBox* m_comboMultiObjectInit;
    QComboBox* m_comboMultiObjectStep;
    QComboBox* m_comboMultiObjectDestroy;
    QTableWidget* m_tblMultiObjectParameters;
    QTableWidget* m_tblMultiObjectFields;
    QPushButton* m_btnAnalyzeMultiObject;
    QPushButton* m_btnValidateMultiObject;
    QLineEdit* m_editMultiObjectHeader;
    QLineEdit* m_editMultiObjectSource;
    QLineEdit* m_editMultiObjectLib;
    CppCodeEditor* m_editMultiObjectAdapter;
    QSpinBox* m_spnMultiObjectCount;
    QSpinBox* m_spnMultiObjectSteps;
    QDoubleSpinBox* m_spnMultiObjectDt;
    QDoubleSpinBox* m_spnMultiObjectTolerance;
    QComboBox* m_comboMultiObjectSchedule;
    QPushButton* m_btnCompileMultiObject;
    QPushButton* m_btnRunMultiObject;
    QLabel* m_lblMultiObjectAdapterStatus;
    QLabel* m_lblMultiObjectResult;
    QTableWidget* m_tblMultiObjectResults;
    TrajectoryViewWidget* m_pMultiObjectTrajectory;
    int m_loadedMultiObjectModelIndex = -1;

    // Report
    QTextBrowser* m_pReportBrowser;
    LogConsoleWidget* m_pLogConsole;

    DllLoader m_dllLoader;
    std::vector<FleetModelEntry> m_models;
    int m_currentModelIndex = -1;
    bool m_blockModelUi = false;
    bool m_showSingleItemReport = false;

    CombinedPrecheckReport m_latestReport;
    FleetSessionReport m_latestFleetReport;
    DualBuildPrecheckReport m_latestDualReport; // aggregate badges / last single
    QThread* m_pWorkerThread = nullptr;
    BusyOverlayWidget* m_busyOverlay = nullptr;
};

#endif // MAIN_WINDOW_H
