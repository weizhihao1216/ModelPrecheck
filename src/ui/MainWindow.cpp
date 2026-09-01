#include "MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QDateTime>
#include <QApplication>
#include <QCoreApplication>
#include <QPalette>
#include <QColor>
#include <QBrush>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QMetaType>
#include <QFont>
#include <QSignalBlocker>
#include <QAbstractItemView>
#include <QSizePolicy>

#include "ChartViewerWidget.h"
#include "TrajectoryViewWidget.h"
#include "../utils/QtEncoding.h"

#include <QProgressDialog>
#include <QEventLoop>

namespace {

void FitButtonText(QPushButton* btn) {
    if (!btn) return;
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    btn->setMinimumHeight(28);
    const int w = btn->fontMetrics().boundingRect(btn->text()).width() + 28;
    btn->setMinimumWidth(qMax(w, 72));
}

/** Modal busy dialog + wait cursor while harness compiles (blocks UI). */
class CompileWaitIndicator {
public:
    CompileWaitIndicator(QWidget* parent, const QString& text)
        : m_dlg(text, QString(), 0, 0, parent) {
        m_dlg.setWindowTitle(QStringLiteral("编译中"));
        m_dlg.setWindowModality(Qt::ApplicationModal);
        m_dlg.setCancelButton(nullptr);
        m_dlg.setMinimumDuration(0);
        m_dlg.setAutoClose(false);
        m_dlg.setAutoReset(false);
        m_dlg.show();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    ~CompileWaitIndicator() {
        QApplication::restoreOverrideCursor();
        m_dlg.hide();
    }

    void setText(const QString& text) {
        m_dlg.setLabelText(text);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

private:
    QProgressDialog m_dlg;
};

QString SanitizeModelFolderName(const QString& name) {
    QString s = name.trimmed();
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == '.') {
            s[i] = QLatin1Char('_');
        }
    }
    while (s.startsWith(QLatin1Char(' '))) s.remove(0, 1);
    if (s.isEmpty()) s = QStringLiteral("model");
    return s;
}

QString ExeTestModelRoot() {
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("TestModel")));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_pWorkerThread(nullptr) {
    qRegisterMetaType<PerfProfileReport>("PerfProfileReport");
    qRegisterMetaType<PerfSample>("PerfSample");
    qRegisterMetaType<ConcurrencyTestReport>("ConcurrencyTestReport");

    setWindowTitle("第三方武器模型 DLL 集成预检工具 (Model Validator) v1.1");
    resize(1440, 900);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // --- Top Control Panel ---
    QGroupBox* grpTop = new QGroupBox("预检控制", this);
    QHBoxLayout* layoutTop = new QHBoxLayout(grpTop);

    m_btnRunPrecheck = new QPushButton("一键预检全部型号", this);
    m_btnRunPrecheck->setStyleSheet(
        "QPushButton { background-color: #0066cc; color: #ffffff; font-weight: bold; font-size: 13px; "
        "padding: 8px 18px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #0052a3; } "
        "QPushButton:pressed { background-color: #003d7a; }");
    FitButtonText(m_btnRunPrecheck);

    m_btnExportReport = new QPushButton("导出预检报告", this);
    FitButtonText(m_btnExportReport);

    layoutTop->addWidget(m_btnRunPrecheck);
    layoutTop->addWidget(m_btnExportReport);
    layoutTop->addStretch(1);

    // --- Status Indicator Bar ---
    QHBoxLayout* layoutBadges = new QHBoxLayout();
    m_lblHeaderStatus = new QLabel("头文件预检: N/A", this);
    m_lblLibStatus = new QLabel("LIB 库预检: N/A", this);
    m_lblDllStatus = new QLabel("DLL 动态库预检: N/A", this);

    QString baseBadgeStyle =
        "QLabel { padding: 4px 14px; border-radius: 12px; font-weight: bold; font-size: 12px; "
        "background-color: #e5eef7; color: #003986; border: 1px solid #b0c4de; }";
    m_lblHeaderStatus->setStyleSheet(baseBadgeStyle);
    m_lblLibStatus->setStyleSheet(baseBadgeStyle);
    m_lblDllStatus->setStyleSheet(baseBadgeStyle);
    m_lblHeaderStatus->setMinimumWidth(140);
    m_lblLibStatus->setMinimumWidth(140);
    m_lblDllStatus->setMinimumWidth(160);

    layoutBadges->addWidget(m_lblHeaderStatus);
    layoutBadges->addWidget(m_lblLibStatus);
    layoutBadges->addWidget(m_lblDllStatus);
    layoutBadges->addStretch(1);

    // --- Content: left models panel + right tabs ---
    QSplitter* splitterContent = new QSplitter(Qt::Horizontal, this);

    // ========== Left: 型号与 UserMain ==========
    QGroupBox* grpModels = new QGroupBox("型号与 UserMain", this);
    QHBoxLayout* layoutTabModels = new QHBoxLayout(grpModels);

    QWidget* leftPanel = new QWidget(grpModels);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel("型号列表:", this));
    m_listModels = new QListWidget(this);
    m_listModels->setMinimumWidth(160);
    leftLayout->addWidget(m_listModels, 1);
    QHBoxLayout* modelBtnRow = new QHBoxLayout();
    m_btnAddModel = new QPushButton("添加型号", this);
    m_btnRemoveModel = new QPushButton("删除", this);
    FitButtonText(m_btnAddModel);
    FitButtonText(m_btnRemoveModel);
    modelBtnRow->addWidget(m_btnAddModel);
    modelBtnRow->addWidget(m_btnRemoveModel);
    leftLayout->addLayout(modelBtnRow);

    QWidget* rightPanel = new QWidget(grpModels);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QFormLayout* formModel = new QFormLayout();
    formModel->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_editModelName = new QLineEdit(this);
    m_editModelPackage = new QLineEdit(this);
    m_editModelPackage->setPlaceholderText("选择该型号的模型包根目录...");
    m_btnBrowseModelPackage = new QPushButton("浏览...", this);
    FitButtonText(m_btnBrowseModelPackage);
    QHBoxLayout* pkgRow = new QHBoxLayout();
    pkgRow->addWidget(m_editModelPackage, 1);
    pkgRow->addWidget(m_btnBrowseModelPackage);
    formModel->addRow("型号名称:", m_editModelName);
    formModel->addRow("模型包路径:", pkgRow);
    m_lblLicenseHint = new QLabel(
        QStringLiteral("提示：授权文件或文件夹请放在第三方模型 dll 同级目录，如不可用可复制一份放在本 exe 同级目录下。"),
        this);
    m_lblLicenseHint->setWordWrap(true);
    m_lblLicenseHint->setStyleSheet("color: #006633; font-size: 11px;");
    rightLayout->addLayout(formModel);
    rightLayout->addWidget(m_lblLicenseHint);

    QHBoxLayout* hdrPickRow = new QHBoxLayout();
    hdrPickRow->addWidget(new QLabel("包含头文件(可多选):", this));
    m_btnRefreshModelHeaders = new QPushButton("刷新列表", this);
    FitButtonText(m_btnRefreshModelHeaders);
    hdrPickRow->addWidget(m_btnRefreshModelHeaders);
    hdrPickRow->addStretch(1);
    rightLayout->addLayout(hdrPickRow);

    m_listHarnessHeaders = new QListWidget(this);
    m_listHarnessHeaders->setSelectionMode(QAbstractItemView::NoSelection);
    m_listHarnessHeaders->setMinimumHeight(100);
    m_listHarnessHeaders->setMaximumHeight(160);
    rightLayout->addWidget(m_listHarnessHeaders);

    rightLayout->addWidget(new QLabel("UserMain 函数体:", this));
    m_editUserMain = new QPlainTextEdit(this);
    m_editUserMain->setMinimumHeight(140);
    QFont mono("Consolas", 10);
    m_editUserMain->setFont(mono);
    rightLayout->addWidget(m_editUserMain, 2);

    QHBoxLayout* rndTitle = new QHBoxLayout();
    rndTitle->addWidget(new QLabel("随机变量 (R.变量名):", this));
    m_btnAddRandomVar = new QPushButton("添加变量", this);
    m_btnRemoveRandomVar = new QPushButton("删除选中", this);
    FitButtonText(m_btnAddRandomVar);
    FitButtonText(m_btnRemoveRandomVar);
    rndTitle->addStretch(1);
    rndTitle->addWidget(m_btnAddRandomVar);
    rndTitle->addWidget(m_btnRemoveRandomVar);
    rightLayout->addLayout(rndTitle);

    m_tblRandomVars = new QTableWidget(0, 5, this);
    m_tblRandomVars->setHorizontalHeaderLabels({ "启用", "变量名", "类型", "最小值", "最大值" });
    m_tblRandomVars->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblRandomVars->setMinimumHeight(100);
    m_tblRandomVars->setMaximumHeight(150);
    rightLayout->addWidget(m_tblRandomVars);

    QHBoxLayout* layoutCompile = new QHBoxLayout();
    m_btnCompileCurrent = new QPushButton("编译当前型号", this);
    m_btnCompileAll = new QPushButton("编译全部型号", this);
    FitButtonText(m_btnCompileCurrent);
    FitButtonText(m_btnCompileAll);
    layoutCompile->addWidget(m_btnCompileCurrent);
    layoutCompile->addWidget(m_btnCompileAll);
    layoutCompile->addStretch(1);
    rightLayout->addLayout(layoutCompile);

    m_lblHarnessStatus = new QLabel("Harness: 未编译", this);
    m_lblHarnessStatus->setWordWrap(true);
    rightLayout->addWidget(m_lblHarnessStatus);

    layoutTabModels->addWidget(leftPanel);
    layoutTabModels->addWidget(rightPanel, 1);
    grpModels->setMinimumWidth(420);

    // ========== Right tabs ==========
    m_pCentralTabs = new QTabWidget(this);

    // Tab 0: Static PE View (always present; multi-model: pick 型号 then DLL)
    QWidget* tabPe = new QWidget(this);
    QVBoxLayout* layoutTabPe = new QVBoxLayout(tabPe);
    QHBoxLayout* layoutPePick = new QHBoxLayout();
    layoutPePick->addWidget(new QLabel("型号:", this));
    m_comboPeModel = new QComboBox(this);
    m_comboPeModel->setMinimumWidth(160);
    m_comboPeModel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layoutPePick->addWidget(m_comboPeModel);
    layoutPePick->addWidget(new QLabel("DLL:", this));
    m_comboPeDll = new QComboBox(this);
    m_comboPeDll->setMinimumWidth(280);
    m_comboPeDll->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layoutPePick->addWidget(m_comboPeDll, 1);
    layoutTabPe->addLayout(layoutPePick);

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

    // Tab 1: Stress
    QWidget* tabPerf = new QWidget(this);
    QVBoxLayout* layoutTabPerf = new QVBoxLayout(tabPerf);
    QHBoxLayout* layoutPerfCtrl = new QHBoxLayout();

    layoutPerfCtrl->addWidget(new QLabel("型号:", this));
    m_comboStressModel = new QComboBox(this);
    m_comboStressModel->setMinimumWidth(180);
    m_comboStressModel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layoutPerfCtrl->addWidget(m_comboStressModel);

    layoutPerfCtrl->addWidget(new QLabel("重复次数:", this));
    m_spnSteps = new QSpinBox(this);
    m_spnSteps->setRange(100, 100000);
    m_spnSteps->setValue(10000);
    m_spnSteps->setSingleStep(1000);
    m_spnSteps->setMinimumWidth(90);
    layoutPerfCtrl->addWidget(m_spnSteps);

    layoutPerfCtrl->addWidget(new QLabel("频率目标:", this));
    m_comboHz = new QComboBox(this);
    m_comboHz->addItem("50 Hz (Budget: 20ms)", 50.0);
    m_comboHz->addItem("100 Hz (Budget: 10ms)", 100.0);
    m_comboHz->addItem("1000 Hz (Budget: 1ms)", 1000.0);
    m_comboHz->setMinimumWidth(180);
    layoutPerfCtrl->addWidget(m_comboHz);

    m_btnRunStress = new QPushButton("执行性能压测", this);
    FitButtonText(m_btnRunStress);
    layoutPerfCtrl->addWidget(m_btnRunStress);
    m_btnRunTrajectory = new QPushButton("试跑并绘制轨迹", this);
    FitButtonText(m_btnRunTrajectory);
    layoutPerfCtrl->addWidget(m_btnRunTrajectory);
    layoutPerfCtrl->addStretch(1);

    m_lblPerfSummary = new QLabel("压测汇总: 未执行压测", this);
    m_lblPerfSummary->setWordWrap(true);
    m_lblTrajOut = new QLabel(
        QStringLiteral("轨迹输出: out_lat / out_lon = (未试跑)"), this);
    m_lblTrajOut->setWordWrap(true);

    m_pChartViewer = new ChartViewerWidget(this);
    m_pTrajectoryView = new TrajectoryViewWidget(this);
    m_pTrajectoryView->setMinimumHeight(220);
    m_pTrajectoryView->setMinimumWidth(280);

    m_tblTrajectoryPoints = new QTableWidget(0, 3, this);
    m_tblTrajectoryPoints->setHorizontalHeaderLabels({ "序号", "纬度 Lat", "经度 Lon" });
    m_tblTrajectoryPoints->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblTrajectoryPoints->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblTrajectoryPoints->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tblTrajectoryPoints->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tblTrajectoryPoints->setMinimumWidth(220);
    m_tblTrajectoryPoints->setAlternatingRowColors(true);

    QSplitter* splitterPerf = new QSplitter(Qt::Vertical, this);
    splitterPerf->addWidget(m_pChartViewer);
    QWidget* trajPanel = new QWidget(this);
    QVBoxLayout* trajLayout = new QVBoxLayout(trajPanel);
    trajLayout->setContentsMargins(0, 0, 0, 0);
    trajLayout->addWidget(m_lblTrajOut);
    QSplitter* splitterTraj = new QSplitter(Qt::Horizontal, trajPanel);
    splitterTraj->addWidget(m_pTrajectoryView);
    QWidget* tblWrap = new QWidget(trajPanel);
    QVBoxLayout* tblLayout = new QVBoxLayout(tblWrap);
    tblLayout->setContentsMargins(0, 0, 0, 0);
    tblLayout->addWidget(new QLabel(QStringLiteral("路径点经纬度"), tblWrap));
    tblLayout->addWidget(m_tblTrajectoryPoints, 1);
    splitterTraj->addWidget(tblWrap);
    splitterTraj->setStretchFactor(0, 3);
    splitterTraj->setStretchFactor(1, 2);
    trajLayout->addWidget(splitterTraj, 1);
    splitterPerf->addWidget(trajPanel);
    splitterPerf->setStretchFactor(0, 3);
    splitterPerf->setStretchFactor(1, 2);

    layoutTabPerf->addLayout(layoutPerfCtrl);
    layoutTabPerf->addWidget(m_lblPerfSummary);
    layoutTabPerf->addWidget(splitterPerf, 1);

    // Tab 2: Multi-model
    QWidget* tabMultiModel = new QWidget(this);
    QVBoxLayout* layoutTabMultiModel = new QVBoxLayout(tabMultiModel);
    QLabel* multiHint = new QLabel(
        "在左侧配置并编译各型号后，在此设置并行实例数并一起跑。", this);
    multiHint->setWordWrap(true);
    layoutTabMultiModel->addWidget(multiHint);

    m_tblFleetCounts = new QTableWidget(0, 4, this);
    m_tblFleetCounts->setHorizontalHeaderLabels({ "名称", "模型包路径", "编译状态", "实例数" });
    m_tblFleetCounts->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblFleetCounts->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblFleetCounts->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tblFleetCounts->setMinimumHeight(160);
    layoutTabMultiModel->addWidget(m_tblFleetCounts);

    QHBoxLayout* layoutFleetRun = new QHBoxLayout();
    m_btnRunMultiModel = new QPushButton("并行测试（一起跑）", this);
    FitButtonText(m_btnRunMultiModel);
    layoutFleetRun->addWidget(m_btnRunMultiModel);
    layoutFleetRun->addStretch(1);
    layoutTabMultiModel->addLayout(layoutFleetRun);

    m_lblMultiModelSummary = new QLabel("多型号并行汇总: 未执行", this);
    m_lblMultiModelSummary->setWordWrap(true);
    m_tblMultiModelResults = new QTableWidget(0, 5, this);
    m_tblMultiModelResults->setHorizontalHeaderLabels({ "型号[实例]", "随机参数", "返回码", "SEH", "详情" });
    m_tblMultiModelResults->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layoutTabMultiModel->addWidget(m_lblMultiModelSummary);
    layoutTabMultiModel->addWidget(m_tblMultiModelResults, 1);

    // Tab 3: Multi-thread
    QWidget* tabMultiThr = new QWidget(this);
    QVBoxLayout* layoutTabMultiThr = new QVBoxLayout(tabMultiThr);
    QHBoxLayout* layoutThrCtrl = new QHBoxLayout();

    layoutThrCtrl->addWidget(new QLabel("型号:", this));
    m_comboThreadModel = new QComboBox(this);
    m_comboThreadModel->setMinimumWidth(180);
    m_comboThreadModel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layoutThrCtrl->addWidget(m_comboThreadModel);

    layoutThrCtrl->addWidget(new QLabel("线程数量:", this));
    m_spnThreadCount = new QSpinBox(this);
    m_spnThreadCount->setRange(1, 64);
    m_spnThreadCount->setValue(4);
    m_spnThreadCount->setMinimumWidth(70);
    layoutThrCtrl->addWidget(m_spnThreadCount);
    m_btnRunMultiThread = new QPushButton("执行多线程测试", this);
    FitButtonText(m_btnRunMultiThread);
    layoutThrCtrl->addWidget(m_btnRunMultiThread);
    layoutThrCtrl->addStretch(1);
    layoutTabMultiThr->addLayout(layoutThrCtrl);

    m_lblMultiThreadSummary = new QLabel("多线程测试汇总: 未执行", this);
    m_lblMultiThreadSummary->setWordWrap(true);
    m_tblMultiThreadResults = new QTableWidget(0, 5, this);
    m_tblMultiThreadResults->setHorizontalHeaderLabels({ "Thread", "随机参数", "返回码", "SEH", "详情" });
    m_tblMultiThreadResults->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layoutTabMultiThr->addWidget(m_lblMultiThreadSummary);
    layoutTabMultiThr->addWidget(m_tblMultiThreadResults, 1);

    // Tab 4: Report
    QWidget* tabReport = new QWidget(this);
    QVBoxLayout* layoutTabReport = new QVBoxLayout(tabReport);
    m_pReportBrowser = new QTextBrowser(this);
    layoutTabReport->addWidget(m_pReportBrowser);

    m_pCentralTabs->addTab(tabPe, "静态 PE 分析");
    m_pCentralTabs->addTab(tabPerf, "性能压测");
    m_pCentralTabs->addTab(tabMultiModel, "多型号并行");
    m_pCentralTabs->addTab(tabMultiThr, "多线程测试");
    m_pCentralTabs->addTab(tabReport, "预检报告");

    splitterContent->addWidget(grpModels);
    splitterContent->addWidget(m_pCentralTabs);
    splitterContent->setStretchFactor(0, 2);
    splitterContent->setStretchFactor(1, 3);
    splitterContent->setSizes({ 520, 780 });

    m_pLogConsole = new LogConsoleWidget(this);

    QSplitter* splitterMain = new QSplitter(Qt::Vertical, this);
    splitterMain->addWidget(splitterContent);
    splitterMain->addWidget(m_pLogConsole);
    splitterMain->setStretchFactor(0, 4);
    splitterMain->setStretchFactor(1, 1);

    rootLayout->addWidget(grpTop);
    rootLayout->addLayout(layoutBadges);
    rootLayout->addWidget(splitterMain, 1);

    // --- Connect Signals ---
    connect(m_btnRunPrecheck, &QPushButton::clicked, this, &MainWindow::runFullPrecheck);
    connect(m_btnExportReport, &QPushButton::clicked, this, &MainWindow::exportReport);
    connect(m_btnAddModel, &QPushButton::clicked, this, &MainWindow::addModel);
    connect(m_btnRemoveModel, &QPushButton::clicked, this, &MainWindow::removeModel);
    connect(m_listModels, &QListWidget::currentRowChanged, this, &MainWindow::onModelSelectionChanged);
    connect(m_btnBrowseModelPackage, &QPushButton::clicked, this, &MainWindow::browseCurrentModelPackage);
    connect(m_btnRefreshModelHeaders, &QPushButton::clicked, this, &MainWindow::refreshCurrentModelHeaders);
    connect(m_btnCompileCurrent, &QPushButton::clicked, this, &MainWindow::compileCurrentModel);
    connect(m_btnCompileAll, &QPushButton::clicked, this, &MainWindow::compileAllModels);
    connect(m_btnAddRandomVar, &QPushButton::clicked, this, &MainWindow::addRandomVarRow);
    connect(m_btnRemoveRandomVar, &QPushButton::clicked, this, &MainWindow::removeRandomVarRow);
    connect(m_btnRunStress, &QPushButton::clicked, this, &MainWindow::runStressTestOnly);
    connect(m_btnRunTrajectory, &QPushButton::clicked, this, &MainWindow::runTrajectoryPreview);
    connect(m_btnRunMultiModel, &QPushButton::clicked, this, &MainWindow::runMultiModelTest);
    connect(m_btnRunMultiThread, &QPushButton::clicked, this, &MainWindow::runMultiThreadTest);
    connect(m_comboPeModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPeModelChanged);
    connect(m_comboPeDll, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPeDllChanged);

    setEditorsEnabled(false);
    applyDarkStyle();
    logMessage("INFO: 初始化完毕。请在左侧「型号与 UserMain」添加型号、配置包路径并编译，输出目录为 exe 旁 TestModel/<型号名>/。");
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

