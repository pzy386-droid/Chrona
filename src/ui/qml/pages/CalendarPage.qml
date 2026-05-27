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

    function formatFocusTime(seconds) {
        var m = Math.floor(seconds / 60)
        var s = seconds % 60
        return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0")
    }

    function endFocus(completeTask) {
        if (completeTask && ScheduleService.focusItem.taskId > 0) {
            ScheduleService.completeTask(ScheduleService.focusItem.taskId)
        } else {
            ScheduleService.stopFocus()
        }
        root.focusActive = false
        focusPanel.active = false
        focusTimer.stop()
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: qsTr("学习日程")
                    color: "#E6EAF2"
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("自动安排你的下一段专注时间")
                    color: "#9AA4B2"
                    font.pixelSize: 13
                }
            }

            DensityControl {
                label: qsTr("行距")
                value: root.timelineMinuteHeight
                from: 0.85
                to: 1.75
                onValueChangedByUser: function(v) { root.timelineMinuteHeight = v }
                valueLabel: root.timelineMinuteHeight < 1.08 ? qsTr("紧") : root.timelineMinuteHeight > 1.42 ? qsTr("松") : qsTr("中")
            }

            DensityControl {
                label: qsTr("列距")
                value: root.timelineColumnWidth
                from: 140
                to: 260
                onValueChangedByUser: function(v) { root.timelineColumnWidth = v }
                valueLabel: root.timelineColumnWidth < 170 ? qsTr("紧") : root.timelineColumnWidth > 225 ? qsTr("松") : qsTr("中")
            }

            SegmentedControl {
                options: [qsTr("日"), qsTr("周")]
                selectedIndex: root.viewMode === "day" ? 0 : 1
                onSelectedIndexChanged: root.viewMode = selectedIndex === 0 ? "day" : "week"
            }

            LanguageToggle {
                currentLocale: LocaleService.locale
                onLocaleRequested: function(locale) { LocaleService.setLocale(locale) }
            }
        }

        CurrentFocusPanel {
            id: focusPanel
            Layout.fillWidth: true
            focusItem: ScheduleService.focusItem
            unscheduledCount: ScheduleService.unscheduledCount
            onFocusStarted: {
                root.focusRemainingSeconds = root.focusDurationSeconds
                root.focusActive = true
                focusTimer.restart()
            }
            onFocusStopped: root.endFocus(false)
        }

        QuickAddBar {
            Layout.fillWidth: true
            capacityStats: ScheduleService.capacityStats
            onAddRequested: function(text) {
                var result = ScheduleService.quickAdd(text)
                quickAddToast.kind = result && result.ok ? "success" : "danger"
                quickAddToast.text = result && result.message ? result.message : qsTr("已处理")
                quickAddToast.open()
            }
            onImagePreviewRequested: function(fileUrl) {
                var preview = ScheduleService.previewImageTask(fileUrl)
                ocrText.text = preview.recognizedText || ""
                ocrMessage.text = preview.message || ""
                ocrPopup.open()
            }
            onFixedEventRequested: fixedEventPopup.open()
        }

        TimelineView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewMode: root.viewMode
            model: ScheduleService.timelineModel
            minuteHeight: root.timelineMinuteHeight
            requestedDayColumnWidth: root.timelineColumnWidth
        }
    }

    Item {
        id: focusLayer
        visible: root.focusActive
        anchors.fill: parent
        z: 20
        opacity: root.focusActive ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            color: "#080A0F"
            opacity: 0.76
        }

        Rectangle {
            id: halo
            anchors.centerIn: focusCard
            width: focusCard.width + 58
            height: focusCard.height + 58
            radius: 18
            color: "transparent"
            border.width: 1
            border.color: "#7C8CFF"
            opacity: 0.18

            SequentialAnimation on scale {
                running: root.focusActive
                loops: Animation.Infinite
                NumberAnimation { to: 1.035; duration: 1800; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 1800; easing.type: Easing.InOutSine }
            }

            SequentialAnimation on opacity {
                running: root.focusActive
                loops: Animation.Infinite
                NumberAnimation { to: 0.30; duration: 1800; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.14; duration: 1800; easing.type: Easing.InOutSine }
            }
        }

        Rectangle {
            id: focusCard
            width: Math.min(548, root.width - 64)
            height: 270
            radius: 12
            color: "#161A23"
            border.width: 1
            border.color: "#30384C"
            anchors.centerIn: parent
            scale: root.focusActive ? 1 : 0.96

            Behavior on scale { NumberAnimation { duration: 320; easing.type: Easing.OutBack } }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 11
                color: "transparent"
                border.width: 1
                border.color: "#1E2533"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 13

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("沉浸专注")
                        color: "#9AA4B2"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: qsTr("25 分钟")
                        color: "#687389"
                        font.pixelSize: 12
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: ScheduleService.focusItem.title || qsTr("当前任务")
                    color: "#E6EAF2"
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    text: root.formatFocusTime(root.focusRemainingSeconds)
                    color: "#7C8CFF"
                    font.pixelSize: 56
                    font.weight: Font.Bold
                    font.family: "Consolas"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    radius: 4
                    color: "#252B3A"
                    clip: true

                    Rectangle {
                        height: parent.height
                        width: parent.width * (1 - root.focusRemainingSeconds / root.focusDurationSeconds)
                        radius: 4
                        color: "#7C8CFF"
                        Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("保持当前节奏，完成这一段时间块。")
                    color: "#9AA4B2"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Item { Layout.fillWidth: true }

                    FocusButton {
                        text: qsTr("取消")
                        muted: true
                        onClicked: root.endFocus(false)
                    }

                    FocusButton {
                        text: qsTr("完成")
                        onClicked: root.endFocus(true)
                    }
                }
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
        id: fixedEventPopup
        width: 440
        height: 348
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 10
            color: "#161A23"
            border.width: 1
            border.color: "#30384C"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Text {
                text: qsTr("添加固定时间")
                color: "#E6EAF2"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            TextField {
                id: eventTitle
                Layout.fillWidth: true
                placeholderText: qsTr("例如：数据库实验课")
                color: "#E6EAF2"
                placeholderTextColor: "#667187"
                background: PopupFieldBackground {}
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ComboBox {
                    id: eventDay
                    Layout.fillWidth: true
                    model: [qsTr("今天"), qsTr("明天"), qsTr("后天"), qsTr("第 4 天"), qsTr("第 5 天"), qsTr("第 6 天"), qsTr("第 7 天")]
                    background: PopupFieldBackground {}
                }

                SpinBox {
                    id: eventHour
                    Layout.preferredWidth: 94
                    from: 0
                    to: 23
                    value: 9
                    editable: true
                    textFromValue: function(value) { return String(value).padStart(2, "0") + qsTr(" 点") }
                    valueFromText: function(text) { return Number(text.replace(/[^0-9]/g, "")) }
                    background: PopupFieldBackground {}
                }

                SpinBox {
                    id: eventMinute
                    Layout.preferredWidth: 94
                    from: 0
                    to: 45
                    stepSize: 15
                    value: 0
                    editable: true
                    textFromValue: function(value) { return String(value).padStart(2, "0") + qsTr(" 分") }
                    valueFromText: function(text) { return Number(text.replace(/[^0-9]/g, "")) }
                    background: PopupFieldBackground {}
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                SpinBox {
                    id: eventDuration
                    Layout.fillWidth: true
                    from: 15
                    to: 360
                    stepSize: 15
                    value: 90
                    editable: true
                    textFromValue: function(value) { return qsTr("%1 分钟").arg(value) }
                    valueFromText: function(text) { return Number(text.replace(/[^0-9]/g, "")) }
                    background: PopupFieldBackground {}
                }

                TextField {
                    id: eventCategory
                    Layout.fillWidth: true
                    text: qsTr("课程")
                    color: "#E6EAF2"
                    background: PopupFieldBackground {}
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: qsTr("锁定后 Scheduler 永远避开这段时间")
                    color: "#8C96AA"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                CheckBox {
                    id: eventLocked
                    checked: true
                    text: qsTr("固定")
                }
            }

            Text {
                id: eventStatus
                Layout.fillWidth: true
                color: "#FFB09B"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }

                FocusButton {
                    text: qsTr("取消")
                    muted: true
                    onClicked: fixedEventPopup.close()
                }

                FocusButton {
                    text: qsTr("加入")
                    onClicked: {
                        var startMinute = eventHour.value * 60 + eventMinute.value
                        var result = ScheduleService.addFixedEvent(eventTitle.text, eventDay.currentIndex, startMinute, eventDuration.value, eventLocked.checked, eventCategory.text)
                        if (result.ok) {
                            fixedEventPopup.close()
                            quickAddToast.kind = "success"
                            quickAddToast.text = result.message
                            quickAddToast.open()
                        } else {
                            eventStatus.text = result.message
                        }
                    }
                }
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
        background: Rectangle {
            radius: 10
            color: "#161A23"
            border.width: 1
            border.color: "#30384C"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                text: qsTr("OCR 识别结果")
                color: "#E6EAF2"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                id: ocrMessage
                Layout.fillWidth: true
                color: "#9AA4B2"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            TextArea {
                id: ocrText
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#E6EAF2"
                font.pixelSize: 14
                wrapMode: TextEdit.WordWrap
                background: PopupFieldBackground {}
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                FocusButton {
                    text: qsTr("取消")
                    muted: true
                    onClicked: ocrPopup.close()
                }

                FocusButton {
                    text: qsTr("加入日程")
                    onClicked: {
                        ScheduleService.quickAdd(ocrText.text)
                        ocrPopup.close()
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

            Text {
                text: density.label
                color: "#9AA4B2"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Slider {
                Layout.fillWidth: true
                from: density.from
                to: density.to
                value: density.value
                live: true
                onMoved: density.valueChangedByUser(value)
            }

            Text {
                text: density.valueLabel
                color: "#E6EAF2"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }
    }

    component PopupFieldBackground: Rectangle {
        radius: 8
        color: "#10141C"
        border.width: 1
        border.color: "#252B3A"
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
