#include "BusyOverlayWidget.h"

#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
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

/** Hard-wrap so long DLL paths (no spaces) still show completely. */
QString HardWrapToWidth(const QString& text, const QFontMetrics& fm, int maxWidth) {
    if (maxWidth < 40 || text.isEmpty()) return text;
    QString out;
    QString line;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\n')) {
            out += line;
            out += QLatin1Char('\n');
            line.clear();
            continue;
        }
        const QString trial = line + ch;
        if (fm.horizontalAdvance(trial) > maxWidth && !line.isEmpty()) {
            out += line;
            out += QLatin1Char('\n');
            line = ch;
        } else {
            line = trial;
        }
    }
    out += line;
    return out;
}

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

    m_panel = new QWidget(this);
    m_panel->setObjectName(QStringLiteral("busyPanel"));
    m_panel->setAttribute(Qt::WA_TranslucentBackground, true);
    m_panel->setAutoFillBackground(false);
    m_panel->setStyleSheet(QStringLiteral(
        "QWidget#busyPanel {"
        "  background-color: rgba(12, 18, 24, 210);"
        "  border: 1px solid #2d3f4f;"
        "  border-radius: 10px;"
        "}"));

    m_spinner = new SpinnerWidget(m_panel);
    m_label = new QLabel(m_panel);
    m_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    applyBusyLabelStyle();

    auto* layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);
    layout->addWidget(m_spinner, 0, Qt::AlignHCenter);
    layout->addWidget(m_label, 0, Qt::AlignHCenter);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addStretch(1);
    outer->addWidget(m_panel, 0, Qt::AlignHCenter);
    outer->addStretch(1);

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() { endToast(); });

    if (parent) {
        parent->installEventFilter(this);
        syncGeometry();
    }
}

void BusyOverlayWidget::applyBusyLabelStyle() {
    if (!m_label) return;
    m_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_label->setStyleSheet(QStringLiteral(
        "color: #ffffff; font-size: 14px; font-family: \"Microsoft YaHei UI\"; "
        "background: transparent; border: none; padding: 4px 8px;"));
}

void BusyOverlayWidget::applyToastLabelStyle(bool success) {
    if (!m_label) return;
    m_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    const QString color = success ? QStringLiteral("#34d399") : QStringLiteral("#f87171");
    m_label->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 16px; font-weight: bold; font-family: \"Microsoft YaHei UI\"; "
        "background: transparent; border: none; padding: 4px 8px;").arg(color));
}

void BusyOverlayWidget::endToast() {
    if (!m_toastMode) return;
    m_toastMode = false;
    if (m_depth > 0) return;
    applyBusyLabelStyle();
    if (m_spinner) m_spinner->show();
    hide();
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
        if (isVisible() && !m_rawText.isEmpty())
            relayoutLabel(m_rawText);
    }
    return QWidget::eventFilter(watched, event);
}

void BusyOverlayWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    syncGeometry();
    raise();
}

void BusyOverlayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (isVisible() && !m_rawText.isEmpty())
        relayoutLabel(m_rawText);
}

void BusyOverlayWidget::relayoutLabel(const QString& rawText) {
    if (!m_label || !m_panel) return;
    m_rawText = rawText;

    const int overlayW = qMax(320, width());
    // Use most of the overlay width so long paths fit with wrapping.
    const int maxLabelW = qMax(280, overlayW - 80);
    const QFontMetrics fm(m_label->font());
    const QString wrapped = HardWrapToWidth(rawText, fm, maxLabelW - 20);
    m_label->setText(wrapped);
    m_label->setWordWrap(true);

    const QRect bound = fm.boundingRect(
        QRect(0, 0, maxLabelW - 20, 100000),
        Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
        wrapped);
    const int labelH = qMax(40, bound.height() + 12);
    m_label->setFixedSize(maxLabelW, labelH);

    const int panelW = maxLabelW + 56;
    const int spinnerH = (m_spinner && m_spinner->isVisible()) ? (48 + 16) : 0;
    const int panelH = 48 + spinnerH + labelH;
    m_panel->setFixedSize(panelW, panelH);
}

void BusyOverlayWidget::showBusy(const QString& text) {
    if (m_toastMode) {
        m_toastTimer->stop();
        m_toastMode = false;
        applyBusyLabelStyle();
        if (m_spinner) m_spinner->show();
    }
    if (m_depth++ == 0) {
        applyBusyLabelStyle();
        if (m_spinner) m_spinner->show();
        setBusyText(text);
        syncGeometry();
        show();
        raise();
        static_cast<SpinnerWidget*>(m_spinner)->startAnimation();
    } else {
        setBusyText(text);
    }
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void BusyOverlayWidget::hideBusy() {
    if (m_depth <= 0) return;
    if (--m_depth == 0) {
        static_cast<SpinnerWidget*>(m_spinner)->stopAnimation();
        if (!m_toastMode) hide();
    }
}

void BusyOverlayWidget::setBusyText(const QString& text) {
    relayoutLabel(text);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void BusyOverlayWidget::showResultToast(const QString& text, bool success, int durationMs) {
    if (m_depth > 0) {
        setBusyText(text);
        return;
    }
    m_toastTimer->stop();
    m_toastMode = true;
    m_toastSuccess = success;
    applyToastLabelStyle(success);
    if (m_spinner) {
        static_cast<SpinnerWidget*>(m_spinner)->stopAnimation();
        m_spinner->hide();
    }
    syncGeometry();
    show();
    raise();
    relayoutLabel(text);
    m_toastTimer->start(qMax(1, durationMs));
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
