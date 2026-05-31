import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    property string viewMode: "week"
    property var model
    property var frameModel: []
    property real requestedDayColumnWidth: 190
    readonly property int dayStartMinute: 0
    readonly property int dayEndMinute: 24 * 60
    readonly property int dayCount: viewMode === "week" ? 7 : 1
    property real minuteHeight: 1.18
    readonly property int rulerWidth: 54
    readonly property int headerHeight: 44
    readonly property real dayColumnWidth: viewMode === "week" ? requestedDayColumnWidth : Math.max(320, width - rulerWidth)
    readonly property real timelineWidth: rulerWidth + dayCount * dayColumnWidth

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
            contentHeight: (root.dayEndMinute - root.dayStartMinute) * root.minuteHeight + 28
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: true
            flickableDirection: Flickable.HorizontalAndVerticalFlick
            Component.onCompleted: contentY = 8 * 60 * root.minuteHeight
            onContentHeightChanged: contentY = Math.min(contentY, Math.max(0, contentHeight - height))

            Item {
                id: canvas
                width: flick.contentWidth
                height: flick.contentHeight

                Repeater {
                    model: 25

                    delegate: Item {
                        width: canvas.width
                        height: 1
                        y: index * 60 * root.minuteHeight

                        Text {
                            x: 12
                            y: -8
                            text: String(index).padStart(2, "0") + ":00"
                            color: "#596276"
                            font.pixelSize: 11
                        }

                        Rectangle {
                            x: root.rulerWidth
                            width: canvas.width - root.rulerWidth
                            height: 1
                            color: index === 0 ? "#2A3142" : "#1D2330"
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
                        return (now.getHours() * 60 + now.getMinutes() - root.dayStartMinute) * root.minuteHeight
                    }
                    width: canvas.width - root.rulerWidth
                    height: 2
                    color: "#7C8CFF"
                    opacity: 0.9
                }

                Repeater {
                    model: root.frameModel

                    delegate: Rectangle {
                        visible: modelData.enabled && modelData.dayIndex >= 0 && (
                            root.viewMode === "week"
                                ? modelData.dayIndex < 7
                                : modelData.dayIndex === 0
                        )
                        x: (root.viewMode === "week" ? modelData.dayIndex : 0) * root.dayColumnWidth + root.rulerWidth + 3
                        y: (modelData.startMinute - root.dayStartMinute) * root.minuteHeight
                        width: Math.max(120, root.dayColumnWidth - 6)
                        height: Math.max(24, modelData.durationMinutes * root.minuteHeight)
                        radius: 8
                        color: modelData.color || "#7C8CFF"
                        opacity: 0.12
                        border.width: 1
                        border.color: modelData.color || "#7C8CFF"

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.topMargin: 6
                            text: (modelData.name || "Study Frame") + (modelData.categoryName ? " · " + modelData.categoryName : "")
                            color: "#C6CFFF"
                            opacity: 0.74
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                    }
                }

                Repeater {
                    model: root.model

                    delegate: TimeBlock {
                        id: blockDelegate
                        visible: root.viewMode === "week"
                            ? model.dayIndex >= 0 && model.dayIndex < 7
                            : model.dayIndex === 0
                        blockId: model.id
                        taskId: model.taskId
                        title: model.title
                        subtitle: model.subtitle
                        timeRange: model.timeRange
                        dayIndex: root.viewMode === "week" ? model.dayIndex : 0
                        startMinute: model.startMinute
                        durationMinutes: model.durationMinutes
                        priority: model.priority
                        accentColor: model.color
                        event: model.isEvent
                        locked: model.isLocked
                        eventLocked: model.isEventLocked
                        selected: model.selected
                        blockOrdinal: model.blockOrdinal
                        blockTotal: model.blockTotal
                        dayWidth: root.dayColumnWidth
                        minuteHeight: root.minuteHeight
                        timelineLeft: root.rulerWidth
                        horizontalInset: 5
                        dayStartMinute: root.dayStartMinute
                        dayEndMinute: root.dayEndMinute
                        dayCount: root.dayCount
                        x: (root.viewMode === "week" ? model.dayIndex : 0) * root.dayColumnWidth + root.rulerWidth + 5
                        y: (model.startMinute - root.dayStartMinute) * root.minuteHeight
                        width: Math.max(120, root.dayColumnWidth - 10)
                        height: Math.max(30, (blockDelegate.resizeActive ? blockDelegate.resizePreviewDuration : model.durationMinutes) * root.minuteHeight - 4)
                        onMoveRequested: function(blockId, dayIndex, startMinute, durationMinutes) {
                            var ok = ScheduleService.moveTimelineItem(blockId, model.isEvent, dayIndex, startMinute, durationMinutes)
                            if (!ok) {
                                x = (root.viewMode === "week" ? model.dayIndex : 0) * root.dayColumnWidth + root.rulerWidth + 5
                                y = (model.startMinute - root.dayStartMinute) * root.minuteHeight
                            }
                        }
                        onSelectedItem: function(taskId, blockId) { ScheduleService.selectTimelineItem(taskId, blockId) }
                    }
                }
            }
        }
    }
}
