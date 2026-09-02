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
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QMetaType>
#include <QFont>
#include <QSignalBlocker>
#include <QAbstractItemView>
#include <QSizePolicy>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>

#include "ChartViewerWidget.h"
#include "TrajectoryViewWidget.h"
#include "../utils/QtEncoding.h"

#include <QProgressDialog>
#include <QEventLoop>
#include <QStyle>
#include <algorithm>

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
    QHBoxLayout* rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // --- Top Control Panel ---
    QGroupBox* grpTop = new QGroupBox("预检控制", this);
    QHBoxLayout* layoutTop = new QHBoxLayout(grpTop);

    m_btnRunPrecheck = new QPushButton("一键预检全部型号", this);
    m_btnRunPrecheck->setStyleSheet(
        "QPushButton { background-color: #0066cc; color: #ffffff; font-family: \"Microsoft YaHei UI\"; "
        "font-weight: bold; font-size: 13px; "
        "padding: 8px 18px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #ffd178; color: #002f86; } "
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

    // --- Guided workflow: always shows where the user is and what comes next ---
    QGroupBox* grpWorkflow = new QGroupBox("操作流程", this);
    QVBoxLayout* workflowLayout = new QVBoxLayout(grpWorkflow);
    QHBoxLayout* workflowStepsLayout = new QHBoxLayout();
    const QStringList workflowNames = {
        "1 添加型号", "2 选择模型包", "3 配置 UserMain",
        "4 编译型号", "5 执行测试", "6 查看报告"
    };
    for (const QString& name : workflowNames) {
        QLabel* step = new QLabel(name, grpWorkflow);
        step->setAlignment(Qt::AlignCenter);
        step->setProperty("workflowStep", true);
        step->setProperty("stepState", "pending");
        step->setMinimumHeight(32);
        workflowStepsLayout->addWidget(step, 1);
        m_workflowSteps.push_back(step);
    }
    m_lblWorkflowSummary = new QLabel(
        QStringLiteral("当前步骤：点击“添加型号”开始配置第三方模型。"), grpWorkflow);
    m_lblWorkflowSummary->setWordWrap(true);
    m_lblWorkflowSummary->setProperty("workflowSummary", true);
    workflowLayout->addLayout(workflowStepsLayout);
    workflowLayout->addWidget(m_lblWorkflowSummary);

    // --- Content: left models panel + right tabs ---
    QSplitter* splitterContent = new QSplitter(Qt::Horizontal, this);

    // ========== Far left: fixed full-height test navigation ==========
    QGroupBox* grpNavigation = new QGroupBox("功能导航", this);
    QVBoxLayout* navigationLayout = new QVBoxLayout(grpNavigation);
    m_listTestNavigation = new QListWidget(grpNavigation);
    m_listTestNavigation->addItem(QStringLiteral("头文件规范检查"));
    m_listTestNavigation->addItem(QStringLiteral("LIB 库文件检查"));
    m_listTestNavigation->addItem(QStringLiteral("DLL 文件与依赖检查"));
    m_listTestNavigation->addItem(QStringLiteral("DLL 接口与加载检查"));
    m_listTestNavigation->addItem(QStringLiteral("UserMain 性能压测"));
    m_listTestNavigation->addItem(QStringLiteral("内存泄漏监测"));
    m_listTestNavigation->addItem(QStringLiteral("运行轨迹查看"));
    m_listTestNavigation->addItem(QStringLiteral("多型号并行"));
    m_listTestNavigation->addItem(QStringLiteral("多线程稳定性"));
    m_listTestNavigation->addItem(QStringLiteral("查看报告"));
    m_listTestNavigation->setProperty("testNavigation", true);
    navigationLayout->addWidget(m_listTestNavigation, 1);

    // ========== Middle: fixed model list ==========
    QGroupBox* grpModels = new QGroupBox("型号列表", this);
    QVBoxLayout* leftLayout = new QVBoxLayout(grpModels);
    m_listModels = new QListWidget(this);
    leftLayout->addWidget(m_listModels, 1);
    QHBoxLayout* modelBtnRow = new QHBoxLayout();
    m_btnAddModel = new QPushButton("添加型号", this);
    m_btnRemoveModel = new QPushButton("删除", this);
    FitButtonText(m_btnAddModel);
    FitButtonText(m_btnRemoveModel);
    modelBtnRow->addWidget(m_btnAddModel);
    modelBtnRow->addWidget(m_btnRemoveModel);
    leftLayout->addLayout(modelBtnRow);

    // ========== Right: one scrollable workflow page ==========
    QGroupBox* rightPanel = new QGroupBox("型号与 UserMain 配置", this);
    m_modelSetupPanel = rightPanel;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(12, 12, 12, 12);

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
    m_lblLicenseHint->setStyleSheet("color: #60a5fa; font-size: 11px;");
    rightLayout->addLayout(formModel);
    rightLayout->addWidget(m_lblLicenseHint);

    m_lblPathStageStatus = new QLabel(
        QStringLiteral("步骤 2：请选择有效的模型包路径，随后将显示头文件和 UserMain 配置。"), this);
    m_lblPathStageStatus->setWordWrap(true);
    m_lblPathStageStatus->setProperty("pathStageStatus", true);
    rightLayout->addWidget(m_lblPathStageStatus);

    m_modelDetailPanel = new QWidget(grpModels);
    m_modelDetailPanel->setObjectName(QStringLiteral("modelDetailPanel"));
    QVBoxLayout* detailLayout = new QVBoxLayout(m_modelDetailPanel);
    detailLayout->setContentsMargins(10, 10, 10, 10);

    QHBoxLayout* hdrPickRow = new QHBoxLayout();
    QLabel* headerTitle = new QLabel("步骤 3.1：选择包含头文件（可多选）", this);
    headerTitle->setProperty("sectionTitle", true);
    hdrPickRow->addWidget(headerTitle);
    m_btnRefreshModelHeaders = new QPushButton("刷新列表", this);
    FitButtonText(m_btnRefreshModelHeaders);
    hdrPickRow->addWidget(m_btnRefreshModelHeaders);
    hdrPickRow->addStretch(1);
    detailLayout->addLayout(hdrPickRow);

    m_listHarnessHeaders = new QListWidget(this);
    m_listHarnessHeaders->setSelectionMode(QAbstractItemView::NoSelection);
    m_listHarnessHeaders->setMinimumHeight(100);
    m_listHarnessHeaders->setMaximumHeight(160);
    detailLayout->addWidget(m_listHarnessHeaders);

    QLabel* userMainTitle = new QLabel("步骤 3.2：编写 UserMain 函数体", this);
    userMainTitle->setProperty("sectionTitle", true);
    detailLayout->addWidget(userMainTitle);
    m_editUserMain = new CppCodeEditor(this);
    m_editUserMain->setMinimumHeight(180);
    detailLayout->addWidget(m_editUserMain, 2);

    QHBoxLayout* rndTitle = new QHBoxLayout();
    QLabel* randomTitle = new QLabel("步骤 3.3：配置随机变量（R.变量名）", this);
    randomTitle->setProperty("sectionTitle", true);
    rndTitle->addWidget(randomTitle);
    m_btnAddRandomVar = new QPushButton("添加变量", this);
    m_btnRemoveRandomVar = new QPushButton("删除选中", this);
    FitButtonText(m_btnAddRandomVar);
    FitButtonText(m_btnRemoveRandomVar);
    rndTitle->addStretch(1);
    rndTitle->addWidget(m_btnAddRandomVar);
    rndTitle->addWidget(m_btnRemoveRandomVar);
    detailLayout->addLayout(rndTitle);

    m_tblRandomVars = new QTableWidget(0, 5, this);
    m_tblRandomVars->setHorizontalHeaderLabels({ "启用", "变量名", "类型", "最小值", "最大值" });
    m_tblRandomVars->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblRandomVars->setMinimumHeight(100);
    m_tblRandomVars->setMaximumHeight(150);
    detailLayout->addWidget(m_tblRandomVars);

    QHBoxLayout* layoutCompile = new QHBoxLayout();
    m_btnCompileCurrent = new QPushButton("步骤 4：编译当前型号", this);
    m_btnCompileAll = new QPushButton("编译全部型号", this);
    FitButtonText(m_btnCompileCurrent);
    FitButtonText(m_btnCompileAll);
    layoutCompile->addWidget(m_btnCompileCurrent);
    layoutCompile->addWidget(m_btnCompileAll);
    layoutCompile->addStretch(1);
    detailLayout->addLayout(layoutCompile);

    m_lblHarnessStatus = new QLabel("Harness: 未编译", this);
    m_lblHarnessStatus->setWordWrap(true);
    detailLayout->addWidget(m_lblHarnessStatus);
    rightLayout->addWidget(m_modelDetailPanel, 1);

    // ========== Right tabs ==========
    m_pCentralTabs = new QTabWidget(this);

    // Tab 0: Header file convention check
    QWidget* tabHeader = new QWidget(this);
    QVBoxLayout* layoutTabHeader = new QVBoxLayout(tabHeader);
    QLabel* headerHint = new QLabel(
        QStringLiteral("检查头文件编码、extern \"C\"、导出声明和接口原型。"), tabHeader);
    headerHint->setProperty("pageHint", true);
    layoutTabHeader->addWidget(headerHint);
    QHBoxLayout* layoutHeaderPick = new QHBoxLayout();
    layoutHeaderPick->addWidget(new QLabel(QStringLiteral("型号:"), tabHeader));
    m_comboHeaderModel = new QComboBox(tabHeader);
    layoutHeaderPick->addWidget(m_comboHeaderModel);
    layoutHeaderPick->addWidget(new QLabel(QStringLiteral("头文件:"), tabHeader));
    m_comboHeaderFile = new QComboBox(tabHeader);
    m_comboHeaderFile->setMinimumWidth(320);
    layoutHeaderPick->addWidget(m_comboHeaderFile, 1);
    m_btnCheckHeader = new QPushButton(QStringLiteral("检查本项"), tabHeader);
    FitButtonText(m_btnCheckHeader);
    layoutHeaderPick->addWidget(m_btnCheckHeader);
    layoutTabHeader->addLayout(layoutHeaderPick);
    m_lblHeaderResult = new QLabel(QStringLiteral("检查结果: 尚未执行"), tabHeader);
    layoutTabHeader->addWidget(m_lblHeaderResult);
    m_tblHeaderFunctions = new QTableWidget(0, 2, tabHeader);
    m_tblHeaderFunctions->setHorizontalHeaderLabels({ QStringLiteral("接口函数"), QStringLiteral("完整声明") });
    m_tblHeaderFunctions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblHeaderFunctions->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layoutTabHeader->addWidget(m_tblHeaderFunctions, 1);

    // Tab 1: LIB file check
    QWidget* tabLib = new QWidget(this);
    QVBoxLayout* layoutTabLib = new QVBoxLayout(tabLib);
    QLabel* libHint = new QLabel(
        QStringLiteral("检查 LIB 文件架构、库类型和包含的接口符号。"), tabLib);
    libHint->setProperty("pageHint", true);
    layoutTabLib->addWidget(libHint);
    QHBoxLayout* layoutLibPick = new QHBoxLayout();
    layoutLibPick->addWidget(new QLabel(QStringLiteral("型号:"), tabLib));
    m_comboLibModel = new QComboBox(tabLib);
    layoutLibPick->addWidget(m_comboLibModel);
    layoutLibPick->addWidget(new QLabel(QStringLiteral("LIB 文件:"), tabLib));
    m_comboLibFile = new QComboBox(tabLib);
    m_comboLibFile->setMinimumWidth(320);
    layoutLibPick->addWidget(m_comboLibFile, 1);
    m_btnCheckLib = new QPushButton(QStringLiteral("检查本项"), tabLib);
    FitButtonText(m_btnCheckLib);
    layoutLibPick->addWidget(m_btnCheckLib);
    layoutTabLib->addLayout(layoutLibPick);
    m_lblLibResult = new QLabel(QStringLiteral("检查结果: 尚未执行"), tabLib);
    layoutTabLib->addWidget(m_lblLibResult);
    m_tblLibSymbols = new QTableWidget(0, 1, tabLib);
    m_tblLibSymbols->setHorizontalHeaderLabels({ QStringLiteral("LIB 中发现的符号") });
    m_tblLibSymbols->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblLibSymbols->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layoutTabLib->addWidget(m_tblLibSymbols, 1);

    // Tab 2: DLL file structure and dependency check
    QWidget* tabPe = new QWidget(this);
    QVBoxLayout* layoutTabPe = new QVBoxLayout(tabPe);
    m_lblPePageHint = new QLabel(
        QStringLiteral("检查 DLL 是 32/64 位、运行库类型，以及它依赖的其他 DLL 是否齐全。"), tabPe);
    m_lblPePageHint->setWordWrap(true);
    m_lblPePageHint->setProperty("pageHint", true);
    layoutTabPe->addWidget(m_lblPePageHint);
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
    m_btnCheckDllFile = new QPushButton(QStringLiteral("检查本项"), tabPe);
    FitButtonText(m_btnCheckDllFile);
    layoutPePick->addWidget(m_btnCheckDllFile);
    layoutTabPe->addLayout(layoutPePick);

    QHBoxLayout* layoutPeInfo = new QHBoxLayout();
    m_lblPeArch = new QLabel("CPU 架构: N/A", this);
    m_lblPeCrt = new QLabel("运行库类型: N/A", this);
    layoutPeInfo->addWidget(m_lblPeArch);
    layoutPeInfo->addWidget(m_lblPeCrt);
    layoutPeInfo->addStretch(1);

    QGroupBox* grpImports = new QGroupBox("DLL 依赖文件检查", this);
    QVBoxLayout* lImp = new QVBoxLayout(grpImports);
    m_tblImports = new QTableWidget(0, 3, this);
    m_tblImports->setHorizontalHeaderLabels({ "依赖 DLL 名称", "状态", "解析路径" });
    m_tblImports->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblImports->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lImp->addWidget(m_tblImports);

    layoutTabPe->addLayout(layoutPeInfo);
    layoutTabPe->addWidget(grpImports, 1);

    // Tab 3: DLL exported interfaces and safe loading
    QWidget* tabLoad = new QWidget(this);
    QVBoxLayout* layoutTabLoad = new QVBoxLayout(tabLoad);
    m_lblLoadPageHint = new QLabel(
        QStringLiteral("检查 DLL 是否提供所需接口，并验证程序能否安全加载该 DLL。"), tabLoad);
    m_lblLoadPageHint->setWordWrap(true);
    m_lblLoadPageHint->setProperty("pageHint", true);
    layoutTabLoad->addWidget(m_lblLoadPageHint);

    QHBoxLayout* layoutLoadPick = new QHBoxLayout();
    layoutLoadPick->addWidget(new QLabel("型号:", tabLoad));
    m_comboLoadModel = new QComboBox(tabLoad);
    m_comboLoadModel->setMinimumWidth(160);
    m_comboLoadModel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layoutLoadPick->addWidget(m_comboLoadModel);
    layoutLoadPick->addWidget(new QLabel("DLL 路径:", tabLoad));
    m_comboLoadDll = new QComboBox(tabLoad);
    m_comboLoadDll->setMinimumWidth(280);
    m_comboLoadDll->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layoutLoadPick->addWidget(m_comboLoadDll, 1);
    m_btnCheckDllLoad = new QPushButton(QStringLiteral("检查本项"), tabLoad);
    FitButtonText(m_btnCheckDllLoad);
    layoutLoadPick->addWidget(m_btnCheckDllLoad);
    layoutTabLoad->addLayout(layoutLoadPick);

    QHBoxLayout* layoutLoadInfo = new QHBoxLayout();
    m_lblLoadStatus = new QLabel(QStringLiteral("加载状态: 尚未执行预检"), tabLoad);
    m_lblLoadApi = new QLabel(QStringLiteral("接口绑定: N/A"), tabLoad);
    layoutLoadInfo->addWidget(m_lblLoadStatus);
    layoutLoadInfo->addWidget(m_lblLoadApi);
    layoutLoadInfo->addStretch(1);
    layoutTabLoad->addLayout(layoutLoadInfo);

    QGroupBox* grpExports = new QGroupBox("DLL 对外接口检查", tabLoad);
    QVBoxLayout* lExp = new QVBoxLayout(grpExports);
    m_tblExports = new QTableWidget(0, 3, tabLoad);
    m_tblExports->setHorizontalHeaderLabels({ "导出函数名", "Ordinal 序号", "匹配状态" });
    m_tblExports->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblExports->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lExp->addWidget(m_tblExports);

    layoutTabLoad->addWidget(grpExports, 1);

    // Tab 4: performance / memory / trajectory (navigation selects a dedicated view)
    QWidget* tabPerf = new QWidget(this);
    QVBoxLayout* layoutTabPerf = new QVBoxLayout(tabPerf);
    m_lblPerfPageHint = new QLabel(
        QStringLiteral("请先添加型号并完成编译，随后可选择已编译型号执行性能、内存和轨迹测试。"), tabPerf);
    m_lblPerfPageHint->setWordWrap(true);
    m_lblPerfPageHint->setProperty("pageHint", true);
    layoutTabPerf->addWidget(m_lblPerfPageHint);
    QHBoxLayout* layoutPerfModel = new QHBoxLayout();
    layoutPerfModel->addWidget(new QLabel("型号:", tabPerf));
    m_comboStressModel = new QComboBox(this);
    m_comboStressModel->setMinimumWidth(180);
    m_comboStressModel->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layoutPerfModel->addWidget(m_comboStressModel);
    layoutPerfModel->addStretch(1);
    layoutTabPerf->addLayout(layoutPerfModel);

    m_perfOptionsPanel = new QWidget(tabPerf);
    QHBoxLayout* layoutPerfCtrl = new QHBoxLayout(m_perfOptionsPanel);
    layoutPerfCtrl->setContentsMargins(0, 0, 0, 0);
    layoutPerfCtrl->addWidget(new QLabel("重复次数:", m_perfOptionsPanel));
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
    layoutPerfCtrl->addStretch(1);
    layoutTabPerf->addWidget(m_perfOptionsPanel);

    m_btnRunTrajectory = new QPushButton("试跑并绘制轨迹", this);
    FitButtonText(m_btnRunTrajectory);
    layoutTabPerf->addWidget(m_btnRunTrajectory, 0, Qt::AlignLeft);

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
    m_trajectoryPanel = new QWidget(this);
    QVBoxLayout* trajLayout = new QVBoxLayout(m_trajectoryPanel);
    trajLayout->setContentsMargins(0, 0, 0, 0);
    trajLayout->addWidget(m_lblTrajOut);
    QSplitter* splitterTraj = new QSplitter(Qt::Horizontal, m_trajectoryPanel);
    splitterTraj->addWidget(m_pTrajectoryView);
    QWidget* tblWrap = new QWidget(m_trajectoryPanel);
    QVBoxLayout* tblLayout = new QVBoxLayout(tblWrap);
    tblLayout->setContentsMargins(0, 0, 0, 0);
    tblLayout->addWidget(new QLabel(QStringLiteral("路径点经纬度"), tblWrap));
    tblLayout->addWidget(m_tblTrajectoryPoints, 1);
    splitterTraj->addWidget(tblWrap);
    splitterTraj->setStretchFactor(0, 3);
    splitterTraj->setStretchFactor(1, 2);
    trajLayout->addWidget(splitterTraj, 1);
    splitterPerf->addWidget(m_trajectoryPanel);
    splitterPerf->setStretchFactor(0, 3);
    splitterPerf->setStretchFactor(1, 2);

    layoutTabPerf->addWidget(m_lblPerfSummary);
    layoutTabPerf->addWidget(splitterPerf, 1);

    // Tab 2: Multi-model
    QWidget* tabMultiModel = new QWidget(this);
    QVBoxLayout* layoutTabMultiModel = new QVBoxLayout(tabMultiModel);
    m_lblMultiModelPageHint = new QLabel(
        "在左侧配置并编译各型号后，在此设置并行实例数并一起跑。", this);
    m_lblMultiModelPageHint->setWordWrap(true);
    m_lblMultiModelPageHint->setProperty("pageHint", true);
    layoutTabMultiModel->addWidget(m_lblMultiModelPageHint);

    m_tblFleetCounts = new QTableWidget(0, 4, this);
    m_tblFleetCounts->setHorizontalHeaderLabels({ "名称", "模型包路径", "编译状态", "实例数" });
    m_tblFleetCounts->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tblFleetCounts->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
    m_tblMultiModelResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layoutTabMultiModel->addWidget(m_lblMultiModelSummary);
    layoutTabMultiModel->addWidget(m_tblMultiModelResults, 1);

    // Tab 3: Multi-thread
    QWidget* tabMultiThr = new QWidget(this);
    QVBoxLayout* layoutTabMultiThr = new QVBoxLayout(tabMultiThr);
    m_lblMultiThreadPageHint = new QLabel(
        QStringLiteral("请先添加型号并完成编译；型号下拉框仅显示已编译型号。"), tabMultiThr);
    m_lblMultiThreadPageHint->setWordWrap(true);
    m_lblMultiThreadPageHint->setProperty("pageHint", true);
    layoutTabMultiThr->addWidget(m_lblMultiThreadPageHint);
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
    m_tblMultiThreadResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layoutTabMultiThr->addWidget(m_lblMultiThreadSummary);
    layoutTabMultiThr->addWidget(m_tblMultiThreadResults, 1);

    // Tab 4: Report
    QWidget* tabReport = new QWidget(this);
    QVBoxLayout* layoutTabReport = new QVBoxLayout(tabReport);
    m_pReportBrowser = new QTextBrowser(this);
    layoutTabReport->addWidget(m_pReportBrowser);

    m_pCentralTabs->addTab(tabHeader, "头文件规范检查");
    m_pCentralTabs->addTab(tabLib, "LIB 库文件检查");
    m_pCentralTabs->addTab(tabPe, "DLL 文件与依赖检查");
    m_pCentralTabs->addTab(tabLoad, "DLL 接口与加载检查");
    m_pCentralTabs->addTab(tabPerf, "性能 / 内存 / 轨迹");
    m_pCentralTabs->addTab(tabMultiModel, "多型号并行");
    m_pCentralTabs->addTab(tabMultiThr, "多线程稳定性");
    m_pCentralTabs->addTab(tabReport, "预检报告");

    QWidget* workflowPage = new QWidget(this);
    workflowPage->setObjectName(QStringLiteral("workflowPage"));
    QVBoxLayout* workflowPageLayout = new QVBoxLayout(workflowPage);
    workflowPageLayout->setContentsMargins(10, 10, 10, 10);
    workflowPageLayout->setSpacing(12);

    QLabel* emptyState = new QLabel(
        QStringLiteral("<h2>开始预检</h2>"
                       "<p>请先点击左侧“添加型号”。添加后将在这里依次显示：</p>"
                       "<p><b>选择模型包 → 选择头文件 → 编写 UserMain → 编译 → 执行测试 → 查看报告</b></p>"),
        workflowPage);
    emptyState->setAlignment(Qt::AlignCenter);
    emptyState->setWordWrap(true);
    emptyState->setTextFormat(Qt::RichText);
    emptyState->setMinimumHeight(260);
    emptyState->setProperty("emptyWorkflow", true);
    m_emptyWorkflowPanel = emptyState;

    m_testSection = new QWidget(workflowPage);
    m_testSection->setObjectName(QStringLiteral("testSection"));
    QVBoxLayout* testSectionLayout = new QVBoxLayout(m_testSection);
    testSectionLayout->setContentsMargins(0, 0, 0, 0);
    testSectionLayout->setSpacing(10);
    m_testSectionTitle = new QLabel(
        QStringLiteral("测试工作区（请从左侧“测试导航”选择）"), m_testSection);
    m_testSectionTitle->setProperty("pageSectionTitle", true);
    testSectionLayout->addWidget(m_testSectionTitle);
    m_pCentralTabs->setMinimumHeight(620);
    m_pCentralTabs->tabBar()->hide();
    testSectionLayout->addWidget(m_pCentralTabs);

    workflowPageLayout->addWidget(m_emptyWorkflowPanel);
    workflowPageLayout->addWidget(m_modelSetupPanel);
    workflowPageLayout->addWidget(m_testSection);
    workflowPageLayout->addStretch(1);

    m_workflowScroll = new QScrollArea(this);
    m_workflowScroll->setObjectName(QStringLiteral("workflowScroll"));
    m_workflowScroll->setWidgetResizable(true);
    m_workflowScroll->setFrameShape(QFrame::NoFrame);
    m_workflowScroll->setWidget(workflowPage);

    grpNavigation->setFixedWidth(205);
    grpModels->setMinimumWidth(210);
    grpModels->setMaximumWidth(270);
    splitterContent->addWidget(grpModels);
    splitterContent->addWidget(m_workflowScroll);
    splitterContent->setStretchFactor(0, 0);
    splitterContent->setStretchFactor(1, 1);
    splitterContent->setSizes({ 235, 1000 });

    m_pLogConsole = new LogConsoleWidget(this);

    QSplitter* splitterMain = new QSplitter(Qt::Vertical, this);
    splitterMain->addWidget(splitterContent);
    splitterMain->addWidget(m_pLogConsole);
    splitterMain->setStretchFactor(0, 4);
    splitterMain->setStretchFactor(1, 1);

    QWidget* mainWorkspace = new QWidget(centralWidget);
    QVBoxLayout* workspaceLayout = new QVBoxLayout(mainWorkspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(8);
    workspaceLayout->addWidget(grpTop);
    workspaceLayout->addLayout(layoutBadges);
    workspaceLayout->addWidget(grpWorkflow);
    workspaceLayout->addWidget(splitterMain, 1);

    rootLayout->addWidget(grpNavigation);
    rootLayout->addWidget(mainWorkspace, 1);

    // --- Connect Signals ---
    connect(m_btnRunPrecheck, &QPushButton::clicked, this, &MainWindow::runFullPrecheck);
    connect(m_btnExportReport, &QPushButton::clicked, this, &MainWindow::exportReport);
    connect(m_btnAddModel, &QPushButton::clicked, this, &MainWindow::addModel);
    connect(m_btnRemoveModel, &QPushButton::clicked, this, &MainWindow::removeModel);
    connect(m_listModels, &QListWidget::currentRowChanged, this, &MainWindow::onModelSelectionChanged);
    connect(m_listModels, &QListWidget::itemClicked, this, [this](QListWidgetItem*) {
        QSignalBlocker blocker(m_listTestNavigation);
        m_listTestNavigation->setCurrentRow(-1);
        updateWorkflowUi();
        m_workflowScroll->ensureWidgetVisible(m_modelSetupPanel, 0, 10);
    });
    connect(m_listTestNavigation, &QListWidget::currentRowChanged,
            this, &MainWindow::onTestNavigationChanged);
    connect(m_btnBrowseModelPackage, &QPushButton::clicked, this, &MainWindow::browseCurrentModelPackage);
    connect(m_editModelPackage, &QLineEdit::editingFinished, this, [this]() {
        if (m_currentModelIndex < 0
            || m_currentModelIndex >= static_cast<int>(m_models.size())) return;
        FleetModelEntry& entry = m_models[static_cast<size_t>(m_currentModelIndex)];
        const QString entered = QDir::toNativeSeparators(m_editModelPackage->text().trimmed());
        if (entry.packageDir != entered) {
            entry.harness.reset();
            entry.status = QStringLiteral("未编译");
            m_latestFleetReport = FleetSessionReport();
        }
        saveEditorsToCurrentModel();
        if (isModelPathValid(entry)) {
            refreshCurrentModelHeaders();
        }
        updateWorkflowUi();
    });
    connect(m_btnRefreshModelHeaders, &QPushButton::clicked, this, &MainWindow::refreshCurrentModelHeaders);
    connect(m_btnCompileCurrent, &QPushButton::clicked, this, &MainWindow::compileCurrentModel);
    connect(m_btnCompileAll, &QPushButton::clicked, this, &MainWindow::compileAllModels);
    connect(m_btnAddRandomVar, &QPushButton::clicked, this, &MainWindow::addRandomVarRow);
    connect(m_btnRemoveRandomVar, &QPushButton::clicked, this, &MainWindow::removeRandomVarRow);
    connect(m_btnRunStress, &QPushButton::clicked, this, &MainWindow::runStressTestOnly);
    connect(m_btnRunTrajectory, &QPushButton::clicked, this, &MainWindow::runTrajectoryPreview);
    connect(m_btnRunMultiModel, &QPushButton::clicked, this, &MainWindow::runMultiModelTest);
    connect(m_btnRunMultiThread, &QPushButton::clicked, this, &MainWindow::runMultiThreadTest);
    connect(m_btnCheckDllFile, &QPushButton::clicked, this, &MainWindow::runDllFileCheckOnly);
    connect(m_btnCheckDllLoad, &QPushButton::clicked, this, &MainWindow::runDllLoadCheckOnly);
    connect(m_btnCheckHeader, &QPushButton::clicked, this, &MainWindow::runHeaderCheckOnly);
    connect(m_btnCheckLib, &QPushButton::clicked, this, &MainWindow::runLibCheckOnly);
    connect(m_comboHeaderModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                fillPackageFileSelector(m_comboHeaderModel, m_comboHeaderFile,
                                        { QStringLiteral("*.h"), QStringLiteral("*.hpp") });
            });
    connect(m_comboLibModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                fillPackageFileSelector(m_comboLibModel, m_comboLibFile,
                                        { QStringLiteral("*.lib") });
            });
    connect(m_comboPeModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPeModelChanged);
    connect(m_comboPeDll, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPeDllChanged);
    connect(m_comboLoadModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLoadModelChanged);
    connect(m_comboLoadDll, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLoadDllChanged);

    m_listTestNavigation->setCurrentRow(-1);
    setEditorsEnabled(false);
    updateWorkflowUi();
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

