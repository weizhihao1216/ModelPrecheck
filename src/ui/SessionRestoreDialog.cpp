#include "SessionRestoreDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QFrame* MakeInfoRow(const QString& label, const QString& value, QWidget* parent) {
    auto* row = new QFrame(parent);
    row->setObjectName(QStringLiteral("sessionInfoRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    auto* key = new QLabel(label, row);
    key->setObjectName(QStringLiteral("sessionInfoKey"));
    key->setMinimumWidth(88);

    auto* val = new QLabel(value, row);
    val->setObjectName(QStringLiteral("sessionInfoValue"));
    val->setWordWrap(true);
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->addWidget(key, 0, Qt::AlignTop);
    layout->addWidget(val, 1);
    return row;
}

} // namespace

SessionRestoreDialog::SessionRestoreDialog(const SessionSnapshot& snapshot,
                                           QWidget* parent)
    : QDialog(parent) {
    setObjectName(QStringLiteral("sessionRestoreDialog"));
    setModal(true);
    // No native title bar / white caption — content is the whole chrome.
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(440);
    setMaximumWidth(520);

    const QString when = snapshot.savedAt.isValid()
        ? snapshot.savedAt.toString(QStringLiteral("yyyy-MM-dd  HH:mm:ss"))
        : QStringLiteral("未知时间");

    QStringList names;
    for (const auto& m : snapshot.models) {
        const QString n = m.name.trimmed();
        if (!n.isEmpty()) names.append(n);
    }
    QString nameText = names.isEmpty()
        ? QStringLiteral("（未命名）")
        : names.mid(0, 4).join(QStringLiteral("、"));
    if (names.size() > 4)
        nameText += QStringLiteral(" 等 %1 个").arg(names.size());

    setStyleSheet(QStringLiteral(
        "QDialog#sessionRestoreDialog {"
        "  background-color: #0f1419;"
        "  border: 1px solid #2d3f4f;"
        "}"
        "QLabel#sessionTitle {"
        "  color: #e2e8f0;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "}"
        "QLabel#sessionSubtitle {"
        "  color: #94a3b8;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 13px;"
        "}"
        "QFrame#sessionAccent {"
        "  background-color: #0d9488;"
        "  border: none;"
        "  max-height: 3px;"
        "  min-height: 3px;"
        "}"
        "QFrame#sessionCard {"
        "  background-color: #141c24;"
        "  border: 1px solid #2d3f4f;"
        "  border-radius: 8px;"
        "}"
        "QFrame#sessionInfoRow {"
        "  background: transparent;"
        "  border: none;"
        "  border-bottom: 1px solid #1e2a36;"
        "}"
        "QLabel#sessionInfoKey {"
        "  color: #14b8a6;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "}"
        "QLabel#sessionInfoValue {"
        "  color: #e2e8f0;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 13px;"
        "}"
        "QLabel#sessionHint {"
        "  color: #64748b;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 11px;"
        "}"
        "QPushButton#sessionBtnRestore {"
        "  background-color: #0d9488;"
        "  color: #ffffff;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "  padding: 8px 22px;"
        "  border-radius: 4px;"
        "  border: 1px solid #14b8a6;"
        "  min-width: 100px;"
        "}"
        "QPushButton#sessionBtnRestore:hover {"
        "  background-color: #fbbf24;"
        "  color: #1c1917;"
        "  border-color: #fbbf24;"
        "}"
        "QPushButton#sessionBtnRestore:pressed {"
        "  background-color: #0f766e;"
        "  color: #ffffff;"
        "  border-color: #0f766e;"
        "}"
        "QPushButton#sessionBtnSkip {"
        "  background-color: #1a242e;"
        "  color: #cbd5e1;"
        "  font-family: \"Microsoft YaHei UI\";"
        "  font-size: 13px;"
        "  padding: 8px 22px;"
        "  border-radius: 4px;"
        "  border: 1px solid #2d3f4f;"
        "  min-width: 100px;"
        "}"
        "QPushButton#sessionBtnSkip:hover {"
        "  background-color: #243040;"
        "  color: #e2e8f0;"
        "  border-color: #3d5163;"
        "}"
        "QPushButton#sessionBtnSkip:pressed {"
        "  background-color: #141c24;"
        "}"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);

    auto* accent = new QFrame(this);
    accent->setObjectName(QStringLiteral("sessionAccent"));
    root->addWidget(accent);

    auto* body = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 20, 24, 20);
    bodyLayout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("检测到上次编辑"), body);
    title->setObjectName(QStringLiteral("sessionTitle"));

    auto* subtitle = new QLabel(
        QStringLiteral("是否恢复型号配置、UserMain / 多对象代码与相关选项？"),
        body);
    subtitle->setObjectName(QStringLiteral("sessionSubtitle"));
    subtitle->setWordWrap(true);

    auto* card = new QFrame(body);
    card->setObjectName(QStringLiteral("sessionCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 4, 0, 4);
    cardLayout->setSpacing(0);
    cardLayout->addWidget(MakeInfoRow(QStringLiteral("保存时间"), when, card));
    cardLayout->addWidget(MakeInfoRow(
        QStringLiteral("型号数量"),
        QStringLiteral("%1 个").arg(snapshot.models.size()), card));
    cardLayout->addWidget(MakeInfoRow(QStringLiteral("型号列表"), nameText, card));

    if (QFrame* last = qobject_cast<QFrame*>(cardLayout->itemAt(cardLayout->count() - 1)->widget()))
        last->setStyleSheet(QStringLiteral(
            "QFrame#sessionInfoRow { background: transparent; border: none; }"));

    auto* hint = new QLabel(
        QStringLiteral("选择「暂不还原」将从空白状态开始；关闭窗口前仍会自动保存当前会话。"),
        body);
    hint->setObjectName(QStringLiteral("sessionHint"));
    hint->setWordWrap(true);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(10);
    buttons->addStretch(1);

    auto* btnSkip = new QPushButton(QStringLiteral("暂不还原"), body);
    btnSkip->setObjectName(QStringLiteral("sessionBtnSkip"));
    btnSkip->setCursor(Qt::PointingHandCursor);
    btnSkip->setDefault(false);
    btnSkip->setAutoDefault(false);

    auto* btnRestore = new QPushButton(QStringLiteral("还原会话"), body);
    btnRestore->setObjectName(QStringLiteral("sessionBtnRestore"));
    btnRestore->setCursor(Qt::PointingHandCursor);
    btnRestore->setDefault(true);
    btnRestore->setAutoDefault(true);

    buttons->addWidget(btnSkip);
    buttons->addWidget(btnRestore);

    bodyLayout->addWidget(title);
    bodyLayout->addWidget(subtitle);
    bodyLayout->addSpacing(4);
    bodyLayout->addWidget(card);
    bodyLayout->addWidget(hint);
    bodyLayout->addSpacing(6);
    bodyLayout->addLayout(buttons);

    root->addWidget(body);

    connect(btnRestore, &QPushButton::clicked, this, [this]() {
        m_restore = true;
        accept();
    });
    connect(btnSkip, &QPushButton::clicked, this, [this]() {
        m_restore = false;
        reject();
    });
}

void SessionRestoreDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void SessionRestoreDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void SessionRestoreDialog::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}