void MainWindow::logMessage(const QString& msg) {
    m_pLogConsole->appendLog(msg);
}

std::vector<RandomVarDef> MainWindow::DefaultRandomVars() {
    std::vector<RandomVarDef> vars;
    auto add = [&](const char* name, double lo, double hi) {
        RandomVarDef v;
        v.name = name;
        v.type = RandomVarType::Double;
        v.minValue = lo;
        v.maxValue = hi;
        v.enabled = true;
        vars.push_back(v);
    };
    add("lat", 20.0, 50.0);
    add("lon", 100.0, 130.0);
    add("alt", 1000.0, 8000.0);
    add("speed", 200.0, 800.0);
    return vars;
}

void MainWindow::FillRandomVarTable(QTableWidget* table, const std::vector<RandomVarDef>& vars) {
    if (!table) return;
    table->setRowCount(0);
    for (const auto& v : vars) {
        int row = table->rowCount();
        table->insertRow(row);
        QTableWidgetItem* en = new QTableWidgetItem();
        en->setCheckState(v.enabled ? Qt::Checked : Qt::Unchecked);
        table->setItem(row, 0, en);
        table->setItem(row, 1, new QTableWidgetItem(qUtf8(v.name)));
        QComboBox* ty = new QComboBox(table);
        ty->addItem("double", 0);
        ty->addItem("int", 1);
        ty->setCurrentIndex(v.type == RandomVarType::Int ? 1 : 0);
        table->setCellWidget(row, 2, ty);
        table->setItem(row, 3, new QTableWidgetItem(QString::number(v.minValue)));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(v.maxValue)));
    }
}

