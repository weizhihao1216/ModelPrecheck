#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QMetaType>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QScreen>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "core/ConcurrencyTester.h"
#include "core/PerfProfiler.h"
#include "qml/QmlAppController.h"
#include "utils/SehHelper.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

// 应用图标：青绿圆角底 + 白色盾牌与对勾，与界面主题保持一致。
QIcon BuildAppIcon() {
    QIcon icon;
    for (int size : { 16, 24, 32, 48, 64, 128, 256 }) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const qreal s = size / 100.0;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(13, 148, 136));
        painter.drawRoundedRect(QRectF(6 * s, 6 * s, 88 * s, 88 * s), 24 * s, 24 * s);

        QPainterPath shield;
        shield.moveTo(50 * s, 20 * s);
        shield.lineTo(77 * s, 31 * s);
        shield.lineTo(77 * s, 54 * s);
        shield.cubicTo(77 * s, 70 * s, 64 * s, 79 * s, 50 * s, 85 * s);
        shield.cubicTo(36 * s, 79 * s, 23 * s, 70 * s, 23 * s, 54 * s);
        shield.lineTo(23 * s, 31 * s);
        shield.closeSubpath();
        painter.setBrush(QColor(255, 255, 255, 240));
        painter.drawPath(shield);

        QPen pen(QColor(13, 148, 136), 8 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        QPainterPath check;
        check.moveTo(37 * s, 52 * s);
        check.lineTo(46 * s, 62 * s);
        check.lineTo(64 * s, 41 * s);
        painter.drawPath(check);
        painter.end();

        icon.addPixmap(pixmap);
    }
    return icon;
}

// 参考逻辑尺寸：本机（1920x1080 @125% 缩放，去掉任务栏后 1536x824 逻辑像素）下布局正常，
// 因此把“逻辑可用空间不小于 1536x824”作为布局基准。
constexpr int kReferenceLogicalWidth = 1536;
constexpr int kReferenceLogicalHeight = 824;

// 界面元素的有效缩放上限：本机 1920x1080 @125% 时元素与文字的物理大小（观感基准）。
// 系统缩放高于它时（如 2560x1600 @150%）按比例缩小，保证不同机器上控件与文字的
// 物理大小一致，布局与本机保持一致。
constexpr qreal kMaxEffectiveScale = 1.25;

/**
 * 反算全局缩放系数，使界面在不同分辨率/缩放下都与本机（1920x1080 @125%）观感一致。
 *
 * 两个约束：
 * 1) 元素有效缩放不超过 kMaxEffectiveScale —— 2560x1600 @150% 时若不做处理，
 *    元素会比本机大 20%，界面显得“整体很大”；
 * 2) 逻辑可用空间不小于基准尺寸 —— 2560x1600 @200% 这类情况下逻辑空间只有约
 *    1280x770，窗口会被压小导致内容被挤掉/截断。
 *
 * 结果通过 QT_SCALE_FACTOR 交给 Qt；本机反算结果是 1.0，不改变现有布局。
 *
 * 注意：必须在 QApplication 之前调用。此时本进程尚未做 DPI 感知，
 * Win32 返回的屏幕尺寸就是缩放后的逻辑尺寸（与 Qt 的坐标系一致）。
 */
qreal CalculateGlobalScaleFactor() {
    RECT work{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) return 1.0;
    const double width = work.right - work.left;
    const double height = work.bottom - work.top;
    if (width <= 0.0 || height <= 0.0) return 1.0;

    // 系统缩放 = 真实分辨率 / 逻辑分辨率
    qreal osScale = 1.0;
    const int logicalWidth = GetSystemMetrics(SM_CXSCREEN);
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (logicalWidth > 0 && EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode)
        && mode.dmPelsWidth > 0) {
        osScale = static_cast<qreal>(mode.dmPelsWidth) / logicalWidth;
    }

    const qreal byScale = kMaxEffectiveScale / osScale;
    const qreal byWidth = static_cast<qreal>(width / kReferenceLogicalWidth);
    const qreal byHeight = static_cast<qreal>(height / kReferenceLogicalHeight);
    // 上限 1.0：够大就不放大，保持现有布局；下限 0.6：屏幕太小时避免小到看不清。
    return qBound<qreal>(0.6, qMin<qreal>(1.0, qMin(byScale, qMin(byWidth, byHeight))), 1.0);
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // 必须在 QApplication 之前设置；用户自己设过 QT_SCALE_FACTOR 时不干预。
    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        const qreal factor = CalculateGlobalScaleFactor();
        // 差别极小时不动，避免在本机（反算结果恰好为 1.0）产生任何变化。
        if (factor < 0.995)
            qputenv("QT_SCALE_FACTOR", QByteArray::number(factor, 'f', 3));
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QApplication app(argc, argv);

    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    app.setWindowIcon(BuildAppIcon());

    qRegisterMetaType<PerfProfileReport>("PerfProfileReport");
    qRegisterMetaType<PerfSample>("PerfSample");
    // 专项测试通过跨线程信号回传结果，显式注册避免队列连接无法投递导致一直等待。
    qRegisterMetaType<ConcurrencyTestReport>("ConcurrencyTestReport");
    qRegisterMetaType<ConcurrencyThreadResult>("ConcurrencyThreadResult");

    // 清理上次运行遗留的被测模块副本（本次运行尚未加载任何模块，删除一定安全）。
    CleanupPrivateModuleCopies(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("TestModel"))
            .toUtf8().toStdString());

    int exitCode = 0;
    {
        QmlAppController controller;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(
            QStringLiteral("appController"), &controller);
        engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        if (engine.rootObjects().isEmpty())
            return -1;
        exitCode = app.exec();
    }   // 这里完成 Qt 侧的正常析构（会话保存、编译进程回收等）
    // 被测模块的私有副本始终留在进程内，其 DETACH 可能在退出时死循环；
    // 只要加载过副本就直接终止进程，避免出现关不掉的假死进程。
    if (HasLoadedPrivateModule())
        TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exitCode));
    return exitCode;
}
