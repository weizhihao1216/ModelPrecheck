#include "QmlAppController.h"
#include "QmlPrecheckRunner.h"

#include <algorithm>
#include <memory>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTime>
#include <QUrl>
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QThread>

#include "core/ConcurrencyTester.h"
#include "core/DllLoader.h"
#include "core/HeaderAnalyzer.h"
#include "core/LibAnalyzer.h"
#include "core/PackageScanner.h"
#include "core/PeAnalyzer.h"
#include "core/PerfProfiler.h"
#include "core/SessionStore.h"
#include "core/FleetSingleThreadMultiObjectTester.h"
#include "core/MultiObjectHarness.h"
#include "core/SingleThreadMultiObjectTester.h"
#include "core/UserCodeHarness.h"
#include "ui/CppSyntaxHighlighter.h"

namespace {

bool IsCompiled(const SessionModelSnapshot& model) {
    return model.status == QStringLiteral("已加载")
        || (!model.lastUserHarnessDll.isEmpty()
            && QFileInfo::exists(model.lastUserHarnessDll));
}

QString DisplayName(const SessionModelSnapshot& model, int index) {
    const QString name = model.name.trimmed();
    return name.isEmpty() ? QStringLiteral("型号 %1").arg(index + 1) : name;
}

std::vector<RandomVarDef> DefaultRandomVars() {
    std::vector<RandomVarDef> vars;
    auto append = [&vars](const char* name, double minimum, double maximum) {
        RandomVarDef value;
        value.name = name;
        value.minValue = minimum;
        value.maxValue = maximum;
        value.enabled = true;
        vars.push_back(value);
    };
    append("lat", 20.0, 50.0);
    append("lon", 100.0, 130.0);
    append("alt", 1000.0, 8000.0);
    append("speed", 200.0, 800.0);
    return vars;
}

QString SafeFolderName(QString name, int index) {
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")),
                 QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("model_%1").arg(index + 1) : name;
}

QVariantList EnrichResultItems(const QVariantList& items) {
    QVariantList enriched;
    for (const QVariant& value : items) {
        QVariantMap row = value.toMap();
        const QString id = row.value(QStringLiteral("id")).toString();
        QString category = QStringLiteral("其他检查");
        if (id == QStringLiteral("header") || id == QStringLiteral("header_conflict"))
            category = QStringLiteral("接口与头文件");
        else if (id == QStringLiteral("lib") || id == QStringLiteral("dll_pe")
                 || id == QStringLiteral("build_config") || id == QStringLiteral("dll_load"))
            category = QStringLiteral("模型包与依赖");
        else if (id == QStringLiteral("perf") || id == QStringLiteral("memory")
                 || id == QStringLiteral("trajectory"))
            category = QStringLiteral("性能与运行");
        else if (id == QStringLiteral("multimodel") || id == QStringLiteral("multithread")
                 || id == QStringLiteral("multiobject"))
            category = QStringLiteral("并发与隔离");
        row.insert(QStringLiteral("category"), category);

        const QString state = row.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("未通过")) {
            row.insert(QStringLiteral("priority"), QStringLiteral("高"));
            row.insert(QStringLiteral("suggestion"),
                       QStringLiteral("建议先修复此项，再重新执行一键预检。"));
        } else if (state == QStringLiteral("警告")) {
            row.insert(QStringLiteral("priority"), QStringLiteral("中"));
            row.insert(QStringLiteral("suggestion"),
                       QStringLiteral("建议确认使用场景是否可接受该风险。"));
        } else if (state == QStringLiteral("通过")) {
            row.insert(QStringLiteral("priority"), QStringLiteral("无"));
            row.insert(QStringLiteral("suggestion"), QStringLiteral("无需处理。"));
        } else {
            row.insert(QStringLiteral("priority"), QStringLiteral("待检查"));
            row.insert(QStringLiteral("suggestion"),
                       QStringLiteral("满足检查条件后重新执行一键预检。"));
        }
        enriched.append(row);
    }
    return enriched;
}

} // namespace

QmlAppController::QmlAppController(QObject* parent)
    : QObject(parent) {
    m_toolchainPath = QString::fromUtf8(UserCodeHarness::FindVcVars64Bat().c_str());
    refresh();
    loadResultSnapshot();
    loadReportHistory();
}

QmlAppController::~QmlAppController() {
    if (m_precheckThread && m_precheckThread->isRunning()) {
        m_precheckThread->quit();
        m_precheckThread->wait();
    }
    if (m_specialtyThread && m_specialtyThread->isRunning()) {
        m_specialtyThread->quit();
        m_specialtyThread->wait();
    }
}

void QmlAppController::refresh() {
    m_snapshot = SessionSnapshot();
    m_restoreSnapshot = SessionSnapshot();
    m_restorePending = false;
    m_hasSnapshot = false;
    QString error;
    SessionSnapshot loaded;
    if (SessionStore::Exists() && SessionStore::Load(loaded, &error)) {
        m_hasSnapshot = true;
        if (!loaded.models.empty()) {
            m_restoreSnapshot = loaded;
            m_restorePending = true;
        } else {
            m_snapshot = loaded;
        }
    }
    for (auto& model : m_snapshot.models) {
        if (model.userMainBody.trimmed().isEmpty()) {
            model.userMainBody = QString::fromUtf8(
                UserCodeHarness::DefaultUserMainTemplate().c_str());
        }
        // 旧会话里没设过判定容差（0）的型号，统一补上默认值。
        if (model.multiObjectTolerance <= 0.0)
            model.multiObjectTolerance = kDefaultMultiObjectTolerance;
    }
    rebuildDashboardFromSnapshot();
    emit settingsChanged();
    emit restoreStateChanged();
}

void QmlAppController::acceptSessionRestore() {
    if (!m_restorePending) return;
    m_snapshot = m_restoreSnapshot;
    for (auto& model : m_snapshot.models) {
        if (model.userMainBody.trimmed().isEmpty()) {
            model.userMainBody = QString::fromUtf8(
                UserCodeHarness::DefaultUserMainTemplate().c_str());
        }
        // 旧会话里没设过判定容差（0）的型号，统一补上默认值。
        if (model.multiObjectTolerance <= 0.0)
            model.multiObjectTolerance = kDefaultMultiObjectTolerance;
    }
    m_restoreSnapshot = SessionSnapshot();
    m_restorePending = false;
    rebuildDashboardFromSnapshot();
    emit settingsChanged();
    emit restoreStateChanged();
}

void QmlAppController::skipSessionRestore() {
    if (!m_restorePending) return;
    m_snapshot = SessionSnapshot();
    m_restoreSnapshot = SessionSnapshot();
    m_restorePending = false;
    m_hasSnapshot = false;
    rebuildDashboardFromSnapshot();
    emit settingsChanged();
    emit restoreStateChanged();
}

void QmlAppController::rebuildDashboardFromSnapshot() {
    m_models.clear();
    m_compiledCount = 0;
    m_readiness = 0;
    m_sessionLabel = m_snapshot.savedAt.isValid()
        ? m_snapshot.savedAt.toString(QStringLiteral("yyyy-MM-dd"))
        : QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));

    int completedUnits = 0;
    const int totalUnits = static_cast<int>(m_snapshot.models.size()) * 3;
    int firstPending = -1;
    for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
        const SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        const bool hasPackage = !model.packageDir.trimmed().isEmpty();
        const bool hasUserMain = !model.userMainBody.trimmed().isEmpty();
        const bool compiled = IsCompiled(model);
        completedUnits += hasPackage ? 1 : 0;
        completedUnits += hasUserMain ? 1 : 0;
        completedUnits += compiled ? 1 : 0;
        if (compiled)
            ++m_compiledCount;
        else if (firstPending < 0)
            firstPending = i;

        QVariantMap row;
        row.insert(QStringLiteral("name"), DisplayName(model, i));
        row.insert(QStringLiteral("packageReady"), hasPackage);
        row.insert(QStringLiteral("packageText"), hasPackage ? QStringLiteral("已就绪")
                                                               : QStringLiteral("未选择"));
        row.insert(QStringLiteral("userMainReady"), hasUserMain);
        row.insert(QStringLiteral("userMainText"), hasUserMain ? QStringLiteral("已配置")
                                                                 : QStringLiteral("未配置"));
        row.insert(QStringLiteral("compiled"), compiled);
        row.insert(QStringLiteral("compileText"), compiled ? QStringLiteral("已完成")
                                                             : QStringLiteral("待编译"));
        row.insert(QStringLiteral("instanceCount"), qBound(1, model.instanceCount, 64));
        row.insert(QStringLiteral("index"), i);
        m_models.append(row);
    }

    m_readiness = totalUnits > 0
        ? qRound(100.0 * completedUnits / totalUnits)
        : 0;

    if (m_snapshot.models.empty()) {
        m_nextActionTitle = QStringLiteral("添加第一个型号");
        m_nextActionDetail = QStringLiteral("添加型号并选择模型包后，即可开始配置和编译。");
    } else if (firstPending >= 0) {
        const QString name = DisplayName(m_snapshot.models[static_cast<size_t>(firstPending)],
                                         firstPending);
        m_nextActionTitle = QStringLiteral("编译 %1 后即可开始一键预检").arg(name);
        m_nextActionDetail = QStringLiteral("当前共有 %1 个型号，仍有 %2 个型号需要完成配置或编译。")
            .arg(m_snapshot.models.size()).arg(m_snapshot.models.size() - m_compiledCount);
    } else {
        m_nextActionTitle = QStringLiteral("所有型号均已就绪");
        m_nextActionDetail = QStringLiteral("现在可以执行一键预检并生成完整检查报告。");
    }

    if (m_selectedModelIndex >= static_cast<int>(m_snapshot.models.size()))
        m_selectedModelIndex = m_snapshot.models.empty() ? -1 : 0;
    if (m_selectedModelIndex < 0 && !m_snapshot.models.empty())
        m_selectedModelIndex = 0;
    emit dashboardChanged();
    emit modelDetailsChanged();
    emit multiObjectChanged();
}

SessionModelSnapshot* QmlAppController::selectedSnapshotModel() {
    if (m_selectedModelIndex < 0
        || m_selectedModelIndex >= static_cast<int>(m_snapshot.models.size()))
        return nullptr;
    return &m_snapshot.models[static_cast<size_t>(m_selectedModelIndex)];
}

const SessionModelSnapshot* QmlAppController::selectedSnapshotModel() const {
    if (m_selectedModelIndex < 0
        || m_selectedModelIndex >= static_cast<int>(m_snapshot.models.size()))
        return nullptr;
    return &m_snapshot.models[static_cast<size_t>(m_selectedModelIndex)];
}

QString QmlAppController::selectedModelName() const {
    const auto* model = selectedSnapshotModel();
    return model ? DisplayName(*model, m_selectedModelIndex) : QString();
}

QString QmlAppController::selectedPackageDir() const {
    const auto* model = selectedSnapshotModel();
    return model ? model->packageDir : QString();
}

QString QmlAppController::selectedUserMain() const {
    const auto* model = selectedSnapshotModel();
    return model ? model->userMainBody : QString();
}

QVariantList QmlAppController::selectedRandomVars() const {
    QVariantList result;
    const auto* model = selectedSnapshotModel();
    if (!model) return result;
    for (const auto& variable : model->randomVars) {
        QVariantMap row;
        row.insert(QStringLiteral("enabled"), variable.enabled);
        row.insert(QStringLiteral("name"), QString::fromStdString(variable.name));
        row.insert(QStringLiteral("type"), variable.type == RandomVarType::Int
                                             ? QStringLiteral("int")
                                             : QStringLiteral("double"));
        row.insert(QStringLiteral("minimum"), variable.minValue);
        row.insert(QStringLiteral("maximum"), variable.maxValue);
        result.append(row);
    }
    return result;
}

