#include "PrecheckSummaryDialog.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "../utils/QtEncoding.h"

PrecheckSummaryDialog::PrecheckSummaryDialog(const PrecheckSummaryBoard& board,
                                             QWidget* parent)
    : QDialog(parent) {
    setObjectName(QStringLiteral("precheckSummaryDialog"));
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(820, 520);
    resize(920, 600);

    setStyleSheet(QStringLiteral(
        "QDialog#precheckSummaryDialog {"
        "  background-color: #0f1419;"
        "  border: 1px solid #2d3f4f;"
        "}"
        "QLabel#precheckTitle {"
        "  color: #e2e8f0;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "}"
        "QLabel#precheckSubtitle {"
        "  color: #94a3b8;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 13px;"
        "}"
        "QFrame#precheckAccent {"
        "  background-color: #0d9488;"
        "  border: none;"
        "  max-height: 3px;"
        "  min-height: 3px;"
        "}"
        "QFrame#precheckCard {"
        "  background-color: #141c24;"
        "  border: 1px solid #2d3f4f;"
        "  border-radius: 8px;"
        "}"
        "QTableWidget#precheckResultTable {"
        "  background-color: #0f1419;"
        "  alternate-background-color: #141c24;"
        "  color: #e2e8f0;"
        "  gridline-color: #1e2a36;"
        "  border: none;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 12px;"
        "  outline: none;"
        "}"
        "QTableWidget#precheckResultTable::item {"
        "  padding: 6px;"
        "  color: #e2e8f0;"
        "}"
        "QTableWidget#precheckResultTable::item:selected {"
        "  background-color: #134e4a;"
        "  color: #e2e8f0;"
        "}"
        "QLabel#statusCellPass, QLabel#statusCellFail, QLabel#statusCellWarn, QLabel#statusCellPending {"
        "  background: transparent;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  padding: 4px 8px;"
        "  qproperty-alignment: AlignCenter;"
        "}"
        "QLabel#statusCellPass { color: #34d399; }"
        "QLabel#statusCellFail { color: #f87171; }"
        "QLabel#statusCellWarn { color: #fbbf24; }"
        "QLabel#statusCellPending { color: #94a3b8; }"
        "QHeaderView::section {"
        "  background-color: #1a242e;"
        "  color: #14b8a6;"
        "  padding: 8px;"
        "  border: none;"
        "  border-bottom: 1px solid #2d3f4f;"
        "  font-weight: bold;"
        "}"
        "QPushButton#precheckOkBtn {"
        "  background-color: #0d9488;"
        "  color: #ffffff;"
        "  border: 1px solid #14b8a6;"
        "  border-radius: 6px;"
        "  padding: 8px 28px;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "}"
        "QPushButton#precheckOkBtn:hover { background-color: #14b8a6; }"
        "QPushButton#precheckOkBtn:pressed { background-color: #0f766e; }"
    ));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* accent = new QFrame(this);
    accent->setObjectName(QStringLiteral("precheckAccent"));
    root->addWidget(accent);

    auto* body = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(20, 16, 20, 16);
    bodyLayout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("一键预检结果总览"), body);
    title->setObjectName(QStringLiteral("precheckTitle"));
    bodyLayout->addWidget(title);

    auto* subtitle = new QLabel(body);
    subtitle->setObjectName(QStringLiteral("precheckSubtitle"));
    subtitle->setTextFormat(Qt::RichText);
    subtitle->setText(
        QStringLiteral("通过 <span style=\"color:#34d399;font-weight:bold;\">%1</span>"
                       " · 未通过 <span style=\"color:#f87171;font-weight:bold;\">%2</span>"
                       " · 警告 <span style=\"color:#fbbf24;font-weight:bold;\">%3</span>"
                       " · <span style=\"color:#94a3b8;\">未测试/跳过 "
                       "<span style=\"color:#94a3b8;font-weight:bold;\">%4</span></span>")
            .arg(board.passCount).arg(board.failCount).arg(board.warnCount)
            .arg(board.notRunCount + board.skippedCount));
    bodyLayout->addWidget(subtitle);

    auto* card = new QFrame(body);
    card->setObjectName(QStringLiteral("precheckCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 8);

    auto* table = new QTableWidget(0, 4, card);
    table->setObjectName(QStringLiteral("precheckResultTable"));
    table->setHorizontalHeaderLabels({
        QStringLiteral("测试项"),
        QStringLiteral("状态"),
        QStringLiteral("具体原因"),
        QStringLiteral("可能导致的情况")
    });
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(true);

    auto statusObjectName = [](TestItemState state) -> QString {
        switch (state) {
        case TestItemState::Pass: return QStringLiteral("statusCellPass");
        case TestItemState::Fail: return QStringLiteral("statusCellFail");
        case TestItemState::Warn: return QStringLiteral("statusCellWarn");
        case TestItemState::NotRun:
        case TestItemState::Skipped: return QStringLiteral("statusCellPending");
        }
        return QStringLiteral("statusCellPending");
    };

    for (const auto& item : board.items) {
        const int row = table->rowCount();
        table->insertRow(row);
        auto* nameItem = new QTableWidgetItem(qUtf8(item.name));
        auto* reasonItem = new QTableWidgetItem(qUtf8(item.reason));
        auto* consequenceItem = new QTableWidgetItem(qUtf8(item.consequence));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        reasonItem->setFlags(reasonItem->flags() & ~Qt::ItemIsEditable);
        consequenceItem->setFlags(consequenceItem->flags() & ~Qt::ItemIsEditable);

        // QLabel 绕过全局 QTableWidget::item { color:#fff }，确保状态色可见
        auto* statusLbl = new QLabel(qUtf8(PrecheckSummary::StateLabel(item.state)), table);
        statusLbl->setObjectName(statusObjectName(item.state));
        statusLbl->setAlignment(Qt::AlignCenter);
        statusLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        table->setItem(row, 0, nameItem);
        table->setCellWidget(row, 1, statusLbl);
        table->setItem(row, 2, reasonItem);
        table->setItem(row, 3, consequenceItem);
    }
    table->resizeRowsToContents();
    cardLayout->addWidget(table);
    bodyLayout->addWidget(card, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    auto* okBtn = new QPushButton(QStringLiteral("知道了"), body);
    okBtn->setObjectName(QStringLiteral("precheckOkBtn"));
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(okBtn);
    bodyLayout->addLayout(btnRow);

    root->addWidget(body, 1);
}

void PrecheckSummaryDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void PrecheckSummaryDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void PrecheckSummaryDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}
