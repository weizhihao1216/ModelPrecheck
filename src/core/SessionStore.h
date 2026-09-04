#ifndef SESSION_STORE_H
#define SESSION_STORE_H

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <vector>

#include "UserCodeHarness.h"

/** Serializable snapshot of last UI / model-edit session. */
struct SessionModelSnapshot {
    QString name;
    QString packageDir;
    QStringList headerPaths;
    QString userMainBody;
    QString userMultiObjectBody;
    std::vector<RandomVarDef> randomVars;
    int instanceCount = 1;
    int multiObjectCount = 3;
    int multiObjectSteps = 100;
    double multiObjectDt = 0.02;
    double multiObjectTolerance = 1e-8;
    int multiObjectSchedule = 0;
    QString status;
    QString lastUserHarnessDll;
    QString lastMultiObjectHarnessDll;
};

struct SessionSnapshot {
    int version = 1;
    QDateTime savedAt;
    int currentModelIndex = -1;
    int navigationRow = -1;
    int perfSteps = 10000;
    double perfHz = 50.0;
    int threadCount = 4;
    QByteArray windowGeometry;
    std::vector<SessionModelSnapshot> models;
};

class SessionStore {
public:
    static QString DefaultPath();
    static bool Exists(const QString& path = QString());
    static bool Save(const SessionSnapshot& snapshot, QString* error = nullptr,
                     const QString& path = QString());
    static bool Load(SessionSnapshot& snapshot, QString* error = nullptr,
                     const QString& path = QString());
    /** Short Chinese summary for the restore prompt. */
    static QString Summarize(const SessionSnapshot& snapshot);
};

#endif // SESSION_STORE_H
