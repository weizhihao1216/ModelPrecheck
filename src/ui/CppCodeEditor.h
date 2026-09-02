#ifndef CPP_CODE_EDITOR_H
#define CPP_CODE_EDITOR_H

#include <QPlainTextEdit>

class CppSyntaxHighlighter;
class QKeyEvent;

class CppCodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CppCodeEditor(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void highlightCurrentLine();

private:
    QString currentIndent() const;
    void indentSelection(bool removeIndent);

    CppSyntaxHighlighter* m_highlighter;
    const QString m_indentUnit = QStringLiteral("    ");
};

#endif // CPP_CODE_EDITOR_H