QStringList QmlAppController::selectedHeaderSymbols() const {
    QStringList result;
    const auto* model = selectedSnapshotModel();
    if (!model) return result;

    static const QRegularExpression pattern(QStringLiteral(
        "\\b(?:struct|class|enum|union)\\s+([A-Za-z_][A-Za-z0-9_]*)"
        "|\\btypedef\\b[^;{}]*?\\b([A-Za-z_][A-Za-z0-9_]*)\\s*;"
        "|\\b([A-Za-z_][A-Za-z0-9_]*)\\s*\\("));
    static const QSet<QString> reserved = {
        QStringLiteral("if"), QStringLiteral("for"), QStringLiteral("while"),
        QStringLiteral("switch"), QStringLiteral("return"), QStringLiteral("sizeof"),
        QStringLiteral("defined"), QStringLiteral("extern"), QStringLiteral("static") };

    QSet<QString> seen;
    for (const QString& path : model->headerPaths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QString content = QString::fromUtf8(file.readAll());
        auto matches = pattern.globalMatch(content);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            for (int group = 1; group <= 3; ++group) {
                const QString name = match.captured(group);
                if (name.isEmpty() || reserved.contains(name) || seen.contains(name)) continue;
                seen.insert(name);
                result.append(name);
            }
        }
    }
    result.sort(Qt::CaseInsensitive);
    return result;
}

QVariantList QmlAppController::selectedPackageContents() const {
    QVariantList result;
    const auto* model = selectedSnapshotModel();
    if (!model || model->packageDir.trimmed().isEmpty()) return result;
    const ModelPackageFiles scanned = PackageScanner::ScanPackageDirectory(
        model->packageDir.toUtf8().toStdString());
    auto appendFiles = [&result, &model](const std::vector<std::string>& files,
                                         const QString& kind,
                                         const QString& root,
                                         const std::vector<std::string>* debugFiles) {
        for (const std::string& file : files) {
            const QString path = QDir::toNativeSeparators(QString::fromUtf8(file.c_str()));
            QVariantMap row;
            row.insert(QStringLiteral("kind"), kind);
            row.insert(QStringLiteral("path"), path);
            row.insert(QStringLiteral("name"), QDir(root).relativeFilePath(path));
            row.insert(QStringLiteral("selected"),
                       kind == QStringLiteral("头文件") && model->headerPaths.contains(path));
            const bool debug = debugFiles
                && std::find(debugFiles->begin(), debugFiles->end(), file) != debugFiles->end();
            row.insert(QStringLiteral("configuration"),
                       kind == QStringLiteral("头文件") ? QString() :
                       (debug ? QStringLiteral("Debug") : QStringLiteral("Release")));
            result.append(row);
        }
    };
    appendFiles(scanned.allHeaderFiles, QStringLiteral("头文件"),
                QString::fromUtf8(scanned.includeDir.c_str()), nullptr);
    appendFiles(scanned.allLibFiles, QStringLiteral("LIB"),
                QString::fromUtf8(scanned.libDir.c_str()), &scanned.debugLibFiles);
    appendFiles(scanned.allDllFiles, QStringLiteral("DLL"),
                QString::fromUtf8(scanned.modelsDir.c_str()), &scanned.debugDllFiles);
    return result;
}

bool QmlAppController::selectedPackageLayoutValid() const {
    const auto* model = selectedSnapshotModel();
    if (!model || model->packageDir.trimmed().isEmpty()) return false;
    return PackageScanner::ScanPackageDirectory(
        model->packageDir.toUtf8().toStdString()).layoutValid();
}

QString QmlAppController::selectedPackageSummary() const {
    const auto* model = selectedSnapshotModel();
    if (!model || model->packageDir.trimmed().isEmpty())
        return QStringLiteral("尚未选择模型包");
    const ModelPackageFiles scanned = PackageScanner::ScanPackageDirectory(
        model->packageDir.toUtf8().toStdString());
    return QStringLiteral("头文件 %1（已选 %2） · LIB %3 · DLL %4 · Release %5/%6 · Debug %7/%8")
        .arg(scanned.allHeaderFiles.size()).arg(model->headerPaths.size())
        .arg(scanned.allLibFiles.size()).arg(scanned.allDllFiles.size())
        .arg(scanned.releaseDllFiles.size()).arg(scanned.releaseLibFiles.size())
        .arg(scanned.debugDllFiles.size()).arg(scanned.debugLibFiles.size());
}

int QmlAppController::selectedMultiObjectCount() const {
    const auto* model = selectedSnapshotModel();
    return model ? qBound(2, model->multiObjectCount, 64) : 3;
}

int QmlAppController::selectedMultiObjectSteps() const {
    const auto* model = selectedSnapshotModel();
    return model ? qBound(1, model->multiObjectSteps, 100000) : 100;
}

double QmlAppController::selectedMultiObjectDt() const {
    const auto* model = selectedSnapshotModel();
    return model && model->multiObjectDt > 0.0 ? model->multiObjectDt : 0.02;
}

double QmlAppController::selectedMultiObjectTolerance() const {
    const auto* model = selectedSnapshotModel();
    if (!model) return kDefaultMultiObjectTolerance;
    // 未设置（0）时给默认值，界面上不会出现空的/无意义的 0 容差。
    return model->multiObjectTolerance > 0.0
        ? qBound(0.0, model->multiObjectTolerance, 1.0)
        : kDefaultMultiObjectTolerance;
}

int QmlAppController::selectedMultiObjectSchedule() const {
    const auto* model = selectedSnapshotModel();
    return model ? qBound(0, model->multiObjectSchedule, 2) : 0;
}

bool QmlAppController::selectedMultiObjectCompiled() const {
    const auto* model = selectedSnapshotModel();
    return model && !model->lastUserHarnessDll.isEmpty()
        && QFileInfo::exists(model->lastUserHarnessDll);
}

QVariantList QmlAppController::fleetMultiObjectModels() const {
    QVariantList result;
    for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
        const SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        const bool ready = !model.lastUserHarnessDll.isEmpty()
            && QFileInfo::exists(model.lastUserHarnessDll);
        QVariantMap row;
        row.insert(QStringLiteral("index"), i);
        row.insert(QStringLiteral("name"), DisplayName(model, i));
        row.insert(QStringLiteral("ready"), ready);
        row.insert(QStringLiteral("selected"), ready && m_fleetMultiObjectSelection.contains(i));
        row.insert(QStringLiteral("objectCount"),
                   m_fleetMultiObjectCounts.value(i, qBound(1, model.multiObjectCount, 32)));
        result.append(row);
    }
    return result;
}

bool QmlAppController::saveSnapshot() {
    m_snapshot.savedAt = QDateTime::currentDateTime();
    m_snapshot.currentModelIndex = m_selectedModelIndex;
    QString error;
    const bool saved = SessionStore::Save(m_snapshot, &error);
    m_hasSnapshot = saved;
    if (!saved)
        m_configurationMessage = QStringLiteral("保存失败：%1").arg(error);
    return saved;
}

void QmlAppController::saveRuntimeSettings(int perfSteps, double perfHz,
                                           int perfMemCapMB, int threadCount) {
    m_snapshot.perfSteps = qBound(1, perfSteps, 1000000);
    m_snapshot.perfHz = perfHz > 0.0 ? qMin(perfHz, 10000.0) : 50.0;
    m_snapshot.perfMemCapMB = qBound(0, perfMemCapMB, 65536);
    m_snapshot.threadCount = qBound(1, threadCount, 64);
    m_configurationMessage = saveSnapshot()
        ? QStringLiteral("运行参数已保存，将在下一次预检中生效。")
        : m_configurationMessage;
    emit settingsChanged();
    emit modelDetailsChanged();
}

void QmlAppController::resetRuntimeSettings() {
    m_snapshot.perfSteps = 10000;
    m_snapshot.perfHz = 50.0;
    m_snapshot.perfMemCapMB = 256;
    m_snapshot.threadCount = 4;
    m_configurationMessage = saveSnapshot()
        ? QStringLiteral("运行参数已恢复为推荐值。")
        : m_configurationMessage;
    emit settingsChanged();
    emit modelDetailsChanged();
}

void QmlAppController::attachCppHighlighter(QObject* quickTextDocument) {
    QQuickTextDocument* quickDocument =
        qobject_cast<QQuickTextDocument*>(quickTextDocument);
    if (!quickDocument || !quickDocument->textDocument()) return;
    QTextDocument* document = quickDocument->textDocument();
    if (document->property("modelPrecheckCppHighlighter").toBool()) return;
    new CppSyntaxHighlighter(document);
    document->setProperty("modelPrecheckCppHighlighter", true);
}

bool QmlAppController::prepareSpecialtyHarness() {
    if (m_specialtyBusy)
        return false;
    const SessionModelSnapshot* model = selectedSnapshotModel();
    if (!model) {
        m_specialtyMessage = QStringLiteral("请先选择一个型号。");
        emit specialtyChanged();
        return false;
    }
    if (model->lastUserHarnessDll.trimmed().isEmpty()
        || !QFileInfo::exists(model->lastUserHarnessDll)) {
        m_specialtyMessage = QStringLiteral("当前型号尚未编译，请先完成 UserMain 编译。");
        emit specialtyChanged();
        return false;
    }

    std::unique_ptr<UserCodeHarness> harness(new UserCodeHarness());
    std::string error;
    if (!harness->LoadCompiledDll(model->lastUserHarnessDll.toUtf8().toStdString(), error)) {
        m_specialtyMessage = QStringLiteral("无法载入已编译代码：%1")
            .arg(QString::fromUtf8(error.c_str()));
        emit specialtyChanged();
        return false;
    }
    harness->SetEnabledRandomVars(model->randomVars);
    harness->SetStepParams(model->multiObjectSteps, model->multiObjectDt);
    m_specialtyHarness = std::move(harness);
    return true;
}

void QmlAppController::finishSpecialtyThread(QThread* thread) {
    if (!thread) return;
    thread->quit();
}

