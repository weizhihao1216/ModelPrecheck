#include <QApplication>
#include <QMetaType>
#include <QFile>
#include <QTextStream>
#include "core/PerfProfiler.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

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
