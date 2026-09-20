import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "NumberFormat.js" as NumberFormat

// 判定容差输入：白底描边输入框 + 右侧一对可点击的上下箭头。
// 容差常跨若干数量级，所以箭头按 10 倍调整（▲ 放大、▼ 缩小）；也可以直接手动输入。
RowLayout {
    id: control

    property real value: 1e-8
    property real minimumValue: 1e-12
    property real maximumValue: 1

    spacing: 4

    function clampValue(raw) {
        var result = Number(raw)
        if (!isFinite(result)) return control.value
        if (result < control.minimumValue) return control.minimumValue
        if (result > control.maximumValue) return control.maximumValue
        return result
    }

    function applyValue(raw) {
        control.value = clampValue(raw)
        syncText()
    }

    // 按 10 倍调整：在十进制文本上移位，避免 double 乘除 10 带来的精度尾巴。
    function stepMagnitude(up) {
        var shifted = NumberFormat.shiftDecimal(NumberFormat.toPlain(control.value), up ? 1 : -1)
        applyValue(Number(shifted))
    }

    // 输入框不跟随 value 做声明式绑定：用户手输后会打断绑定，改由这里统一同步。
    function syncText() {
        valueField.text = NumberFormat.toPlain(control.value)
    }

    Component.onCompleted: syncText()

    AppTextField {
        id: valueField
        Layout.fillWidth: true
        onEditingFinished: {
            var text2 = text.trim()
            if (text2.length === 0) {
                valueField.text = NumberFormat.toPlain(control.value)
                return
            }
            control.applyValue(text2)
        }
    }

    ColumnLayout {
        spacing: 2
        AppButton {
            variant: "secondary"
            implicitWidth: 24
            implicitHeight: 15
            enabled: control.value < control.maximumValue
            onClicked: control.stepMagnitude(true)
            contentItem: Text {
                text: "▲"
                color: parent.enabled ? "#3f5a63" : "#a9b3bd"
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: !parent.enabled ? "#f2f4f6"
                                       : (parent.pressed ? "#d8eeeb"
                                                         : (parent.hovered ? "#e8f5f3" : "#f6f9fa"))
                border.color: parent.enabled ? "#cfe0e4" : "#e4e8ec"
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("容差放大 10 倍")
        }
        AppButton {
            variant: "secondary"
            implicitWidth: 24
            implicitHeight: 15
            enabled: control.value > control.minimumValue
            onClicked: control.stepMagnitude(false)
            contentItem: Text {
                text: "▼"
                color: parent.enabled ? "#3f5a63" : "#a9b3bd"
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4
                color: !parent.enabled ? "#f2f4f6"
                                       : (parent.pressed ? "#d8eeeb"
                                                         : (parent.hovered ? "#e8f5f3" : "#f6f9fa"))
                border.color: parent.enabled ? "#cfe0e4" : "#e4e8ec"
            }
            ToolTip.visible: hovered
            ToolTip.text: qsTr("容差缩小 10 倍")
        }
    }
}
