import QtQuick 2.14
import QtQuick.Controls 2.14

// 全应用统一的下拉框外观：白底描边 + 展开列表（普通/高亮状态都清晰可读、行距紧凑）。
// 实例若自行提供 contentItem / indicator / delegate / popup，则按实例的写法渲染。
ComboBox {
    id: control

    implicitHeight: 32

    contentItem: Text {
        leftPadding: 10
        rightPadding: 26
        text: control.displayText
        color: control.enabled ? "#14213d" : "#9aa6b5"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 8
        y: (control.height - height) / 2
        text: "▾"
        color: control.enabled ? "#60708a" : "#a9b3bd"
        font.pixelSize: 12
    }

    background: Rectangle {
        implicitWidth: 150
        implicitHeight: 32
        radius: 6
        color: control.enabled ? "white" : "#f2f4f6"
        border.color: control.activeFocus ? "#6cc8c0"
                                         : (control.hovered ? "#a9d6d1" : "#cfe0e4")
    }

    delegate: ItemDelegate {
        id: option
        width: control.width
        height: 30
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            leftPadding: 10
            rightPadding: 10
            // 与默认风格一致：textRole 模型用 modelData[role]，字符串模型直接用 modelData。
            // 不能用 textAt()，它会实例化代理从而递归。
            text: control.textRole
                  ? (Array.isArray(control.model) ? modelData[control.textRole]
                                                  : model[control.textRole])
                  : modelData
            color: option.highlighted ? "#0f5f58" : "#14213d"
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 4
            color: option.highlighted ? "#e8f5f3" : "white"
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
        padding: 4
        background: Rectangle {
            radius: 6
            color: "white"
            border.color: "#cfe0e4"
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