bool MainWindow::isModelPathValid(const FleetModelEntry& entry) const {
    const QString path = entry.packageDir.trimmed();
    return !path.isEmpty() && QDir(path).exists();
}

bool MainWindow::isModelConfigured(const FleetModelEntry& entry) const {
    return isModelPathValid(entry)
        && !entry.headerPaths.isEmpty()
        && !entry.userMainBody.trimmed().isEmpty();
}

bool MainWindow::isModelCompiled(const FleetModelEntry& entry) const {
    return entry.harness && entry.harness->IsLoaded();
}

bool MainWindow::allModelsCompiled() const {
    if (m_models.empty()) return false;
    for (const FleetModelEntry& entry : m_models) {
        if (!isModelCompiled(entry)) return false;
    }
    return true;
}

void MainWindow::setEditorsEnabled(bool on) {
    m_editModelName->setEnabled(on);
    m_editModelPackage->setEnabled(on);
    m_btnBrowseModelPackage->setEnabled(on);
    if (!on) {
        m_btnRefreshModelHeaders->setEnabled(false);
        m_listHarnessHeaders->setEnabled(false);
        m_editUserMain->setEnabled(false);
        m_tblRandomVars->setEnabled(false);
        m_btnAddRandomVar->setEnabled(false);
        m_btnRemoveRandomVar->setEnabled(false);
        m_btnCompileCurrent->setEnabled(false);
    }
}

