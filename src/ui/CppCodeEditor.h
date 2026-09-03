#ifndef CPP_CODE_EDITOR_H
#define CPP_CODE_EDITOR_H

#include <QStringList>
#include <Qsci/qsciscintilla.h>

class QsciLexerCPP;
class QsciAPIs;
class QWheelEvent;

/** C++ code editor backed by QScintilla. */
class CppCodeEditor : public QsciScintilla {
    Q_OBJECT
public:
    explicit CppCodeEditor(QWidget* parent = nullptr);
    ~CppCodeEditor() override;

    void setCompletionSymbols(const QStringList& symbols);
    void refreshCompletionsFromHeaders(const QStringList& headerPaths);

    // Compatibility helpers used by MainWindow (QPlainTextEdit-like API).
    QString toPlainText() const;
    void setPlainText(const QString& text);
    void clear();

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    void applyDarkTheme();
    void applyFont();
    void rebuildApis(const QStringList& symbols);

    QsciLexerCPP* m_lexer = nullptr;
    QsciAPIs* m_apis = nullptr;
    QStringList m_completionSymbols;
};

#endif // CPP_CODE_EDITOR_H
