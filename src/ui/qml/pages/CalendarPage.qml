import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Item {
    id: root
    property int leftPadding: 24
    property int rightPadding: 20
    property string viewMode: "week"
    property real timelineMinuteHeight: 1.18
    property real timelineColumnWidth: 190
    property bool focusActive: false
    property int focusDurationSeconds: 25 * 60
    property int focusRemainingSeconds: focusDurationSeconds
    property var aiDraft: ({})

    function formatFocusTime(seconds) {
        var m = Math.floor(seconds / 60)
        var s = seconds % 60
        return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0")
    }

    function endFocus(completeTask) {
        focusExitConfirmPopup.close()
        if (completeTask && ScheduleService.focusItem.taskId > 0) {
            ScheduleService.completeTask(ScheduleService.focusItem.taskId)
        } else {
            ScheduleService.stopFocus()
        }
        root.focusActive = false
        focusPanel.active = false
        focusTimer.stop()
    }

    function requestEndFocus() {
        if (root.focusActive) {
            focusExitConfirmPopup.open()
        }
    }

    function submitAiDraft() {
        var draft = {
            title: aiDraftTitle.text,
            notes: aiDraftNotes.text,
            deadline: aiDraftDeadline.text,
            estimatedMinutes: aiDraftDuration.value,
            priority: aiDraftPriority.selectedIndex,
            categoryName: aiDraftCategory.text,
            preferredStudyTime: "evening",
            source: root.aiDraft.source || "local",
            explanation: aiDraftReason.text
        }
        var result = ScheduleService.createTaskFromDraft(draft)
        aiDraftPopup.close()
        quickAddToast.kind = result && result.ok ? "success" : "danger"
        quickAddToast.text = result && result.message ? result.message : qsTr("Handled")
        quickAddToast.open()
    }

    Timer {
        id: focusTimer
        interval: 1000
        repeat: true
        running: root.focusActive
        onTriggered: {
            if (root.focusRemainingSeconds > 0) {
                root.focusRemainingSeconds--
            } else {
                root.endFocus(false)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding
        anchors.topMargin: 22
        anchors.bottomMargin: 18
        spacing: 18

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120

            radius: 18

            color: "#161B22"

            border.width: 1
            border.color: "#2A3140"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: qsTr("Welcome back")
                        color: "#FFFFFF"
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Text {
                        text: qsTr("Chrona has generated a study plan for today")
                        color: "#9AA4B2"
                        font.pixelSize: 13
                    }

                    Text {
                        text: Qt.formatDate(new Date(), "yyyy-MM-dd")
                        color: "#677184"
                        font.pixelSize: 11
                    }
                }

                Rectangle {
                    width: 100
                    height: 72
                    radius: 12

                    color: "#202638"

                    Column {
                        anchors.centerIn: parent

                        Text {
                            text: ScheduleService.taskModel.count
                            color: "white"
                            font.pixelSize: 24
                            font.bold: true
                        }

                        Text {
                            text: qsTr("Tasks")
                            color: "#9AA4B2"
                            font.pixelSize: 11
                        }
                    }
                }

                Rectangle {
                    width: 100
                    height: 72
                    radius: 12

                    color: "#202638"

                    Column {
                        anchors.centerIn: parent

                        Text {
                            text: ScheduleService.unscheduledCount
                            color: "#FFB547"
                            font.pixelSize: 24
                            font.bold: true
                        }

                        Text {
                            text: qsTr("Unscheduled")
                            color: "#9AA4B2"
                            font.pixelSize: 11
                        }
                    }
                }

                DensityControl {
                    label: qsTr("Row spacing")
                    value: root.timelineMinuteHeight
                    from: 0.85
                    to: 1.75
                    onValueChangedByUser: function(v) {
                        root.timelineMinuteHeight = v
                    }
                    valueLabel: root.timelineMinuteHeight < 1.08
                                ? qsTr("Tight")
                                : root.timelineMinuteHeight > 1.42
                                  ? qsTr("Loose")
                                  : qsTr("Normal")
                }

                DensityControl {
                    label: qsTr("Column width")
                    value: root.timelineColumnWidth
                    from: 140
                    to: 260
                    onValueChangedByUser: function(v) {
                        root.timelineColumnWidth = v
                    }
                    valueLabel: root.timelineColumnWidth < 170
                                ? qsTr("Tight")
                                : root.timelineColumnWidth > 225
                                  ? qsTr("Loose")
                                  : qsTr("Normal")
                }

                SegmentedControl {
                    options: [qsTr("Day"), qsTr("Week")]
                    selectedIndex: root.viewMode === "day" ? 0 : 1
                    onSelectedIndexChanged: {
                        root.viewMode = selectedIndex === 0
                                        ? "day"
                                        : "week"
                    }
                }

                LanguageToggle {
                    currentLocale: LocaleService.locale
                    onLocaleRequested: function(locale) {
                        LocaleService.setLocale(locale)
                    }
                }
            }
        }

        CurrentFocusPanel {
            id: focusPanel
            Layout.fillWidth: true
            focusItem: ScheduleService.focusItem
            unscheduledCount: ScheduleService.unscheduledCount
            scheduleIssues: ScheduleService.scheduleIssues
            onFocusStarted: {
                root.focusRemainingSeconds = root.focusDurationSeconds
                root.focusActive = true
                focusTimer.restart()
            }
            onFocusStopRequested: root.requestEndFocus()
        }

        Rectangle {
            visible: ScheduleService.unscheduledCount > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 78
            radius: 8
            color: "#161A23"
            border.width: 1
            border.color: "#3A2A2A"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                Rectangle {
                    width: 5
                    Layout.fillHeight: true
                    radius: 3
                    color: "#FF7A59"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Schedule conflicts")
                            color: "#FFB09B"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: qsTr("%1 tasks could not be fully scheduled").arg(ScheduleService.unscheduledCount)
                            color: "#8C96AA"
                            font.pixelSize: 12
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            var issues = ScheduleService.scheduleIssues || []
                            if (issues.length === 0) {
                                return qsTr("Adjust deadlines, reduce estimated duration, or expand available study time.")
                            }
                            var first = issues[0]
                            return qsTr("%1: %2, %3 minutes remaining unscheduled")
                                .arg(first.title || qsTr("Task"))
                                .arg(first.reason || qsTr("No available time"))
                                .arg(first.remainingMinutes || 0)
                        }
                        color: "#C8D0DE"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 36
                    radius: 8
                    color: issueMouse.containsMouse ? "#202638" : "#11151D"
                    border.width: 1
                    border.color: "#30384C"

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Review")
                        color: "#E6EAF2"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: issueMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            ScheduleService.reschedule()
                            quickAddToast.kind = "success"
                            quickAddToast.text = qsTr("Schedule refreshed")
                            quickAddToast.open()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 88

            radius: 14

            color: "#1A1F2B"

            border.width: 1
            border.color: "#6C63FF"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16

                Text {
                    text: "Spark"
                    font.pixelSize: 28
                }

                ColumnLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "AI schedule suggestion"
                        color: "#FFFFFF"
                        font.bold: true
                    }

                    Text {
                        text: "14:00 - 15:30 fits high-focus study"
                        color: "#A6B0C3"
                        font.pixelSize: 12
                    }
                }

                Text {
                    text: "+18%"
                    color: "#4ADE80"
                    font.pixelSize: 24
                    font.bold: true
                }
            }
        }

        QuickAddBar {
            Layout.fillWidth: true
            capacityStats: ScheduleService.capacityStats
            onAddRequested: function(text) {
                var result = ScheduleService.previewTaskDraft(text)
                if (result && result.ok) {
                    root.aiDraft = result.draft || ({})
                    aiDraftTitle.text = root.aiDraft.title || ""
                    aiDraftDeadline.text = root.aiDraft.deadline || ""
                    aiDraftDuration.value = Math.max(30, root.aiDraft.estimatedMinutes || 60)
                    aiDraftPriority.selectedIndex = Math.max(0, Math.min(2, root.aiDraft.priority || 1))
                    aiDraftCategory.text = root.aiDraft.categoryName || qsTr("Study")
                    aiDraftNotes.text = root.aiDraft.notes || ""
                    aiDraftReason.text = root.aiDraft.explanation || result.message || ""
                    aiDraftSource.text = (result.source === "deepseek") ? qsTr("DeepSeek AI draft") : qsTr("Local parser draft")
                    aiDraftPopup.open()
                } else {
                    quickAddToast.kind = "danger"
                    quickAddToast.text = result && result.message ? result.message : qsTr("Could not create a draft")
                    quickAddToast.open()
                }
            }
            onImagePreviewRequested: function(fileUrl) {
                var preview = ScheduleService.previewImageTask(fileUrl)
                ocrText.text = preview.recognizedText || ""
                ocrMessage.text = preview.message || ""
                ocrPopup.open()
            }
        }

        TimelineView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewMode: root.viewMode
            model: ScheduleService.timelineModel
            frameModel: ScheduleService.studyFrames
            minuteHeight: root.timelineMinuteHeight
            requestedDayColumnWidth: root.timelineColumnWidth
            onTimelineMessage: function(kind, message) {
                quickAddToast.kind = kind
                quickAddToast.text = message
                quickAddToast.open()
            }
        }
    }

    FocusScope {
        id: focusLayer
        parent: Overlay.overlay
        anchors.fill: parent
        visible: root.focusActive
        z: 500
        focus: root.focusActive
        opacity: root.focusActive ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        onVisibleChanged: if (visible) forceActiveFocus()

        Keys.onEscapePressed: root.requestEndFocus()

        Rectangle {
            anchors.fill: parent
            color: "#080A0F"
        }

        Item {
            id: breathAnchor
            anchors.centerIn: parent
            width: Math.min(520, parent.width * 0.55)
            height: width

            Rectangle {
                id: breathInner
                anchors.centerIn: parent
                width: parent.width * 0.72
                height: parent.width * 0.72
                radius: height * 0.5
                color: "#FFFFFF"
                opacity: 0.06
                transformOrigin: Item.Center

                SequentialAnimation {
                    running: root.focusActive
                    loops: Animation.Infinite
                    NumberAnimation { target: breathInner; property: "opacity"; to: 0.18; duration: 3200; easing.type: Easing.InOutSine }
                    NumberAnimation { target: breathInner; property: "opacity"; to: 0.04; duration: 3200; easing.type: Easing.InOutSine }
                }

                SequentialAnimation on scale {
                    running: root.focusActive
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.05; duration: 3200; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 0.96; duration: 3200; easing.type: Easing.InOutSine }
                }
            }
        }

        ColumnLayout {
            id: focusContent
            z: 1
            anchors.centerIn: parent
            width: Math.min(640, parent.width - 80)
            spacing: 28

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Focus mode")
                color: "#687389"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.letterSpacing: 2
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                text: ScheduleService.focusItem.title || qsTr("Current task")
                color: "#E6EAF2"
                font.pixelSize: 34
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                id: focusTimerText
                Layout.alignment: Qt.AlignHCenter
                text: root.formatFocusTime(root.focusRemainingSeconds)
                color: "#FFFFFF"
                font.pixelSize: 120
                font.weight: Font.Light
                font.family: "Consolas"
                font.letterSpacing: 2
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: "#2A2A2A"
                clip: true

                Rectangle {
                    height: parent.height
                    width: parent.width * (1 - root.focusRemainingSeconds / root.focusDurationSeconds)
                    radius: 2
                    color: "#FFFFFF"
                    opacity: 0.9
                    Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Keep the current rhythm and finish this time block.")
                color: "#9AA4B2"
                font.pixelSize: 14
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                FocusButton { text: qsTr("Exit focus"); muted: true; onClicked: root.requestEndFocus() }
                FocusButton { text: qsTr("Complete"); onClicked: root.endFocus(true) }
            }
        }
    }

    Popup {
        id: focusExitConfirmPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent ? parent.width - 48 : 420)
        height: 208
        modal: true
        focus: true
        padding: 0
        z: 600
        closePolicy: Popup.CloseOnEscape
        background: PopupBackground {}

        onClosed: if (root.focusActive && focusLayer.visible) focusLayer.forceActiveFocus()

        ColumnLayout {
            id: exitConfirmLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: qsTr("Exit focus session?")
                color: "#E6EAF2"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The current focus session is not complete. Exit without saving progress?")
                color: "#9AA4B2"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                lineHeight: 1.35
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                FocusButton { text: qsTr("Continue"); onClicked: focusExitConfirmPopup.close() }
                FocusButton { text: qsTr("Exit"); muted: true; onClicked: root.endFocus(false) }
            }
        }
    }

    Popup {
        id: quickAddToast
        property alias text: toastText.text
        property string kind: "success"
        x: root.width - width - 28
        y: 92
        width: 300
        height: 58
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 8
            color: quickAddToast.kind === "danger" ? "#2A1D1B" : "#16251F"
            border.width: 1
            border.color: quickAddToast.kind === "danger" ? "#FF7A59" : "#4FAE85"
        }
        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 28
            color: quickAddToast.kind === "danger" ? "#FFB09B" : "#A9F0C9"
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }

    Popup {
        id: aiDraftPopup
        width: 520
        height: 520
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupBackground {}

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    id: aiDraftSource
                    Layout.fillWidth: true
                    text: qsTr("AI task draft")
                    color: "#E6EAF2"
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Text {
                    text: qsTr("Confirm before writing to the database")
                    color: "#8C96AA"
                    font.pixelSize: 12
                }
            }

            TextField { id: aiDraftTitle; Layout.fillWidth: true; placeholderText: qsTr("Task title"); color: "#E6EAF2"; background: PopupFieldBackground {} }
            TextField { id: aiDraftDeadline; Layout.fillWidth: true; placeholderText: "yyyy-MM-dd HH:mm"; color: "#E6EAF2"; background: PopupFieldBackground {} }

            RowLayout {
                Layout.fillWidth: true
                SpinBox {
                    id: aiDraftDuration
                    Layout.fillWidth: true
                    from: 30
                    to: 720
                    stepSize: 15
                    value: 60
                    editable: true
                    textFromValue: function(value) { return qsTr("%1 minutes").arg(value) }
                    valueFromText: function(text) { return Number(text.replace(/[^0-9]/g, "")) }
                    background: PopupFieldBackground {}
                }
                SegmentedControl {
                    id: aiDraftPriority
                    options: [qsTr("Low"), qsTr("Medium"), qsTr("High")]
                    selectedIndex: 1
                }
            }

            TextField { id: aiDraftCategory; Layout.fillWidth: true; placeholderText: qsTr("Category"); color: "#E6EAF2"; background: PopupFieldBackground {} }
            TextArea {
                id: aiDraftNotes
                Layout.fillWidth: true
                Layout.preferredHeight: 82
                color: "#E6EAF2"
                wrapMode: TextEdit.WordWrap
                placeholderText: qsTr("Notes")
                background: PopupFieldBackground {}
            }
            Text {
                id: aiDraftReason
                Layout.fillWidth: true
                color: "#9AA4B2"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FocusButton { text: qsTr("Cancel"); muted: true; onClicked: aiDraftPopup.close() }
                FocusButton { text: qsTr("Create task"); onClicked: root.submitAiDraft() }
            }
        }
    }

    Popup {
        id: ocrPopup
        width: 480
        height: 300
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupBackground {}

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14
            Text { text: qsTr("OCR import"); color: "#E6EAF2"; font.pixelSize: 20; font.weight: Font.DemiBold }
            Text { id: ocrMessage; Layout.fillWidth: true; color: "#9AA4B2"; font.pixelSize: 13; wrapMode: Text.WordWrap }
            TextArea { id: ocrText; Layout.fillWidth: true; Layout.fillHeight: true; color: "#E6EAF2"; font.pixelSize: 14; wrapMode: TextEdit.WordWrap; background: PopupFieldBackground {} }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FocusButton { text: qsTr("Cancel"); muted: true; onClicked: ocrPopup.close() }
                FocusButton {
                    text: qsTr("Create draft")
                    onClicked: {
                        ocrPopup.close()
                        var result = ScheduleService.previewTaskDraft(ocrText.text)
                        if (result && result.ok) {
                            root.aiDraft = result.draft || ({})
                            aiDraftTitle.text = root.aiDraft.title || ""
                            aiDraftDeadline.text = root.aiDraft.deadline || ""
                            aiDraftDuration.value = Math.max(30, root.aiDraft.estimatedMinutes || 60)
                            aiDraftPriority.selectedIndex = Math.max(0, Math.min(2, root.aiDraft.priority || 1))
                            aiDraftCategory.text = root.aiDraft.categoryName || qsTr("Study")
                            aiDraftNotes.text = root.aiDraft.notes || ""
                            aiDraftReason.text = root.aiDraft.explanation || result.message || ""
                            aiDraftSource.text = (result.source === "deepseek") ? qsTr("DeepSeek AI draft") : qsTr("Local parser draft")
                            aiDraftPopup.open()
                        }
                    }
                }
            }
        }
    }

    component DensityControl: Rectangle {
        id: density
        property string label: ""
        property real value: 0
        property real from: 0
        property real to: 1
        property string valueLabel: ""
        signal valueChangedByUser(real value)

        Layout.preferredWidth: 188
        Layout.preferredHeight: 38
        radius: 8
        color: "#161A23"
        border.width: 1
        border.color: "#2A3142"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8
            Text { text: density.label; color: "#9AA4B2"; font.pixelSize: 12; font.weight: Font.DemiBold }
            Slider { Layout.fillWidth: true; from: density.from; to: density.to; value: density.value; live: true; onMoved: density.valueChangedByUser(value) }
            Text { text: density.valueLabel; color: "#E6EAF2"; font.pixelSize: 12; font.weight: Font.DemiBold }
        }
    }

    component PopupBackground: Rectangle {
        radius: 10
        color: "#161A23"
        border.width: 1
        border.color: "#30384C"
    }

    component PopupFieldBackground: Rectangle {
        radius: 8
        color: "#10141C"
        border.width: 1
        border.color: "#252B3A"
    }

    component DarkPills: Rectangle {
        id: pills
        property var options: []
        property int currentIndex: 0

        Layout.preferredHeight: 42
        radius: 8
        color: "#10141C"
        border.width: 1
        border.color: "#252B3A"

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

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: pills.currentIndex === index ? "#E6EAF2" : "#8D98AB"
                        font.pixelSize: 11
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

        Layout.preferredHeight: 42
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

    component FocusButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        signal clicked()

        width: label.text.length > 4 ? 96 : 76
        height: 38
        radius: 8
        color: muted ? (mouse.containsMouse ? "#202638" : "#161A23") : (mouse.containsMouse ? "#8B99FF" : "#7C8CFF")
        border.width: muted ? 1 : 0
        border.color: "#30384C"
        Behavior on color { ColorAnimation { duration: 140 } }
        Text { id: label; anchors.centerIn: parent; color: muted ? "#AAB4C6" : "white"; font.pixelSize: 13; font.weight: Font.DemiBold }
        MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: button.clicked() }
    }
}
