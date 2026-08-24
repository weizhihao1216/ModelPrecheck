#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QDateTime>
#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QBrush>
#include <QFile>
#include <QMetaType>
#include <chrono>

#include "ChartViewerWidget.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_pWorkerThread(nullptr) {
    qRegisterMetaType<PerfProfileReport>("PerfProfileReport");
    qRegisterMetaType<PerfSample>("PerfSample");

    setWindowTitle("第三方武器模型 DLL 集成预检工具 (Model Validator) v1.0 [VS2017 / Qt 5.11]");
    resize(1280, 850);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // --- Top Control Panel ---
    QGroupBox* grpTop = new QGroupBox("三方武器模型集成包配置 (一键选择目录)", this);
    QHBoxLayout* layoutTop = new QHBoxLayout(grpTop);

    m_editPackageDir = new QLineEdit(this);
    m_editPackageDir->setPlaceholderText("选择或输入三方模型包根目录文件夹 (自动检索 .h、Release/Debug .dll 及 .lib)...");
    m_btnBrowsePackageDir = new QPushButton("📁 浏览模型包文件夹...", this);

    m_editSearchDir = new QLineEdit(this);
    m_editSearchDir->setPlaceholderText("依赖库附加搜索目录 (可选)...");
    m_btnBrowseDir = new QPushButton("浏览目录...", this);

    m_btnRunPrecheck = new QPushButton("🚀 开始 Release + Debug 全套自动预检", this);
    m_btnRunPrecheck->setStyleSheet("QPushButton { background-color: #0066cc; color: #ffffff; font-weight: bold; font-size: 14px; padding: 6px 16px; border-radius: 4px; } QPushButton:hover { background-color: #0052a3; } QPushButton:pressed { background-color: #003d7a; }");

    m_btnExportReport = new QPushButton("📄 导出预检报告", this);

    layoutTop->addWidget(m_editPackageDir, 3);
    layoutTop->addWidget(m_btnBrowsePackageDir);
    layoutTop->addWidget(m_editSearchDir, 2);
    layoutTop->addWidget(m_btnBrowseDir);
    layoutTop->addWidget(m_btnRunPrecheck);
    layoutTop->addWidget(m_btnExportReport);

    // --- Status Indicator Bar ---
    QHBoxLayout* layoutBadges = new QHBoxLayout();
    m_lblHeaderStatus = new QLabel("头文件预检: N/A", this);
    m_lblLibStatus = new QLabel("LIB 库预检: N/A", this);
    m_lblDllStatus = new QLabel("DLL 动态库预检: N/A", this);

    QString baseBadgeStyle = "QLabel { padding: 4px 12px; border-radius: 12px; font-weight: bold; font-size: 12px; background-color: #e5eef7; color: #003986; border: 1px solid #b0c4de; }";
    m_lblHeaderStatus->setStyleSheet(baseBadgeStyle);
    m_lblLibStatus->setStyleSheet(baseBadgeStyle);
    m_lblDllStatus->setStyleSheet(baseBadgeStyle);

    layoutBadges->addWidget(m_lblHeaderStatus);
    layoutBadges->addWidget(m_lblLibStatus);
    layoutBadges->addWidget(m_lblDllStatus);
    layoutBadges->addStretch(1);

    // --- Main Splitter (Central Tabs on top, Log Console on bottom) ---
    QSplitter* splitterMain = new QSplitter(Qt::Vertical, this);
    m_pCentralTabs = new QTabWidget(this);

    // Tab 1: Static PE View
    QWidget* tabPe = new QWidget(this);
    QVBoxLayout* layoutTabPe = new QVBoxLayout(tabPe);
    QHBoxLayout* layoutPeInfo = new QHBoxLayout();
    m_lblPeArch = new QLabel("CPU 架构: N/A", this);
    m_lblPeCrt = new QLabel("CRT 链接方式: N/A", this);
    layoutPeInfo->addWidget(m_lblPeArch);
    layoutPeInfo->addWidget(m_lblPeCrt);
    layoutPeInfo->addStretch(1);

    QSplitter* splitterPeTables = new QSplitter(Qt::Horizontal, this);
    
    QGroupBox* grpImports = new QGroupBox("依赖库扫描明细 (Import Directory)", this);
    QVBoxLayout* lImp = new QVBoxLayout(grpImports);
    m_tblImports = new QTableWidget(0, 3, this);
    m_tblImports->setHorizontalHeaderLabels({ "依赖 DLL 名称", "状态", "解析路径" });
    m_tblImports->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    lImp->addWidget(m_tblImports);

    QGroupBox* grpExports = new QGroupBox("导出接口符号比对 (Export Directory)", this);
    QVBoxLayout* lExp = new QVBoxLayout(grpExports);
    m_tblExports = new QTableWidget(0, 3, this);
    m_tblExports->setHorizontalHeaderLabels({ "导出函数名", "Ordinal 序号", "匹配状态" });
    m_tblExports->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    lExp->addWidget(m_tblExports);

    splitterPeTables->addWidget(grpImports);
    splitterPeTables->addWidget(grpExports);

    layoutTabPe->addLayout(layoutPeInfo);
    layoutTabPe->addWidget(splitterPeTables, 1);

    // Tab 2: Performance Profiling
    QWidget* tabPerf = new QWidget(this);
    QVBoxLayout* layoutTabPerf = new QVBoxLayout(tabPerf);
    QHBoxLayout* layoutPerfCtrl = new QHBoxLayout();
    
    layoutPerfCtrl->addWidget(new QLabel("压测步数:", this));
    m_spnSteps = new QSpinBox(this);
    m_spnSteps->setRange(100, 100000);
    m_spnSteps->setValue(10000);
    m_spnSteps->setSingleStep(1000);
    layoutPerfCtrl->addWidget(m_spnSteps);

    layoutPerfCtrl->addWidget(new QLabel("仿真频率目标:", this));
    m_comboHz = new QComboBox(this);
    m_comboHz->addItem("50 Hz (Budget: 20ms)", 50.0);
    m_comboHz->addItem("100 Hz (Budget: 10ms)", 100.0);
    m_comboHz->addItem("1000 Hz (Budget: 1ms)", 1000.0);
    layoutPerfCtrl->addWidget(m_comboHz);

    m_btnRunStress = new QPushButton("⚡ 仅执行性能压测", this);
    layoutPerfCtrl->addWidget(m_btnRunStress);
    layoutPerfCtrl->addStretch(1);

    m_lblPerfSummary = new QLabel("压测汇总: 未执行压测", this);
    m_pChartViewer = new ChartViewerWidget(this);

    layoutTabPerf->addLayout(layoutPerfCtrl);
    layoutTabPerf->addWidget(m_lblPerfSummary);
    layoutTabPerf->addWidget(m_pChartViewer, 1);

    // Tab 3: Functional Verification & Property Editor
    QWidget* tabProp = new QWidget(this);
    QHBoxLayout* layoutTabProp = new QHBoxLayout(tabProp);
    m_pPropEditor = new PropertyEditorWidget(this);
    
    QGroupBox* grpTrajData = new QGroupBox("模型 Step 实时输出表格", this);
    QVBoxLayout* lTraj = new QVBoxLayout(grpTrajData);
    m_tblTrajData = new QTableWidget(0, 9, this);
    m_tblTrajData->setHorizontalHeaderLabels({ "SimTime(s)", "Lat(°)", "Lon(°)", "Alt(m)", "Vx(m/s)", "Vy(m/s)", "Vz(m/s)", "Pitch(°)", "Status" });
    m_tblTrajData->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    lTraj->addWidget(m_tblTrajData);

    layoutTabProp->addWidget(m_pPropEditor, 1);
    layoutTabProp->addWidget(grpTrajData, 2);

    // Tab 4: HTML Report View
    QWidget* tabReport = new QWidget(this);
    QVBoxLayout* layoutTabReport = new QVBoxLayout(tabReport);
    m_pReportBrowser = new QTextBrowser(this);
    layoutTabReport->addWidget(m_pReportBrowser);

    // Add Tabs
    m_pCentralTabs->addTab(tabPe, "1. 静态 PE 结构与依赖分析");
    m_pCentralTabs->addTab(tabPerf, "2. 动态加载与性能压测");
    m_pCentralTabs->addTab(tabProp, "3. 参数配置与轨迹校验");
    m_pCentralTabs->addTab(tabReport, "4. 预检报告预览");

    // Log Console at bottom
    m_pLogConsole = new LogConsoleWidget(this);

    splitterMain->addWidget(m_pCentralTabs);
    splitterMain->addWidget(m_pLogConsole);
    splitterMain->setStretchFactor(0, 3);
    splitterMain->setStretchFactor(1, 1);

    rootLayout->addWidget(grpTop);
    rootLayout->addLayout(layoutBadges);
    rootLayout->addWidget(splitterMain, 1);

    // --- Connect Signals ---
    connect(m_btnBrowsePackageDir, &QPushButton::clicked, this, &MainWindow::browsePackageDir);
    connect(m_btnBrowseDir, &QPushButton::clicked, this, &MainWindow::browseSearchDir);
    connect(m_btnRunPrecheck, &QPushButton::clicked, this, &MainWindow::runFullPrecheck);
    connect(m_btnRunStress, &QPushButton::clicked, this, &MainWindow::runStressTestOnly);
    connect(m_btnExportReport, &QPushButton::clicked, this, &MainWindow::exportReport);

    applyDarkStyle();
    logMessage("INFO: 武器模型 DLL 集成预检工具初始化完毕。请选择目标 DLL 并点击【开始全套自动化预检】。");
}

