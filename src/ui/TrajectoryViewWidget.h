#ifndef TRAJECTORY_VIEW_WIDGET_H
#define TRAJECTORY_VIEW_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QString>

struct TrajectoryPoint {
    double lat = 0.0;
    double lon = 0.0;
};

struct TrajectorySeries {
    QString name;
    int objectId = -1;
    bool baseline = false;
    QColor color;
    QVector<TrajectoryPoint> points;
};

/** 2D ground track: Lon (X) × Lat (Y), polyline of step outputs. */
class TrajectoryViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrajectoryViewWidget(QWidget* parent = nullptr);

    void setPoints(const QVector<TrajectoryPoint>& points);
    void setSeries(const QVector<TrajectorySeries>& series);
    void setHighlightedObject(int objectId);
    void setEmptyHint(const QString& hint);
    void clearPoints();
    int pointCount() const;
    /** When true, widget height tracks width (square plot area). */
    void setKeepSquare(bool keepSquare);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    void applySquareConstraints();

    QPointF toScreen(double lon, double lat,
                     double minLon, double maxLon,
                     double minLat, double maxLat,
                     const QRect& plot) const;

    QVector<TrajectoryPoint> m_points;
    QVector<TrajectorySeries> m_series;
    int m_highlightedObject = -1;
    bool m_keepSquare = false;
    QString m_emptyHint =
        QStringLiteral("暂无轨迹点\n请先编译型号，再点击「试跑并绘制轨迹」\n"
                       "UserMain 中需用 out_lat/out_lon 调用 RecordTrajectoryPoint");
};

#endif // TRAJECTORY_VIEW_WIDGET_H
