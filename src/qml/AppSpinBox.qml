import QtQuick 2.14
import QtQuick.Controls 2.14

// 全应用统一的数值微调框外观：白底描边 + 可见的加减按钮。
// 仅覆盖 background 与加减指示器，保留默认 contentItem（editable 文本输入正常工作）。
SpinBox {
    id: control

    // 左右留出加减按钮的空间，避免窄宽度时数字压在按钮上
    implicitWidth: 108
    leftPadding: 24
    rightPadding: 24
    up.indicator: Rectangle {
        x: control.mirrored ? 2 : parent.width - width - 2
        y: 2
        width: 22
        height: Math.max(18, parent.height - 4)
        radius: 5
        color: !control.enabled ? "#f2f4f6"
                                : (control.up.pressed ? "#d8eeeb"
                                                      : (control.up.hovered ? "#e8f5f3" : "#f6f9fa"))
        border.color: control.enabled ? "#cfe0e4" : "#e4e8ec"
        Text {
            anchors.centerIn: parent
            text: "+"
            color: !control.enabled ? "#a9b3bd" : (control.up.hovered ? "#0a7f76" : "#3f5a63")
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width - 2 : 2
        y: 2
        width: 22
        height: Math.max(18, parent.height - 4)
        radius: 5
        color: !control.enabled ? "#f2f4f6"
                                : (control.down.pressed ? "#d8eeeb"
                                                        : (control.down.hovered ? "#e8f5f3" : "#f6f9fa"))
        border.color: control.enabled ? "#cfe0e4" : "#e4e8ec"
        Text {
            anchors.centerIn: parent
            text: "−"
            color: !control.enabled ? "#a9b3bd" : (control.down.hovered ? "#0a7f76" : "#3f5a63")
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
    }

    background: Rectangle {
        implicitWidth: 140
        implicitHeight: 36
        radius: 6
        color: control.enabled ? "white" : "#f2f4f6"
        border.color: control.activeFocus ? "#6cc8c0" : "#cfe0e4"
    }
}
