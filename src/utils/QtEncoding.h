#ifndef QT_ENCODING_H
#define QT_ENCODING_H

#include <QString>
#include <QByteArray>
#include <QTextCodec>
#include <string>

// Qt5 QString::fromStdString / toStdString use Latin-1 — wrong for UTF-8 Chinese.
// Project sources are compiled with /utf-8; internal std::string text is UTF-8.

inline QString qUtf8(const std::string& s) {
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

inline std::string qToUtf8(const QString& s) {
    const QByteArray ba = s.toUtf8();
    return std::string(ba.constData(), static_cast<size_t>(ba.size()));
}

// For mixed logs (app UTF-8 + MSVC cl.exe system locale / GBK on Chinese Windows)
inline QString qDecodeLog(const std::string& s) {
    if (s.empty()) return QString();
    QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
    if (utf8) {
        QTextCodec::ConverterState state;
        QString u = utf8->toUnicode(s.data(), static_cast<int>(s.size()), &state);
        if (state.invalidChars == 0) return u;
    }
    return QString::fromLocal8Bit(s.data(), static_cast<int>(s.size()));
}

#endif // QT_ENCODING_H
