import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property int blockId: 0
    property int taskId: 0
    property string title: ""
    property string subtitle: ""
    property string timeRange: ""
    property int dayIndex: 0
    property int startMinute: 0
    property int durationMinutes: 0
    property int priority: 1
    property color accentColor: "#7C8CFF"
    property bool event: false
    property bool locked: false
    property bool eventLocked: false
    property bool selected: false
    property real dayWidth: 120
    property real minuteHeight: 1
    property real timelineLeft: 0
    property real horizontalInset: 5
    property int dayStartMinute: 480
    property int dayEndMinute: 1380
    property int dayCount: 7
    property real pressX: 0
    property real pressY: 0
    property bool wasDragged: false
    signal moveRequested(int blockId, int dayIndex, int startMinute, int durationMinutes)
    signal selectedTask(int taskId)
    signal eventSettingsRequested(int eventId, bool locked)

    radius: 8
    color: event ? "#242A36" : selected ? "#252D44" : "#1A2030"
    border.width: selected || mouse.containsMouse ? 1 : 0
    border.color: event ? "#4A5264" : accentColor
    opacity: mouse.drag.active ? 0.88 : 1
    z: mouse.drag.active ? 10 : selected ? 4 : 1

    Behavior on x { enabled: !mouse.drag.active; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: !mouse.drag.active; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 130 } }
    Behavior on color { ColorAnimation { duration: 160 } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        radius: 2
        color: root.accentColor
    }

    Rectangle {
        visible: root.event
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 7
        anchors.topMargin: 7
        width: root.eventLocked ? 34 : 46
        height: 18
        radius: 6
        color: root.eventLocked ? "#2D3342" : "#1E3B34"
        border.width: 1
        border.color: root.eventLocked ? "#556074" : "#4FAE85"

        Text {
            anchors.centerIn: parent
            text: root.eventLocked ? qsTr("固定") : qsTr("可移动")
            color: root.eventLocked ? "#AAB4C6" : "#A9F0C9"
            font.pixelSize: 10
            font.weight: Font.DemiBold
        }
    }

    Column {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 4

        Text {
            width: parent.width
            text: root.title
            color: "#E6EAF2"
            font.pixelSize: root.height < 48 ? 11 : 13
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            visible: root.height >= 48
            width: parent.width
            text: root.timeRange
            color: "#A5AEC0"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            visible: root.height >= 72
            width: parent.width
            text: root.subtitle
            color: "#7F8A9E"
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        drag.target: root.locked ? null : root
        drag.axis: Drag.XAndYAxis
        drag.minimumX: root.timelineLeft + root.horizontalInset
        drag.maximumX: root.timelineLeft + Math.max(0, root.dayCount - 1) * root.dayWidth + root.horizontalInset
        drag.minimumY: 0
        drag.maximumY: Math.max(0, (root.dayEndMinute - root.dayStartMinute - root.durationMinutes) * root.minuteHeight)
        onPressed: {
            root.pressX = root.x
            root.pressY = root.y
            root.wasDragged = false
        }
        onPositionChanged: {
            if (pressed && (Math.abs(root.x - root.pressX) > 3 || Math.abs(root.y - root.pressY) > 3)) {
                root.wasDragged = true
            }
        }
        onReleased: {
            if (!root.wasDragged || root.locked) {
                return
            }

            var rawDay = (root.x - root.timelineLeft - root.horizontalInset) / root.dayWidth
            var nextDay = Math.max(0, Math.min(root.dayCount - 1, Math.round(rawDay)))
            var rawMinute = root.dayStartMinute + root.y / root.minuteHeight
            var nextMinute = Math.max(root.dayStartMinute, Math.min(root.dayEndMinute - root.durationMinutes, Math.round(rawMinute / 15) * 15))

            root.x = root.timelineLeft + nextDay * root.dayWidth + root.horizontalInset
            root.y = (nextMinute - root.dayStartMinute) * root.minuteHeight
            root.moveRequested(root.blockId, nextDay, nextMinute, root.durationMinutes)
        }
        onClicked: {
            if (root.wasDragged) {
                return
            }
            if (root.event) {
                root.eventSettingsRequested(Math.abs(root.blockId), root.eventLocked)
            } else if (root.taskId > 0) {
                root.selectedTask(root.taskId)
            }
        }
        onDoubleClicked: {
            if (!root.event && root.taskId > 0) {
                root.selectedTask(root.taskId)
            }
        }
    }
}