void MainWindow::updateWorkflowUi() {
    const bool hasSelection = m_currentModelIndex >= 0
        && m_currentModelIndex < static_cast<int>(m_models.size());
    const FleetModelEntry* current = hasSelection
        ? &m_models[static_cast<size_t>(m_currentModelIndex)] : nullptr;
    const bool pathValid = current && isModelPathValid(*current);
    const bool configured = current && isModelConfigured(*current);
    const bool currentCompiled = current && isModelCompiled(*current);
    const bool anyCompiled = std::any_of(m_models.begin(), m_models.end(),
        [this](const FleetModelEntry& entry) { return isModelCompiled(entry); });
    const bool everyCompiled = allModelsCompiled();
    const bool hasPrecheck = !m_latestFleetReport.modelReports.empty()
        && m_latestFleetReport.modelReports.size() == m_models.size();
    const int navigationRow = m_listTestNavigation->currentRow();
    const bool testPageSelected = navigationRow >= 0;
    const bool testPageAccessible =
        navigationRow == 9
        || (navigationRow >= 0 && navigationRow <= 3 && !m_models.empty())
        || (navigationRow >= 4 && navigationRow <= 8 && anyCompiled);
    const bool navigationBlocked = testPageSelected && !testPageAccessible;

    if (navigationBlocked) {
        m_emptyWorkflowPanel->setText(navigationRow <= 3
            ? QStringLiteral("<h2>请添加型号</h2>")
            : QStringLiteral("<h2>请添加型号并编译</h2>"));
    } else {
        m_emptyWorkflowPanel->setText(
            QStringLiteral("<h2>开始预检</h2>"
                           "<p>请先点击左侧“添加型号”。添加后将在这里依次显示：</p>"
                           "<p><b>选择模型包 → 选择头文件 → 编写 UserMain → 编译 → 执行测试 → 查看报告</b></p>"));
    }

    setEditorsEnabled(hasSelection);
    m_emptyWorkflowPanel->setVisible(navigationBlocked || (!hasSelection && !testPageSelected));
    m_modelSetupPanel->setVisible(hasSelection && !testPageSelected);
    m_modelDetailPanel->setVisible(pathValid);
    m_testSection->setVisible(testPageSelected && testPageAccessible);
    m_btnRefreshModelHeaders->setEnabled(pathValid);
    m_listHarnessHeaders->setEnabled(pathValid);
    m_editUserMain->setEnabled(pathValid);
    m_tblRandomVars->setEnabled(pathValid);
    m_btnAddRandomVar->setEnabled(pathValid);
    m_btnRemoveRandomVar->setEnabled(pathValid);
    m_btnCompileCurrent->setEnabled(configured);

    bool allConfigured = !m_models.empty();
    for (const FleetModelEntry& entry : m_models) {
        if (!isModelConfigured(entry)) {
            allConfigured = false;
            break;
        }
    }
    m_btnCompileAll->setEnabled(allConfigured);
    m_btnRunPrecheck->setEnabled(!m_models.empty());
    m_btnExportReport->setEnabled(true);

    m_comboStressModel->setEnabled(anyCompiled);
    m_spnSteps->setEnabled(anyCompiled);
    m_comboHz->setEnabled(anyCompiled);
    m_btnRunStress->setEnabled(anyCompiled);
    m_btnRunTrajectory->setEnabled(anyCompiled);
    m_lblPerfPageHint->setVisible(!anyCompiled);
    m_lblPerfPageHint->setText(m_models.empty()
        ? QStringLiteral("请先添加型号并完成编译，才能执行性能、内存和轨迹测试。")
        : QStringLiteral("当前没有已编译型号。请先在型号配置页完成编译。"));

    m_tblFleetCounts->setEnabled(everyCompiled);
    m_btnRunMultiModel->setEnabled(everyCompiled);
    m_lblMultiModelPageHint->setVisible(!everyCompiled);
    m_lblMultiModelPageHint->setText(m_models.empty()
        ? QStringLiteral("请先添加型号并完成编译，才能执行多型号并行测试。")
        : QStringLiteral("多型号并行要求所有已添加型号均编译成功。"));

    m_comboThreadModel->setEnabled(anyCompiled);
    m_spnThreadCount->setEnabled(anyCompiled);
    m_btnRunMultiThread->setEnabled(anyCompiled);
    m_lblMultiThreadPageHint->setVisible(!anyCompiled);
    m_lblMultiThreadPageHint->setText(m_models.empty()
        ? QStringLiteral("请先添加型号并完成编译，才能执行多线程稳定性测试。")
        : QStringLiteral("当前没有已编译型号；型号下拉框只显示编译成功的型号。"));

    if (hasSelection) {
        if (pathValid) {
            m_lblPathStageStatus->setText(
                QStringLiteral("模型包路径有效。请继续选择头文件、编写 UserMain 并完成编译。"));
            m_lblPathStageStatus->setProperty("pathValid", true);
        } else {
            m_lblPathStageStatus->setText(
                QStringLiteral("步骤 2：请选择有效的模型包路径，随后将显示头文件和 UserMain 配置。"));
            m_lblPathStageStatus->setProperty("pathValid", false);
        }
    } else {
        m_lblPathStageStatus->setText(QStringLiteral("请先点击“添加型号”。"));
        m_lblPathStageStatus->setProperty("pathValid", false);
    }
    m_lblPathStageStatus->style()->unpolish(m_lblPathStageStatus);
    m_lblPathStageStatus->style()->polish(m_lblPathStageStatus);

    int activeStep = 0;
    QString summary = QStringLiteral("当前步骤：点击“添加型号”开始配置第三方模型。");
    if (!m_models.empty()) {
        activeStep = 1;
        summary = QStringLiteral("当前步骤：为选中的型号选择有效模型包路径。");
        if (pathValid) {
            activeStep = 2;
            summary = QStringLiteral("当前步骤：选择头文件、编写 UserMain，并配置随机变量。");
        }
        if (configured) {
            activeStep = 3;
            summary = QStringLiteral("当前步骤：编译当前型号；多型号时请确保全部型号编译成功。");
        }
        if (everyCompiled) {
            activeStep = 4;
            summary = hasPrecheck
                ? QStringLiteral("可从左侧测试导航继续运行各测试项，或直接查看报告。")
                : QStringLiteral("编译已完成：可从左侧进入测试项，也可执行一键预检。");
        } else if (currentCompiled) {
            summary = QStringLiteral("当前型号已编译；请继续编译其余型号，或点击“编译全部型号”。");
        }
        if (hasPrecheck) {
            activeStep = 5;
            summary = QStringLiteral("预检已完成：可查看测试明细、运行专项测试并导出报告。");
        }
    }
    m_lblWorkflowSummary->setText(summary);
    for (int i = 0; i < static_cast<int>(m_workflowSteps.size()); ++i) {
        QLabel* label = m_workflowSteps[static_cast<size_t>(i)];
        const char* state = i < activeStep ? "done" : (i == activeStep ? "active" : "pending");
        label->setProperty("stepState", state);
        label->style()->unpolish(label);
        label->style()->polish(label);
    }

    if (m_models.empty()) {
        m_lblPePageHint->setText(QStringLiteral("请先添加型号。"));
        m_lblPePageHint->show();
    } else if (m_latestFleetReport.modelReports.empty()) {
        m_lblPePageHint->setText(
            QStringLiteral("已自动列出型号包中的 DLL。点击顶部“一键预检全部型号”后可查看文件结构与依赖检查结果。"));
        m_lblPePageHint->show();
    } else {
        m_lblPePageHint->hide();
    }
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
        updateWorkflowUi();
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
    updateWorkflowUi();
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
            if (isModelCompiled(e)) {
                combo->addItem(QString("%1 (已编译)").arg(e.name), i);
            }
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
    auto refillModels = [this](QComboBox* combo) {
        const int previous = combo->currentData().toInt();
        QSignalBlocker blocker(combo);
        combo->clear();
        for (int i = 0; i < static_cast<int>(m_models.size()); ++i) {
            combo->addItem(m_models[static_cast<size_t>(i)].name, i);
        }
        int restore = combo->findData(previous);
        combo->setCurrentIndex(restore >= 0 ? restore : (combo->count() > 0 ? 0 : -1));
    };

    refillModels(m_comboHeaderModel);
    refillModels(m_comboLibModel);
    refillModels(m_comboPeModel);
    refillModels(m_comboLoadModel);
    fillPackageFileSelector(m_comboHeaderModel, m_comboHeaderFile,
                            { QStringLiteral("*.h"), QStringLiteral("*.hpp") });
    fillPackageFileSelector(m_comboLibModel, m_comboLibFile,
                            { QStringLiteral("*.lib") });
    onPeModelChanged(m_comboPeModel->currentIndex());
    onLoadModelChanged(m_comboLoadModel->currentIndex());
}

