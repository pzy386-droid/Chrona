import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    color: Theme.appBackground

    property int filterIndex: 0
    property var reminders: []
    property string statusText: ""
    readonly property color ddlAccent: "#F59E0B"
    readonly property color ddlAccentHover: "#FBBF24"
    readonly property color ddlAccentText: Theme.dark ? "#FFD58A" : "#92400E"
    readonly property color ddlAccentSurface: Theme.dark ? "#2A2114" : "#FFF7E6"
    readonly property color ddlAccentBorder: "#F59E0B"
    readonly property color dangerText: Theme.dark ? "#FFB4B4" : "#B42318"
    readonly property color dangerSurface: Theme.dark ? "#3A1517" : "#FDEDEA"
    readonly property color dangerBorder: Theme.dark ? "#EF4444" : "#DE7565"

    function refresh() {
        var source = ScheduleService.deadlineReminders
        var next = []
        for (var i = 0; i < source.length; ++i) {
            var item = source[i]
            if (filterIndex === 0 && item.status !== 0)
                continue
            if (filterIndex === 1 && (!item.reminderDue || item.status !== 0))
                continue
            if (filterIndex === 2 && item.status !== 1)
                continue
            next.push(item)
        }
        reminders = next
    }

    function addReminder() {
        var result = ScheduleService.addDeadlineReminder(
            titleField.text,
            dueField.text,
            notesField.text,
            categoryField.text,
            remindDays.value
        )
        statusText = result.message || ""
        if (result.ok) {
            titleField.text = ""
            notesField.text = ""
            categoryField.text = "DDL"
            refresh()
        }
    }

    Component.onCompleted: refresh()

    Connections {
        target: ScheduleService
        function onDataChanged() { root.refresh() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: qsTr("DDL 提醒中心")
                    color: Theme.strongText
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: ScheduleService.urgentDeadlineCount > 0
                          ? qsTr("%1 个截止事项需要留意").arg(ScheduleService.urgentDeadlineCount)
                          : qsTr("暂无临近截止提醒")
                    color: ScheduleService.urgentDeadlineCount > 0 ? root.ddlAccentText : Theme.secondaryText
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                spacing: 8
                Repeater {
                    model: [qsTr("未完成"), qsTr("近期提醒"), qsTr("已完成")]
                    delegate: Rectangle {
                        Layout.preferredWidth: 88
                        Layout.preferredHeight: 36
                        radius: 8
                        color: root.filterIndex === index ? root.ddlAccentSurface : Theme.surfaceElevated
                        border.width: 1
                        border.color: root.filterIndex === index ? root.ddlAccentBorder : Theme.border

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: root.filterIndex === index ? root.ddlAccentText : Theme.secondaryText
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.filterIndex = index
                                root.refresh()
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 142
            radius: 8
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Field {
                        id: titleField
                        Layout.fillWidth: true
                        placeholderText: qsTr("慕课结课")
                    }

                    Field {
                        id: dueField
                        Layout.preferredWidth: 178
                        text: Qt.formatDate(new Date(), "yyyy-MM-dd") + " 23:59"
                        placeholderText: qsTr("yyyy-MM-dd HH:mm")
                    }

                    Field {
                        id: categoryField
                        Layout.preferredWidth: 120
                        text: "DDL"
                        placeholderText: qsTr("分类")
                    }

                    SpinBox {
                        id: remindDays
                        Layout.preferredWidth: 118
                        from: 0
                        to: 365
                        value: 3
                        editable: true
                        contentItem: TextInput {
                            text: remindDays.textFromValue(remindDays.value, remindDays.locale)
                            color: Theme.primaryText
                            font.pixelSize: 13
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                            validator: remindDays.validator
                        }
                        background: Rectangle {
                            radius: 8
                            color: Theme.inputBackground
                            border.width: 1
                            border.color: Theme.border
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 82
                        Layout.preferredHeight: 38
                        radius: 8
                        color: addMouse.containsMouse ? root.ddlAccentHover : root.ddlAccent

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("添加")
                            color: "#111827"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            id: addMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.addReminder()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Field {
                        id: notesField
                        Layout.fillWidth: true
                        placeholderText: qsTr("备注")
                    }

                    Text {
                        Layout.preferredWidth: 220
                        text: root.statusText
                        color: Theme.secondaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: root.reminders

            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 112
                radius: 8
                color: hover.hovered ? Theme.surfaceHover : Theme.surface
                border.width: 1
                border.color: modelData.overdue ? root.dangerBorder : modelData.reminderDue ? root.ddlAccentBorder : Theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    Rectangle {
                        Layout.preferredWidth: 5
                        Layout.fillHeight: true
                        radius: 3
                        color: modelData.overdue ? root.dangerBorder : modelData.categoryColor
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 7

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                color: modelData.status === 1 ? Theme.secondaryText : Theme.strongText
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.preferredWidth: Math.max(68, urgencyText.implicitWidth + 18)
                                Layout.preferredHeight: 24
                                radius: 8
                                color: modelData.overdue ? root.dangerSurface : modelData.reminderDue ? root.ddlAccentSurface : Theme.surfaceMuted
                                border.width: 1
                                border.color: modelData.overdue ? root.dangerBorder : modelData.reminderDue ? root.ddlAccentBorder : Theme.border

                                Text {
                                    id: urgencyText
                                    anchors.centerIn: parent
                                    text: modelData.overdue ? qsTr("已逾期") : modelData.daysLeft === 0 ? qsTr("今天") : qsTr("%1 天").arg(modelData.daysLeft)
                                    color: modelData.overdue ? root.dangerText : modelData.reminderDue ? root.ddlAccentText : Theme.secondaryText
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.dueText + "  ·  " + modelData.categoryName + "  ·  " + qsTr("提前 %1 天提醒").arg(modelData.remindDaysBefore)
                            color: Theme.secondaryText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.notes
                            visible: modelData.notes.length > 0
                            color: Theme.mutedText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        spacing: 8

                        ActionButton {
                            visible: modelData.status === 0
                            label: qsTr("完成")
                            colorValue: Theme.successSurface
                            borderValue: Theme.successBorder
                            textValue: Theme.success
                            onClicked: ScheduleService.completeDeadlineReminder(modelData.id)
                        }

                        ActionButton {
                            visible: modelData.status === 0
                            label: qsTr("归档")
                            colorValue: Theme.surfaceMuted
                            borderValue: Theme.border
                            textValue: Theme.primaryText
                            onClicked: ScheduleService.archiveDeadlineReminder(modelData.id)
                        }

                        ActionButton {
                            label: qsTr("删除")
                            colorValue: Theme.dangerSurface
                            borderValue: Theme.dangerBorder
                            textValue: Theme.error
                            onClicked: ScheduleService.deleteDeadlineReminder(modelData.id)
                        }
                    }
                }

                HoverHandler { id: hover }
            }

            Rectangle {
                anchors.centerIn: parent
                visible: list.count === 0
                width: 320
                height: 72
                radius: 8
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Text {
                    anchors.centerIn: parent
                    text: qsTr("这里还没有 DDL")
                    color: Theme.secondaryText
                    font.pixelSize: 14
                }
            }
        }
    }

    component Field: TextField {
        id: field
        color: Theme.primaryText
        placeholderTextColor: Theme.mutedText
        font.pixelSize: 14
        selectedTextColor: "#111827"
        selectionColor: root.ddlAccent
        background: Rectangle {
            radius: 8
            color: Theme.inputBackground
            border.width: 1
            border.color: field.activeFocus ? root.ddlAccentBorder : Theme.border
        }
    }

    component ActionButton: Rectangle {
        id: button
        property string label: ""
        property color colorValue: Theme.surfaceMuted
        property color borderValue: Theme.border
        property color textValue: Theme.primaryText
        signal clicked()

        Layout.preferredWidth: 56
        Layout.preferredHeight: 34
        radius: 8
        color: buttonMouse.containsMouse ? Qt.lighter(colorValue, 1.25) : colorValue
        border.width: 1
        border.color: borderValue

        Text {
            anchors.centerIn: parent
            text: button.label
            color: textValue
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }
}
