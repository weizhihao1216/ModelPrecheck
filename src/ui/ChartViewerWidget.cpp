#include "ChartViewerWidget.h"
#include <QVBoxLayout>
#include <QPen>
#include <QColor>
#include <algorithm>

using namespace QtCharts;

ChartViewerWidget::ChartViewerWidget(QWidget* parent)
    : QWidget(parent)
    , m_liveBudgetMs(20.0)
    , m_liveTotalSteps(10000)
    , m_liveMaxTime(0.0)
    , m_liveMinMem(1e9)
    , m_liveMaxMem(-1e9) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    m_pTabWidget = new QTabWidget(this);

    m_pTimeChart = new QChart();
    m_pTimeChart->setTitle("单步计算耗时抖动曲线 (CPU Step Execution Time)");
    m_pTimeChart->setTheme(QChart::ChartThemeLight);

    m_pTimeSeries = new QLineSeries();
    m_pTimeSeries->setName("单步耗时 (ms)");
    QPen penTime(QColor("#0066cc"));
    penTime.setWidth(2);
    m_pTimeSeries->setPen(penTime);

    m_pBudgetSeries = new QLineSeries();
    m_pBudgetSeries->setName("实时帧预算 (Frame Budget Line)");
    QPen penBudget(QColor("#dc2626"));
    penBudget.setStyle(Qt::DashLine);
    penBudget.setWidth(2);
    m_pBudgetSeries->setPen(penBudget);

    m_axisTimeX = new QValueAxis();
    m_axisTimeX->setTitleText("步进次数 (Step Count)");
    m_axisTimeX->setLabelFormat("%d");
    m_axisTimeX->setGridLineVisible(true);

    m_axisTimeY = new QValueAxis();
    m_axisTimeY->setTitleText("单步耗时 (ms)");
    m_axisTimeY->setLabelFormat("%.3f");
    m_axisTimeY->setGridLineVisible(true);

    m_pTimeChart->addSeries(m_pTimeSeries);
    m_pTimeChart->addSeries(m_pBudgetSeries);
    m_pTimeChart->addAxis(m_axisTimeX, Qt::AlignBottom);
    m_pTimeChart->addAxis(m_axisTimeY, Qt::AlignLeft);
    m_pTimeSeries->attachAxis(m_axisTimeX);
    m_pTimeSeries->attachAxis(m_axisTimeY);
    m_pBudgetSeries->attachAxis(m_axisTimeX);
    m_pBudgetSeries->attachAxis(m_axisTimeY);

    m_pTimeChartView = new QChartView(m_pTimeChart, this);
    m_pTimeChartView->setRenderHint(QPainter::Antialiasing);

    m_pMemChart = new QChart();
    m_pMemChart->setTitle("物理内存占用曲线 (Working Set Memory Footprint)");
    m_pMemChart->setTheme(QChart::ChartThemeLight);

    m_pMemSeries = new QLineSeries();
    m_pMemSeries->setName("Working Set 物理内存 (MB)");
    QPen penMem(QColor("#16a34a"));
    penMem.setWidth(2);
    m_pMemSeries->setPen(penMem);

    m_axisMemX = new QValueAxis();
    m_axisMemX->setTitleText("步进次数 (Step Count)");
    m_axisMemX->setLabelFormat("%d");
    m_axisMemX->setGridLineVisible(true);

    m_axisMemY = new QValueAxis();
    m_axisMemY->setTitleText("内存占用 (MB)");
    m_axisMemY->setLabelFormat("%.2f");
    m_axisMemY->setGridLineVisible(true);

    m_pMemChart->addSeries(m_pMemSeries);
    m_pMemChart->addAxis(m_axisMemX, Qt::AlignBottom);
    m_pMemChart->addAxis(m_axisMemY, Qt::AlignLeft);
    m_pMemSeries->attachAxis(m_axisMemX);
    m_pMemSeries->attachAxis(m_axisMemY);

    m_pMemChartView = new QChartView(m_pMemChart, this);
    m_pMemChartView->setRenderHint(QPainter::Antialiasing);

    m_pTabWidget->addTab(m_pTimeChartView, "耗时抖动曲线");
    m_pTabWidget->addTab(m_pMemChartView, "内存占用曲线");

    mainLayout->addWidget(m_pTabWidget);
}