MainWindow::~MainWindow() {
    if (m_pWorkerThread) {
        m_pWorkerThread->quit();
        m_pWorkerThread->wait();
    }
}

void MainWindow::applyDarkStyle() {
    qApp->setStyle("Fusion");
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor("#1e1e2e"));
    darkPalette.setColor(QPalette::WindowText, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::Base, QColor("#181825"));
    darkPalette.setColor(QPalette::AlternateBase, QColor("#313244"));
    darkPalette.setColor(QPalette::ToolTipBase, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::ToolTipText, QColor("#11111b"));
    darkPalette.setColor(QPalette::Text, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::Button, QColor("#313244"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#cdd6f4"));
    darkPalette.setColor(QPalette::BrightText, QColor("#f38ba8"));
    darkPalette.setColor(QPalette::Highlight, QColor("#89b4fa"));
    darkPalette.setColor(QPalette::HighlightedText, QColor("#11111b"));
    qApp->setPalette(darkPalette);
}

void MainWindow::browsePackageDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择三方武器模型包根目录文件夹");
    if (!dir.isEmpty()) {
        m_editPackageDir->setText(dir);
    }
}

void MainWindow::browseSearchDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择依赖库附加搜索目录");
    if (!dir.isEmpty()) {
        m_editSearchDir->setText(dir);
    }
}

