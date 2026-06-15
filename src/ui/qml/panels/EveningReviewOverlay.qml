import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    visible: false
    color: "#B00A0E17"
    z: 200

    property var review: ({})

    onVisibleChanged: {
        if (visible) {
            review = ScheduleService.eveningReview()
        }
    }

    MouseArea { anchors.fill: parent }

    Rectangle {
        width: Math.min(parent.width - 48, 760)
        height: Math.min(parent.height - 48, 680)
        radius: 16
        color: "#11151D"
        border.color: "#30384C"
        border.width: 1
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("晚间复盘")
                        color: "#E6EAF2"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.review.summary || qsTr("复盘今天的计划完成情况")
                        color: "#8D98AB"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }

                CloseButton { onClicked: root.visible = false }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                StatCard {
                    Layout.fillWidth: true
                    label: qsTr("完成率")
                    value: qsTr("%1%").arg(root.review.completionRate || 0)
                    accent: "#4ADE80"
                }
                StatCard {
                    Layout.fillWidth: true
                    label: qsTr("计划分钟")
                    value: qsTr("%1").arg(root.review.plannedMinutes || 0)
                    accent: "#7C8CFF"
                }
                StatCard {
                    Layout.fillWidth: true
                    label: qsTr("完成分钟")
                    value: qsTr("%1").arg(root.review.completedMinutes || 0)
                    accent: "#F59E0B"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 10
                radius: 5
                color: "#1D2432"

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(100, root.review.completionRate || 0)) / 100
                    height: parent.height
                    radius: parent.radius
                    color: "#4ADE80"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                ReviewList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("未完成任务")
                    emptyText: qsTr("今天没有未完成的已排任务")
                    modelData: root.review.unfinishedTasks || []
                }

                ReviewList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("结转任务")
                    emptyText: qsTr("暂无需要结转的任务")
                    modelData: root.review.carriedOverTasks || []
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("稍后再说")
                    muted: true
                    onClicked: root.visible = false
                }

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("确认结束今日")
                    onClicked: {
                        ScheduleService.reschedule()
                        root.review = ScheduleService.eveningReview()
                        root.visible = false
                    }
                }
            }
        }
    }

    component ReviewList: Rectangle {
        property string title: ""
        property string emptyText: ""
        property var modelData: []

        radius: 10
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: title + qsTr(" · %1").arg(modelData.length || 0)
                color: "#E6EAF2"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: modelData

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 58
                    radius: 8
                    color: "#10141C"
                    border.width: 1
                    border.color: "#252B3A"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            width: parent.width
                            text: modelData.title || qsTr("未命名任务")
                            color: "#E6EAF2"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: qsTr("%1 分钟 · %2").arg(modelData.todayPlannedMinutes || modelData.remainingMinutes || 0).arg(modelData.deadlineText || "")
                            color: "#94A3B8"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: modelData.length === 0
                    text: emptyText
                    color: "#7D8798"
                    font.pixelSize: 12
                }
            }
        }
    }

    component StatCard: Rectangle {
        property string label: ""
        property string value: ""
        property color accent: "#7C8CFF"

        Layout.preferredHeight: 92
        radius: 10
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: label
                color: "#8D98AB"
                font.pixelSize: 12
            }

            Text {
                Layout.fillWidth: true
                text: value
                color: accent
                font.pixelSize: 28
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
        }
    }

    component CloseButton: Rectangle {
        signal clicked()

        Layout.preferredWidth: 32
        Layout.preferredHeight: 32
        radius: 8
        color: closeMouse.containsMouse ? "#202638" : "#151A23"
        border.width: 1
        border.color: "#2C3548"

        Text {
            anchors.centerIn: parent
            text: "x"
            color: "#AAB4C6"
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    component ActionButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        signal clicked()

        Layout.preferredHeight: 44
        radius: 10
        color: muted
            ? (mouse.containsMouse ? "#202638" : "#151A23")
            : (mouse.containsMouse ? "#8B99FF" : "#7C8CFF")
        border.width: muted ? 1 : 0
        border.color: "#30384C"

        Text {
            id: label
            anchors.centerIn: parent
            color: muted ? "#AAB4C6" : "white"
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }
}
