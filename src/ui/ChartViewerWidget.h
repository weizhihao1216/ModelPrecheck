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

class ChartViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartViewerWidget(QWidget* parent = nullptr);
    ~ChartViewerWidget();

public slots:
    void PrepareLiveProfiling(int totalSteps, double frameBudgetMs);
    void AddLiveSample(int stepIndex, double timeMs, double memoryMB);
    void UpdatePerfCharts(const PerfProfileReport& report);
    void ClearCharts();
    void SetCurrentChart(int index);

private:
    QTabWidget* m_pTabWidget;

    QtCharts::QChart* m_pTimeChart;
    QtCharts::QChartView* m_pTimeChartView;
    QtCharts::QLineSeries* m_pTimeSeries;
    QtCharts::QLineSeries* m_pBudgetSeries;
    QtCharts::QValueAxis* m_axisTimeX;
    QtCharts::QValueAxis* m_axisTimeY;

    QtCharts::QChart* m_pMemChart;
    QtCharts::QChartView* m_pMemChartView;
    QtCharts::QLineSeries* m_pMemSeries;
    QtCharts::QValueAxis* m_axisMemX;
    QtCharts::QValueAxis* m_axisMemY;

    double m_liveBudgetMs;
    int m_liveTotalSteps;
    double m_liveMaxTime;
    double m_liveMinMem;
    double m_liveMaxMem;
};

#endif // CHART_VIEWER_WIDGET_H
