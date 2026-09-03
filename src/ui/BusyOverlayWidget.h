#ifndef BUSY_OVERLAY_WIDGET_H
#define BUSY_OVERLAY_WIDGET_H

#include <QWidget>
#include <functional>

class QLabel;

/** Transparent overlay with spinning wait animation. */
class BusyOverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit BusyOverlayWidget(QWidget* parent = nullptr);

    void showBusy(const QString& text);
    void hideBusy();
    void setBusyText(const QString& text);
    bool isBusy() const { return m_depth > 0; }

    /** Run blocking work on a worker thread so the spinner can keep rotating. */
    void runBlocking(const std::function<void()>& work);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void syncGeometry();

    QWidget* m_spinner = nullptr;
    QLabel* m_label = nullptr;
    int m_depth = 0;
};

#endif // BUSY_OVERLAY_WIDGET_H