void MainWindow::onPeModelChanged(int /*index*/) {
    fillDllSelector(m_comboPeModel, m_comboPeDll);
    onPeDllChanged(m_comboPeDll->currentIndex());
}

void MainWindow::onPeDllChanged(int /*index*/) {
    const CombinedPrecheckReport* rep = selectedPeDllReport();
    if (rep) {
        updatePeView(rep->peReport);
    } else {
        m_lblPeArch->setText(QStringLiteral("CPU 架构: 等待执行预检"));
        m_lblPeCrt->setText(QStringLiteral("运行库类型: 等待执行预检"));
        m_tblImports->setRowCount(0);
    }
}

void MainWindow::fillDllSelector(QComboBox* modelCombo, QComboBox* dllCombo) {
    if (!modelCombo || !dllCombo) return;
    const QString previous = dllCombo->currentData().toString();
    const int modelIndex = modelCombo->currentData().toInt();
    QStringList dllPaths;

    if (modelIndex >= 0 && modelIndex < static_cast<int>(m_models.size())) {
        const QString packageDir = m_models[static_cast<size_t>(modelIndex)].packageDir;
        if (!packageDir.isEmpty() && QDir(packageDir).exists()) {
            QDirIterator it(packageDir, QStringList() << QStringLiteral("*.dll"),
                QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                dllPaths.push_back(QDir::toNativeSeparators(QFileInfo(it.next()).absoluteFilePath()));
            }
        }
    }
    if (modelIndex >= 0
        && modelIndex < static_cast<int>(m_latestFleetReport.modelReports.size())) {
        const auto& report = m_latestFleetReport.modelReports[static_cast<size_t>(modelIndex)];
        for (const auto& dll : report.dllReports) {
            const QString path = qUtf8(dll.dllPath.empty() ? dll.peReport.filePath : dll.dllPath);
            if (!path.isEmpty() && !dllPaths.contains(path, Qt::CaseInsensitive)) {
                dllPaths.push_back(path);
            }
        }
    }

    dllPaths.removeDuplicates();
    dllPaths.sort(Qt::CaseInsensitive);
    QSignalBlocker blocker(dllCombo);
    dllCombo->clear();
    for (const QString& path : dllPaths) {
        dllCombo->addItem(path, path);
    }
    int restore = dllCombo->findData(previous);
    dllCombo->setCurrentIndex(restore >= 0 ? restore : (dllCombo->count() > 0 ? 0 : -1));
}