ChartViewerWidget::~ChartViewerWidget() {
}

void ChartViewerWidget::PrepareLiveProfiling(int totalSteps, double frameBudgetMs) {
    m_liveTotalSteps = totalSteps > 0 ? totalSteps : 10000;
    m_liveBudgetMs = frameBudgetMs;
    m_liveMaxTime = 0.0;
    m_liveMinMem = 1e9;
    m_liveMaxMem = -1e9;

    m_pTimeSeries->clear();
    m_pBudgetSeries->clear();
    m_pMemSeries->clear();

    m_pBudgetSeries->append(0, m_liveBudgetMs);
    m_pBudgetSeries->append(m_liveTotalSteps, m_liveBudgetMs);

    m_axisTimeX->setRange(0, m_liveTotalSteps);
    m_axisTimeY->setRange(-0.5, m_liveBudgetMs * 1.25 + 0.5);
    m_axisMemX->setRange(0, m_liveTotalSteps);

    m_pTimeChart->update();
    m_pMemChart->update();
}

void ChartViewerWidget::AddLiveSample(int stepIndex, double timeMs, double memoryMB) {
    m_pTimeSeries->append(stepIndex, timeMs);
    m_pMemSeries->append(stepIndex, memoryMB);

    if (timeMs > m_liveMaxTime) {
        m_liveMaxTime = timeMs;
        double yLimit = (std::max)(m_liveMaxTime, m_liveBudgetMs);
        m_axisTimeY->setRange(-0.5, yLimit * 1.25 + 0.5);
    }

    if (memoryMB < m_liveMinMem) m_liveMinMem = memoryMB;
    if (memoryMB > m_liveMaxMem) m_liveMaxMem = memoryMB;

    double memMargin = (m_liveMaxMem - m_liveMinMem) * 0.2;
    if (memMargin < 2.0) memMargin = 2.0;
    m_axisMemY->setRange(m_liveMinMem - memMargin, m_liveMaxMem + memMargin);
}

void ChartViewerWidget::UpdatePerfCharts(const PerfProfileReport& report) {
    m_pTimeSeries->clear();
    m_pBudgetSeries->clear();
    m_pMemSeries->clear();

    if (report.samples.empty()) return;

    int maxStep = report.samples.back().stepIndex;
    if (maxStep <= 0) maxStep = report.totalSteps;
    if (maxStep <= 0) maxStep = 100;

    m_pBudgetSeries->append(0, report.frameBudgetMs);
    m_pBudgetSeries->append(maxStep, report.frameBudgetMs);

    double maxT = report.maxTimeMs;
    if (maxT < report.frameBudgetMs) maxT = report.frameBudgetMs;

    double minMem = report.initialMemoryMB;
    double maxMem = report.finalMemoryMB;

    for (const auto& s : report.samples) {
        m_pTimeSeries->append(s.stepIndex, s.timeMs);
        m_pMemSeries->append(s.stepIndex, s.memoryMB);
        if (s.memoryMB < minMem) minMem = s.memoryMB;
        if (s.memoryMB > maxMem) maxMem = s.memoryMB;
    }

    m_axisTimeX->setRange(0, maxStep);
    m_axisTimeY->setRange(-0.5, maxT * 1.25 + 0.5);

    double memMargin = (maxMem - minMem) * 0.2;
    if (memMargin < 2.0) memMargin = 2.0;

    m_axisMemX->setRange(0, maxStep);
    m_axisMemY->setRange(minMem - memMargin, maxMem + memMargin);

    m_pTimeChart->update();
    m_pMemChart->update();
    m_pTimeChartView->repaint();
    m_pMemChartView->repaint();
}

void ChartViewerWidget::ClearCharts() {
    m_pTimeSeries->clear();
    m_pBudgetSeries->clear();
    m_pMemSeries->clear();
}
