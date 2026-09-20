import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

// 轨迹点经纬度数据表：序号 / 对象 / 经度 / 纬度，可滚动，避免只靠图形判断数据。
Rectangle {
    id: table

    // [{ objectId, baseline, points: [{ x: 经度, y: 纬度 }] }]
    property var series: []
    property int maxRows: 1200
    property bool singleTrajectory: false

    radius: 6
    color: "white"
    border.color: "#dce4ec"

    property var rows: buildRows()

    function buildRows() {
        var out = []
        var all = series || []
        for (var s = 0; s < all.length; ++s) {
            var points = all[s].points || []
            for (var i = 0; i < points.length; ++i) {
                if (out.length >= maxRows)
                    return out
                out.push({
                    "seq": out.length,
                    "label": singleTrajectory
                             ? ""
                             : ("#" + all[s].objectId + (all[s].baseline ? qsTr(" 基准") : "")),
                    "lon": Number(points[i].x),
                    "lat": Number(points[i].y)
                })
            }
        }
        return out
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 5

        Text {
            Layout.fillWidth: true
            text: qsTr("轨迹点经纬度（%1）").arg(table.rows.length)
            color: "#14213d"
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            Text {
                Layout.preferredWidth: 32
                text: qsTr("序号")
                color: "#8794a6"
                font.pixelSize: 9
            }
            Text {
                Layout.preferredWidth: 60
                visible: !table.singleTrajectory
                text: qsTr("对象")
                color: "#8794a6"
                font.pixelSize: 9
            }
            Text {
                Layout.preferredWidth: 76
                text: qsTr("经度")
                color: "#8794a6"
                font.pixelSize: 9
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("纬度")
                color: "#8794a6"
                font.pixelSize: 9
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 1
            model: table.rows
            ScrollBar.vertical: ScrollBar { }
            delegate: RowLayout {
                width: ListView.view.width
                spacing: 4
                Text {
                    Layout.preferredWidth: 32
                    text: modelData.seq
                    color: "#8794a6"
                    font.pixelSize: 10
                }
                Text {
                    Layout.preferredWidth: 60
                    visible: !table.singleTrajectory
                    text: modelData.label
                    color: "#60708a"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Text {
                    Layout.preferredWidth: 76
                    text: Number(modelData.lon).toFixed(6)
                    color: "#14213d"
                    font.family: "Consolas"
                    font.pixelSize: 10
                }
                Text {
                    Layout.fillWidth: true
                    text: Number(modelData.lat).toFixed(6)
                    color: "#14213d"
                    font.family: "Consolas"
                    font.pixelSize: 10
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: table.rows.length === 0
            text: qsTr("未采集到轨迹点")
            color: "#8794a6"
            font.pixelSize: 11
        }
    }
}
