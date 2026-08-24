#ifndef LOG_CONSOLE_WIDGET_H
#define LOG_CONSOLE_WIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>

class LogConsoleWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogConsoleWidget(QWidget* parent = nullptr);
    ~LogConsoleWidget();

public slots:
    void appendLog(const QString& text);
    void clearLog();

private slots:
    void filterLog(const QString& keyword);

private:
    QTextEdit* m_pTextEdit;
    QLineEdit* m_pFilterInput;
    QPushButton* m_pClearBtn;
};

#endif // LOG_CONSOLE_WIDGET_H
