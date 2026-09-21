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

// 布局设计基准逻辑尺寸：界面在 1920x1040 逻辑像素（即 1080p 屏幕不缩放时的可用区域）
// 下呈现设计比例——顶部 64px 头部、左侧 210px 导航、右侧 238/450px 两列，中间代码框
// 仍有约 1150px 宽，能完整显示对象代码模板并留出空白。
constexpr int kDesignLogicalWidth = 1920;
constexpr int kDesignLogicalHeight = 1040;

/**
 * 反算全局缩放系数，使界面在不同分辨率/缩放下都保持设计基准下的布局比例。
 *
 * Qt 的屏幕缩放 = “系统缩放取整后的值”（Windows 下为整数缩放策略，150% → 2.0），
 * QT_SCALE_FACTOR 则在其上相乘（实测 150% 系统缩放 + 0.833 → dpr 1.666）。
 * 因此要让逻辑可用空间达到设计基准，系数取：
 *
 *     系数 = min(物理宽 / 1920, 物理可用高 / 1040) / 系统缩放取整值
 *
 * 上限 1.0：屏幕足够大时不再放大（逻辑空间更大、布局更宽松）；下限 0.6：屏幕过小时
 * 避免文字小到看不清。结果通过 QT_SCALE_FACTOR 交给 Qt。
 *
 * 注意：必须在 QApplication 之前调用。此时本进程尚未做 DPI 感知，
 * Win32 返回的屏幕尺寸就是缩放后的逻辑尺寸。
 */
qreal CalculateGlobalScaleFactor() {
    RECT work{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) return 1.0;
    const double width = work.right - work.left;
    const double height = work.bottom - work.top;
    if (width <= 0.0 || height <= 0.0) return 1.0;

    // 系统缩放 = 真实分辨率 / 逻辑分辨率。按整数除法会得到 1.4997 这类值，
    // 先吸附到 Windows 的 25% 档位（100/125/150/175/200%），再按 Qt 的整数缩放策略取整。
    qreal osScale = 1.0;
    const int logicalWidth = GetSystemMetrics(SM_CXSCREEN);
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (logicalWidth > 0 && EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode)
        && mode.dmPelsWidth > 0) {
        osScale = static_cast<qreal>(mode.dmPelsWidth) / logicalWidth;
        osScale = qRound(osScale * 4.0) / 4.0;
    }

    // Qt 侧实际生效的基准缩放（系统缩放按 Round 策略取整；150% → 2.0）。
    const qreal qtBaseScale = qMax<qreal>(1.0, qRound(osScale));
    const qreal physicalWorkHeight = height * osScale;
    const qreal byWidth = static_cast<qreal>(mode.dmPelsWidth) / kDesignLogicalWidth;
    const qreal byHeight = static_cast<qreal>(physicalWorkHeight / kDesignLogicalHeight);
    const qreal result = qBound<qreal>(0.6, qMin(byWidth, byHeight) / qtBaseScale, 1.0);
    return result;
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
