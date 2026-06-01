import QtQuick
import QtQuick.Controls
import Chrona

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
    property color accentColor: "#6C63FF"

    property bool event: false
    property bool locked: false
    property bool eventLocked: false
    readonly property bool lockActive: root.locked || root.eventLocked

    property bool selected: false
    property int blockOrdinal: 0
    property int blockTotal: 0
    property int spanDays: 1
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

    property bool readyForResizeAnimation: false
    property real resizeGlowOpacity: 0
    property real resizePressY: 0
    property int resizeStartDuration: 0
    property int resizePreviewDuration: 0
    property bool resizeActive: false

    signal moveRequested(int blockId, int dayIndex, int startMinute, int durationMinutes)
    signal selectedItem(int taskId, int blockId)

    radius: 14
    color: selected ? Qt.darker(accentColor, 1.35) : "#1A1F2B"
    border.width: selected ? 2 : mouse.containsMouse ? 1 : 0
    border.color: accentColor
    opacity: mouse.drag.active ? 0.9 : 1
    z: root.resizeActive ? 12 : mouse.drag.active ? 10 : selected ? 4 : 1
    scale: mouse.containsMouse ? 1.02 : 1

    Behavior on scale { NumberAnimation { duration: 120 } }
    Behavior on x { enabled: !mouse.drag.active; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: !mouse.drag.active; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 360; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 130 } }
    Behavior on color { ColorAnimation { duration: 160 } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        radius: 3
        color: root.accentColor
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 12
        color: root.accentColor
        opacity: mouse.containsMouse ? 0.18 : 0.08
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.accentColor
        opacity: root.resizeGlowOpacity
        visible: opacity > 0
    }

    SequentialAnimation {
        id: resizeGlow
        NumberAnimation { target: root; property: "resizeGlowOpacity"; to: 0.22; duration: 80; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "resizeGlowOpacity"; to: 0; duration: 320; easing.type: Easing.OutCubic }
    }

    Component.onCompleted: root.readyForResizeAnimation = true

    onDurationMinutesChanged: {
        if (root.readyForResizeAnimation) {
            resizeGlow.restart()
        }
    }

    Item {
        id: lockBadge
        visible: root.lockActive || opacity > 0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 7
        anchors.topMargin: 7
        width: 20
        height: 20
        opacity: root.lockActive ? 1 : 0
        scale: root.lockActive ? 1 : 0.72

        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }

        Rectangle {
            anchors.fill: parent
            radius: 7
            color: "#2D3342"
            border.width: 1
            border.color: "#566176"
        }

        Rectangle {
            x: 5
            y: 4
            width: 10
            height: 8
            radius: 5
            color: "transparent"
            border.width: 2
            border.color: "#E6EAF2"
            transformOrigin: Item.BottomRight
            rotation: root.lockActive ? 0 : -18
            Behavior on rotation { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }

        Rectangle {
            x: 5
            y: 10
            width: 10
            height: 7
            radius: 2
            color: "#E6EAF2"
        }
    }

    Column {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: 4

        Text {
            width: parent.width
            text: root.title
            color: "#FFFFFF"
            font.pixelSize: root.height < 48 ? 12 : 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            visible: root.height >= 48
            width: parent.width
            text: root.timeRange + (root.blockTotal > 1 ? qsTr(" · %1/%2").arg(root.blockOrdinal).arg(root.blockTotal) : "")
            color: "#CBD5E1"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            visible: root.height >= 72
            width: parent.width
            text: root.subtitle
            color: "#94A3B8"
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        enabled: !root.resizeActive
        drag.target: root.lockActive ? null : root
        drag.axis: Drag.XAndYAxis
        drag.minimumX: root.timelineLeft + root.horizontalInset
        drag.maximumX: root.timelineLeft + Math.max(0, root.dayCount - 1) * root.dayWidth + root.horizontalInset
        drag.minimumY: 0
        drag.maximumY: Math.max(0, (root.dayEndMinute - root.dayStartMinute - root.durationMinutes) * root.minuteHeight)

        onPressed: {
            root.pressX = root.x
            root.pressY = root.y
            root.wasDragged = false
            ScheduleService.selectTimelineItem(root.taskId, root.blockId)
            root.selectedItem(root.taskId, root.blockId)
        }

        onPositionChanged: {
            if (pressed && (Math.abs(root.x - root.pressX) > 3 || Math.abs(root.y - root.pressY) > 3)) {
                root.wasDragged = true
            }
        }

        onReleased: {
            if (!root.wasDragged || root.lockActive) {
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

        onDoubleClicked: root.selectedItem(root.taskId, root.blockId)
    }

    Rectangle {
        id: resizeHandle
        visible: !root.lockActive && root.height >= 42
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 4
        height: 18
        radius: 2
        color: "transparent"
        z: 20

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: resizeMouse.containsMouse || root.resizeActive ? 4 : 2
            radius: 2
            color: root.accentColor
            opacity: resizeMouse.containsMouse || root.resizeActive ? 0.78 : 0.28

            Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            id: resizeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true

            onPressed: {
                root.resizeActive = true
                root.resizePressY = mouseY
                root.resizeStartDuration = root.durationMinutes
                root.resizePreviewDuration = root.durationMinutes
                ScheduleService.selectTimelineItem(root.taskId, root.blockId)
                root.selectedItem(root.taskId, root.blockId)
            }

            onPositionChanged: {
                if (!pressed) {
                    return
                }
                var deltaMinutes = (mouseY - root.resizePressY) / root.minuteHeight
                var nextDuration = Math.round((root.resizeStartDuration + deltaMinutes) / 15) * 15
                root.resizePreviewDuration = Math.max(15, Math.min(root.dayEndMinute - root.startMinute, nextDuration))
            }

            onReleased: {
                root.resizeActive = false
                if (root.resizePreviewDuration > 0 && root.resizePreviewDuration !== root.durationMinutes) {
                    root.moveRequested(root.blockId, root.dayIndex, root.startMinute, root.resizePreviewDuration)
                }
            }

            onCanceled: root.resizeActive = false
        }
    }

    Rectangle {
        visible: root.resizeActive
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: resizeHandle.top
        anchors.bottomMargin: 8
        width: 64
        height: 24
        radius: 7
        color: "#10141C"
        border.width: 1
        border.color: "#30384C"
        z: 21

        Text {
            anchors.centerIn: parent
            text: qsTr("%1 分钟").arg(root.resizePreviewDuration)
            color: "#E6EAF2"
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
    }

}