void QmlAppController::runSelectedPerformanceTest() {
    if (!prepareSpecialtyHarness()) return;

    m_specialtyPerformance.clear();
    m_specialtyBusy = true;
    m_specialtyMessage = QStringLiteral("正在对“%1”执行 %2 次性能压测…")
        .arg(selectedModelName()).arg(perfSteps());
    emit specialtyChanged();

    QThread* thread = new QThread(this);
    m_specialtyThread = thread;
    PerfProfilerWorker* worker = new PerfProfilerWorker(
        m_specialtyHarness.get(), perfSteps(), perfHz(),
        static_cast<uint32_t>(m_selectedModelIndex + 1), perfMemCapMB());
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &PerfProfilerWorker::process);
    connect(worker, &PerfProfilerWorker::logMessage, this, [this](const QString& text) {
        appendLog(text);
        emit logsChanged();
    });
    connect(worker, &PerfProfilerWorker::progressUpdated, this,
            [this](int current, int total, double, double memoryMB) {
        if (current == total || current % qMax(1, total / 20) == 0) {
            m_specialtyMessage = QStringLiteral("性能压测 %1/%2 · 工作集 %3 MB")
                .arg(current).arg(total).arg(memoryMB, 0, 'f', 1);
            emit specialtyChanged();
        }
    });
    connect(worker, &PerfProfilerWorker::finished, this,
            [this, thread](const PerfProfileReport& report) {
        QVariantList samples;
        for (const PerfSample& sample : report.samples) {
            QVariantMap point;
            point.insert(QStringLiteral("step"), sample.stepIndex);
            point.insert(QStringLiteral("timeMs"), sample.timeMs);
            point.insert(QStringLiteral("memoryMB"), sample.memoryMB);
            samples.append(point);
        }
        QVariantMap result;
        result.insert(QStringLiteral("modelName"), selectedModelName());
        result.insert(QStringLiteral("perfVerdict"), QString::fromStdString(report.realtimeVerdict));
        result.insert(QStringLiteral("perfAverageMs"), report.avgTimeMs);
        result.insert(QStringLiteral("perfMaximumMs"), report.maxTimeMs);
        result.insert(QStringLiteral("perfJitterMs"), report.jitterMs);
        result.insert(QStringLiteral("memoryDeltaMB"), report.memoryDeltaMB);
        result.insert(QStringLiteral("memoryLeakRate"), report.memoryLeakRateMBPer10k);
        result.insert(QStringLiteral("initialMemoryMB"), report.initialMemoryMB);
        result.insert(QStringLiteral("finalMemoryMB"), report.finalMemoryMB);
        result.insert(QStringLiteral("completedSteps"), report.completedSteps);
        result.insert(QStringLiteral("totalSteps"), report.totalSteps);
        result.insert(QStringLiteral("perfSamples"), samples);
        result.insert(QStringLiteral("trajectory"), m_specialtyTrajectory);
        result.insert(QStringLiteral("trajectoryMessages"), QVariantList());
        result.insert(QStringLiteral("configuration"), QStringLiteral("专项性能压测"));
        result.insert(QStringLiteral("name"), selectedModelName());
        m_specialtyPerformance = result;
        m_specialtyMessage = QStringLiteral("性能压测完成：%1，平均 %2 ms，最大 %3 ms。")
            .arg(QString::fromStdString(report.realtimeVerdict))
            .arg(report.avgTimeMs, 0, 'f', 4)
            .arg(report.maxTimeMs, 0, 'f', 4);
        emit specialtyChanged();
        finishSpecialtyThread(thread);
    });
    connect(worker, &PerfProfilerWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_specialtyThread == thread) {
            m_specialtyThread = nullptr;
            m_specialtyHarness.reset();
            m_specialtyFleetHarnesses.clear();
            m_specialtyBusy = false;
            emit specialtyChanged();
        }
        thread->deleteLater();
    });
    thread->start();
}

void QmlAppController::runSelectedTrajectoryTest() {
    if (!prepareSpecialtyHarness()) return;
    m_specialtyBusy = true;
    m_specialtyTrajectory.clear();
    m_specialtyMessage = QStringLiteral("正在试跑“%1”并采集轨迹…")
        .arg(selectedModelName());
    emit specialtyChanged();

    bool captureSupported = m_specialtyHarness->SetTrajectoryCapture(true);
    RandomValueBlob values = m_specialtyHarness->Sample(42u);
    int returnCode = 0;
    bool seh = false;
    std::string error;
    const bool ok = captureSupported
        && m_specialtyHarness->RunOnce(values, &returnCode, &seh, error);
    m_specialtyHarness->SetTrajectoryCapture(false);
    std::vector<TrajectorySample> samples;
    if (ok) m_specialtyHarness->FetchTrajectory(samples);
    for (int i = 0; i < static_cast<int>(samples.size()); ++i) {
        QVariantMap point;
        point.insert(QStringLiteral("step"), i + 1);
        point.insert(QStringLiteral("x"), samples[static_cast<size_t>(i)].lon);
        point.insert(QStringLiteral("y"), samples[static_cast<size_t>(i)].lat);
        point.insert(QStringLiteral("lon"), samples[static_cast<size_t>(i)].lon);
        point.insert(QStringLiteral("lat"), samples[static_cast<size_t>(i)].lat);
        m_specialtyTrajectory.append(point);
    }
    if (!captureSupported) {
        m_specialtyMessage = QStringLiteral("当前编译产物不支持轨迹采集，请重新编译 UserMain。");
    } else if (!ok || seh) {
        m_specialtyMessage = QStringLiteral("轨迹试跑失败：%1")
            .arg(error.empty() ? QStringLiteral("运行异常或 SEH")
                               : QString::fromUtf8(error.c_str()));
    } else if (m_specialtyTrajectory.isEmpty()) {
        m_specialtyMessage = QStringLiteral(
            "未采集到轨迹点，请确认 UserMain 调用了 RecordTrajectoryPoint(out_lat, out_lon)。");
    } else {
        m_specialtyMessage = QStringLiteral("轨迹采集完成，共 %1 个点，UserMain 返回码 %2。")
            .arg(m_specialtyTrajectory.size()).arg(returnCode);
    }
    if (!m_specialtyPerformance.isEmpty()) {
        m_specialtyPerformance.insert(QStringLiteral("trajectory"), m_specialtyTrajectory);
    }
    m_specialtyBusy = false;
    m_specialtyHarness.reset();
    emit specialtyChanged();
}

void QmlAppController::runSelectedStaticChecks() {
    if (m_specialtyBusy) return;
    const SessionModelSnapshot* selected = selectedSnapshotModel();
    if (!selected || selected->packageDir.trimmed().isEmpty()) {
        m_specialtyMessage = QStringLiteral("请先选择型号并设置模型包路径。");
        emit specialtyChanged();
        return;
    }

    const SessionModelSnapshot model = *selected;
    const QString modelName = selectedModelName();
    m_specialtyStaticReport.clear();
    m_specialtyBusy = true;
    m_specialtyMessage = QStringLiteral("正在检查“%1”的头文件、LIB、DLL 与加载能力…")
        .arg(modelName);
    emit specialtyChanged();

    QThread* thread = QThread::create([this, model, modelName]() {
        const ModelPackageFiles package = PackageScanner::ScanPackageDirectory(
            model.packageDir.toUtf8().toStdString());
        QVariantList headers;
        QVariantList libraries;
        QVariantList dlls;
        bool overallPass = package.layoutValid();
        int passedHeaders = 0;
        int passedLibraries = 0;
        int passedDlls = 0;
        InterfaceMapping loadMapping = InterfaceMapping::DefaultSingleton();
        bool mappingSelected = false;

        std::vector<std::string> headerPaths;
        for (const QString& path : model.headerPaths)
            headerPaths.push_back(path.toUtf8().toStdString());
        if (headerPaths.empty()) headerPaths = package.allHeaderFiles;
        for (const std::string& path : headerPaths) {
            const HeaderAnalysisReport report = HeaderAnalyzer::AnalyzeHeader(path);
            QVariantMap row;
            row.insert(QStringLiteral("name"), QFileInfo(QString::fromUtf8(path.c_str())).fileName());
            row.insert(QStringLiteral("path"), QString::fromUtf8(path.c_str()));
            row.insert(QStringLiteral("pass"), report.overallPass);
            row.insert(QStringLiteral("encoding"), QString::fromStdString(report.encoding));
            row.insert(QStringLiteral("externC"), report.hasExternC);
            row.insert(QStringLiteral("declspec"), report.hasDeclspec);
            row.insert(QStringLiteral("pack"), report.hasPackDirective);
            row.insert(QStringLiteral("apiStyle"), QString::fromStdString(report.apiStyleDescription));
            QVariantList functions;
            for (const HeaderFunctionDecl& decl : report.functionDecls)
                functions.append(QString::fromStdString(decl.fullDeclaration));
            row.insert(QStringLiteral("functions"), functions);
            headers.append(row);
            if (report.overallPass) ++passedHeaders;
            else overallPass = false;
            if (!mappingSelected && !report.suggestedMapping.entries.empty()) {
                loadMapping = report.suggestedMapping;
                mappingSelected = true;
            }
        }

        const HeaderConflictReport conflicts = HeaderAnalyzer::AnalyzeHeaderSet(headerPaths);
        if (!conflicts.overallPass) overallPass = false;

        for (const std::string& path : package.allLibFiles) {
            const LibAnalysisReport report = LibAnalyzer::AnalyzeLib(path);
            QVariantMap row;
            row.insert(QStringLiteral("name"), QFileInfo(QString::fromUtf8(path.c_str())).fileName());
            row.insert(QStringLiteral("path"), QString::fromUtf8(path.c_str()));
            row.insert(QStringLiteral("pass"), report.overallPass);
            row.insert(QStringLiteral("architecture"), QString::fromStdString(report.architecture));
            row.insert(QStringLiteral("type"), QString::fromStdString(report.libType));
            QVariantList symbols;
            for (const std::string& symbol : report.foundSymbols)
                symbols.append(QString::fromStdString(symbol));
            row.insert(QStringLiteral("symbols"), symbols);
            QVariantList missing;
            for (const std::string& symbol : report.missingSymbols)
                missing.append(QString::fromStdString(symbol));
            row.insert(QStringLiteral("missing"), missing);
            libraries.append(row);
            if (report.overallPass) ++passedLibraries;
            else overallPass = false;
        }

        const std::vector<std::string> searchPaths = {
            model.packageDir.toUtf8().toStdString(), package.modelsDir, package.libDir
        };
        for (const std::string& path : package.allDllFiles) {
            const bool isDebug = std::find(package.debugDllFiles.begin(),
                                           package.debugDllFiles.end(), path)
                != package.debugDllFiles.end();
            const PeAnalysisReport pe = PeAnalyzer::AnalyzeDll(path, searchPaths);
            QVariantMap row;
            row.insert(QStringLiteral("name"), QFileInfo(QString::fromUtf8(path.c_str())).fileName());
            row.insert(QStringLiteral("path"), QString::fromUtf8(path.c_str()));
            row.insert(QStringLiteral("configuration"), isDebug ? QStringLiteral("Debug") : QStringLiteral("Release"));
            row.insert(QStringLiteral("architecture"), QString::fromStdString(pe.architecture));
            row.insert(QStringLiteral("crt"), QString::fromStdString(pe.crtLinkage));
            row.insert(QStringLiteral("pePass"), pe.overallPass);
            row.insert(QStringLiteral("missingDependencies"), pe.missingDependencyCount);
            row.insert(QStringLiteral("missingExports"), pe.missingExportCount);
            QVariantList imports;
            for (const ImportedDllInfo& item : pe.importedDlls) {
                QVariantMap value;
                value.insert(QStringLiteral("name"), QString::fromStdString(item.name));
                value.insert(QStringLiteral("found"), item.found);
                value.insert(QStringLiteral("resolvedPath"), QString::fromStdString(item.resolvedPath));
                imports.append(value);
            }
            row.insert(QStringLiteral("imports"), imports);
            QVariantList exports;
            for (const ExportedSymbolInfo& item : pe.exportedSymbols) {
                QVariantMap value;
                value.insert(QStringLiteral("name"), QString::fromStdString(item.name));
                value.insert(QStringLiteral("ordinal"), static_cast<int>(item.ordinal));
                value.insert(QStringLiteral("required"), item.isRequiredInterface);
                exports.append(value);
            }
            row.insert(QStringLiteral("exports"), exports);

            bool loadPass = true;
            QString loadState;
            QString loadError;
            int boundSymbols = 0;
            int missingSymbols = 0;
            if (isDebug) {
                loadState = QStringLiteral("跳过：Release 工具不加载 Debug DLL");
            } else if (!pe.isArchMatch) {
                loadPass = false;
                loadState = QStringLiteral("架构不匹配，未加载");
            } else {
                DllLoader loader;
                const LoadResult load = loader.Load(path, loadMapping);
                loadPass = load.isLoaded;
                loadState = load.isLoaded ? QStringLiteral("加载成功") : QStringLiteral("加载失败");
                loadError = QString::fromStdString(load.errorLog);
                boundSymbols = load.boundSymbolCount;
                missingSymbols = load.missingSymbolCount;
                loader.Unload();
            }
            row.insert(QStringLiteral("loadPass"), loadPass);
            row.insert(QStringLiteral("loadState"), loadState);
            row.insert(QStringLiteral("loadError"), loadError);
            row.insert(QStringLiteral("boundSymbols"), boundSymbols);
            row.insert(QStringLiteral("missingSymbols"), missingSymbols);
            const bool dllPass = pe.overallPass && loadPass;
            row.insert(QStringLiteral("pass"), dllPass);
            dlls.append(row);
            if (dllPass) ++passedDlls;
            else overallPass = false;
        }

        QVariantMap conflictMap;
        conflictMap.insert(QStringLiteral("pass"), conflicts.overallPass);
        conflictMap.insert(QStringLiteral("duplicateTypes"), conflicts.duplicateTypeCount);
        conflictMap.insert(QStringLiteral("odrConflicts"), conflicts.odrConflictCount);
        conflictMap.insert(QStringLiteral("namespaceRisks"), conflicts.namespacePollutionCount);

        QVariantMap result;
        result.insert(QStringLiteral("modelName"), modelName);
        result.insert(QStringLiteral("packageDir"), model.packageDir);
        result.insert(QStringLiteral("layoutValid"), package.layoutValid());
        result.insert(QStringLiteral("overallPass"), overallPass);
        result.insert(QStringLiteral("headers"), headers);
        result.insert(QStringLiteral("libraries"), libraries);
        result.insert(QStringLiteral("dlls"), dlls);
        result.insert(QStringLiteral("conflicts"), conflictMap);
        result.insert(QStringLiteral("headerPassed"), passedHeaders);
        result.insert(QStringLiteral("libraryPassed"), passedLibraries);
        result.insert(QStringLiteral("dllPassed"), passedDlls);

        QMetaObject::invokeMethod(this, [this, result]() {
            m_specialtyStaticReport = result;
            m_specialtyMessage = QStringLiteral("模型包单项检查完成：头文件 %1/%2，LIB %3/%4，DLL %5/%6。")
                .arg(result.value(QStringLiteral("headerPassed")).toInt())
                .arg(result.value(QStringLiteral("headers")).toList().size())
                .arg(result.value(QStringLiteral("libraryPassed")).toInt())
                .arg(result.value(QStringLiteral("libraries")).toList().size())
                .arg(result.value(QStringLiteral("dllPassed")).toInt())
                .arg(result.value(QStringLiteral("dlls")).toList().size());
            emit specialtyChanged();
        }, Qt::QueuedConnection);
    });
    m_specialtyThread = thread;
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_specialtyThread == thread) {
            m_specialtyThread = nullptr;
            m_specialtyBusy = false;
            emit specialtyChanged();
        }
        thread->deleteLater();
    });
    thread->start();
}

