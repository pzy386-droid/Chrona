import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    property string viewMode: "week"
    property var model
    property real requestedDayColumnWidth: 190
    readonly property int dayStartMinute: 0
    readonly property int dayEndMinute: 24 * 60
    readonly property int dayCount: viewMode === "week" ? 7 : 1
    property real minuteHeight: 1.18
    readonly property bool fitFullDay: minuteHeight <= 0.52
    readonly property real fitMinuteHeight: Math.max(0.22, (height - headerHeight - 30) / Math.max(1, dayEndMinute - dayStartMinute))
    readonly property real effectiveMinuteHeight: fitFullDay ? fitMinuteHeight : minuteHeight
    readonly property int hourLabelStep: fitFullDay ? 2 : (effectiveMinuteHeight < 0.58 ? 2 : 1)
    readonly property int rulerWidth: 54
    readonly property int headerHeight: 44
    readonly property real dayColumnWidth: viewMode === "week" ? requestedDayColumnWidth : Math.max(320, width - rulerWidth)
    readonly property real timelineWidth: rulerWidth + dayCount * dayColumnWidth
    property alias flickable: flick
    property string anchorDate: ScheduleService.selectedDateText
    property bool readOnly: false
    signal moveRejected(string message)
    property int currentMinute: {
        var now = new Date()
        return now.getHours() * 60 + now.getMinutes()
    }
    property real previousEffectiveMinuteHeight: effectiveMinuteHeight
    property bool initialScrollApplied: false

    function clampContentY(value) {
        return Math.max(0, Math.min(value, Math.max(0, flick.contentHeight - flick.height)))
    }

    function centerCurrentTime() {
        if (fitFullDay || flick.height <= 0) {
            flick.contentY = 0
            return
        }
        var currentY = (currentMinute - dayStartMinute) * effectiveMinuteHeight
        flick.contentY = clampContentY(currentY - flick.height / 2)
    }

    function preserveCurrentTimeAnchor(oldMinuteHeight) {
        if (!initialScrollApplied || fitFullDay || flick.height <= 0) {
            centerCurrentTime()
            initialScrollApplied = true
            return
        }

        // Keep the current-time line at the same viewport position while zooming.
        var viewportAnchor = (currentMinute - dayStartMinute) * oldMinuteHeight - flick.contentY
        var nextCurrentY = (currentMinute - dayStartMinute) * effectiveMinuteHeight
        flick.contentY = clampContentY(nextCurrentY - viewportAnchor)
    }

    onEffectiveMinuteHeightChanged: {
        var oldMinuteHeight = previousEffectiveMinuteHeight
        previousEffectiveMinuteHeight = effectiveMinuteHeight
        Qt.callLater(function() {
            preserveCurrentTimeAnchor(oldMinuteHeight)
        })
    }

    Timer {
        interval: 30000
        repeat: true
        running: true
        onTriggered: {
            var now = new Date()
            root.currentMinute = now.getHours() * 60 + now.getMinutes()
        }
    }

    radius: 8
    color: Theme.canvasBackground
    border.color: Theme.divider
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: headerViewport
            Layout.fillWidth: true
            Layout.preferredHeight: root.headerHeight
            clip: true

            Row {
                x: -flick.contentX
                height: parent.height

                Rectangle {
                    width: root.rulerWidth
                    height: root.headerHeight
                    color: Theme.canvasBackground
                }

                Repeater {
                    model: root.dayCount

                    delegate: Rectangle {
                        width: root.dayColumnWidth
                        height: root.headerHeight
                        color: index === 0 ? Theme.surfaceMuted : Theme.canvasBackground

                        Text {
                            anchors.centerIn: parent
                            text: {
                                var d = new Date(root.anchorDate + "T00:00:00")
                                d.setDate(d.getDate() + index)
                                return root.viewMode === "day"
                                    ? Qt.formatDate(d, LocaleService.locale === "zh_CN" ? "M月d日 ddd" : "ddd, MMM d")
                                    : Qt.formatDate(d, LocaleService.locale === "zh_CN" ? "M月d日 ddd" : "ddd, MMM d")
                            }
                            color: index === 0 ? Theme.primaryText : Theme.secondaryText
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.divider
                        }
                    }
                }
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: root.timelineWidth
            contentHeight: (root.dayEndMinute - root.dayStartMinute) * root.effectiveMinuteHeight + 28
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: true
            flickableDirection: Flickable.HorizontalAndVerticalFlick
            Component.onCompleted: Qt.callLater(function() {
                root.centerCurrentTime()
                root.initialScrollApplied = true
                root.previousEffectiveMinuteHeight = root.effectiveMinuteHeight
            })
            onContentHeightChanged: {
                if (root.initialScrollApplied) {
                    contentY = root.fitFullDay ? 0 : root.clampContentY(contentY)
                }
            }

            Item {
                id: canvas
                width: flick.contentWidth
                height: flick.contentHeight

                Repeater {
                    model: 25

                    delegate: Item {
                        readonly property bool majorTick: index % root.hourLabelStep === 0
                        width: canvas.width
                        height: 1
                        y: index * 60 * root.effectiveMinuteHeight

                        Text {
                            x: 12
                            y: -8
                            visible: majorTick
                            text: String(index).padStart(2, "0") + ":00"
                            color: majorTick ? Theme.tertiaryText : Theme.border
                            font.pixelSize: root.hourLabelStep >= 4 ? 10 : 11
                            opacity: majorTick ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        }

                        Rectangle {
                            x: root.rulerWidth
                            width: canvas.width - root.rulerWidth
                            height: 1
                            color: index === 0 ? Theme.border : (majorTick ? Theme.gridLine : Theme.gridLineSoft)
                            opacity: majorTick ? 1 : 0.54
                            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        }
                    }
                }

                Repeater {
                    model: root.dayCount

                    delegate: Rectangle {
                        x: root.rulerWidth + index * root.dayColumnWidth
                        width: root.dayColumnWidth
                        height: canvas.height
                        color: index % 2 === 0 ? Theme.canvasBackground : Theme.canvasAlternate
                        opacity: 0.62

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.divider
                        }
                    }
                }

                Rectangle {
                    visible: root.currentMinute >= root.dayStartMinute
                        && root.currentMinute <= root.dayEndMinute
                    x: root.rulerWidth
                    y: (root.currentMinute - root.dayStartMinute) * root.effectiveMinuteHeight
                    width: canvas.width - root.rulerWidth
                    height: 2
                    color: Theme.accentBright
                    opacity: 0.9
                    z: 40
                }

                MouseArea {
                    anchors.fill: parent
                    z: 5
                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: false
                    propagateComposedEvents: false
                    preventStealing: false
                    onPressed: ScheduleService.clearSelection()
                    onClicked: ScheduleService.clearSelection()
                }

                Repeater {
                    model: root.model

                    delegate: TimeBlock {
                        id: blockDelegate
                        z: 20
                        visible: !model.hiddenInSpan && (root.viewMode === "week"
                            ? model.dayIndex >= 0 && model.dayIndex < 7
                            : model.dayIndex === 0)
                        blockId: model.id
                        taskId: model.taskId
                        title: model.title
                        subtitle: model.subtitle
                        explanation: model.explanation
                        timeRange: model.timeRange
                        dayIndex: root.viewMode === "week" ? model.dayIndex : 0
                        startMinute: model.startMinute
                        durationMinutes: model.durationMinutes
                        priority: model.priority
                        accentColor: model.color
                        kind: model.kind
                        source: model.source
                        event: model.isEvent
                        locked: model.isLocked
                        eventLocked: model.isEventLocked
                        completed: model.completed
                        canMove: model.canMove && !root.readOnly
                        canResize: model.canResize && !root.readOnly
                        canLock: model.canLock && !root.readOnly
                        selected: model.selected
                        blockOrdinal: model.blockOrdinal
                        blockTotal: model.blockTotal
                        spanDays: model.spanDays || 1
                        dayWidth: root.dayColumnWidth
                        minuteHeight: root.effectiveMinuteHeight
                        timelineLeft: root.rulerWidth
                        horizontalInset: 5
                        dayStartMinute: root.dayStartMinute
                        dayEndMinute: root.dayEndMinute
                        dayCount: root.dayCount
                        x: (root.viewMode === "week"
                            ? (blockDelegate.spanResizeActive && blockDelegate.spanResizeFromLeft
                                ? blockDelegate.spanPreviewVisualStartDayIndex
                                : model.dayIndex)
                            : 0) * root.dayColumnWidth + root.rulerWidth + 5
                        y: ((blockDelegate.resizeActive && blockDelegate.resizeFromTop
                             ? blockDelegate.resizePreviewStartMinute
                             : model.startMinute) - root.dayStartMinute) * root.effectiveMinuteHeight
                        width: Math.max(120, root.dayColumnWidth * (blockDelegate.spanResizeActive
                            ? blockDelegate.spanPreviewVisualDays
                            : (model.spanDays || 1)) - 10)
                        height: Math.max(root.fitFullDay ? 18 : 30, (blockDelegate.resizeActive ? blockDelegate.resizePreviewDuration : model.durationMinutes) * root.effectiveMinuteHeight - 4)
                        onMoveRequested: function(blockId, dayIndex, startMinute, durationMinutes) {
                            var result = ScheduleService.moveTimelineItem(blockId, dayIndex, startMinute, durationMinutes)
                            if (!result || result.ok !== true) {
                                x = (root.viewMode === "week" ? model.dayIndex : 0) * root.dayColumnWidth + root.rulerWidth + 5
                                y = (model.startMinute - root.dayStartMinute) * root.effectiveMinuteHeight
                                root.moveRejected(result && result.message
                                    ? result.message
                                    : qsTr("时间块移动失败"))
                            }
                        }
                        onResizeTimeRequested: function(blockId, startMinute, durationMinutes) {
                            var result = ScheduleService.resizeTimelineItemTime(blockId, startMinute, durationMinutes)
                            if (!result || result.ok !== true) {
                                root.moveRejected(result && result.message
                                    ? result.message
                                    : qsTr("时间块调整失败"))
                            }
                        }
                        onSpanRequested: function(blockId, endDayIndex) {
                            var result = ScheduleService.stretchTimelineItemDays(blockId, endDayIndex)
                            if (!result || result.ok !== true) {
                                root.moveRejected(result && result.message
                                    ? result.message
                                    : qsTr("横向延伸失败"))
                            }
                        }
                        onSpanRangeRequested: function(blockId, startDayIndex, endDayIndex) {
                            var result = ScheduleService.resizeTimelineItemDays(blockId, startDayIndex, endDayIndex)
                            if (!result || result.ok !== true) {
                                root.moveRejected(result && result.message
                                    ? result.message
                                    : qsTr("横向延伸失败"))
                            }
                        }
                        onSelectedItem: function(taskId, blockId) { ScheduleService.selectTimelineItem(taskId, blockId) }
                    }
                }

                Repeater {
                    model: root.model

                    delegate: Item {
                        readonly property bool intersectsNow: root.currentMinute >= model.startMinute
                            && root.currentMinute <= model.startMinute + model.durationMinutes
                        visible: !model.hiddenInSpan && intersectsNow
                            && (root.viewMode === "week"
                                ? model.dayIndex >= 0 && model.dayIndex < 7
                                : model.dayIndex === 0)
                        x: (root.viewMode === "week" ? model.dayIndex : 0) * root.dayColumnWidth
                            + root.rulerWidth + 5
                        y: (root.currentMinute - root.dayStartMinute) * root.effectiveMinuteHeight - 18
                        width: Math.max(120, root.dayColumnWidth * (model.spanDays || 1) - 10)
                        height: 20
                        z: 45

                        Repeater {
                            model: 4

                            delegate: Rectangle {
                                id: particle
                                readonly property real originX: parent.width * (0.14 + index * 0.24)
                                readonly property real drift: index % 2 === 0 ? 3 : -3
                                width: index % 3 === 0 ? 2 : 1.5
                                height: width
                                radius: width / 2
                                x: originX
                                y: 15
                                color: Theme.dark ? "#AFC5FF" : "#365FD9"
                                opacity: 0

                                SequentialAnimation {
                                    running: parent.visible
                                    loops: Animation.Infinite

                                    PauseAnimation { duration: 260 + index * 410 }
                                    ParallelAnimation {
                                        NumberAnimation {
                                            target: particle
                                            property: "y"
                                            from: 15
                                            to: index % 2 === 0 ? 1 : 5
                                            duration: 1250 + index * 90
                                            easing.type: Easing.OutCubic
                                        }
                                        NumberAnimation {
                                            target: particle
                                            property: "x"
                                            from: particle.originX
                                            to: particle.originX + particle.drift
                                            duration: 1250 + index * 90
                                            easing.type: Easing.InOutSine
                                        }
                                        SequentialAnimation {
                                            NumberAnimation {
                                                target: particle
                                                property: "opacity"
                                                from: 0
                                                to: 0.3
                                                duration: 280
                                            }
                                            NumberAnimation {
                                                target: particle
                                                property: "opacity"
                                                to: 0
                                                duration: 970 + index * 90
                                                easing.type: Easing.InCubic
                                            }
                                        }
                                    }
                                    PauseAnimation { duration: 700 }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