std::vector<RandomVarDef> MainWindow::ReadRandomVarTable(QTableWidget* table) {
    std::vector<RandomVarDef> vars;
    if (!table) return vars;
    for (int r = 0; r < table->rowCount(); ++r) {
        RandomVarDef v;
        QTableWidgetItem* en = table->item(r, 0);
        v.enabled = en && en->checkState() == Qt::Checked;
        QTableWidgetItem* nameItem = table->item(r, 1);
        v.name = nameItem ? qToUtf8(nameItem->text().trimmed()) : "";
        QComboBox* ty = qobject_cast<QComboBox*>(table->cellWidget(r, 2));
        v.type = (ty && ty->currentData().toInt() == 1) ? RandomVarType::Int : RandomVarType::Double;
        QTableWidgetItem* lo = table->item(r, 3);
        QTableWidgetItem* hi = table->item(r, 4);
        v.minValue = lo ? lo->text().toDouble() : 0.0;
        v.maxValue = hi ? hi->text().toDouble() : 1.0;
        vars.push_back(v);
    }
    return vars;
}

int MainWindow::currentModelIndex() const {
    return m_currentModelIndex;
}

int MainWindow::selectedTestModelIndex(QComboBox* combo) const {
    if (!combo) return -1;
    int idx = combo->currentData().toInt();
    if (idx < 0 || idx >= static_cast<int>(m_models.size())) return -1;
    return idx;
}

FleetModelEntry* MainWindow::selectedTestModel(QComboBox* combo) {
    int idx = selectedTestModelIndex(combo);
    if (idx < 0) return nullptr;
    return &m_models[static_cast<size_t>(idx)];
}

bool MainWindow::requireSelectedModelCompiled(QComboBox* combo, int /*switchToTab*/) {
    FleetModelEntry* entry = selectedTestModel(combo);
    if (!entry) {
        logMessage("ERROR: 请先选择已配置的型号");
        return false;
    }
    if (!entry->harness || !entry->harness->IsLoaded()) {
        logMessage(QString("ERROR: 型号「%1」尚未编译成功，请先在左侧「型号与 UserMain」编译").arg(entry->name));
        return false;
    }
    return true;
}

void MainWindow::setEditorsEnabled(bool on) {
    m_editModelName->setEnabled(on);
    m_editModelPackage->setEnabled(on);
    m_btnBrowseModelPackage->setEnabled(on);
    m_btnRefreshModelHeaders->setEnabled(on);
    m_listHarnessHeaders->setEnabled(on);
    m_editUserMain->setEnabled(on);
    m_tblRandomVars->setEnabled(on);
    m_btnAddRandomVar->setEnabled(on);
    m_btnRemoveRandomVar->setEnabled(on);
    m_btnCompileCurrent->setEnabled(on);
}

void MainWindow::saveEditorsToCurrentModel() {
    if (m_blockModelUi) return;
    if (m_currentModelIndex < 0 || m_currentModelIndex >= static_cast<int>(m_models.size())) return;

    FleetModelEntry& entry = m_models[static_cast<size_t>(m_currentModelIndex)];
    entry.name = m_editModelName->text().trimmed();
    if (entry.name.isEmpty()) {
        entry.name = QStringLiteral("model%1").arg(m_currentModelIndex + 1);
    }
    entry.packageDir = QDir::toNativeSeparators(m_editModelPackage->text().trimmed());
    entry.userMainBody = m_editUserMain->toPlainText();
    entry.randomVars = ReadRandomVarTable(m_tblRandomVars);

    entry.headerPaths.clear();
    for (int i = 0; i < m_listHarnessHeaders->count(); ++i) {
        QListWidgetItem* it = m_listHarnessHeaders->item(i);
        if (it && it->checkState() == Qt::Checked) {
            entry.headerPaths.push_back(it->data(Qt::UserRole).toString());
        }
    }
}

void MainWindow::loadEditorsFromModel(int index) {
    m_blockModelUi = true;
    if (index < 0 || index >= static_cast<int>(m_models.size())) {
        m_currentModelIndex = -1;
        m_editModelName->clear();
        m_editModelPackage->clear();
        m_listHarnessHeaders->clear();
        m_editUserMain->clear();
        m_tblRandomVars->setRowCount(0);
        m_lblHarnessStatus->setText("Harness: 未编译");
        setEditorsEnabled(false);
        m_blockModelUi = false;
        return;
    }

    m_currentModelIndex = index;
    const FleetModelEntry& entry = m_models[static_cast<size_t>(index)];
    setEditorsEnabled(true);

    m_editModelName->setText(entry.name);
    m_editModelPackage->setText(entry.packageDir);
    m_editUserMain->setPlainText(entry.userMainBody);
    FillRandomVarTable(m_tblRandomVars, entry.randomVars);

    // Rebuild header checklist from package dir, restore checked paths
    m_listHarnessHeaders->clear();
    QString dir = entry.packageDir.trimmed();
    if (!dir.isEmpty() && QDir(dir).exists()) {
        QDirIterator it(dir, QStringList() << "*.h" << "*.hpp", QDir::Files, QDirIterator::Subdirectories);
        int n = 0;
        while (it.hasNext() && n < 300) {
            QString path = it.next();
            QString abs = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
            auto* item = new QListWidgetItem(QDir(dir).relativeFilePath(path));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
            item->setCheckState(entry.headerPaths.contains(abs) ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, abs);
            item->setToolTip(abs);
            m_listHarnessHeaders->addItem(item);
            ++n;
        }
    }

    if (entry.harness && entry.harness->IsLoaded()) {
        m_lblHarnessStatus->setText(QString("Harness: 已加载 — %1").arg(qUtf8(entry.harness->DllPath())));
    } else {
        m_lblHarnessStatus->setText(QString("Harness: %1").arg(entry.status));
    }

    m_blockModelUi = false;
}