void QmlAppController::runSelectedMultiThreadTest() {
    if (!prepareSpecialtyHarness()) return;
    m_specialtyConcurrency.clear();
    m_specialtyBusy = true;
    m_specialtyMessage = QStringLiteral("正在对“%1”执行 %2 线程稳定性测试…")
        .arg(selectedModelName()).arg(threadCount());
    emit specialtyChanged();

    ConcurrencyTestConfig config;
    config.mode = ConcurrencyTestMode::MultiThread;
    config.count = threadCount();
    config.randomSeed = static_cast<uint32_t>(m_selectedModelIndex + 1);
    QThread* thread = new QThread(this);
    m_specialtyThread = thread;
    ConcurrencyTestWorker* worker = new ConcurrencyTestWorker(m_specialtyHarness.get(), config);
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &ConcurrencyTestWorker::process);
    connect(worker, &ConcurrencyTestWorker::logMessage, this, [this](const QString& text) {
        appendLog(text);
        emit logsChanged();
    });
    connect(worker, &ConcurrencyTestWorker::finished, this,
            [this, thread](const ConcurrencyTestReport& report) {
        QVariantList workers;
        for (const ConcurrencyThreadResult& item : report.threadResults) {
            QVariantMap row;
            row.insert(QStringLiteral("threadId"), item.threadId);
            row.insert(QStringLiteral("modelName"), QString::fromStdString(item.modelName));
            row.insert(QStringLiteral("instanceId"), item.instanceId);
            row.insert(QStringLiteral("returnCode"), item.userReturnCode);
            row.insert(QStringLiteral("exception"), item.exceptionOccurred);
            row.insert(QStringLiteral("userFail"), item.userReportedFail);
            row.insert(QStringLiteral("randomSummary"), QString::fromStdString(item.randomSummary));
            row.insert(QStringLiteral("error"), QString::fromStdString(item.errorLog));
            workers.append(row);
        }
        QVariantMap result;
        result.insert(QStringLiteral("title"), QStringLiteral("多线程稳定性 · %1").arg(selectedModelName()));
        result.insert(QStringLiteral("verdict"), QString::fromStdString(report.verdict));
        result.insert(QStringLiteral("summary"), QString::fromStdString(report.summary));
        result.insert(QStringLiteral("workerCount"), report.workerCount);
        result.insert(QStringLiteral("successCount"), report.successCount);
        result.insert(QStringLiteral("exceptionCount"), report.exceptionCount);
        result.insert(QStringLiteral("userFailCount"), report.userFailCount);
        result.insert(QStringLiteral("workers"), workers);
        m_specialtyConcurrency = result;
        m_specialtyMessage = QStringLiteral("多线程测试完成：%1；成功 %2，异常 %3，返回失败 %4。")
            .arg(QString::fromStdString(report.verdict))
            .arg(report.successCount).arg(report.exceptionCount).arg(report.userFailCount);
        emit specialtyChanged();
        finishSpecialtyThread(thread);
    });
    connect(worker, &ConcurrencyTestWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_specialtyThread == thread) {
            m_specialtyThread = nullptr;
            m_specialtyHarness.reset();
            m_specialtyFleetHarnesses.clear();
            m_specialtyBusy = false;
            emit specialtyChanged();
        }
        thread->deleteLater();
    });
    thread->start();
}

void QmlAppController::runMultiModelParallelTest(const QVariantList& modelIndexes) {
    if (m_specialtyBusy) return;
    m_specialtyFleetHarnesses.clear();
    QSet<int> selectedIndexes;
    for (const QVariant& value : modelIndexes)
        selectedIndexes.insert(value.toInt());
    ConcurrencyTestConfig config;
    config.mode = ConcurrencyTestMode::MultiModel;
    config.randomSeed = 17u;

    for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
        if (!selectedIndexes.contains(i))
            continue;
        const SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        if (model.lastUserHarnessDll.trimmed().isEmpty()
            || !QFileInfo::exists(model.lastUserHarnessDll)) {
            continue;
        }
        std::unique_ptr<UserCodeHarness> harness(new UserCodeHarness());
        std::string error;
        if (!harness->LoadCompiledDll(model.lastUserHarnessDll.toUtf8().toStdString(), error)) {
            m_specialtyMessage = QStringLiteral("无法载入型号“%1”：%2")
                .arg(DisplayName(model, i), QString::fromUtf8(error.c_str()));
            m_specialtyFleetHarnesses.clear();
            emit specialtyChanged();
            return;
        }
        harness->SetEnabledRandomVars(model.randomVars);
        harness->SetStepParams(model.multiObjectSteps, model.multiObjectDt);
        MultiModelSpec spec;
        spec.harness = harness.get();
        spec.count = qBound(1, model.instanceCount, 64);
        spec.modelName = DisplayName(model, i).toUtf8().toStdString();
        config.models.push_back(spec);
        m_specialtyFleetHarnesses.push_back(std::move(harness));
    }
    if (config.models.size() < 2) {
        m_specialtyFleetHarnesses.clear();
        m_specialtyMessage = QStringLiteral("多型号并行至少需要两个已编译型号。");
        emit specialtyChanged();
        return;
    }

    m_specialtyConcurrency.clear();
    m_specialtyBusy = true;
    m_specialtyMessage = QStringLiteral("正在执行 %1 个型号的并行稳定性测试…")
        .arg(config.models.size());
    emit specialtyChanged();

    QThread* thread = new QThread(this);
    m_specialtyThread = thread;
    ConcurrencyTestWorker* worker = new ConcurrencyTestWorker(nullptr, config);
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &ConcurrencyTestWorker::process);
    connect(worker, &ConcurrencyTestWorker::logMessage, this, [this](const QString& text) {
        appendLog(text);
        emit logsChanged();
    });
    connect(worker, &ConcurrencyTestWorker::finished, this,
            [this, thread](const ConcurrencyTestReport& report) {
        QVariantList workers;
        for (const ConcurrencyThreadResult& item : report.threadResults) {
            QVariantMap row;
            row.insert(QStringLiteral("threadId"), item.threadId);
            row.insert(QStringLiteral("modelName"), QString::fromStdString(item.modelName));
            row.insert(QStringLiteral("instanceId"), item.instanceId);
            row.insert(QStringLiteral("returnCode"), item.userReturnCode);
            row.insert(QStringLiteral("exception"), item.exceptionOccurred);
            row.insert(QStringLiteral("userFail"), item.userReportedFail);
            row.insert(QStringLiteral("randomSummary"), QString::fromStdString(item.randomSummary));
            row.insert(QStringLiteral("error"), QString::fromStdString(item.errorLog));
            workers.append(row);
        }
        QVariantMap result;
        result.insert(QStringLiteral("title"), QStringLiteral("多型号并行测试"));
        result.insert(QStringLiteral("verdict"), QString::fromStdString(report.verdict));
        result.insert(QStringLiteral("summary"), QString::fromStdString(report.summary));
        result.insert(QStringLiteral("workerCount"), report.workerCount);
        result.insert(QStringLiteral("modelTypeCount"), report.modelTypeCount);
        result.insert(QStringLiteral("successCount"), report.successCount);
        result.insert(QStringLiteral("exceptionCount"), report.exceptionCount);
        result.insert(QStringLiteral("userFailCount"), report.userFailCount);
        result.insert(QStringLiteral("workers"), workers);
        m_specialtyConcurrency = result;
        m_specialtyMessage = QStringLiteral("多型号并行完成：%1；成功 %2，异常 %3，返回失败 %4。")
            .arg(QString::fromStdString(report.verdict))
            .arg(report.successCount).arg(report.exceptionCount).arg(report.userFailCount);
        emit specialtyChanged();
        finishSpecialtyThread(thread);
    });
    connect(worker, &ConcurrencyTestWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_specialtyThread == thread) {
            m_specialtyThread = nullptr;
            m_specialtyHarness.reset();
            m_specialtyFleetHarnesses.clear();
            m_specialtyBusy = false;
            emit specialtyChanged();
        }
        thread->deleteLater();
    });
    thread->start();
}

