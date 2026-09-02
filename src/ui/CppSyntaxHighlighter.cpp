#include "CppSyntaxHighlighter.h"

#include <QTextDocument>

namespace {

QTextCharFormat MakeFormat(const QColor& color, bool bold = false, bool italic = false) {
    QTextCharFormat format;
    format.setForeground(color);
    if (bold) format.setFontWeight(QFont::Bold);
    format.setFontItalic(italic);
    return format;
}

} // namespace

CppSyntaxHighlighter::CppSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
    , m_commentStart(QStringLiteral("/\\*"))
    , m_commentEnd(QStringLiteral("\\*/")) {
    const QTextCharFormat keyword = MakeFormat(QColor("#89b4fa"), true);
    const QTextCharFormat type = MakeFormat(QColor("#94e2d5"), true);
    const QTextCharFormat domain = MakeFormat(QColor("#cba6f7"), true);
    const QTextCharFormat string = MakeFormat(QColor("#a6e3a1"));
    const QTextCharFormat number = MakeFormat(QColor("#fab387"));
    const QTextCharFormat preprocessor = MakeFormat(QColor("#f9e2af"));
    const QTextCharFormat comment = MakeFormat(QColor("#7f849c"), false, true);

    const QStringList keywords = {
        "alignas", "alignof", "asm", "auto", "break", "case", "catch", "class",
        "const", "constexpr", "continue", "default", "delete", "do", "else",
        "enum", "explicit", "export", "extern", "for", "friend", "goto", "if",
        "inline", "namespace", "new", "noexcept", "nullptr", "operator", "private",
        "protected", "public", "register", "return", "sizeof", "static", "struct",
        "switch", "template", "this", "throw", "try", "typedef", "typename",
        "union", "using", "virtual", "volatile", "while"
    };
    for (const QString& word : keywords) {
        m_rules.push_back({ QRegularExpression(QStringLiteral("\\b%1\\b").arg(word)), keyword });
    }

    const QStringList types = {
        "bool", "char", "double", "float", "int", "long", "short", "signed",
        "unsigned", "void", "wchar_t", "size_t", "uint32_t", "RandomBag",
        "WeaponModelParams", "WeaponModelOutput"
    };
    for (const QString& word : types) {
        m_rules.push_back({ QRegularExpression(QStringLiteral("\\b%1\\b").arg(word)), type });
    }

    const QStringList domainWords = {
        "Model_Init", "Model_Step", "Model_Destroy", "Model_GetInfo",
        "RecordTrajectoryPoint", "out_lat", "out_lon"
    };
    for (const QString& word : domainWords) {
        m_rules.push_back({ QRegularExpression(QStringLiteral("\\b%1\\b").arg(word)), domain });
    }

    m_rules.push_back({ QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"")), string });
    m_rules.push_back({ QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])'")), string });
    m_rules.push_back({ QRegularExpression(QStringLiteral("\\b(?:0[xX][0-9A-Fa-f]+|\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)\\b")), number });
    m_rules.push_back({ QRegularExpression(QStringLiteral("^\\s*#.*$")), preprocessor });
    m_rules.push_back({ QRegularExpression(QStringLiteral("//.*$")), comment });

    m_blockCommentFormat = comment;
}

void CppSyntaxHighlighter::highlightBlock(const QString& text) {
    for (const Rule& rule : m_rules) {
        QRegularExpressionMatchIterator matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);
    int start = previousBlockState() == 1 ? 0 : text.indexOf(m_commentStart);
    while (start >= 0) {
        const QRegularExpressionMatch endMatch = m_commentEnd.match(text, start + 2);
        int length = 0;
        if (!endMatch.hasMatch()) {
            setCurrentBlockState(1);
            length = text.length() - start;
        } else {
            length = endMatch.capturedEnd() - start;
        }
        setFormat(start, length, m_blockCommentFormat);
        start = endMatch.hasMatch() ? text.indexOf(m_commentStart, start + length) : -1;
    }
}
