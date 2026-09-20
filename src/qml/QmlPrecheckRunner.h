#ifndef QML_PRECHECK_RUNNER_H
#define QML_PRECHECK_RUNNER_H

#include <functional>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "core/SessionStore.h"

struct QmlPrecheckResult {
    QVariantList items;
    QVariantList models;
    QVariantList concurrency;
    QVariantMap headerConflicts;
    QVariantList multiObject;
    int passCount = 0;
    int warnCount = 0;
    int failCount = 0;
    int pendingCount = 0;
    QString timestamp;
    bool overallPass = false;
    QString reportPath;
    QString error;
};

class QmlPrecheckRunner {
public:
    using ProgressCallback = std::function<void(const QString&)>;

    static QmlPrecheckResult Run(const SessionSnapshot& snapshot,
                                 const ProgressCallback& progress = {});
};

#endif // QML_PRECHECK_RUNNER_H