void QmlAppController::selectModel(int modelIndex) {
    if (m_specialtyBusy)
        return;
    if (modelIndex < 0 || modelIndex >= static_cast<int>(m_snapshot.models.size()))
        return;
    const bool changed = m_selectedModelIndex != modelIndex;
    m_selectedModelIndex = modelIndex;
    m_configurationMessage.clear();
    m_multiObjectMessage.clear();
    m_multiObjectResultItems.clear();
    m_multiObjectTrajectories.clear();
    m_multiObjectVerdict.clear();
    m_multiObjectSummary.clear();
    if (changed && !m_specialtyBusy) {
        m_specialtyMessage.clear();
        m_specialtyPerformance.clear();
        m_specialtyTrajectory.clear();
        m_specialtyConcurrency.clear();
        m_specialtyStaticReport.clear();
        emit specialtyChanged();
    }
    emit modelDetailsChanged();
    emit multiObjectChanged();
}

void QmlAppController::updateModelInstanceCount(int modelIndex, int count) {
    if (m_specialtyBusy || modelIndex < 0
        || modelIndex >= static_cast<int>(m_snapshot.models.size())) {
        return;
    }
    m_snapshot.models[static_cast<size_t>(modelIndex)].instanceCount = qBound(1, count, 64);
    saveSnapshot();
    rebuildDashboardFromSnapshot();
}

void QmlAppController::addModelInQml() {
    m_fleetMultiObjectSelection.clear();
    m_fleetMultiObjectCounts.clear();
    SessionModelSnapshot model;
    model.name = QStringLiteral("model%1").arg(m_snapshot.models.size() + 1);
    model.userMainBody = QString::fromUtf8(UserCodeHarness::DefaultUserMainTemplate().c_str());
    model.randomVars = DefaultRandomVars();
    model.status = QStringLiteral("未编译");
    m_snapshot.models.push_back(model);
    m_selectedModelIndex = static_cast<int>(m_snapshot.models.size()) - 1;
    m_configurationMessage = QStringLiteral("已添加型号，请继续选择模型包。");
    saveSnapshot();
    rebuildDashboardFromSnapshot();
    emit multiObjectChanged();
}

void QmlAppController::removeSelectedModel() {
    if (m_selectedModelIndex < 0
        || m_selectedModelIndex >= static_cast<int>(m_snapshot.models.size()))
        return;
    const QString removedName = selectedModelName();
    m_fleetMultiObjectSelection.clear();
    m_fleetMultiObjectCounts.clear();
    m_snapshot.models.erase(m_snapshot.models.begin() + m_selectedModelIndex);
    if (m_snapshot.models.empty())
        m_selectedModelIndex = -1;
    else if (m_selectedModelIndex >= static_cast<int>(m_snapshot.models.size()))
        m_selectedModelIndex = static_cast<int>(m_snapshot.models.size()) - 1;
    m_configurationMessage = QStringLiteral("已移除型号“%1”。").arg(removedName);
    saveSnapshot();
    rebuildDashboardFromSnapshot();
    emit multiObjectChanged();
}

void QmlAppController::invalidateSelectedCompilation(const QString& message) {
    auto* model = selectedSnapshotModel();
    if (!model) return;
    model->status = QStringLiteral("未编译");
    model->lastUserHarnessDll.clear();
    m_fleetMultiObjectSelection.remove(m_selectedModelIndex);
    m_configurationMessage = message;
    saveSnapshot();
    rebuildDashboardFromSnapshot();
    emit multiObjectChanged();
}

void QmlAppController::addRandomVariable() {
    auto* model = selectedSnapshotModel();
    if (!model) return;
    RandomVarDef variable;
    int suffix = static_cast<int>(model->randomVars.size()) + 1;
    std::string candidate;
    for (;;) {
        candidate = QStringLiteral("var%1").arg(suffix++).toStdString();
        bool exists = false;
        for (const auto& current : model->randomVars) {
            if (current.name == candidate) {
                exists = true;
                break;
            }
        }
        if (!exists) break;
    }
    variable.name = candidate;
    variable.type = RandomVarType::Double;
    variable.minValue = 0.0;
    variable.maxValue = 1.0;
    variable.enabled = true;
    model->randomVars.push_back(variable);
    invalidateSelectedCompilation(QStringLiteral("已添加随机变量，需要重新编译。"));
}

void QmlAppController::updateRandomVariable(int row, bool enabled, const QString& name,
                                             const QString& type, double minimum,
                                             double maximum) {
    auto* model = selectedSnapshotModel();
    if (!model || row < 0 || row >= static_cast<int>(model->randomVars.size()))
        return;

    const QString normalizedName = name.trimmed();
    static const QRegularExpression identifier(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!identifier.match(normalizedName).hasMatch()) {
        m_configurationMessage = QStringLiteral(
            "变量名只能包含英文字母、数字和下划线，且不能以数字开头。");
        emit modelDetailsChanged();
        return;
    }
    for (int i = 0; i < static_cast<int>(model->randomVars.size()); ++i) {
        if (i != row
            && QString::fromStdString(model->randomVars[static_cast<size_t>(i)].name)
                   == normalizedName) {
            m_configurationMessage = QStringLiteral("变量名“%1”已经存在。").arg(normalizedName);
            emit modelDetailsChanged();
            return;
        }
    }
    if (minimum > maximum) {
        m_configurationMessage = QStringLiteral("随机变量的最小值不能大于最大值。");
        emit modelDetailsChanged();
        return;
    }

    RandomVarDef& variable = model->randomVars[static_cast<size_t>(row)];
    variable.enabled = enabled;
    variable.name = normalizedName.toStdString();
    variable.type = type.compare(QStringLiteral("int"), Qt::CaseInsensitive) == 0
        ? RandomVarType::Int : RandomVarType::Double;
    variable.minValue = minimum;
    variable.maxValue = maximum;
    invalidateSelectedCompilation(QStringLiteral("随机变量已保存，需要重新编译。"));
}

void QmlAppController::removeRandomVariable(int row) {
    auto* model = selectedSnapshotModel();
    if (!model || row < 0 || row >= static_cast<int>(model->randomVars.size()))
        return;
    model->randomVars.erase(model->randomVars.begin() + row);
    invalidateSelectedCompilation(QStringLiteral("已删除随机变量，需要重新编译。"));
}

void QmlAppController::browseSelectedPackage() {
    auto* model = selectedSnapshotModel();
    if (!model) return;
    const QString selected = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("选择型号模型包根目录（须含 include / lib / models）"),
        model->packageDir);
    if (selected.isEmpty()) return;

    const QString packageDir = QDir::toNativeSeparators(selected);
    const ModelPackageFiles scanned =
        PackageScanner::ScanPackageDirectory(packageDir.toUtf8().toStdString());
    model->packageDir = packageDir;
    model->headerPaths.clear();
    for (const auto& header : scanned.allHeaderFiles) {
        model->headerPaths.append(QDir::toNativeSeparators(
            QString::fromUtf8(header.c_str())));
    }
    if (model->name.startsWith(QStringLiteral("model")))
        model->name = QFileInfo(packageDir).fileName();
    model->status = QStringLiteral("未编译");
    model->lastUserHarnessDll.clear();
    m_fleetMultiObjectSelection.remove(m_selectedModelIndex);
    m_configurationMessage = scanned.layoutValid()
        ? QStringLiteral("模型包结构有效，已选择 %1 个头文件。").arg(model->headerPaths.size())
        : QStringLiteral("目录已保存，但缺少 include、lib 或 models 子目录。");
    saveSnapshot();
    rebuildDashboardFromSnapshot();
    emit multiObjectChanged();
}

void QmlAppController::setHeaderSelected(const QString& path, bool selected) {
    auto* model = selectedSnapshotModel();
    if (!model) return;
    const QString normalized = QDir::toNativeSeparators(path);
    if (selected) {
        if (!model->headerPaths.contains(normalized))
            model->headerPaths.append(normalized);
    } else {
        model->headerPaths.removeAll(normalized);
    }
    model->status = QStringLiteral("未编译");
    model->lastUserHarnessDll.clear();
    m_fleetMultiObjectSelection.remove(m_selectedModelIndex);
    m_configurationMessage = QStringLiteral("头文件选择已更新，需要重新编译相关代码。");
    saveSnapshot();
    rebuildDashboardFromSnapshot();
}

void QmlAppController::selectAllPackageHeaders(bool selected) {
    auto* model = selectedSnapshotModel();
    if (!model || model->packageDir.trimmed().isEmpty()) return;
    model->headerPaths.clear();
    if (selected) {
        const ModelPackageFiles scanned = PackageScanner::ScanPackageDirectory(
            model->packageDir.toUtf8().toStdString());
        for (const std::string& header : scanned.allHeaderFiles) {
            model->headerPaths.append(QDir::toNativeSeparators(
                QString::fromUtf8(header.c_str())));
        }
    }
    model->status = QStringLiteral("未编译");
    model->lastUserHarnessDll.clear();
    m_fleetMultiObjectSelection.remove(m_selectedModelIndex);
    m_configurationMessage = selected
        ? QStringLiteral("已选择全部头文件，需要重新编译相关代码。")
        : QStringLiteral("已取消全部头文件，请至少选择一个用于编译。");
    saveSnapshot();
    rebuildDashboardFromSnapshot();
}

void QmlAppController::saveSelectedUserMain(const QString& code) {
    auto* model = selectedSnapshotModel();
    if (!model) return;
    if (model->userMainBody == code) {
        m_configurationMessage = QStringLiteral("代码没有变化。");
        emit modelDetailsChanged();
        return;
    }
    model->userMainBody = code;
    model->status = QStringLiteral("未编译");
    model->lastUserHarnessDll.clear();
    m_configurationMessage = QStringLiteral("对象代码已保存，需要重新编译。");
    saveSnapshot();
    rebuildDashboardFromSnapshot();
}

CompileResult QmlAppController::compileSnapshotModel(SessionModelSnapshot& model,
                                                      int modelIndex) {
    UserHarnessConfig config;
    config.userMainBody = model.userMainBody.toUtf8().toStdString();
    config.randomVars = model.randomVars;
    config.stepCount = qBound(1, model.multiObjectSteps, 100000);
    config.stepDt = model.multiObjectDt > 0.0 ? model.multiObjectDt : 0.02;
    for (const QString& header : model.headerPaths)
        config.headerPaths.push_back(header.toUtf8().toStdString());

    const std::string packageUtf8 = model.packageDir.toUtf8().toStdString();
    const ModelPackageFiles scanned = PackageScanner::ScanPackageDirectory(packageUtf8);
    if (scanned.includeDirExists) config.includeDirs.push_back(scanned.includeDir);
    if (scanned.modelsDirExists) config.libPaths.push_back(scanned.modelsDir);
    if (scanned.libDirExists) config.libPaths.push_back(scanned.libDir);
    for (int i = 0; i < static_cast<int>(scanned.allLibFiles.size()) && i < 20; ++i)
        config.linkLibs.push_back(scanned.allLibFiles[static_cast<size_t>(i)]);

    const QString folder = SafeFolderName(model.name, modelIndex);
    const QString outputDir = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("TestModel/%1").arg(folder));
    QDir().mkpath(outputDir);
    config.workDir = QDir::toNativeSeparators(outputDir).toUtf8().toStdString();
    config.outputBaseName = (folder + QStringLiteral("_UserHarness")).toUtf8().toStdString();

    UserCodeHarness harness;
    return harness.Compile(config);
}

void QmlAppController::saveStepSettings(int steps, double dt) {
    auto* model = selectedSnapshotModel();
    if (!model) return;
    model->multiObjectSteps = qBound(1, steps, 100000);
    model->multiObjectDt = qBound(0.000001, dt, 60.0);
    // 步数与步长在运行期传入 Harness，无需重新编译。
    m_configurationMessage = QStringLiteral("运行步数已更新为 %1 步 · dt %2，全部测试项共用该设置。")
        .arg(model->multiObjectSteps).arg(model->multiObjectDt);
    saveSnapshot();
    emit modelDetailsChanged();
    emit multiObjectChanged();
}

