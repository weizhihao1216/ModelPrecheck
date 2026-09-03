#include "CppCodeEditor.h"
#include "CppCodeCompletion.h"

#include <QFont>
#include <QFontDatabase>
#include <QColor>
#include <QWheelEvent>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qsciapis.h>

CppCodeEditor::CppCodeEditor(QWidget* parent)
    : QsciScintilla(parent)
    , m_completionSymbols(CppCodeCompletion::builtinSymbols()) {
    setObjectName(QStringLiteral("userMainEditor"));
    setUtf8(true);
    setToolTip(QStringLiteral("选择有效模型包后，在此编写 UserMain / 多对象代码…"));

    m_lexer = new QsciLexerCPP(this);
    setLexer(m_lexer);
    applyFont();
    applyDarkTheme();

    setMarginType(0, QsciScintilla::NumberMargin);
    setMarginWidth(0, QStringLiteral("0000"));
    setMarginLineNumbers(0, true);
    setMarginsBackgroundColor(QColor("#0a1014"));
    setMarginsForegroundColor(QColor("#64748b"));

    setBraceMatching(QsciScintilla::SloppyBraceMatch);
    setMatchedBraceBackgroundColor(QColor("#134e4a"));
    setMatchedBraceForegroundColor(QColor("#ffffff"));
    setUnmatchedBraceBackgroundColor(QColor("#7f1d1d"));
    setUnmatchedBraceForegroundColor(QColor("#ffffff"));

    setIndentationsUseTabs(false);
    setTabWidth(4);
    setAutoIndent(true);
    setIndentationGuides(true);
    setCaretLineVisible(true);
    setCaretLineBackgroundColor(QColor("#1e3a34"));
    setCaretForegroundColor(QColor("#ffffff"));
    setCaretWidth(2);

    setAutoCompletionSource(QsciScintilla::AcsAPIs);
    setAutoCompletionThreshold(1);
    setAutoCompletionCaseSensitivity(false);
    setAutoCompletionReplaceWord(true);
    setAutoCompletionUseSingle(QsciScintilla::AcusNever);
    setCallTipsStyle(QsciScintilla::CallTipsNoContext);

    setFolding(QsciScintilla::PlainFoldStyle, 2);
    setFoldMarginColors(QColor("#0a1014"), QColor("#1a242e"));

    setEdgeMode(QsciScintilla::EdgeNone);
    setWhitespaceVisibility(QsciScintilla::WsInvisible);
    setEolMode(QsciScintilla::EolWindows);
    setWrapMode(QsciScintilla::WrapNone);

    rebuildApis(m_completionSymbols);
}

CppCodeEditor::~CppCodeEditor() = default;

void CppCodeEditor::applyFont() {
    const QStringList candidates = {
        QStringLiteral("Cascadia Code"),
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Source Code Pro"),
        QStringLiteral("Fira Code"),
        QStringLiteral("Consolas"),
        QStringLiteral("Courier New")
    };
    QString chosen = QStringLiteral("Consolas");
    QFontDatabase db;
    for (const QString& family : candidates) {
        if (!db.families().filter(family, Qt::CaseInsensitive).isEmpty()) {
            chosen = family;
            break;
        }
    }
    QFont font(chosen, 11);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    setFont(font);
    if (m_lexer) m_lexer->setFont(font);
}

void CppCodeEditor::wheelEvent(QWheelEvent* event) {
    // Let QScintilla handle Ctrl+wheel zoom (and normal scrolling), then accept so
    // the outer QScrollArea / parent page does not also scroll.
    QsciScintilla::wheelEvent(event);
    event->accept();
}

