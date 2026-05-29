import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var capacityStats: ({})
    signal addRequested(string text)
    signal imagePreviewRequested(string fileUrl)

    height: 112
    radius: 8
    color: "#161A23"
    border.width: 1
    border.color: dropArea.containsDrag ? "#7C8CFF" : "#252B3A"

    Behavior on border.color { ColorAnimation { duration: 160 } }

    DropArea {
        id: dropArea
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.urls.length > 0) {
                root.imagePreviewRequested(String(drop.urls[0]))
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#10141C"
            border.width: 1
            border.color: "#252B3A"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 10

                Text {
                    text: "AI"
                    color: "#7C8CFF"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }

                TextField {
                    id: input
                    Layout.fillWidth: true
                    placeholderText: qsTr("例如：明天下午两点写高数作业")
                    color: "#E6EAF2"
                    placeholderTextColor: "#667187"
                    font.pixelSize: 14
                    background: Item {}
                    onAccepted: {
                        if (text.trim().length > 0) {
                            root.addRequested(text.trim())
                            text = ""
                        }
                    }
                }

                CapsuleButton {
                    text: qsTr("AI 规划")
                    onClicked: {
                        if (input.text.trim().length > 0) {
                            root.addRequested(input.text.trim())
                            input.text = ""
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 210
            Layout.fillHeight: true
            radius: 8
            color: dropArea.containsDrag ? "#202842" : "#10141C"
            border.width: 1
            border.color: dropArea.containsDrag ? "#7C8CFF" : "#252B3A"

            Column {
                anchors.centerIn: parent
                spacing: 5

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("拖拽截图识别")
                    color: "#E6EAF2"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("课程表 / 作业通知 / 考试安排")
                    color: "#8C96AA"
                    font.pixelSize: 11
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 184
            Layout.fillHeight: true
            radius: 8
            color: "#10141C"
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: root.capacityStats.message || qsTr("学习容量健康")
                    color: root.capacityStats.overloaded ? "#FFB09B" : "#A9F0C9"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("今日已排 %1 分钟").arg(root.capacityStats.todayScheduledMinutes || 0)
                    color: "#9AA4B2"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("本周负载 %1 小时").arg(Math.round((root.capacityStats.weekScheduledMinutes || 0) / 60))
                    color: "#9AA4B2"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }
    }

    component CapsuleButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        signal clicked()

        width: muted ? 88 : 76
        height: 38
        radius: 8
        color: muted ? (mouse.containsMouse ? "#202638" : "#161A23") : (mouse.containsMouse ? "#8B99FF" : "#7C8CFF")
        border.width: muted ? 1 : 0
        border.color: "#30384C"

        Behavior on color { ColorAnimation { duration: 140 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: muted ? "#AAB4C6" : "white"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.clicked()
        }
    }
}