void QmlAppController::saveMultiObjectConfiguration(
    int objectCount, double tolerance, int schedule) {
    auto* model = selectedSnapshotModel();
    if (!model || m_multiObjectBusy) return;
    model->multiObjectCount = qBound(2, objectCount, 64);
    model->multiObjectTolerance = qBound(0.0, tolerance, 1.0);
    model->multiObjectSchedule = qBound(0, schedule, 2);
    saveSnapshot();
    emit multiObjectChanged();
}

void QmlAppController::ensureModelsCompiledForPrecheck() {
    // 先统计需要补编译的型号数，便于把进度按型号数分摊到 5%~30% 区间。
    int pending = 0;
    for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
        const SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        if (!model.lastUserHarnessDll.isEmpty()
            && QFileInfo::exists(model.lastUserHarnessDll)) {
            continue;
        }
        if (model.packageDir.trimmed().isEmpty() || model.headerPaths.isEmpty()
            || model.userMainBody.trimmed().isEmpty()) {
            continue;
        }
        ++pending;
    }
    if (pending == 0) {
        appendLog(QStringLiteral("所有型号对象代码均已编译，跳过编译步骤"));
        emit logsChanged();
    }
    int compiled = 0;
    for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
        SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        if (!model.lastUserHarnessDll.isEmpty()
            && QFileInfo::exists(model.lastUserHarnessDll)) {
            continue;
        }
        if (model.packageDir.trimmed().isEmpty() || model.headerPaths.isEmpty()
            || model.userMainBody.trimmed().isEmpty()) {
            continue;  // 未配置完整的型号由预检标记为“未运行”。
        }
        const QString name = DisplayName(model, i);
        if (pending > 0)
            setBusyProgress(0.05 + 0.25 * static_cast<double>(compiled) / pending);
        m_precheckProgress = QStringLiteral("正在编译 %1（%2/%3）…")
            .arg(name).arg(i + 1).arg(m_snapshot.models.size());
        appendLog(m_precheckProgress);
        emit logsChanged();
        emit precheckStateChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        const CompileResult result = compileSnapshotModel(model, i);
        if (result.success) {
            model.status = QStringLiteral("已加载");
            model.lastUserHarnessDll = QString::fromUtf8(result.dllPath.c_str());
            appendLog(QStringLiteral("编译成功：%1").arg(name));
        } else {
            model.status = QStringLiteral("编译失败");
            model.lastUserHarnessDll.clear();
            appendLog(QStringLiteral("编译失败：%1").arg(name));
        }
        emit logsChanged();
        ++compiled;
        if (pending > 0)
            setBusyProgress(0.05 + 0.25 * static_cast<double>(compiled) / pending);
    }
    if (pending > 0) {
        appendLog(QStringLiteral("预检前自动编译完成：本次编译 %1 个型号").arg(compiled));
        emit logsChanged();
    }
    saveSnapshot();
    rebuildDashboardFromSnapshot();
}

void QmlAppController::runSelectedMultiObject(
    int objectCount, double tolerance, int schedule) {
    if (m_multiObjectBusy) return;
    saveMultiObjectConfiguration(objectCount, tolerance, schedule);
    auto* model = selectedSnapshotModel();
    if (!model) return;
    // 对象代码只需编译一次：与预检、压测、并发共用同一个 DLL。
    if (model->lastUserHarnessDll.isEmpty()
        || !QFileInfo::exists(model->lastUserHarnessDll)) {
        m_multiObjectMessage = QStringLiteral("当前型号尚未编译，请先在“型号与代码”页保存并编译。");
        emit multiObjectChanged();
        return;
    }

    m_multiObjectBusy = true;
    m_multiObjectMessage = QStringLiteral("正在执行基线与交错测试…");
    m_multiObjectResultItems.clear();
    m_multiObjectTrajectories.clear();
    m_multiObjectVerdict.clear();
    m_multiObjectSummary.clear();
    m_multiObjectResultIsFleet = false;
    emit multiObjectChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    MultiObjectHarness harness;
    std::string loadError;
    if (!harness.LoadCompiledDll(
            model->lastUserHarnessDll.toUtf8().toStdString(), loadError)) {
        m_multiObjectBusy = false;
        m_multiObjectMessage = QStringLiteral("无法加载多对象 Harness：%1")
            .arg(QString::fromUtf8(loadError.c_str()));
        emit multiObjectChanged();
        return;
    }
    harness.SetEnabledRandomVars(model->randomVars);

    MultiObjectTestConfig config;
    config.objectCount = model->multiObjectCount;
    config.stepCount = model->multiObjectSteps;
    config.stepDt = model->multiObjectDt;
    config.tolerance = model->multiObjectTolerance;
    config.schedule = static_cast<MultiObjectSchedule>(model->multiObjectSchedule);
    config.randomSeed = 20260902u;
    const RandomValueBlob values = harness.Sample(config.randomSeed, config.objectCount);
    const MultiObjectTestReport report =
        SingleThreadMultiObjectTester::Run(harness, config, values);

    for (const MultiObjectResult& object : report.objectResults) {
        QVariantMap row;
        row.insert(QStringLiteral("objectId"), object.objectId);
        row.insert(QStringLiteral("baselinePoints"),
                   static_cast<int>(object.baselineTrajectory.size()));
        row.insert(QStringLiteral("interleavedPoints"),
                   static_cast<int>(object.interleavedTrajectory.size()));
        row.insert(QStringLiteral("deviation"), object.maxPositionDeviation);
        row.insert(QStringLiteral("baselineReturn"), object.baselineReturnCode);
        row.insert(QStringLiteral("interleavedReturn"), object.interleavedReturnCode);
        row.insert(QStringLiteral("detail"), QString::fromUtf8(object.detail.c_str()));
        const bool passed = !object.exceptionOccurred
            && object.baselineReturnCode == 0
            && object.interleavedReturnCode == 0
            && object.maxPositionDeviation <= config.tolerance;
        row.insert(QStringLiteral("state"), passed ? QStringLiteral("通过")
                                                    : QStringLiteral("异常"));
        m_multiObjectResultItems.append(row);

        auto appendSeries = [this, &object](const std::vector<TrajectorySample>& samples,
                                             bool baseline) {
            QVariantList points;
            for (const TrajectorySample& point : samples) {
                QVariantMap value;
                value.insert(QStringLiteral("x"), point.lon);
                value.insert(QStringLiteral("y"), point.lat);
                points.append(value);
            }
            QVariantMap series;
            series.insert(QStringLiteral("objectId"), object.objectId);
            series.insert(QStringLiteral("baseline"), baseline);
            series.insert(QStringLiteral("points"), points);
            m_multiObjectTrajectories.append(series);
        };
        appendSeries(object.baselineTrajectory, true);
        appendSeries(object.interleavedTrajectory, false);
    }

    m_multiObjectVerdict = QString::fromUtf8(report.verdict.c_str());
    m_multiObjectSummary = QString::fromUtf8(report.summary.c_str());
    m_multiObjectMaxDeviation = report.maxPositionDeviation;
    m_multiObjectMaxFrameMs = report.maxFrameTimeMs;
    m_multiObjectMemoryDeltaMB = report.memoryDeltaMB;
    m_multiObjectMessage = QStringLiteral("测试完成：%1 — %2")
        .arg(m_multiObjectVerdict, m_multiObjectSummary);
    appendLog(m_multiObjectMessage);
    emit logsChanged();
    m_multiObjectBusy = false;
    emit multiObjectChanged();
}

void QmlAppController::initializeFleetMultiObjectSelection() {
    // 只准备默认勾选与对象数，保留上一次的测试结果。
    if (m_fleetMultiObjectSelection.isEmpty()) {
        for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
            const SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
            if (!model.lastUserHarnessDll.isEmpty()
                && QFileInfo::exists(model.lastUserHarnessDll)) {
                m_fleetMultiObjectSelection.insert(i);
                m_fleetMultiObjectCounts.insert(i, qBound(1, model.multiObjectCount, 32));
            }
        }
    }
    emit multiObjectChanged();
}

void QmlAppController::updateFleetMultiObjectModel(
    int modelIndex, bool selected, int objectCount) {
    if (modelIndex < 0 || modelIndex >= static_cast<int>(m_snapshot.models.size()))
        return;
    const SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(modelIndex)];
    const bool ready = !model.lastUserHarnessDll.isEmpty()
        && QFileInfo::exists(model.lastUserHarnessDll);
    if (selected && ready)
        m_fleetMultiObjectSelection.insert(modelIndex);
    else
        m_fleetMultiObjectSelection.remove(modelIndex);
    m_fleetMultiObjectCounts.insert(modelIndex, qBound(1, objectCount, 32));
    emit multiObjectChanged();
}