void CppCodeEditor::applyDarkTheme() {
    const QColor bg("#0a1014");
    const QColor fg("#e2e8f0");
    setPaper(bg);
    setColor(fg);

    if (!m_lexer) return;
    m_lexer->setDefaultPaper(bg);
    m_lexer->setDefaultColor(fg);

    auto style = [this](int id, const QColor& color, bool bold = false) {
        m_lexer->setColor(color, id);
        m_lexer->setPaper(QColor("#0a1014"), id);
        QFont f = m_lexer->font(id);
        f.setBold(bold);
        m_lexer->setFont(f, id);
    };

    style(QsciLexerCPP::Default, fg);
    style(QsciLexerCPP::Comment, QColor("#94a3b8"));
    style(QsciLexerCPP::CommentLine, QColor("#94a3b8"));
    style(QsciLexerCPP::CommentDoc, QColor("#94a3b8"));
    style(QsciLexerCPP::CommentLineDoc, QColor("#94a3b8"));
    style(QsciLexerCPP::Number, QColor("#fb923c"));
    style(QsciLexerCPP::Keyword, QColor("#22d3ee"), true);
    style(QsciLexerCPP::KeywordSet2, QColor("#fde047"), true);
    style(QsciLexerCPP::DoubleQuotedString, QColor("#4ade80"));
    style(QsciLexerCPP::SingleQuotedString, QColor("#4ade80"));
    style(QsciLexerCPP::RawString, QColor("#4ade80"));
    style(QsciLexerCPP::HashQuotedString, QColor("#4ade80"));
    style(QsciLexerCPP::PreProcessor, QColor("#ffffff"), true);
    style(QsciLexerCPP::Operator, QColor("#e2e8f0"));
    style(QsciLexerCPP::Identifier, fg);
    style(QsciLexerCPP::UnclosedString, QColor("#f87171"));
    style(QsciLexerCPP::VerbatimString, QColor("#4ade80"));
    style(QsciLexerCPP::Regex, QColor("#c4b5fd"));
    style(QsciLexerCPP::CommentDocKeyword, QColor("#60a5fa"));
    style(QsciLexerCPP::CommentDocKeywordError, QColor("#f87171"));
    style(QsciLexerCPP::GlobalClass, QColor("#fde047"), true);
    style(QsciLexerCPP::UUID, QColor("#fbbf24"));
    style(QsciLexerCPP::TripleQuotedVerbatimString, QColor("#4ade80"));
    style(QsciLexerCPP::PreProcessorComment, QColor("#94a3b8"));
    style(QsciLexerCPP::PreProcessorCommentLineDoc, QColor("#94a3b8"));
    style(QsciLexerCPP::UserLiteral, QColor("#fb923c"));
    style(QsciLexerCPP::TaskMarker, QColor("#fbbf24"), true);
    style(QsciLexerCPP::EscapeSequence, QColor("#86efac"));

    setSelectionBackgroundColor(QColor("#134e4a"));
    setSelectionForegroundColor(QColor("#ffffff"));

    // Secondary keyword sets: types (set 1) and domain identifiers (set 3).
    static const char kTypeKeywords[] =
        "bool char char8_t char16_t char32_t wchar_t double float int long short "
        "signed unsigned void size_t ssize_t ptrdiff_t intptr_t uintptr_t "
        "int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t "
        "string wstring vector map set unordered_map shared_ptr unique_ptr "
        "optional pair tuple atomic mutex thread QString "
        "RandomBag WeaponModelParams WeaponModelOutput RandomValueBlob "
        "WeaponObject TrajectorySample";
    static const char kDomainKeywords[] =
        "Model_Create Model_Init Model_Step Model_Destroy Model_GetInfo "
        "MoCreate MoInit MoStep MoDestroy RecordTrajectoryPoint UserMain "
        "out_lat out_lon";
    SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS, 1, kTypeKeywords);
    SendScintilla(QsciScintillaBase::SCI_SETKEYWORDS, 3, kDomainKeywords);
}

void CppCodeEditor::rebuildApis(const QStringList& symbols) {
    if (!m_lexer) return;
    delete m_apis;
    m_apis = new QsciAPIs(m_lexer);
    for (const QString& symbol : symbols) {
        if (!symbol.trimmed().isEmpty()) m_apis->add(symbol);
    }
    m_apis->prepare();
    m_lexer->setAPIs(m_apis);
}

void CppCodeEditor::setCompletionSymbols(const QStringList& symbols) {
    m_completionSymbols = CppCodeCompletion::mergeSymbols(symbols);
    rebuildApis(m_completionSymbols);
}

void CppCodeEditor::refreshCompletionsFromHeaders(const QStringList& headerPaths) {
    setCompletionSymbols(CppCodeCompletion::symbolsFromHeaders(headerPaths));
}

QString CppCodeEditor::toPlainText() const {
    return text();
}

void CppCodeEditor::setPlainText(const QString& text) {
    setText(text);
}

void CppCodeEditor::clear() {
    setText(QString());
}
