#include "TrajectoryViewWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QtMath>
#include <algorithm>
#include <cmath>

TrajectoryViewWidget::TrajectoryViewWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void TrajectoryViewWidget::setPoints(const QVector<TrajectoryPoint>& points) {
    m_points = points;
    update();
}

void TrajectoryViewWidget::clearPoints() {
    m_points.clear();
    update();
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
    p.fillRect(area, QColor("#1e1e2e"));
    p.setPen(QPen(QColor("#45475a"), 1));
    p.drawRoundedRect(area, 6, 6);

    QRect plot = area.adjusted(52, 32, -16, -40);
    p.setPen(QColor("#89b4fa"));
    p.setFont(QFont(font().family(), 10, QFont::Bold));
    p.drawText(area.adjusted(12, 6, -12, 0), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("二维轨迹 (经度 Lon × 纬度 Lat)"));

    if (m_points.isEmpty()) {
        p.setPen(QColor("#a6adc8"));
        p.drawText(plot, Qt::AlignCenter,
                   QStringLiteral("暂无轨迹点\n请先编译型号，再点击「试跑并绘制轨迹」\n"
                                  "UserMain 中需用 out_lat/out_lon 调用 RecordTrajectoryPoint"));
        return;
    }

    double minLon = m_points[0].lon, maxLon = minLon;
    double minLat = m_points[0].lat, maxLat = minLat;
    for (const auto& pt : m_points) {
        minLon = qMin(minLon, pt.lon); maxLon = qMax(maxLon, pt.lon);
        minLat = qMin(minLat, pt.lat); maxLat = qMax(maxLat, pt.lat);
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
    p.setPen(QPen(QColor("#313244"), 1, Qt::DotLine));
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
    p.setPen(QPen(QColor("#585b70"), 1.5));
    p.drawRect(plot);
    p.setPen(QColor("#a6adc8"));
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

    QVector<QPointF> poly;
    poly.reserve(m_points.size());
    for (const auto& pt : m_points) {
        poly.push_back(toScreen(pt.lon, pt.lat, minLon, maxLon, minLat, maxLat, plot));
    }

    p.setPen(QPen(QColor("#cba6f7"), 2.0));
    for (int i = 1; i < poly.size(); ++i) {
        p.drawLine(poly[i - 1], poly[i]);
    }

    // Draw points (downsample if many)
    const int step = qMax(1, poly.size() / 80);
    for (int i = 0; i < poly.size(); i += step) {
        const bool ends = (i == 0) || (i + step >= poly.size());
        p.setBrush(ends ? QColor("#a6e3a1") : QColor("#89b4fa"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(poly[i], ends ? 4.0 : 2.5, ends ? 4.0 : 2.5);
    }
    if (!poly.isEmpty()) {
        p.setBrush(QColor("#f38ba8"));
        p.drawEllipse(poly.last(), 4.5, 4.5);
    }

    const auto& last = m_points.last();
    p.setPen(QColor("#cdd6f4"));
    p.setFont(QFont(font().family(), 9));
    p.drawText(area.adjusted(12, 0, -12, -8), Qt::AlignLeft | Qt::AlignBottom,
               QStringLiteral("点数 %1 | 末点 lat=%2  lon=%3")
                   .arg(m_points.size())
                   .arg(last.lat, 0, 'f', 6)
                   .arg(last.lon, 0, 'f', 6));
}
