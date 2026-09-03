#include "TrajectoryViewWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QtMath>
#include <algorithm>
#include <cmath>

TrajectoryViewWidget::TrajectoryViewWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(240, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void TrajectoryViewWidget::setKeepSquare(bool keepSquare) {
    if (m_keepSquare == keepSquare) return;
    m_keepSquare = keepSquare;
    QSizePolicy policy(QSizePolicy::Expanding,
                       keepSquare ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    policy.setHeightForWidth(keepSquare);
    setSizePolicy(policy);
    if (!keepSquare) {
        setMinimumHeight(240);
        setMaximumHeight(QWIDGETSIZE_MAX);
    }
    applySquareConstraints();
    updateGeometry();
}

void TrajectoryViewWidget::applySquareConstraints() {
    if (!m_keepSquare) return;
    const int side = qMax(240, width() > 0 ? width() : sizeHint().width());
    setMinimumHeight(side);
    setMaximumHeight(side);
}

bool TrajectoryViewWidget::hasHeightForWidth() const {
    return m_keepSquare;
}

int TrajectoryViewWidget::heightForWidth(int width) const {
    return m_keepSquare ? qMax(240, width) : QWidget::heightForWidth(width);
}

QSize TrajectoryViewWidget::sizeHint() const {
    return m_keepSquare ? QSize(360, 360) : QSize(320, 240);
}

QSize TrajectoryViewWidget::minimumSizeHint() const {
    return m_keepSquare ? QSize(240, 240) : QSize(240, 180);
}

void TrajectoryViewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    applySquareConstraints();
}

void TrajectoryViewWidget::setPoints(const QVector<TrajectoryPoint>& points) {
    m_points = points;
    m_series.clear();
    m_highlightedObject = -1;
    update();
}

void TrajectoryViewWidget::setSeries(const QVector<TrajectorySeries>& series) {
    m_series = series;
    m_points.clear();
    update();
}

void TrajectoryViewWidget::setHighlightedObject(int objectId) {
    m_highlightedObject = objectId;
    update();
}

void TrajectoryViewWidget::setEmptyHint(const QString& hint) {
    m_emptyHint = hint;
    update();
}

void TrajectoryViewWidget::clearPoints() {
    m_points.clear();
    m_series.clear();
    m_highlightedObject = -1;
    update();
}

int TrajectoryViewWidget::pointCount() const {
    int count = m_points.size();
    for (const auto& series : m_series) count += series.points.size();
    return count;
}

QPointF TrajectoryViewWidget::toScreen(double lon, double lat,
                                       double minLon, double maxLon,
                                       double minLat, double maxLat,
                                       const QRect& plot) const {
    const double spanLon = (maxLon > minLon) ? (maxLon - minLon) : 1.0;
    const double spanLat = (maxLat > minLat) ? (maxLat - minLat) : 1.0;
    const double x = plot.left() + (lon - minLon) / spanLon * plot.width();
    // Lat grows upward
    const double y = plot.bottom() - (lat - minLat) / spanLat * plot.height();
    return QPointF(x, y);
}

void TrajectoryViewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect area = rect().adjusted(8, 8, -8, -8);
    p.fillRect(area, QColor("#0c1218"));
    p.setPen(QPen(QColor("#2d3f4f"), 1));
    p.drawRoundedRect(area, 6, 6);

    QRect plot = area.adjusted(52, 32, -16, -40);
    p.setPen(QColor("#14b8a6"));
    p.setFont(QFont(font().family(), 10, QFont::Bold));
    p.drawText(area.adjusted(12, 6, -12, 0), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("二维轨迹 (经度 Lon × 纬度 Lat)"));

    if (m_points.isEmpty() && m_series.isEmpty()) {
        p.setPen(QColor("#94a3b8"));
        p.drawText(plot, Qt::AlignCenter, m_emptyHint);
        return;
    }

    TrajectoryPoint first;
    bool hasFirst = false;
    if (!m_points.isEmpty()) {
        first = m_points.first();
        hasFirst = true;
    } else {
        for (const auto& series : m_series) {
            if (!series.points.isEmpty()) {
                first = series.points.first();
                hasFirst = true;
                break;
            }
        }
    }
    if (!hasFirst) return;
    double minLon = first.lon, maxLon = minLon;
    double minLat = first.lat, maxLat = minLat;
    auto includePoint = [&](const TrajectoryPoint& pt) {
        minLon = qMin(minLon, pt.lon); maxLon = qMax(maxLon, pt.lon);
        minLat = qMin(minLat, pt.lat); maxLat = qMax(maxLat, pt.lat);
    };
    for (const auto& pt : m_points) includePoint(pt);
    for (const auto& series : m_series) {
        for (const auto& pt : series.points) includePoint(pt);
    }
    auto pad = [](double& a, double& b) {
        double span = b - a;
        if (span < 1e-8) {
            a -= 0.01;
            b += 0.01;
            span = b - a;
        }
        const double m = span * 0.1;
        a -= m;
        b += m;
    };
    pad(minLon, maxLon);
    pad(minLat, maxLat);

    // Grid
    p.setPen(QPen(QColor("#1e3a34"), 1, Qt::DotLine));
    for (int i = 0; i <= 4; ++i) {
        const double t = i / 4.0;
        const double lon = minLon + t * (maxLon - minLon);
        const double lat = minLat + t * (maxLat - minLat);
        p.drawLine(toScreen(lon, minLat, minLon, maxLon, minLat, maxLat, plot),
                   toScreen(lon, maxLat, minLon, maxLon, minLat, maxLat, plot));
        p.drawLine(toScreen(minLon, lat, minLon, maxLon, minLat, maxLat, plot),
                   toScreen(maxLon, lat, minLon, maxLon, minLat, maxLat, plot));
    }

    // Axes
    p.setPen(QPen(QColor("#2d3f4f"), 1.5));
    p.drawRect(plot);
    p.setPen(QColor("#94a3b8"));
    p.setFont(QFont(font().family(), 8));
    p.drawText(QRect(plot.left(), plot.bottom() + 4, plot.width(), 16),
               Qt::AlignCenter, QStringLiteral("经度 Lon"));
    p.save();
    p.translate(area.left() + 14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRect(-40, -10, 80, 20), Qt::AlignCenter, QStringLiteral("纬度 Lat"));
    p.restore();
    p.drawText(QPointF(plot.left(), plot.bottom() + 18),
               QString::number(minLon, 'f', 5));
    p.drawText(QRectF(plot.right() - 70, plot.bottom() + 4, 70, 16),
               Qt::AlignRight, QString::number(maxLon, 'f', 5));
    p.drawText(QPointF(area.left() + 8, plot.bottom()),
               QString::number(minLat, 'f', 5));
    p.drawText(QPointF(area.left() + 8, plot.top() + 10),
               QString::number(maxLat, 'f', 5));

    auto drawTrack = [&](const QVector<TrajectoryPoint>& points, const QColor& color,
                         Qt::PenStyle style, qreal width) {
        QVector<QPointF> poly;
        poly.reserve(points.size());
        for (const auto& point : points)
            poly.push_back(toScreen(point.lon, point.lat, minLon, maxLon, minLat, maxLat, plot));
        p.setPen(QPen(color, width, style));
        p.setBrush(Qt::NoBrush);
        for (int i = 1; i < poly.size(); ++i) p.drawLine(poly[i - 1], poly[i]);
        if (!poly.isEmpty()) {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(poly.first(), 3.0, 3.0);
            p.drawEllipse(poly.last(), 4.0, 4.0);
        }
    };

    if (!m_series.isEmpty()) {
        int legendX = plot.left() + 8;
        int legendY = plot.top() + 8;
        for (const auto& series : m_series) {
            const bool highlighted = m_highlightedObject < 0
                || m_highlightedObject == series.objectId;
            QColor color = series.color.isValid() ? series.color : QColor("#2dd4bf");
            if (!highlighted) color.setAlpha(65);
            drawTrack(series.points, color,
                      series.baseline ? Qt::DashLine : Qt::SolidLine,
                      highlighted ? 2.2 : 1.0);
            p.setPen(QPen(color, 2, series.baseline ? Qt::DashLine : Qt::SolidLine));
            p.drawLine(legendX, legendY + 5, legendX + 22, legendY + 5);
            p.setPen(QColor("#e2e8f0"));
            p.drawText(legendX + 28, legendY + 10, series.name);
            legendY += 16;
        }
        p.setPen(QColor("#e2e8f0"));
        p.drawText(area.adjusted(12, 0, -12, -8), Qt::AlignLeft | Qt::AlignBottom,
                   QStringLiteral("轨迹 %1 条 | 总点数 %2 | 虚线=基线，实线=交错")
                       .arg(m_series.size()).arg(pointCount()));
    } else {
        drawTrack(m_points, QColor("#14b8a6"), Qt::SolidLine, 2.0);
        const auto& last = m_points.last();
        p.setPen(QColor("#e2e8f0"));
        p.setFont(QFont(font().family(), 9));
        p.drawText(area.adjusted(12, 0, -12, -8), Qt::AlignLeft | Qt::AlignBottom,
                   QStringLiteral("点数 %1 | 末点 lat=%2  lon=%3")
                       .arg(m_points.size())
                       .arg(last.lat, 0, 'f', 6)
                       .arg(last.lon, 0, 'f', 6));
    }
}