void QmlAppController::runFleetMultiObject(double tolerance, int schedule) {
    if (m_multiObjectBusy) return;

    std::vector<std::unique_ptr<MultiObjectHarness>> harnesses;
    FleetMultiObjectTestConfig config;
    config.tolerance = qBound(0.0, tolerance, 1.0);
    config.schedule = static_cast<MultiObjectSchedule>(qBound(0, schedule, 2));
    config.randomSeed = 20260903u;

    for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
        if (!m_fleetMultiObjectSelection.contains(i)) continue;
        SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        if (model.lastUserHarnessDll.isEmpty()
            || !QFileInfo::exists(model.lastUserHarnessDll)) {
            continue;
        }
        std::unique_ptr<MultiObjectHarness> harness(new MultiObjectHarness());
        std::string error;
        if (!harness->LoadCompiledDll(
                model.lastUserHarnessDll.toUtf8().toStdString(), error)) {
            m_multiObjectMessage = QStringLiteral("型号“%1”的对象代码加载失败：%2")
                .arg(DisplayName(model, i), QString::fromUtf8(error.c_str()));
            emit multiObjectChanged();
            return;
        }
        harness->SetEnabledRandomVars(model.randomVars);
        if (!harness->SupportsObjectSession()) {
            m_multiObjectMessage = QStringLiteral(
                "型号“%1”不支持对象级调度，请重新编译对象代码。")
                .arg(DisplayName(model, i));
            emit multiObjectChanged();
            return;
        }
        FleetMultiObjectModelSpec spec;
        spec.harness = harness.get();
        spec.modelName = DisplayName(model, i).toUtf8().toStdString();
        spec.objectCount = m_fleetMultiObjectCounts.value(
            i, qBound(1, model.multiObjectCount, 32));
        config.models.push_back(spec);
        harnesses.push_back(std::move(harness));
    }

    // 各型号若未单独设置步数，则沿用其“型号与代码”页的运行参数。
    int stepCount = 100;
    double stepDt = 0.02;
    for (int i = 0; i < static_cast<int>(m_snapshot.models.size()); ++i) {
        if (!m_fleetMultiObjectSelection.contains(i)) continue;
        const SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        stepCount = qBound(1, model.multiObjectSteps, 100000);
        stepDt = model.multiObjectDt > 0.0 ? model.multiObjectDt : 0.02;
        break;
    }
    config.stepCount = stepCount;
    config.stepDt = stepDt;

    if (config.models.size() < 2) {
        m_multiObjectMessage = QStringLiteral("跨型号对象交错至少需要选择两个已编译型号。");
        emit multiObjectChanged();
        return;
    }

    m_multiObjectBusy = true;
    m_multiObjectMessage = QStringLiteral("正在执行跨型号对象交错测试…");
    m_multiObjectResultItems.clear();
    m_multiObjectTrajectories.clear();
    m_multiObjectVerdict.clear();
    m_multiObjectSummary.clear();
    m_multiObjectResultIsFleet = true;
    emit multiObjectChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const FleetMultiObjectTestReport report =
        FleetSingleThreadMultiObjectTester::Run(config);
    for (const FleetMultiObjectResult& object : report.objectResults) {
        const MultiObjectResult& detail = object.detail;
        QVariantMap row;
        row.insert(QStringLiteral("objectId"), object.globalObjectId);
        row.insert(QStringLiteral("localObjectId"), object.localObjectId);
        row.insert(QStringLiteral("modelName"), QString::fromUtf8(object.modelName.c_str()));
        row.insert(QStringLiteral("baselinePoints"),
                   static_cast<int>(detail.baselineTrajectory.size()));
        row.insert(QStringLiteral("interleavedPoints"),
                   static_cast<int>(detail.interleavedTrajectory.size()));
        row.insert(QStringLiteral("deviation"), detail.maxPositionDeviation);
        row.insert(QStringLiteral("baselineReturn"), detail.baselineReturnCode);
        row.insert(QStringLiteral("interleavedReturn"), detail.interleavedReturnCode);
        row.insert(QStringLiteral("detail"), QString::fromUtf8(detail.detail.c_str()));
        const bool passed = !detail.exceptionOccurred
            && detail.baselineReturnCode == 0
            && detail.interleavedReturnCode == 0
            && detail.maxPositionDeviation <= config.tolerance;
        row.insert(QStringLiteral("state"), passed ? QStringLiteral("通过")
                                                    : QStringLiteral("异常"));
        m_multiObjectResultItems.append(row);

        auto appendSeries = [this, &object](const std::vector<TrajectorySample>& samples,
                                             bool baseline) {
            QVariantList points;
            for (const TrajectorySample& point : samples) {
                QVariantMap value;
                value.insert(QStringLiteral("x"), point.lon);
                value.insert(QStringLiteral("y"), point.lat);
                points.append(value);
            }
            QVariantMap series;
            series.insert(QStringLiteral("objectId"), object.globalObjectId);
            series.insert(QStringLiteral("modelName"),
                          QString::fromUtf8(object.modelName.c_str()));
            series.insert(QStringLiteral("baseline"), baseline);
            series.insert(QStringLiteral("points"), points);
            m_multiObjectTrajectories.append(series);
        };
        appendSeries(detail.baselineTrajectory, true);
        appendSeries(detail.interleavedTrajectory, false);
    }

    m_multiObjectVerdict = QString::fromUtf8(report.verdict.c_str());
    m_multiObjectSummary = QString::fromUtf8(report.summary.c_str());
    m_multiObjectMaxDeviation = report.maxPositionDeviation;
    m_multiObjectMaxFrameMs = report.maxFrameTimeMs;
    m_multiObjectMemoryDeltaMB = report.memoryDeltaMB;
    m_multiObjectMessage = QStringLiteral("跨型号测试完成：%1 — %2")
        .arg(m_multiObjectVerdict, m_multiObjectSummary);
    appendLog(m_multiObjectMessage);
    emit logsChanged();
    m_multiObjectBusy = false;
    emit multiObjectChanged();
}

void QmlAppController::setBusyProgress(double value) {
    if (m_busyProgress == value) return;
    m_busyProgress = value;
    emit busyProgressChanged();
}

void QmlAppController::appendLog(const QString& text) {
    m_logLines.append(QStringLiteral("[%1] %2")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")), text));
    while (m_logLines.size() > 1000) m_logLines.removeFirst();
}

void QmlAppController::compileSelectedModel(const QString& code) {
    auto* model = selectedSnapshotModel();
    if (!model || m_compileBusy) return;
    model->userMainBody = code;
    if (model->packageDir.trimmed().isEmpty() || model->headerPaths.isEmpty()) {
        m_configurationMessage = QStringLiteral("请先选择包含头文件的有效模型包。");
        emit modelDetailsChanged();
        return;
    }
    if (model->userMainBody.trimmed().isEmpty()) {
        m_configurationMessage = QStringLiteral("UserMain 代码不能为空。");
        emit modelDetailsChanged();
        return;
    }

    m_compileBusy = true;
    setBusyProgress(-1.0);  // 单型号编译无法细分进度，使用不确定进度条
    m_configurationMessage = QStringLiteral("正在编译 %1…").arg(selectedModelName());
    appendLog(m_configurationMessage);
    emit compileStateChanged();
    emit modelDetailsChanged();
    emit logsChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const CompileResult result = compileSnapshotModel(*model, m_selectedModelIndex);
    model = selectedSnapshotModel();
    if (model) {
        if (result.success) {
            model->status = QStringLiteral("已加载");
            model->lastUserHarnessDll = QString::fromUtf8(result.dllPath.c_str());
            m_configurationMessage = QStringLiteral("编译成功，可以返回首页执行预检。");
            appendLog(QStringLiteral("编译成功：%1").arg(selectedModelName()));
        } else {
            model->status = QStringLiteral("编译失败");
            model->lastUserHarnessDll.clear();
            const QString log = QString::fromUtf8(result.log.c_str()).trimmed();
            m_configurationMessage = log.isEmpty()
                ? QStringLiteral("编译失败，请检查模型包和 UserMain 代码。")
                : QStringLiteral("编译失败：%1").arg(log.right(320));
            appendLog(QStringLiteral("编译失败：%1").arg(selectedModelName()));
        }
    }
    emit logsChanged();
    saveSnapshot();
    m_compileBusy = false;
    setBusyProgress(-1.0);
    // 先刷新看板（compiledCount/pendingCount），再通知编译状态，
    // 这样 QML 在 compileStateChanged 里读到的是最终的待编译数量。
    rebuildDashboardFromSnapshot();
    emit compileStateChanged();
}

void QmlAppController::compileAllModelsInQml(const QString& currentCode) {
    if (m_compileBusy || m_snapshot.models.empty()) return;
    if (auto* current = selectedSnapshotModel())
        current->userMainBody = currentCode;

    m_compileBusy = true;
    setBusyProgress(0.0);
    emit compileStateChanged();
    int succeeded = 0;
    QString lastFailure;
    const int total = static_cast<int>(m_snapshot.models.size());
    for (int i = 0; i < total; ++i) {
        SessionModelSnapshot& model = m_snapshot.models[static_cast<size_t>(i)];
        const QString name = DisplayName(model, i);
        m_configurationMessage = QStringLiteral("正在编译 %1（%2/%3）…")
            .arg(name).arg(i + 1).arg(m_snapshot.models.size());
        // 按型号推进进度：当前型号开始时占其时间片的 20%，避免进度条长时间不动。
        setBusyProgress((static_cast<double>(i) + 0.2) / total);
        appendLog(m_configurationMessage);
        emit modelDetailsChanged();
        emit logsChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        if (model.packageDir.trimmed().isEmpty() || model.headerPaths.isEmpty()) {
            model.status = QStringLiteral("失败:无头文件");
            model.lastUserHarnessDll.clear();
            lastFailure = QStringLiteral("%1 尚未选择有效模型包").arg(name);
            appendLog(QStringLiteral("跳过编译：%1 尚未选择有效模型包").arg(name));
            setBusyProgress(static_cast<double>(i + 1) / total);
            continue;
        }
        if (model.userMainBody.trimmed().isEmpty()) {
            model.status = QStringLiteral("失败:无UserMain");
            model.lastUserHarnessDll.clear();
            lastFailure = QStringLiteral("%1 的 UserMain 为空").arg(name);
            appendLog(QStringLiteral("跳过编译：%1 的 UserMain 为空").arg(name));
            setBusyProgress(static_cast<double>(i + 1) / total);
            continue;
        }

        const CompileResult result = compileSnapshotModel(model, i);
        if (result.success) {
            model.status = QStringLiteral("已加载");
            model.lastUserHarnessDll = QString::fromUtf8(result.dllPath.c_str());
            ++succeeded;
            appendLog(QStringLiteral("编译成功：%1").arg(name));
        } else {
            model.status = QStringLiteral("编译失败");
            model.lastUserHarnessDll.clear();
            lastFailure = QStringLiteral("%1 编译失败").arg(name);
            appendLog(QStringLiteral("编译失败：%1").arg(name));
        }
        emit logsChanged();
        setBusyProgress(static_cast<double>(i + 1) / total);
    }

    if (succeeded == static_cast<int>(m_snapshot.models.size())) {
        m_configurationMessage = QStringLiteral("全部 %1 个型号编译成功，可以执行一键预检。")
            .arg(succeeded);
    } else {
        m_configurationMessage = QStringLiteral("已编译成功 %1/%2；%3。")
            .arg(succeeded).arg(m_snapshot.models.size())
            .arg(lastFailure.isEmpty() ? QStringLiteral("请检查失败型号") : lastFailure);
    }
    appendLog(m_configurationMessage);
    emit logsChanged();
    saveSnapshot();
    m_compileBusy = false;
    setBusyProgress(-1.0);
    // 先刷新看板（compiledCount/pendingCount），再通知编译状态，
    // 这样 QML 在 compileStateChanged 里读到的是最终的待编译数量。
    rebuildDashboardFromSnapshot();
    emit compileStateChanged();
}

void QmlAppController::acceptPrecheckSummary(
    const QVariantList& items, int passCount, int warnCount, int failCount,
    int pendingCount, const QString& timestamp, bool overallPass,
    const QString& cachedReportPath) {
    m_resultItems = EnrichResultItems(items);
    m_resultPassCount = passCount;
    m_resultWarnCount = warnCount;
    m_resultFailCount = failCount;
    m_resultPendingCount = pendingCount;
    m_resultTimestamp = timestamp;
    m_resultOverallPass = overallPass;
    m_cachedReportPath = QDir::toNativeSeparators(cachedReportPath);
    m_receivedResultCurrentRun = true;
    saveResultSnapshot();
    archiveCurrentReport();
    emit resultsChanged();
}

void QmlAppController::acceptPrecheckModelDetails(const QVariantList& models) {
    m_resultModels = models;
    emit resultsChanged();
}

void QmlAppController::acceptPrecheckConcurrencyDetails(const QVariantList& reports) {
    m_resultConcurrency = reports;
    emit resultsChanged();
}

void QmlAppController::acceptPrecheckHeaderConflicts(const QVariantMap& report) {
    m_resultHeaderConflicts = report;
    emit resultsChanged();
}

void QmlAppController::acceptPrecheckMultiObjectDetails(const QVariantList& reports) {
    m_resultMultiObject = reports;
    emit resultsChanged();
}

void QmlAppController::saveResultSnapshot() const {
    const QString directory = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session"));
    QDir().mkpath(directory);
    QJsonObject root;
    root.insert(QStringLiteral("timestamp"), m_resultTimestamp);
    root.insert(QStringLiteral("overallPass"), m_resultOverallPass);
    root.insert(QStringLiteral("passCount"), m_resultPassCount);
    root.insert(QStringLiteral("warnCount"), m_resultWarnCount);
    root.insert(QStringLiteral("failCount"), m_resultFailCount);
    root.insert(QStringLiteral("pendingCount"), m_resultPendingCount);
    root.insert(QStringLiteral("cachedReportPath"), m_cachedReportPath);
    root.insert(QStringLiteral("items"), QJsonArray::fromVariantList(m_resultItems));
    root.insert(QStringLiteral("models"), QJsonArray::fromVariantList(m_resultModels));
    root.insert(QStringLiteral("concurrency"),
                QJsonArray::fromVariantList(m_resultConcurrency));
    root.insert(QStringLiteral("headerConflicts"),
                QJsonObject::fromVariantMap(m_resultHeaderConflicts));
    root.insert(QStringLiteral("multiObject"),
                QJsonArray::fromVariantList(m_resultMultiObject));

    QSaveFile file(QDir(directory).filePath(QStringLiteral("last_result.json")));
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

void QmlAppController::loadResultSnapshot() {
    const QString path = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session/last_result.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return;
    const QJsonObject root = document.object();
    m_resultTimestamp = root.value(QStringLiteral("timestamp")).toString();
    m_resultOverallPass = root.value(QStringLiteral("overallPass")).toBool();
    m_resultPassCount = root.value(QStringLiteral("passCount")).toInt();
    m_resultWarnCount = root.value(QStringLiteral("warnCount")).toInt();
    m_resultFailCount = root.value(QStringLiteral("failCount")).toInt();
    m_resultPendingCount = root.value(QStringLiteral("pendingCount")).toInt();
    m_cachedReportPath = root.value(QStringLiteral("cachedReportPath")).toString();
    m_resultItems = EnrichResultItems(
        root.value(QStringLiteral("items")).toArray().toVariantList());
    m_resultModels = root.value(QStringLiteral("models")).toArray().toVariantList();
    m_resultConcurrency = root.value(QStringLiteral("concurrency"))
        .toArray().toVariantList();
    m_resultHeaderConflicts = root.value(QStringLiteral("headerConflicts"))
        .toObject().toVariantMap();
    m_resultMultiObject = root.value(QStringLiteral("multiObject"))
        .toArray().toVariantList();
    emit resultsChanged();
}

void QmlAppController::loadReportHistory() {
    const QString path = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session/report_history.json"));
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (document.isArray())
            m_reportHistory = document.array().toVariantList();
    }
    QVariantList existing;
    bool historyChanged = false;
    for (const QVariant& value : m_reportHistory) {
        QVariantMap row = value.toMap();
        const QString sourcePath = row.value(QStringLiteral("path")).toString();
        if (QFileInfo(sourcePath).fileName() == QStringLiteral("last_precheck_report.html")
            && QFileInfo::exists(sourcePath)) {
            const QString archiveDirectory = QDir(QCoreApplication::applicationDirPath())
                .filePath(QStringLiteral("session/reports"));
            QDir().mkpath(archiveDirectory);
            const QString archivePath = QDir(archiveDirectory).filePath(
                QStringLiteral("Precheck_Imported_%1.html").arg(
                    QDateTime::currentDateTime().toString(
                        QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
            if (QFile::copy(sourcePath, archivePath)) {
                row.insert(QStringLiteral("path"), QDir::toNativeSeparators(archivePath));
                historyChanged = true;
            }
        }
        if (QFileInfo::exists(row.value(QStringLiteral("path")).toString()))
            existing.append(row);
        else
            historyChanged = true;
    }
    m_reportHistory = existing;
    if (m_reportHistory.isEmpty() && !m_cachedReportPath.isEmpty()
        && QFileInfo::exists(m_cachedReportPath)) {
        QString importedPath = m_cachedReportPath;
        const QString archiveDirectory = QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("session/reports"));
        QDir().mkpath(archiveDirectory);
        const QString archivePath = QDir(archiveDirectory).filePath(
            QStringLiteral("Precheck_Imported_%1.html").arg(
                QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
        if (QFile::copy(m_cachedReportPath, archivePath))
            importedPath = QDir::toNativeSeparators(archivePath);
        QVariantMap row;
        row.insert(QStringLiteral("path"), importedPath);
        row.insert(QStringLiteral("timestamp"), m_resultTimestamp);
        row.insert(QStringLiteral("overallPass"), m_resultOverallPass);
        row.insert(QStringLiteral("passCount"), m_resultPassCount);
        row.insert(QStringLiteral("warnCount"), m_resultWarnCount);
        row.insert(QStringLiteral("failCount"), m_resultFailCount);
        row.insert(QStringLiteral("pendingCount"), m_resultPendingCount);
        m_reportHistory.append(row);
        saveReportHistory();
    } else if (historyChanged) {
        saveReportHistory();
    }
    emit resultsChanged();
}

void QmlAppController::saveReportHistory() const {
    const QString directory = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session"));
    QDir().mkpath(directory);
    QSaveFile file(QDir(directory).filePath(QStringLiteral("report_history.json")));
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(QJsonArray::fromVariantList(m_reportHistory))
                   .toJson(QJsonDocument::Indented));
    file.commit();
}

void QmlAppController::archiveCurrentReport() {
    if (m_cachedReportPath.isEmpty() || !QFileInfo::exists(m_cachedReportPath))
        return;
    const QString directory = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session/reports"));
    QDir().mkpath(directory);
    const QString archivePath = QDir(directory).filePath(
        QStringLiteral("Precheck_%1.html")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
    QFile source(m_cachedReportPath);
    QSaveFile target(archivePath);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly))
        return;
    target.write(source.readAll());
    if (!target.commit()) return;

    QVariantMap row;
    row.insert(QStringLiteral("path"), QDir::toNativeSeparators(archivePath));
    row.insert(QStringLiteral("timestamp"), m_resultTimestamp);
    row.insert(QStringLiteral("overallPass"), m_resultOverallPass);
    row.insert(QStringLiteral("passCount"), m_resultPassCount);
    row.insert(QStringLiteral("warnCount"), m_resultWarnCount);
    row.insert(QStringLiteral("failCount"), m_resultFailCount);
    row.insert(QStringLiteral("pendingCount"), m_resultPendingCount);
    m_reportHistory.prepend(row);
    saveReportHistory();
}

void QmlAppController::openCachedReport() {
    if (!m_cachedReportPath.isEmpty() && QFileInfo::exists(m_cachedReportPath))
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_cachedReportPath));
    else
        openReport();
}

void QmlAppController::openReportAt(int index) {
    if (index < 0 || index >= m_reportHistory.size()) return;
    const QString path = m_reportHistory.at(index).toMap()
        .value(QStringLiteral("path")).toString();
    if (QFileInfo::exists(path))
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void QmlAppController::exportReportAt(int index) {
    if (index < 0 || index >= m_reportHistory.size()) return;
    const QString sourcePath = m_reportHistory.at(index).toMap()
        .value(QStringLiteral("path")).toString();
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) return;
    const QString targetPath = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("导出预检报告"),
        QFileInfo(sourcePath).fileName(), QStringLiteral("HTML Files (*.html)"));
    if (targetPath.isEmpty()) return;
    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly)) return;
    target.write(source.readAll());
    target.commit();
}

