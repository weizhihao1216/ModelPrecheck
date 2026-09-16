#include <QApplication>
#include <QMetaType>
#include <QFile>
#include <QTextStream>
#include <QScreen>
#include <QFont>
#include "core/PerfProfiler.h"
#include "ui/MainWindow.h"

namespace {

// Reference computer: 1920 x 1080 at Windows 100% scaling. A normal taskbar
// leaves roughly 1920 x 1040 logical pixels available. Qt already handles
// DPI-aware font rendering, so this factor is used only for major geometry.
qreal CalculateUiScale(const QSize& availableSize) {
    if (availableSize.isEmpty()) return 1.0;
    const qreal widthScale = availableSize.width() / 1920.0;
    const qreal heightScale = availableSize.height() / 1040.0;
    return qBound<qreal>(0.72, qMin<qreal>(1.0, qMin(widthScale, heightScale)), 1.0);
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    const QSize availableSize = app.primaryScreen()
        ? app.primaryScreen()->availableGeometry().size()
        : QSize(1920, 1040);
    const qreal uiScale = CalculateUiScale(availableSize);
    app.setProperty("modelPrecheckUiScale", uiScale);
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));

    qRegisterMetaType<PerfProfileReport>("PerfProfileReport");
    qRegisterMetaType<PerfSample>("PerfSample");

    QFile qssFile(":/ui/themestyle.qss");
    if (!qssFile.open(QFile::ReadOnly | QFile::Text)) {
        qssFile.setFileName(app.applicationDirPath() + "/themestyle.qss");
        qssFile.open(QFile::ReadOnly | QFile::Text);
    }
    if (qssFile.isOpen()) {
        QTextStream ts(&qssFile);
        ts.setCodec("UTF-8");
        app.setStyleSheet(ts.readAll());
        qssFile.close();
    }

    MainWindow w;
    w.show();

    return app.exec();
}