void MainWindow::refreshModelListUi() {
    int keep = m_currentModelIndex;
    QString keepName;
    if (keep >= 0 && keep < static_cast<int>(m_models.size())) {
        keepName = m_models[static_cast<size_t>(keep)].name;
    }

    m_blockModelUi = true;
    {
        QSignalBlocker blocker(m_listModels);
        m_listModels->clear();
        for (size_t i = 0; i < m_models.size(); ++i) {
            const FleetModelEntry& e = m_models[i];
            m_listModels->addItem(QString("%1  [%2]").arg(e.name, e.status));
        }
    }
    m_blockModelUi = false;

    int select = -1;
    if (!keepName.isEmpty()) {
        for (int i = 0; i < static_cast<int>(m_models.size()); ++i) {
            if (m_models[static_cast<size_t>(i)].name == keepName) {
                select = i;
                break;
            }
        }
    }
    if (select < 0 && keep >= 0 && keep < static_cast<int>(m_models.size())) {
        select = keep;
    }
    if (select < 0 && !m_models.empty()) {
        select = 0;
    }

    if (select >= 0) {
        m_listModels->setCurrentRow(select);
        // currentRowChanged may not fire if already selected; force load
        if (m_currentModelIndex != select) {
            onModelSelectionChanged();
        } else {
            loadEditorsFromModel(select);
        }
    } else {
        loadEditorsFromModel(-1);
    }

    refreshModelSelectors();
    refreshFleetCountTable();
}

void MainWindow::refreshModelSelectors() {
    auto refill = [this](QComboBox* combo) {
        if (!combo) return;
        int prev = combo->currentData().toInt();
        QSignalBlocker blocker(combo);
        combo->clear();
        for (int i = 0; i < static_cast<int>(m_models.size()); ++i) {
            const FleetModelEntry& e = m_models[static_cast<size_t>(i)];
            combo->addItem(QString("%1 (%2)").arg(e.name, e.status), i);
        }
        int restore = -1;
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == prev) {
                restore = i;
                break;
            }
        }
        if (restore >= 0) {
            combo->setCurrentIndex(restore);
        } else if (combo->count() > 0) {
            combo->setCurrentIndex(0);
        }
    };
    refill(m_comboStressModel);
    refill(m_comboThreadModel);
    refreshPeSelectors();
}

void MainWindow::refreshPeSelectors() {
    if (!m_comboPeModel || !m_comboPeDll) return;

    const int prevModel = m_comboPeModel->currentData().toInt();
    QString prevDllPath;
    if (m_comboPeDll->currentIndex() >= 0) {
        prevDllPath = m_comboPeDll->currentData().toString();
    }

    {
        QSignalBlocker b(m_comboPeModel);
        m_comboPeModel->clear();
        if (!m_latestFleetReport.modelReports.empty()) {
            for (int i = 0; i < static_cast<int>(m_latestFleetReport.modelReports.size()); ++i) {
                const auto& m = m_latestFleetReport.modelReports[static_cast<size_t>(i)];
                QString name = m.modelName.empty() ? QString("型号%1").arg(i + 1) : qUtf8(m.modelName);
                m_comboPeModel->addItem(name, i);
            }
        } else if (!m_latestDualReport.dllReports.empty()) {
            QString name = m_latestDualReport.modelName.empty()
                ? QStringLiteral("当前包") : qUtf8(m_latestDualReport.modelName);
            m_comboPeModel->addItem(name, 0);
        } else if (!m_latestReport.peReport.filePath.empty() || !m_latestReport.dllPath.empty()) {
            m_comboPeModel->addItem(QStringLiteral("当前 DLL"), 0);
        }
        int restore = -1;
        for (int i = 0; i < m_comboPeModel->count(); ++i) {
            if (m_comboPeModel->itemData(i).toInt() == prevModel) {
                restore = i;
                break;
            }
        }
        if (restore >= 0) m_comboPeModel->setCurrentIndex(restore);
        else if (m_comboPeModel->count() > 0) m_comboPeModel->setCurrentIndex(0);
    }

    onPeModelChanged(m_comboPeModel->currentIndex());

    if (!prevDllPath.isEmpty()) {
        for (int i = 0; i < m_comboPeDll->count(); ++i) {
            if (m_comboPeDll->itemData(i).toString() == prevDllPath) {
                QSignalBlocker b(m_comboPeDll);
                m_comboPeDll->setCurrentIndex(i);
                break;
            }
        }
        onPeDllChanged(m_comboPeDll->currentIndex());
    }
}

void MainWindow::onPeModelChanged(int /*index*/) {
    if (!m_comboPeDll) return;
    QSignalBlocker b(m_comboPeDll);
    m_comboPeDll->clear();

    if (!m_latestFleetReport.modelReports.empty()) {
        int mi = m_comboPeModel->currentData().toInt();
        if (mi >= 0 && mi < static_cast<int>(m_latestFleetReport.modelReports.size())) {
            const auto& m = m_latestFleetReport.modelReports[static_cast<size_t>(mi)];
            for (const auto& d : m.dllReports) {
                QString path = qUtf8(d.dllPath.empty() ? d.peReport.filePath : d.dllPath);
                QString cfg = qUtf8(d.buildConfig);
                QString label = path;
                if (!cfg.isEmpty()) label = QString("[%1] %2").arg(cfg, path);
                m_comboPeDll->addItem(label, path);
            }
        }
    } else if (!m_latestDualReport.dllReports.empty()) {
        for (const auto& d : m_latestDualReport.dllReports) {
            QString path = qUtf8(d.dllPath.empty() ? d.peReport.filePath : d.dllPath);
            QString cfg = qUtf8(d.buildConfig);
            QString label = path;
            if (!cfg.isEmpty()) label = QString("[%1] %2").arg(cfg, path);
            m_comboPeDll->addItem(label, path);
        }
    } else if (!m_latestReport.peReport.filePath.empty() || !m_latestReport.dllPath.empty()) {
        QString path = qUtf8(m_latestReport.dllPath.empty()
            ? m_latestReport.peReport.filePath : m_latestReport.dllPath);
        m_comboPeDll->addItem(path, path);
    }

    if (m_comboPeDll->count() > 0) {
        m_comboPeDll->setCurrentIndex(0);
    }
    onPeDllChanged(m_comboPeDll->currentIndex());
}

void MainWindow::onPeDllChanged(int /*index*/) {
    const CombinedPrecheckReport* rep = selectedPeDllReport();
    if (rep) {
        updatePeView(rep->peReport);
    }
}

const CombinedPrecheckReport* MainWindow::selectedPeDllReport() const {
    if (!m_comboPeModel || !m_comboPeDll || m_comboPeDll->currentIndex() < 0) {
        return nullptr;
    }
    const QString wantPath = m_comboPeDll->currentData().toString();

    auto matchPath = [&](const CombinedPrecheckReport& d) -> bool {
        QString path = qUtf8(d.dllPath.empty() ? d.peReport.filePath : d.dllPath);
        return path == wantPath;
    };

    if (!m_latestFleetReport.modelReports.empty()) {
        int mi = m_comboPeModel->currentData().toInt();
        if (mi >= 0 && mi < static_cast<int>(m_latestFleetReport.modelReports.size())) {
            const auto& m = m_latestFleetReport.modelReports[static_cast<size_t>(mi)];
            for (const auto& d : m.dllReports) {
                if (matchPath(d)) return &d;
            }
            if (!m.dllReports.empty()) return &m.dllReports.front();
        }
        return nullptr;
    }
    if (!m_latestDualReport.dllReports.empty()) {
        for (const auto& d : m_latestDualReport.dllReports) {
            if (matchPath(d)) return &d;
        }
        return &m_latestDualReport.dllReports.front();
    }
    if (!m_latestReport.peReport.filePath.empty() || !m_latestReport.dllPath.empty()) {
        return &m_latestReport;
    }
    return nullptr;
}

void MainWindow::refreshReportBrowser() {
    if (!m_pReportBrowser) return;
    if (!m_latestFleetReport.modelReports.empty()) {
        m_pReportBrowser->setHtml(qUtf8(ReportGenerator::GenerateFleetHtml(m_latestFleetReport)));
    } else {
        m_pReportBrowser->setHtml(qUtf8(ReportGenerator::GenerateHtml(m_latestReport)));
    }
}

void MainWindow::refreshFleetCountTable() {
    QSignalBlocker blocker(m_tblFleetCounts);
    m_tblFleetCounts->setRowCount(0);
    for (int i = 0; i < static_cast<int>(m_models.size()); ++i) {
        const FleetModelEntry& e = m_models[static_cast<size_t>(i)];
        int row = m_tblFleetCounts->rowCount();
        m_tblFleetCounts->insertRow(row);
        m_tblFleetCounts->setItem(row, 0, new QTableWidgetItem(e.name));
        m_tblFleetCounts->setItem(row, 1, new QTableWidgetItem(e.packageDir));
        m_tblFleetCounts->setItem(row, 2, new QTableWidgetItem(e.status));
        QSpinBox* spn = new QSpinBox(m_tblFleetCounts);
        spn->setRange(1, 64);
        spn->setValue(e.instanceCount);
        spn->setProperty("fleetIndex", i);
        connect(spn, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onFleetCountChanged);
        m_tblFleetCounts->setCellWidget(row, 3, spn);
    }
}

void MainWindow::onFleetCountChanged(int value) {
    QSpinBox* spn = qobject_cast<QSpinBox*>(sender());
    if (!spn) return;
    int idx = spn->property("fleetIndex").toInt();
    if (idx < 0 || idx >= static_cast<int>(m_models.size())) return;
    m_models[static_cast<size_t>(idx)].instanceCount = value;
}

void MainWindow::addModel() {
    saveEditorsToCurrentModel();

    FleetModelEntry entry;
    entry.name = QStringLiteral("model%1").arg(m_models.size() + 1);
    entry.packageDir = QString();
    entry.userMainBody = qUtf8(UserCodeHarness::DefaultUserMainTemplate());
    entry.randomVars = DefaultRandomVars();
    entry.instanceCount = 1;
    entry.status = QStringLiteral("未编译");

    m_models.push_back(entry);
    refreshModelListUi();
    m_listModels->setCurrentRow(static_cast<int>(m_models.size()) - 1);
    logMessage(QString("INFO: 已添加型号「%1」（请浏览并设置模型包路径）").arg(entry.name));
}

void MainWindow::removeModel() {
    int row = m_listModels->currentRow();
    if (row < 0 || row >= static_cast<int>(m_models.size())) {
        QMessageBox::information(this, "提示", "请先选中要删除的型号");
        return;
    }
    QString name = m_models[static_cast<size_t>(row)].name;
    m_models.erase(m_models.begin() + row);
    m_currentModelIndex = -1;
    refreshModelListUi();
    logMessage(QString("INFO: 已删除型号「%1」").arg(name));
}

