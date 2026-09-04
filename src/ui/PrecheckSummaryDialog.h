#ifndef PRECHECK_SUMMARY_DIALOG_H
#define PRECHECK_SUMMARY_DIALOG_H

#include <QDialog>
#include <QPoint>
#include "../core/PrecheckSummary.h"

class QMouseEvent;
class QTableWidget;
class QLabel;

/** Frameless dark dialog listing all precheck test items (status / reason / consequence). */
class PrecheckSummaryDialog : public QDialog {
    Q_OBJECT
public:
    explicit PrecheckSummaryDialog(const PrecheckSummaryBoard& board,
                                   QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool m_dragging = false;
    QPoint m_dragOffset;
};

#endif // PRECHECK_SUMMARY_DIALOG_H
