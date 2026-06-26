import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    visible: false
    color: Theme.overlay
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
        color: Theme.canvasBackground
        border.color: Theme.border
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
                        color: Theme.primaryText
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.review.summary || qsTr("复盘今天的计划完成情况")
                        color: Theme.tertiaryText
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
                    accent: Theme.accentBright
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
                color: Theme.surfaceElevated

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
        color: Theme.surfaceMuted
        border.width: 1
        border.color: Theme.borderSoft

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: title + qsTr(" · %1").arg(modelData.length || 0)
                color: Theme.primaryText
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
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.surfaceHover

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            width: parent.width
                            text: modelData.title || qsTr("未命名任务")
                            color: Theme.primaryText
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: qsTr("%1 分钟 · %2").arg(modelData.todayPlannedMinutes || modelData.remainingMinutes || 0).arg(modelData.deadlineText || "")
                            color: Theme.secondaryText
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: modelData.length === 0
                    text: emptyText
                    color: Theme.tertiaryText
                    font.pixelSize: 12
                }
            }
        }
    }

    component StatCard: Rectangle {
        property string label: ""
        property string value: ""
        property color accent: Theme.accentBright

        Layout.preferredHeight: 92
        radius: 10
        color: Theme.surfaceMuted
        border.width: 1
        border.color: Theme.borderSoft

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: label
                color: Theme.tertiaryText
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
        color: closeMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
        border.width: 1
        border.color: Theme.borderSoft

        Text {
            anchors.centerIn: parent
            text: "x"
            color: Theme.secondaryText
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
            ? (mouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted)
            : (mouse.containsMouse ? "#8B99FF" : Theme.accentBright)
        border.width: muted ? 1 : 0
        border.color: Theme.border

        Text {
            id: label
            anchors.centerIn: parent
            color: muted ? Theme.secondaryText : "white"
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
