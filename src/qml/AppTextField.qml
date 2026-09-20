import QtQuick 2.14
import QtQuick.Controls 2.14

// 全应用统一的单行输入框外观：白底描边，聚焦/悬浮时青绿描边，与 AppComboBox / AppSpinBox 保持一致。
// 仅覆盖 background 与文字配色，保留默认 contentItem（输入、选择、校验器都正常工作）。
TextField {
    id: control

    implicitHeight: 32
    leftPadding: 10
    rightPadding: 10
    selectByMouse: true
    color: control.enabled ? "#14213d" : "#9aa6b5"
    placeholderTextColor: "#8b98a8"
    selectionColor: "#e6f7f5"
    selectedTextColor: "#14213d"
    font.pixelSize: 11

    background: Rectangle {
        implicitHeight: 32
        radius: 6
        color: control.enabled ? "white" : "#f2f4f6"
        border.color: control.activeFocus ? "#6cc8c0"
                                       : (control.hovered ? "#a9d6d1" : "#cfe0e4")
    }
}
