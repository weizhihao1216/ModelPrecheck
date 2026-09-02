#include "CppCodeEditor.h"
#include "CppSyntaxHighlighter.h"

#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QFontDatabase>

CppCodeEditor::CppCodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_highlighter(new CppSyntaxHighlighter(document())) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setFamily(QStringLiteral("Consolas"));
    font.setPointSize(10);
    setFont(font);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    setObjectName(QStringLiteral("userMainEditor"));
    setPlaceholderText(QStringLiteral("选择有效模型包后，在此编写 UserMain 函数体…"));

    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CppCodeEditor::highlightCurrentLine);
    highlightCurrentLine();
}

QString CppCodeEditor::currentIndent() const {
    const QString line = textCursor().block().text();
    int count = 0;
    while (count < line.size() && (line.at(count) == QLatin1Char(' ') || line.at(count) == QLatin1Char('\t'))) {
        ++count;
    }
    return line.left(count);
}

void CppCodeEditor::indentSelection(bool removeIndent) {
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        if (!removeIndent) cursor.insertText(m_indentUnit);
        return;
    }

    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    cursor.setPosition(start);
    cursor.movePosition(QTextCursor::StartOfBlock);
    start = cursor.position();
    cursor.setPosition(end);
    if (cursor.atBlockStart() && end > start) cursor.movePosition(QTextCursor::PreviousBlock);
    end = cursor.block().position() + cursor.block().length() - 1;

    cursor.beginEditBlock();
    cursor.setPosition(start);
    while (cursor.position() <= end) {
        cursor.movePosition(QTextCursor::StartOfBlock);
        if (removeIndent) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m_indentUnit.size());
            QString selected = cursor.selectedText();
            int removeCount = 0;
            while (removeCount < selected.size() && removeCount < m_indentUnit.size()
                   && selected.at(removeCount) == QLatin1Char(' ')) {
                ++removeCount;
            }
            cursor.clearSelection();
            if (removeCount > 0) {
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, removeCount);
                cursor.removeSelectedText();
                end -= removeCount;
            }
        } else {
            cursor.insertText(m_indentUnit);
            end += m_indentUnit.size();
        }
        if (!cursor.movePosition(QTextCursor::NextBlock)) break;
    }
    cursor.endEditBlock();
}

void CppCodeEditor::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
        indentSelection(false);
        return;
    }
    if (event->key() == Qt::Key_Backtab
        || (event->key() == Qt::Key_Tab && event->modifiers().testFlag(Qt::ShiftModifier))) {
        indentSelection(true);
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const QString indent = currentIndent();
        const QString trimmed = textCursor().block().text().trimmed();
        QPlainTextEdit::keyPressEvent(event);
        textCursor().insertText(indent + (trimmed.endsWith(QLatin1Char('{')) ? m_indentUnit : QString()));
        return;
    }
    if (event->text() == QStringLiteral("}")) {
        QTextCursor cursor = textCursor();
        const QString beforeCursor = cursor.block().text().left(cursor.positionInBlock());
        if (beforeCursor.trimmed().isEmpty() && beforeCursor.endsWith(m_indentUnit)) {
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, m_indentUnit.size());
            cursor.removeSelectedText();
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void CppCodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor("#25253a"));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    setExtraSelections(selections);
}