void MainWindow::logMessage(const QString& msg) {
    m_pLogConsole->appendLog(msg);
}

CombinedPrecheckReport MainWindow::runBuildPrecheck(const std::string& dllPath, const std::string& headerPath, const std::string& libPath, const std::string& buildConfig) {
    CombinedPrecheckReport report;
    report.dllPath = dllPath;
    report.headerPath = headerPath;
    report.libPath = libPath;
    report.buildConfig = buildConfig;
    report.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();

    std::vector<std::string> searchPaths;
    QString extraDir = m_editSearchDir->text().trimmed();
    if (!extraDir.isEmpty()) {
        searchPaths.push_back(extraDir.toStdString());
    }

    InterfaceMapping mapping = m_pPropEditor->GetInterfaceMapping();
    std::vector<std::string> reqExports = {};

    // 1. Static PE Analysis
    report.peReport = PeAnalyzer::AnalyzeDll(dllPath, searchPaths, reqExports);

    std::vector<std::string> peExports;
    for (const auto& sym : report.peReport.exportedSymbols) {
        peExports.push_back(sym.name);
    }

    // 2. LIB Analysis
    if (!libPath.empty() && QFile::exists(QString::fromStdString(libPath))) {
        report.libReport = LibAnalyzer::AnalyzeLib(libPath, reqExports);
    }

    // 3. Header & Export Consistency Verification
    if (!headerPath.empty() && QFile::exists(QString::fromStdString(headerPath))) {
        report.headerReport = HeaderAnalyzer::AnalyzeHeader(headerPath);
        report.consistencyReport = HeaderAnalyzer::VerifyConsistency(report.headerReport.declaredFunctions, peExports);
    }

    // 3. Dynamic Load & SEH
    report.loadReport = m_dllLoader.Load(dllPath, mapping);

    if (report.loadReport.isLoaded) {
        WeaponModelParams modelParams = m_pPropEditor->GetModelParams();

        int initRes = 0;
        std::string errStr;
        m_dllLoader.CallInit(modelParams, initRes, errStr);

        std::vector<WeaponModelOutput> history;
        std::vector<double> stepTimesMs;
        for (int i = 0; i < 1000; ++i) {
            auto tStart = std::chrono::high_resolution_clock::now();
            WeaponModelOutput outData;
            int stepRes = 0;
            if (m_dllLoader.CallStep(outData, stepRes, errStr)) {
                auto tEnd = std::chrono::high_resolution_clock::now();
                double tMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
                stepTimesMs.push_back(tMs);
                if (i < 100) history.push_back(outData);
            } else {
                break;
            }
        }

        report.trajReport = FunctionalVerifier::VerifyTrajectory(history);

        if (!stepTimesMs.empty()) {
            report.perfReport.totalSteps = (int)stepTimesMs.size();
            report.perfReport.completedSteps = (int)stepTimesMs.size();
            double sumT = 0.0;
            double maxT = 0.0;
            for (double t : stepTimesMs) {
                sumT += t;
                if (t > maxT) maxT = t;
            }
            report.perfReport.avgTimeMs = sumT / stepTimesMs.size();
            report.perfReport.maxTimeMs = maxT;
            report.perfReport.realtimeVerdict = (report.perfReport.avgTimeMs < 20.0) ? "PASS" : "WARNING";
        }

        m_dllLoader.Unload();
    }

    report.overallPass = report.peReport.overallPass && report.loadReport.isLoaded &&
                         (report.perfReport.realtimeVerdict == "PASS" || report.perfReport.realtimeVerdict == "WARNING") &&
                         report.trajReport.overallPass;
    return report;
}

