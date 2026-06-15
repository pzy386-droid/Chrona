import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    property var focusItem: ({})
    property int unscheduledCount: 0
    property var scheduleIssues: []
    property bool active: false
    signal focusStarted()
    signal focusStopRequested()
    signal rescheduleRequested()

    height: root.unscheduledCount > 0 ? 150 : 104
    radius: 8
    color: "#161A23"
    border.width: 1
    border.color: "#252B3A"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 18

        Rectangle {
            width: 5
            Layout.fillHeight: true
            radius: 3
            color: root.focusItem.color || "#7C8CFF"
            Behavior on color { ColorAnimation { duration: 220 } }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 7

            Text {
                text: qsTr("当前专注")
                color: "#9AA4B2"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: root.focusItem.title || qsTr("没有正在进行的任务")
                color: "#E6EAF2"
                font.pixelSize: 21
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.focusItem.subtitle || qsTr("系统会在你添加任务后自动安排")
                color: "#9AA4B2"
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }

        Rectangle {
            visible: root.unscheduledCount > 0
            radius: 8
            color: "#2A1D1B"
            border.color: "#FF7A59"
            border.width: 1
            Layout.preferredWidth: 390
            Layout.preferredHeight: 104

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("调度冲突 · %1 个任务").arg(root.unscheduledCount)
                        color: "#FFB09B"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        Layout.preferredWidth: 70
                        Layout.preferredHeight: 26
                        radius: 7
                        color: conflictMouse.containsMouse ? "#3A2824" : "#11151D"
                        border.width: 1
                        border.color: "#684033"

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("重规划")
                            color: "#E6EAF2"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            id: conflictMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.rescheduleRequested()
                        }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: root.scheduleIssues || []

                    delegate: Text {
                        width: ListView.view.width
                        text: qsTr("%1 · 剩余 %2 分钟 · %3 · %4")
                            .arg(modelData.title || qsTr("任务"))
                            .arg(modelData.remainingMinutes || 0)
                            .arg(modelData.reason || qsTr("容量不足"))
                            .arg(modelData.fixHint || qsTr("调整截止时间或减少时长"))
                        color: "#FFD2C5"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            radius: 8
            color: startMouse.containsMouse ? "#8B99FF" : root.active ? "#30384C" : "#7C8CFF"
            Layout.preferredWidth: 92
            Layout.preferredHeight: 42
            opacity: startMouse.enabled ? 1 : 0.48

            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on opacity { NumberAnimation { duration: 160 } }

            MouseArea {
                id: startMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: root.focusItem && root.focusItem.taskId > 0
                onClicked: {
                    if (root.active) {
                        root.focusStopRequested()
                    } else {
                        var result = ScheduleService.startFocus()
                        root.active = result && result.ok
                        if (root.active) {
                            root.focusStarted()
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                text: root.active ? qsTr("取消") : qsTr("开始")
                color: "white"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
        }
    }
}
