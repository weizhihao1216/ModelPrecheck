#ifndef TRAJECTORY_VIEW_WIDGET_H
#define TRAJECTORY_VIEW_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>

struct TrajectoryPoint {
    double lat = 0.0;
    double lon = 0.0;
};

/** 2D ground track: Lon (X) × Lat (Y), polyline of step outputs. */
class TrajectoryViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrajectoryViewWidget(QWidget* parent = nullptr);

    void setPoints(const QVector<TrajectoryPoint>& points);
    void clearPoints();
    int pointCount() const { return m_points.size(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPointF toScreen(double lon, double lat,
                     double minLon, double maxLon,
                     double minLat, double maxLat,
                     const QRect& plot) const;

    QVector<TrajectoryPoint> m_points;
};

#endif // TRAJECTORY_VIEW_WIDGET_H
