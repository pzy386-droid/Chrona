import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    property var task: ({})
    property string editStartTime: "09:00"
    property string editEndTime: "10:30"
    readonly property bool hasTask: task && task.id
    readonly property bool isEvent: hasTask && task.kind === "event"
    readonly property var preferredValues: ["morning", "afternoon", "evening"]
    readonly property var energyValues: ["low", "medium", "high"]
    readonly property var minChunkValues: [30, 45, 60]
    readonly property var idealChunkValues: [60, 90, 120]
    signal closeRequested()

    function nearestIndex(values, value) {
        var best = 0
        var bestDistance = Math.abs(values[0] - value)
        for (var i = 1; i < values.length; ++i) {
            var distance = Math.abs(values[i] - value)
            if (distance < bestDistance) {
                best = i
                bestDistance = distance
            }
        }
        return best
    }

    function syncFields() {
        if (!hasTask) {
            return
        }
        titleField.text = task.title || ""
        notesField.text = task.notes || ""
        deadlineField.text = task.deadlineText || ""
        editStartTime = task.blockStartText || "09:00"
        editEndTime = task.blockEndText || "10:30"
        dayPicker.currentIndex = Math.max(0, Math.min(6, task.blockDayIndex || 0))
        priorityPicker.currentIndex = Math.max(0, Math.min(2, task.priority || 0))
        categoryField.text = task.categoryName || ""
        var preferred = task.preferredStudyTime || "evening"
        preferredPicker.currentIndex = Math.max(0, preferredValues.indexOf(preferred))
        autoScheduleToggle.checked = task.autoScheduleEnabled !== false
        deadlineTypePicker.currentIndex = Math.max(0, Math.min(1, task.deadlineType || 0))
        minChunkPicker.currentIndex = nearestIndex(minChunkValues, task.minChunkMinutes || 30)
        idealChunkPicker.currentIndex = nearestIndex(idealChunkValues, task.idealChunkMinutes || 90)
        effortPicker.currentIndex = Math.max(0, Math.min(2, task.effortLevel || 1))
        eventLockToggle.checked = task.isLocked === true
        statusText.text = ""
    }

    color: "#11151D"
    border.width: 1
    border.color: "#232836"

    onTaskChanged: syncFields()
    Component.onCompleted: syncFields()

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 22
        anchors.bottomMargin: 18
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: root.isEvent ? qsTr("Fixed Time Details") : qsTr("Task Details")
                color: "#AAB4C6"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            RowLayout {
                visible: root.hasTask
                spacing: 7

                Text {
                    text: qsTr("Lock current block")
                    color: "#8D98AB"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                ToggleSwitch {
                    id: eventLockToggle
                    checked: true
                    onToggled: function(checked) {
                        var result = root.isEvent
                            ? { ok: ScheduleService.setEventLocked(root.task.id, checked), message: checked ? qsTr("Current fixed time locked") : qsTr("Current fixed time unlocked") }
                            : ScheduleService.setSelectedBlockLocked(checked)
                        statusText.color = result.ok ? "#A9F0C9" : "#FFB09B"
                        statusText.text = result.message || ""
                    }
                }
            }

            Rectangle {
                width: 32
                height: 32
                radius: 8
                color: closeMouse.containsMouse ? "#202638" : "#151A23"
                border.width: 1
                border.color: "#2C3548"

                Text {
                    anchors.centerIn: parent
                    text: "X"
                    color: "#AAB4C6"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.closeRequested()
                }
            }

            Rectangle {
                visible: root.hasTask
                width: 10
                height: 10
                radius: 5
                color: root.task.categoryColor || "#7C8CFF"
                Behavior on color { ColorAnimation { duration: 180 } }
            }

            ActionButton {
                Layout.preferredWidth: 86
                text: "Frames"
                muted: true
                onClicked: studyFramePopup.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 5
            radius: 3
            color: root.hasTask ? (root.task.categoryColor || "#7C8CFF") : "#252B3A"
            Behavior on color { ColorAnimation { duration: 200 } }
        }

        ColumnLayout {
            visible: !root.hasTask
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                text: qsTr("Select a time block")
                color: "#E6EAF2"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Select a time block on the timeline to edit its task and current block here.")
                color: "#8C96AA"
                font.pixelSize: 13
                lineHeight: 1.35
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            Item { Layout.fillHeight: true }
        }

        Flickable {
            visible: root.hasTask
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: formColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: formColumn
                width: parent.width
                spacing: 13

                FieldLabel { text: root.isEvent ? qsTr("Title") : qsTr("Task title") }
                TextField {
                    id: titleField
                    Layout.fillWidth: true
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: qsTr("For example: Calculus midterm review")
                    placeholderTextColor: "#667187"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    background: FieldBackground {}
                }

                FieldLabel {
                    visible: !root.isEvent
                    text: qsTr("Notes")
                }
                TextArea {
                    id: notesField
                    visible: !root.isEvent
                    Layout.fillWidth: true
                    Layout.preferredHeight: 88
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: qsTr("Notes, chapters, resources, or completion criteria")
                    placeholderTextColor: "#667187"
                    font.pixelSize: 13
                    wrapMode: TextEdit.WordWrap
                    background: FieldBackground {}
                }

                FieldLabel {
                    visible: !root.isEvent
                    text: qsTr("Deadline")
                }
                TextField {
                    id: deadlineField
                    visible: !root.isEvent
                    Layout.fillWidth: true
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: "2026-05-30 23:00"
                    placeholderTextColor: "#667187"
                    font.pixelSize: 13
                    background: FieldBackground {}
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    FieldLabel {
                        Layout.fillWidth: true
                        text: qsTr("Current time block")
                    }

                    Text {
                        visible: !root.isEvent && (root.task.blockTotal || 0) > 1
                        text: qsTr("Block %1 / %2").arg(root.task.blockOrdinal || 1).arg(root.task.blockTotal || 1)
                        color: "#7C8CFF"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }

                OptionPills {
                    id: dayPicker
                    Layout.fillWidth: true
                    options: [qsTr("Today"), qsTr("Tomorrow"), qsTr("+2"), qsTr("+3"), qsTr("+4"), qsTr("+5"), qsTr("+6")]
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    TimeSelectButton {
                        Layout.fillWidth: true
                        value: root.editStartTime || "09:00"
                        onClicked: startWheel.openWith(root.editStartTime)
                    }

                    Text {
                        text: "-"
                        color: "#667187"
                        font.pixelSize: 16
                    }

                    TimeSelectButton {
                        Layout.fillWidth: true
                        value: root.editEndTime || "10:30"
                        onClicked: endWheel.openWith(root.editEndTime)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: !root.isEvent
                        FieldLabel { text: qsTr("Priority") }
                        OptionPills {
                            id: priorityPicker
                            Layout.fillWidth: true
                            options: [qsTr("Low"), qsTr("Medium"), qsTr("High")]
                        }
                    }
                }

                FieldLabel { text: qsTr("Category") }
                TextField {
                    id: categoryField
                    Layout.fillWidth: true
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: qsTr("Study / Course / Meal")
                    placeholderTextColor: "#667187"
                    font.pixelSize: 13
                    background: FieldBackground {}
                }

                FieldLabel {
                    visible: !root.isEvent
                    text: qsTr("Preferred time")
                }
                OptionPills {
                    id: preferredPicker
                    visible: !root.isEvent
                    Layout.fillWidth: true
                    options: [qsTr("Morning"), qsTr("Afternoon"), qsTr("Evening")]
                }

                FieldLabel {
                    visible: !root.isEvent
                    text: qsTr("Scheduler behavior")
                }
                Rectangle {
                    visible: !root.isEvent
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: 8
                    color: "#151A23"
                    border.width: 1
                    border.color: "#2C3548"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Allow scheduler to adjust this task")
                                color: "#E6EAF2"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: qsTr("When off, only manually dragged time blocks are kept")
                                color: "#7F8A9E"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }

                        ToggleSwitch {
                            id: autoScheduleToggle
                            checked: true
                        }
                    }
                }

                RowLayout {
                    visible: !root.isEvent
                    Layout.fillWidth: true
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("Deadline type") }
                        OptionPills {
                            id: deadlineTypePicker
                            Layout.fillWidth: true
                            options: [qsTr("Hard deadline"), qsTr("Soft deadline")]
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("Effort") }
                        OptionPills {
                            id: effortPicker
                            Layout.fillWidth: true
                            options: [qsTr("Light"), qsTr("Medium"), qsTr("Heavy")]
                        }
                    }
                }

                RowLayout {
                    visible: !root.isEvent
                    Layout.fillWidth: true
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("Duration") }
                        OptionPills {
                            id: minChunkPicker
                            Layout.fillWidth: true
                            options: [qsTr("30"), qsTr("45"), qsTr("60")]
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("Ideal block") }
                        OptionPills {
                            id: idealChunkPicker
                            Layout.fillWidth: true
                            options: [qsTr("60"), qsTr("90"), qsTr("120")]
                        }
                    }
                }

                Text {
                    id: statusText
                    Layout.fillWidth: true
                    color: "#A9F0C9"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            visible: root.hasTask && !root.isEvent
            Layout.fillWidth: true
            spacing: 10

            ActionButton {
                Layout.fillWidth: true
                text: qsTr("Delete")
                danger: true
                onClicked: {
                    var result = ScheduleService.deleteTask(root.task.id)
                    statusText.text = result.message || ""
                }
            }
        }

        ActionButton {
            visible: root.hasTask
            Layout.fillWidth: true
            text: root.isEvent ? qsTr("Save fixed time") : qsTr("Save task")
            onClicked: {
                var savedTitle = titleField.text
                var savedNotes = notesField.text
                var savedDeadline = deadlineField.text
                var savedCategory = categoryField.text
                var savedDayIndex = dayPicker.currentIndex
                var savedStartTime = root.editStartTime
                var savedEndTime = root.editEndTime
                var savedPriority = priorityPicker.currentIndex
                var savedPreferred = root.preferredValues[preferredPicker.currentIndex]
                var savedAutoSchedule = autoScheduleToggle.checked
                var savedDeadlineType = deadlineTypePicker.currentIndex
                var savedMinChunk = root.minChunkValues[minChunkPicker.currentIndex]
                var savedIdealChunk = root.idealChunkValues[idealChunkPicker.currentIndex]
                var savedEffort = effortPicker.currentIndex
                var savedEventLocked = eventLockToggle.checked

                var startParts = String(savedStartTime).split(":")
                var endParts = String(savedEndTime).split(":")
                var startMinute = Number(startParts[0]) * 60 + Number(startParts[1])
                var endMinute = Number(endParts[0]) * 60 + Number(endParts[1])
                if (startParts.length !== 2 || endParts.length !== 2 || isNaN(startMinute) || isNaN(endMinute) || endMinute <= startMinute) {
                    statusText.color = "#FFB09B"
                    statusText.text = qsTr("End time must be later than start time")
                    return
                }

                if (root.isEvent) {
                    var eventResult = ScheduleService.updateSelectedEvent(
                        savedTitle,
                        savedDayIndex,
                        savedStartTime,
                        savedEndTime,
                        savedEventLocked,
                        savedCategory
                    )
                    statusText.color = eventResult.ok ? "#A9F0C9" : "#FFB09B"
                    statusText.text = eventResult.message || ""
                    return
                }

                var moveResult = ScheduleService.moveSelectedTaskBlock(savedDayIndex, savedStartTime, savedEndTime)
                if (!moveResult.ok) {
                    statusText.color = "#FFB09B"
                    statusText.text = moveResult.message || qsTr("Could not move this block")
                    return
                }

                var lockResult = ScheduleService.setSelectedBlockLocked(savedEventLocked)
                if (!lockResult.ok) {
                    statusText.color = "#FFB09B"
                    statusText.text = lockResult.message || qsTr("Failed to save lock state")
                    return
                }

                var updateResult = ScheduleService.updateTask(
                    root.task.id,
                    savedTitle,
                    savedNotes,
                    savedDeadline,
                    root.task.estimatedMinutes || 60,
                    savedPriority,
                    savedCategory,
                    savedPreferred,
                    savedAutoSchedule,
                    savedDeadlineType,
                    savedMinChunk,
                    savedIdealChunk,
                    savedEffort
                )
                statusText.color = updateResult.ok ? "#A9F0C9" : "#FFB09B"
                statusText.text = updateResult.ok ? qsTr("Saved and moved time block") : (updateResult.message || qsTr("Save failed"))
            }
        }
    }

    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    TimeWheelPicker {
        id: startWheel
        title: qsTr("Select start time")
        onAccepted: function(value) { root.editStartTime = value }
    }

    TimeWheelPicker {
        id: endWheel
        title: qsTr("Select end time")
        onAccepted: function(value) { root.editEndTime = value }
    }

    Popup {
        id: studyFramePopup
        width: 316
        height: Math.min(620, root.height - 44)
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
            anchors.margins: 16
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: "Study Frames"
                color: "#E6EAF2"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            TextField {
                id: frameNameField
                Layout.fillWidth: true
                text: "Deep Study"
                color: "#E6EAF2"
                selectedTextColor: "white"
                selectionColor: "#5968D8"
                placeholderText: "Frame name"
                placeholderTextColor: "#667187"
                font.pixelSize: 13
                background: FieldBackground {}
            }

            OptionPills {
                id: frameDayPicker
                Layout.fillWidth: true
                options: ["Today", "Tomorrow", "+2", "+3", "+4", "+5", "+6"]
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                TextField {
                    id: frameStartField
                    Layout.fillWidth: true
                    text: "19:00"
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    font.pixelSize: 13
                    background: FieldBackground {}
                }

                TextField {
                    id: frameEndField
                    Layout.fillWidth: true
                    text: "22:00"
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    font.pixelSize: 13
                    background: FieldBackground {}
                }
            }

            TextField {
                id: frameCategoryField
                Layout.fillWidth: true
                text: root.hasTask ? (root.task.categoryName || "") : ""
                color: "#E6EAF2"
                selectedTextColor: "white"
                selectionColor: "#5968D8"
                placeholderText: qsTr("Study / Math / English")
                placeholderTextColor: "#667187"
                font.pixelSize: 13
                background: FieldBackground {}
            }

            OptionPills {
                id: frameEnergyPicker
                Layout.fillWidth: true
                currentIndex: 1
                options: ["Low", "Medium", "High"]
            }

            Text {
                id: frameStatusText
                Layout.fillWidth: true
                text: ""
                color: "#A9F0C9"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            ActionButton {
                Layout.fillWidth: true
                text: "Add Frame"
                onClicked: {
                    var result = ScheduleService.addStudyFrame(
                        frameNameField.text,
                        frameDayPicker.currentIndex,
                        frameStartField.text,
                        frameEndField.text,
                        frameCategoryField.text,
                        root.energyValues[frameEnergyPicker.currentIndex]
                    )
                    frameStatusText.color = result.ok ? "#A9F0C9" : "#FFB09B"
                    frameStatusText.text = result.message || ""
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: ScheduleService.studyFrames

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 74
                    radius: 8
                    color: "#11151D"
                    border.width: 1
                    border.color: modelData.enabled ? "#30384C" : "#252B35"
                    opacity: modelData.enabled ? 1 : 0.58

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Rectangle {
                            width: 5
                            Layout.fillHeight: true
                            radius: 3
                            color: modelData.color || "#7C8CFF"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name || "Study Frame"
                                color: "#E6EAF2"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: (modelData.startText || "") + " - " + (modelData.endText || "") + (modelData.categoryName ? " - " + modelData.categoryName : "")
                                color: "#8C96AA"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            width: 34
                            height: 26
                            radius: 7
                            color: enableMouse.containsMouse ? "#252D44" : "#1A2030"
                            border.width: 1
                            border.color: modelData.enabled ? "#7C8CFF" : "#3A4254"
                            Text {
                                anchors.centerIn: parent
                                text: modelData.enabled ? "On" : "Off"
                                color: modelData.enabled ? "#C6CFFF" : "#8C96AA"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }
                            MouseArea {
                                id: enableMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: ScheduleService.setStudyFrameEnabled(modelData.id, !modelData.enabled)
                            }
                        }

                        Rectangle {
                            width: 26
                            height: 26
                            radius: 7
                            color: deleteMouse.containsMouse ? "#3A211E" : "#1A2030"
                            border.width: 1
                            border.color: "#53332F"
                            Text {
                                anchors.centerIn: parent
                                text: "X"
                                color: "#FFB09B"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                            MouseArea {
                                id: deleteMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: ScheduleService.deleteStudyFrame(modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }

    component FieldLabel: Text {
        color: "#8D98AB"
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    component FieldBackground: Rectangle {
        radius: 8
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"
    }

    component OptionPills: Rectangle {
        id: pills
        property var options: []
        property int currentIndex: 0

        Layout.preferredHeight: 42
        radius: 8
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4

            Repeater {
                model: pills.options
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 7
                    color: pills.currentIndex === index ? "#252D44" : "transparent"
                    border.width: pills.currentIndex === index ? 1 : 0
                    border.color: "#7C8CFF"
                    Behavior on color { ColorAnimation { duration: 140 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: pills.currentIndex === index ? "#E6EAF2" : "#8D98AB"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: pills.currentIndex = index
                    }
                }
            }
        }
    }

    component TimeSelectButton: Rectangle {
        id: timeButton
        property string value: "09:00"
        signal clicked()

        Layout.preferredHeight: 44
        radius: 10
        color: mouse.containsMouse ? "#1A2030" : "#151A23"
        border.width: 1
        border.color: mouse.containsMouse ? "#53607C" : "#2C3548"

        Behavior on color { ColorAnimation { duration: 140 } }
        Behavior on border.color { ColorAnimation { duration: 140 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: timeButton.value
                color: "#E6EAF2"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Text {
                text: "v"
                color: "#7C8CFF"
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: timeButton.clicked()
        }
    }

    component ToggleSwitch: Rectangle {
        id: toggle
        property bool checked: false
        signal toggled(bool checked)

        Layout.preferredWidth: 48
        Layout.preferredHeight: 28
        radius: 14
        color: toggle.checked ? "#7C8CFF" : "#273044"
        border.width: 1
        border.color: toggle.checked ? "#96A2FF" : "#3A445A"

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Rectangle {
            width: 22
            height: 22
            radius: 11
            x: toggle.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            color: "#F3F5FF"
            Behavior on x { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                toggle.checked = !toggle.checked
                toggle.toggled(toggle.checked)
            }
        }
    }

    component ActionButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        property bool danger: false
        signal clicked()

        Layout.preferredHeight: 42
        radius: 8
        color: danger
            ? (mouse.containsMouse ? "#3A211E" : "#2A1D1B")
            : muted ? (mouse.containsMouse ? "#202638" : "#161A23")
            : (mouse.containsMouse ? "#8B99FF" : "#7C8CFF")
        border.width: muted || danger ? 1 : 0
        border.color: danger ? "#FF7A59" : "#30384C"
        Behavior on color { ColorAnimation { duration: 140 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: button.danger ? "#FFB09B" : button.muted ? "#AAB4C6" : "white"
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
