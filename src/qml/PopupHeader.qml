import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

// 统一弹框标题栏：左侧标题+副标题，右侧可选徽章+关闭按钮
// 所有弹框必须使用本组件，保证关闭按钮位置与样式完全一致
Rectangle {
    id: header

    signal closeRequested()

    property string title: ""
    property string subtitle: ""
    property Component badge: null      // 可选：右侧状态徽章（如 PASS / 执行中）
    property bool badgeVisible: true    // 徽章是否显示
    property color lineColor: "#dce4ec"
    property color inkColor: "#14213d"
    property color subColor: "#60708a"

    Layout.fillWidth: true
    Layout.preferredHeight: 62
    Layout.minimumHeight: 62
    Layout.maximumHeight: 62
    radius: 11
    color: "white"
    border.color: header.lineColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 14
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: header.title
                color: header.inkColor
                font.pixelSize: 18
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            Text {
                visible: header.subtitle.length > 0
                Layout.fillWidth: true
                text: header.subtitle
                color: header.subColor
                font.pixelSize: 10
                elide: Text.ElideMiddle
            }
        }

        Loader {
            visible: active
            active: header.badge !== null && header.badgeVisible
            sourceComponent: header.badge
            Layout.alignment: Qt.AlignVCenter
        }

        PopupCloseButton {
            onClicked: header.closeRequested()
        }
    }
}
