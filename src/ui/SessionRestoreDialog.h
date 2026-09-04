#ifndef SESSION_RESTORE_DIALOG_H
#define SESSION_RESTORE_DIALOG_H

#include <QDialog>
#include <QPoint>
#include "../core/SessionStore.h"

class QMouseEvent;

/** Dark-themed restore prompt matching Model Validator UI (frameless). */
class SessionRestoreDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionRestoreDialog(const SessionSnapshot& snapshot,
                                  QWidget* parent = nullptr);

    /** true = restore, false = start fresh */
    bool shouldRestore() const { return m_restore; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool m_restore = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

#endif // SESSION_RESTORE_DIALOG_H
