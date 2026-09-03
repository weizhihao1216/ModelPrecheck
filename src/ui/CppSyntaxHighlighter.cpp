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
    const QTextCharFormat keyword = MakeFormat(QColor("#22d3ee"), true);
    const QTextCharFormat type = MakeFormat(QColor("#fde047"), true);
    const QTextCharFormat domain = MakeFormat(QColor("#60a5fa"), true);
    const QTextCharFormat function = MakeFormat(QColor("#c4b5fd"), true);
    const QTextCharFormat member = MakeFormat(QColor("#ffffff"));
    const QTextCharFormat string = MakeFormat(QColor("#4ade80"));
    const QTextCharFormat number = MakeFormat(QColor("#fb923c"));
    const QTextCharFormat preprocessor = MakeFormat(QColor("#ffffff"), true);
    const QTextCharFormat comment = MakeFormat(QColor("#94a3b8"), false, true);

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

    // Generic call sites first; later type/domain rules override known names.
    m_rules.push_back({ QRegularExpression(QStringLiteral("\\b[A-Za-z_]\\w*(?=\\s*\\()")), function });

    const QStringList types = {
        "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t",
        "double", "float", "int", "long", "short", "signed", "unsigned", "void",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t",
        "intmax_t", "uintmax_t", "nullptr_t",
        "string", "wstring", "u16string", "u32string",
        "string_view", "wstring_view",
        "vector", "array", "deque", "list", "forward_list",
        "map", "multimap", "set", "multiset",
        "unordered_map", "unordered_set", "unordered_multimap", "unordered_multiset",
        "pair", "tuple", "optional", "variant", "any", "span",
        "shared_ptr", "unique_ptr", "weak_ptr", "atomic",
        "mutex", "recursive_mutex", "shared_mutex", "condition_variable",
        "thread", "future", "promise", "packaged_task",
        "ifstream", "ofstream", "fstream", "stringstream",
        "ostringstream", "istringstream", "ostream", "istream", "iostream",
        "FILE", "HANDLE", "HMODULE", "HWND", "DWORD", "WORD", "BYTE", "BOOL",
        "HRESULT", "LPCSTR", "LPCWSTR", "LPSTR", "LPWSTR",
        "RandomBag", "WeaponModelParams", "WeaponModelOutput", "RandomValueBlob",
        "WeaponObject", "TrajectorySample", "QString", "QByteArray", "QStringList"
    };
    for (const QString& word : types) {
        m_rules.push_back({ QRegularExpression(QStringLiteral("\\b%1\\b").arg(word)), type });
    }

    // std::string / std::vector / std::chrono::milliseconds / cv::Mat ...
    m_rules.push_back({
        QRegularExpression(QStringLiteral(
            "\\b(?:std|cv|boost|glm|Eigen)"
            "(?:::[A-Za-z_]\\w*)+\\b")),
        type
    });
    // PascalCase or *_t / *_type before '<'
    m_rules.push_back({
        QRegularExpression(QStringLiteral(
            "\\b(?:[A-Z][A-Za-z0-9_]*|[A-Za-z_]\\w*_(?:t|type|ptr|ref))\\b(?=\\s*<)")),
        type
    });

    const QStringList domainWords = {
        "Model_Create", "Model_Init", "Model_Step", "Model_Destroy", "Model_GetInfo",
        "MoCreate", "MoInit", "MoStep", "MoDestroy",
        "RecordTrajectoryPoint", "UserMain", "out_lat", "out_lon"
    };
    for (const QString& word : domainWords) {
        m_rules.push_back({ QRegularExpression(QStringLiteral("\\b%1\\b").arg(word)), domain });
    }

    m_rules.push_back({ QRegularExpression(QStringLiteral("\\.\\s*([A-Za-z_]\\w*)")), member });
    m_rules.push_back({ QRegularExpression(QStringLiteral("->\\s*([A-Za-z_]\\w*)")), member });

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
