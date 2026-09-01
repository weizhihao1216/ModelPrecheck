#include "LogConsoleWidget.h"
#include <QDateTime>
#include <QLabel>

LogConsoleWidget::LogConsoleWidget(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout* topLayout = new QHBoxLayout();
    QLabel* label = new QLabel("<b>控制台 / 运行日志</b>", this);
    m_pFilterInput = new QLineEdit(this);
    m_pFilterInput->setPlaceholderText("过滤日志关键词...");
    m_pClearBtn = new QPushButton("清空日志", this);

    topLayout->addWidget(label);
    topLayout->addWidget(m_pFilterInput, 1);
    topLayout->addWidget(m_pClearBtn);

    m_pTextEdit = new QTextEdit(this);
    m_pTextEdit->setReadOnly(true);
    m_pTextEdit->setStyleSheet(
        "QTextEdit { background-color: #ffffff; color: #003986; "
        "font-family: 'Microsoft YaHei UI', Consolas, 'Courier New', monospace; "
        "font-size: 12px; border: 1px solid #a0b3c0; border-radius: 4px; }");

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_pTextEdit, 1);

    connect(m_pClearBtn, &QPushButton::clicked, this, &LogConsoleWidget::clearLog);
    connect(m_pFilterInput, &QLineEdit::textChanged, this, &LogConsoleWidget::filterLog);
}

LogConsoleWidget::~LogConsoleWidget() {
}

void LogConsoleWidget::appendLog(const QString& text) {
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString formatted;

    if (text.startsWith("ERROR") || text.contains("Exception") || text.contains("FAIL")) {
        formatted = QString("<span style=\"color:#dc2626; font-weight:bold;\">[%1] %2</span>").arg(timeStr, text.toHtmlEscaped());
    } else if (text.startsWith("WARNING") || text.contains("WARN")) {
        formatted = QString("<span style=\"color:#d97706; font-weight:bold;\">[%1] %2</span>").arg(timeStr, text.toHtmlEscaped());
    } else if (text.startsWith("SUCCESS") || text.contains("PASS")) {
        formatted = QString("<span style=\"color:#16a34a; font-weight:bold;\">[%1] %2</span>").arg(timeStr, text.toHtmlEscaped());
    } else {
        formatted = QString("<span style=\"color:#2563eb; font-weight:bold;\">[%1]</span> <span style=\"color:#1e293b;\">%2</span>").arg(timeStr, text.toHtmlEscaped());
    }

    m_pTextEdit->append(formatted);
}

void LogConsoleWidget::clearLog() {
    m_pTextEdit->clear();
}

void LogConsoleWidget::filterLog(const QString& keyword) {
    // Basic filter placeholder logic
}
