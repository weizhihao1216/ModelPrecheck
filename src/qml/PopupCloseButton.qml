import QtQuick 2.14
import QtQuick.Controls 2.14

Button {
    id: control
    implicitWidth: 28
    implicitHeight: 28
    hoverEnabled: true
    contentItem: Text {
        text: "×"
        color: control.pressed ? "#1f2d3d"
              : (control.hovered ? "#15243a" : "#5a6a7d")
        font.pixelSize: 16
        font.weight: Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 3
        color: control.pressed ? "#e3e9ef"
              : (control.hovered ? "#edf1f5" : "#f5f7f9")
        border.color: control.hovered ? "#d8e0e8" : "transparent"
    }
}