void MainWindow::onModelSelectionChanged() {
    if (m_blockModelUi) return;
    saveEditorsToCurrentModel();
    int row = m_listModels->currentRow();
    loadEditorsFromModel(row);
}

void MainWindow::browseCurrentModelPackage() {
    if (m_currentModelIndex < 0) return;
    QString start = m_editModelPackage->text().trimmed();
    QString dir = QFileDialog::getExistingDirectory(this, "选择型号模型包根目录", start);
    if (dir.isEmpty()) return;

    dir = QDir::toNativeSeparators(dir);
    m_editModelPackage->setText(dir);
    if (m_editModelName->text().trimmed().isEmpty()
        || m_editModelName->text().startsWith(QStringLiteral("model"))) {
        m_editModelName->setText(QFileInfo(dir).fileName());
    }
    saveEditorsToCurrentModel();
    refreshCurrentModelHeaders();
    refreshModelListUi();
}

void MainWindow::refreshCurrentModelHeaders() {
    if (m_currentModelIndex < 0 || m_currentModelIndex >= static_cast<int>(m_models.size())) {
        logMessage("WARN: 请先选择型号");
        return;
    }
    saveEditorsToCurrentModel();
    FleetModelEntry& entry = m_models[static_cast<size_t>(m_currentModelIndex)];
    QString dir = entry.packageDir.trimmed();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        logMessage("WARN: 请先为当前型号选择有效的模型包目录");
        return;
    }

    QStringList prevChecked = entry.headerPaths;
    m_listHarnessHeaders->clear();
    QDirIterator it(dir, QStringList() << "*.h" << "*.hpp", QDir::Files, QDirIterator::Subdirectories);
    int n = 0;
    while (it.hasNext() && n < 300) {
        QString path = it.next();
        QString abs = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
        auto* item = new QListWidgetItem(QDir(dir).relativeFilePath(path));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        bool checked = prevChecked.contains(abs) || (prevChecked.isEmpty() && n == 0);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, abs);
        item->setToolTip(abs);
        m_listHarnessHeaders->addItem(item);
        ++n;
    }
    saveEditorsToCurrentModel();
    logMessage(QString("INFO: 型号「%1」头文件列表已刷新，共 %2 个").arg(entry.name).arg(n));
}

UserHarnessConfig MainWindow::buildHarnessConfig(const FleetModelEntry& entry, int index) const {
    UserHarnessConfig cfg;
    cfg.userMainBody = qToUtf8(entry.userMainBody);
    cfg.randomVars = entry.randomVars;

    QString folderName = SanitizeModelFolderName(entry.name);
    if (folderName.isEmpty()) {
        folderName = QStringLiteral("model_%1").arg(index);
    }
    QString outDir = QDir(ExeTestModelRoot()).filePath(folderName);
    QDir().mkpath(outDir);
    cfg.workDir = qToUtf8(QDir::toNativeSeparators(outDir));
    cfg.outputBaseName = qToUtf8(folderName);

    QString pkg = entry.packageDir.trimmed();
    if (!pkg.isEmpty()) {
        cfg.includeDirs.push_back(qToUtf8(pkg));
        cfg.libPaths.push_back(qToUtf8(pkg));
        QStringList sub = { "include", "Include", "inc", "lib", "Lib", "x64/Release", "Release", "lib/Release" };
        for (const QString& s : sub) {
            QString p = QDir(pkg).filePath(s);
            if (QDir(p).exists()) {
                cfg.includeDirs.push_back(qToUtf8(QDir::toNativeSeparators(p)));
                cfg.libPaths.push_back(qToUtf8(QDir::toNativeSeparators(p)));
            }
        }
        QDirIterator lit(pkg, QStringList() << "*.lib", QDir::Files, QDirIterator::Subdirectories);
        int libCount = 0;
        while (lit.hasNext() && libCount < 20) {
            cfg.linkLibs.push_back(qToUtf8(QDir::toNativeSeparators(lit.next())));
            ++libCount;
        }
    }
    for (const QString& h : entry.headerPaths) {
        cfg.headerPaths.push_back(qToUtf8(h));
    }
    return cfg;
}

void MainWindow::compileCurrentModel() {
    saveEditorsToCurrentModel();
    if (m_currentModelIndex < 0 || m_currentModelIndex >= static_cast<int>(m_models.size())) {
        QMessageBox::warning(this, "警告", "请先选择要编译的型号");
        return;
    }

    FleetModelEntry& entry = m_models[static_cast<size_t>(m_currentModelIndex)];
    if (entry.headerPaths.isEmpty()) {
        logMessage(QString("ERROR: 型号「%1」未勾选头文件").arg(entry.name));
        return;
    }
    if (entry.userMainBody.trimmed().isEmpty()) {
        logMessage(QString("ERROR: 型号「%1」UserMain 为空").arg(entry.name));
        return;
    }

    logMessage(QString("INFO: 开始编译型号「%1」...").arg(entry.name));
    m_lblHarnessStatus->setText(QString("Harness: 正在编译「%1」…").arg(entry.name));
    entry.status = QStringLiteral("编译中…");
    refreshModelListUi();

    CompileWaitIndicator wait(this, QString("正在编译型号「%1」…\n请稍候").arg(entry.name));
    UserHarnessConfig cfg = buildHarnessConfig(entry, m_currentModelIndex);
    entry.harness = std::make_shared<UserCodeHarness>();
    CompileResult cr = entry.harness->Compile(cfg);
    logMessage(qDecodeLog(cr.log));
    if (cr.success) {
        entry.status = QStringLiteral("已加载");
        m_lblHarnessStatus->setText(QString("Harness: 已加载 — %1").arg(qUtf8(cr.dllPath)));
        logMessage(QString("SUCCESS: 型号「%1」编译成功 → %2").arg(entry.name).arg(qUtf8(cr.dllPath)));
    } else {
        entry.status = QStringLiteral("编译失败");
        entry.harness.reset();
        m_lblHarnessStatus->setText("Harness: 编译失败（详见日志）");
    }
    refreshModelListUi();
}

void MainWindow::compileAllModels() {
    saveEditorsToCurrentModel();
    if (m_models.empty()) {
        QMessageBox::warning(this, "警告", "请先添加型号");
        return;
    }

    logMessage("INFO: 开始编译全部型号 Harness...");
    CompileWaitIndicator wait(this, QString("正在编译全部型号 (0/%1)…").arg(m_models.size()));

    int ok = 0;
    for (int i = 0; i < static_cast<int>(m_models.size()); ++i) {
        FleetModelEntry& entry = m_models[static_cast<size_t>(i)];
        wait.setText(QString("正在编译型号「%1」…\n(%2 / %3)")
            .arg(entry.name).arg(i + 1).arg(m_models.size()));
        entry.status = QStringLiteral("编译中…");
        refreshModelListUi();

        if (entry.headerPaths.isEmpty()) {
            entry.status = QStringLiteral("失败:无头文件");
            logMessage(QString("ERROR: 型号「%1」未勾选头文件").arg(entry.name));
            continue;
        }
        if (entry.userMainBody.trimmed().isEmpty()) {
            entry.status = QStringLiteral("失败:无UserMain");
            logMessage(QString("ERROR: 型号「%1」UserMain 为空").arg(entry.name));
            continue;
        }
        UserHarnessConfig cfg = buildHarnessConfig(entry, i);
        entry.harness = std::make_shared<UserCodeHarness>();
        CompileResult cr = entry.harness->Compile(cfg);
        logMessage(QString("---- 型号「%1」编译日志 ----").arg(entry.name));
        logMessage(qDecodeLog(cr.log));
        if (cr.success) {
            entry.status = QStringLiteral("已加载");
            ++ok;
            logMessage(QString("SUCCESS: 型号「%1」编译成功 → %2").arg(entry.name).arg(qUtf8(cr.dllPath)));
        } else {
            entry.status = QStringLiteral("编译失败");
            entry.harness.reset();
        }
    }
    refreshModelListUi();
    logMessage(QString("INFO: 型号编译完成 %1/%2 成功").arg(ok).arg(m_models.size()));
}

void MainWindow::addRandomVarRow() {
    int row = m_tblRandomVars->rowCount();
    m_tblRandomVars->insertRow(row);
    QTableWidgetItem* en = new QTableWidgetItem();
    en->setCheckState(Qt::Checked);
    m_tblRandomVars->setItem(row, 0, en);
    m_tblRandomVars->setItem(row, 1, new QTableWidgetItem("var"));
    QComboBox* ty = new QComboBox(m_tblRandomVars);
    ty->addItem("double", 0);
    ty->addItem("int", 1);
    m_tblRandomVars->setCellWidget(row, 2, ty);
    m_tblRandomVars->setItem(row, 3, new QTableWidgetItem("0"));
    m_tblRandomVars->setItem(row, 4, new QTableWidgetItem("1"));
}

void MainWindow::removeRandomVarRow() {
    int row = m_tblRandomVars->currentRow();
    if (row >= 0) m_tblRandomVars->removeRow(row);
}

std::string MainWindow::pickHeaderForDll(const ModelPackageFiles& pkg, const std::string& dllPath) const {
    if (pkg.allHeaderFiles.empty()) return std::string();
    QFileInfo dllInfo(qUtf8(dllPath));
    QString base = dllInfo.completeBaseName().toLower();
    for (const auto& h : pkg.allHeaderFiles) {
        QString hn = QFileInfo(qUtf8(h)).completeBaseName().toLower();
        if (hn.contains(base) || base.contains(hn) || hn == "weaponmodel") {
            return h;
        }
    }
    return pkg.allHeaderFiles.front();
}

