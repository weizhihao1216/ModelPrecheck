import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "NumberFormat.js" as NumberFormat

// 多对象测试的内嵌结果面板：结论 + 基线/交错坐标对照表（左右两张，逐行一一对应），无需弹出独立窗口。
Item {
    id: panel

    property var trajectorySeries: []
    property var resultItems: []
    property string verdict: ""
    property string summary: ""
    property string emptyHint: ""
    property double maxDeviation: 0.0
    property double maxFrameMs: 0.0
    property double memoryDeltaMB: 0.0

    // 按对象归并“基线 / 交错”两组轨迹点，供左右两张表逐行对照。
    property var objectEntries: buildEntries(trajectorySeries)
    property int currentObjectIndex: 0
    // 两张表的 ListView 引用（key -> ListView），用于同步滚动。
    property var pointLists: ({})

    function buildEntries(series) {
        var list = []
        var slots = ({})
        var all = series || []
        for (var i = 0; i < all.length; ++i) {
            var item = all[i]
            var objectId = Number(item.objectId)
            if (isNaN(objectId)) objectId = 0
            var slot = slots[objectId]
            if (slot === undefined) {
                slot = list.length
                slots[objectId] = slot
                var name = item.modelName !== undefined ? String(item.modelName) : ""
                list.push({
                    objectId: objectId,
                    label: name.length > 0 ? qsTr("#%1 %2").arg(objectId).arg(name)
                                           : qsTr("#%1").arg(objectId),
                    baseline: [],
                    interleaved: []
                })
            }
            var target = item.baseline ? list[slot].baseline : list[slot].interleaved
            var points = item.points || []
            for (var p = 0; p < points.length; ++p)
                target.push({ x: Number(points[p].x), y: Number(points[p].y) })
        }
        return list
    }

    readonly property var currentEntry: (objectEntries && currentObjectIndex >= 0
                                         && currentObjectIndex < objectEntries.length)
                                        ? objectEntries[currentObjectIndex] : null
    readonly property var baselinePoints: currentEntry ? currentEntry.baseline : []
    readonly property var interleavedPoints: currentEntry ? currentEntry.interleaved : []

    onTrajectorySeriesChanged: currentObjectIndex = 0
    onObjectEntriesChanged: if (currentObjectIndex >= objectEntries.length) currentObjectIndex = 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                width: 12
                height: 12
                radius: 6
                Layout.alignment: Qt.AlignVCenter
                color: panel.verdict === "PASS" ? "#1a9c5b" : "#e39a16"
            }
            Text {
                text: panel.verdict.length > 0
                      ? qsTr("结论 %1").arg(panel.verdict)
                      : qsTr("尚未执行")
                color: "#14213d"
                font.pixelSize: 18
                font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: panel.resultItems.length > 0
                text: qsTr("%1 个对象").arg(panel.resultItems.length)
                color: "#60708a"
                font.pixelSize: 12
            }
        }

        Text {
            Layout.fillWidth: true
            text: panel.verdict.length > 0 ? panel.summary : panel.emptyHint
            color: "#60708a"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panel.verdict.length > 0
            spacing: 16
            Text {
                // 偏差可能极小，放不下时简略显示，完整数值鼠标悬停可见
                text: qsTr("最大偏差 %1").arg(NumberFormat.toCompact(panel.maxDeviation, 12))
                color: "#55657c"
                font.pixelSize: 12
                elide: Text.ElideRight
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    ToolTip.visible: containsMouse
                    ToolTip.text: NumberFormat.toPlain(panel.maxDeviation)
                }
            }
            Text {
                text: qsTr("最大单帧 %1 ms").arg(Number(panel.maxFrameMs).toFixed(4))
                color: "#55657c"
                font.pixelSize: 12
            }
            Text {
                text: qsTr("内存变化 %1 MB").arg(Number(panel.memoryDeltaMB).toFixed(2))
                color: "#55657c"
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
        }

        // 对象选择：切换后左右两张表同时刷新为该对象的坐标。
        RowLayout {
            Layout.fillWidth: true
            visible: panel.verdict.length > 0
            spacing: 10
            Text { text: qsTr("查看对象"); color: "#60708a"; font.pixelSize: 11 }
            AppComboBox {
                id: objectBox
                implicitWidth: 220
                implicitHeight: 30
                enabled: panel.objectEntries.length > 0
                model: panel.objectEntries
                textRole: "label"
                onActivated: panel.currentObjectIndex = currentIndex
                // 重新执行测试后面板会重置为 #0，这里同步回下拉框选中项。
                Connections {
                    target: panel
                    onCurrentObjectIndexChanged: {
                        if (objectBox.currentIndex !== panel.currentObjectIndex)
                            objectBox.currentIndex = panel.currentObjectIndex
                    }
                }
            }
            Text {
                text: qsTr("基线 %1 点 · 交错 %2 点")
                      .arg(panel.baselinePoints.length).arg(panel.interleavedPoints.length)
                color: "#60708a"
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
        }

        // 左右两张坐标表：同一行号表示同一步，可直接比较基线与交错坐标。
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150
            visible: panel.verdict.length > 0
            spacing: 8

            Repeater {
                model: [{ key: "baseline", title: qsTr("单对象基线轨迹") },
                        { key: "interleaved", title: qsTr("单线程交错运行轨迹") }]

                delegate: Rectangle {
                    id: tableCard
                    readonly property string tableKey: modelData.key
                    readonly property var tablePoints: tableKey === "baseline"
                                                       ? panel.baselinePoints
                                                       : panel.interleavedPoints

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 190
                    radius: 7
                    color: "white"
                    border.color: "#e2e9ef"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: "#14213d"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 26
                            radius: 4
                            color: "#f1f5f9"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 6
                                Text {
                                    Layout.preferredWidth: 44
                                    text: qsTr("序号")
                                    color: "#55657c"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("经度")
                                    color: "#55657c"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("纬度")
                                    color: "#55657c"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }

                        ListView {
                            id: pointList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: tableCard.tablePoints
                            ScrollBar.vertical: ScrollBar { }
                            // 两张表行数一致，滚动时保持同步，便于逐行比对。
                            onContentYChanged: {
                                var other = panel.pointLists[tableCard.tableKey === "baseline"
                                                             ? "interleaved" : "baseline"]
                                if (other && Math.abs(other.contentY - contentY) > 0.5)
                                    other.contentY = contentY
                            }
                            Component.onCompleted: panel.pointLists[tableCard.tableKey] = pointList

                            delegate: RowLayout {
                                width: pointList.width - 12
                                height: 24
                                spacing: 6
                                Text {
                                    Layout.preferredWidth: 44
                                    text: index + 1
                                    color: "#8794a6"
                                    font.pixelSize: 11
                                    font.family: "Consolas"
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: Number(modelData.x).toFixed(6)
                                    color: "#33415c"
                                    font.pixelSize: 11
                                    font.family: "Consolas"
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: Number(modelData.y).toFixed(6)
                                    color: "#33415c"
                                    font.pixelSize: 11
                                    font.family: "Consolas"
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: tableCard.tablePoints.length === 0
                            text: qsTr("未采集到轨迹点")
                            color: "#8794a6"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }

        // 逐对象明细：点击某一行即可在上方两张表中查看该对象的坐标。
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 150
            Layout.minimumHeight: 0
            visible: panel.resultItems.length > 0
            clip: true
            spacing: 5
            model: panel.resultItems
            ScrollBar.vertical: ScrollBar { }
            delegate: Rectangle {
                width: ListView.view.width
                height: 44
                radius: 6
                color: index === panel.currentObjectIndex ? "#eef7f6" : "#f8fafc"
                border.color: index === panel.currentObjectIndex ? "#7fc9c3" : "#e2e9ef"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Text {
                        Layout.preferredWidth: 170
                        text: modelData.modelName !== undefined && String(modelData.modelName).length > 0
                              ? qsTr("#%1 %2").arg(modelData.objectId).arg(modelData.modelName)
                              : qsTr("#%1").arg(modelData.objectId)
                        color: "#14213d"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        // 放不下时简略显示，完整数值鼠标悬停可见
                        text: qsTr("偏差 %1").arg(NumberFormat.toCompact(modelData.deviation, 12))
                        color: "#60708a"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        Layout.preferredWidth: 60
                        Layout.preferredHeight: 26
                        Layout.alignment: Qt.AlignVCenter
                        radius: 13
                        color: modelData.state === "通过" ? "#e6f6ec" : "#fdecec"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.state
                            color: modelData.state === "通过" ? "#1a9c5b" : "#c0392b"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: panel.currentObjectIndex = index
                    // 悬停时给出完整偏差数值（表格里可能只显示简略写法）
                    hoverEnabled: true
                    ToolTip.visible: containsMouse
                    ToolTip.text: qsTr("偏差 %1").arg(NumberFormat.toPlain(modelData.deviation))
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: panel.verdict.length > 0
            text: qsTr("左右两表按行号一一对应：基线第 N 点与交错第 N 点为同一步的输出坐标；点击下方对象条目可切换查看对象。")
            color: "#8794a6"
            font.pixelSize: 9
            wrapMode: Text.WordWrap
        }
    }
}
