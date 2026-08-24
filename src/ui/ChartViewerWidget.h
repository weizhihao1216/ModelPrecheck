#ifndef CHART_VIEWER_WIDGET_H
#define CHART_VIEWER_WIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <vector>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>

#include "../core/PerfProfiler.h"
#include "../core/FunctionalVerifier.h"

class ChartViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartViewerWidget(QWidget* parent = nullptr);
    ~ChartViewerWidget();

public slots:
    void PrepareLiveProfiling(int totalSteps, double frameBudgetMs);
    void AddLiveSample(int stepIndex, double timeMs, double memoryMB);
    void UpdatePerfCharts(const PerfProfileReport& report);
    void UpdateTrajectoryChart(const TrajectoryVerificationReport& report);
    void ClearCharts();

private:
    QTabWidget* m_pTabWidget;

    // CPU Time Chart
    QtCharts::QChart* m_pTimeChart;
    QtCharts::QChartView* m_pTimeChartView;
    QtCharts::QLineSeries* m_pTimeSeries;
    QtCharts::QLineSeries* m_pBudgetSeries;
    QtCharts::QValueAxis* m_axisTimeX;
    QtCharts::QValueAxis* m_axisTimeY;

    // Memory Chart
    QtCharts::QChart* m_pMemChart;
    QtCharts::QChartView* m_pMemChartView;
    QtCharts::QLineSeries* m_pMemSeries;
    QtCharts::QValueAxis* m_axisMemX;
    QtCharts::QValueAxis* m_axisMemY;

    // Trajectory Chart
    QtCharts::QChart* m_pTrajChart;
    QtCharts::QChartView* m_pTrajChartView;
    QtCharts::QLineSeries* m_pTrajSeries;
    QtCharts::QValueAxis* m_axisTrajX;
    QtCharts::QValueAxis* m_axisTrajY;

    // Dynamic Live Tracking
    double m_liveBudgetMs;
    int m_liveTotalSteps;
    double m_liveMaxTime;
    double m_liveMinMem;
    double m_liveMaxMem;
};

#endif // CHART_VIEWER_WIDGET_H