CombinedPrecheckReport MainWindow::runBuildPrecheck(const std::string& dllPath, const std::string& headerPath,
                                                    const std::string& libPath, const std::string& buildConfig,
                                                    const std::string& packageDir) {
    CombinedPrecheckReport report;
    report.dllPath = dllPath;
    report.headerPath = headerPath;
    report.libPath = libPath;
    report.buildConfig = buildConfig;
    report.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    std::vector<std::string> searchPaths;
    if (!packageDir.empty()) {
        searchPaths.push_back(packageDir);
    }

    std::vector<std::string> reqExports = {};

    report.peReport = PeAnalyzer::AnalyzeDll(dllPath, searchPaths, reqExports);

    std::vector<std::string> peExports;
    for (const auto& sym : report.peReport.exportedSymbols) {
        peExports.push_back(sym.name);
    }

    if (!libPath.empty() && QFile::exists(qUtf8(libPath))) {
        report.libReport = LibAnalyzer::AnalyzeLib(libPath, reqExports);
    }

    if (!headerPath.empty() && QFile::exists(qUtf8(headerPath))) {
        report.headerReport = HeaderAnalyzer::AnalyzeHeader(headerPath);
        report.consistencyReport = HeaderAnalyzer::VerifyConsistency(report.headerReport.declaredFunctions, peExports);
    }

    report.loadReport = m_dllLoader.Load(dllPath, InterfaceMapping::DefaultSingleton());
    if (!report.loadReport.isLoaded) {
        logMessage("WARN: DLL 加载失败: " + qDecodeLog(report.loadReport.errorLog));
    } else {
        logMessage("INFO: DLL 已加载（动态调用请在「型号与 UserMain」中编写 UserMain）");
        report.trajReport.overallPass = true;
        report.trajReport.warnings.push_back("轨迹校验功能已移除；请在 UserMain 中自行驱动模型");
        report.perfReport.realtimeVerdict = "PASS";
    }

    report.overallPass = report.peReport.overallPass && report.loadReport.isLoaded;
    return report;
}

DualBuildPrecheckReport MainWindow::precheckOneModel(const FleetModelEntry& entry) {
    DualBuildPrecheckReport dual;
    dual.modelName = qToUtf8(entry.name);
    dual.packageDir = qToUtf8(entry.packageDir);
    dual.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    QString pkgDir = entry.packageDir.trimmed();
    if (pkgDir.isEmpty() || !QDir(pkgDir).exists()) {
        logMessage(QString("ERROR: 型号「%1」模型包路径无效: %2").arg(entry.name, pkgDir));
        dual.overallPass = false;
        return dual;
    }

    logMessage("================================================================================");
    logMessage(QString("INFO: 型号「%1」全覆盖预检: %2").arg(entry.name, pkgDir));

    ModelPackageFiles pkgFiles = PackageScanner::ScanPackageDirectory(qToUtf8(pkgDir));
    dual.packageFiles = pkgFiles;
    for (const auto& msg : pkgFiles.scanLog) {
        logMessage(qDecodeLog(msg));
    }

    std::vector<std::string> combinedHeaderFuncs;
    std::vector<std::string> combinedBinaryExports;

    logMessage("--------------------------------------------------------------------------------");
    logMessage("INFO: 阶段 1/3: 头文件预检 (" + QString::number(pkgFiles.allHeaderFiles.size()) + " 个)...");
    for (const auto& hPath : pkgFiles.allHeaderFiles) {
        logMessage(" -> 预检头文件: " + qUtf8(hPath));
        HeaderAnalysisReport hRep = HeaderAnalyzer::AnalyzeHeader(hPath);
        for (const auto& msg : hRep.logMessages) {
            logMessage(qDecodeLog(msg));
        }
        if (hRep.overallPass) {
            dual.passedHeaderCount++;
        }
        dual.headerReports.push_back(hRep);
        combinedHeaderFuncs.insert(combinedHeaderFuncs.end(), hRep.declaredFunctions.begin(), hRep.declaredFunctions.end());
    }

    logMessage("--------------------------------------------------------------------------------");
    logMessage("INFO: 阶段 2/3: LIB 库预检 (" + QString::number(pkgFiles.allLibFiles.size()) + " 个)...");
    for (const auto& lPath : pkgFiles.allLibFiles) {
        logMessage(" -> 预检 LIB 库: " + qUtf8(lPath));
        LibAnalysisReport lRep = LibAnalyzer::AnalyzeLib(lPath, {});
        for (const auto& msg : lRep.logMessages) {
            logMessage(qDecodeLog(msg));
        }
        if (lRep.overallPass) {
            dual.passedLibCount++;
        }
        dual.libReports.push_back(lRep);
        combinedBinaryExports.insert(combinedBinaryExports.end(), lRep.foundSymbols.begin(), lRep.foundSymbols.end());
    }

    logMessage("--------------------------------------------------------------------------------");
    logMessage("INFO: 阶段 3/3: DLL 动态库预检 (" + QString::number(pkgFiles.allDllFiles.size()) + " 个)...");
    for (const auto& dPath : pkgFiles.allDllFiles) {
        QFileInfo fi(qUtf8(dPath));
        QString lowerPath = qUtf8(dPath).toLower();
        QString configStr = "Release";
        if (lowerPath.contains("/debug/") || lowerPath.contains("\\debug\\")
            || fi.completeBaseName().endsWith("d", Qt::CaseInsensitive)) {
            configStr = "Debug";
        }

        logMessage(" -> 预检 DLL 动态库 [" + configStr + "]: " + qUtf8(dPath));
        std::string matchedHeader = pickHeaderForDll(pkgFiles, dPath);
        if (!matchedHeader.empty()) {
            logMessage("    关联头文件契约: " + qUtf8(matchedHeader));
        }
        CombinedPrecheckReport dRep = runBuildPrecheck(dPath, matchedHeader, "", qToUtf8(configStr), qToUtf8(pkgDir));
        if (dRep.overallPass) {
            dual.passedDllCount++;
        }
        dual.dllReports.push_back(dRep);

        for (const auto& sym : dRep.peReport.exportedSymbols) {
            combinedBinaryExports.push_back(sym.name);
        }

        m_latestReport = dRep;
    }

    dual.consistencyReport = HeaderAnalyzer::VerifyConsistency(combinedHeaderFuncs, combinedBinaryExports);
    for (const auto& msg : dual.consistencyReport.logMessages) {
        logMessage(qDecodeLog(msg));
    }

    bool headersPass = pkgFiles.allHeaderFiles.empty()
        || (dual.passedHeaderCount == static_cast<int>(pkgFiles.allHeaderFiles.size()));
    bool libsPass = pkgFiles.allLibFiles.empty()
        || (dual.passedLibCount == static_cast<int>(pkgFiles.allLibFiles.size()));
    bool dllsPass = pkgFiles.allDllFiles.empty()
        || (dual.passedDllCount == static_cast<int>(pkgFiles.allDllFiles.size()));

    dual.overallPass = headersPass && libsPass && dllsPass;
    return dual;
}

void MainWindow::runFullPrecheck() {
    saveEditorsToCurrentModel();
    if (m_models.empty()) {
        QMessageBox::warning(this, "警告", "请先在左侧「型号与 UserMain」添加至少一个型号！");
        return;
    }

    for (const auto& e : m_models) {
        if (e.packageDir.trimmed().isEmpty() || !QDir(e.packageDir).exists()) {
            QMessageBox::warning(this, "警告",
                QString("型号「%1」尚未设置有效的模型包路径").arg(e.name));
            return;
        }
    }

    logMessage("================================================================================");
    logMessage(QString("INFO: 启动全部型号一键预检（共 %1 个型号）...").arg(m_models.size()));

    // 保留已跑过的压测/并行结果，避免一键预检后矩阵丢失这些行
    const auto keepMultiModel = m_latestFleetReport.multiModelReport;
    const auto keepMultiThread = m_latestFleetReport.multiThreadReport;
    const auto keepPerf = m_latestFleetReport.perfReport;

    m_latestFleetReport = FleetSessionReport();
    m_latestFleetReport.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    m_latestFleetReport.multiModelReport = keepMultiModel;
    m_latestFleetReport.multiThreadReport = keepMultiThread;
    m_latestFleetReport.perfReport = keepPerf;
    if (keepPerf.realtimeVerdict.empty() && !m_latestReport.perfReport.realtimeVerdict.empty()) {
        m_latestFleetReport.perfReport = m_latestReport.perfReport;
    }
    if (keepMultiThread.verdict.empty() && !m_latestReport.multiThreadReport.verdict.empty()) {
        m_latestFleetReport.multiThreadReport = m_latestReport.multiThreadReport;
    }
    if (keepMultiModel.verdict.empty() && !m_latestReport.multiModelReport.verdict.empty()) {
        m_latestFleetReport.multiModelReport = m_latestReport.multiModelReport;
    }

    int passedModels = 0;
    for (size_t i = 0; i < m_models.size(); ++i) {
        DualBuildPrecheckReport one = precheckOneModel(m_models[i]);
        one.modelName = qToUtf8(m_models[i].name);
        if (one.overallPass) ++passedModels;
        m_latestFleetReport.modelReports.push_back(one);
        m_latestDualReport = one;
    }

    m_latestFleetReport.overallPass = (passedModels == static_cast<int>(m_models.size()));

    // Aggregate badges across all models into m_latestDualReport counts/sizes
    m_latestDualReport = DualBuildPrecheckReport();
    m_latestDualReport.timestamp = m_latestFleetReport.timestamp;
    for (const auto& mr : m_latestFleetReport.modelReports) {
        m_latestDualReport.passedHeaderCount += mr.passedHeaderCount;
        m_latestDualReport.passedLibCount += mr.passedLibCount;
        m_latestDualReport.passedDllCount += mr.passedDllCount;
        m_latestDualReport.headerReports.insert(m_latestDualReport.headerReports.end(),
                                                mr.headerReports.begin(), mr.headerReports.end());
        m_latestDualReport.libReports.insert(m_latestDualReport.libReports.end(),
                                             mr.libReports.begin(), mr.libReports.end());
        m_latestDualReport.dllReports.insert(m_latestDualReport.dllReports.end(),
                                             mr.dllReports.begin(), mr.dllReports.end());
        if (!mr.packageDir.empty()) {
            m_latestDualReport.packageDir = mr.packageDir;
        }
    }
    m_latestDualReport.overallPass = m_latestFleetReport.overallPass;

    // Sync first DLL into m_latestReport so single-report fields stay consistent
    if (!m_latestDualReport.dllReports.empty()) {
        const auto& first = m_latestDualReport.dllReports.front();
        m_latestReport.dllPath = first.dllPath;
        m_latestReport.peReport = first.peReport;
        m_latestReport.loadReport = first.loadReport;
        m_latestReport.buildConfig = first.buildConfig;
        m_latestReport.timestamp = m_latestFleetReport.timestamp;
        m_latestReport.multiModelReport = m_latestFleetReport.multiModelReport;
        m_latestReport.multiThreadReport = m_latestFleetReport.multiThreadReport;
        m_latestReport.perfReport = m_latestFleetReport.perfReport;
    }

    updateStatusBadges();
    refreshPeSelectors();
    refreshReportBrowser();
    m_pCentralTabs->setCurrentIndex(4); // 预检报告

    logMessage(QString("SUCCESS: 全部型号预检完毕！通过 %1/%2")
        .arg(passedModels).arg(m_models.size()));
}