void MainWindow::fillPackageFileSelector(
    QComboBox* modelCombo, QComboBox* fileCombo, const QStringList& nameFilters) {
    if (!modelCombo || !fileCombo) return;
    const QString previous = fileCombo->currentData().toString();
    const int modelIndex = modelCombo->currentData().toInt();
    QStringList paths;
    if (modelIndex >= 0 && modelIndex < static_cast<int>(m_models.size())) {
        const QString packageDir = m_models[static_cast<size_t>(modelIndex)].packageDir;
        if (!packageDir.isEmpty() && QDir(packageDir).exists()) {
            QDirIterator it(packageDir, nameFilters, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                paths.push_back(QDir::toNativeSeparators(QFileInfo(it.next()).absoluteFilePath()));
            }
        }
    }
    paths.removeDuplicates();
    paths.sort(Qt::CaseInsensitive);
    QSignalBlocker blocker(fileCombo);
    fileCombo->clear();
    for (const QString& path : paths) fileCombo->addItem(path, path);
    const int restore = fileCombo->findData(previous);
    fileCombo->setCurrentIndex(restore >= 0 ? restore : (fileCombo->count() > 0 ? 0 : -1));
}

const CombinedPrecheckReport* MainWindow::findDllReport(
    int modelIndex, const QString& dllPath) const {
    auto normalizedPath = [](const QString& path) {
        QFileInfo info(path);
        QString normalized = info.canonicalFilePath();
        if (normalized.isEmpty()) normalized = info.absoluteFilePath();
        return QDir::toNativeSeparators(QDir::cleanPath(normalized));
    };
    const QString wantedPath = normalizedPath(dllPath);
    auto matches = [&wantedPath, &normalizedPath](const CombinedPrecheckReport& report) {
        const QString path = qUtf8(report.dllPath.empty()
            ? report.peReport.filePath : report.dllPath);
        return normalizedPath(path).compare(wantedPath, Qt::CaseInsensitive) == 0;
    };
    if (matches(m_latestReport)) return &m_latestReport;
    if (modelIndex >= 0
        && modelIndex < static_cast<int>(m_latestFleetReport.modelReports.size())) {
        const auto& model = m_latestFleetReport.modelReports[static_cast<size_t>(modelIndex)];
        for (const auto& report : model.dllReports) {
            if (matches(report)) return &report;
        }
    }
    for (const auto& report : m_latestDualReport.dllReports) {
        if (matches(report)) return &report;
    }
    return nullptr;
}

void MainWindow::onLoadModelChanged(int /*index*/) {
    fillDllSelector(m_comboLoadModel, m_comboLoadDll);
    onLoadDllChanged(m_comboLoadDll->currentIndex());
}

