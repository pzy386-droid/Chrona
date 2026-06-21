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
    property double nowMs: Date.now()

    readonly property bool hasFocusBlock: focusItem && focusItem.taskId > 0 && focusItem.blockId > 0
    readonly property double startMs: Number(focusItem.startMs || 0)
    readonly property double endMs: Number(focusItem.endMs || 0)
    readonly property bool hasValidClock: startMs > 0 && endMs > startMs
    readonly property bool canStartByClock: hasFocusBlock && hasValidClock && nowMs >= startMs && nowMs < endMs
    readonly property bool canStartNow: hasFocusBlock && (focusItem.isCurrent === true || canStartByClock)
    readonly property bool blockEnded: hasFocusBlock
                                       && (focusItem.canComplete === true
                                           || focusItem.isPast === true
                                           || (hasValidClock && nowMs >= endMs))
    readonly property bool actionEnabled: hasFocusBlock && (active || canStartNow)

    signal focusStarted()
    signal focusStopRequested()
    signal rescheduleRequested()

    height: root.unscheduledCount > 0 ? 150 : 104
    radius: 8
    color: Theme.surface
    border.width: 1
    border.color: root.canStartNow && !root.active ? Theme.successBorder : Theme.border

    Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: 220 } }

    Timer {
        interval: 1000
        repeat: true
        running: root.visible
        triggeredOnStart: true
        onTriggered: root.nowMs = Date.now()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 18

        Rectangle {
            width: 5
            Layout.fillHeight: true
            radius: 3
            color: root.focusItem.color || Theme.accentBright
            Behavior on color { ColorAnimation { duration: 220 } }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 7

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("当前专注")
                    color: Theme.secondaryText
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    visible: root.canStartNow && !root.active
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: "#6EE7A8"
                    opacity: 0.85

                    SequentialAnimation on opacity {
                        running: root.canStartNow && !root.active
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.25; duration: 720; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 0.95; duration: 720; easing.type: Easing.InOutSine }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.focusItem.title || qsTr("没有正在进行的任务")
                color: Theme.primaryText
                font.pixelSize: 21
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.focusItem.subtitle || qsTr("系统会在你添加任务后自动安排")
                color: Theme.secondaryText
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }

        Rectangle {
            visible: root.unscheduledCount > 0
            radius: 8
            color: Theme.dangerSurface
            border.color: Theme.dangerBorder
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
                        color: Theme.error
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        Layout.preferredWidth: 70
                        Layout.preferredHeight: 26
                        radius: 7
                        color: conflictMouse.containsMouse ? Theme.dangerSurfaceHover : Theme.canvasBackground
                        border.width: 1
                        border.color: Theme.dangerBorder
                        Behavior on color { ColorAnimation { duration: 140 } }

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("重规划")
                            color: Theme.primaryText
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
                        color: Theme.error
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            radius: 8
            color: startMouse.containsMouse && startMouse.enabled
                   ? (root.canStartNow ? "#74E0A4" : "#8B99FF")
                   : root.active
                     ? Theme.border
                     : root.canStartNow
                       ? "#55D98B"
                       : Theme.accentBright
            Layout.preferredWidth: 96
            Layout.preferredHeight: 42
            opacity: startMouse.enabled ? 1 : 0.48

            Behavior on color { ColorAnimation { duration: 180 } }
            Behavior on opacity { NumberAnimation { duration: 160 } }

            SequentialAnimation on scale {
                running: root.canStartNow && !root.active
                loops: Animation.Infinite
                NumberAnimation { to: 1.035; duration: 760; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 760; easing.type: Easing.InOutSine }
            }

            MouseArea {
                id: startMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: root.actionEnabled
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (root.active) {
                        root.focusStopRequested()
                        return
                    }
                    var result = ScheduleService.startFocus()
                    root.active = result && result.ok
                    if (root.active) {
                        root.focusStarted()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                text: root.active ? qsTr("退出") : qsTr("开始")
                color: "white"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
        }
    }
}
