#include "BusyOverlayWidget.h"

#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QShowEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

namespace {

class SpinnerWidget : public QWidget {
public:
    explicit SpinnerWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedSize(48, 48);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAutoFillBackground(false);
        m_timer.setInterval(16);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 12) % 360;
            update();
        });
    }

    void startAnimation() {
        m_angle = 0;
        if (!m_timer.isActive()) m_timer.start();
        update();
    }

    void stopAnimation() {
        m_timer.stop();
        m_angle = 0;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), Qt::transparent);
        const QRectF arcRect = QRectF(4, 4, width() - 8, height() - 8);
        QPen pen(QColor("#2dd4bf"));
        pen.setWidth(4);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawArc(arcRect, -m_angle * 16, 270 * 16);
        pen.setColor(QColor("#5eead4"));
        pen.setWidth(2);
        painter.setPen(pen);
        painter.drawArc(arcRect.adjusted(6, 6, -6, -6), (-m_angle + 120) * 16, 180 * 16);
    }

private:
    QTimer m_timer;
    int m_angle = 0;
};

} // namespace

BusyOverlayWidget::BusyOverlayWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("busyOverlay"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
    setStyleSheet(QStringLiteral(
        "BusyOverlayWidget#busyOverlay {"
        "  background-color: transparent;"
        "}"));
    hide();

    auto* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("busyPanel"));
    panel->setAttribute(Qt::WA_TranslucentBackground, true);
    panel->setAutoFillBackground(false);
    panel->setStyleSheet(QStringLiteral(
        "QWidget#busyPanel {"
        "  background-color: transparent;"
        "  border: none;"
        "}"));

    m_spinner = new SpinnerWidget(panel);
    m_label = new QLabel(panel);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    m_label->setStyleSheet(QStringLiteral(
        "color: #ffffff; font-size: 14px; background: transparent; border: none;"));
    m_label->setMinimumWidth(260);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);
    layout->addWidget(m_spinner, 0, Qt::AlignHCenter);
    layout->addWidget(m_label);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addStretch();
    outer->addWidget(panel, 0, Qt::AlignHCenter);
    outer->addStretch();

    if (parent) {
        parent->installEventFilter(this);
        syncGeometry();
    }
}

void BusyOverlayWidget::syncGeometry() {
    if (QWidget* p = parentWidget()) {
        setGeometry(p->rect());
        raise();
    }
}

bool BusyOverlayWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget()
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        syncGeometry();
    }
    return QWidget::eventFilter(watched, event);
}

void BusyOverlayWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    syncGeometry();
    raise();
}

void BusyOverlayWidget::showBusy(const QString& text) {
    if (m_depth++ == 0) {
        m_label->setText(text);
        syncGeometry();
        show();
        raise();
        static_cast<SpinnerWidget*>(m_spinner)->startAnimation();
    } else {
        m_label->setText(text);
    }
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void BusyOverlayWidget::hideBusy() {
    if (m_depth <= 0) return;
    if (--m_depth == 0) {
        static_cast<SpinnerWidget*>(m_spinner)->stopAnimation();
        hide();
    }
}

void BusyOverlayWidget::setBusyText(const QString& text) {
    m_label->setText(text);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void BusyOverlayWidget::runBlocking(const std::function<void()>& work) {
    if (!work) return;

    QEventLoop loop;
    QThread* thread = QThread::create(work);
    QObject::connect(thread, &QThread::finished, &loop, &QEventLoop::quit);
    thread->start();
    loop.exec();
    thread->wait();
    delete thread;
}
