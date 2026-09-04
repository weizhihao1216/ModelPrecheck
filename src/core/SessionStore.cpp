#include "SessionStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QJsonObject randomVarToJson(const RandomVarDef& var) {
    QJsonObject o;
    o.insert(QStringLiteral("name"), QString::fromStdString(var.name));
    o.insert(QStringLiteral("type"), static_cast<int>(var.type));
    o.insert(QStringLiteral("minValue"), var.minValue);
    o.insert(QStringLiteral("maxValue"), var.maxValue);
    o.insert(QStringLiteral("enabled"), var.enabled);
    return o;
}

RandomVarDef randomVarFromJson(const QJsonObject& o) {
    RandomVarDef var;
    var.name = o.value(QStringLiteral("name")).toString().toStdString();
    var.type = (o.value(QStringLiteral("type")).toInt() == 1)
        ? RandomVarType::Int : RandomVarType::Double;
    var.minValue = o.value(QStringLiteral("minValue")).toDouble();
    var.maxValue = o.value(QStringLiteral("maxValue")).toDouble(1.0);
    var.enabled = o.value(QStringLiteral("enabled")).toBool(true);
    return var;
}

QJsonObject modelToJson(const SessionModelSnapshot& model) {
    QJsonObject o;
    o.insert(QStringLiteral("name"), model.name);
    o.insert(QStringLiteral("packageDir"), model.packageDir);
    QJsonArray headers;
    for (const QString& h : model.headerPaths)
        headers.append(h);
    o.insert(QStringLiteral("headerPaths"), headers);
    o.insert(QStringLiteral("userMainBody"), model.userMainBody);
    o.insert(QStringLiteral("userMultiObjectBody"), model.userMultiObjectBody);
    QJsonArray vars;
    for (const auto& v : model.randomVars)
        vars.append(randomVarToJson(v));
    o.insert(QStringLiteral("randomVars"), vars);
    o.insert(QStringLiteral("instanceCount"), model.instanceCount);
    o.insert(QStringLiteral("multiObjectCount"), model.multiObjectCount);
    o.insert(QStringLiteral("multiObjectSteps"), model.multiObjectSteps);
    o.insert(QStringLiteral("multiObjectDt"), model.multiObjectDt);
    o.insert(QStringLiteral("multiObjectTolerance"), model.multiObjectTolerance);
    o.insert(QStringLiteral("multiObjectSchedule"), model.multiObjectSchedule);
    o.insert(QStringLiteral("status"), model.status);
    o.insert(QStringLiteral("lastUserHarnessDll"), model.lastUserHarnessDll);
    o.insert(QStringLiteral("lastMultiObjectHarnessDll"), model.lastMultiObjectHarnessDll);
    return o;
}

SessionModelSnapshot modelFromJson(const QJsonObject& o) {
    SessionModelSnapshot model;
    model.name = o.value(QStringLiteral("name")).toString();
    model.packageDir = o.value(QStringLiteral("packageDir")).toString();
    for (const auto& v : o.value(QStringLiteral("headerPaths")).toArray())
        model.headerPaths.append(v.toString());
    model.userMainBody = o.value(QStringLiteral("userMainBody")).toString();
    model.userMultiObjectBody = o.value(QStringLiteral("userMultiObjectBody")).toString();
    for (const auto& v : o.value(QStringLiteral("randomVars")).toArray())
        model.randomVars.push_back(randomVarFromJson(v.toObject()));
    model.instanceCount = o.value(QStringLiteral("instanceCount")).toInt(1);
    model.multiObjectCount = o.value(QStringLiteral("multiObjectCount")).toInt(3);
    model.multiObjectSteps = o.value(QStringLiteral("multiObjectSteps")).toInt(100);
    model.multiObjectDt = o.value(QStringLiteral("multiObjectDt")).toDouble(0.02);
    model.multiObjectTolerance = o.value(QStringLiteral("multiObjectTolerance")).toDouble(1e-8);
    model.multiObjectSchedule = o.value(QStringLiteral("multiObjectSchedule")).toInt(0);
    model.status = o.value(QStringLiteral("status")).toString(QStringLiteral("未编译"));
    model.lastUserHarnessDll = o.value(QStringLiteral("lastUserHarnessDll")).toString();
    model.lastMultiObjectHarnessDll =
        o.value(QStringLiteral("lastMultiObjectHarnessDll")).toString();
    return model;
}

} // namespace

