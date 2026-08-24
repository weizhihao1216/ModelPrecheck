#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QTabWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QTextBrowser>
#include <QThread>

#include "../core/PeAnalyzer.h"
#include "../core/HeaderAnalyzer.h"
#include "../core/LibAnalyzer.h"
#include "../core/DllLoader.h"
#include "../core/PerfProfiler.h"
#include "../core/FunctionalVerifier.h"
#include "../core/ReportGenerator.h"
#include "../core/PackageScanner.h"

#include "LogConsoleWidget.h"
#include "PropertyEditorWidget.h"

class ChartViewerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void browsePackageDir();
    void browseSearchDir();
    void runFullPrecheck();
    void runStressTestOnly();
    void exportReport();

    void onPerfProfileProgress(int step, int total, double timeMs, double memMB);
    void onPerfProfileFinished(const PerfProfileReport& report);
    void logMessage(const QString& msg);

private:
    void applyDarkStyle();
    void updateStatusBadges();
    CombinedPrecheckReport runBuildPrecheck(const std::string& dllPath, const std::string& headerPath, const std::string& libPath, const std::string& buildConfig);

    // Top Controls (One-click Package Directory Selection)
    QLineEdit* m_editPackageDir;
    QLineEdit* m_editSearchDir;
    QPushButton* m_btnBrowsePackageDir;
    QPushButton* m_btnBrowseDir;
    QPushButton* m_btnRunPrecheck;
    QPushButton* m_btnExportReport;

    // Status Indicator Labels (All Files Batch Status)
    QLabel* m_lblHeaderStatus;
    QLabel* m_lblLibStatus;
    QLabel* m_lblDllStatus;

    // Central Tabbed Widget
    QTabWidget* m_pCentralTabs;

    // Tab 1: Static PE View
    QLabel* m_lblPeArch;
    QLabel* m_lblPeCrt;
    QTableWidget* m_tblImports;
    QTableWidget* m_tblExports;

    // Tab 2: Perf Stress Test View
    QSpinBox* m_spnSteps;
    QComboBox* m_comboHz;
    QPushButton* m_btnRunStress;
    QLabel* m_lblPerfSummary;

    // Tab 3: Functional Verification View
    PropertyEditorWidget* m_pPropEditor;
    QTableWidget* m_tblTrajData;

    // Tab 4: HTML Report View
    QTextBrowser* m_pReportBrowser;

    // Common Charts & Console
    ChartViewerWidget* m_pChartViewer;
    LogConsoleWidget* m_pLogConsole;

    // Core Logic Objects
    DllLoader m_dllLoader;
    CombinedPrecheckReport m_latestReport;
    DualBuildPrecheckReport m_latestDualReport;
    QThread* m_pWorkerThread;
};

#endif // MAIN_WINDOW_H
