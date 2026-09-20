import QtQuick 2.11
import QtQuick.Window 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtCharts 2.3
import "NumberFormat.js" as NumberFormat

ApplicationWindow {
    id: window
    visible: true
    width: 1600
    height: 900
    minimumWidth: 1180
    minimumHeight: 720
    title: qsTr("第三方武器模型集成预检")
    color: "#f4f7fa"

    onClosing: function(close) {
        if (appController.precheckRunning || appController.compileBusy
                || appController.specialtyBusy || appController.multiObjectBusy)
            close.accepted = false
    }

    readonly property color ink: "#14213d"
    readonly property color secondaryInk: "#60708a"
    readonly property color teal: "#0d9488"
    readonly property color tealDark: "#0f766e"
    readonly property color tealPale: "#e6f7f5"
    readonly property color green: "#169b62"
    readonly property color greenPale: "#eaf8f1"
    readonly property color amber: "#e39a16"
    readonly property color amberPale: "#fff7e6"
    readonly property color red: "#d94b55"
    readonly property color redPale: "#fff0f1"
    readonly property color line: "#dce4ec"
    readonly property color panel: "#ffffff"

    // 日志着色：失败/异常 → 红，警告 → 黄，成功/通过 → 绿，其余为常规深灰。
    // 先剔除“失败=0 / 异常=0 / 错误=0”这类计数为 0 的表述，否则会被误判成失败。
    function logLineColor(line) {
        var text = String(line)
        var stripped = text.replace(/(失败|异常|错误)\s*[=:：]?\s*0(?![\dA-Za-z])/g, "")
        if (stripped.indexOf("ERROR") >= 0 || stripped.indexOf("FAIL") >= 0
                || stripped.indexOf("错误") >= 0 || stripped.indexOf("失败") >= 0
                || stripped.indexOf("异常") >= 0 || stripped.indexOf("未通过") >= 0
                || stripped.indexOf("不通过") >= 0)
            return red
        if (text.indexOf("WARN") >= 0 || text.indexOf("警告") >= 0
                || text.indexOf("疑似") >= 0)
            return "#b8791a"
        if (text.indexOf("SUCCESS") >= 0 || text.indexOf("PASS") >= 0
                || text.indexOf("成功") >= 0 || text.indexOf("通过") >= 0
                || text.indexOf("已加载") >= 0)
            return green
        return "#3d4d60"
    }

    function openPrimaryAction() {
        if (appController.modelCount === 0) {
            appController.addModelInQml()
            configurationPopup.open()
        } else if (appController.pendingCount > 0) {
            var rows = appController.models
            for (var i = 0; i < rows.length; ++i) {
                if (!rows[i].compiled) {
                    appController.selectModel(rows[i].index)
                    break
                }
            }
            configurationPopup.open()
        } else {
            appController.runPrecheck()
        }
    }

    function openConfiguration(modelIndex) {
        appController.selectModel(modelIndex)
        configurationPopup.open()
    }

    header: Rectangle {
        height: 64
        color: "#ffffff"
        border.color: window.line
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 22
            spacing: 12

            Rectangle {
                width: 36
                height: 36
                radius: 10
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#14a89a" }
                    GradientStop { position: 1.0; color: "#0b7f75" }
                }
                Text {
                    anchors.centerIn: parent
                    text: "◆"
                    color: "white"
                    font.pixelSize: 18
                }
            }
            ColumnLayout {
                spacing: 1
                Text {
                    text: qsTr("第三方武器模型集成预检")
                    color: window.ink
                    font.family: "Microsoft YaHei UI"
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    font.letterSpacing: 0.6
                }
                Text {
                    text: qsTr("DLL 集成预检 · 性能 · 并发 · 多对象")
                    color: window.secondaryInk
                    font.pixelSize: 11
                }
            }
            Text {
                text: "v1.1"
                color: window.secondaryInk
                font.pixelSize: 12
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("会话：") + appController.sessionLabel
                color: window.secondaryInk
                font.pixelSize: 13
            }
            Rectangle { width: 1; height: 26; color: window.line }
            AppButton {
                text: qsTr("导出报告")
                implicitWidth: 112
                implicitHeight: 38
                onClicked: appController.exportReport()
                contentItem: Text {
                    text: parent.text
                    color: window.ink
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? "#f6f9fb" : "white"
                    border.color: "#bdcad7"
                }
            }
            AppButton {
                text: qsTr("▶  一键预检")
                implicitWidth: 132
                implicitHeight: 40
                enabled: appController.modelCount > 0 && appController.pendingCount === 0
                onClicked: appController.runPrecheck()
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    opacity: parent.enabled ? 1.0 : 0.7
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: parent.enabled
                           ? (parent.pressed ? window.tealDark
                                             : (parent.hovered ? "#10a89b" : window.teal))
                           : "#9bbab7"
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: window.width < 1400 ? 190 : 210
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: window.line

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 24
                anchors.bottomMargin: 18
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Repeater {
                    model: [
                        { label: qsTr("开始预检"), mark: "⌂",
                          active: !configurationPopup.opened
                                  && !resultsPopup.opened && !reportCenterPopup.opened
                                  && !settingsPopup.opened && !specialtyPopup.opened, action: 0 },
                        { label: qsTr("型号与代码"), mark: "</>", active: configurationPopup.opened, action: 1 },
                        { label: qsTr("专项测试"), mark: "⌁", active: specialtyPopup.opened, action: 5 },
                        { label: qsTr("检查结果"), mark: "✓", active: resultsPopup.opened, action: 2 },
                        { label: qsTr("报告中心"), mark: "▥", active: reportCenterPopup.opened, action: 3 }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 52
                        radius: 7
                        color: modelData.active ? window.tealPale
                                                : (navMouse.containsMouse ? "#f4f7fa" : "transparent")

                        Rectangle {
                            visible: modelData.active
                            width: 3
                            height: parent.height - 12
                            radius: 2
                            color: window.teal
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        RowLayout {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 17
                            spacing: 14
                            Text {
                                Layout.preferredWidth: 27
                                Layout.alignment: Qt.AlignVCenter
                                text: modelData.mark
                                color: modelData.active ? window.teal : window.secondaryInk
                                font.pixelSize: modelData.action === 1 ? 12 : 19
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                // 与侧栏底部「设置」同样：行盒居中后文字仍偏高 1.5px（见该处注释），
                                // 用 topMargin=3（下移 1.5px）让文字正对图标中心。
                                Layout.topMargin: 3
                                text: modelData.label
                                color: modelData.active ? window.tealDark : window.ink
                                font.pixelSize: 14
                                font.weight: modelData.active ? Font.DemiBold : Font.Normal
                            }
                        }
                        MouseArea {
                            id: navMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modelData.action === 0) {
                                    configurationPopup.close()
                                    resultsPopup.close()
                                    reportCenterPopup.close()
                                    settingsPopup.close()
                                    specialtyPopup.close()
                                } else if (modelData.action === 1) configurationPopup.open()
                                else if (modelData.action === 2) resultsPopup.open()
                                else if (modelData.action === 3) reportCenterPopup.open()
                                else if (modelData.action === 5) specialtyPopup.open()
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    height: 48
                    radius: 7
                    color: settingsMouse.containsMouse ? "#f4f7fa" : "transparent"
                    // 图标与文字垂直对齐：Row 是顶部对齐（差 4.5px），RowLayout+AlignVCenter 只对齐行盒。
                    // 行盒居中后仍差 1.5px（字号 19/14 不同，字形墨迹在行盒里的位置不同）；
                    // 残差按实际渲染像素量得（齿轮墨迹中心 26.5、文字 25.0），
                    // 故给文字 topMargin=3 —— 布局把项下移 topMargin/2，取整后为 2px，
                    // 实测两者墨迹中心只差 0.5px（像素取整下的最小残差）。
                    RowLayout {
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 14
                        Text {
                            Layout.alignment: Qt.AlignVCenter
                            text: "⚙"
                            color: window.secondaryInk
                            font.pixelSize: 19
                        }
                        Text {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.topMargin: 3
                            text: qsTr("设置")
                            color: window.ink
                            font.pixelSize: 14
                        }
                    }
                    MouseArea {
                        id: settingsMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsPopup.open()
                    }
                }
            }
        }

        Flickable {
            id: mainFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: contentColumn.implicitHeight + 48
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            ColumnLayout {
                id: contentColumn
                width: mainFlick.width - (window.width < 1400 ? 36 : 54)
                x: (mainFlick.width - width) / 2
                y: 24
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            text: qsTr("准备开始预检")
                            color: window.ink
                            font.pixelSize: 27
                            font.weight: Font.Bold
                        }
                        Text {
                            text: appController.pendingCount > 0
                                  ? qsTr("完成 %1 个待办项后，即可检查全部型号").arg(appController.pendingCount)
                                  : (appController.modelCount > 0
                                     ? qsTr("所有型号均已就绪，可以开始一键预检")
                                     : qsTr("添加第一个型号，开始配置预检任务"))
                            color: window.secondaryInk
                            font.pixelSize: 14
                        }
                    }
                    Rectangle {
                        width: 280
                        height: 54
                        radius: 9
                        color: window.greenPale
                        border.color: "#c8ead9"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12
                            Text {
                                text: qsTr("准备度  %1%").arg(appController.readiness)
                                color: "#23724d"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 10
                                radius: 5
                                color: "#d4e4df"
                                Rectangle {
                                    width: parent.width * appController.readiness / 100
                                    height: parent.height
                                    radius: parent.radius
                                    color: window.green
                                    Behavior on width { NumberAnimation { duration: 280 } }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 104
                    radius: 9
                    color: window.panel
                    border.color: window.line

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 42
                        anchors.rightMargin: 42
                        spacing: 8

                        Repeater {
                            model: [
                                { number: "✓", title: qsTr("准备型号"), done: appController.modelCount > 0, current: appController.modelCount === 0 },
                                { number: "✓", title: qsTr("配置代码"), done: appController.modelCount > 0 && appController.readiness >= 67, current: appController.modelCount > 0 && appController.readiness < 67 },
                                { number: "3", title: qsTr("编译确认"), done: appController.modelCount > 0 && appController.pendingCount === 0, current: appController.modelCount > 0 && appController.readiness >= 67 && appController.pendingCount > 0 },
                                { number: "4", title: qsTr("执行预检"), done: false, current: appController.modelCount > 0 && appController.pendingCount === 0 }
                            ]
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle {
                                    width: 43
                                    height: 43
                                    radius: 22
                                    color: modelData.done ? window.green
                                          : (modelData.current ? window.teal : "#edf1f5")
                                    border.color: modelData.current ? "#7dd8d0" : "transparent"
                                    border.width: modelData.current ? 3 : 0
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.number
                                        color: modelData.done || modelData.current ? "white" : "#8390a3"
                                        font.pixelSize: 17
                                        font.weight: Font.Bold
                                    }
                                }
                                Column {
                                    spacing: 2
                                    Text {
                                        text: (index + 1) + ". " + modelData.title
                                        color: window.ink
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: modelData.done ? qsTr("已完成")
                                             : (modelData.current ? qsTr("进行中") : qsTr("待开始"))
                                        color: modelData.done ? window.green
                                             : (modelData.current ? window.teal : window.secondaryInk)
                                        font.pixelSize: 12
                                    }
                                }
                                Rectangle {
                                    visible: index < 3
                                    Layout.fillWidth: true
                                    height: 2
                                    color: modelData.done ? window.green : window.line
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(270, modelListColumn.implicitHeight + 54)
                    spacing: 16

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 610
                        radius: 9
                        color: window.panel
                        border.color: window.line

                        ColumnLayout {
                            id: modelListColumn
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 9

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTr("型号准备情况")
                                    color: window.ink
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                }
                                Item { Layout.fillWidth: true }
                                AppButton {
                                    text: qsTr("＋ 添加型号")
                                    implicitWidth: 104
                                    implicitHeight: 34
                                    onClicked: {
                                        appController.addModelInQml()
                                        configurationPopup.open()
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: window.tealDark
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 5
                                        color: parent.hovered ? window.tealPale : "white"
                                        border.color: "#78cfc7"
                                    }
                                }
                            }
                            Text {
                                text: qsTr("检查各型号的模型包、对象代码和编译状态")
                                color: window.secondaryInk
                                font.pixelSize: 12
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 35
                                radius: 4
                                color: "#f4f7fa"
                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    Text { width: parent.width * 0.24; anchors.verticalCenter: parent.verticalCenter; text: qsTr("型号"); color: window.secondaryInk; font.pixelSize: 12 }
                                    Text { width: parent.width * 0.20; anchors.verticalCenter: parent.verticalCenter; text: qsTr("模型包"); color: window.secondaryInk; font.pixelSize: 12 }
                                    Text { width: parent.width * 0.20; anchors.verticalCenter: parent.verticalCenter; text: qsTr("对象代码"); color: window.secondaryInk; font.pixelSize: 12 }
                                    Text { width: parent.width * 0.20; anchors.verticalCenter: parent.verticalCenter; text: qsTr("编译"); color: window.secondaryInk; font.pixelSize: 12 }
                                    Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("操作"); color: window.secondaryInk; font.pixelSize: 12 }
                                }
                            }

                            Text {
                                visible: appController.modelCount === 0
                                Layout.fillWidth: true
                                Layout.preferredHeight: 96
                                text: qsTr("尚未添加型号\n点击“添加型号”开始配置")
                                color: window.secondaryInk
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Repeater {
                                model: appController.models
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    height: 48
                                    radius: 6
                                    color: modelData.compiled ? "#f0faf5" : window.amberPale
                                    border.color: modelData.compiled ? "#d5efe1" : "#f3dfb7"

                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: 14
                                        Text {
                                            width: parent.width * 0.24
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: modelData.name
                                            color: window.ink
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            width: parent.width * 0.20
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: (modelData.packageReady ? "●  " : "○  ") + modelData.packageText
                                            color: modelData.packageReady ? window.green : window.amber
                                            font.pixelSize: 12
                                        }
                                        Text {
                                            width: parent.width * 0.20
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: (modelData.userMainReady ? "●  " : "○  ") + modelData.userMainText
                                            color: modelData.userMainReady ? window.green : window.amber
                                            font.pixelSize: 12
                                        }
                                        Text {
                                            width: parent.width * 0.20
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: (modelData.compiled ? "●  " : "!  ") + modelData.compileText
                                            color: modelData.compiled ? window.green : window.amber
                                            font.pixelSize: 12
                                        }
                                        AppButton {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 72
                                            height: 30
                                            text: modelData.compiled ? qsTr("查看") : qsTr("打开")
                                            onClicked: window.openConfiguration(modelData.index)
                                            contentItem: Text {
                                                text: parent.text
                                                color: window.tealDark
                                                font.pixelSize: 12
                                                font.weight: Font.DemiBold
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                radius: 5
                                                color: parent.hovered ? window.tealPale : "white"
                                                border.color: "#75c8c1"
                                            }
                                        }
                                    }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: window.width < 1400 ? 340 : 410
                        Layout.fillHeight: true
                        radius: 9
                        color: window.panel
                        border.color: window.line

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 12
                            Text {
                                text: qsTr("下一步操作")
                                color: window.ink
                                font.pixelSize: 18
                                font.weight: Font.Bold
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 7
                                color: appController.pendingCount > 0 ? "#f7fbfd" : window.greenPale
                                border.color: appController.pendingCount > 0 ? "#d9e7ef" : "#c8ead9"
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 10
                                    Text {
                                        Layout.fillWidth: true
                                        text: appController.nextActionTitle
                                        wrapMode: Text.WordWrap
                                        color: window.ink
                                        font.pixelSize: 17
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: appController.nextActionDetail
                                        wrapMode: Text.WordWrap
                                        color: window.secondaryInk
                                        font.pixelSize: 13
                                        lineHeight: 1.35
                                    }
                                    Item { Layout.fillHeight: true }
                                    AppButton {
                                        Layout.fillWidth: true
                                        implicitHeight: 48
                                        text: appController.modelCount === 0 ? qsTr("＋ 添加型号")
                                              : (appController.pendingCount > 0 ? qsTr("⚙  编译待处理型号")
                                                                                 : qsTr("▶  开始一键预检"))
                                        onClicked: window.openPrimaryAction()
                                        contentItem: Text {
                                            text: parent.text
                                            color: "white"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            radius: 6
                                            color: parent.pressed ? window.tealDark
                                                                  : (parent.hovered ? "#10a89b" : window.teal)
                                        }
                                    }
                                    CheckBox {
                                        id: returnAfterCompileBox
                                        visible: appController.pendingCount > 0
                                        text: qsTr("编译成功后返回此页开始预检")
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: checkItemGrid.implicitHeight + 108
                    radius: 9
                    color: window.panel
                    border.color: window.line

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("本次将执行 10 项检查")
                                color: window.ink
                                font.pixelSize: 18
                                font.weight: Font.Bold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: qsTr("◷  预计耗时约 2 分钟")
                                color: window.secondaryInk
                                font.pixelSize: 13
                            }
                        }
                        Text {
                            text: qsTr("对模型的集成完整性、依赖、性能与稳定性进行全面验证")
                            color: window.secondaryInk
                            font.pixelSize: 12
                        }
                        GridLayout {
                            id: checkItemGrid
                            Layout.fillWidth: true
                            columns: window.width < 1400 ? 3 : 5
                            columnSpacing: 9
                            rowSpacing: 9
                            Repeater {
                                model: [qsTr("头文件规范"), qsTr("LIB 库文件"), qsTr("DLL 依赖"),
                                        qsTr("DLL 接口"), qsTr("UserMain 性能"), qsTr("内存泄漏"),
                                        qsTr("运行轨迹"), qsTr("多型号并行"), qsTr("多线程稳定性"),
                                        qsTr("单线程多对象")]
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    height: 40
                                    radius: 6
                                    color: "#f4f8fa"
                                    border.color: "#dde8ee"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 11
                                        anchors.rightMargin: 10
                                        spacing: 8
                                        Rectangle {
                                            width: 6
                                            height: 6
                                            radius: 3
                                            color: window.teal
                                            Layout.alignment: Qt.AlignVCenter
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData
                                            color: window.ink
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 48
                    radius: 7
                    color: window.panel
                    border.color: window.line
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        Text { text: "☷"; color: window.secondaryInk; font.pixelSize: 18 }
                        Text {
                            text: appController.logCount > 0
                                  ? qsTr("运行日志（%1）").arg(appController.logCount)
                                  : qsTr("运行日志")
                            color: window.ink
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text { text: "⌄"; color: window.secondaryInk; font.pixelSize: 16 }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: logPopup.open()
                    }
                }
            }
        }
    }

    Popup {
        id: settingsPopup
        x: window.width < 1400 ? 190 : 210
        y: 0
        width: window.width - x
        height: window.height - window.header.height
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.NoAutoClose
        property string notice: ""

        background: Rectangle { color: "#f4f7fa" }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: "white"
                border.color: window.line
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 14
                    AppButton {
                        text: qsTr("‹  返回工作台")
                        implicitWidth: 118
                        implicitHeight: 36
                        onClicked: settingsPopup.close()
                        contentItem: Text {
                            text: parent.text
                            color: window.tealDark
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealPale : "white"
                            border.color: "#86cec8"
                        }
                    }
                    Column {
                        spacing: 2
                        Text {
                            text: qsTr("运行设置")
                            color: window.ink
                            font.pixelSize: 21
                            font.weight: Font.Bold
                        }
                        Text {
                            text: qsTr("可设置一键预检所用默认值")
                            color: window.secondaryInk
                            font.pixelSize: 12
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: environmentText.implicitWidth + 26
                        implicitHeight: 30
                        radius: 15
                        color: appController.toolchainReady ? window.greenPale : window.amberPale
                        Text {
                            id: environmentText
                            anchors.centerIn: parent
                            text: appController.toolchainReady ? qsTr("● 编译环境可用") : qsTr("● 未找到编译环境")
                            color: appController.toolchainReady ? window.green : window.amber
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: settingsColumn.implicitHeight + 48
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }

                ColumnLayout {
                    id: settingsColumn
                    width: Math.min(parent.width - 48, 920)
                    x: (parent.width - width) / 2
                    y: 24
                    spacing: 16

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: runtimeSettings.implicitHeight + 42
                        radius: 10
                        color: "white"
                        border.color: window.line

                        ColumnLayout {
                            id: runtimeSettings
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 20
                            spacing: 15

                            Text {
                                text: qsTr("一键预检默认值")
                                color: window.ink
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("一键预检按以下默认值执行；专项测试中可单独调整当次参数。")
                                color: window.secondaryInk
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: settingsPopup.width < 850 ? 1 : 2
                                columnSpacing: 14
                                rowSpacing: 12

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 82
                                    radius: 7
                                    color: "#f8fafc"
                                    border.color: window.line
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text { text: qsTr("压测次数"); color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                            Text { text: qsTr("每个型号执行的性能采样次数"); color: window.secondaryInk; font.pixelSize: 11 }
                                        }
                                        AppSpinBox {
                                            id: perfStepsBox
                                            from: 1
                                            to: 1000000
                                            stepSize: 1000
                                            editable: true
                                            implicitWidth: 142
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                            value: appController.perfSteps
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 82
                                    radius: 7
                                    color: "#f8fafc"
                                    border.color: window.line
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text { text: qsTr("目标频率"); color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                            Text { text: qsTr("性能测试的目标调用频率"); color: window.secondaryInk; font.pixelSize: 11 }
                                        }
                                        AppComboBox {
                                            id: perfHzBox
                                            implicitWidth: 142
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                            model: ["20 Hz", "50 Hz", "100 Hz", "200 Hz"]
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 82
                                    radius: 7
                                    color: "#f8fafc"
                                    border.color: window.line
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text { text: qsTr("内存增长上限"); color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                            Text { text: qsTr("超过上限时停止压测，0 表示不限制"); color: window.secondaryInk; font.pixelSize: 11 }
                                        }
                                        AppSpinBox {
                                            id: memoryCapBox
                                            from: 0
                                            to: 65536
                                            stepSize: 64
                                            editable: true
                                            implicitWidth: 142
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                            value: appController.perfMemCapMB
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 82
                                    radius: 7
                                    color: "#f8fafc"
                                    border.color: window.line
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 16
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text { text: qsTr("并发线程数"); color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                                            Text { text: qsTr("多线程稳定性检查使用的线程数"); color: window.secondaryInk; font.pixelSize: 11 }
                                        }
                                        AppSpinBox {
                                            id: threadCountBox
                                            from: 1
                                            to: 64
                                            editable: true
                                            implicitWidth: 142
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                            value: appController.threadCount
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: environmentColumn.implicitHeight + 40
                        radius: 10
                        color: "white"
                        border.color: window.line
                        ColumnLayout {
                            id: environmentColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 20
                            spacing: 9
                            Text { text: qsTr("本机环境"); color: window.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                            Text {
                                Layout.fillWidth: true
                                text: appController.toolchainReady
                                      ? qsTr("已找到 C++ 编译环境，一键预检可以正常编译 UserMain。")
                                      : qsTr("未找到兼容的 C++ 编译环境，编译 UserMain 时将无法继续。")
                                color: appController.toolchainReady ? window.green : window.amber
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: appController.toolchainReady
                                text: appController.toolchainPath
                                color: window.secondaryInk
                                font.pixelSize: 11
                                elide: Text.ElideMiddle
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            Layout.fillWidth: true
                            text: settingsPopup.notice
                            color: window.green
                            font.pixelSize: 12
                        }
                        AppButton {
                            visible: false
                            text: qsTr("高级设置")
                            implicitWidth: 104
                            implicitHeight: 38
                            onClicked: appController.openWorkspace()
                        }
                        AppButton {
                            text: qsTr("恢复推荐值")
                            implicitWidth: 116
                            implicitHeight: 38
                            onClicked: {
                                appController.resetRuntimeSettings()
                                settingsPopup.loadValues()
                                settingsPopup.notice = qsTr("已恢复推荐值")
                            }
                        }
                        AppButton {
                            text: qsTr("保存设置")
                            implicitWidth: 108
                            implicitHeight: 38
                            onClicked: {
                                var rates = [20, 50, 100, 200]
                                appController.saveRuntimeSettings(perfStepsBox.value,
                                                                  rates[perfHzBox.currentIndex],
                                                                  memoryCapBox.value,
                                                                  threadCountBox.value)
                                settingsPopup.notice = qsTr("设置已保存，将在下一次预检中生效")
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 6
                                color: parent.pressed ? window.tealDark : window.teal
                            }
                        }
                    }
                }
            }
        }

        function loadValues() {
            perfStepsBox.value = appController.perfSteps
            memoryCapBox.value = appController.perfMemCapMB
            threadCountBox.value = appController.threadCount
            var rates = [20, 50, 100, 200]
            var closest = 0
            var distance = Math.abs(rates[0] - appController.perfHz)
            for (var i = 1; i < rates.length; ++i) {
                var nextDistance = Math.abs(rates[i] - appController.perfHz)
                if (nextDistance < distance) {
                    closest = i
                    distance = nextDistance
                }
            }
            perfHzBox.currentIndex = closest
        }

        onOpened: {
            configurationPopup.close()
            resultsPopup.close()
            reportCenterPopup.close()
            specialtyPopup.close()
            notice = ""
            loadValues()
        }
    }

    Popup {
        id: specialtyPopup
        x: window.width < 1400 ? 190 : 210
        y: 0
        width: window.width - x
        height: window.height - window.header.height
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.NoAutoClose
        background: Rectangle { color: "#f4f7fa" }

        contentItem: ColumnLayout {
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: "white"
                border.color: window.line
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22
                    spacing: 14
                    AppButton {
                        text: qsTr("‹  返回工作台")
                        implicitWidth: 118
                        implicitHeight: 36
                        onClicked: specialtyPopup.close()
                        contentItem: Text {
                            text: parent.text
                            color: window.tealDark
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealPale : "white"
                            border.color: "#86cec8"
                        }
                    }
                    Column {
                        spacing: 2
                        Text { text: qsTr("专项测试"); color: window.ink; font.pixelSize: 21; font.weight: Font.Bold }
                        Text {
                            text: qsTr("针对当前型号单独执行性能、轨迹和多线程稳定性测试")
                            color: window.secondaryInk
                            font.pixelSize: 11
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: specialtyStatus.implicitWidth + 24
                        implicitHeight: 30
                        radius: 15
                        color: appController.specialtyBusy ? window.amberPale : window.greenPale
                        Text {
                            id: specialtyStatus
                            anchors.centerIn: parent
                            text: appController.specialtyBusy ? qsTr("● 测试执行中") : qsTr("● 可以开始")
                            color: appController.specialtyBusy ? window.amber : window.green
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 16
                spacing: 12

                Rectangle {
                    visible: false
                    Layout.preferredWidth: 255
                    Layout.fillHeight: true
                    radius: 9
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8
                        Text { text: qsTr("选择型号"); color: window.ink; font.pixelSize: 14; font.weight: Font.Bold }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("专项测试需要型号已完成 UserMain 编译。")
                            color: window.secondaryInk
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: appController.models
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 64
                                radius: 6
                                color: appController.selectedModelIndex === modelData.index
                                       ? window.tealPale : "#f8fafc"
                                border.color: appController.selectedModelIndex === modelData.index
                                              ? "#83d4cc" : window.line
                                Column {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 11
                                    anchors.rightMargin: 78
                                    spacing: 3
                                    Text {
                                        width: parent.width
                                        text: modelData.name
                                        color: window.ink
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width
                                        text: modelData.compiled ? qsTr("✓ 已编译") : qsTr("需要先编译")
                                        color: modelData.compiled ? window.green : window.amber
                                        font.pixelSize: 9
                                    }
                                }
                                AppSpinBox {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 106
                                    height: 34
                                    z: 2
                                    from: 1
                                    to: 64
                                    value: Number(modelData.instanceCount || 1)
                                    enabled: !appController.specialtyBusy
                                    editable: true
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("多型号并行实例数")
                                    onValueModified: appController.updateModelInstanceCount(modelData.index, value)
                                }
                                MouseArea {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.right: parent.right
                                    anchors.rightMargin: 76
                                    z: 1
                                    enabled: !appController.specialtyBusy
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appController.selectModel(modelData.index)
                                }
                            }
                        }
                    }
                }

                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: width
                    contentHeight: specialtyContent.implicitHeight + 8
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }
                    ColumnLayout {
                        id: specialtyContent
                        width: parent.width
                        spacing: 10

                        Rectangle {
                            visible: false
                            Layout.fillWidth: true
                            Layout.preferredHeight: 66
                            radius: 8
                            color: appController.specialtyBusy ? "#fff8eb" : "white"
                            border.color: appController.specialtyBusy ? "#ead19a" : window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                BusyIndicator {
                                    running: appController.specialtyBusy
                                    visible: running
                                    implicitWidth: 28
                                    implicitHeight: 28
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: appController.specialtyMessage.length > 0
                                          ? appController.specialtyMessage
                                          : qsTr("请选择已编译型号，然后执行需要的专项测试。")
                                    color: window.ink
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        Rectangle {
                            visible: false
                            Layout.fillWidth: true
                            Layout.preferredHeight: 94
                            radius: 8
                            color: "white"
                            border.color: window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 14
                                Rectangle {
                                    width: 42
                                    height: 42
                                    radius: 21
                                    color: Object.keys(appController.specialtyStaticReport).length === 0
                                           ? "#eef3f7"
                                           : (appController.specialtyStaticReport.overallPass
                                              ? window.greenPale : window.amberPale)
                                    Text {
                                        anchors.centerIn: parent
                                        text: Object.keys(appController.specialtyStaticReport).length === 0
                                              ? "⌕" : (appController.specialtyStaticReport.overallPass ? "✓" : "!")
                                        color: Object.keys(appController.specialtyStaticReport).length === 0
                                               ? window.secondaryInk
                                               : (appController.specialtyStaticReport.overallPass ? window.green : window.amber)
                                        font.pixelSize: 18
                                        font.weight: Font.Bold
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text { text: qsTr("模型包静态与加载检查"); color: window.ink; font.pixelSize: 14; font.weight: Font.Bold }
                                    Text {
                                        Layout.fillWidth: true
                                        text: Object.keys(appController.specialtyStaticReport).length === 0
                                              ? qsTr("检查头文件规范与冲突、LIB 架构和符号、DLL 依赖/导出及 Release 加载能力。")
                                              : qsTr("头文件 %1/%2 · LIB %3/%4 · DLL %5/%6")
                                                .arg(appController.specialtyStaticReport.headerPassed || 0)
                                                .arg((appController.specialtyStaticReport.headers || []).length)
                                                .arg(appController.specialtyStaticReport.libraryPassed || 0)
                                                .arg((appController.specialtyStaticReport.libraries || []).length)
                                                .arg(appController.specialtyStaticReport.dllPassed || 0)
                                                .arg((appController.specialtyStaticReport.dlls || []).length)
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                }
                                AppButton {
                                    visible: Object.keys(appController.specialtyStaticReport).length > 0
                                    text: qsTr("查看明细")
                                    implicitWidth: 92
                                    implicitHeight: 34
                                    onClicked: staticCheckPopup.open()
                                }
                                AppButton {
                                    text: qsTr("执行检查")
                                    implicitWidth: 100
                                    implicitHeight: 34
                                    enabled: appController.selectedModelIndex >= 0 && !appController.specialtyBusy
                                    onClicked: appController.runSelectedStaticChecks()
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: specialtyPopup.width < 1050 ? 1 : 2
                            rowSpacing: 10
                            columnSpacing: 10
                            Repeater {
                                model: [
                                    { title: qsTr("模型包检查"), detail: qsTr("检查头文件规范与冲突、LIB 架构和符号、DLL 依赖/导出及 Release 加载能力。"), button: qsTr("执行模型包检查"), action: 4 },
                                    { title: qsTr("性能与内存"), detail: qsTr("按设置中的次数和目标频率连续执行 UserMain，统计耗时、抖动与内存增长。"), button: qsTr("执行性能压测"), action: 0 },
                                    { title: qsTr("运行轨迹"), detail: qsTr("单次试跑并读取 RecordTrajectoryPoint 记录的经纬度轨迹。"), button: qsTr("采集运行轨迹"), action: 1 },
                                    { title: qsTr("多线程稳定性"), detail: qsTr("按设置的线程数同时运行当前型号，检查返回码及硬件异常。"), button: qsTr("执行多线程测试"), action: 2 },
                                    { title: qsTr("多型号并行"), detail: qsTr("将全部已编译型号按各自实例数同时运行，检查跨 DLL 并行时的异常和干扰。"), button: qsTr("执行多型号并行"), action: 3 },
                                    { title: qsTr("单线程多对象"), detail: qsTr("在同一线程内交替推进多个对象实例，检查对象间状态干扰与轨迹偏差，并支持跨型号交错。"), button: qsTr("设置并测试"), action: 5 }
                                ]
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 146
                                    radius: 8
                                    color: "white"
                                    border.color: window.line
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 6
                                        Text { text: modelData.title; color: window.ink; font.pixelSize: 14; font.weight: Font.Bold }
                                        Text {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            text: modelData.detail
                                            color: window.secondaryInk
                                            font.pixelSize: 10
                                            wrapMode: Text.WordWrap
                                        }
                                        AppButton {
                                            Layout.fillWidth: true
                                            implicitHeight: 34
                                            text: qsTr("测试")
                                            enabled: !appController.specialtyBusy
                                            onClicked: {
                                                if (modelData.action === 5) {
                                                    multiObjectPopup.open()
                                                } else {
                                                    specialtyTestDialog.testType = modelData.action
                                                    specialtyTestDialog.prepareSelection()
                                                    specialtyTestDialog.open()
                                                }
                                            }
                                            contentItem: Text {
                                                text: parent.text
                                                color: "white"
                                                opacity: parent.enabled ? 1.0 : 0.7
                                                font.pixelSize: 13
                                                font.weight: Font.Bold
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                radius: 6
                                                color: parent.enabled
                                                       ? (parent.pressed ? window.tealDark
                                                                         : (parent.hovered ? "#10a89b" : window.teal))
                                                       : "#c5ccd4"
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 110
                            visible: false
                            radius: 8
                            color: "white"
                            border.color: window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 16
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Text { text: qsTr("性能压测结果"); color: window.ink; font.pixelSize: 14; font.weight: Font.Bold }
                                    Text {
                                        text: qsTr("%1 · 平均 %2 ms · 最大 %3 ms · 抖动 %4 ms")
                                            .arg(appController.specialtyPerformance.perfVerdict || "N/A")
                                            .arg(Number(appController.specialtyPerformance.perfAverageMs || 0).toFixed(4))
                                            .arg(Number(appController.specialtyPerformance.perfMaximumMs || 0).toFixed(4))
                                            .arg(Number(appController.specialtyPerformance.perfJitterMs || 0).toFixed(4))
                                        color: window.secondaryInk
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: qsTr("内存变化 %1 MB · 每万次增长 %2 MB")
                                            .arg(Number(appController.specialtyPerformance.memoryDeltaMB || 0).toFixed(2))
                                            .arg(Number(appController.specialtyPerformance.memoryLeakRate || 0).toFixed(2))
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                    }
                                }
                                AppButton {
                                    text: qsTr("查看图表")
                                    implicitWidth: 96
                                    implicitHeight: 34
                                    onClicked: {
                                        var detail = appController.specialtyPerformance
                                        detail.trajectory = appController.specialtyTrajectory
                                        performanceDetailPopup.modelName = appController.selectedModelName
                                        performanceDetailPopup.detail = detail
                                        performanceDetailPopup.mode = "performance"
                                        performanceDetailPopup.open()
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 92
                            visible: false
                            radius: 8
                            color: "white"
                            border.color: window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Text { text: qsTr("轨迹采集结果"); color: window.ink; font.pixelSize: 14; font.weight: Font.Bold }
                                    Text {
                                        text: qsTr("已采集 %1 个经纬度点，可在图表中查看完整路径。")
                                            .arg(appController.specialtyTrajectory.length)
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                    }
                                }
                                AppButton {
                                    text: qsTr("查看轨迹")
                                    implicitWidth: 96
                                    implicitHeight: 34
                                    onClicked: {
                                        var detail = appController.specialtyPerformance
                                        detail.trajectory = appController.specialtyTrajectory
                                        detail.name = appController.selectedModelName
                                        detail.configuration = qsTr("专项轨迹试跑")
                                        detail.perfSamples = detail.perfSamples || []
                                        detail.perfVerdict = detail.perfVerdict || ""
                                        detail.trajectoryMessages = []
                                        performanceDetailPopup.modelName = appController.selectedModelName
                                        performanceDetailPopup.detail = detail
                                        performanceDetailPopup.mode = "trajectory"
                                        performanceDetailPopup.open()
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 102
                            visible: false
                            radius: 8
                            color: "white"
                            border.color: window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 14
                                Rectangle {
                                    width: 54
                                    height: 30
                                    radius: 15
                                    color: appController.specialtyConcurrency.verdict === "PASS" ? window.green : window.amber
                                    Text {
                                        anchors.centerIn: parent
                                        text: appController.specialtyConcurrency.verdict || "N/A"
                                        color: "white"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Text { text: qsTr("多线程稳定性结果"); color: window.ink; font.pixelSize: 14; font.weight: Font.Bold }
                                    Text {
                                        Layout.fillWidth: true
                                        text: appController.specialtyConcurrency.summary || ""
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                }
                                AppButton {
                                    text: qsTr("线程明细")
                                    implicitWidth: 96
                                    implicitHeight: 34
                                    onClicked: {
                                        concurrencyDetailPopup.detail = appController.specialtyConcurrency
                                        concurrencyDetailPopup.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        onOpened: {
            configurationPopup.close()
            resultsPopup.close()
            reportCenterPopup.close()
            settingsPopup.close()
            if (appController.selectedModelIndex < 0 && appController.modelCount > 0)
                appController.selectModel(0)
        }
    }

    Popup {
        id: specialtyTestDialog
        property int testType: 0
        property var selectedIndexes: []
        property bool runStarted: false
        readonly property bool multiSelection: testType === 3
        readonly property string testTitle: testType === 0 ? qsTr("性能与内存")
                                                   : (testType === 1 ? qsTr("运行轨迹")
                                                      : (testType === 2 ? qsTr("多线程稳定性")
                                                         : (testType === 3 ? qsTr("多型号并行")
                                                            : qsTr("模型包检查"))))
        function prepareSelection() {
            runStarted = false
            var choices = []
            var rows = appController.models
            for (var i = 0; i < rows.length; ++i) {
                if (multiSelection && rows[i].compiled)
                    choices.push(rows[i].index)
            }
            selectedIndexes = choices
            if (!multiSelection && appController.selectedModelIndex < 0 && rows.length > 0)
                appController.selectModel(rows[0].index)
        }
        function toggleModel(modelIndex, checked) {
            var next = selectedIndexes.slice(0)
            var pos = next.indexOf(modelIndex)
            if (checked && pos < 0) next.push(modelIndex)
            if (!checked && pos >= 0) next.splice(pos, 1)
            selectedIndexes = next
        }
        width: Math.min(window.width - 100, 900)
        height: Math.min(window.height - window.header.height - 70, 680)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: specialtyTestDialog.testTitle
                subtitle: specialtyTestDialog.multiSelection
                          ? qsTr("勾选至少两个型号并设置各自实例数，然后执行并行测试。")
                          : qsTr("先选择一个型号，再执行当前专项测试。")
                badge: Component {
                    Rectangle {
                        implicitWidth: stateText.implicitWidth + 22
                        implicitHeight: 28
                        radius: 14
                        color: appController.specialtyBusy ? window.amberPale : window.greenPale
                        Text {
                            id: stateText
                            anchors.centerIn: parent
                            text: appController.specialtyBusy ? qsTr("执行中") : qsTr("已完成")
                            color: appController.specialtyBusy ? window.amber : window.green
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }
                badgeVisible: specialtyTestDialog.runStarted
                onCloseRequested: specialtyTestDialog.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                spacing: 12
                Rectangle {
                    Layout.preferredWidth: 280
                    Layout.fillHeight: true
                    radius: 8
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 11
                        spacing: 7
                        Text {
                            text: specialtyTestDialog.multiSelection ? qsTr("选择参与并行的型号") : qsTr("选择测试型号")
                            color: window.ink
                            font.pixelSize: 13
                            font.weight: Font.Bold
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: !specialtyTestDialog.multiSelection
                            text: qsTr("灰色型号尚未满足当前测试条件。")
                            color: window.secondaryInk
                            font.pixelSize: 9
                            wrapMode: Text.WordWrap
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 6
                            clip: true
                            model: appController.models
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 62
                                radius: 6
                                color: (!specialtyTestDialog.multiSelection
                                        && appController.selectedModelIndex === modelData.index)
                                       || (specialtyTestDialog.multiSelection
                                           && specialtyTestDialog.selectedIndexes.indexOf(modelData.index) >= 0)
                                       ? window.tealPale : "#f8fafc"
                                border.color: window.line
                                readonly property bool canUse: specialtyTestDialog.testType === 4
                                                               ? modelData.packageReady : modelData.compiled
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 6
                                    CheckBox {
                                        visible: specialtyTestDialog.multiSelection
                                        checked: specialtyTestDialog.selectedIndexes.indexOf(modelData.index) >= 0
                                        enabled: parent.parent.canUse && !appController.specialtyBusy
                                        onClicked: specialtyTestDialog.toggleModel(modelData.index, checked)
                                    }
                                    RadioButton {
                                        visible: !specialtyTestDialog.multiSelection
                                        checked: appController.selectedModelIndex === modelData.index
                                        enabled: parent.parent.canUse && !appController.specialtyBusy
                                        onClicked: appController.selectModel(modelData.index)
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            color: parent.parent.parent.canUse ? window.ink : "#9aa6b5"
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: modelData.compiled ? qsTr("已编译")
                                                  : (modelData.packageReady ? qsTr("模型包已选择") : qsTr("未就绪"))
                                            color: modelData.compiled ? window.green : window.secondaryInk
                                            font.pixelSize: 8
                                        }
                                    }
                                    AppSpinBox {
                                        visible: specialtyTestDialog.multiSelection
                                        from: 1
                                        to: 64
                                        value: Number(modelData.instanceCount || 1)
                                        editable: true
                                        enabled: parent.parent.canUse && !appController.specialtyBusy
                                        implicitWidth: 104
                                        implicitHeight: 32
                                        onValueModified: appController.updateModelInstanceCount(modelData.index, value)
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("并行实例数")
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10
                        Text { text: qsTr("测试说明与结果"); color: window.ink; font.pixelSize: 14; font.weight: Font.Bold }
                        Text {
                            Layout.fillWidth: true
                            text: specialtyTestDialog.testType === 0
                                  ? qsTr("连续运行 UserMain，输出耗时曲线、实时性判定和内存增长。")
                                  : (specialtyTestDialog.testType === 1
                                     ? qsTr("试跑一次 UserMain，显示经纬度轨迹，并检查无效值、跳变、越界和坐标单位。")
                                     : (specialtyTestDialog.testType === 2
                                        ? qsTr("对所选型号同时启动多个线程，检查返回码和硬件异常。")
                                        : (specialtyTestDialog.testType === 3
                                           ? qsTr("仅对左侧勾选的型号执行并行测试，各型号实例数可分别设置。")
                                           : qsTr("检查所选型号的头文件、LIB、DLL 依赖、导出和 Release 加载能力。"))))
                            color: window.secondaryInk
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                        // 本测试项的运行参数（默认值来自「设置 · 一键预检默认值」，此处可为当次测试调整）
                        RowLayout {
                            Layout.fillWidth: true
                            visible: specialtyTestDialog.testType === 0
                                    || specialtyTestDialog.testType === 2
                            spacing: 10
                            ColumnLayout {
                                visible: specialtyTestDialog.testType === 0
                                spacing: 3
                                Text { text: qsTr("压测次数"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppSpinBox {
                                    id: dialogPerfStepsBox
                                    from: 1
                                    to: 1000000
                                    stepSize: 1000
                                    editable: true
                                    implicitWidth: 122
                                    implicitHeight: 32
                                    value: appController.perfSteps
                                }
                            }
                            ColumnLayout {
                                visible: specialtyTestDialog.testType === 0
                                spacing: 3
                                Text { text: qsTr("目标频率"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppComboBox {
                                    id: dialogPerfHzBox
                                    implicitWidth: 118
                                    implicitHeight: 32
                                    model: ["20 Hz", "50 Hz", "100 Hz", "200 Hz"]
                                    currentIndex: Math.max(0, [20, 50, 100, 200]
                                                           .indexOf(Math.round(appController.perfHz)))
                                }
                            }
                            ColumnLayout {
                                visible: specialtyTestDialog.testType === 0
                                spacing: 3
                                Text { text: qsTr("内存上限 MB"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppSpinBox {
                                    id: dialogMemCapBox
                                    from: 0
                                    to: 65536
                                    stepSize: 64
                                    editable: true
                                    implicitWidth: 122
                                    implicitHeight: 32
                                    value: appController.perfMemCapMB
                                }
                            }
                            ColumnLayout {
                                visible: specialtyTestDialog.testType === 2
                                spacing: 3
                                Text { text: qsTr("并发线程数"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppSpinBox {
                                    id: dialogThreadsBox
                                    from: 1
                                    to: 64
                                    editable: true
                                    implicitWidth: 122
                                    implicitHeight: 32
                                    value: appController.threadCount
                                }
                            }
                            Item { Layout.fillWidth: true }
                            AppButton {
                                text: qsTr("应用参数")
                                implicitWidth: 92
                                implicitHeight: 32
                                Layout.alignment: Qt.AlignBottom
                                onClicked: appController.saveRuntimeSettings(
                                    dialogPerfStepsBox.value,
                                    [20, 50, 100, 200][dialogPerfHzBox.currentIndex],
                                    dialogMemCapBox.value,
                                    dialogThreadsBox.value)
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70
                            radius: 6
                            color: appController.specialtyBusy ? "#fff8eb" : "#f7fafc"
                            border.color: appController.specialtyBusy ? "#ead19a" : window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                BusyIndicator { running: appController.specialtyBusy; visible: running; implicitWidth: 26; implicitHeight: 26 }
                                Text {
                                    Layout.fillWidth: true
                                    text: specialtyTestDialog.runStarted
                                          ? appController.specialtyMessage
                                          : qsTr("选择型号后点击下方按钮开始测试。")
                                    color: window.ink
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }

                        ColumnLayout {
                            visible: specialtyTestDialog.runStarted && !appController.specialtyBusy
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                visible: specialtyTestDialog.testType === 0
                                         && Object.keys(appController.specialtyPerformance).length > 0
                                text: qsTr("%1 · 平均 %2 ms · 最大 %3 ms · 抖动 %4 ms")
                                    .arg(appController.specialtyPerformance.perfVerdict || "N/A")
                                    .arg(Number(appController.specialtyPerformance.perfAverageMs || 0).toFixed(4))
                                    .arg(Number(appController.specialtyPerformance.perfMaximumMs || 0).toFixed(4))
                                    .arg(Number(appController.specialtyPerformance.perfJitterMs || 0).toFixed(4))
                                color: window.ink
                                font.pixelSize: 11
                            }
                            Text {
                                visible: specialtyTestDialog.testType === 1
                                text: qsTr("已采集 %1 个轨迹点").arg(appController.specialtyTrajectory.length)
                                color: window.ink
                                font.pixelSize: 11
                            }
                            Text {
                                visible: (specialtyTestDialog.testType === 2 || specialtyTestDialog.testType === 3)
                                         && Object.keys(appController.specialtyConcurrency).length > 0
                                Layout.fillWidth: true
                                text: appController.specialtyConcurrency.summary || ""
                                color: window.ink
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                visible: specialtyTestDialog.testType === 4
                                         && Object.keys(appController.specialtyStaticReport).length > 0
                                text: qsTr("头文件 %1/%2 · LIB %3/%4 · DLL %5/%6")
                                    .arg(appController.specialtyStaticReport.headerPassed || 0)
                                    .arg((appController.specialtyStaticReport.headers || []).length)
                                    .arg(appController.specialtyStaticReport.libraryPassed || 0)
                                    .arg((appController.specialtyStaticReport.libraries || []).length)
                                    .arg(appController.specialtyStaticReport.dllPassed || 0)
                                    .arg((appController.specialtyStaticReport.dlls || []).length)
                                color: window.ink
                                font.pixelSize: 11
                            }
                            AppButton {
                                visible: specialtyTestDialog.testType === 0
                                         && Object.keys(appController.specialtyPerformance).length > 0
                                text: qsTr("查看性能图")
                                onClicked: {
                                    var detail = appController.specialtyPerformance
                                    detail.trajectory = []
                                    performanceDetailPopup.modelName = appController.selectedModelName
                                    performanceDetailPopup.detail = detail
                                    performanceDetailPopup.mode = "performance"
                                    performanceDetailPopup.open()
                                }
                            }
                            AppButton {
                                visible: specialtyTestDialog.testType === 0
                                         && Object.keys(appController.specialtyPerformance).length > 0
                                text: qsTr("查看内存曲线")
                                onClicked: {
                                    var memoryDetail = appController.specialtyPerformance
                                    memoryDetail.trajectory = []
                                    performanceDetailPopup.modelName = appController.selectedModelName
                                    performanceDetailPopup.detail = memoryDetail
                                    performanceDetailPopup.mode = "memory"
                                    performanceDetailPopup.open()
                                }
                            }
                            AppButton {
                                visible: specialtyTestDialog.testType === 1
                                         && appController.specialtyTrajectory.length > 0
                                text: qsTr("查看轨迹图与质量指标")
                                onClicked: {
                                    var detail = ({})
                                    var nanCount = 0
                                    var outOfBounds = 0
                                    var jumpCount = 0
                                    var points = appController.specialtyTrajectory
                                    for (var i = 0; i < points.length; ++i) {
                                        var lat = Number(points[i].y)
                                        var lon = Number(points[i].x)
                                        if (!isFinite(lat) || !isFinite(lon)) nanCount++
                                        if (lat < -90 || lat > 90 || lon < -180 || lon > 180) outOfBounds++
                                        if (i > 0) {
                                            var prevLat = Number(points[i - 1].y)
                                            var prevLon = Number(points[i - 1].x)
                                            if (Math.abs(lat - prevLat) > 5 || Math.abs(lon - prevLon) > 5)
                                                jumpCount++
                                        }
                                    }
                                    detail.name = appController.selectedModelName
                                    detail.configuration = qsTr("专项轨迹试跑")
                                    detail.perfVerdict = ""
                                    detail.perfSamples = []
                                    detail.trajectory = points
                                    detail.trajectoryPoints = points.length
                                    detail.trajectoryNanCount = nanCount
                                    detail.trajectoryJumpCount = jumpCount
                                    detail.trajectoryOutOfBoundsCount = outOfBounds
                                    detail.trajectoryUnit = qsTr("经纬度（度）")
                                    detail.trajectoryMessages = []
                                    performanceDetailPopup.modelName = appController.selectedModelName
                                    performanceDetailPopup.detail = detail
                                    performanceDetailPopup.mode = "trajectory"
                                    performanceDetailPopup.open()
                                }
                            }
                            AppButton {
                                visible: (specialtyTestDialog.testType === 2 || specialtyTestDialog.testType === 3)
                                         && Object.keys(appController.specialtyConcurrency).length > 0
                                text: qsTr("查看执行明细")
                                onClicked: {
                                    concurrencyDetailPopup.detail = appController.specialtyConcurrency
                                    concurrencyDetailPopup.open()
                                }
                            }
                            AppButton {
                                visible: specialtyTestDialog.testType === 4
                                         && Object.keys(appController.specialtyStaticReport).length > 0
                                text: qsTr("查看文件明细")
                                onClicked: staticCheckPopup.open()
                            }
                        }

                        AppButton {
                            Layout.fillWidth: true
                            implicitHeight: 40
                            text: appController.specialtyBusy ? qsTr("正在执行…") : qsTr("开始") + specialtyTestDialog.testTitle
                            enabled: !appController.specialtyBusy
                                     && (specialtyTestDialog.multiSelection
                                         ? specialtyTestDialog.selectedIndexes.length >= 2
                                         : appController.selectedModelIndex >= 0)
                            onClicked: {
                                appController.saveRuntimeSettings(
                                    dialogPerfStepsBox.value,
                                    [20, 50, 100, 200][dialogPerfHzBox.currentIndex],
                                    dialogMemCapBox.value,
                                    dialogThreadsBox.value)
                                specialtyTestDialog.runStarted = true
                                if (specialtyTestDialog.testType === 0) appController.runSelectedPerformanceTest()
                                else if (specialtyTestDialog.testType === 1) appController.runSelectedTrajectoryTest()
                                else if (specialtyTestDialog.testType === 2) appController.runSelectedMultiThreadTest()
                                else if (specialtyTestDialog.testType === 3) appController.runMultiModelParallelTest(specialtyTestDialog.selectedIndexes)
                                else appController.runSelectedStaticChecks()
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: configurationPopup
        x: window.width < 1400 ? 190 : 210
        y: 0
        width: window.width - x
        height: window.height - window.header.height
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.NoAutoClose

        background: Rectangle { color: "#f4f7fa" }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                height: 70
                color: "white"
                border.color: window.line
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 14
                    AppButton {
                        text: qsTr("‹  返回工作台")
                        implicitWidth: 118
                        implicitHeight: 36
                        onClicked: configurationPopup.close()
                        contentItem: Text {
                            text: parent.text
                            color: window.tealDark
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealPale : "white"
                            border.color: "#86cec8"
                        }
                    }
                    Column {
                        spacing: 2
                        Text {
                            text: qsTr("型号与代码")
                            color: window.ink
                            font.pixelSize: 21
                            font.weight: Font.Bold
                        }
                        Text {
                            text: qsTr("选择模型包、编辑对象代码并完成编译")
                            color: window.secondaryInk
                            font.pixelSize: 12
                        }
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: appController.compileBusy ? qsTr("正在编译…") : qsTr("编译全部型号")
                        implicitWidth: 118
                        implicitHeight: 36
                        enabled: appController.modelCount > 0 && !appController.compileBusy
                        onClicked: appController.compileAllModelsInQml(codeEditor.text)
                        contentItem: Text {
                            text: parent.text
                            color: window.tealDark
                            opacity: parent.enabled ? 1 : 0.6
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealPale : "white"
                            border.color: "#78cfc7"
                        }
                    }
                    AppButton {
                        visible: false
                        text: qsTr("打开高级工作区")
                        implicitWidth: 132
                        implicitHeight: 36
                        onClicked: appController.openModel(appController.selectedModelIndex)
                        contentItem: Text {
                            text: parent.text
                            color: window.ink
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#f5f8fb" : "white"
                            border.color: "#bdcad7"
                        }
                    }
                }
            }

            RowLayout {
                id: configurationBody
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 18
                spacing: 16

                Rectangle {
                    Layout.preferredWidth: window.width < 1400 ? 196 : 238
                    Layout.fillHeight: true
                    radius: 9
                    color: "white"
                    border.color: window.line

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("型号列表")
                                color: window.ink
                                font.pixelSize: 16
                                font.weight: Font.Bold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: appController.modelCount
                                color: window.secondaryInk
                                font.pixelSize: 12
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7
                            AppButton {
                                Layout.fillWidth: true
                                implicitHeight: 38
                                text: qsTr("＋ 添加型号")
                                onClicked: appController.addModelInQml()
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: parent.pressed ? window.tealDark
                                                          : (parent.hovered ? "#10a89b" : window.teal)
                                }
                            }
                            AppButton {
                                implicitWidth: 38
                                implicitHeight: 38
                                text: "×"
                                enabled: appController.selectedModelIndex >= 0
                                onClicked: removeModelDialog.open()
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("删除当前型号")
                                contentItem: Text {
                                    text: parent.text
                                    color: window.red
                                    opacity: parent.enabled ? 1 : 0.4
                                    font.pixelSize: 20
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: parent.hovered ? "#fff0f1" : "white"
                                    border.color: "#efc4c8"
                                }
                            }
                        }
                        Text {
                            visible: appController.modelCount === 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: qsTr("暂无型号")
                            color: window.secondaryInk
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Repeater {
                            model: appController.models
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                height: 58
                                radius: 7
                                color: appController.selectedModelIndex === modelData.index
                                       ? window.tealPale
                                       : (configModelMouse.containsMouse ? "#f6f8fa" : "white")
                                border.color: appController.selectedModelIndex === modelData.index
                                              ? "#74cdc5" : window.line
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 10
                                    spacing: 9
                                    Rectangle {
                                        width: 10
                                        height: 10
                                        radius: 5
                                        color: modelData.compiled ? window.green : window.amber
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            elide: Text.ElideRight
                                            color: window.ink
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            text: modelData.compiled ? qsTr("编译成功") : qsTr("需要配置或编译")
                                            color: modelData.compiled ? window.green : window.amber
                                            font.pixelSize: 11
                                        }
                                    }
                                    Text {
                                        text: "›"
                                        color: window.secondaryInk
                                        font.pixelSize: 20
                                    }
                                }
                                MouseArea {
                                    id: configModelMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        codeEditor.focus = false
                                        appController.selectModel(modelData.index)
                                    }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        height: 92
                        radius: 9
                        color: "white"
                        border.color: window.line
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 14
                            Rectangle {
                                width: 42
                                height: 42
                                radius: 21
                                color: window.tealPale
                                Text {
                                    anchors.centerIn: parent
                                    text: "◇"
                                    color: window.teal
                                    font.pixelSize: 20
                                    font.weight: Font.Bold
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    text: appController.selectedModelName.length > 0
                                          ? appController.selectedModelName : qsTr("请选择型号")
                                    color: window.ink
                                    font.pixelSize: 17
                                    font.weight: Font.Bold
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: appController.selectedPackageDir.length > 0
                                          ? appController.selectedPackageDir
                                          : qsTr("尚未选择模型包路径")
                                    color: appController.selectedPackageDir.length > 0
                                           ? window.secondaryInk : window.amber
                                    font.pixelSize: 12
                                    elide: Text.ElideMiddle
                                }
                            }
                            AppButton {
                                text: qsTr("查看内容")
                                implicitWidth: 88
                                implicitHeight: 36
                                enabled: appController.selectedPackageDir.length > 0
                                onClicked: packageContentsPopup.open()
                            }
                            AppButton {
                                text: appController.selectedPackageDir.length > 0
                                      ? qsTr("重新选择") : qsTr("选择模型包")
                                implicitWidth: 112
                                implicitHeight: 36
                                enabled: appController.selectedModelIndex >= 0
                                onClicked: appController.browseSelectedPackage()
                                contentItem: Text {
                                    text: parent.text
                                    color: window.tealDark
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: parent.hovered ? window.tealPale : "white"
                                    border.color: "#78cfc7"
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 350
                        radius: 9
                        color: "white"
                        border.color: window.line
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTr("对象生命周期代码")
                                    color: window.ink
                                    font.pixelSize: 16
                                    font.weight: Font.Bold
                                }
                                Text {
                                    // 提示文案必须可省略、且不参与最小宽度：
                                    // 它不可换行时会给整列定下 500+ 的最小宽度，窗口一旦
                                    // 装不下，整列（含代码卡片）就被撑宽、盖掉右侧 16px 间距。
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    elide: Text.ElideRight
                                    text: qsTr("MoCreate / MoInit / MoStep / MoDestroy · 所有测试项共用")
                                    color: window.secondaryInk
                                    font.pixelSize: 11
                                }
                                Item { Layout.fillWidth: true }
                                AppButton {
                                    text: qsTr("保存代码")
                                    implicitWidth: 88
                                    implicitHeight: 32
                                    enabled: appController.selectedModelIndex >= 0
                                    onClicked: appController.saveSelectedUserMain(codeEditor.text)
                                    contentItem: Text {
                                        text: parent.text
                                        color: window.tealDark
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 5
                                        color: parent.hovered ? window.tealPale : "white"
                                        border.color: "#78cfc7"
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 46
                                radius: 6
                                color: "#f7fafc"
                                border.color: window.line
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("运行步数与步长：预检、压测、并发与多对象测试共用，修改后无需重新编译。")
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                    Text { text: qsTr("步数"); color: window.secondaryInk; font.pixelSize: 10 }
                                    AppSpinBox {
                                        id: runtimeStepsBox
                                        from: 1
                                        to: 100000
                                        stepSize: 1
                                        editable: true
                                        implicitWidth: 132
                                        implicitHeight: 32
                                    }
                                    Text { text: qsTr("步长 dt"); color: window.secondaryInk; font.pixelSize: 10 }
                                    AppTextField {
                                        id: runtimeDtField
                                        implicitWidth: 100
                                        font.pixelSize: 15
                                        validator: DoubleValidator { bottom: 0.000001; top: 60.0 }
                                    }
                                    AppButton {
                                        text: qsTr("应用")
                                        implicitWidth: 72
                                        implicitHeight: 32
                                        enabled: appController.selectedModelIndex >= 0
                                        onClicked: appController.saveStepSettings(
                                            runtimeStepsBox.value, Number(runtimeDtField.text))
                                    }
                                }
                            }
                            Rectangle {
                                id: codeEditorFrame
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                // 代码行较长时编辑器会有几百像素的隐式最小宽度，
                                // 若不加限制，窄窗口下它会溢出到中间列之外、被右侧
                                // 「随机变量」卡片盖住（右边框、滚动条、右侧留白都看不见）。
                                // 这里放开最小宽度，让它老实收在中间列内、由横向滚动条
                                // 承接超宽内容，小窗与满屏的观感保持一致。
                                Layout.minimumWidth: 0
                                radius: 6
                                color: "#111d27"
                                border.color: codeEditor.activeFocus ? window.teal : "#2f4658"
                                clip: true

                                // ---------- 代码补全：头文件符号 / 关键字 / 文档内标识符 ----------
                                property int completionIndex: 0
                                property string completionPrefix: ""
                                readonly property var completionKeywords: [
                                    "MoCreate", "MoInit", "MoStep", "MoDestroy",
                                    "double", "int", "float", "bool", "char", "void", "auto", "const",
                                    "struct", "class", "enum", "union", "typedef", "namespace", "return",
                                    "static", "inline", "extern", "template", "typename", "sizeof",
                                    "if", "else", "for", "while", "switch", "case", "break", "continue",
                                    "true", "false", "nullptr", "new", "delete", "public", "private",
                                    "protected", "virtual", "override", "constexpr", "std", "vector",
                                    "string", "size_t", "uint32_t", "int32_t", "uint64_t", "int64_t"
                                ]

                                function wordBeforeCursor() {
                                    var head = codeEditor.text.substring(0, codeEditor.cursorPosition)
                                    var matched = head.match(/[A-Za-z_][A-Za-z0-9_]*$/)
                                    return matched ? matched[0] : ""
                                }
                                function refreshCompletion() {
                                    if (!codeEditor.activeFocus) {
                                        completionModel.clear()
                                        return
                                    }
                                    var prefix = wordBeforeCursor()
                                    completionPrefix = prefix
                                    if (prefix.length < 2 || prefix.length > 40) {
                                        completionModel.clear()
                                        return
                                    }
                                    var picked = []
                                    var seen = {}
                                    function offer(word) {
                                        if (!word || word.length <= prefix.length || seen[word]) return
                                        if (word.indexOf(prefix) !== 0) return
                                        seen[word] = true
                                        picked.push(word)
                                    }
                                    var symbols = appController.selectedHeaderSymbols()
                                    for (var i = 0; i < symbols.length; ++i)
                                        offer(symbols[i])
                                    if (picked.length < 40) {
                                        for (var k = 0; k < completionKeywords.length; ++k)
                                            offer(completionKeywords[k])
                                    }
                                    var words = codeEditor.text.match(/[A-Za-z_][A-Za-z0-9_]*/g) || []
                                    for (var w = 0; w < words.length && picked.length < 80; ++w)
                                        offer(words[w])
                                    picked.sort()
                                    completionModel.clear()
                                    for (var p = 0; p < picked.length && p < 60; ++p)
                                        completionModel.append({ "word": picked[p] })
                                    completionIndex = 0
                                }
                                function moveCompletion(step) {
                                    if (completionModel.count === 0) return
                                    completionIndex = (completionIndex + step + completionModel.count)
                                                      % completionModel.count
                                }
                                function acceptCompletion() {
                                    if (completionModel.count === 0) return
                                    var word = completionModel.get(completionIndex).word
                                    if (completionPrefix.length > 0) {
                                        codeEditor.remove(codeEditor.cursorPosition - completionPrefix.length,
                                                          codeEditor.cursorPosition)
                                    }
                                    codeEditor.insert(codeEditor.cursorPosition, word)
                                    completionModel.clear()
                                }

                                ListModel { id: completionModel }

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 0
                                    Rectangle {
                                        Layout.preferredWidth: 46
                                        Layout.fillHeight: true
                                        color: "#0d1720"
                                        clip: true
                                        border.color: "#263946"
                                        Text {
                                            x: 0
                                            y: 12 - (codeScroll.contentItem ? codeScroll.contentItem.contentY : 0)
                                            width: parent.width - 8
                                            text: {
                                                var rows = []
                                                for (var line = 1; line <= codeEditor.lineCount; ++line)
                                                    rows.push(line)
                                                return rows.join("\n")
                                            }
                                            color: "#64788a"
                                            font.family: "Consolas"
                                            font.pixelSize: 13
                                            lineHeight: 1.25
                                            lineHeightMode: Text.ProportionalHeight
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                    ScrollView {
                                        id: codeScroll
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        // 同上：不再沿用文本内容撑出的最小宽度。
                                        Layout.minimumWidth: 0
                                        clip: true
                                        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                        TextArea {
                                            id: codeEditor
                                            text: appController.selectedUserMain
                                            enabled: appController.selectedModelIndex >= 0
                                            selectByMouse: true
                                            persistentSelection: true
                                            hoverEnabled: true
                                            wrapMode: TextEdit.NoWrap
                                            color: "#d9e7f5"
                                            selectionColor: "#2d647a"
                                            selectedTextColor: "white"
                                            font.family: "Consolas"
                                            font.pixelSize: 13
                                            leftPadding: 12
                                            rightPadding: 14
                                            topPadding: 12
                                            bottomPadding: 12
                                            Keys.onPressed: {
                                                if (completionModel.count > 0) {
                                                    if (event.key === Qt.Key_Down) {
                                                        codeEditorFrame.moveCompletion(1)
                                                        event.accepted = true
                                                        return
                                                    }
                                                    if (event.key === Qt.Key_Up) {
                                                        codeEditorFrame.moveCompletion(-1)
                                                        event.accepted = true
                                                        return
                                                    }
                                                    if (event.key === Qt.Key_Tab
                                                            || event.key === Qt.Key_Return
                                                            || event.key === Qt.Key_Enter) {
                                                        codeEditorFrame.acceptCompletion()
                                                        event.accepted = true
                                                        return
                                                    }
                                                    if (event.key === Qt.Key_Escape) {
                                                        completionModel.clear()
                                                        event.accepted = true
                                                        return
                                                    }
                                                }
                                                if (event.key === Qt.Key_Tab) {
                                                    codeEditor.insert(codeEditor.cursorPosition, "    ")
                                                    event.accepted = true
                                                }
                                            }
                                            onCursorPositionChanged: codeEditorFrame.refreshCompletion()
                                            onTextChanged: codeEditorFrame.refreshCompletion()
                                            onActiveFocusChanged: if (!activeFocus) completionModel.clear()
                                            Component.onCompleted:
                                                appController.attachCppHighlighter(codeEditor.textDocument)
                                            background: Rectangle {
                                                color: "#14202b"
                                                Rectangle {
                                                    x: 0
                                                    y: codeEditor.cursorRectangle.y
                                                    width: parent.width
                                                    height: Math.max(18, codeEditor.cursorRectangle.height)
                                                    color: "#192c39"
                                                    visible: codeEditor.activeFocus
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    id: completionBox
                                    z: 40
                                    visible: completionModel.count > 0
                                    readonly property point caretPos: codeEditor.mapToItem(
                                        codeEditorFrame,
                                        codeEditor.cursorRectangle.x,
                                        codeEditor.cursorRectangle.y + codeEditor.cursorRectangle.height)
                                    width: 300
                                    height: Math.min(completionList.contentHeight + 8, 180)
                                    x: Math.max(6, Math.min(caretPos.x, codeEditorFrame.width - width - 6))
                                    y: Math.max(6, Math.min(caretPos.y + 2,
                                                           codeEditorFrame.height - height - 6))
                                    radius: 6
                                    color: "#16232e"
                                    border.color: "#3d5c72"
                                    clip: true
                                    ListView {
                                        id: completionList
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        clip: true
                                        model: completionModel
                                        currentIndex: codeEditorFrame.completionIndex
                                        highlightMoveDuration: 0
                                        onCurrentIndexChanged: if (currentIndex >= 0)
                                                                   positionViewAtIndex(currentIndex, ListView.Contain)
                                        delegate: Rectangle {
                                            width: ListView.view.width
                                            height: 24
                                            radius: 4
                                            color: index === codeEditorFrame.completionIndex
                                                   ? "#1d4a55" : "transparent"
                                            Text {
                                                anchors.left: parent.left
                                                anchors.leftMargin: 8
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: model.word
                                                color: index === codeEditorFrame.completionIndex
                                                       ? "#7fe3d6" : "#c3d3e2"
                                                font.family: "Consolas"
                                                font.pixelSize: 12
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 60
                        radius: 8
                        color: appController.configurationMessage.indexOf(qsTr("失败")) >= 0
                               ? "#fff0f1" : "#eef8fb"
                        border.color: appController.configurationMessage.indexOf(qsTr("失败")) >= 0
                                      ? "#f1c2c6" : "#cfe5ed"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 15
                            anchors.rightMargin: 12
                            spacing: 12
                            Text {
                                Layout.fillWidth: true
                                text: appController.configurationMessage.length > 0
                                      ? appController.configurationMessage
                                      : qsTr("保存代码并完成编译后，即可返回工作台执行一键预检。")
                                color: window.secondaryInk
                                elide: Text.ElideRight
                                font.pixelSize: 12
                            }
                            AppButton {
                                text: appController.compileBusy ? qsTr("正在编译…") : qsTr("⚙  保存并编译")
                                implicitWidth: 132
                                implicitHeight: 40
                                enabled: appController.selectedModelIndex >= 0 && !appController.compileBusy
                                onClicked: appController.compileSelectedModel(codeEditor.text)
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    opacity: parent.enabled ? 1 : 0.7
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: parent.enabled
                                           ? (parent.pressed ? window.tealDark
                                                             : (parent.hovered ? "#10a89b" : window.teal))
                                           : "#9bbab7"
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    // 小窗口下收窄本列，把宽度让给中间的代码输入框。
                    Layout.preferredWidth: window.width < 1400 ? 360 : 450
                    Layout.fillHeight: true
                    radius: 9
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("随机变量")
                                color: window.ink
                                font.pixelSize: 16
                                font.weight: Font.Bold
                            }
                            Item { Layout.fillWidth: true }
                            AppButton {
                                text: qsTr("＋ 添加")
                                implicitWidth: 76
                                implicitHeight: 30
                                enabled: appController.selectedModelIndex >= 0
                                onClicked: appController.addRandomVariable()
                                contentItem: Text {
                                    text: parent.text
                                    color: window.tealDark
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 5
                                    color: parent.hovered ? window.tealPale : "white"
                                    border.color: "#78cfc7"
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("每次运行自动在最小值和最大值之间取值；在 UserMain 中通过 R.变量名 使用。")
                            color: window.secondaryInk
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Text { Layout.preferredWidth: 34; text: qsTr("启用"); color: window.secondaryInk; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter }
                            Text { Layout.preferredWidth: window.width < 1400 ? 62 : 90; text: qsTr("变量名"); color: window.secondaryInk; font.pixelSize: 9 }
                            Text { Layout.preferredWidth: window.width < 1400 ? 78 : 104; text: qsTr("类型"); color: window.secondaryInk; font.pixelSize: 9 }
                            Text { Layout.preferredWidth: window.width < 1400 ? 48 : 64; text: qsTr("最小值"); color: window.secondaryInk; font.pixelSize: 9 }
                            Text { Layout.preferredWidth: window.width < 1400 ? 48 : 64; text: qsTr("最大值"); color: window.secondaryInk; font.pixelSize: 9 }
                            Item { Layout.fillWidth: true }
                            Item { Layout.preferredWidth: 26 }
                        }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            ColumnLayout {
                                width: parent.width
                                spacing: 5
                                Repeater {
                                    model: appController.selectedRandomVars
                                    delegate: Rectangle {
                                        id: randomVariableRow
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 40
                                        radius: 6
                                        color: "#f8fafc"
                                        border.color: window.line

                                        function saveVariable() {
                                            appController.updateRandomVariable(
                                                index,
                                                enabledCheck.checked,
                                                nameField.text,
                                                typeField.currentText,
                                                Number(minField.text),
                                                Number(maxField.text))
                                        }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 2
                                            spacing: 5
                                            CheckBox {
                                                id: enabledCheck
                                                text: ""
                                                checked: modelData.enabled
                                                implicitWidth: 34
                                                implicitHeight: 24
                                                indicator: Rectangle {
                                                    implicitWidth: 18
                                                    implicitHeight: 18
                                                    x: 6
                                                    y: 3
                                                    radius: 4
                                                    color: enabledCheck.checked ? window.teal : "white"
                                                    border.color: enabledCheck.checked ? window.teal : "#a8b5c2"
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: enabledCheck.checked ? "✓" : ""
                                                        color: "white"
                                                        font.pixelSize: 12
                                                        font.weight: Font.Bold
                                                    }
                                                }
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("是否在运行时生成该随机变量")
                                                onClicked: randomVariableRow.saveVariable()
                                            }
                                            TextField {
                                                id: nameField
                                                Layout.preferredWidth: window.width < 1400 ? 62 : 90
                                                implicitHeight: 28
                                                text: modelData.name
                                                placeholderText: qsTr("变量名")
                                                selectByMouse: true
                                                color: window.ink
                                                placeholderTextColor: "#8b98a8"
                                                selectionColor: window.tealPale
                                                selectedTextColor: window.ink
                                                font.pixelSize: 11
                                                background: Rectangle {
                                                    radius: 4
                                                    color: "white"
                                                    border.color: nameField.activeFocus ? window.teal : window.line
                                                }
                                                ToolTip.visible: hovered && text.length > 0
                                                ToolTip.text: qsTr("代码中使用：R.%1").arg(text)
                                                onEditingFinished: randomVariableRow.saveVariable()
                                            }
                                            AppComboBox {
                                                id: typeField
                                                Layout.preferredWidth: window.width < 1400 ? 78 : 104
                                                implicitHeight: 28
                                                model: ["double", "int"]
                                                currentIndex: modelData.type === "int" ? 1 : 0
                                                font.pixelSize: 11
                                                contentItem: Text {
                                                    leftPadding: 9
                                                    rightPadding: 24
                                                    text: typeField.currentText
                                                    color: window.ink
                                                    font.pixelSize: 11
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideRight
                                                }
                                                indicator: Text {
                                                    x: typeField.width - width - 8
                                                    y: (typeField.height - height) / 2
                                                    text: "▾"
                                                    color: window.secondaryInk
                                                    font.pixelSize: 12
                                                }
                                                delegate: ItemDelegate {
                                                    width: typeField.width
                                                    height: 30
                                                    highlighted: typeField.highlightedIndex === index
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: window.ink
                                                        font.pixelSize: 11
                                                        leftPadding: 8
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    background: Rectangle {
                                                        color: parent.highlighted ? window.tealPale : "white"
                                                    }
                                                }
                                                popup: Popup {
                                                    y: typeField.height - 1
                                                    width: typeField.width
                                                    implicitHeight: contentItem.implicitHeight + 2
                                                    padding: 1
                                                    contentItem: ListView {
                                                        clip: true
                                                        implicitHeight: contentHeight
                                                        model: typeField.popup.visible ? typeField.delegateModel : null
                                                        currentIndex: typeField.highlightedIndex
                                                    }
                                                    background: Rectangle {
                                                        radius: 4
                                                        color: "white"
                                                        border.color: window.teal
                                                    }
                                                }
                                                background: Rectangle {
                                                    radius: 4
                                                    color: "white"
                                                    border.color: typeField.activeFocus ? window.teal : window.line
                                                }
                                                onActivated: randomVariableRow.saveVariable()
                                            }
                                            TextField {
                                                id: minField
                                                Layout.preferredWidth: window.width < 1400 ? 48 : 64
                                                implicitHeight: 28
                                                text: modelData.minimum
                                                selectByMouse: true
                                                validator: DoubleValidator { }
                                                color: window.ink
                                                horizontalAlignment: Text.AlignRight
                                                font.pixelSize: 11
                                                background: Rectangle {
                                                    radius: 4
                                                    color: "white"
                                                    border.color: minField.activeFocus ? window.teal : window.line
                                                }
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("随机范围最小值")
                                                onEditingFinished: randomVariableRow.saveVariable()
                                            }
                                            TextField {
                                                id: maxField
                                                Layout.preferredWidth: window.width < 1400 ? 48 : 64
                                                implicitHeight: 28
                                                text: modelData.maximum
                                                selectByMouse: true
                                                validator: DoubleValidator { }
                                                color: window.ink
                                                horizontalAlignment: Text.AlignRight
                                                font.pixelSize: 11
                                                background: Rectangle {
                                                    radius: 4
                                                    color: "white"
                                                    border.color: maxField.activeFocus ? window.teal : window.line
                                                }
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("随机范围最大值")
                                                onEditingFinished: randomVariableRow.saveVariable()
                                            }
                                            Item { Layout.fillWidth: true }
                                            AppButton {
                                                implicitWidth: 26
                                                implicitHeight: 26
                                                text: "×"
                                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                                onClicked: appController.removeRandomVariable(index)
                                                contentItem: Text {
                                                    text: parent.text
                                                    color: window.red
                                                    font.pixelSize: 17
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                background: Rectangle {
                                                    radius: 4
                                                    color: parent.hovered ? window.redPale : "white"
                                                    border.color: "#efc4c8"
                                                }
                                            }
                                        }
                                    }
                                }
                                Text {
                                    visible: appController.selectedRandomVars.length === 0
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 90
                                    text: qsTr("暂无随机变量\n点击“添加”创建变量")
                                    color: window.secondaryInk
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 12
                                }
                            }
                        }
                        // 编译状态与日志全文：状态栏只有一行放不下 cl.exe 的完整输出，
                        // 这里放在随机变量下方的空白区域，可换行、可滚动、可选中复制。
                        Rectangle {
                            visible: appController.compileLog.length > 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 172
                            Layout.minimumHeight: 96
                            radius: 6
                            color: appController.configurationMessage.indexOf(qsTr("失败")) >= 0
                                   ? "#fff7f7" : "#f7fafc"
                            border.color: appController.configurationMessage.indexOf(qsTr("失败")) >= 0
                                          ? "#f1c2c6" : window.line
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: qsTr("编译日志")
                                        color: window.ink
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: qsTr("可选中复制")
                                        color: window.secondaryInk
                                        font.pixelSize: 9
                                    }
                                }
                                ScrollView {
                                    id: compileLogScroll
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumWidth: 0
                                    clip: true
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                    TextArea {
                                        width: compileLogScroll.availableWidth
                                        readOnly: true
                                        selectByMouse: true
                                        text: appController.compileLog
                                        wrapMode: TextArea.Wrap
                                        color: "#33415c"
                                        font.family: "Consolas"
                                        font.pixelSize: 10
                                        background: Rectangle { color: "transparent" }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        onOpened: {
            settingsPopup.close()
            resultsPopup.close()
            reportCenterPopup.close()
            specialtyPopup.close()
            if (appController.selectedModelIndex < 0 && appController.modelCount > 0)
                appController.selectModel(0)
            codeEditor.text = appController.selectedUserMain
            runtimeStepsBox.value = appController.selectedMultiObjectSteps
            runtimeDtField.text = String(appController.selectedMultiObjectDt)
        }
    }

    Popup {
        id: packageContentsPopup
        property string fileFilter: qsTr("全部")
        width: Math.min(window.width - 100, 860)
        height: Math.min(window.height - window.header.height - 70, 680)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            radius: 11
            color: "white"
            border.color: window.line
        }

        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: qsTr("模型包内容")
                subtitle: appController.selectedPackageDir
                badge: Component {
                    Rectangle {
                        implicitWidth: packageLayoutText.implicitWidth + 24
                        implicitHeight: 28
                        radius: 14
                        color: appController.selectedPackageLayoutValid ? window.greenPale : window.amberPale
                        Text {
                            id: packageLayoutText
                            anchors.centerIn: parent
                            text: appController.selectedPackageLayoutValid ? qsTr("结构有效") : qsTr("结构不完整")
                            color: appController.selectedPackageLayoutValid ? window.green : window.amber
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }
                onCloseRequested: packageContentsPopup.close()
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.margins: 14
                height: 52
                radius: 7
                color: "#f8fafc"
                border.color: window.line
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    Text {
                        Layout.fillWidth: true
                        text: appController.selectedPackageSummary
                        color: window.secondaryInk
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                spacing: 7
                Repeater {
                    model: [qsTr("全部"), qsTr("头文件"), "LIB", "DLL"]
                    delegate: AppButton {
                        text: modelData
                        implicitWidth: 76
                        implicitHeight: 32
                        onClicked: packageContentsPopup.fileFilter = modelData
                        contentItem: Text {
                            text: parent.text
                            color: packageContentsPopup.fileFilter === parent.text ? "white" : window.secondaryInk
                            font.pixelSize: 11
                            font.weight: packageContentsPopup.fileFilter === parent.text ? Font.DemiBold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 16
                            color: packageContentsPopup.fileFilter === parent.text ? window.teal : "white"
                            border.color: packageContentsPopup.fileFilter === parent.text ? window.teal : window.line
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                AppButton {
                    text: qsTr("取消全部头文件")
                    implicitWidth: 140
                    implicitHeight: 32
                    onClicked: appController.selectAllPackageHeaders(false)
                }
                AppButton {
                    text: qsTr("选择全部头文件")
                    implicitWidth: 140
                    implicitHeight: 32
                    onClicked: appController.selectAllPackageHeaders(true)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                radius: 8
                color: "#f8fafc"
                border.color: window.line
                Flickable {
                    id: packageListFlick
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    contentWidth: width
                    contentHeight: packageListColumn.implicitHeight + 4
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }
                    ColumnLayout {
                        id: packageListColumn
                        width: packageListFlick.width - 12
                        spacing: 6
                        Repeater {
                            model: {
                                var all = appController.selectedPackageContents
                                if (packageContentsPopup.fileFilter === qsTr("全部")) return all
                                var filtered = []
                                for (var i = 0; i < all.length; ++i)
                                    if (all[i].kind === packageContentsPopup.fileFilter) filtered.push(all[i])
                                return filtered
                            }
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                height: 56
                                radius: 6
                                color: "white"
                                border.color: window.line
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 12
                                    spacing: 10
                                    Rectangle {
                                        Layout.preferredWidth: 42
                                        Layout.preferredHeight: 24
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 12
                                        color: modelData.kind === qsTr("头文件") ? window.tealPale
                                               : (modelData.kind === "LIB" ? "#eef2ff" : window.amberPale)
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.kind
                                            color: modelData.kind === qsTr("头文件") ? window.tealDark
                                                   : (modelData.kind === "LIB" ? "#5264b0" : window.amber)
                                            font.pixelSize: 9
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 2
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            color: window.ink
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.path
                                            color: window.secondaryInk
                                            font.pixelSize: 9
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                    Rectangle {
                                        visible: modelData.configuration.length > 0
                                        Layout.preferredWidth: 62
                                        Layout.preferredHeight: 24
                                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        radius: 12
                                        color: modelData.configuration === "Debug" ? "#fff0f1" : window.greenPale
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.configuration
                                            color: modelData.configuration === "Debug" ? window.red : window.green
                                            font.pixelSize: 9
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                    CheckBox {
                                        visible: modelData.kind === qsTr("头文件")
                                        checked: modelData.selected
                                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        onClicked: appController.setHeaderSelected(modelData.path, checked)
                                    }
                                }
                            }
                        }
                        Text {
                            visible: appController.selectedPackageContents.length === 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            text: qsTr("没有扫描到可显示的文件")
                            color: window.secondaryInk
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: multiObjectPopup
        width: Math.min(window.width - 100, 1180)
        height: Math.min(window.height - window.header.height - 60, 840)
        x: (window.width - width) / 2
        y: 40
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0

            PopupHeader {
                title: qsTr("单线程多对象")
                subtitle: qsTr("同一线程内交替推进多个对象实例，检查对象间状态干扰与轨迹偏差")
                badge: Component {
                    Rectangle {
                        implicitWidth: objectHeaderState.implicitWidth + 26
                        implicitHeight: 28
                        radius: 14
                        color: appController.selectedMultiObjectCompiled
                               ? window.greenPale : window.amberPale
                        Text {
                            id: objectHeaderState
                            anchors.centerIn: parent
                            text: appController.selectedMultiObjectCompiled
                                  ? qsTr("已编译") : qsTr("待编译")
                            color: appController.selectedMultiObjectCompiled
                                   ? window.green : window.amber
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }
                onCloseRequested: multiObjectPopup.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                spacing: 14

                // ==================== 左：单型号多对象 ====================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    radius: 9
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("单型号多对象")
                                color: window.ink
                                font.pixelSize: 15
                                font.weight: Font.Bold
                            }
                            Item { Layout.fillWidth: true }
                            AppButton {
                                text: qsTr("型号与代码")
                                implicitWidth: 100
                                implicitHeight: 32
                                onClicked: {
                                    multiObjectPopup.close()
                                    configurationPopup.open()
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("代码、步数与步长在“型号与代码”页设置；本页只设置多对象运行参数。")
                            color: window.secondaryInk
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 56
                            radius: 7
                            color: "#f8fafc"
                            border.color: window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 10
                                spacing: 10
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: appController.selectedModelName.length > 0
                                              ? appController.selectedModelName : qsTr("请先添加型号")
                                        color: window.ink
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: appController.selectedMultiObjectCompiled
                                              ? qsTr("对象代码已编译")
                                              : qsTr("尚未编译，请先在“型号与代码”页编译")
                                        color: appController.selectedMultiObjectCompiled
                                               ? window.green : window.amber
                                        font.pixelSize: 10
                                    }
                                }
                                AppComboBox {
                                    id: singleModelBox
                                    implicitWidth: 166
                                    implicitHeight: 32
                                    model: appController.models
                                    textRole: "name"
                                    onActivated: {
                                        var row = appController.models[currentIndex]
                                        if (row) appController.selectModel(row.index)
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            ColumnLayout {
                                spacing: 3
                                Text { text: qsTr("对象数"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppSpinBox {
                                    id: multiObjectCountBox
                                    from: 2
                                    to: 64
                                    editable: true
                                    implicitWidth: 118
                                    implicitHeight: 32
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text { text: qsTr("判定容差"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppToleranceField {
                                    id: multiObjectToleranceField
                                    Layout.fillWidth: true
                                }
                            }
                            ColumnLayout {
                                spacing: 3
                                Text { text: qsTr("调度顺序"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppComboBox {
                                    id: multiObjectScheduleBox
                                    implicitWidth: 166
                                    implicitHeight: 32
                                    model: [qsTr("正序"), qsTr("逆序"), qsTr("固定种子随机顺序")]
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("步数 %1 步 · dt %2（取自“型号与代码”页）")
                                  .arg(appController.selectedMultiObjectSteps)
                                  .arg(appController.selectedMultiObjectDt)
                            color: window.secondaryInk
                            font.pixelSize: 10
                        }

                        AppButton {
                            Layout.fillWidth: true
                            implicitHeight: 40
                            text: appController.multiObjectBusy ? qsTr("执行中…") : qsTr("▶  执行单型号测试")
                            enabled: appController.selectedModelIndex >= 0 && !appController.multiObjectBusy
                            onClicked: appController.runSelectedMultiObject(
                                multiObjectCountBox.value,
                                Number(multiObjectToleranceField.value),
                                multiObjectScheduleBox.currentIndex)
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 7
                            color: "#f8fafc"
                            border.color: window.line
                            ObjectResultPanel {
                                anchors.fill: parent
                                anchors.margins: 12
                                trajectorySeries: appController.multiObjectResultIsFleet
                                                  ? [] : appController.multiObjectTrajectories
                                resultItems: appController.multiObjectResultIsFleet
                                             ? [] : appController.multiObjectResultItems
                                verdict: appController.multiObjectResultIsFleet
                                         ? "" : appController.multiObjectVerdict
                                summary: appController.multiObjectResultIsFleet
                                         ? "" : appController.multiObjectSummary
                                maxDeviation: appController.multiObjectResultIsFleet
                                              ? 0 : appController.multiObjectMaxDeviation
                                maxFrameMs: appController.multiObjectResultIsFleet
                                            ? 0 : appController.multiObjectMaxFrameMs
                                memoryDeltaMB: appController.multiObjectResultIsFleet
                                               ? 0 : appController.multiObjectMemoryDeltaMB
                                emptyHint: qsTr("设置参数后点击上方按钮开始测试，结论、轨迹与逐对象明细会显示在这里。")
                            }
                        }
                    }
                }

                // ==================== 右：跨型号交错 ====================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    radius: 9
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10
                        Text {
                            text: qsTr("跨型号对象交错")
                            color: window.ink
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("同一线程中交替推进多个型号的对象，检查跨 DLL 对象级串扰。")
                            color: window.secondaryInk
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Flickable {
                            id: fleetListFlick
                            Layout.fillWidth: true
                            Layout.preferredHeight: 128
                            Layout.minimumHeight: 90
                            clip: true
                            contentWidth: width
                            contentHeight: fleetListColumn.implicitHeight + 4
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { }
                            ColumnLayout {
                                id: fleetListColumn
                                width: fleetListFlick.width - 12
                                spacing: 6
                                Repeater {
                                    model: appController.fleetMultiObjectModels
                                    delegate: Rectangle {
                                        Layout.fillWidth: true
                                        height: 58
                                        radius: 6
                                        color: modelData.ready ? "#f8fafc" : "#f4f5f7"
                                        border.color: modelData.selected ? "#79cec7" : window.line
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10
                                            spacing: 8
                                            CheckBox {
                                                id: fleetModelCheck
                                                Layout.alignment: Qt.AlignVCenter
                                                checked: modelData.selected
                                                enabled: modelData.ready && !appController.multiObjectBusy
                                                onClicked: appController.updateFleetMultiObjectModel(
                                                    modelData.index, checked, fleetObjectCount.value)
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                Layout.alignment: Qt.AlignVCenter
                                                spacing: 2
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.name
                                                    color: modelData.ready ? window.ink : "#8d98a7"
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    text: modelData.ready
                                                          ? qsTr("对象代码已编译")
                                                          : qsTr("请先在“型号与代码”页编译")
                                                    color: modelData.ready ? window.green : window.amber
                                                    font.pixelSize: 9
                                                }
                                            }
                                            Text {
                                                text: qsTr("对象数")
                                                color: window.secondaryInk
                                                font.pixelSize: 10
                                                Layout.alignment: Qt.AlignVCenter
                                            }
                                            AppSpinBox {
                                                id: fleetObjectCount
                                                Layout.alignment: Qt.AlignVCenter
                                                from: 1
                                                to: 32
                                                editable: true
                                                implicitWidth: 108
                                                implicitHeight: 30
                                                value: modelData.objectCount
                                                enabled: modelData.ready && fleetModelCheck.checked
                                                onValueModified: appController.updateFleetMultiObjectModel(
                                                    modelData.index, fleetModelCheck.checked, value)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text { text: qsTr("判定容差"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppToleranceField {
                                    id: fleetToleranceField
                                    Layout.fillWidth: true
                                }
                            }
                            ColumnLayout {
                                spacing: 3
                                Text { text: qsTr("调度顺序"); color: window.secondaryInk; font.pixelSize: 10 }
                                AppComboBox {
                                    id: fleetScheduleBox
                                    implicitWidth: 166
                                    implicitHeight: 32
                                    model: [qsTr("正序"), qsTr("逆序"), qsTr("固定种子随机顺序")]
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("步数 %1 步 · dt %2（取自当前型号“型号与代码”页）")
                                  .arg(appController.selectedMultiObjectSteps)
                                  .arg(appController.selectedMultiObjectDt)
                            color: window.secondaryInk
                            font.pixelSize: 10
                        }

                        AppButton {
                            Layout.fillWidth: true
                            implicitHeight: 40
                            text: appController.multiObjectBusy
                                  ? qsTr("正在测试…") : qsTr("▶  开始跨型号测试")
                            enabled: appController.modelCount >= 2 && !appController.multiObjectBusy
                            onClicked: appController.runFleetMultiObject(
                                Number(fleetToleranceField.value),
                                fleetScheduleBox.currentIndex)
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 240
                            radius: 7
                            color: "#f8fafc"
                            border.color: window.line
                            ObjectResultPanel {
                                anchors.fill: parent
                                anchors.margins: 12
                                trajectorySeries: appController.multiObjectResultIsFleet
                                                  ? appController.multiObjectTrajectories : []
                                resultItems: appController.multiObjectResultIsFleet
                                             ? appController.multiObjectResultItems : []
                                verdict: appController.multiObjectResultIsFleet
                                         ? appController.multiObjectVerdict : ""
                                summary: appController.multiObjectResultIsFleet
                                         ? appController.multiObjectSummary : ""
                                maxDeviation: appController.multiObjectResultIsFleet
                                              ? appController.multiObjectMaxDeviation : 0
                                maxFrameMs: appController.multiObjectResultIsFleet
                                            ? appController.multiObjectMaxFrameMs : 0
                                memoryDeltaMB: appController.multiObjectResultIsFleet
                                               ? appController.multiObjectMemoryDeltaMB : 0
                                emptyHint: qsTr("勾选至少两个型号并设置各自对象数后开始测试，结论、轨迹与逐对象明细会显示在这里。")
                            }
                        }
                    }
                }
            }
        }

        function loadValues() {
            multiObjectCountBox.value = appController.selectedMultiObjectCount
            // 容差未设置（0）时控制器会给默认值，这里只负责显示。
            multiObjectToleranceField.value = appController.selectedMultiObjectTolerance
            multiObjectScheduleBox.currentIndex = appController.selectedMultiObjectSchedule
            fleetToleranceField.value = appController.selectedMultiObjectTolerance
            fleetScheduleBox.currentIndex = appController.selectedMultiObjectSchedule
        }

        onOpened: {
            settingsPopup.close()
            configurationPopup.close()
            resultsPopup.close()
            reportCenterPopup.close()
            if (appController.selectedModelIndex < 0 && appController.modelCount > 0)
                appController.selectModel(0)
            appController.initializeFleetMultiObjectSelection()
            if (singleModelBox.count > 0)
                singleModelBox.currentIndex = Math.max(0, appController.selectedModelIndex)
            loadValues()
        }
    }

    Popup {
        id: resultsPopup
        property string resultFilter: qsTr("全部")
        // 当前展开的检查项目名称（手风琴：同时只展开一项）
        property string expandedResultName: ""
        // 展开区按条目类型分发：报告类条目只看自己的报告，其余条目看型号级明细。
        function isReportItem(name) {
            return showsHeaderConflict(name) || showsConcurrency(name) || showsMultiObject(name)
        }
        function showsHeaderConflict(name) {
            return name === qsTr("跨型号头文件冲突")
        }
        function showsConcurrency(name) {
            return name === qsTr("多型号并行") || name === qsTr("多线程稳定性")
        }
        function showsMultiObject(name) {
            return name === qsTr("单线程多对象")
        }
        function concurrencyReportsFor(name) {
            var all = appController.resultConcurrency
            var out = []
            var wantMultiModel = (name === qsTr("多型号并行"))
            for (var i = 0; i < all.length; ++i) {
                var title = String(all[i].title || "")
                var isMultiModel = title.indexOf(qsTr("多型号")) >= 0
                if (isMultiModel === wantMultiModel)
                    out.push(all[i])
            }
            return out
        }
        function filteredResultItems() {
            if (!appController.hasResult)
                return []
            if (resultFilter === qsTr("全部"))
                return appController.resultItems
            var filtered = []
            for (var i = 0; i < appController.resultItems.length; ++i) {
                var item = appController.resultItems[i]
                if (resultFilter === qsTr("未测/跳过")) {
                    if (item.state !== qsTr("通过") && item.state !== qsTr("警告")
                            && item.state !== qsTr("未通过"))
                        filtered.push(item)
                } else if (item.state === resultFilter) {
                    filtered.push(item)
                }
            }
            return filtered
        }
        x: window.width < 1400 ? 190 : 210
        y: 0
        width: window.width - x
        height: window.height - window.header.height
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.NoAutoClose
        background: Rectangle { color: "#f4f7fa" }
        onOpened: {
            settingsPopup.close()
            configurationPopup.close()
            reportCenterPopup.close()
            specialtyPopup.close()
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                height: 70
                color: "white"
                border.color: window.line
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 14
                    AppButton {
                        text: qsTr("‹  返回工作台")
                        implicitWidth: 118
                        implicitHeight: 36
                        onClicked: resultsPopup.close()
                        contentItem: Text {
                            text: parent.text
                            color: window.tealDark
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealPale : "white"
                            border.color: "#86cec8"
                        }
                    }
                    Column {
                        spacing: 2
                        Text {
                            text: qsTr("检查结果")
                            color: window.ink
                            font.pixelSize: 21
                            font.weight: Font.Bold
                        }
                        Text {
                            text: appController.hasResult
                                  ? qsTr("最近一次预检：%1").arg(appController.resultTimestamp)
                                  : qsTr("尚未执行一键预检")
                            color: window.secondaryInk
                            font.pixelSize: 12
                        }
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("报告中心")
                        implicitWidth: 118
                        implicitHeight: 36
                        enabled: appController.hasResult
                        onClicked: reportCenterPopup.open()
                        contentItem: Text {
                            text: parent.text
                            color: window.tealDark
                            opacity: parent.enabled ? 1 : 0.5
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealPale : "white"
                            border.color: "#78cfc7"
                        }
                    }
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: resultContent.implicitHeight + 42
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }

                ColumnLayout {
                    id: resultContent
                    width: parent.width - 32
                    x: 16
                    y: 14
                    spacing: 10

                    Rectangle {
                        visible: !appController.hasResult
                        Layout.fillWidth: true
                        Layout.preferredHeight: 360
                        radius: 10
                        color: "white"
                        border.color: window.line
                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(460, parent.width - 60)
                            spacing: 14
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 70
                                height: 70
                                radius: 35
                                color: window.tealPale
                                Text {
                                    anchors.centerIn: parent
                                    text: "✓"
                                    color: window.teal
                                    font.pixelSize: 32
                                    font.weight: Font.Bold
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("还没有预检结果")
                                color: window.ink
                                font.pixelSize: 22
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                Layout.fillWidth: true
                                text: appController.pendingCount === 0 && appController.modelCount > 0
                                      ? qsTr("所有型号均已完成编译，可以立即执行一键预检。")
                                      : qsTr("请先完成型号配置和编译，再执行一键预检。")
                                color: window.secondaryInk
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                            AppButton {
                                Layout.alignment: Qt.AlignHCenter
                                implicitWidth: 170
                                implicitHeight: 44
                                text: appController.pendingCount === 0 && appController.modelCount > 0
                                      ? qsTr("▶  开始一键预检") : qsTr("前往型号配置")
                                onClicked: {
                                    if (appController.pendingCount === 0 && appController.modelCount > 0)
                                        appController.runPrecheck()
                                    else {
                                        resultsPopup.close()
                                        configurationPopup.open()
                                    }
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: parent.pressed ? window.tealDark : window.teal
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: appController.hasResult
                        Layout.fillWidth: true
                        Layout.preferredHeight: 88
                        radius: 10
                        color: appController.resultOverallPass ? window.greenPale : "#fff8eb"
                        border.color: appController.resultOverallPass ? "#bee7d2" : "#f0d79f"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 14
                            Rectangle {
                                width: 44
                                height: 44
                                radius: 22
                                color: appController.resultOverallPass ? window.green : window.amber
                                Text {
                                    anchors.centerIn: parent
                                    text: appController.resultOverallPass ? "✓" : "!"
                                    color: "white"
                                    font.pixelSize: 21
                                    font.weight: Font.Bold
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    text: appController.resultOverallPass
                                          ? qsTr("预检通过，可以进入集成流程")
                                          : qsTr("预检完成，存在需要处理的项目")
                                    color: window.ink
                                    font.pixelSize: 17
                                    font.weight: Font.Bold
                                }
                                Text {
                                    text: qsTr("共检查 %1 项；通过 %2 项，警告 %3 项，未通过 %4 项。")
                                          .arg(appController.resultItems.length)
                                          .arg(appController.resultPassCount)
                                          .arg(appController.resultWarnCount)
                                          .arg(appController.resultFailCount)
                                    color: window.secondaryInk
                                    font.pixelSize: 11
                                }
                            }
                            AppButton {
                                text: qsTr("再次预检")
                                implicitWidth: 110
                                implicitHeight: 38
                                enabled: appController.pendingCount === 0
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                onClicked: appController.runPrecheck()
                                contentItem: Text {
                                    text: parent.text
                                    color: window.tealDark
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: parent.hovered ? window.tealPale : "white"
                                    border.color: "#78cfc7"
                                }
                            }
                        }
                    }

                    RowLayout {
                        visible: appController.hasResult
                        Layout.fillWidth: true
                        spacing: 12
                        Repeater {
                            model: [
                                { label: qsTr("通过"), count: appController.resultPassCount, color: window.green, pale: window.greenPale },
                                { label: qsTr("警告"), count: appController.resultWarnCount, color: window.amber, pale: window.amberPale },
                                { label: qsTr("未通过"), count: appController.resultFailCount, color: window.red, pale: "#fff0f1" },
                                { label: qsTr("未测/跳过"), count: appController.resultPendingCount, color: "#78879b", pale: "#f0f3f6" }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 58
                                radius: 8
                                color: modelData.pale
                                border.color: Qt.lighter(modelData.color, 1.55)
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    Text {
                                        text: modelData.count
                                        color: modelData.color
                                        font.pixelSize: 22
                                        font.weight: Font.Bold
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: modelData.label
                                        color: window.secondaryInk
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        visible: appController.hasResult
                        text: qsTr("检查项目")
                        color: window.ink
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    RowLayout {
                        visible: appController.hasResult
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        spacing: 7
                        Repeater {
                            model: [qsTr("全部"), qsTr("未通过"), qsTr("警告"), qsTr("通过"), qsTr("未测/跳过")]
                            delegate: AppButton {
                                text: modelData
                                implicitWidth: Math.max(66, compactFilterLabel.implicitWidth + 24)
                                implicitHeight: 30
                                onClicked: resultsPopup.resultFilter = modelData
                                contentItem: Text {
                                    id: compactFilterLabel
                                    text: parent.text
                                    color: resultsPopup.resultFilter === parent.text ? "white" : window.secondaryInk
                                    font.pixelSize: 10
                                    font.weight: resultsPopup.resultFilter === parent.text ? Font.DemiBold : Font.Normal
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 15
                                    color: resultsPopup.resultFilter === parent.text ? window.teal : "white"
                                    border.color: resultsPopup.resultFilter === parent.text ? window.teal : window.line
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Repeater {
                        model: resultsPopup.filteredResultItems()
                        delegate: Rectangle {
                            id: compactResultItemCard
                            readonly property bool expanded: resultsPopup.expandedResultName === modelData.name
                            readonly property color stateColor: modelData.state === "通过" ? window.green
                                                               : (modelData.state === "警告" ? window.amber
                                                                  : (modelData.state === "未通过" ? window.red : "#78879b"))
                            Layout.fillWidth: true
                            Layout.preferredHeight: compactResultCardColumn.implicitHeight + 20
                            radius: 8
                            color: "white"
                            border.color: compactResultItemCard.expanded ? window.teal : window.line

                            ColumnLayout {
                                id: compactResultCardColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 10
                                spacing: 8

                                RowLayout {
                                    id: compactResultItemHeader
                                    Layout.fillWidth: true
                                    spacing: 10
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 4
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8
                                            Text {
                                                text: compactResultItemCard.expanded ? "▾" : "▸"
                                                color: compactResultItemCard.expanded
                                                       ? window.teal : window.secondaryInk
                                                font.pixelSize: 11
                                                Layout.alignment: Qt.AlignVCenter
                                            }
                                            Rectangle {
                                                width: 10
                                                height: 10
                                                radius: 5
                                                Layout.alignment: Qt.AlignVCenter
                                                color: compactResultItemCard.stateColor
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: window.ink
                                                font.pixelSize: 14
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                            }
                                        }
                                        Text {
                                            Layout.leftMargin: 29
                                            Layout.fillWidth: true
                                            text: modelData.reason.length > 0 ? modelData.reason : qsTr("暂无详细说明")
                                            color: window.secondaryInk
                                            font.pixelSize: 10
                                            wrapMode: Text.WordWrap
                                        }
                                        Text {
                                            visible: modelData.state !== "通过"
                                            Layout.leftMargin: 29
                                            Layout.fillWidth: true
                                            text: qsTr("建议：") + modelData.suggestion
                                            color: window.tealDark
                                            font.pixelSize: 9
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                    Rectangle {
                                        width: compactResultState.implicitWidth + 30
                                        height: 36
                                        radius: 18
                                        color: Qt.lighter(compactResultItemCard.stateColor, 1.85)
                                        border.color: compactResultItemCard.stateColor
                                        Layout.alignment: Qt.AlignVCenter
                                        Text {
                                            id: compactResultState
                                            anchors.centerIn: parent
                                            text: modelData.state
                                            color: compactResultItemCard.stateColor
                                            font.pixelSize: 13
                                            font.weight: Font.Bold
                                        }
                                    }
                                }

                                Loader {
                                    id: compactModelDetailLoader
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: active && item ? item.implicitHeight : 0
                                    active: compactResultItemCard.expanded
                                    visible: active
                                    sourceComponent: resultsPopup.isReportItem(modelData.name)
                                                     ? reportDetailSection : modelDetailSection
                                }
                            }

                            MouseArea {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: compactResultItemHeader.height + 12
                                cursorShape: Qt.PointingHandCursor
                                onClicked: resultsPopup.expandedResultName =
                                    compactResultItemCard.expanded ? "" : modelData.name
                            }
                        }
                    }

                    // 型号级详情：作为可复用组件，由「检查项目」条目展开时实例化。
                    Component {
                        id: modelDetailSection
                        ColumnLayout {
                            spacing: 8
                    Repeater {
                        model: appController.resultModels
                        delegate: Rectangle {
                            id: modelResultCard
                            property bool expanded: true
                            property string resultModelName: modelData.name
                            // 当前展开条目：只显示与该条目相关的数据
                            readonly property string itemName: resultsPopup.expandedResultName
                            readonly property bool headerItem: itemName.indexOf(qsTr("头文件规范")) >= 0
                            readonly property bool libItem: itemName.indexOf(qsTr("LIB")) >= 0
                            readonly property bool dllDepItem: itemName.indexOf(qsTr("DLL 文件")) >= 0
                            readonly property bool dllIfaceItem: itemName.indexOf(qsTr("DLL 接口")) >= 0
                            readonly property bool buildItem: itemName.indexOf(qsTr("Release")) >= 0
                            readonly property bool perfItem: itemName.indexOf(qsTr("性能")) >= 0
                            readonly property bool memoryItem: itemName.indexOf(qsTr("内存")) >= 0
                            readonly property bool trajectoryItem: itemName.indexOf(qsTr("轨迹")) >= 0
                            readonly property bool staticItem: headerItem || libItem
                                                               || dllDepItem || dllIfaceItem || buildItem
                            // 头文件 / LIB 条目下逐文件列出明细
                            readonly property var fileList: headerItem ? (modelData.headers || [])
                                                                      : (libItem ? (modelData.libraries || []) : [])
                            // 按 Release / Debug 分组
                            function filesWith(config) {
                                var out = []
                                for (var i = 0; i < fileList.length; ++i) {
                                    if (String(fileList[i].configuration || "Release") === config)
                                        out.push(fileList[i])
                                }
                                return out
                            }
                            function dllsWith(config) {
                                var all = modelData.dlls || []
                                var out = []
                                for (var i = 0; i < all.length; ++i) {
                                    if (String(all[i].configuration || "Release") === config)
                                        out.push(all[i])
                                }
                                return out
                            }
                            // 同一构建配置只在首行显示分组标题（Release / Debug）
                            function dllGroupLabel(idx) {
                                var all = modelData.dlls || []
                                if (idx < 0 || idx >= all.length)
                                    return ""
                                var config = String(all[idx].configuration || "Release")
                                if (idx === 0 || String(all[idx - 1].configuration || "Release") !== config)
                                    return config
                                return ""
                            }
                            Layout.fillWidth: true
                            Layout.preferredHeight: modelResultColumn.implicitHeight + 16
                            radius: 9
                            color: "white"
                            border.color: window.line
                            ColumnLayout {
                                id: modelResultColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Rectangle {
                                        width: 30
                                        height: 30
                                        radius: 15
                                        color: modelData.overallPass ? window.greenPale : window.amberPale
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.overallPass ? "✓" : "!"
                                            color: modelData.overallPass ? window.green : window.amber
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: modelData.name
                                            color: window.ink
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.packageDir
                                            color: window.secondaryInk
                                            font.pixelSize: 9
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                    AppButton {
                                        text: qsTr("静态明细")
                                        implicitWidth: 78
                                        implicitHeight: 30
                                        visible: modelResultCard.headerItem || modelResultCard.libItem
                                                 || modelResultCard.buildItem
                                        enabled: (modelData.headers && modelData.headers.length > 0)
                                                 || (modelData.libraries && modelData.libraries.length > 0)
                                        onClicked: {
                                            staticModelDetailPopup.detail = modelData
                                            staticModelDetailPopup.open()
                                        }
                                    }
                                    AppButton {
                                        text: modelResultCard.expanded ? qsTr("收起") : qsTr("展开详情")
                                        implicitWidth: 82
                                        implicitHeight: 30
                                        onClicked: modelResultCard.expanded = !modelResultCard.expanded
                                    }
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    visible: modelResultCard.headerItem || modelResultCard.libItem
                                             || modelResultCard.dllDepItem || modelResultCard.dllIfaceItem
                                    spacing: 7
                                    Repeater {
                                        model: modelResultCard.headerItem
                                               ? [{ label: qsTr("头文件"), passed: modelData.passedHeaderCount, total: modelData.headerCount }]
                                               : (modelResultCard.libItem
                                                  ? [{ label: "LIB", passed: modelData.passedLibCount, total: modelData.libCount }]
                                                  : [{ label: "DLL", passed: modelData.passedDllCount, total: modelData.dllCount }])
                                        delegate: Rectangle {
                                            width: resultCountText.implicitWidth + 20
                                            height: 26
                                            radius: 13
                                            color: modelData.passed === modelData.total ? window.greenPale : window.amberPale
                                            Text {
                                                id: resultCountText
                                                anchors.centerIn: parent
                                                text: modelData.label + "  " + modelData.passed + "/" + modelData.total
                                                color: modelData.passed === modelData.total ? window.green : window.amber
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                    }
                                }

                                // 包内文件明细行（头文件 / LIB 共用）
                                Component {
                                    id: packageFileRow
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 34
                                        radius: 5
                                        color: "#f8fafc"
                                        border.color: window.line
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10
                                            spacing: 8
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: window.ink
                                                font.pixelSize: 11
                                                elide: Text.ElideMiddle
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.messages && modelData.messages.length > 0)
                                                      ? modelData.messages.join("；") : ""
                                                color: window.secondaryInk
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                            Rectangle {
                                                visible: modelData.configuration !== undefined
                                                implicitWidth: fileConfigText.implicitWidth + 14
                                                implicitHeight: 20
                                                radius: 10
                                                color: String(modelData.configuration) === "Debug"
                                                       ? window.amberPale : window.tealPale
                                                Text {
                                                    id: fileConfigText
                                                    anchors.centerIn: parent
                                                    text: String(modelData.configuration)
                                                    color: String(modelData.configuration) === "Debug"
                                                           ? window.amber : window.tealDark
                                                    font.pixelSize: 9
                                                    font.weight: Font.DemiBold
                                                }
                                            }
                                            Rectangle {
                                                implicitWidth: fileStateText.implicitWidth + 16
                                                implicitHeight: 20
                                                radius: 10
                                                color: modelData.pass ? window.greenPale : window.redPale
                                                Text {
                                                    id: fileStateText
                                                    anchors.centerIn: parent
                                                    text: modelData.pass ? qsTr("通过") : qsTr("未通过")
                                                    color: modelData.pass ? window.green : window.red
                                                    font.pixelSize: 9
                                                    font.weight: Font.DemiBold
                                                }
                                            }
                                        }
                                    }
                                }

                                // 头文件条目：逐头文件明细
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: modelResultCard.expanded && modelResultCard.headerItem
                                             && modelResultCard.fileList.length > 0
                                    spacing: 4
                                    Repeater {
                                        model: modelResultCard.fileList
                                        delegate: packageFileRow
                                    }
                                }

                                // LIB 条目：Release / Debug 分开列出
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: modelResultCard.expanded && modelResultCard.libItem
                                             && modelResultCard.fileList.length > 0
                                    spacing: 4
                                    Text {
                                        visible: modelResultCard.filesWith("Release").length > 0
                                        text: qsTr("Release（%1）")
                                              .arg(modelResultCard.filesWith("Release").length)
                                        color: window.ink
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                    }
                                    Repeater {
                                        model: modelResultCard.filesWith("Release")
                                        delegate: packageFileRow
                                    }
                                    Text {
                                        visible: modelResultCard.filesWith("Debug").length > 0
                                        text: qsTr("Debug（%1）")
                                              .arg(modelResultCard.filesWith("Debug").length)
                                        color: window.ink
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                    }
                                    Repeater {
                                        model: modelResultCard.filesWith("Debug")
                                        delegate: packageFileRow
                                    }
                                }

                                GridLayout {
                                    visible: modelResultCard.expanded && modelResultCard.buildItem
                                    Layout.fillWidth: true
                                    columns: resultsPopup.width < 1180 ? 1 : 2
                                    rowSpacing: 10
                                    columnSpacing: 10
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: 46
                                        radius: 6
                                        color: modelData.releaseBuildOk ? window.greenPale : window.amberPale
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 2
                                            Text {
                                                text: qsTr("Release · ") + (modelData.releaseBuildOk ? qsTr("可集成") : qsTr("需处理"))
                                                color: modelData.releaseBuildOk ? window.green : window.amber
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.releaseSummary
                                                color: window.secondaryInk
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: 46
                                        radius: 6
                                        color: modelData.debugBuildOk ? window.greenPale : window.amberPale
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 2
                                            Text {
                                                text: qsTr("Debug · ") + (modelData.debugBuildOk ? qsTr("可编译") : qsTr("不可用"))
                                                color: modelData.debugBuildOk ? window.green : window.amber
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.debugSummary
                                                color: window.secondaryInk
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    visible: modelResultCard.expanded
                                             && (modelResultCard.dllDepItem
                                                 || modelResultCard.dllIfaceItem
                                                 || modelResultCard.perfItem
                                                 || modelResultCard.memoryItem
                                                 || modelResultCard.trajectoryItem)
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        visible: modelData.dlls.length > 0
                                        text: qsTr("DLL 检查明细")
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }
                                    Repeater {
                                        model: modelData.dlls
                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: dllGroupTitle.visible ? 86 : 66
                                            radius: 5
                                            color: "#f8fafc"
                                            border.color: window.line
                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10
                                                anchors.topMargin: 6
                                                anchors.bottomMargin: 6
                                                spacing: 3
                                                // Release / Debug 分组标题
                                                Text {
                                                    id: dllGroupTitle
                                                    Layout.fillWidth: true
                                                    visible: text.length > 0
                                                    text: modelResultCard.dllGroupLabel(index)
                                                    color: text === "Debug" ? window.amber : window.tealDark
                                                    font.pixelSize: 10
                                                    font.weight: Font.DemiBold
                                                }
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.name
                                                        color: window.ink
                                                        font.pixelSize: 11
                                                        font.weight: Font.DemiBold
                                                        elide: Text.ElideMiddle
                                                    }
                                                    Text {
                                                        visible: modelResultCard.dllIfaceItem
                                                                 || modelResultCard.perfItem
                                                                 || modelResultCard.memoryItem
                                                                 || modelResultCard.trajectoryItem
                                                        text: modelData.configuration + " · " + modelData.architecture
                                                        color: window.secondaryInk
                                                        font.pixelSize: 9
                                                    }
                                                    Rectangle {
                                                        visible: modelResultCard.dllIfaceItem
                                                        implicitWidth: dllPeState.implicitWidth + 16
                                                        implicitHeight: 22
                                                        radius: 11
                                                        color: modelData.pePass ? window.greenPale : window.redPale
                                                        Text {
                                                            id: dllPeState
                                                            anchors.centerIn: parent
                                                            text: modelData.pePass ? qsTr("PE通过") : qsTr("PE失败")
                                                            color: modelData.pePass ? window.green : window.red
                                                            font.pixelSize: 9
                                                            font.weight: Font.DemiBold
                                                        }
                                                    }
                                                    Rectangle {
                                                        visible: modelResultCard.dllIfaceItem
                                                        implicitWidth: dllLoadState.implicitWidth + 16
                                                        implicitHeight: 22
                                                        radius: 11
                                                        color: modelData.configuration === "Debug" || modelData.loaded
                                                               ? window.greenPale : window.redPale
                                                        Text {
                                                            id: dllLoadState
                                                            anchors.centerIn: parent
                                                            text: modelData.configuration === "Debug"
                                                                  ? qsTr("仅静态检查")
                                                                  : (modelData.loaded ? qsTr("加载成功") : qsTr("加载失败"))
                                                            color: modelData.configuration === "Debug" || modelData.loaded
                                                                   ? window.green : window.red
                                                            font.pixelSize: 9
                                                            font.weight: Font.DemiBold
                                                        }
                                                    }
                                                }
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.crt
                                                        color: window.secondaryInk
                                                        font.pixelSize: 9
                                                        elide: Text.ElideRight
                                                    }
                                                    AppButton {
                                                        visible: modelResultCard.dllDepItem
                                                                 || modelResultCard.dllIfaceItem
                                                        text: qsTr("检查详情")
                                                        implicitWidth: 76
                                                        implicitHeight: 27
                                                        onClicked: {
                                                            dllDetailPopup.modelName = modelResultCard.resultModelName
                                                            dllDetailPopup.detail = modelData
                                                            dllDetailPopup.open()
                                                        }
                                                    }
                                                    AppButton {
                                                        visible: modelResultCard.perfItem
                                                                 || modelResultCard.memoryItem
                                                                 || modelResultCard.trajectoryItem
                                                        text: modelResultCard.trajectoryItem
                                                              ? qsTr("轨迹详情")
                                                              : (modelResultCard.memoryItem
                                                                 ? qsTr("内存详情") : qsTr("性能详情"))
                                                        implicitWidth: 82
                                                        implicitHeight: 27
                                                        enabled: (modelData.perfSamples
                                                                  && modelData.perfSamples.length > 0)
                                                                 || (modelData.trajectory
                                                                     && modelData.trajectory.length > 0)
                                                        onClicked: {
                                                            performanceDetailPopup.modelName = modelResultCard.resultModelName
                                                            performanceDetailPopup.detail = modelData
                                                            performanceDetailPopup.mode = modelResultCard.trajectoryItem
                                                                                         ? "trajectory"
                                                                                         : (modelResultCard.memoryItem
                                                                                            ? "memory" : "performance")
                                                            performanceDetailPopup.open()
                                                        }
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text {
                                                        visible: modelResultCard.trajectoryItem
                                                        text: qsTr("轨迹点 %1").arg(modelData.trajectory
                                                                                   ? modelData.trajectory.length : 0)
                                                        color: window.secondaryInk
                                                        font.pixelSize: 9
                                                    }
                                                    Text {
                                                        visible: modelResultCard.perfItem || modelResultCard.memoryItem
                                                        text: qsTr("性能采样 %1").arg(modelData.perfSamples
                                                                                    ? modelData.perfSamples.length : 0)
                                                        color: window.secondaryInk
                                                        font.pixelSize: 9
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                        }
                    }

                    // 报告类条目（冲突 / 并发 / 多对象）的详情，由对应条目展开时实例化。
                    Component {
                        id: reportDetailSection
                        ColumnLayout {
                            spacing: 8

                    Text {
                        visible: resultsPopup.showsHeaderConflict(resultsPopup.expandedResultName)
                                 && appController.resultHeaderConflicts.issues !== undefined
                        text: qsTr("头文件冲突分析")
                        color: window.ink
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    Rectangle {
                        visible: appController.resultHeaderConflicts.issues !== undefined
                        Layout.fillWidth: true
                        Layout.preferredHeight: 88
                        radius: 8
                        color: "white"
                        border.color: window.line
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10
                            Rectangle {
                                width: 36
                                height: 36
                                radius: 18
                                color: appController.resultHeaderConflicts.overallPass
                                       ? window.greenPale : window.redPale
                                Text {
                                    anchors.centerIn: parent
                                    text: appController.resultHeaderConflicts.overallPass ? "✓" : "!"
                                    color: appController.resultHeaderConflicts.overallPass ? window.green : window.red
                                    font.pixelSize: 17
                                    font.weight: Font.Bold
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: appController.resultHeaderConflicts.overallPass
                                          ? qsTr("未发现跨型号头文件冲突")
                                          : qsTr("发现需要处理的头文件冲突")
                                    color: window.ink
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 7
                                    Repeater {
                                        model: [
                                            { label: qsTr("类型重名"), count: Number(appController.resultHeaderConflicts.duplicateTypeCount || 0) },
                                            { label: qsTr("ODR 冲突"), count: Number(appController.resultHeaderConflicts.odrConflictCount || 0) },
                                            { label: qsTr("命名空间风险"), count: Number(appController.resultHeaderConflicts.namespacePollutionCount || 0) }
                                        ]
                                        delegate: Rectangle {
                                            width: headerConflictCount.implicitWidth + 18
                                            height: 25
                                            radius: 12
                                            color: modelData.count === 0 ? window.greenPale : window.amberPale
                                            Text {
                                                id: headerConflictCount
                                                anchors.centerIn: parent
                                                text: modelData.label + "  " + modelData.count
                                                color: modelData.count === 0 ? window.green : window.amber
                                                font.pixelSize: 9
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                    }
                                }
                            }
                            AppButton {
                                text: qsTr("查看冲突")
                                implicitWidth: 88
                                implicitHeight: 32
                                enabled: appController.resultHeaderConflicts.issues
                                         && appController.resultHeaderConflicts.issues.length > 0
                                onClicked: headerConflictPopup.open()
                            }
                        }
                    }

                    Text {
                        visible: resultsPopup.showsConcurrency(resultsPopup.expandedResultName)
                                 && appController.resultConcurrency.length > 0
                        text: qsTr("并发与稳定性")
                        color: window.ink
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    GridLayout {
                        visible: resultsPopup.showsConcurrency(resultsPopup.expandedResultName)
                                 && appController.resultConcurrency.length > 0
                        Layout.fillWidth: true
                        columns: resultsPopup.width < 1180 ? 1 : 2
                        rowSpacing: 10
                        columnSpacing: 10
                        Repeater {
                            model: resultsPopup.concurrencyReportsFor(resultsPopup.expandedResultName)
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                radius: 8
                                color: "white"
                                border.color: window.line
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.title
                                            color: window.ink
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                        }
                                        Rectangle {
                                            implicitWidth: concurrencyVerdict.implicitWidth + 18
                                            implicitHeight: 24
                                            radius: 12
                                            color: modelData.verdict === "PASS" ? window.greenPale
                                                   : (modelData.verdict === "FAIL" ? window.redPale : window.amberPale)
                                            Text {
                                                id: concurrencyVerdict
                                                anchors.centerIn: parent
                                                text: modelData.verdict.length > 0 ? modelData.verdict : qsTr("未执行")
                                                color: modelData.verdict === "PASS" ? window.green
                                                       : (modelData.verdict === "FAIL" ? window.red : window.amber)
                                                font.pixelSize: 9
                                                font.weight: Font.Bold
                                            }
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.description
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.summary.length > 0 ? modelData.summary : qsTr("本次未生成执行结果")
                                        color: window.ink
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("成功 %1 · 异常 %2 · 返回失败 %3")
                                                  .arg(modelData.successCount)
                                                  .arg(modelData.exceptionCount)
                                                  .arg(modelData.userFailCount)
                                            color: window.secondaryInk
                                            font.pixelSize: 9
                                        }
                                        AppButton {
                                            text: qsTr("线程明细")
                                            implicitWidth: 78
                                            implicitHeight: 28
                                            enabled: modelData.workers && modelData.workers.length > 0
                                            onClicked: {
                                                concurrencyDetailPopup.detail = modelData
                                                concurrencyDetailPopup.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        visible: resultsPopup.showsMultiObject(resultsPopup.expandedResultName)
                                 && appController.resultMultiObject.length > 0
                        text: qsTr("单线程多对象")
                        color: window.ink
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    GridLayout {
                        visible: resultsPopup.showsMultiObject(resultsPopup.expandedResultName)
                                 && appController.resultMultiObject.length > 0
                        Layout.fillWidth: true
                        columns: resultsPopup.width < 1180 ? 1 : 2
                        rowSpacing: 10
                        columnSpacing: 10
                        Repeater {
                            model: appController.resultMultiObject
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 116
                                radius: 8
                                color: "white"
                                border.color: window.line
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.scope === "fleet"
                                                  ? modelData.title : qsTr("型号：") + modelData.title
                                            color: window.ink
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Rectangle {
                                            implicitWidth: multiObjectVerdict.implicitWidth + 18
                                            implicitHeight: 24
                                            radius: 12
                                            color: modelData.verdict === "PASS" ? window.greenPale
                                                   : (modelData.verdict === "FAIL" ? window.redPale : window.amberPale)
                                            Text {
                                                id: multiObjectVerdict
                                                anchors.centerIn: parent
                                                text: modelData.verdict.length > 0 ? modelData.verdict : qsTr("未执行")
                                                color: modelData.verdict === "PASS" ? window.green
                                                       : (modelData.verdict === "FAIL" ? window.red : window.amber)
                                                font.pixelSize: 9
                                                font.weight: Font.Bold
                                            }
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.summary.length > 0 ? modelData.summary
                                              : (!modelData.configured ? qsTr("尚未配置多对象代码")
                                                 : qsTr("尚未执行多对象测试"))
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("型号 %1 · 对象 %2 · 完成 %3 · 干扰 %4 · 异常 %5")
                                              .arg(modelData.modelCount)
                                              .arg(modelData.objectCount)
                                              .arg(modelData.completedObjects)
                                              .arg(modelData.interferenceCount)
                                              .arg(modelData.exceptionCount)
                                        color: window.secondaryInk
                                        font.pixelSize: 9
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("最大偏差 %1 · 最大帧 %2 ms · 内存 %3 MB")
                                                  .arg(NumberFormat.toCompact(modelData.maxDeviation || 0, 15))
                                                  .arg(Number(modelData.maxFrameMs || 0).toFixed(3))
                                                  .arg(Number(modelData.memoryDeltaMB || 0).toFixed(2))
                                            color: window.secondaryInk
                                            font.pixelSize: 9
                                            elide: Text.ElideRight
                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.NoButton
                                                ToolTip.visible: containsMouse
                                                // 完整数值（普通小数）在悬停时显示
                                                ToolTip.text: qsTr("最大偏差 %1")
                                                    .arg(NumberFormat.toPlain(modelData.maxDeviation || 0))
                                            }
                                        }
                                        AppButton {
                                            text: qsTr("对象明细")
                                            implicitWidth: 78
                                            implicitHeight: 28
                                            enabled: modelData.objects && modelData.objects.length > 0
                                            onClicked: {
                                                precheckMultiObjectDetailPopup.detail = modelData
                                                precheckMultiObjectDetailPopup.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                        }
                    }

                    Text {
                        visible: false
                        text: qsTr("检查项目")
                        color: window.ink
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    RowLayout {
                        visible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0
                        spacing: 8
                        Repeater {
                            model: [qsTr("全部"), qsTr("未通过"), qsTr("警告"), qsTr("通过"), qsTr("未测/跳过")]
                            delegate: AppButton {
                                text: modelData
                                implicitWidth: Math.max(70, filterLabel.implicitWidth + 26)
                                implicitHeight: 32
                                onClicked: resultsPopup.resultFilter = modelData
                                contentItem: Text {
                                    id: filterLabel
                                    text: parent.text
                                    color: resultsPopup.resultFilter === parent.text ? "white" : window.secondaryInk
                                    font.pixelSize: 11
                                    font.weight: resultsPopup.resultFilter === parent.text ? Font.DemiBold : Font.Normal
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 16
                                    color: resultsPopup.resultFilter === parent.text ? window.teal : "white"
                                    border.color: resultsPopup.resultFilter === parent.text ? window.teal : window.line
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Repeater {
                        model: []
                        delegate: Rectangle {
                            id: resultItemCard
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(110, resultItemColumn.implicitHeight + 20)
                            radius: 8
                            color: "white"
                            border.color: window.line
                            readonly property color stateColor: modelData.state === "通过" ? window.green
                                                               : (modelData.state === "警告" ? window.amber
                                                                  : (modelData.state === "未通过" ? window.red
                                                                     : "#78879b"))
                            ColumnLayout {
                                id: resultItemColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                anchors.topMargin: 10
                                anchors.bottomMargin: 10
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Rectangle {
                                        width: 10
                                        height: 10
                                        radius: 5
                                        color: resultItemCard.stateColor
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: window.ink
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.WordWrap
                                    }
                                    Rectangle {
                                        width: 72
                                        height: 28
                                        radius: 14
                                        color: Qt.lighter(resultItemCard.stateColor, 1.85)
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.state
                                            color: resultItemCard.stateColor
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }
                                Text {
                                    Layout.leftMargin: 20
                                    Layout.fillWidth: true
                                    text: modelData.category + qsTr(" · 优先级：") + modelData.priority
                                    color: window.secondaryInk
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                                ColumnLayout {
                                    id: resultDetailColumn
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 20
                                    spacing: 5
                                    Text {
                                        id: resultReason
                                        Layout.fillWidth: true
                                        text: modelData.reason.length > 0 ? modelData.reason : qsTr("暂无详细说明")
                                        color: window.secondaryInk
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }
                                    Text {
                                        visible: modelData.consequence.length > 0 && modelData.state !== "通过"
                                        Layout.fillWidth: true
                                        text: qsTr("影响：") + modelData.consequence
                                        color: modelData.state === "未通过" ? window.red : window.amber
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }
                                    Text {
                                        visible: modelData.state !== "通过"
                                        Layout.fillWidth: true
                                        text: qsTr("建议：") + modelData.suggestion
                                        color: window.tealDark
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: staticModelDetailPopup
        property var detail: ({ "headers": [], "libraries": [] })
        width: Math.min(window.width - 100, 1080)
        height: Math.min(window.height - window.header.height - 70, 690)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0
            clip: true
            PopupHeader {
                title: qsTr("模型包静态明细 · %1").arg(staticModelDetailPopup.detail.name || "")
                subtitle: staticModelDetailPopup.detail.packageDir || ""
                onCloseRequested: staticModelDetailPopup.close()
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 62
                Layout.margins: 14
                radius: 7
                color: staticModelDetailPopup.detail.consistencyPass ? window.greenPale : window.amberPale
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 12
                    Text {
                        text: qsTr("头文件 / DLL 接口一致性")
                        color: window.ink
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: Number(staticModelDetailPopup.detail.consistencyRatio || 0).toFixed(1) + "%"
                        color: staticModelDetailPopup.detail.consistencyPass ? window.green : window.amber
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: qsTr("仅声明未导出 %1 · 仅导出未声明 %2")
                              .arg((staticModelDetailPopup.detail.declaredOnly || []).length)
                              .arg((staticModelDetailPopup.detail.exportedOnly || []).length)
                        color: window.secondaryInk
                        font.pixelSize: 10
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.bottomMargin: 14
                spacing: 12
                Repeater {
                    model: [
                        { title: qsTr("头文件规范"), kind: "header" },
                        { title: qsTr("LIB 库检查"), kind: "library" }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        color: "white"
                        border.color: window.line
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 7
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: window.ink
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: qsTr("共 %1 项").arg(modelData.kind === "header"
                                          ? (staticModelDetailPopup.detail.headers || []).length
                                          : (staticModelDetailPopup.detail.libraries || []).length)
                                    color: window.secondaryInk
                                    font.pixelSize: 9
                                }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: window.line }
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                Column {
                                    width: parent.width
                                    spacing: 5
                                    Repeater {
                                        model: modelData.kind === "header"
                                               ? (staticModelDetailPopup.detail.headers || [])
                                               : (staticModelDetailPopup.detail.libraries || [])
                                        delegate: Rectangle {
                                            width: parent.width
                                            height: 70
                                            radius: 6
                                            color: modelData.pass ? "#f8fafc" : window.redPale
                                            border.color: modelData.pass ? window.line : "#efb6ba"
                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                spacing: 4
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.name
                                                        color: window.ink
                                                        font.pixelSize: 10
                                                        font.weight: Font.DemiBold
                                                        elide: Text.ElideMiddle
                                                        ToolTip.visible: staticFileMouse.containsMouse
                                                        ToolTip.text: modelData.path
                                                        MouseArea { id: staticFileMouse; anchors.fill: parent; hoverEnabled: true }
                                                    }
                                                    Text {
                                                        text: modelData.pass ? qsTr("通过") : qsTr("需处理")
                                                        color: modelData.pass ? window.green : window.red
                                                        font.pixelSize: 9
                                                        font.weight: Font.DemiBold
                                                    }
                                                }
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.encoding !== undefined
                                                          ? qsTr("编码 %1 · extern C %2 · 导出宏 %3 · 函数 %4")
                                                            .arg(modelData.encoding)
                                                            .arg(modelData.externC ? qsTr("有") : qsTr("无"))
                                                            .arg(modelData.declspec ? qsTr("有") : qsTr("无"))
                                                            .arg((modelData.functions || []).length)
                                                          : qsTr("%1 · %2 · 已识别符号 %3 · 缺失 %4")
                                                            .arg(modelData.architecture)
                                                            .arg(modelData.type)
                                                            .arg((modelData.foundSymbols || []).length)
                                                            .arg((modelData.missingSymbols || []).length)
                                                    color: window.secondaryInk
                                                    font.pixelSize: 9
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    visible: modelData.encoding === undefined
                                                             && (modelData.missingSymbols || []).length > 0
                                                    Layout.fillWidth: true
                                                    text: qsTr("缺失：") + (modelData.missingSymbols || []).join("、")
                                                    color: window.red
                                                    font.pixelSize: 9
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                    }
                                    Text {
                                        visible: (modelData.kind === "header"
                                                  ? (staticModelDetailPopup.detail.headers || []).length
                                                  : (staticModelDetailPopup.detail.libraries || []).length) === 0
                                        width: parent.width
                                        height: 70
                                        text: modelData.kind === "header" ? qsTr("未发现头文件") : qsTr("未发现 LIB 文件")
                                        color: window.secondaryInk
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: dllDetailPopup
        property string modelName: ""
        property var detail: ({ "dependencies": [], "exportedSymbols": [] })
        width: Math.min(window.width - 100, 1080)
        height: Math.min(window.height - window.header.height - 70, 690)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: qsTr("DLL 检查详情 · %1").arg(dllDetailPopup.modelName)
                subtitle: (dllDetailPopup.detail.name || "") + " · "
                          + (dllDetailPopup.detail.configuration || "")
                badge: Component {
                    Rectangle {
                        implicitWidth: dllDetailState.implicitWidth + 22
                        implicitHeight: 28
                        radius: 14
                        color: dllDetailPopup.detail.pePass ? window.greenPale : window.redPale
                        Text {
                            id: dllDetailState
                            anchors.centerIn: parent
                            text: dllDetailPopup.detail.pePass ? qsTr("静态检查通过") : qsTr("需要处理")
                            color: dllDetailPopup.detail.pePass ? window.green : window.red
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }
                onCloseRequested: dllDetailPopup.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                Layout.minimumHeight: 64
                Layout.maximumHeight: 64
                Layout.margins: 14
                spacing: 9
                Repeater {
                    model: [
                        { label: qsTr("目标架构"), value: dllDetailPopup.detail.architecture || qsTr("未知"), ok: dllDetailPopup.detail.architecture === "x64" },
                        { label: qsTr("运行库"), value: dllDetailPopup.detail.crt || qsTr("未知"), ok: true },
                        { label: qsTr("缺失依赖"), value: Number(dllDetailPopup.detail.missingDependencies || 0) + qsTr(" 项"), ok: Number(dllDetailPopup.detail.missingDependencies || 0) === 0 },
                        { label: qsTr("接口绑定"), value: Number(dllDetailPopup.detail.boundSymbols || 0) + qsTr(" 个"), ok: Number(dllDetailPopup.detail.missingSymbols || 0) === 0 }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: "white"
                        border.color: modelData.ok ? window.line : window.red
                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 20
                            spacing: 2
                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.value
                                color: modelData.ok ? "#14213d" : window.red
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                                MouseArea {
                                    id: dllMetricHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                                ToolTip.visible: dllMetricHover.containsMouse
                                ToolTip.text: modelData.label + "：" + modelData.value
                            }
                            Text {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.label
                                color: "#7a8a9a"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 7
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("依赖库"); color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: qsTr("共 %1 项").arg((dllDetailPopup.detail.dependencies || []).length)
                                color: window.secondaryInk
                                font.pixelSize: 9
                            }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: window.line }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            Column {
                                width: parent.width
                                spacing: 4
                                Repeater {
                                    model: dllDetailPopup.detail.dependencies || []
                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 42
                                        radius: 5
                                        color: modelData.found ? "#f8fafc" : window.redPale
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 9
                                            anchors.rightMargin: 9
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: window.ink
                                                font.pixelSize: 10
                                                elide: Text.ElideMiddle
                                                ToolTip.visible: dependencyMouse.containsMouse && modelData.path.length > 0
                                                ToolTip.text: modelData.path
                                                MouseArea { id: dependencyMouse; anchors.fill: parent; hoverEnabled: true }
                                            }
                                            Text {
                                                text: modelData.found ? qsTr("已找到") : qsTr("缺失")
                                                color: modelData.found ? window.green : window.red
                                                font.pixelSize: 9
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                    }
                                }
                                Text {
                                    visible: (dllDetailPopup.detail.dependencies || []).length === 0
                                    width: parent.width
                                    height: 50
                                    text: qsTr("未发现导入依赖")
                                    color: window.secondaryInk
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "white"
                    border.color: window.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 7
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("导出接口"); color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: qsTr("共 %1 项").arg((dllDetailPopup.detail.exportedSymbols || []).length)
                                color: window.secondaryInk
                                font.pixelSize: 9
                            }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: window.line }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            Column {
                                width: parent.width
                                spacing: 4
                                Repeater {
                                    model: dllDetailPopup.detail.exportedSymbols || []
                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 36
                                        radius: 5
                                        color: modelData.required ? window.tealPale : "#f8fafc"
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 9
                                            anchors.rightMargin: 9
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: window.ink
                                                font.pixelSize: 10
                                                elide: Text.ElideMiddle
                                            }
                                            Text {
                                                visible: modelData.required
                                                text: qsTr("必要接口")
                                                color: window.tealDark
                                                font.pixelSize: 9
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                text: "#" + modelData.ordinal
                                                color: window.secondaryInk
                                                font.pixelSize: 9
                                            }
                                        }
                                    }
                                }
                                Text {
                                    visible: (dllDetailPopup.detail.exportedSymbols || []).length === 0
                                    width: parent.width
                                    height: 50
                                    text: qsTr("未解析到导出接口")
                                    color: window.secondaryInk
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: dllLoadError.visible ? 66 : 42
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.topMargin: 10
                Layout.bottomMargin: 14
                radius: 7
                color: dllDetailPopup.detail.loaded || dllDetailPopup.detail.configuration === "Debug"
                       ? "white" : window.redPale
                border.color: window.line
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 2
                    Text {
                        text: dllDetailPopup.detail.configuration === "Debug"
                              ? qsTr("Debug DLL 按安全策略仅做静态检查")
                              : (dllDetailPopup.detail.loaded
                                 ? qsTr("动态加载成功 · %1").arg(dllDetailPopup.detail.apiStyle || qsTr("接口类型未知"))
                                 : qsTr("动态加载失败"))
                        color: dllDetailPopup.detail.loaded || dllDetailPopup.detail.configuration === "Debug"
                               ? window.ink : window.red
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                    Text {
                        id: dllLoadError
                        visible: (dllDetailPopup.detail.loadError || "").length > 0
                        Layout.fillWidth: true
                        text: dllDetailPopup.detail.loadError || ""
                        color: window.secondaryInk
                        font.pixelSize: 9
                        elide: Text.ElideRight
                        ToolTip.visible: loadErrorMouse.containsMouse && visible
                        ToolTip.text: text
                        MouseArea { id: loadErrorMouse; anchors.fill: parent; hoverEnabled: true }
                    }
                }
            }
        }
    }

    Popup {
        id: staticCheckPopup
        property string section: "header"
        property var report: appController.specialtyStaticReport
        width: Math.min(window.width - 100, 1080)
        height: Math.min(window.height - window.header.height - 70, 690)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }
        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: qsTr("模型包检查明细 · %1").arg(staticCheckPopup.report.modelName || "")
                subtitle: staticCheckPopup.report.packageDir || ""
                badge: Component {
                    Rectangle {
                        implicitWidth: staticVerdict.implicitWidth + 24
                        implicitHeight: 28
                        radius: 14
                        color: staticCheckPopup.report.overallPass ? window.green : window.amber
                        Text {
                            id: staticVerdict
                            anchors.centerIn: parent
                            text: staticCheckPopup.report.overallPass ? qsTr("✓ 通过") : qsTr("! 需处理")
                            color: "white"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }
                onCloseRequested: staticCheckPopup.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                spacing: 8
                Repeater {
                    model: [
                        { key: "header", text: qsTr("头文件 %1/%2").arg(staticCheckPopup.report.headerPassed || 0).arg((staticCheckPopup.report.headers || []).length) },
                        { key: "lib", text: qsTr("LIB %1/%2").arg(staticCheckPopup.report.libraryPassed || 0).arg((staticCheckPopup.report.libraries || []).length) },
                        { key: "dll", text: qsTr("DLL 与加载 %1/%2").arg(staticCheckPopup.report.dllPassed || 0).arg((staticCheckPopup.report.dlls || []).length) }
                    ]
                    delegate: AppButton {
                        text: modelData.text
                        implicitWidth: 140
                        implicitHeight: 32
                        onClicked: staticCheckPopup.section = modelData.key
                        background: Rectangle {
                            radius: 16
                            color: staticCheckPopup.section === modelData.key ? window.teal : "white"
                            border.color: staticCheckPopup.section === modelData.key ? window.teal : window.line
                        }
                        contentItem: Text {
                            text: parent.text
                            color: staticCheckPopup.section === modelData.key ? "white" : window.secondaryInk
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    visible: staticCheckPopup.section === "header"
                    text: qsTr("冲突：类型 %1 · ODR %2 · 命名空间 %3")
                        .arg((staticCheckPopup.report.conflicts || {}).duplicateTypes || 0)
                        .arg((staticCheckPopup.report.conflicts || {}).odrConflicts || 0)
                        .arg((staticCheckPopup.report.conflicts || {}).namespaceRisks || 0)
                    color: window.secondaryInk
                    font.pixelSize: 9
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.bottomMargin: 14
                clip: true
                Column {
                    width: parent.width
                    spacing: 7
                    Repeater {
                        model: staticCheckPopup.section === "header" ? (staticCheckPopup.report.headers || [])
                               : (staticCheckPopup.section === "lib" ? (staticCheckPopup.report.libraries || [])
                                  : (staticCheckPopup.report.dlls || []))
                        delegate: Rectangle {
                            width: parent.width
                            height: staticCheckPopup.section === "dll" ? 118 : 92
                            radius: 7
                            color: "white"
                            border.color: modelData.pass ? window.line : "#edc08a"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 5
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: window.ink
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        visible: staticCheckPopup.section === "dll"
                                        text: (modelData.configuration || "") + " · " + (modelData.architecture || "")
                                        color: window.secondaryInk
                                        font.pixelSize: 9
                                    }
                                    Rectangle {
                                        width: itemState.implicitWidth + 18
                                        height: 24
                                        radius: 12
                                        color: modelData.pass ? window.greenPale : window.amberPale
                                        Text {
                                            id: itemState
                                            anchors.centerIn: parent
                                            text: modelData.pass ? qsTr("通过") : qsTr("需处理")
                                            color: modelData.pass ? window.green : window.amber
                                            font.pixelSize: 9
                                            font.weight: Font.Bold
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: staticCheckPopup.section === "header"
                                          ? qsTr("编码 %1 · extern C %2 · 导出声明 %3 · pack %4 · 函数 %5")
                                            .arg(modelData.encoding || "N/A")
                                            .arg(modelData.externC ? qsTr("有") : qsTr("无"))
                                            .arg(modelData.declspec ? qsTr("有") : qsTr("无"))
                                            .arg(modelData.pack ? qsTr("有") : qsTr("无"))
                                            .arg((modelData.functions || []).length)
                                          : (staticCheckPopup.section === "lib"
                                             ? qsTr("%1 · %2 · 已发现符号 %3 · 缺失 %4")
                                               .arg(modelData.architecture || "N/A")
                                               .arg(modelData.type || "")
                                               .arg((modelData.symbols || []).length)
                                               .arg((modelData.missing || []).length)
                                             : qsTr("PE %1 · %2 · 缺失依赖 %3 · 缺失导出 %4")
                                               .arg(modelData.pePass ? qsTr("通过") : qsTr("失败"))
                                               .arg(modelData.crt || "N/A")
                                               .arg(modelData.missingDependencies || 0)
                                               .arg(modelData.missingExports || 0))
                                    color: window.secondaryInk
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: staticCheckPopup.section === "dll"
                                    Layout.fillWidth: true
                                    text: qsTr("加载：%1 · 已绑定 %2 · 缺失 %3%4")
                                        .arg(modelData.loadState || "N/A")
                                        .arg(modelData.boundSymbols || 0)
                                        .arg(modelData.missingSymbols || 0)
                                        .arg((modelData.loadError || "").length > 0 ? qsTr(" · ") + modelData.loadError : "")
                                    color: modelData.loadPass ? window.secondaryInk : window.red
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.path || ""
                                    color: "#8491a2"
                                    font.pixelSize: 8
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        height: 90
                        visible: (staticCheckPopup.section === "header" ? (staticCheckPopup.report.headers || [])
                                  : (staticCheckPopup.section === "lib" ? (staticCheckPopup.report.libraries || [])
                                     : (staticCheckPopup.report.dlls || []))).length === 0
                        text: qsTr("此分类没有可检查的文件")
                        color: window.secondaryInk
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Popup {
        id: performanceDetailPopup
        property string modelName: ""
        property string mode: "combined"
        property var detail: ({ "perfSamples": [], "trajectory": [] })
        property int paintRevision: 0
        // 兼容历史会话中保存的旧文案（角度 / 弧度）
        readonly property string trajectoryUnitLabel: {
            var value = String(detail.trajectoryUnit || "")
            if (value === "角度" || value === "弧度")
                return value === "弧度" ? qsTr("疑似弧度") : qsTr("经纬度（度）")
            return value.length > 0 ? value : qsTr("未知")
        }
        width: Math.min(window.width - 100, 1080)
        height: Math.min(window.height - window.header.height - 70, 690)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: (performanceDetailPopup.mode === "performance"
                        ? qsTr("UserMain 耗时 · %1")
                        : (performanceDetailPopup.mode === "memory"
                           ? qsTr("内存增长 · %1")
                           : (performanceDetailPopup.mode === "trajectory"
                              ? qsTr("运行轨迹 · %1")
                              : qsTr("性能 · 内存 · 轨迹 · %1"))))
                       .arg(performanceDetailPopup.modelName)
                subtitle: performanceDetailPopup.detail.name + " · "
                          + performanceDetailPopup.detail.configuration
                badge: Component {
                    Rectangle {
                        implicitWidth: perfVerdictText.implicitWidth + 22
                        implicitHeight: 28
                        radius: 14
                        color: performanceDetailPopup.detail.perfVerdict === "PASS"
                               ? window.green : window.amber
                        Text {
                            id: perfVerdictText
                            anchors.centerIn: parent
                            text: performanceDetailPopup.detail.perfVerdict.length > 0
                                  ? (performanceDetailPopup.detail.perfVerdict === "PASS"
                                     ? "✓  PASS" : performanceDetailPopup.detail.perfVerdict)
                                  : qsTr("未执行性能测试")
                            color: "white"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                    }
                }
                badgeVisible: performanceDetailPopup.mode !== "trajectory"
                onCloseRequested: performanceDetailPopup.close()
            }

            RowLayout {
                visible: performanceDetailPopup.mode !== "trajectory"
                Layout.fillWidth: true
                Layout.minimumHeight: visible ? 70 : 0
                Layout.preferredHeight: visible ? 70 : 0
                Layout.maximumHeight: visible ? 70 : 0
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 10
                Layout.bottomMargin: 8
                spacing: 9
                Repeater {
                    model: performanceDetailPopup.mode === "memory"
                           ? [
                               { label: qsTr("初始内存"), value: Number(performanceDetailPopup.detail.initialMemoryMB || 0).toFixed(2) + " MB" },
                               { label: qsTr("最终内存"), value: Number(performanceDetailPopup.detail.finalMemoryMB || 0).toFixed(2) + " MB" },
                               { label: qsTr("内存变化"), value: Number(performanceDetailPopup.detail.memoryDeltaMB || 0).toFixed(2) + " MB" },
                               { label: qsTr("每万次增长"), value: Number(performanceDetailPopup.detail.memoryLeakRate || 0).toFixed(2) + " MB" },
                               { label: qsTr("采样次数"), value: Number((performanceDetailPopup.detail.perfSamples || []).length) }
                             ]
                           : [
                               { label: qsTr("平均耗时"), value: Number(performanceDetailPopup.detail.perfAverageMs || 0).toFixed(4) + " ms" },
                               { label: qsTr("最大耗时"), value: Number(performanceDetailPopup.detail.perfMaximumMs || 0).toFixed(4) + " ms" },
                               { label: qsTr("耗时抖动"), value: Number(performanceDetailPopup.detail.perfJitterMs || 0).toFixed(4) + " ms" },
                               { label: qsTr("内存变化"), value: Number(performanceDetailPopup.detail.memoryDeltaMB || 0).toFixed(2) + " MB" },
                               { label: qsTr("每万次增长"), value: Number(performanceDetailPopup.detail.memoryLeakRate || 0).toFixed(2) + " MB" }
                             ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: "white"
                        border.color: window.line
                        Column {
                            anchors.centerIn: parent
                            spacing: 3
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                color: window.ink
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: window.secondaryInk
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 250
                Layout.preferredHeight: 290
                Layout.maximumHeight: 320
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 8
                spacing: 12
                Repeater {
                    model: performanceDetailPopup.mode === "performance"
                           ? [{ title: qsTr("UserMain 单次耗时"), type: "perf" }]
                           : (performanceDetailPopup.mode === "memory"
                              ? [{ title: qsTr("内存增长"), type: "memory" }]
                              : (performanceDetailPopup.mode === "trajectory"
                                 ? [{ title: qsTr("运行轨迹（经度 / 纬度）"), type: "trajectory" }]
                                 : [
                                     { title: qsTr("UserMain 单次耗时"), type: "perf" },
                                     { title: qsTr("内存增长"), type: "memory" },
                                     { title: qsTr("运行轨迹（经度 / 纬度）"), type: "trajectory" }
                                   ]))
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        radius: 8
                        color: "white"
                        border.color: window.line
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 7
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: modelData.title; color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: modelData.type === "perf"
                                          ? qsTr("横轴：执行次数  纵轴：毫秒")
                                          : (modelData.type === "memory"
                                             ? qsTr("横轴：执行次数  纵轴：内存 MB")
                                             : qsTr("横轴：经度  纵轴：纬度"))
                                    color: "#33415c"
                                    font.pixelSize: 10
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 8

                            ChartView {
                                id: detailChartView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 240
                                antialiasing: true
                                legend.visible: false
                                backgroundColor: "#fbfcfd"
                                plotAreaColor: "#fbfcfd"
                                backgroundRoundness: 4
                                // 缩小图表边距，让坐标轴线更长、刻度间距更舒展。
                                margins { top: 10; bottom: 4; left: 6; right: 14 }
                                property string chartType: modelData.type
                                property var chartDetail: performanceDetailPopup.detail
                                property int popupPaintRevision: performanceDetailPopup.paintRevision
                                function rebuildSeries() {
                                    detailLineSeries.clear()
                                    var sampled = chartType === "perf" || chartType === "memory"
                                    var raw = sampled ? (chartDetail.perfSamples || [])
                                                      : (chartDetail.trajectory || [])
                                    if (!raw || raw.length === 0)
                                        return
                                    var minX = Number.POSITIVE_INFINITY
                                    var maxX = Number.NEGATIVE_INFINITY
                                    var minY = Number.POSITIVE_INFINITY
                                    var maxY = Number.NEGATIVE_INFINITY
                                    for (var i = 0; i < raw.length; ++i) {
                                        var xValue = Number(sampled ? raw[i].step : raw[i].x)
                                        var yValue = Number(chartType === "memory" ? raw[i].memoryMB
                                                                                   : (sampled ? raw[i].timeMs : raw[i].y))
                                        if (!isFinite(xValue) || !isFinite(yValue))
                                            continue
                                        detailLineSeries.append(xValue, yValue)
                                        minX = Math.min(minX, xValue)
                                        maxX = Math.max(maxX, xValue)
                                        minY = Math.min(minY, yValue)
                                        maxY = Math.max(maxY, yValue)
                                    }
                                    if (detailLineSeries.count === 0)
                                        return
                                    var xRange = maxX - minX
                                    var yRange = maxY - minY
                                    if (xRange <= 0) xRange = Math.max(Math.abs(minX) * 0.05, 1.0)
                                    if (yRange <= 0) yRange = Math.max(Math.abs(minY) * 0.05, 0.001)
                                    detailAxisX.min = minX - xRange * 0.04
                                    detailAxisX.max = maxX + xRange * 0.04
                                    detailAxisY.min = minY - yRange * 0.08
                                    detailAxisY.max = maxY + yRange * 0.08
                                }
                                onChartDetailChanged: Qt.callLater(rebuildSeries)
                                onPopupPaintRevisionChanged: Qt.callLater(rebuildSeries)
                                onVisibleChanged: {
                                    if (visible) Qt.callLater(rebuildSeries)
                                }
                                Component.onCompleted: Qt.callLater(rebuildSeries)

                                ValueAxis {
                                    id: detailAxisX
                                    min: 0
                                    max: 1
                                    // 轨迹图用更多刻度、更高的精度，经纬度刻度间距更宽松。
                                    tickCount: detailChartView.chartType === "trajectory" ? 7 : 5
                                    labelFormat: detailChartView.chartType === "trajectory"
                                                 ? "%.5f" : "%.4g"
                                    labelsColor: "#33415c"
                                    labelsFont.pixelSize: 10
                                    color: "#93a4b8"
                                    gridLineColor: "#dde5ec"
                                    lineVisible: true
                                    titleBrush: window.ink
                                    titleFont.pixelSize: 11
                                    titleFont.bold: true
                                    titleText: detailChartView.chartType === "trajectory"
                                               ? qsTr("经度") : qsTr("执行次数")
                                }
                                ValueAxis {
                                    id: detailAxisY
                                    min: 0
                                    max: 1
                                    tickCount: detailChartView.chartType === "trajectory" ? 6 : 5
                                    labelFormat: detailChartView.chartType === "trajectory"
                                                 ? "%.5f" : "%.4g"
                                    labelsColor: "#33415c"
                                    labelsFont.pixelSize: 10
                                    color: "#93a4b8"
                                    gridLineColor: "#dde5ec"
                                    lineVisible: true
                                    titleBrush: window.ink
                                    titleFont.pixelSize: 11
                                    titleFont.bold: true
                                    titleText: detailChartView.chartType === "perf" ? qsTr("耗时（ms）")
                                               : (detailChartView.chartType === "memory"
                                                  ? qsTr("内存（MB）") : qsTr("纬度"))
                                }
                                LineSeries {
                                    id: detailLineSeries
                                    axisX: detailAxisX
                                    axisY: detailAxisY
                                    color: detailChartView.chartType === "perf" ? window.teal
                                           : (detailChartView.chartType === "memory" ? "#e39a16" : "#3b82f6")
                                    width: 2
                                    pointsVisible: true
                                }
                                Text {
                                    anchors.centerIn: parent
                                    visible: detailLineSeries.count === 0
                                    text: detailChartView.chartType === "perf" ? qsTr("没有性能采样数据")
                                          : (detailChartView.chartType === "memory"
                                             ? qsTr("没有内存采样数据") : qsTr("没有轨迹数据"))
                                    color: window.secondaryInk
                                    font.pixelSize: 11
                                }
                            }

                                // 轨迹图右侧：逐点经纬度数据表（合并视图下宽度不足则不显示）
                                TrajectoryTable {
                                    visible: modelData.type === "trajectory"
                                             && performanceDetailPopup.mode === "trajectory"
                                    Layout.preferredWidth: 252
                                    Layout.minimumWidth: 200
                                    Layout.fillHeight: true
                                    singleTrajectory: true
                                    series: [{ "objectId": 0, "baseline": false,
                                               "points": performanceDetailPopup.detail.trajectory || [] }]
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: performanceDetailPopup.mode === "trajectory"
                         || performanceDetailPopup.mode === "combined"
                Layout.fillWidth: true
                Layout.minimumHeight: visible ? ((performanceDetailPopup.detail.trajectoryMessages || []).length > 0 ? 152 : 132) : 0
                Layout.preferredHeight: visible ? ((performanceDetailPopup.detail.trajectoryMessages || []).length > 0 ? 152 : 132) : 0
                Layout.maximumHeight: visible ? ((performanceDetailPopup.detail.trajectoryMessages || []).length > 0 ? 152 : 132) : 0
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.topMargin: 6
                Layout.bottomMargin: 10
                radius: 8
                color: "white"
                border.color: window.line
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 6
                    Text {
                        text: qsTr("轨迹质量指标")
                        color: window.ink
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        spacing: 8
                        Repeater {
                            model: [
                                { label: qsTr("采样点"), value: Number(performanceDetailPopup.detail.trajectoryPoints || 0), ok: true },
                                { label: "NaN/Inf", value: Number(performanceDetailPopup.detail.trajectoryNanCount || 0), ok: Number(performanceDetailPopup.detail.trajectoryNanCount || 0) === 0 },
                                { label: qsTr("位置跳变"), value: Number(performanceDetailPopup.detail.trajectoryJumpCount || 0), ok: Number(performanceDetailPopup.detail.trajectoryJumpCount || 0) === 0 },
                                { label: qsTr("越界"), value: Number(performanceDetailPopup.detail.trajectoryOutOfBoundsCount || 0), ok: Number(performanceDetailPopup.detail.trajectoryOutOfBoundsCount || 0) === 0 },
                                { label: qsTr("坐标单位"), value: performanceDetailPopup.trajectoryUnitLabel, ok: performanceDetailPopup.trajectoryUnitLabel !== qsTr("疑似弧度") }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 6
                                color: modelData.ok ? "#f7faf9" : window.amberPale
                                border.color: modelData.ok ? "#e2ecea" : "#ead19a"
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 3
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.value
                                        color: modelData.ok ? "#14213d" : window.amber
                                        font.pixelSize: 17
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.label
                                        color: "#7a8a9a"
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("判定轨迹是否含无效数值、异常跳变、经纬度越界，或把弧度误当角度使用。")
                        color: window.secondaryInk
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: (performanceDetailPopup.detail.trajectoryMessages || []).length > 0
                        Layout.fillWidth: true
                        text: (performanceDetailPopup.detail.trajectoryMessages || []).join("；")
                        color: window.amber
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        ToolTip.visible: trajectoryMessageMouse.containsMouse && visible
                        ToolTip.text: text
                        MouseArea { id: trajectoryMessageMouse; anchors.fill: parent; hoverEnabled: true }
                    }
                }
            }
        }

        onOpened: paintRevision += 1
    }

    Popup {
        id: precheckMultiObjectDetailPopup
        property var detail: ({ "objects": [] })
        readonly property real maxDeviationValue: Number(detail.maxDeviation || 0)
        width: Math.min(window.width - 100, 1000)
        height: Math.min(window.height - window.header.height - 70, 660)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: qsTr("多对象执行明细 · %1").arg(precheckMultiObjectDetailPopup.detail.title || "")
                subtitle: precheckMultiObjectDetailPopup.detail.summary || ""
                onCloseRequested: precheckMultiObjectDetailPopup.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 78
                Layout.margins: 14
                spacing: 8
                Repeater {
                    model: [
                        { label: qsTr("对象数"), value: Number(precheckMultiObjectDetailPopup.detail.objectCount || 0), color: window.ink },
                        { label: qsTr("完成"), value: Number(precheckMultiObjectDetailPopup.detail.completedObjects || 0), color: window.green },
                        { label: qsTr("干扰"), value: Number(precheckMultiObjectDetailPopup.detail.interferenceCount || 0), color: Number(precheckMultiObjectDetailPopup.detail.interferenceCount || 0) === 0 ? window.green : window.red },
                        { label: qsTr("异常"), value: Number(precheckMultiObjectDetailPopup.detail.exceptionCount || 0), color: Number(precheckMultiObjectDetailPopup.detail.exceptionCount || 0) === 0 ? window.green : window.red },
                        // 偏差可能极小，卡片里放不下时简略显示，完整数值鼠标悬停可见。
                        { label: qsTr("最大偏差"), value: NumberFormat.toCompact(precheckMultiObjectDetailPopup.maxDeviationValue, 14), tooltip: NumberFormat.toPlain(precheckMultiObjectDetailPopup.maxDeviationValue), color: window.ink }
                    ]
                    delegate: Rectangle {
                        id: metricCard
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: "white"
                        border.color: window.line
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                id: metricValue
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(implicitWidth, metricCard.width - 16)
                                text: modelData.value
                                color: modelData.color
                                font.pixelSize: 15
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                    ToolTip.visible: containsMouse && modelData.tooltip !== undefined
                                    ToolTip.text: modelData.tooltip !== undefined ? modelData.tooltip : ""
                                }
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: window.secondaryInk
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                color: "transparent"
                RowLayout {
                    anchors.fill: parent
                    Text { Layout.preferredWidth: 170; text: qsTr("型号 / 对象"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.preferredWidth: 80; text: qsTr("基线返回"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.preferredWidth: 80; text: qsTr("交错返回"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.preferredWidth: 92; text: qsTr("最大偏差"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.preferredWidth: 76; text: qsTr("状态"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.fillWidth: true; text: qsTr("说明"); color: window.secondaryInk; font.pixelSize: 9 }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.bottomMargin: 14
                clip: true
                Column {
                    width: parent.width
                    spacing: 5
                    Repeater {
                        model: precheckMultiObjectDetailPopup.detail.objects || []
                        delegate: Rectangle {
                            width: parent.width
                            height: Math.max(54, objectDetailText.implicitHeight + 20)
                            radius: 6
                            color: modelData.exception || modelData.baselineReturn !== 0
                                   || modelData.interleavedReturn !== 0
                                   || Number(modelData.deviation) > Number(precheckMultiObjectDetailPopup.detail.tolerance || 0)
                                   ? window.redPale : "white"
                            border.color: window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 9
                                anchors.rightMargin: 9
                                spacing: 8
                                Text {
                                    Layout.preferredWidth: 162
                                    text: (modelData.modelName || qsTr("未命名")) + " / #" + modelData.objectId
                                    color: window.ink
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                }
                                Text { Layout.preferredWidth: 72; text: modelData.baselineReturn; color: modelData.baselineReturn === 0 ? window.green : window.red; font.pixelSize: 10 }
                                Text { Layout.preferredWidth: 72; text: modelData.interleavedReturn; color: modelData.interleavedReturn === 0 ? window.green : window.red; font.pixelSize: 10 }
                                Text {
                                    Layout.preferredWidth: 84
                                    text: NumberFormat.toCompact(modelData.deviation || 0, 12)
                                    color: window.secondaryInk
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.NoButton
                                        ToolTip.visible: containsMouse
                                        // 完整数值（普通小数）在悬停时显示
                                        ToolTip.text: NumberFormat.toPlain(modelData.deviation || 0)
                                    }
                                }
                                Text {
                                    Layout.preferredWidth: 68
                                    text: modelData.exception ? qsTr("SEH 异常")
                                          : (modelData.baselineReturn !== 0 || modelData.interleavedReturn !== 0 ? qsTr("失败") : qsTr("完成"))
                                    color: modelData.exception || modelData.baselineReturn !== 0 || modelData.interleavedReturn !== 0 ? window.red : window.green
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    id: objectDetailText
                                    Layout.fillWidth: true
                                    text: modelData.detail || (modelData.exception
                                          ? qsTr("异常码 %1，故障步 %2").arg(modelData.exceptionCode).arg(modelData.faultStep)
                                          : qsTr("基线与交错执行完成"))
                                    color: window.secondaryInk
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                    Text {
                        visible: (precheckMultiObjectDetailPopup.detail.objects || []).length === 0
                        width: parent.width
                        height: 90
                        text: qsTr("没有对象执行明细")
                        color: window.secondaryInk
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Popup {
        id: headerConflictPopup
        width: Math.min(window.width - 100, 1000)
        height: Math.min(window.height - window.header.height - 70, 660)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: qsTr("头文件冲突详情")
                subtitle: qsTr("同包及跨型号扫描结果；失败项可能导致编译错误、符号覆盖或运行时状态串扰")
                onCloseRequested: headerConflictPopup.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                Layout.margins: 14
                spacing: 9
                Repeater {
                    model: [
                        { label: qsTr("类型重名"), value: Number(appController.resultHeaderConflicts.duplicateTypeCount || 0) },
                        { label: qsTr("ODR 冲突"), value: Number(appController.resultHeaderConflicts.odrConflictCount || 0) },
                        { label: qsTr("命名空间风险"), value: Number(appController.resultHeaderConflicts.namespacePollutionCount || 0) }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: "white"
                        border.color: modelData.value === 0 ? window.line : window.amber
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                color: modelData.value === 0 ? window.green : window.amber
                                font.pixelSize: 17
                                font.weight: Font.Bold
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: window.secondaryInk
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.bottomMargin: 14
                clip: true
                Column {
                    width: parent.width
                    spacing: 7
                    Repeater {
                        model: appController.resultHeaderConflicts.issues || []
                        delegate: Rectangle {
                            width: parent.width
                            height: conflictIssueColumn.implicitHeight + 22
                            radius: 7
                            color: "white"
                            border.color: modelData.severity === "FAIL" ? "#efb6ba" : "#efd39a"
                            ColumnLayout {
                                id: conflictIssueColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 11
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    Rectangle {
                                        implicitWidth: conflictSeverity.implicitWidth + 16
                                        implicitHeight: 23
                                        radius: 11
                                        color: modelData.severity === "FAIL" ? window.redPale : window.amberPale
                                        Text {
                                            id: conflictSeverity
                                            anchors.centerIn: parent
                                            text: modelData.severity
                                            color: modelData.severity === "FAIL" ? window.red : window.amber
                                            font.pixelSize: 9
                                            font.weight: Font.Bold
                                        }
                                    }
                                    Text {
                                        text: modelData.category === "DUPLICATE_TYPE" ? qsTr("类型重名")
                                              : (modelData.category === "ODR_CONFLICT" ? qsTr("ODR 冲突")
                                                 : qsTr("命名空间污染"))
                                        color: window.secondaryInk
                                        font.pixelSize: 9
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.symbol
                                        color: window.ink
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideMiddle
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.detail
                                    color: window.secondaryInk
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("涉及文件：") + (modelData.files || []).join("\n")
                                    color: window.tealDark
                                    font.pixelSize: 9
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                    Text {
                        visible: !(appController.resultHeaderConflicts.issues
                                   && appController.resultHeaderConflicts.issues.length > 0)
                        width: parent.width
                        height: 100
                        text: qsTr("未发现头文件冲突或污染风险")
                        color: window.green
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Popup {
        id: concurrencyDetailPopup
        property var detail: ({ "workers": [] })
        width: Math.min(window.width - 100, 980)
        height: Math.min(window.height - window.header.height - 70, 660)
        x: (window.width - width) / 2
        y: 35
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { radius: 11; color: "#f4f7fa"; border.color: window.line }

        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: concurrencyDetailPopup.detail.title || qsTr("并发执行明细")
                subtitle: concurrencyDetailPopup.detail.summary || ""
                onCloseRequested: concurrencyDetailPopup.close()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 78
                Layout.margins: 14
                spacing: 8
                Repeater {
                    model: [
                        { label: qsTr("工作线程"), value: Number(concurrencyDetailPopup.detail.workerCount || 0), color: window.ink },
                        { label: qsTr("型号数"), value: Number(concurrencyDetailPopup.detail.modelTypeCount || 0), color: window.ink },
                        { label: qsTr("成功"), value: Number(concurrencyDetailPopup.detail.successCount || 0), color: window.green },
                        { label: qsTr("异常"), value: Number(concurrencyDetailPopup.detail.exceptionCount || 0), color: Number(concurrencyDetailPopup.detail.exceptionCount || 0) === 0 ? window.green : window.red },
                        { label: qsTr("返回失败"), value: Number(concurrencyDetailPopup.detail.userFailCount || 0), color: Number(concurrencyDetailPopup.detail.userFailCount || 0) === 0 ? window.green : window.red }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: "white"
                        border.color: window.line
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value
                                color: modelData.color
                                font.pixelSize: 16
                                font.weight: Font.Bold
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: window.secondaryInk
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                color: "transparent"
                RowLayout {
                    anchors.fill: parent
                    Text { Layout.preferredWidth: 56; text: qsTr("线程"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.preferredWidth: 160; text: qsTr("型号 / 实例"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.preferredWidth: 72; text: qsTr("返回码"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.preferredWidth: 80; text: qsTr("状态"); color: window.secondaryInk; font.pixelSize: 9 }
                    Text { Layout.fillWidth: true; text: qsTr("随机变量与错误信息"); color: window.secondaryInk; font.pixelSize: 9 }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.bottomMargin: 14
                clip: true
                Column {
                    width: parent.width
                    spacing: 5
                    Repeater {
                        model: concurrencyDetailPopup.detail.workers || []
                        delegate: Rectangle {
                            width: parent.width
                            height: Math.max(52, workerMessage.implicitHeight + 20)
                            radius: 6
                            color: modelData.exception || modelData.userFail || modelData.returnCode !== 0
                                   ? window.redPale : "white"
                            border.color: modelData.exception || modelData.userFail || modelData.returnCode !== 0
                                          ? "#efb6ba" : window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 9
                                anchors.rightMargin: 9
                                spacing: 8
                                Text { Layout.preferredWidth: 48; text: "#" + modelData.threadId; color: window.secondaryInk; font.pixelSize: 10 }
                                Text {
                                    Layout.preferredWidth: 152
                                    text: (modelData.modelName || qsTr("未命名")) + " / " + modelData.instanceId
                                    color: window.ink
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                }
                                Text { Layout.preferredWidth: 64; text: modelData.returnCode; color: modelData.returnCode === 0 ? window.green : window.red; font.pixelSize: 10 }
                                Text {
                                    Layout.preferredWidth: 72
                                    text: modelData.exception ? qsTr("SEH 异常")
                                          : (modelData.userFail || modelData.returnCode !== 0 ? qsTr("失败") : qsTr("成功"))
                                    color: modelData.exception || modelData.userFail || modelData.returnCode !== 0 ? window.red : window.green
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    id: workerMessage
                                    Layout.fillWidth: true
                                    text: {
                                        var parts = []
                                        if ((modelData.randomSummary || "").length > 0) parts.push(modelData.randomSummary)
                                        if ((modelData.error || "").length > 0) parts.push(modelData.error)
                                        return parts.length > 0 ? parts.join(" · ") : qsTr("执行完成")
                                    }
                                    color: window.secondaryInk
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                    Text {
                        visible: (concurrencyDetailPopup.detail.workers || []).length === 0
                        width: parent.width
                        height: 80
                        text: qsTr("没有线程执行明细")
                        color: window.secondaryInk
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Popup {
        id: reportCenterPopup
        x: window.width < 1400 ? 190 : 210
        y: 0
        width: window.width - x
        height: window.height - window.header.height
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.NoAutoClose
        background: Rectangle { color: "#f4f7fa" }

        contentItem: ColumnLayout {
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                height: 70
                color: "white"
                border.color: window.line
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 14
                    AppButton {
                        text: qsTr("‹  返回工作台")
                        implicitWidth: 118
                        implicitHeight: 36
                        onClicked: reportCenterPopup.close()
                        contentItem: Text {
                            text: parent.text
                            color: window.tealDark
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealPale : "white"
                            border.color: "#86cec8"
                        }
                    }
                    Column {
                        spacing: 2
                        Text { text: qsTr("报告中心"); color: window.ink; font.pixelSize: 21; font.weight: Font.Bold }
                        Text {
                            text: qsTr("保存并管理每次一键预检生成的完整 HTML 报告")
                            color: window.secondaryInk
                            font.pixelSize: 12
                        }
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("打开报告目录")
                        implicitWidth: 124
                        implicitHeight: 36
                        onClicked: appController.openReportsFolder()
                    }
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: reportHistoryColumn.implicitHeight + 48
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }

                ColumnLayout {
                    id: reportHistoryColumn
                    width: Math.min(parent.width - 48, 1040)
                    x: (parent.width - width) / 2
                    y: 24
                    spacing: 14

                    Rectangle {
                        Layout.fillWidth: true
                        height: 96
                        radius: 10
                        color: "white"
                        border.color: window.line
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            spacing: 18
                            Rectangle {
                                width: 48
                                height: 48
                                radius: 10
                                color: window.tealPale
                                Text { anchors.centerIn: parent; text: "▥"; color: window.teal; font.pixelSize: 24 }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    text: appController.reportHistoryCount > 0
                                          ? qsTr("已保存 %1 份预检报告").arg(appController.reportHistoryCount)
                                          : qsTr("还没有保存的预检报告")
                                    color: window.ink
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                }
                                Text {
                                    text: appController.reportHistoryCount > 0
                                          ? qsTr("每次一键预检完成后会自动归档，可随时打开或另存。")
                                          : qsTr("完成一次一键预检后，报告会自动出现在这里。")
                                    color: window.secondaryInk
                                    font.pixelSize: 11
                                }
                            }
                            AppButton {
                                text: qsTr("查看检查结果")
                                implicitWidth: 118
                                implicitHeight: 36
                                onClicked: resultsPopup.open()
                            }
                        }
                    }

                    Text {
                        visible: appController.reportHistoryCount > 0
                        text: qsTr("历史报告")
                        color: window.ink
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: appController.reportHistory
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 92
                            radius: 9
                            color: "white"
                            border.color: window.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 14
                                spacing: 14
                                Rectangle {
                                    width: 42
                                    height: 42
                                    radius: 21
                                    color: modelData.overallPass ? window.greenPale : window.amberPale
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.overallPass ? "✓" : "!"
                                        color: modelData.overallPass ? window.green : window.amber
                                        font.pixelSize: 19
                                        font.weight: Font.Bold
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        text: modelData.timestamp.length > 0
                                              ? modelData.timestamp : qsTr("预检报告")
                                        color: window.ink
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: qsTr("通过 %1 · 警告 %2 · 未通过 %3 · 未测/跳过 %4")
                                              .arg(modelData.passCount).arg(modelData.warnCount)
                                              .arg(modelData.failCount).arg(modelData.pendingCount)
                                        color: window.secondaryInk
                                        font.pixelSize: 11
                                    }
                                }
                                RowLayout {
                                    spacing: 10
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    Rectangle {
                                        Layout.preferredWidth: 62
                                        Layout.preferredHeight: 34
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 17
                                        color: modelData.overallPass ? window.greenPale : window.amberPale
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.overallPass ? qsTr("通过") : qsTr("需处理")
                                            color: modelData.overallPass ? window.green : window.amber
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                    AppButton {
                                        text: qsTr("打开")
                                        implicitWidth: 72
                                        implicitHeight: 34
                                        Layout.alignment: Qt.AlignVCenter
                                        onClicked: appController.openReportAt(index)
                                        contentItem: Text {
                                            text: parent.text
                                            color: window.tealDark
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            radius: 6
                                            color: parent.hovered ? window.tealPale : "white"
                                            border.color: "#75c8c1"
                                        }
                                    }
                                    AppButton {
                                        text: qsTr("另存为")
                                        implicitWidth: 78
                                        implicitHeight: 34
                                        Layout.alignment: Qt.AlignVCenter
                                        onClicked: appController.exportReportAt(index)
                                        contentItem: Text {
                                            text: parent.text
                                            color: window.tealDark
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            radius: 6
                                            color: parent.hovered ? window.tealPale : "white"
                                            border.color: "#75c8c1"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        onOpened: {
            settingsPopup.close()
            configurationPopup.close()
            resultsPopup.close()
            specialtyPopup.close()
        }
    }

    Popup {
        id: sessionRestorePopup
        width: Math.min(window.width - 80, 520)
        height: 300
        x: (window.width - width) / 2
        y: Math.max(30, (window.height - window.header.height - height) / 2)
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        background: Rectangle { radius: 11; color: "white"; border.color: window.line }
        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: qsTr("发现上次会话")
                onCloseRequested: {
                    appController.skipSessionRestore()
                    sessionRestorePopup.close()
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14
                Text {
                    Layout.fillWidth: true
                    text: qsTr("上次会话包含 %1 个型号，保存时间为 %2。是否继续上次工作？")
                        .arg(appController.restoreModelCount)
                        .arg(appController.restoreSavedAt)
                    color: window.ink
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    radius: 7
                    color: window.tealPale
                    border.color: "#9dd9d3"
                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        text: qsTr("还原后会恢复型号、模型包路径、UserMain、随机变量、编译状态和运行参数。")
                        color: window.tealDark
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("暂不还原")
                        implicitWidth: 108
                        implicitHeight: 36
                        onClicked: {
                            appController.skipSessionRestore()
                            sessionRestorePopup.close()
                        }
                        contentItem: Text {
                            text: parent.text
                            color: window.secondaryInk
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#f0f3f6" : "white"
                            border.color: window.line
                        }
                    }
                    AppButton {
                        text: qsTr("还原会话")
                        implicitWidth: 108
                        implicitHeight: 36
                        onClicked: {
                            appController.acceptSessionRestore()
                            sessionRestorePopup.close()
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? window.tealDark : window.teal
                            border.color: window.tealDark
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: removeModelDialog
        modal: true
        anchors.centerIn: parent
        title: qsTr("删除型号")
        standardButtons: Dialog.Yes | Dialog.No
        closePolicy: Popup.CloseOnEscape
        onAccepted: appController.removeSelectedModel()
        contentItem: Text {
            text: qsTr("确定删除型号“%1”吗？\n该操作不会删除模型包中的源文件。").arg(
                      appController.selectedModelName)
            color: window.ink
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            padding: 18
        }
    }

    Popup {
        id: logPopup
        x: window.width < 1400 ? 190 : 210
        y: window.height - window.header.height - height
        width: window.width - x
        height: Math.min(460, (window.height - window.header.height) * 0.58)
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 11
            color: "#f4f7fa"
            border.color: window.line
        }
        contentItem: ColumnLayout {
            spacing: 0
            PopupHeader {
                title: qsTr("编译与运行日志")
                subtitle: qsTr("记录编译、加载与各项测试的执行过程")
                badge: Component {
                    RowLayout {
                        spacing: 8
                        Text {
                            text: qsTr("%1 条").arg(appController.logCount)
                            color: window.secondaryInk
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignVCenter
                        }
                        AppButton {
                            text: qsTr("导出")
                            implicitWidth: 66
                            implicitHeight: 30
                            enabled: appController.logCount > 0
                            onClicked: appController.exportLogs()
                        }
                        AppButton {
                            text: qsTr("清空")
                            implicitWidth: 66
                            implicitHeight: 30
                            enabled: appController.logCount > 0 && !appController.precheckRunning
                            onClicked: appController.clearLogs()
                        }
                    }
                }
                onCloseRequested: logPopup.close()
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                radius: 8
                color: "white"
                border.color: window.line
                ListView {
                    id: logList
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    model: appController.logLines
                    spacing: 3
                    ScrollBar.vertical: ScrollBar { }
                    onCountChanged: if (count > 0) positionViewAtEnd()
                    delegate: Text {
                        // 预留滚动条宽度，避免每行末尾被滚动条遮挡导致显示不全。
                        width: logList.width - 26
                        text: modelData
                        color: window.logLineColor(modelData)
                        font.family: "Consolas"
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: appController.logCount === 0
                    text: qsTr("暂无运行日志")
                    color: window.secondaryInk
                    font.pixelSize: 13
                }
            }
        }
    }

    // 所有耗时操作（一键预检、代码编译、专项测试、多对象测试）共用同一个等候提示层。
    BusyOverlay {
        id: busyOverlay
        active: appController.precheckRunning || appController.compileBusy
                || appController.specialtyBusy || appController.multiObjectBusy
        title: appController.precheckRunning ? qsTr("正在执行一键预检")
               : (appController.compileBusy ? qsTr("正在编译对象代码")
                  : (appController.multiObjectBusy ? qsTr("正在执行多对象测试")
                     : qsTr("正在执行专项测试")))
        detail: appController.precheckRunning
                ? (appController.precheckProgress.length > 0
                   ? appController.precheckProgress : qsTr("正在准备检查环境…"))
                : (appController.compileBusy
                   ? (appController.configurationMessage.length > 0
                      ? appController.configurationMessage : qsTr("正在调用 MSVC 编译…"))
                   : (appController.multiObjectBusy
                      ? (appController.multiObjectMessage.length > 0
                         ? appController.multiObjectMessage : qsTr("正在准备多对象测试…"))
                      : (appController.specialtyMessage.length > 0
                         ? appController.specialtyMessage : qsTr("正在准备专项测试…"))))
        hint: appController.precheckRunning
              ? qsTr("预检期间请勿关闭程序")
              : qsTr("执行期间请勿关闭程序；若长时间无响应，可能是被测模型内部阻塞")
        logLine: appController.lastLogLine
        progress: appController.compileBusy || appController.precheckRunning
                  ? appController.busyProgress : -1
    }

    Connections {
        target: appController
        onModelDetailsChanged: {
            if (!codeEditor.activeFocus)
                codeEditor.text = appController.selectedUserMain
            if (!runtimeDtField.activeFocus)
                runtimeDtField.text = String(appController.selectedMultiObjectDt)
            runtimeStepsBox.value = appController.selectedMultiObjectSteps
        }
        onPrecheckFinished: {
            configurationPopup.close()
            if (appController.precheckError.length > 0)
                logPopup.open()
            else
                resultsPopup.open()
        }
        // 「编译成功后返回此页开始预检」：勾选后编译完成且全部型号就绪时自动回到首页，
        // 并且只生效一次——用完立即取消勾选，需要再控制时重新勾选。
        onCompileStateChanged: {
            if (appController.compileBusy || !returnAfterCompileBox.checked)
                return
            if (appController.pendingCount > 0)
                return
            returnAfterCompileBox.checked = false
            configurationPopup.close()
        }
    }

    Component.onCompleted: {
        if (appController.restorePending)
            Qt.callLater(function() { sessionRestorePopup.open() })
    }
}
