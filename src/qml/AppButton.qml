import QtQuick 2.14
import QtQuick.Controls 2.14

// 全应用统一的按钮外观：
//   secondary（默认）：白底 + 青绿描边 + 青绿文字
//   primary / success / danger：实心填充 + 白字
// 实例若自行提供 contentItem / background，则按实例的写法渲染。
Button {
    id: control

    property string variant: "secondary"

    readonly property color accentColor: variant === "primary" ? "#12857c"
                                        : (variant === "success" ? "#1a9c5b"
                                           : (variant === "danger" ? "#c0392b" : "#0f5f58"))

    contentItem: Text {
        text: control.text
        color: !control.enabled ? "#9aa6b5"
                                : (control.variant === "secondary" ? control.accentColor : "white")
        font.pixelSize: 12
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 36
        radius: 6
        color: !control.enabled ? "#f2f4f6"
             : control.variant === "secondary"
               ? (control.pressed ? "#d8eeeb" : (control.hovered ? "#e8f5f3" : "white"))
               : (control.pressed ? Qt.darker(control.accentColor, 1.15)
                                  : (control.hovered ? Qt.lighter(control.accentColor, 1.08)
                                                     : control.accentColor))
        border.color: !control.enabled ? "#dfe4e8"
                    : (control.variant === "secondary"
                       ? (control.hovered ? "#6cc8c0" : "#b7dcd8") : "transparent")
    }
}