void MainWindow::runFullPrecheck() {
    QString pkgDir = m_editPackageDir->text().trimmed();
    if (pkgDir.isEmpty() || !QDir(pkgDir).exists()) {
        QMessageBox::warning(this, "警告", "请先选择有效的模型包根目录文件夹！");
        return;
    }

    logMessage("================================================================================");
    logMessage("INFO: 启动三方模型包全覆盖批量自动化预验流程: " + pkgDir);

    ModelPackageFiles pkgFiles = PackageScanner::ScanPackageDirectory(pkgDir.toStdString());
    for (const auto& msg : pkgFiles.scanLog) {
        logMessage(QString::fromStdString(msg));
    }

    m_latestDualReport = DualBuildPrecheckReport();
    m_latestDualReport.packageDir = pkgDir.toStdString();
    m_latestDualReport.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    m_latestDualReport.packageFiles = pkgFiles;

    std::vector<std::string> combinedHeaderFuncs;
    std::vector<std::string> combinedBinaryExports;

    // 1. Pre-check ALL Header Files
    logMessage("--------------------------------------------------------------------------------");
    logMessage("INFO: 阶段 1/3: 开始对所有 C/C++ 头文件 (" + QString::number(pkgFiles.allHeaderFiles.size()) + " 个) 进行批量语法与规范预检...");
    for (const auto& hPath : pkgFiles.allHeaderFiles) {
        logMessage(" -> 预检头文件: " + QString::fromStdString(hPath));
        HeaderAnalysisReport hRep = HeaderAnalyzer::AnalyzeHeader(hPath);
        for (const auto& msg : hRep.logMessages) {
            logMessage(QString::fromStdString(msg));
        }
        if (hRep.overallPass) {
            m_latestDualReport.passedHeaderCount++;
        }
        m_latestDualReport.headerReports.push_back(hRep);
        combinedHeaderFuncs.insert(combinedHeaderFuncs.end(), hRep.declaredFunctions.begin(), hRep.declaredFunctions.end());
    }

    // 2. Pre-check ALL LIB Files
    logMessage("--------------------------------------------------------------------------------");
    logMessage("INFO: 阶段 2/3: 开始对所有 LIB 库 (" + QString::number(pkgFiles.allLibFiles.size()) + " 个) 进行批量 COFF 架构与符号预检...");
    for (const auto& lPath : pkgFiles.allLibFiles) {
        logMessage(" -> 预检 LIB 库: " + QString::fromStdString(lPath));
        LibAnalysisReport lRep = LibAnalyzer::AnalyzeLib(lPath, {});
        for (const auto& msg : lRep.logMessages) {
            logMessage(QString::fromStdString(msg));
        }
        if (lRep.overallPass) {
            m_latestDualReport.passedLibCount++;
        }
        m_latestDualReport.libReports.push_back(lRep);
        combinedBinaryExports.insert(combinedBinaryExports.end(), lRep.foundSymbols.begin(), lRep.foundSymbols.end());
    }

    // 3. Pre-check ALL DLL Files
    logMessage("--------------------------------------------------------------------------------");
    logMessage("INFO: 阶段 3/3: 开始对所有 DLL 动态库 (" + QString::number(pkgFiles.allDllFiles.size()) + " 个) 进行批量 PE 结构、SEH 加载与推演预检...");
    for (const auto& dPath : pkgFiles.allDllFiles) {
        QFileInfo fi(QString::fromStdString(dPath));
        QString lowerPath = QString::fromStdString(dPath).toLower();
        QString configStr = "Release";
        if (lowerPath.contains("/debug/") || lowerPath.contains("\\debug\\") || fi.completeBaseName().endsWith("d", Qt::CaseInsensitive)) {
            configStr = "Debug";
        }

        logMessage(" -> 预检 DLL 动态库 [" + configStr + "]: " + QString::fromStdString(dPath));
        CombinedPrecheckReport dRep = runBuildPrecheck(dPath, "", "", configStr.toStdString());
        if (dRep.overallPass) {
            m_latestDualReport.passedDllCount++;
        }
        m_latestDualReport.dllReports.push_back(dRep);

        for (const auto& sym : dRep.peReport.exportedSymbols) {
            combinedBinaryExports.push_back(sym.name);
        }

        m_latestReport = dRep; // single view backup
    }

    // 4. Interface Consistency Verification
    m_latestDualReport.consistencyReport = HeaderAnalyzer::VerifyConsistency(combinedHeaderFuncs, combinedBinaryExports);
    for (const auto& msg : m_latestDualReport.consistencyReport.logMessages) {
        logMessage(QString::fromStdString(msg));
    }

    bool headersPass = pkgFiles.allHeaderFiles.empty() || (m_latestDualReport.passedHeaderCount == (int)pkgFiles.allHeaderFiles.size());
    bool libsPass = pkgFiles.allLibFiles.empty() || (m_latestDualReport.passedLibCount == (int)pkgFiles.allLibFiles.size());
    bool dllsPass = pkgFiles.allDllFiles.empty() || (m_latestDualReport.passedDllCount == (int)pkgFiles.allDllFiles.size());

    m_latestDualReport.overallPass = headersPass && libsPass && dllsPass;

    updateStatusBadges();

    // Render HTML Report
    std::string html = ReportGenerator::GenerateDualBuildHtml(m_latestDualReport);
    m_pReportBrowser->setHtml(QString::fromStdString(html));

    logMessage("SUCCESS: 模型包全覆盖批量预检完毕！所有头文件、LIB 库及 DLL 库均已完成检测并实时生成总报告。");
}