void MainWindow::runStressTestOnly() {
    if (!requireSelectedModelCompiled(m_comboStressModel, 1)) {
        return;
    }
    FleetModelEntry* entry = selectedTestModel(m_comboStressModel);
    if (!entry || !entry->harness) return;

    m_pCentralTabs->setCurrentIndex(1); // 性能压测
    int runs = m_spnSteps->value();
    double targetHz = m_comboHz->currentData().toDouble();
    double frameBudgetMs = 1000.0 / (targetHz > 0 ? targetHz : 50.0);

    m_pChartViewer->PrepareLiveProfiling(runs, frameBudgetMs);

    logMessage(QString("INFO: 启动型号「%1」UserMain 性能压测 (重复次数: %2, 目标频率: %3 Hz)...")
        .arg(entry->name).arg(runs).arg(targetHz));

    if (m_pWorkerThread) {
        m_pWorkerThread->quit();
        m_pWorkerThread->wait();
        delete m_pWorkerThread;
        m_pWorkerThread = nullptr;
    }

    m_pWorkerThread = new QThread();
    PerfProfilerWorker* worker = new PerfProfilerWorker(entry->harness.get(), runs, targetHz);
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

void MainWindow::runTrajectoryPreview() {
    if (!requireSelectedModelCompiled(m_comboStressModel, 1)) {
        return;
    }
    FleetModelEntry* entry = selectedTestModel(m_comboStressModel);
    if (!entry || !entry->harness) return;

    m_pCentralTabs->setCurrentIndex(1);
    logMessage(QString("INFO: 试跑型号「%1」并采集 out_lat/out_lon 二维轨迹...")
        .arg(entry->name));

    if (!entry->harness->SetTrajectoryCapture(true)) {
        QMessageBox::warning(this, "警告",
            "当前 Harness 不支持轨迹采集。请重新「编译当前型号」后再试（需使用含 RecordTrajectoryPoint(out_lat, out_lon) 的新模板）。");
        return;
    }

    RandomValueBlob blob = entry->harness->Sample(42);
    int userRet = 0;
    bool seh = false;
    std::string err;
    const bool ok = entry->harness->RunOnce(blob, &userRet, &seh, err);
    entry->harness->SetTrajectoryCapture(false);

    std::vector<TrajectorySample> samples;
    entry->harness->FetchTrajectory(samples);

    QVector<TrajectoryPoint> pts;
    pts.reserve(static_cast<int>(samples.size()));
    for (const auto& s : samples) {
        TrajectoryPoint p;
        p.lat = s.lat;
        p.lon = s.lon;
        pts.push_back(p);
    }
    m_pTrajectoryView->setPoints(pts);

    auto fillTrajTable = [this](const QVector<TrajectoryPoint>& points) {
        m_tblTrajectoryPoints->setRowCount(0);
        for (int i = 0; i < points.size(); ++i) {
            const int row = m_tblTrajectoryPoints->rowCount();
            m_tblTrajectoryPoints->insertRow(row);
            m_tblTrajectoryPoints->setItem(row, 0, new QTableWidgetItem(QString::number(i + 1)));
            m_tblTrajectoryPoints->setItem(row, 1,
                new QTableWidgetItem(QString::number(points[i].lat, 'f', 8)));
            m_tblTrajectoryPoints->setItem(row, 2,
                new QTableWidgetItem(QString::number(points[i].lon, 'f', 8)));
        }
    };

    if (!ok || seh) {
        fillTrajTable({});
        m_lblTrajOut->setText(QStringLiteral("轨迹输出: 试跑失败 — %1")
            .arg(qUtf8(err.empty() ? "SEH/加载错误" : err)));
        logMessage(QString("ERROR: 轨迹试跑失败: %1").arg(qUtf8(err)));
        return;
    }
    if (userRet != 0) {
        logMessage(QString("WARN: UserMain 返回码=%1，仍尝试显示已记录轨迹点").arg(userRet));
    }
    if (pts.isEmpty()) {
        fillTrajTable({});
        m_lblTrajOut->setText(
            QStringLiteral("轨迹输出: 未采集到点。请在 UserMain 循环中写入 "
                           "out_lat/out_lon 并调用 RecordTrajectoryPoint(out_lat, out_lon) 后重新编译。"));
        logMessage("WARN: 轨迹点为空，请检查 UserMain 是否调用了 RecordTrajectoryPoint");
        return;
    }

    fillTrajTable(pts);
    const auto& last = pts.last();
    m_lblTrajOut->setText(
        QStringLiteral("轨迹输出: 点数=%1 | 末点 out_lat=%2, out_lon=%3 | 随机参数: %4")
            .arg(pts.size())
            .arg(last.lat, 0, 'f', 6)
            .arg(last.lon, 0, 'f', 6)
            .arg(qUtf8(blob.summary)));
    logMessage(QString("SUCCESS: 轨迹试跑完成，记录 %1 个经纬度点").arg(pts.size()));
}

void MainWindow::startConcurrencyWorker(UserCodeHarness* harness, const ConcurrencyTestConfig& cfg, int resultTabIndex) {
    if (m_pWorkerThread) {
        m_pWorkerThread->quit();
        m_pWorkerThread->wait();
        delete m_pWorkerThread;
        m_pWorkerThread = nullptr;
    }

    m_pCentralTabs->setCurrentIndex(resultTabIndex);

    m_pWorkerThread = new QThread();
    ConcurrencyTestWorker* worker = new ConcurrencyTestWorker(harness, cfg);
    worker->moveToThread(m_pWorkerThread);

    connect(m_pWorkerThread, &QThread::started, worker, &ConcurrencyTestWorker::process);
    connect(worker, &ConcurrencyTestWorker::finished, this, &MainWindow::onConcurrencyFinished);
    connect(worker, &ConcurrencyTestWorker::logMessage, this, &MainWindow::logMessage);
    connect(worker, &ConcurrencyTestWorker::finished, m_pWorkerThread, &QThread::quit);
    connect(worker, &ConcurrencyTestWorker::finished, worker, &QObject::deleteLater);

    m_pWorkerThread->start();
}

void MainWindow::runMultiModelTest() {
    saveEditorsToCurrentModel();
    if (m_models.empty()) {
        QMessageBox::warning(this, "警告", "请先添加至少一个型号");
        return;
    }
    for (size_t i = 0; i < m_models.size(); ++i) {
        if (!m_models[i].harness || !m_models[i].harness->IsLoaded()) {
            QMessageBox::warning(this, "警告",
                QString("型号「%1」尚未编译成功，请先在左侧「型号与 UserMain」编译").arg(m_models[i].name));
            return;
        }
        if (m_models[i].instanceCount < 1) {
            QMessageBox::warning(this, "警告", QString("型号「%1」实例数至少为 1").arg(m_models[i].name));
            return;
        }
    }

    ConcurrencyTestConfig cfg;
    cfg.mode = ConcurrencyTestMode::MultiModel;
    for (auto& entry : m_models) {
        MultiModelSpec spec;
        spec.harness = entry.harness.get();
        spec.count = entry.instanceCount;
        spec.modelName = qToUtf8(entry.name);
        cfg.models.push_back(spec);
    }

    int total = 0;
    for (const auto& s : cfg.models) total += s.count;
    logMessage(QString("INFO: 启动多型号并行 (型号 %1 种, 总实例 %2)...")
        .arg(cfg.models.size()).arg(total));
    startConcurrencyWorker(nullptr, cfg, 2); // 多型号并行
}

void MainWindow::runMultiThreadTest() {
    if (!requireSelectedModelCompiled(m_comboThreadModel, 1)) {
        return;
    }
    FleetModelEntry* entry = selectedTestModel(m_comboThreadModel);
    if (!entry || !entry->harness) return;

    ConcurrencyTestConfig cfg;
    cfg.mode = ConcurrencyTestMode::MultiThread;
    cfg.count = m_spnThreadCount->value();

    logMessage(QString("INFO: 启动型号「%1」多线程测试 (线程数=%2)...")
        .arg(entry->name).arg(cfg.count));
    startConcurrencyWorker(entry->harness.get(), cfg, 3); // 多线程
}

void MainWindow::onPerfProfileProgress(int step, int total, double timeMs, double memMB) {
    if (step % 2000 == 0 || step == total) {
        logMessage(QString("INFO: 压测进度: %1/%2 次 | 单次耗时: %3 ms | WorkingSet: %4 MB")
            .arg(step).arg(total).arg(timeMs, 0, 'f', 4).arg(memMB, 0, 'f', 2));
    }
}

void MainWindow::onPerfProfileFinished(const PerfProfileReport& report) {
    m_latestReport.perfReport = report;
    m_latestFleetReport.perfReport = report;
    m_pChartViewer->UpdatePerfCharts(report);
    m_pCentralTabs->setCurrentIndex(1); // 性能压测

    QString summary = QString(
        "压测结果: 平均耗时: %1 ms | 最大耗时: %2 ms | 抖动 StdDev: %3 ms | "
        "10k次内存增量: %4 MB | 实时性判定: <b>%5</b>")
        .arg(report.avgTimeMs, 0, 'f', 4)
        .arg(report.maxTimeMs, 0, 'f', 4)
        .arg(report.jitterMs, 0, 'f', 4)
        .arg(report.memoryLeakRateMBPer10k, 0, 'f', 2)
        .arg(qUtf8(report.realtimeVerdict));

    m_lblPerfSummary->setText(summary);

    m_latestReport.overallPass = (m_latestReport.peReport.overallPass &&
                                  m_latestReport.loadReport.isLoaded &&
                                  m_latestReport.perfReport.realtimeVerdict != "FAIL");

    updateStatusBadges();
    refreshReportBrowser();

    logMessage("SUCCESS: UserMain 性能压测执行完毕！");
}

void MainWindow::onConcurrencyFinished(const ConcurrencyTestReport& report) {
    const bool isMultiModel = (report.mode == ConcurrencyTestMode::MultiModel);

    if (isMultiModel) {
        m_latestReport.multiModelReport = report;
        m_latestFleetReport.multiModelReport = report;
        updateResultTable(m_tblMultiModelResults, report);
        m_lblMultiModelSummary->setText(QString("多型号并行汇总: <b>%1</b> — %2")
            .arg(qUtf8(report.verdict))
            .arg(qUtf8(report.summary)));
        m_pCentralTabs->setCurrentIndex(2); // 多型号并行
    } else {
        m_latestReport.multiThreadReport = report;
        m_latestFleetReport.multiThreadReport = report;
        updateResultTable(m_tblMultiThreadResults, report);
        m_lblMultiThreadSummary->setText(QString("多线程测试汇总: <b>%1</b> — %2")
            .arg(qUtf8(report.verdict))
            .arg(qUtf8(report.summary)));
        m_pCentralTabs->setCurrentIndex(3); // 多线程
    }

    if (report.verdict == "FAIL") {
        m_latestReport.overallPass = false;
        if (!m_latestFleetReport.modelReports.empty()) {
            m_latestFleetReport.overallPass = false;
        }
    }

    updateStatusBadges();
    refreshReportBrowser();

    logMessage(QString("SUCCESS: %1 完成，判定=%2")
        .arg(isMultiModel ? "多型号并行" : "多线程测试")
        .arg(qUtf8(report.verdict)));
}

void MainWindow::updatePeView(const PeAnalysisReport& pe) {
    m_lblPeArch->setText(QString("CPU 架构: %1%2")
        .arg(qUtf8(pe.architecture))
        .arg(pe.isArchMatch ? " (与宿主匹配)" : " (与宿主不匹配)"));
    m_lblPeCrt->setText(QString("CRT 链接方式: %1").arg(qUtf8(pe.crtLinkage)));
    if (!pe.filePath.empty()) {
        m_lblPeArch->setToolTip(qUtf8(pe.filePath));
        m_lblPeCrt->setToolTip(qUtf8(pe.filePath));
    }

    m_tblImports->setRowCount(0);
    for (const auto& dep : pe.importedDlls) {
        int row = m_tblImports->rowCount();
        m_tblImports->insertRow(row);
        m_tblImports->setItem(row, 0, new QTableWidgetItem(qUtf8(dep.name)));
        QTableWidgetItem* st = new QTableWidgetItem(dep.found ? "找到" : "缺失");
        if (!dep.found) {
            st->setForeground(QBrush(QColor("#dc2626")));
        }
        m_tblImports->setItem(row, 1, st);
        m_tblImports->setItem(row, 2, new QTableWidgetItem(qUtf8(dep.resolvedPath)));
    }

    m_tblExports->setRowCount(0);
    for (const auto& exp : pe.exportedSymbols) {
        int row = m_tblExports->rowCount();
        m_tblExports->insertRow(row);
        m_tblExports->setItem(row, 0, new QTableWidgetItem(qUtf8(exp.name)));
        m_tblExports->setItem(row, 1, new QTableWidgetItem(QString::number(exp.ordinal)));
        QString match = exp.isRequiredInterface ? "必需接口" : "普通导出";
        m_tblExports->setItem(row, 2, new QTableWidgetItem(match));
    }

    logMessage(QString("INFO: PE 视图已更新 — 依赖 %1 项, 导出 %2 个符号 [%3]")
        .arg(pe.importedDlls.size())
        .arg(pe.exportedSymbols.size())
        .arg(qUtf8(pe.filePath)));
}

void MainWindow::updateResultTable(QTableWidget* table, const ConcurrencyTestReport& report) {
    if (!table) return;
    table->setRowCount(0);
    const bool isMultiModel = (report.mode == ConcurrencyTestMode::MultiModel);
    for (const auto& tr : report.threadResults) {
        int row = table->rowCount();
        table->insertRow(row);
        QString idLabel;
        if (isMultiModel) {
            idLabel = QString("%1[%2]").arg(qUtf8(tr.modelName)).arg(tr.instanceId);
        } else {
            idLabel = QString("#%1").arg(tr.threadId);
        }
        table->setItem(row, 0, new QTableWidgetItem(idLabel));
        table->setItem(row, 1, new QTableWidgetItem(qUtf8(tr.randomSummary)));
        table->setItem(row, 2, new QTableWidgetItem(QString::number(tr.userReturnCode)));
        table->setItem(row, 3, new QTableWidgetItem(tr.exceptionOccurred ? "YES" : "NO"));
        table->setItem(row, 4, new QTableWidgetItem(qDecodeLog(tr.errorLog)));
    }
}

void MainWindow::updateStatusBadges() {
    auto setBadge = [](QLabel* lbl, const QString& prefix, const QString& status,
                       const QString& bgColor, const QString& fgColor, const QString& borderColor) {
        lbl->setText(prefix + ": " + status);
        lbl->setStyleSheet(QString(
            "QLabel { padding: 4px 12px; border-radius: 12px; font-weight: bold; font-size: 12px; "
            "background-color: %1; color: %2; border: 1px solid %3; }")
            .arg(bgColor, fgColor, borderColor));
    };

    int passedH = 0, totalH = 0;
    int passedL = 0, totalL = 0;
    int passedD = 0, totalD = 0;

    if (!m_latestFleetReport.modelReports.empty()) {
        for (const auto& mr : m_latestFleetReport.modelReports) {
            passedH += mr.passedHeaderCount;
            totalH += static_cast<int>(mr.headerReports.size());
            passedL += mr.passedLibCount;
            totalL += static_cast<int>(mr.libReports.size());
            passedD += mr.passedDllCount;
            totalD += static_cast<int>(mr.dllReports.size());
        }
    } else {
        passedH = m_latestDualReport.passedHeaderCount;
        totalH = static_cast<int>(m_latestDualReport.headerReports.size());
        passedL = m_latestDualReport.passedLibCount;
        totalL = static_cast<int>(m_latestDualReport.libReports.size());
        passedD = m_latestDualReport.passedDllCount;
        totalD = static_cast<int>(m_latestDualReport.dllReports.size());
    }

    if (totalH > 0) {
        QString statusStr = QString("%1 (%2/%3)").arg(passedH == totalH ? "PASS" : "FAIL").arg(passedH).arg(totalH);
        if (passedH == totalH) {
            setBadge(m_lblHeaderStatus, "头文件预检", statusStr, "#dcfce7", "#166534", "#86efac");
        } else {
            setBadge(m_lblHeaderStatus, "头文件预检", statusStr, "#fee2e2", "#991b1b", "#fca5a5");
        }
    } else {
        setBadge(m_lblHeaderStatus, "头文件预检", "N/A", "#e5eef7", "#003986", "#b0c4de");
    }

    if (totalL > 0) {
        QString statusStr = QString("%1 (%2/%3)").arg(passedL == totalL ? "PASS" : "FAIL").arg(passedL).arg(totalL);
        if (passedL == totalL) {
            setBadge(m_lblLibStatus, "LIB 库预检", statusStr, "#dcfce7", "#166534", "#86efac");
        } else {
            setBadge(m_lblLibStatus, "LIB 库预检", statusStr, "#fee2e2", "#991b1b", "#fca5a5");
        }
    } else {
        setBadge(m_lblLibStatus, "LIB 库预检", "N/A", "#e5eef7", "#003986", "#b0c4de");
    }

    if (totalD > 0) {
        QString statusStr = QString("%1 (%2/%3)").arg(passedD == totalD ? "PASS" : "FAIL").arg(passedD).arg(totalD);
        if (passedD == totalD) {
            setBadge(m_lblDllStatus, "DLL 动态库预检", statusStr, "#dcfce7", "#166534", "#86efac");
        } else {
            setBadge(m_lblDllStatus, "DLL 动态库预检", statusStr, "#fee2e2", "#991b1b", "#fca5a5");
        }
    } else {
        setBadge(m_lblDllStatus, "DLL 动态库预检", "N/A", "#e5eef7", "#003986", "#b0c4de");
    }
}

void MainWindow::exportReport() {
    const bool hasFleet = !m_latestFleetReport.modelReports.empty();
    if (!hasFleet && m_latestDualReport.packageDir.empty() && m_latestReport.dllPath.empty()) {
        QMessageBox::warning(this, "警告", "请先运行预检流程后再导出报告！");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(
        this, "保存预检报告 HTML", "Precheck_Report.html", "HTML Files (*.html)");
    if (savePath.isEmpty()) return;

    bool success = false;
    if (hasFleet) {
        success = ReportGenerator::SaveFleetReportToFile(m_latestFleetReport, qToUtf8(savePath));
    } else if (!m_latestDualReport.packageDir.empty()) {
        success = ReportGenerator::SaveDualReportToFile(m_latestDualReport, qToUtf8(savePath));
    } else {
        success = ReportGenerator::SaveReportToFile(m_latestReport, qToUtf8(savePath));
    }

    if (success) {
        QMessageBox::information(this, "成功", "预检报告已成功导出至:\n" + savePath);
        logMessage("SUCCESS: 报告已成功导出: " + savePath);
    } else {
        QMessageBox::critical(this, "错误", "导出预检报告失败！");
    }
}