QString SessionStore::DefaultPath() {
    const QString dir = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("last_session.json"));
}

bool SessionStore::Exists(const QString& path) {
    const QString p = path.isEmpty() ? DefaultPath() : path;
    return QFileInfo::exists(p) && QFileInfo(p).isFile() && QFileInfo(p).size() > 2;
}

bool SessionStore::Save(const SessionSnapshot& snapshot, QString* error, const QString& path) {
    QJsonObject root;
    root.insert(QStringLiteral("version"), snapshot.version);
    root.insert(QStringLiteral("savedAt"),
                snapshot.savedAt.isValid()
                    ? snapshot.savedAt.toString(Qt::ISODate)
                    : QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("currentModelIndex"), snapshot.currentModelIndex);
    root.insert(QStringLiteral("navigationRow"), snapshot.navigationRow);
    root.insert(QStringLiteral("perfSteps"), snapshot.perfSteps);
    root.insert(QStringLiteral("perfHz"), snapshot.perfHz);
    root.insert(QStringLiteral("threadCount"), snapshot.threadCount);
    root.insert(QStringLiteral("windowGeometry"),
                QString::fromLatin1(snapshot.windowGeometry.toBase64()));

    QJsonArray models;
    for (const auto& model : snapshot.models)
        models.append(modelToJson(model));
    root.insert(QStringLiteral("models"), models);

    const QString p = path.isEmpty() ? DefaultPath() : path;
    QFileInfo fi(p);
    if (!fi.dir().mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("无法创建会话目录");
        return false;
    }
    QFile file(p);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("无法写入会话文件: %1").arg(p);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool SessionStore::Load(SessionSnapshot& snapshot, QString* error, const QString& path) {
    const QString p = path.isEmpty() ? DefaultPath() : path;
    QFile file(p);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法读取会话文件: %1").arg(p);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("会话文件 JSON 无效");
        return false;
    }
    const QJsonObject root = doc.object();
    snapshot = SessionSnapshot();
    snapshot.version = root.value(QStringLiteral("version")).toInt(1);
    snapshot.savedAt = QDateTime::fromString(
        root.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
    snapshot.currentModelIndex = root.value(QStringLiteral("currentModelIndex")).toInt(-1);
    snapshot.navigationRow = root.value(QStringLiteral("navigationRow")).toInt(-1);
    snapshot.perfSteps = root.value(QStringLiteral("perfSteps")).toInt(10000);
    snapshot.perfHz = root.value(QStringLiteral("perfHz")).toDouble(50.0);
    snapshot.threadCount = root.value(QStringLiteral("threadCount")).toInt(4);
    snapshot.windowGeometry = QByteArray::fromBase64(
        root.value(QStringLiteral("windowGeometry")).toString().toLatin1());
    for (const auto& v : root.value(QStringLiteral("models")).toArray())
        snapshot.models.push_back(modelFromJson(v.toObject()));
    return true;
}

QString SessionStore::Summarize(const SessionSnapshot& snapshot) {
    QString when = snapshot.savedAt.isValid()
        ? snapshot.savedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("未知时间");
    QStringList names;
    for (const auto& m : snapshot.models) {
        if (!m.name.trimmed().isEmpty())
            names.append(m.name.trimmed());
    }
    QString namePart = names.isEmpty()
        ? QStringLiteral("无名称")
        : names.mid(0, 3).join(QStringLiteral("、"));
    if (names.size() > 3)
        namePart += QStringLiteral(" 等");
    return QStringLiteral("保存时间：%1\n型号数量：%2（%3）")
        .arg(when)
        .arg(snapshot.models.size())
        .arg(namePart);
}