void MainWindow::runStressTestOnly() {
    if (!m_dllLoader.IsLoaded()) {
        logMessage("ERROR: 请先加载 DLL 或运行全套预检！");
        return;
    }

    m_pCentralTabs->setCurrentIndex(1);

    int steps = m_spnSteps->value();
    double targetHz = m_comboHz->currentData().toDouble();
    double frameBudgetMs = 1000.0 / (targetHz > 0 ? targetHz : 50.0);

    m_pChartViewer->PrepareLiveProfiling(steps, frameBudgetMs);

    logMessage(QString("INFO: 启动性能压力测试 (总步数: %1, 目标频率: %2 Hz)...").arg(steps).arg(targetHz));

    if (m_pWorkerThread) {
        m_pWorkerThread->quit();
        m_pWorkerThread->wait();
        delete m_pWorkerThread;
        m_pWorkerThread = nullptr;
    }

    m_pWorkerThread = new QThread();
    PerfProfilerWorker* worker = new PerfProfilerWorker(&m_dllLoader, m_pPropEditor->GetModelParams(), steps, targetHz);
    worker->moveToThread(m_pWorkerThread);

    connect(m_pWorkerThread, &QThread::started, worker, &PerfProfilerWorker::process);
    connect(worker, &PerfProfilerWorker::progressUpdated, this, &MainWindow::onPerfProfileProgress);
    connect(worker, &PerfProfilerWorker::sampleAdded, m_pChartViewer, &ChartViewerWidget::AddLiveSample);
    connect(worker, &PerfProfilerWorker::finished, this, &MainWindow::onPerfProfileFinished);
    connect(worker, &PerfProfilerWorker::logMessage, this, &MainWindow::logMessage);
    connect(worker, &PerfProfilerWorker::finished, m_pWorkerThread, &QThread::quit);
    connect(worker, &PerfProfilerWorker::finished, worker, &QObject::deleteLater);

    m_pWorkerThread->start();
}