void QmlAppController::openReportsFolder() {
    const QString directory = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("session/reports"));
    QDir().mkpath(directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void QmlAppController::clearLogs() {
    m_logLines.clear();
    emit logsChanged();
}

void QmlAppController::exportLogs() {
    if (m_logLines.isEmpty()) return;
    const QString target = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("导出运行日志"),
        QStringLiteral("ModelPrecheck_Log.txt"),
        QStringLiteral("Text Files (*.txt)"));
    if (target.isEmpty()) return;
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    file.write(m_logLines.join(QStringLiteral("\r\n")).toUtf8());
    file.write("\r\n");
    file.commit();
}

void QmlAppController::runPrecheck() {
    if (m_precheckRunning) return;
    if (m_snapshot.models.empty()) {
        m_precheckError = QStringLiteral("请先添加至少一个型号。");
        m_precheckProgress = QStringLiteral("无法开始预检");
        emit precheckStateChanged();
        return;
    }

    m_precheckRunning = true;
    m_precheckProgress = QStringLiteral("正在准备一键预检…");
    setBusyProgress(0.02);
    m_precheckError.clear();
    m_receivedResultCurrentRun = false;
    m_logLines.clear();
    emit logsChanged();
    emit precheckStateChanged();

    // 一键预检不再要求先手动编译：缺少 DLL 的型号在此自动编译一次，
    // 同一份对象代码同时覆盖预检、压测、并发与多对象测试。
    ensureModelsCompiledForPrecheck();
    setBusyProgress(0.30);

    const SessionSnapshot snapshot = m_snapshot;
    QThread* thread = QThread::create([this, snapshot]() {
        const QmlPrecheckResult result = QmlPrecheckRunner::Run(
            snapshot, [this](const QString& text) {
                QMetaObject::invokeMethod(this, [this, text]() {
                    m_precheckProgress = text;
                    // 预检阶段无法预知总步数，按回调次数在 30%~95% 之间单调推进。
                    if (m_busyProgress >= 0.0 && m_busyProgress < 0.95)
                        setBusyProgress(qMin(0.95, m_busyProgress + 0.06));
                    appendLog(text);
                    emit logsChanged();
                    emit precheckStateChanged();
                }, Qt::QueuedConnection);
            });

        QMetaObject::invokeMethod(this, [this, result]() {
            m_resultModels = result.models;
            m_resultConcurrency = result.concurrency;
            m_resultHeaderConflicts = result.headerConflicts;
            m_resultMultiObject = result.multiObject;
            m_resultItems = EnrichResultItems(result.items);
            m_resultPassCount = result.passCount;
            m_resultWarnCount = result.warnCount;
            m_resultFailCount = result.failCount;
            m_resultPendingCount = result.pendingCount;
            m_resultTimestamp = result.timestamp;
            m_resultOverallPass = result.overallPass;
            m_cachedReportPath = QDir::toNativeSeparators(result.reportPath);
            m_receivedResultCurrentRun = !result.items.isEmpty();
            m_precheckRunning = false;
            setBusyProgress(-1.0);
            if (m_receivedResultCurrentRun) {
                m_precheckProgress = QStringLiteral("预检完成");
                m_precheckError = result.error;
                saveResultSnapshot();
                archiveCurrentReport();
                loadReportHistory();
            } else {
                m_precheckProgress = QStringLiteral("预检未能生成结果");
                m_precheckError = result.error.isEmpty()
                    ? QStringLiteral("本次预检没有生成结果，请检查型号配置。")
                    : result.error;
            }
            emit resultsChanged();
            emit precheckStateChanged();
            emit precheckFinished();
        }, Qt::QueuedConnection);
    });
    m_precheckThread = thread;
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_precheckThread == thread) m_precheckThread = nullptr;
        thread->deleteLater();
    });
    thread->start();
}

void QmlAppController::openReport() {
    if (!m_cachedReportPath.isEmpty() && QFileInfo::exists(m_cachedReportPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_cachedReportPath));
        return;
    }
    m_precheckError = QStringLiteral("尚无可打开的预检报告，请先执行一键预检。");
    emit precheckStateChanged();
}

void QmlAppController::exportReport() {
    if (m_cachedReportPath.isEmpty() || !QFileInfo::exists(m_cachedReportPath)) {
        m_precheckError = QStringLiteral("尚无可导出的预检报告，请先执行一键预检。");
        emit precheckStateChanged();
        return;
    }
    const QString target = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("导出预检报告"),
        QStringLiteral("Precheck_Report.html"),
        QStringLiteral("HTML Files (*.html)"));
    if (target.isEmpty()) return;
    QFile source(m_cachedReportPath);
    QSaveFile destination(target);
    if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
        return;
    destination.write(source.readAll());
    destination.commit();
}
