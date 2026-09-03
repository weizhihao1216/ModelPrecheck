#ifndef CPP_CODE_COMPLETION_H
#define CPP_CODE_COMPLETION_H

#include <QStringList>

/** Built-in C++/domain symbols plus simple header parsing for editor completion. */
class CppCodeCompletion {
public:
    static QStringList builtinSymbols();
    static QStringList symbolsFromHeaders(const QStringList& headerPaths);
    static QStringList mergeSymbols(const QStringList& extra);
};

#endif // CPP_CODE_COMPLETION_H