void MainWindow::onPerfProfileProgress(int step, int total, double timeMs, double memMB) {
    if (step % 2000 == 0 || step == total) {
        logMessage(QString("INFO: 压测进度: %1/%2 步 | 单步耗时: %3 ms | WorkingSet: %4 MB")
            .arg(step).arg(total).arg(timeMs, 0, 'f', 4).arg(memMB, 0, 'f', 2));
    }
}

void MainWindow::onPerfProfileFinished(const PerfProfileReport& report) {
    m_latestReport.perfReport = report;
    m_pChartViewer->UpdatePerfCharts(report);
    m_pCentralTabs->setCurrentIndex(1);

    QString summary = QString("压测结果: 平均耗时: %1 ms | 最大耗时: %2 ms | 抖动 StdDev: %3 ms | 10k步内存增量: %4 MB | 实时性判定: <b>%5</b>")
        .arg(report.avgTimeMs, 0, 'f', 4)
        .arg(report.maxTimeMs, 0, 'f', 4)
        .arg(report.jitterMs, 0, 'f', 4)
        .arg(report.memoryLeakRateMBPer10k, 0, 'f', 2)
        .arg(QString::fromStdString(report.realtimeVerdict));

    m_lblPerfSummary->setText(summary);

    m_latestReport.overallPass = (m_latestReport.peReport.overallPass &&
                                  m_latestReport.loadReport.isLoaded &&
                                  m_latestReport.perfReport.realtimeVerdict != "FAIL" &&
                                  m_latestReport.trajReport.overallPass);

    updateStatusBadges();

    // Render HTML report into preview tab
    std::string html = ReportGenerator::GenerateHtml(m_latestReport);
    m_pReportBrowser->setHtml(QString::fromStdString(html));

    logMessage("SUCCESS: 预检流程执行完毕！已实时生成预检报告，可在【4. 预检报告预览】中查看或一键导出。");
}