void MainWindow::onLoadDllChanged(int /*index*/) {
    const int modelIndex = m_comboLoadModel->currentData().toInt();
    const QString dllPath = m_comboLoadDll->currentData().toString();
    const CombinedPrecheckReport* report = findDllReport(modelIndex, dllPath);
    m_tblExports->setRowCount(0);
    if (!report) {
        m_lblLoadStatus->setText(QStringLiteral("加载状态: 等待执行预检"));
        m_lblLoadApi->setText(QStringLiteral("接口绑定: 等待执行预检"));
        return;
    }

    m_lblLoadStatus->setText(report->loadReport.isLoaded
        ? QStringLiteral("加载状态: 成功")
        : QStringLiteral("加载状态: 失败"));
    m_lblLoadApi->setText(QStringLiteral("接口绑定: 成功 %1 个，缺失 %2 个")
        .arg(report->loadReport.boundSymbolCount)
        .arg(report->loadReport.missingSymbolCount));
    for (const auto& symbol : report->peReport.exportedSymbols) {
        const int row = m_tblExports->rowCount();
        m_tblExports->insertRow(row);
        m_tblExports->setItem(row, 0, new QTableWidgetItem(qUtf8(symbol.name)));
        m_tblExports->setItem(row, 1, new QTableWidgetItem(QString::number(symbol.ordinal)));
        m_tblExports->setItem(row, 2,
            new QTableWidgetItem(symbol.isRequiredInterface ? QStringLiteral("必需接口")
                                                           : QStringLiteral("普通导出")));
    }
}

const CombinedPrecheckReport* MainWindow::selectedPeDllReport() const {
    if (!m_comboPeModel || !m_comboPeDll || m_comboPeDll->currentIndex() < 0) {
        return nullptr;
    }
    return findDllReport(m_comboPeModel->currentData().toInt(),
                         m_comboPeDll->currentData().toString());
}

