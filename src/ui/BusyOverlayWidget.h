#ifndef BUSY_OVERLAY_WIDGET_H
#define BUSY_OVERLAY_WIDGET_H

#include <QWidget>
#include <functional>

class QLabel;
class QTimer;

/** Transparent overlay with spinning wait animation / result toast. */
class BusyOverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit BusyOverlayWidget(QWidget* parent = nullptr);

    void showBusy(const QString& text);
    void hideBusy();
    void setBusyText(const QString& text);
    bool isBusy() const { return m_depth > 0; }

    /** Same place as busy wait: brief success/failure tip, then auto-hide. */
    void showResultToast(const QString& text, bool success, int durationMs = 2500);

    /** Run blocking work on a worker thread so the spinner can keep rotating. */
    void runBlocking(const std::function<void()>& work);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void syncGeometry();
    void applyBusyLabelStyle();
    void applyToastLabelStyle(bool success);
    void endToast();
    void relayoutLabel(const QString& rawText);

    QWidget* m_panel = nullptr;
    QWidget* m_spinner = nullptr;
    QLabel* m_label = nullptr;
    QTimer* m_toastTimer = nullptr;
    int m_depth = 0;
    bool m_toastMode = false;
    bool m_toastSuccess = true;
    QString m_rawText;
};

#endif // BUSY_OVERLAY_WIDGET_H
