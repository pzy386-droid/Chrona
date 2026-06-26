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
    property string explanation: ""
    property int dayIndex: 0
    property int startMinute: 0
    property int durationMinutes: 0
    property int priority: 1
    property color accentColor: Theme.accent
    property string kind: "task"
    property string source: "auto"

    property bool event: false
    property bool locked: false
    property bool eventLocked: false
    property bool completed: false
    property bool canMove: true
    property bool canResize: true
    property bool canLock: true

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
    property real resizePressSceneY: 0
    property int resizeStartDuration: 0
    property int resizePreviewDuration: 0
    property bool resizeActive: false
    property bool resizeFromTop: false
    property int resizeStartMinute: 0
    property int resizePreviewStartMinute: 0
    property bool spanResizeActive: false
    property real spanPressX: 0
    property real spanPressSceneX: 0
    property int spanStartDays: 1
    property int spanPreviewDays: 1
    property real spanPreviewVisualDays: 1
    property bool spanResizeFromLeft: false
    property int spanFixedEndDayIndex: 0
    property int spanPreviewStartDayIndex: dayIndex
    property real spanPreviewVisualStartDayIndex: dayIndex
    property int pendingSpanEndDayIndex: -1
    property int pendingSpanStartDayIndex: -1
    property real spanDrillGlow: 0
    readonly property bool compactBlock: root.height < 34
    readonly property bool tinyBlock: root.height < 24
    readonly property bool handlesRevealed: root.selected || mouse.containsMouse
        || root.resizeActive || root.spanResizeActive
    readonly property bool highPriority: root.priority >= 2 && !root.completed
    property real priorityPulse: 0.26

    signal moveRequested(int blockId, int dayIndex, int startMinute, int durationMinutes)
    signal resizeTimeRequested(int blockId, int startMinute, int durationMinutes)
    signal spanRequested(int blockId, int endDayIndex)
    signal spanRangeRequested(int blockId, int startDayIndex, int endDayIndex)
    signal selectedItem(int taskId, int blockId)

    radius: 14
    color: root.completed
        ? (selected ? Theme.blockCompletedSelected : Theme.blockCompleted)
        : selected ? (Theme.dark ? Qt.darker(accentColor, 1.35) : Theme.blockSelected)
                   : Theme.surfaceElevated
    border.width: highPriority ? (selected ? 3 : 2) : selected ? 2 : mouse.containsMouse ? 1 : 0
    border.color: root.completed ? Theme.blockCompletedAccent : highPriority ? Theme.highPriority : accentColor
    opacity: mouse.drag.active ? 0.9 : root.completed ? 0.72 : 1
    z: root.resizeActive || root.spanResizeActive ? 12 : mouse.drag.active ? 10 : selected ? 4 : 1
    scale: mouse.containsMouse ? 1.02 : 1

    Behavior on scale { NumberAnimation { duration: 120 } }
    Behavior on x { enabled: !mouse.drag.active; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on y { enabled: !mouse.drag.active; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on width { NumberAnimation { duration: root.spanResizeActive ? 180 : 220; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 360; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 130 } }
    Behavior on color { ColorAnimation { duration: 160 } }

    SequentialAnimation on priorityPulse {
        running: root.highPriority
        loops: Animation.Infinite
        NumberAnimation { from: 0.18; to: 0.42; duration: 920; easing.type: Easing.InOutSine }
        NumberAnimation { from: 0.42; to: 0.18; duration: 920; easing.type: Easing.InOutSine }
    }

    Rectangle {
        visible: root.highPriority
        anchors.fill: parent
        anchors.margins: 3
        radius: Math.max(2, root.radius - 3)
        color: "transparent"
        border.width: 2
        border.color: Theme.highPriority
        opacity: root.priorityPulse
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        radius: 3
        color: root.completed ? Theme.blockCompletedAccent : root.accentColor
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 12
        color: root.completed ? Theme.blockCompletedAccent : root.accentColor
        opacity: root.completed ? (mouse.containsMouse ? 0.16 : 0.1) : mouse.containsMouse ? 0.18 : 0.08
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.accentColor
        opacity: root.resizeGlowOpacity
        visible: opacity > 0
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(parent.width, 46)
        radius: root.radius
        color: root.accentColor
        opacity: root.spanDrillGlow
        visible: opacity > 0
    }

    SequentialAnimation {
        id: resizeGlow
        NumberAnimation { target: root; property: "resizeGlowOpacity"; to: 0.22; duration: 80; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "resizeGlowOpacity"; to: 0; duration: 320; easing.type: Easing.OutCubic }
    }

    SequentialAnimation {
        id: spanDrill
        NumberAnimation { target: root; property: "spanDrillGlow"; to: 0.24; duration: 90; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "spanDrillGlow"; to: 0; duration: 260; easing.type: Easing.OutCubic }
    }

    Timer {
        id: spanCommitTimer
        interval: 140
        repeat: false
        onTriggered: {
            if (root.pendingSpanEndDayIndex >= 0 && root.pendingSpanStartDayIndex >= 0) {
                var startDay = root.pendingSpanStartDayIndex
                var endDay = root.pendingSpanEndDayIndex
                root.pendingSpanStartDayIndex = -1
                root.pendingSpanEndDayIndex = -1
                root.spanResizeActive = false
                root.spanRangeRequested(root.blockId, startDay, endDay)
            }
        }
    }

    Component.onCompleted: root.readyForResizeAnimation = true

    onDurationMinutesChanged: {
        if (root.readyForResizeAnimation) {
            resizeGlow.restart()
        }
    }

    Item {
        visible: root.locked || root.eventLocked
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: root.compactBlock ? 12 : 18
        anchors.topMargin: root.compactBlock ? 4 : 8
        width: root.compactBlock ? 14 : 18
        height: root.compactBlock ? 14 : 18
        z: 15

        Rectangle {
            x: parent.width * 0.25
            y: 1
            width: parent.width * 0.5
            height: parent.height * 0.52
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: Theme.blockHandle
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.58
            radius: 4
            color: Theme.blockHandle
            opacity: 0.92
        }
    }

    Column {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        anchors.topMargin: root.height < 28 ? 1 : root.height < 48 ? 5 : 10
        anchors.bottomMargin: root.height < 28 ? 1 : root.height < 48 ? 4 : 10
        spacing: root.height < 48 ? 2 : 4
        clip: true

        Text {
            width: parent.width
            height: root.height < 28 ? Math.max(12, parent.height) : implicitHeight
            text: root.title
            color: root.completed ? Theme.secondaryText : Theme.strongText
            font.pixelSize: root.height < 28 ? 10 : root.height < 48 ? 12 : 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            visible: root.height >= 46
            width: parent.width
            text: root.timeRange + (root.blockTotal > 1 ? qsTr(" · %1/%2").arg(root.blockOrdinal).arg(root.blockTotal) : "")
            color: root.completed ? Theme.tertiaryText : Theme.primaryText
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            visible: root.height >= 72
            width: parent.width
            text: root.explanation && root.explanation.length > 0 ? root.explanation : root.subtitle
            color: root.completed ? Theme.mutedText : Theme.secondaryText
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        enabled: !root.resizeActive && !root.spanResizeActive
        drag.target: root.canMove ? root : null
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
            if (!root.wasDragged || !root.canMove) {
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
        id: topResizeHandle
        visible: root.canResize
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.compactBlock ? 18 : 14
        anchors.rightMargin: root.compactBlock ? 18 : 14
        anchors.topMargin: root.compactBlock ? 2 : 3
        height: root.compactBlock ? 8 : 14
        radius: 3
        color: "transparent"
        opacity: root.handlesRevealed ? 0.82 : 0
        z: 22

        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 1
            width: Math.max(12, Math.min(56, parent.width * 0.58))
            height: topResizeMouse.containsMouse || (root.resizeActive && root.resizeFromTop) ? 4 : 2
            radius: height / 2
            color: root.completed ? Theme.primaryText : Theme.blockHandle
            opacity: topResizeMouse.containsMouse || (root.resizeActive && root.resizeFromTop) ? 0.82 : 0.62

            Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            id: topResizeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true

            onPressed: {
                root.resizeActive = true
                root.resizeFromTop = true
                root.resizePressY = mouseY
                root.resizePressSceneY = topResizeMouse.mapToItem(root.parent, mouseX, mouseY).y
                root.resizeStartMinute = root.startMinute
                root.resizeStartDuration = root.durationMinutes
                root.resizePreviewStartMinute = root.startMinute
                root.resizePreviewDuration = root.durationMinutes
                ScheduleService.selectTimelineItem(root.taskId, root.blockId)
                root.selectedItem(root.taskId, root.blockId)
            }

            onPositionChanged: {
                if (!pressed) {
                    return
                }
                var fixedEnd = root.resizeStartMinute + root.resizeStartDuration
                var sceneY = topResizeMouse.mapToItem(root.parent, mouseX, mouseY).y
                var deltaMinutes = (sceneY - root.resizePressSceneY) / root.minuteHeight
                var nextStart = Math.round((root.resizeStartMinute + deltaMinutes) / 15) * 15
                nextStart = Math.max(root.dayStartMinute, Math.min(fixedEnd - 15, nextStart))
                root.resizePreviewStartMinute = nextStart
                root.resizePreviewDuration = fixedEnd - nextStart
            }

            onReleased: {
                if (root.resizePreviewDuration > 0
                        && (root.resizePreviewStartMinute !== root.startMinute
                            || root.resizePreviewDuration !== root.durationMinutes)) {
                    root.resizeTimeRequested(root.blockId, root.resizePreviewStartMinute, root.resizePreviewDuration)
                }
                root.resizeActive = false
            }

            onCanceled: root.resizeActive = false
        }
    }

    Rectangle {
        id: resizeHandle
        visible: root.canResize
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.compactBlock ? 18 : 14
        anchors.rightMargin: root.compactBlock ? 18 : 14
        anchors.bottomMargin: root.compactBlock ? 2 : 3
        height: root.compactBlock ? 8 : 14
        radius: 2
        color: "transparent"
        opacity: root.handlesRevealed ? 0.82 : 0
        z: 20

        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            width: Math.max(12, Math.min(56, parent.width * 0.58))
            height: resizeMouse.containsMouse || (root.resizeActive && !root.resizeFromTop) ? 4 : 2
            radius: height / 2
            color: root.completed ? Theme.primaryText : Theme.blockHandle
            opacity: resizeMouse.containsMouse || root.resizeActive ? 0.82 : 0.62

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
                root.resizeFromTop = false
                root.resizePressY = mouseY
                root.resizePressSceneY = resizeMouse.mapToItem(root.parent, mouseX, mouseY).y
                root.resizeStartMinute = root.startMinute
                root.resizeStartDuration = root.durationMinutes
                root.resizePreviewStartMinute = root.startMinute
                root.resizePreviewDuration = root.durationMinutes
                ScheduleService.selectTimelineItem(root.taskId, root.blockId)
                root.selectedItem(root.taskId, root.blockId)
            }

            onPositionChanged: {
                if (!pressed) {
                    return
                }
                var sceneY = resizeMouse.mapToItem(root.parent, mouseX, mouseY).y
                var deltaMinutes = (sceneY - root.resizePressSceneY) / root.minuteHeight
                var nextDuration = Math.round((root.resizeStartDuration + deltaMinutes) / 15) * 15
                root.resizePreviewDuration = Math.max(15, Math.min(root.dayEndMinute - root.startMinute, nextDuration))
                root.resizePreviewStartMinute = root.startMinute
            }

            onReleased: {
                if (root.resizePreviewDuration > 0 && root.resizePreviewDuration !== root.durationMinutes) {
                    root.resizeTimeRequested(root.blockId, root.startMinute, root.resizePreviewDuration)
                }
                root.resizeActive = false
            }

            onCanceled: root.resizeActive = false
        }
    }

    Rectangle {
        id: leftSpanHandle
        visible: root.canResize && root.dayCount > 1
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.compactBlock ? 1 : 3
        anchors.topMargin: root.compactBlock ? 3 : 6
        anchors.bottomMargin: root.compactBlock ? 3 : 6
        width: root.compactBlock ? 9 : 14
        radius: 7
        color: "transparent"
        opacity: root.handlesRevealed ? 0.82 : 0
        z: 24

        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.verticalCenter: parent.verticalCenter
            width: leftSpanMouse.containsMouse || (root.spanResizeActive && root.spanResizeFromLeft) ? 5 : 3
            height: Math.max(12, Math.min(34, parent.height * 0.58))
            radius: width / 2
            color: root.completed ? Theme.primaryText : Theme.blockHandle
            opacity: leftSpanMouse.containsMouse || (root.spanResizeActive && root.spanResizeFromLeft) ? 0.82 : 0.5

            Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            id: leftSpanMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true

            onPressed: {
                root.spanResizeActive = true
                root.spanResizeFromLeft = true
                root.spanPressSceneX = leftSpanMouse.mapToItem(root.parent, mouseX, mouseY).x
                root.spanStartDays = Math.max(1, root.spanDays)
                root.spanFixedEndDayIndex = root.dayIndex + root.spanStartDays - 1
                root.spanPreviewStartDayIndex = root.dayIndex
                root.spanPreviewVisualStartDayIndex = root.dayIndex
                root.spanPreviewDays = root.spanStartDays
                root.spanPreviewVisualDays = root.spanStartDays
                ScheduleService.selectTimelineItem(root.taskId, root.blockId)
                root.selectedItem(root.taskId, root.blockId)
            }

            onPositionChanged: {
                if (!pressed) {
                    return
                }
                var sceneX = leftSpanMouse.mapToItem(root.parent, mouseX, mouseY).x
                var rawStart = root.dayIndex + (sceneX - root.spanPressSceneX) / root.dayWidth
                var visualStart = Math.max(0, Math.min(root.spanFixedEndDayIndex, rawStart))
                var nextStart = Math.max(0, Math.min(root.spanFixedEndDayIndex, Math.round(visualStart)))
                root.spanPreviewVisualStartDayIndex = visualStart
                root.spanPreviewVisualDays = root.spanFixedEndDayIndex - visualStart + 1
                if (nextStart !== root.spanPreviewStartDayIndex) {
                    root.spanPreviewStartDayIndex = nextStart
                    root.spanPreviewDays = root.spanFixedEndDayIndex - nextStart + 1
                    spanDrill.restart()
                }
            }

            onReleased: {
                if (root.spanPreviewStartDayIndex !== root.dayIndex) {
                    root.pendingSpanStartDayIndex = root.spanPreviewStartDayIndex
                    root.pendingSpanEndDayIndex = root.spanFixedEndDayIndex
                    spanCommitTimer.restart()
                } else {
                    root.spanResizeActive = false
                }
            }

            onCanceled: root.spanResizeActive = false
        }
    }

    Rectangle {
        id: spanHandle
        visible: root.canResize && root.dayCount > 1
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.compactBlock ? 1 : 3
        anchors.topMargin: root.compactBlock ? 5 : 8
        anchors.bottomMargin: root.compactBlock ? 5 : 8
        width: root.compactBlock ? 9 : 14
        radius: 7
        color: "transparent"
        opacity: root.handlesRevealed ? 0.82 : 0
        z: 23

        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.verticalCenter: parent.verticalCenter
            width: spanMouse.containsMouse || (root.spanResizeActive && !root.spanResizeFromLeft) ? 5 : 3
            height: Math.max(12, Math.min(34, parent.height * 0.58))
            radius: width / 2
            color: root.completed ? Theme.primaryText : Theme.blockHandle
            opacity: spanMouse.containsMouse || root.spanResizeActive ? 0.82 : 0.62

            Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            id: spanMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true

            onPressed: {
                root.spanResizeActive = true
                root.spanResizeFromLeft = false
                root.spanPressX = mouseX
                root.spanPressSceneX = spanMouse.mapToItem(root.parent, mouseX, mouseY).x
                root.spanStartDays = Math.max(1, root.spanDays)
                root.spanPreviewDays = root.spanStartDays
                root.spanPreviewVisualDays = root.spanStartDays
                root.spanPreviewStartDayIndex = root.dayIndex
                root.spanPreviewVisualStartDayIndex = root.dayIndex
                ScheduleService.selectTimelineItem(root.taskId, root.blockId)
                root.selectedItem(root.taskId, root.blockId)
            }

            onPositionChanged: {
                if (!pressed) {
                    return
                }
                var sceneX = spanMouse.mapToItem(root.parent, mouseX, mouseY).x
                var rawDays = root.spanStartDays + (sceneX - root.spanPressSceneX) / root.dayWidth
                var visualDays = Math.max(1, Math.min(root.dayCount - root.dayIndex, rawDays))
                var nextDays = Math.max(1, Math.min(root.dayCount - root.dayIndex, Math.round(visualDays)))
                root.spanPreviewVisualDays = visualDays
                if (nextDays !== root.spanPreviewDays) {
                    root.spanPreviewDays = nextDays
                    spanDrill.restart()
                }
            }

            onReleased: {
                if (root.spanPreviewDays !== root.spanDays) {
                    root.pendingSpanStartDayIndex = root.dayIndex
                    root.pendingSpanEndDayIndex = root.dayIndex + root.spanPreviewDays - 1
                    spanCommitTimer.restart()
                } else {
                    root.spanResizeActive = false
                }
            }

            onCanceled: root.spanResizeActive = false
        }
    }

    Rectangle {
        visible: root.resizeActive
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: root.resizeFromTop ? topResizeHandle.top : resizeHandle.top
        anchors.bottomMargin: root.resizeFromTop ? 4 : 8
        width: root.resizeFromTop ? 106 : 78
        height: 24
        radius: 7
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        z: 21

        Text {
            anchors.centerIn: parent
            text: root.resizeFromTop
                ? resizeLabelHelper.resizeLabel(root.resizePreviewStartMinute, root.resizePreviewDuration)
                : qsTr("%1 分钟").arg(root.resizePreviewDuration)
            color: Theme.primaryText
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
    }

    Rectangle {
        visible: root.spanResizeActive
        anchors.right: root.spanResizeFromLeft ? undefined : spanHandle.left
        anchors.left: root.spanResizeFromLeft ? leftSpanHandle.right : undefined
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 8
        anchors.leftMargin: 8
        width: 64
        height: 24
        radius: 7
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        z: 24

        Text {
            anchors.centerIn: parent
            text: qsTr("%1 天").arg(root.spanPreviewDays)
            color: Theme.primaryText
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
    }

    QtObject {
        id: resizeLabelHelper
        function resizeLabel(startMinute, duration) {
            var h = Math.floor(startMinute / 60)
            var m = startMinute % 60
            return String(h).padStart(2, "0") + ":" + String(m).padStart(2, "0") + " · " + duration + qsTr("分")
        }
    }
}
