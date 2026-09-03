#include "CppCodeCompletion.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace {

void insertUnique(QStringList* out, QSet<QString>& seen, const QString& symbol) {
    if (symbol.size() < 2 || seen.contains(symbol)) return;
    seen.insert(symbol);
    out->append(symbol);
}

void extractFromText(const QString& text, QStringList* out, QSet<QString>* seen) {
    static const QRegularExpression structRe(
        QStringLiteral("\\b(?:struct|class|enum(?:\\s+class)?)\\s+([A-Za-z_]\\w*)"));
    static const QRegularExpression usingRe(
        QStringLiteral("\\busing\\s+([A-Za-z_]\\w*)\\s*="));
    static const QRegularExpression typedefRe(
        QStringLiteral("\\btypedef\\s+[\\w\\s\\*&,<>:\\.]+\\s+([A-Za-z_]\\w*)\\s*;"));
    static const QRegularExpression funcRe(
        QStringLiteral("\\b([A-Za-z_]\\w*)\\s*\\([^;{]*\\)\\s*(?:const)?\\s*;"));
    static const QRegularExpression fieldRe(
        QStringLiteral("\\b([A-Za-z_]\\w*)\\s*;"));

    auto collect = [&](const QRegularExpression& re) {
        QRegularExpressionMatchIterator it = re.globalMatch(text);
        while (it.hasNext()) {
            insertUnique(out, *seen, it.next().captured(1));
        }
    };

    collect(structRe);
    collect(usingRe);
    collect(typedefRe);
    collect(funcRe);

    for (QRegularExpressionMatchIterator it = fieldRe.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        static const QStringList skip = {
            QStringLiteral("if"), QStringLiteral("for"), QStringLiteral("while"),
            QStringLiteral("return"), QStringLiteral("else"), QStringLiteral("case")
        };
        if (!skip.contains(name)) insertUnique(out, *seen, name);
    }
}

} // namespace

QStringList CppCodeCompletion::builtinSymbols() {
    return QStringList{
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("auto"),
        QStringLiteral("bool"), QStringLiteral("break"), QStringLiteral("case"),
        QStringLiteral("catch"), QStringLiteral("char"), QStringLiteral("class"),
        QStringLiteral("const"), QStringLiteral("constexpr"), QStringLiteral("continue"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("double"), QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("extern"), QStringLiteral("false"), QStringLiteral("float"),
        QStringLiteral("for"), QStringLiteral("if"), QStringLiteral("inline"),
        QStringLiteral("int"), QStringLiteral("long"), QStringLiteral("namespace"),
        QStringLiteral("new"), QStringLiteral("nullptr"), QStringLiteral("operator"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("return"), QStringLiteral("short"), QStringLiteral("signed"),
        QStringLiteral("sizeof"), QStringLiteral("static"), QStringLiteral("struct"),
        QStringLiteral("switch"), QStringLiteral("template"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"),
        QStringLiteral("typedef"), QStringLiteral("typename"), QStringLiteral("union"),
        QStringLiteral("unsigned"), QStringLiteral("using"), QStringLiteral("virtual"),
        QStringLiteral("void"), QStringLiteral("volatile"), QStringLiteral("while"),
        QStringLiteral("size_t"), QStringLiteral("ssize_t"), QStringLiteral("ptrdiff_t"),
        QStringLiteral("int8_t"), QStringLiteral("int16_t"), QStringLiteral("int32_t"),
        QStringLiteral("int64_t"), QStringLiteral("uint8_t"), QStringLiteral("uint16_t"),
        QStringLiteral("uint32_t"), QStringLiteral("uint64_t"),
        QStringLiteral("std::string"), QStringLiteral("std::wstring"),
        QStringLiteral("std::vector"), QStringLiteral("std::array"),
        QStringLiteral("std::map"), QStringLiteral("std::set"),
        QStringLiteral("std::unordered_map"), QStringLiteral("std::shared_ptr"),
        QStringLiteral("std::unique_ptr"), QStringLiteral("std::optional"),
        QStringLiteral("std::pair"), QStringLiteral("std::tuple"),
        QStringLiteral("string"), QStringLiteral("vector"), QStringLiteral("map"),
        QStringLiteral("RandomBag"), QStringLiteral("WeaponModelParams"),
        QStringLiteral("WeaponModelOutput"), QStringLiteral("RandomValueBlob"),
        QStringLiteral("WeaponObject"), QStringLiteral("Model_Create"), QStringLiteral("Model_Init"),
        QStringLiteral("Model_Step"), QStringLiteral("Model_Destroy"),
        QStringLiteral("Model_GetInfo"), QStringLiteral("MoCreate"),
        QStringLiteral("MoInit"), QStringLiteral("MoStep"), QStringLiteral("MoDestroy"),
        QStringLiteral("RecordTrajectoryPoint"), QStringLiteral("out_lat"),
        QStringLiteral("out_lon"), QStringLiteral("UserMain")
    };
}

QStringList CppCodeCompletion::symbolsFromHeaders(const QStringList& headerPaths) {
    QStringList out;
    QSet<QString> seen;
    for (const QString& path : headerPaths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        QString text = stream.readAll();
        text.remove(QRegularExpression(QStringLiteral("//[^\n]*")));
        text.remove(QRegularExpression(QStringLiteral("/\\*[\\s\\S]*?\\*/")));
        extractFromText(text, &out, &seen);
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

QStringList CppCodeCompletion::mergeSymbols(const QStringList& extra) {
    QSet<QString> seen;
    QStringList merged;
    for (const QString& word : builtinSymbols()) {
        insertUnique(&merged, seen, word);
    }
    for (const QString& word : extra) {
        insertUnique(&merged, seen, word);
    }
    merged.sort(Qt::CaseInsensitive);
    return merged;
}
