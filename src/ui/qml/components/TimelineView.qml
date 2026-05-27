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
                    model: root.model

                    delegate: TimeBlock {
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
                        height: Math.max(30, model.durationMinutes * root.minuteHeight - 4)
                        onMoveRequested: function(blockId, dayIndex, startMinute, durationMinutes) {
                            var ok = ScheduleService.moveTimelineItem(blockId, model.isEvent, dayIndex, startMinute, durationMinutes)
                            if (!ok) {
                                x = (root.viewMode === "week" ? model.dayIndex : 0) * root.dayColumnWidth + root.rulerWidth + 5
                                y = (model.startMinute - root.dayStartMinute) * root.minuteHeight
                            }
                        }
                        onSelectedTask: function(taskId) { ScheduleService.selectTask(taskId) }
                        onEventSettingsRequested: function(eventId, locked) {
                            eventEditor.eventId = eventId
                            eventEditor.locked = locked
                            eventEditor.open()
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: eventEditor
        property int eventId: 0
        property bool locked: true
        width: 292
        height: 164
        modal: true
        focus: true
        anchors.centerIn: parent
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 8
            color: "#161A23"
            border.width: 1
            border.color: "#30384C"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("固定时间")
                        color: "#E6EAF2"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("取消固定后可在时间轴中拖动")
                        color: "#8C96AA"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    width: 48
                    height: 28
                    radius: 14
                    color: lockMouse.containsMouse ? "#30384C" : eventEditor.locked ? "#7C8CFF" : "#2A3142"
                    border.width: 1
                    border.color: eventEditor.locked ? "#9EA8FF" : "#455066"

                    Behavior on color { ColorAnimation { duration: 140 } }

                    Rectangle {
                        width: 20
                        height: 20
                        radius: 10
                        y: 4
                        x: eventEditor.locked ? 24 : 4
                        color: "white"
                        Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    }

                    MouseArea {
                        id: lockMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: eventEditor.locked = !eventEditor.locked
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }

                SmallButton {
                    text: qsTr("取消")
                    muted: true
                    onClicked: eventEditor.close()
                }

                SmallButton {
                    text: qsTr("保存")
                    onClicked: {
                        ScheduleService.setEventLocked(eventEditor.eventId, eventEditor.locked)
                        eventEditor.close()
                    }
                }
            }
        }
    }

    component SmallButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        signal clicked()

        width: 72
        height: 36
        radius: 8
        color: muted ? (mouse.containsMouse ? "#202638" : "#161A23") : (mouse.containsMouse ? "#8B99FF" : "#7C8CFF")
        border.width: muted ? 1 : 0
        border.color: "#30384C"

        Text {
            id: label
            anchors.centerIn: parent
            color: muted ? "#AAB4C6" : "white"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.clicked()
        }
    }
}
