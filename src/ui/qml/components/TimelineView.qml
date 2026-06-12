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

    radius: 8
    color: "#11151D"
    border.color: "#232836"
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
                    color: "#11151D"
                }

                Repeater {
                    model: root.dayCount

                    delegate: Rectangle {
                        width: root.dayColumnWidth
                        height: root.headerHeight
                        color: index === 0 ? "#151A24" : "#11151D"

                        Text {
                            anchors.centerIn: parent
                            text: {
                                var d = new Date()
                                d.setDate(d.getDate() + index)
                                return root.viewMode === "day"
                                    ? qsTr("今天")
                                    : Qt.formatDate(d, LocaleService.locale === "zh_CN" ? "M月d日 ddd" : "ddd, MMM d")
                            }
                            color: index === 0 ? "#E6EAF2" : "#9AA4B2"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: "#202635"
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
            Component.onCompleted: contentY = root.fitFullDay ? 0 : 8 * 60 * root.effectiveMinuteHeight
            onContentHeightChanged: contentY = root.fitFullDay ? 0 : Math.min(contentY, Math.max(0, contentHeight - height))

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
                            color: majorTick ? "#6E7890" : "#3D4557"
                            font.pixelSize: root.hourLabelStep >= 4 ? 10 : 11
                            opacity: majorTick ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        }

                        Rectangle {
                            x: root.rulerWidth
                            width: canvas.width - root.rulerWidth
                            height: 1
                            color: index === 0 ? "#2A3142" : (majorTick ? "#202738" : "#171D29")
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
                        color: index % 2 === 0 ? "#11151D" : "#121720"
                        opacity: 0.62

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: "#202635"
                        }
                    }
                }

                Rectangle {
                    visible: {
                        var now = new Date()
                        var minute = now.getHours() * 60 + now.getMinutes()
                        return minute >= root.dayStartMinute && minute <= root.dayEndMinute
                    }
                    x: root.rulerWidth
                    y: {
                        var now = new Date()
                        return (now.getHours() * 60 + now.getMinutes() - root.dayStartMinute) * root.effectiveMinuteHeight
                    }
                    width: canvas.width - root.rulerWidth
                    height: 2
                    color: "#7C8CFF"
                    opacity: 0.9
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
                        canMove: model.canMove
                        canResize: model.canResize
                        canLock: model.canLock
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
                        x: (root.viewMode === "week" ? model.dayIndex : 0) * root.dayColumnWidth + root.rulerWidth + 5
                        y: ((blockDelegate.resizeActive && blockDelegate.resizeFromTop
                             ? blockDelegate.resizePreviewStartMinute
                             : model.startMinute) - root.dayStartMinute) * root.effectiveMinuteHeight
                        width: Math.max(120, root.dayColumnWidth * (blockDelegate.spanResizeActive
                            ? blockDelegate.spanPreviewVisualDays
                            : (model.spanDays || 1)) - 10)
                        height: Math.max(root.fitFullDay ? 18 : 30, (blockDelegate.resizeActive ? blockDelegate.resizePreviewDuration : model.durationMinutes) * root.effectiveMinuteHeight - 4)
                        onMoveRequested: function(blockId, dayIndex, startMinute, durationMinutes) {
                            var ok = ScheduleService.moveTimelineItem(blockId, dayIndex, startMinute, durationMinutes)
                            if (!ok) {
                                x = (root.viewMode === "week" ? model.dayIndex : 0) * root.dayColumnWidth + root.rulerWidth + 5
                                y = (model.startMinute - root.dayStartMinute) * root.effectiveMinuteHeight
                            }
                        }
                        onResizeTimeRequested: function(blockId, startMinute, durationMinutes) {
                            ScheduleService.resizeTimelineItemTime(blockId, startMinute, durationMinutes)
                        }
                        onSpanRequested: function(blockId, endDayIndex) {
                            ScheduleService.stretchTimelineItemDays(blockId, endDayIndex)
                        }
                        onSelectedItem: function(taskId, blockId) { ScheduleService.selectTimelineItem(taskId, blockId) }
                    }
                }
            }
        }
    }
}