void MainWindow::refreshReportBrowser() {
    if (!m_pReportBrowser) return;
    if (m_showSingleItemReport) {
        m_pReportBrowser->setHtml(qUtf8(ReportGenerator::GenerateHtml(m_latestReport)));
    } else if (!m_latestFleetReport.modelReports.empty()) {
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
        if (!isModelCompiled(e)) {
            continue;
        }
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

void MainWindow::onTestNavigationChanged(int row) {
    if (row < 0 || row >= m_listTestNavigation->count()) {
        m_testSection->hide();
        return;
    }

    int tabIndex = 0;
    if (row <= 3) {
        tabIndex = row;
    } else if (row <= 6) {
        tabIndex = 4;
    } else {
        tabIndex = row - 2;
    }

    if (row == 9) {
        refreshReportBrowser();
    } else if (row <= 3) {
        refreshPeSelectors();
    }
    m_testSectionTitle->setText(
        QStringLiteral("测试工作区 — %1").arg(m_listTestNavigation->item(row)->text()));

    if (row >= 4 && row <= 6) {
        const bool trajectoryMode = row == 6;
        m_perfOptionsPanel->setVisible(!trajectoryMode);
        m_btnRunTrajectory->setVisible(trajectoryMode);
        m_pChartViewer->setVisible(!trajectoryMode);
        m_trajectoryPanel->setVisible(trajectoryMode);
        m_lblPerfSummary->setVisible(!trajectoryMode);
        if (!trajectoryMode) {
            m_pChartViewer->SetCurrentChart(row == 4 ? 0 : 1);
            m_btnRunStress->setText(row == 4
                ? QStringLiteral("执行性能压测")
                : QStringLiteral("执行内存监测"));
            FitButtonText(m_btnRunStress);
        }
    }

    m_pCentralTabs->setCurrentIndex(tabIndex);
    updateWorkflowUi();
    m_workflowScroll->verticalScrollBar()->setValue(0);
}

void MainWindow::addModel() {
    saveEditorsToCurrentModel();
    m_latestFleetReport = FleetSessionReport();

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
    m_latestFleetReport = FleetSessionReport();
    m_currentModelIndex = -1;
    refreshModelListUi();
    logMessage(QString("INFO: 已删除型号「%1」").arg(name));
}

void MainWindow::onModelSelectionChanged() {
    if (m_blockModelUi) return;
    {
        QSignalBlocker blocker(m_listTestNavigation);
        m_listTestNavigation->setCurrentRow(-1);
    }
    saveEditorsToCurrentModel();
    int row = m_listModels->currentRow();
    loadEditorsFromModel(row);
    m_workflowScroll->ensureWidgetVisible(m_modelSetupPanel, 0, 10);
}

void MainWindow::browseCurrentModelPackage() {
    if (m_currentModelIndex < 0) return;
    QString start = m_editModelPackage->text().trimmed();
    QString dir = QFileDialog::getExistingDirectory(this, "选择型号模型包根目录", start);
    if (dir.isEmpty()) return;

    dir = QDir::toNativeSeparators(dir);
    FleetModelEntry& entry = m_models[static_cast<size_t>(m_currentModelIndex)];
    if (entry.packageDir != dir) {
        entry.harness.reset();
        entry.status = QStringLiteral("未编译");
        m_latestFleetReport = FleetSessionReport();
    }
    m_editModelPackage->setText(dir);
    if (m_editModelName->text().trimmed().isEmpty()
        || m_editModelName->text().startsWith(QStringLiteral("model"))) {
        m_editModelName->setText(QFileInfo(dir).fileName());
    }
    saveEditorsToCurrentModel();
    refreshCurrentModelHeaders();
    refreshModelListUi();
    updateWorkflowUi();
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
    updateWorkflowUi();
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
        LibAnalysisReport lRep = LibAnalyzer::AnalyzeLib(lPath);
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

void MainWindow::runHeaderCheckOnly() {
    const QString path = m_comboHeaderFile->currentData().toString();
    if (path.isEmpty()) {
        m_lblHeaderResult->setText(QStringLiteral("检查结果: 当前型号包中未找到头文件"));
        return;
    }
    if (!m_showSingleItemReport) m_latestReport = CombinedPrecheckReport();
    m_showSingleItemReport = true;
    m_latestReport.headerPath = qToUtf8(path);
    m_latestReport.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    m_latestReport.headerReport = HeaderAnalyzer::AnalyzeHeader(qToUtf8(path));
    const auto& report = m_latestReport.headerReport;
    m_lblHeaderResult->setText(
        QString("检查结果: %1 | 编码: %2 | extern \"C\": %3 | 导出声明: %4")
            .arg(report.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
            .arg(qUtf8(report.encoding))
            .arg(report.hasExternC ? QStringLiteral("有") : QStringLiteral("无"))
            .arg(report.hasDeclspec ? QStringLiteral("有") : QStringLiteral("无")));
    m_tblHeaderFunctions->setRowCount(0);
    for (const auto& function : report.functionDecls) {
        const int row = m_tblHeaderFunctions->rowCount();
        m_tblHeaderFunctions->insertRow(row);
        m_tblHeaderFunctions->setItem(row, 0, new QTableWidgetItem(qUtf8(function.name)));
        m_tblHeaderFunctions->setItem(row, 1, new QTableWidgetItem(qUtf8(function.fullDeclaration)));
    }
    refreshReportBrowser();
    logMessage(QStringLiteral("SUCCESS: 头文件规范检查完成：%1").arg(path));
}

void MainWindow::runLibCheckOnly() {
    const QString path = m_comboLibFile->currentData().toString();
    if (path.isEmpty()) {
        m_lblLibResult->setText(QStringLiteral("检查结果: 当前型号包中未找到 LIB 文件"));
        return;
    }
    if (!m_showSingleItemReport) m_latestReport = CombinedPrecheckReport();
    m_showSingleItemReport = true;
    m_latestReport.libPath = qToUtf8(path);
    m_latestReport.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    m_latestReport.libReport = LibAnalyzer::AnalyzeLib(qToUtf8(path));
    const auto& report = m_latestReport.libReport;
    m_lblLibResult->setText(
        QString("检查结果: %1 | 架构: %2 | 类型: %3 | 缺少符号: %4")
            .arg(report.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
            .arg(qUtf8(report.architecture))
            .arg(qUtf8(report.libType))
            .arg(report.missingSymbols.size()));
    m_tblLibSymbols->setRowCount(0);
    for (const auto& symbol : report.foundSymbols) {
        const int row = m_tblLibSymbols->rowCount();
        m_tblLibSymbols->insertRow(row);
        m_tblLibSymbols->setItem(row, 0, new QTableWidgetItem(qUtf8(symbol)));
    }
    refreshReportBrowser();
    logMessage(QStringLiteral("SUCCESS: LIB 库文件检查完成：%1").arg(path));
}

void MainWindow::runDllFileCheckOnly() {
    const int modelIndex = m_comboPeModel->currentData().toInt();
    const QString dllPath = m_comboPeDll->currentData().toString();
    if (modelIndex < 0 || modelIndex >= static_cast<int>(m_models.size()) || dllPath.isEmpty()) {
        m_lblPePageHint->setText(QStringLiteral("当前型号包中未找到可检查的 DLL。"));
        m_lblPePageHint->show();
        return;
    }

    const FleetModelEntry& model = m_models[static_cast<size_t>(modelIndex)];
    if (!m_showSingleItemReport) m_latestReport = CombinedPrecheckReport();
    if (m_latestReport.dllPath != qToUtf8(dllPath)) {
        m_latestReport.peReport = PeAnalysisReport();
        m_latestReport.loadReport = LoadResult();
    }
    m_showSingleItemReport = true;
    m_latestReport.dllPath = qToUtf8(dllPath);
    m_latestReport.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    m_latestReport.peReport = PeAnalyzer::AnalyzeDll(
        qToUtf8(dllPath), { qToUtf8(model.packageDir) }, {});
    m_latestReport.overallPass = m_latestReport.peReport.overallPass;
    updatePeView(m_latestReport.peReport);
    m_lblPePageHint->hide();
    refreshReportBrowser();
    logMessage(QStringLiteral("SUCCESS: DLL 文件与依赖检查完成：%1").arg(dllPath));
}

void MainWindow::runDllLoadCheckOnly() {
    const int modelIndex = m_comboLoadModel->currentData().toInt();
    const QString dllPath = m_comboLoadDll->currentData().toString();
    if (modelIndex < 0 || modelIndex >= static_cast<int>(m_models.size()) || dllPath.isEmpty()) {
        m_lblLoadPageHint->setText(QStringLiteral("当前型号包中未找到可检查的 DLL。"));
        m_lblLoadPageHint->show();
        return;
    }

    const FleetModelEntry& model = m_models[static_cast<size_t>(modelIndex)];
    if (!m_showSingleItemReport) m_latestReport = CombinedPrecheckReport();
    m_showSingleItemReport = true;
    m_latestReport.dllPath = qToUtf8(dllPath);
    m_latestReport.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    m_latestReport.peReport = PeAnalyzer::AnalyzeDll(
        qToUtf8(dllPath), { qToUtf8(model.packageDir) }, {});
    m_latestReport.loadReport = m_dllLoader.Load(
        qToUtf8(dllPath), InterfaceMapping::DefaultSingleton());
    m_latestReport.overallPass =
        m_latestReport.peReport.overallPass && m_latestReport.loadReport.isLoaded;
    onLoadDllChanged(m_comboLoadDll->currentIndex());
    m_lblLoadPageHint->hide();
    refreshReportBrowser();
    logMessage(QStringLiteral("SUCCESS: DLL 接口与加载检查完成：%1").arg(dllPath));
}

void MainWindow::runFullPrecheck() {
    saveEditorsToCurrentModel();
    if (m_models.empty()) {
        QMessageBox::warning(this, "警告", "请先在左侧「型号与 UserMain」添加至少一个型号！");
        return;
    }

    m_showSingleItemReport = false;
    logMessage("================================================================================");
    logMessage(QString("INFO: 启动全部型号一键预检（共 %1 个型号）...").arg(m_models.size()));

    m_latestFleetReport = FleetSessionReport();
    m_latestFleetReport.timestamp = qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    int passedModels = 0;
    for (size_t i = 0; i < m_models.size(); ++i) {
        DualBuildPrecheckReport one = precheckOneModel(m_models[i]);
        one.modelName = qToUtf8(m_models[i].name);
        if (one.overallPass) ++passedModels;
        m_latestFleetReport.modelReports.push_back(one);
        m_latestDualReport = one;
    }

    std::vector<int> compiledIndexes;
    for (int i = 0; i < static_cast<int>(m_models.size()); ++i) {
        if (isModelCompiled(m_models[static_cast<size_t>(i)])) {
            compiledIndexes.push_back(i);
        } else {
            logMessage(QString("INFO: 型号「%1」未编译，跳过性能、内存、轨迹和并发测试")
                .arg(m_models[static_cast<size_t>(i)].name));
        }
    }

    bool firstPerf = true;
    double weightedTimeSum = 0.0;
    for (int modelIndex : compiledIndexes) {
        FleetModelEntry& model = m_models[static_cast<size_t>(modelIndex)];
        PerfProfileReport perf;
        PerfProfilerWorker worker(model.harness.get(), m_spnSteps->value(),
                                  m_comboHz->currentData().toDouble(),
                                  static_cast<uint32_t>(modelIndex + 1));
        connect(&worker, &PerfProfilerWorker::logMessage, this, &MainWindow::logMessage);
        connect(&worker, &PerfProfilerWorker::finished, this,
                [&perf](const PerfProfileReport& report) { perf = report; });
        worker.process();

        if (firstPerf) {
            m_latestFleetReport.perfReport = perf;
            weightedTimeSum = perf.avgTimeMs * perf.completedSteps;
            firstPerf = false;
        } else {
            PerfProfileReport& total = m_latestFleetReport.perfReport;
            weightedTimeSum += perf.avgTimeMs * perf.completedSteps;
            total.totalSteps += perf.totalSteps;
            total.completedSteps += perf.completedSteps;
            total.minTimeMs = (std::min)(total.minTimeMs, perf.minTimeMs);
            total.maxTimeMs = (std::max)(total.maxTimeMs, perf.maxTimeMs);
            total.jitterMs = (std::max)(total.jitterMs, perf.jitterMs);
            total.memoryDeltaMB += perf.memoryDeltaMB;
            total.memoryLeakRateMBPer10k =
                (std::max)(total.memoryLeakRateMBPer10k, perf.memoryLeakRateMBPer10k);
            total.encounteredException = total.encounteredException || perf.encounteredException;
            if (perf.realtimeVerdict == "FAIL"
                || (perf.realtimeVerdict == "WARNING" && total.realtimeVerdict == "PASS")) {
                total.realtimeVerdict = perf.realtimeVerdict;
            }
            if (total.completedSteps > 0) {
                total.avgTimeMs = weightedTimeSum / total.completedSteps;
            }
        }
        for (auto& dllReport :
             m_latestFleetReport.modelReports[static_cast<size_t>(modelIndex)].dllReports) {
            dllReport.perfReport = perf;
        }

        model.harness->SetTrajectoryCapture(true);
        RandomValueBlob sample = model.harness->Sample(static_cast<uint32_t>(1000 + modelIndex));
        int returnCode = 0;
        bool seh = false;
        std::string trajectoryError;
        model.harness->RunOnce(sample, &returnCode, &seh, trajectoryError);
        model.harness->SetTrajectoryCapture(false);
        std::vector<TrajectorySample> trajectory;
        model.harness->FetchTrajectory(trajectory);
        ++m_latestFleetReport.trajectoryModelsTested;
        if (!seh && returnCode == 0 && !trajectory.empty()) {
            ++m_latestFleetReport.trajectoryModelsPassed;
        }
        for (auto& dllReport :
             m_latestFleetReport.modelReports[static_cast<size_t>(modelIndex)].dllReports) {
            dllReport.trajReport.totalDataPoints = static_cast<int>(trajectory.size());
            dllReport.trajReport.overallPass =
                !seh && returnCode == 0 && !trajectory.empty();
        }
        logMessage(QString("INFO: 型号「%1」轨迹检查完成，采集 %2 个点")
            .arg(model.name).arg(trajectory.size()));
    }

    if (!compiledIndexes.empty()) {
        ConcurrencyTestConfig multiConfig;
        multiConfig.mode = ConcurrencyTestMode::MultiModel;
        for (int modelIndex : compiledIndexes) {
            FleetModelEntry& model = m_models[static_cast<size_t>(modelIndex)];
            MultiModelSpec spec;
            spec.harness = model.harness.get();
            spec.count = model.instanceCount;
            spec.modelName = qToUtf8(model.name);
            multiConfig.models.push_back(spec);
        }
        m_latestFleetReport.multiModelReport = ConcurrencyTester::RunMultiModel(multiConfig);

        ConcurrencyTestReport threadTotal;
        threadTotal.mode = ConcurrencyTestMode::MultiThread;
        threadTotal.modelTypeCount = static_cast<int>(compiledIndexes.size());
        threadTotal.multiThreadSafe = true;
        threadTotal.verdict = "PASS";
        for (int modelIndex : compiledIndexes) {
            ConcurrencyTestConfig threadConfig;
            threadConfig.mode = ConcurrencyTestMode::MultiThread;
            threadConfig.count = m_spnThreadCount->value();
            ConcurrencyTestReport current =
                ConcurrencyTester::Run(*m_models[static_cast<size_t>(modelIndex)].harness, threadConfig);
            threadTotal.workerCount += current.workerCount;
            threadTotal.successCount += current.successCount;
            threadTotal.userFailCount += current.userFailCount;
            threadTotal.exceptionCount += current.exceptionCount;
            threadTotal.crashed = threadTotal.crashed || current.crashed;
            threadTotal.multiThreadSafe = threadTotal.multiThreadSafe && current.multiThreadSafe;
            for (auto& result : current.threadResults) {
                result.modelName = qToUtf8(m_models[static_cast<size_t>(modelIndex)].name);
                threadTotal.threadResults.push_back(result);
            }
            if (current.verdict == "FAIL"
                || (current.verdict == "WARNING" && threadTotal.verdict == "PASS")) {
                threadTotal.verdict = current.verdict;
            }
        }
        threadTotal.summary = "已完成全部已编译型号的多线程 UserMain 测试";
        m_latestFleetReport.multiThreadReport = threadTotal;
    }

    m_latestFleetReport.overallPass = (passedModels == static_cast<int>(m_models.size()));
    if (m_latestFleetReport.perfReport.realtimeVerdict == "FAIL"
        || m_latestFleetReport.multiModelReport.verdict == "FAIL"
        || m_latestFleetReport.multiThreadReport.verdict == "FAIL"
        || (m_latestFleetReport.trajectoryModelsTested > 0
            && m_latestFleetReport.trajectoryModelsPassed
                != m_latestFleetReport.trajectoryModelsTested)) {
        m_latestFleetReport.overallPass = false;
    }

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
        m_latestReport.trajReport = first.trajReport;
        m_latestReport.buildConfig = first.buildConfig;
        m_latestReport.timestamp = m_latestFleetReport.timestamp;
        m_latestReport.multiModelReport = m_latestFleetReport.multiModelReport;
        m_latestReport.multiThreadReport = m_latestFleetReport.multiThreadReport;
        m_latestReport.perfReport = m_latestFleetReport.perfReport;
    }

    if (!m_latestFleetReport.perfReport.realtimeVerdict.empty()) {
        const auto& perf = m_latestFleetReport.perfReport;
        m_pChartViewer->UpdatePerfCharts(perf);
        m_lblPerfSummary->setText(
            QString("压测汇总: Avg %1 ms | Max %2 ms | 内存增长 %3 MB/10k | %4")
                .arg(perf.avgTimeMs, 0, 'f', 4)
                .arg(perf.maxTimeMs, 0, 'f', 4)
                .arg(perf.memoryLeakRateMBPer10k, 0, 'f', 2)
                .arg(qUtf8(perf.realtimeVerdict)));
    }
    if (!m_latestFleetReport.multiModelReport.verdict.empty()) {
        updateResultTable(m_tblMultiModelResults, m_latestFleetReport.multiModelReport);
        m_lblMultiModelSummary->setText(QString("多型号并行汇总: %1 — %2")
            .arg(qUtf8(m_latestFleetReport.multiModelReport.verdict))
            .arg(qUtf8(m_latestFleetReport.multiModelReport.summary)));
    }
    if (!m_latestFleetReport.multiThreadReport.verdict.empty()) {
        updateResultTable(m_tblMultiThreadResults, m_latestFleetReport.multiThreadReport);
        m_lblMultiThreadSummary->setText(QString("多线程测试汇总: %1 — %2")
            .arg(qUtf8(m_latestFleetReport.multiThreadReport.verdict))
            .arg(qUtf8(m_latestFleetReport.multiThreadReport.summary)));
    }
    if (!m_latestDualReport.headerReports.empty()) {
        const auto& header = m_latestDualReport.headerReports.front();
        m_lblHeaderResult->setText(
            QString("检查结果: %1 | 编码: %2 | extern \"C\": %3 | 导出声明: %4")
                .arg(header.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                .arg(qUtf8(header.encoding))
                .arg(header.hasExternC ? QStringLiteral("有") : QStringLiteral("无"))
                .arg(header.hasDeclspec ? QStringLiteral("有") : QStringLiteral("无")));
        m_tblHeaderFunctions->setRowCount(0);
        for (const auto& function : header.functionDecls) {
            const int row = m_tblHeaderFunctions->rowCount();
            m_tblHeaderFunctions->insertRow(row);
            m_tblHeaderFunctions->setItem(row, 0, new QTableWidgetItem(qUtf8(function.name)));
            m_tblHeaderFunctions->setItem(row, 1,
                new QTableWidgetItem(qUtf8(function.fullDeclaration)));
        }
    }
    if (!m_latestDualReport.libReports.empty()) {
        const auto& lib = m_latestDualReport.libReports.front();
        m_lblLibResult->setText(
            QString("检查结果: %1 | 架构: %2 | 类型: %3 | 缺少符号: %4")
                .arg(lib.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                .arg(qUtf8(lib.architecture))
                .arg(qUtf8(lib.libType))
                .arg(lib.missingSymbols.size()));
        m_tblLibSymbols->setRowCount(0);
        for (const auto& symbol : lib.foundSymbols) {
            const int row = m_tblLibSymbols->rowCount();
            m_tblLibSymbols->insertRow(row);
            m_tblLibSymbols->setItem(row, 0, new QTableWidgetItem(qUtf8(symbol)));
        }
    }

    updateStatusBadges();
    refreshPeSelectors();
    refreshReportBrowser();
    updateWorkflowUi();

    logMessage(QString("SUCCESS: 全部型号预检完毕！通过 %1/%2")
        .arg(passedModels).arg(m_models.size()));
}

void MainWindow::runStressTestOnly() {
    if (!requireSelectedModelCompiled(m_comboStressModel, 1)) {
        return;
    }
    FleetModelEntry* entry = selectedTestModel(m_comboStressModel);
    if (!entry || !entry->harness) return;

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
        updateWorkflowUi();
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
        updateWorkflowUi();
        m_lblTrajOut->setText(
            QStringLiteral("轨迹输出: 未采集到点。请在 UserMain 循环中写入 "
                           "out_lat/out_lon 并调用 RecordTrajectoryPoint(out_lat, out_lon) 后重新编译。"));
        logMessage("WARN: 轨迹点为空，请检查 UserMain 是否调用了 RecordTrajectoryPoint");
        return;
    }

    fillTrajTable(pts);
    updateWorkflowUi();
    const auto& last = pts.last();
    m_lblTrajOut->setText(
        QStringLiteral("轨迹输出: 点数=%1 | 末点 out_lat=%2, out_lon=%3 | 随机参数: %4")
            .arg(pts.size())
            .arg(last.lat, 0, 'f', 6)
            .arg(last.lon, 0, 'f', 6)
            .arg(qUtf8(blob.summary)));
    logMessage(QString("SUCCESS: 轨迹试跑完成，记录 %1 个经纬度点").arg(pts.size()));
}

void MainWindow::startConcurrencyWorker(UserCodeHarness* harness, const ConcurrencyTestConfig& cfg) {
    if (m_pWorkerThread) {
        m_pWorkerThread->quit();
        m_pWorkerThread->wait();
        delete m_pWorkerThread;
        m_pWorkerThread = nullptr;
    }

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
    startConcurrencyWorker(nullptr, cfg);
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
    startConcurrencyWorker(entry->harness.get(), cfg);
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
    updateWorkflowUi();

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
    } else {
        m_latestReport.multiThreadReport = report;
        m_latestFleetReport.multiThreadReport = report;
        updateResultTable(m_tblMultiThreadResults, report);
        m_lblMultiThreadSummary->setText(QString("多线程测试汇总: <b>%1</b> — %2")
            .arg(qUtf8(report.verdict))
            .arg(qUtf8(report.summary)));
    }

    if (report.verdict == "FAIL") {
        m_latestReport.overallPass = false;
        if (!m_latestFleetReport.modelReports.empty()) {
            m_latestFleetReport.overallPass = false;
        }
    }

    updateStatusBadges();
    refreshReportBrowser();
    updateWorkflowUi();

    logMessage(QString("SUCCESS: %1 完成，判定=%2")
        .arg(isMultiModel ? "多型号并行" : "多线程测试")
        .arg(qUtf8(report.verdict)));
}

void MainWindow::updatePeView(const PeAnalysisReport& pe) {
    m_lblPeArch->setText(QString("CPU 架构: %1%2")
        .arg(qUtf8(pe.architecture))
        .arg(pe.isArchMatch ? " (与宿主匹配)" : " (与宿主不匹配)"));
    m_lblPeCrt->setText(QString("运行库类型: %1").arg(qUtf8(pe.crtLinkage)));
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
        m_tblImports->setItem(row, 1, st);
        m_tblImports->setItem(row, 2, new QTableWidgetItem(qUtf8(dep.resolvedPath)));
    }

    logMessage(QString("INFO: DLL 文件与依赖视图已更新 — 依赖 %1 项, 导出 %2 个符号 [%3]")
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
    const bool hasFleet = !m_latestFleetReport.modelReports.empty() && !m_showSingleItemReport;
    const bool hasSingle = !m_latestReport.headerPath.empty()
        || !m_latestReport.libPath.empty()
        || !m_latestReport.dllPath.empty();
    if (!hasFleet && m_latestDualReport.packageDir.empty() && !hasSingle) {
        QMessageBox::warning(this, "警告", "请先运行预检流程后再导出报告！");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(
        this, "保存预检报告 HTML", "Precheck_Report.html", "HTML Files (*.html)");
    if (savePath.isEmpty()) return;

    bool success = false;
    if (hasFleet) {
        success = ReportGenerator::SaveFleetReportToFile(m_latestFleetReport, qToUtf8(savePath));
    } else if (!m_showSingleItemReport && !m_latestDualReport.packageDir.empty()) {
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