void MainWindow::updateStatusBadges() {
    auto setBadge = [](QLabel* lbl, const QString& prefix, const QString& status, const QString& bgColor, const QString& fgColor, const QString& borderColor) {
        lbl->setText(prefix + ": " + status);
        lbl->setStyleSheet(QString("QLabel { padding: 4px 12px; border-radius: 12px; font-weight: bold; font-size: 12px; background-color: %1; color: %2; border: 1px solid %3; }").arg(bgColor, fgColor, borderColor));
    };

    // Header Status
    int totalH = (int)m_latestDualReport.headerReports.size();
    if (totalH > 0) {
        QString statusStr = QString("%1 (%2/%3)").arg(m_latestDualReport.passedHeaderCount == totalH ? "PASS" : "FAIL").arg(m_latestDualReport.passedHeaderCount).arg(totalH);
        if (m_latestDualReport.passedHeaderCount == totalH) {
            setBadge(m_lblHeaderStatus, "头文件预检", statusStr, "#dcfce7", "#166534", "#86efac");
        } else {
            setBadge(m_lblHeaderStatus, "头文件预检", statusStr, "#fee2e2", "#991b1b", "#fca5a5");
        }
    } else {
        setBadge(m_lblHeaderStatus, "头文件预检", "N/A", "#e5eef7", "#003986", "#b0c4de");
    }

    // LIB Status
    int totalL = (int)m_latestDualReport.libReports.size();
    if (totalL > 0) {
        QString statusStr = QString("%1 (%2/%3)").arg(m_latestDualReport.passedLibCount == totalL ? "PASS" : "FAIL").arg(m_latestDualReport.passedLibCount).arg(totalL);
        if (m_latestDualReport.passedLibCount == totalL) {
            setBadge(m_lblLibStatus, "LIB 库预检", statusStr, "#dcfce7", "#166534", "#86efac");
        } else {
            setBadge(m_lblLibStatus, "LIB 库预检", statusStr, "#fee2e2", "#991b1b", "#fca5a5");
        }
    } else {
        setBadge(m_lblLibStatus, "LIB 库预检", "N/A", "#e5eef7", "#003986", "#b0c4de");
    }

    // DLL Status
    int totalD = (int)m_latestDualReport.dllReports.size();
    if (totalD > 0) {
        QString statusStr = QString("%1 (%2/%3)").arg(m_latestDualReport.passedDllCount == totalD ? "PASS" : "FAIL").arg(m_latestDualReport.passedDllCount).arg(totalD);
        if (m_latestDualReport.passedDllCount == totalD) {
            setBadge(m_lblDllStatus, "DLL 动态库预检", statusStr, "#dcfce7", "#166534", "#86efac");
        } else {
            setBadge(m_lblDllStatus, "DLL 动态库预检", statusStr, "#fee2e2", "#991b1b", "#fca5a5");
        }
    } else {
        setBadge(m_lblDllStatus, "DLL 动态库预检", "N/A", "#e5eef7", "#003986", "#b0c4de");
    }
}

void MainWindow::exportReport() {
    if (m_latestDualReport.packageDir.empty() && m_latestReport.dllPath.empty()) {
        QMessageBox::warning(this, "警告", "请先运行预检流程后再导出报告！");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "保存预检报告 HTML", "Precheck_Report.html", "HTML Files (*.html)");
    if (!savePath.isEmpty()) {
        bool success = false;
        if (!m_latestDualReport.packageDir.empty()) {
            success = ReportGenerator::SaveDualReportToFile(m_latestDualReport, savePath.toStdString());
        } else {
            success = ReportGenerator::SaveReportToFile(m_latestReport, savePath.toStdString());
        }

        if (success) {
            QMessageBox::information(this, "成功", "预检报告已成功导出至:\n" + savePath);
            logMessage("SUCCESS: 报告已成功导出: " + savePath);
        } else {
            QMessageBox::critical(this, "错误", "导出预检报告失败！");
        }
    }
}
