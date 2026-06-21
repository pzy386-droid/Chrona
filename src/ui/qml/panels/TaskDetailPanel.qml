import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    property var task: ({})
    property string editStartTime: "09:00"
    property string editEndTime: "10:30"
    property string draftStartTime: "09:00"
    property string draftEndTime: "10:30"
    property string activeTimeTarget: "editStart"
    property string selectedCategoryColor: Theme.accentBright
    property string draftCategoryColor: "#6FD6A7"
    property int rangeStartDayIndex: 0
    property int rangeEndDayIndex: 0
    property bool readOnly: false
    property var focusBufferSuggestion: ({})
    readonly property bool hasTask: Boolean(task && Number(task.id || 0) > 0)
    readonly property bool hasExistingBlock: hasTask && Number(task.blockId || 0) > 0
    readonly property var preferredValues: ["morning", "afternoon", "evening"]
    readonly property var energyValues: ["low", "medium", "high"]
    readonly property var minChunkValues: [30, 45, 60]
    readonly property var idealChunkValues: [60, 90, 120]
    readonly property var categoryColors: ["#6FD6A7", Theme.accentBright, "#FF8A65", Theme.warning, "#A78BFA", "#60A5FA", Theme.secondaryText, Theme.highPriority]
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
        editStartTime = task.blockStartText || "09:00"
        editEndTime = task.blockEndText || "10:30"
        rangeStartDayIndex = Math.max(0, Math.min(6, task.blockDayIndex || 0))
        rangeEndDayIndex = Math.max(rangeStartDayIndex, Math.min(6, task.blockEndDayIndex || rangeStartDayIndex))
        priorityPicker.currentIndex = Math.max(0, Math.min(2, task.priority || 0))
        categoryField.text = task.categoryName || ""
        selectedCategoryColor = task.categoryColor || Theme.accentBright
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

    function minuteFromTime(value) {
        var parts = String(value || "").split(":")
        if (parts.length !== 2) {
            return -1
        }
        var hour = Number(parts[0])
        var minute = Number(parts[1])
        if (isNaN(hour) || isNaN(minute) || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
            return -1
        }
        return hour * 60 + minute
    }

    function openStartPicker(target, value) {
        activeTimeTarget = target
        startWheel.openWith(value)
    }

    function openEndPicker(target, value) {
        activeTimeTarget = target
        endWheel.openWith(value)
    }

    function offerFocusBufferIfNeeded(effortLevel) {
        if (effortLevel < 2) {
            return
        }
        var suggestion = ScheduleService.previewSelectedFocusBuffer()
        if (!suggestion || suggestion.ok !== true) {
            statusText.color = Theme.warning
            statusText.text = suggestion && suggestion.message
                ? suggestion.message
                : qsTr("暂时无法生成专注缓冲建议")
            return
        }
        root.focusBufferSuggestion = suggestion
        focusBufferPopup.open()
    }

    color: Theme.canvasBackground
    border.width: 1
    border.color: Theme.divider

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
                text: qsTr("任务详情")
                color: Theme.secondaryText
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            RowLayout {
                visible: root.hasTask && root.task.canLock !== false && !root.readOnly
                spacing: 7

                Text {
                    text: qsTr("固定当前块")
                    color: Theme.tertiaryText
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                ToggleSwitch {
                    id: eventLockToggle
                    checked: true
                    onToggled: function(checked) {
                        statusText.color = Theme.success
                        statusText.text = checked ? qsTr("保存后将固定整个连续时间块") : qsTr("保存后将取消固定整个连续时间块")
                    }
                }
            }

            Rectangle {
                width: 32
                height: 32
                radius: 8
                color: closeMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
                border.width: 1
                border.color: Theme.borderSoft

                Text {
                    anchors.centerIn: parent
                    text: "›"
                    color: Theme.secondaryText
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
                color: root.selectedCategoryColor
                Behavior on color { ColorAnimation { duration: 180 } }
            }

        }

        Rectangle {
            Layout.fillWidth: true
            height: 5
            radius: 3
            color: root.hasTask ? root.selectedCategoryColor : Theme.surfaceHover
            Behavior on color { ColorAnimation { duration: 200 } }
        }

        ColumnLayout {
            visible: !root.hasTask
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: qsTr("添加时间块")
                color: Theme.primaryText
                font.pixelSize: 21
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("手动加入课程、吃饭、考试等固定安排。Scheduler 会自动避开固定块。")
                color: Theme.secondaryText
                font.pixelSize: 12
                lineHeight: 1.35
                wrapMode: Text.WordWrap
            }

            FieldLabel { text: qsTr("标题") }
            TextField {
                id: draftTitleField
                Layout.fillWidth: true
                color: Theme.primaryText
                selectedTextColor: "white"
                selectionColor: Theme.accent
                placeholderText: qsTr("例如：实验课 / 吃饭")
                placeholderTextColor: Theme.mutedText
                font.pixelSize: 16
                font.weight: Font.DemiBold
                background: FieldBackground {}
            }

            FieldLabel { text: qsTr("日期") }
            OptionPills {
                id: draftDayPicker
                Layout.fillWidth: true
                options: [qsTr("今天"), qsTr("明天"), qsTr("后天"), qsTr("第4天"), qsTr("第5天"), qsTr("第6天"), qsTr("第7天")]
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                TimeSelectButton {
                    Layout.fillWidth: true
                    value: root.draftStartTime
                    onClicked: root.openStartPicker("draftStart", root.draftStartTime)
                }

                Text {
                    text: "-"
                    color: Theme.mutedText
                    font.pixelSize: 16
                }

                TimeSelectButton {
                    Layout.fillWidth: true
                    value: root.draftEndTime
                    onClicked: root.openEndPicker("draftEnd", root.draftEndTime)
                }
            }

            FieldLabel { text: qsTr("分类") }
            TextField {
                id: draftCategoryField
                Layout.fillWidth: true
                text: qsTr("课程")
                color: Theme.primaryText
                selectedTextColor: "white"
                selectionColor: Theme.accent
                placeholderText: qsTr("课程 / 固定时间")
                placeholderTextColor: Theme.mutedText
                font.pixelSize: 13
                background: FieldBackground {}
            }

            FieldLabel { text: qsTr("颜色") }
            ColorSwatches {
                Layout.fillWidth: true
                colors: root.categoryColors
                selectedColor: root.draftCategoryColor
                onColorPicked: function(color) { root.draftCategoryColor = color }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                radius: 8
                color: Theme.surfaceMuted
                border.width: 1
                border.color: Theme.borderSoft

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("固定时间块")
                            color: Theme.primaryText
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: qsTr("固定后自动排程会避开这段时间")
                            color: Theme.tertiaryText
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    ToggleSwitch {
                        id: draftLockToggle
                        checked: true
                    }
                }
            }

            Text {
                id: draftStatusText
                Layout.fillWidth: true
                text: ""
                color: Theme.success
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            ActionButton {
                Layout.fillWidth: true
                text: qsTr("加入时间轴")
                onClicked: {
                    var startMinute = root.minuteFromTime(root.draftStartTime)
                    var endMinute = root.minuteFromTime(root.draftEndTime)
                    if (startMinute < 0 || endMinute < 0 || endMinute <= startMinute) {
                        draftStatusText.color = Theme.error
                        draftStatusText.text = qsTr("结束时间必须晚于起始时间")
                        return
                    }
                    var result = ScheduleService.createScheduledTask(
                        draftTitleField.text,
                        draftDayPicker.currentIndex,
                        startMinute,
                        endMinute - startMinute,
                        draftLockToggle.checked,
                        draftCategoryField.text,
                        root.draftCategoryColor
                    )
                    draftStatusText.color = result.ok ? Theme.success : Theme.error
                    draftStatusText.text = result.message || ""
                    if (result.ok) {
                        draftTitleField.text = ""
                    }
                }
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

                FieldLabel { text: qsTr("任务标题") }
                TextField {
                    id: titleField
                    Layout.fillWidth: true
                    color: Theme.primaryText
                    selectedTextColor: "white"
                    selectionColor: Theme.accent
                    placeholderText: qsTr("例如：高数期中复习")
                    placeholderTextColor: Theme.mutedText
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    background: FieldBackground {}
                }

                Rectangle {
                    visible: false
                    Layout.fillWidth: true
                    Layout.preferredHeight: 54
                    radius: 10
                    color: Theme.surfaceMuted
                    border.width: 1
                    border.color: Theme.borderSoft

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("固定时间会作为硬约束保留在时间轴中，Scheduler 会自动避开。选择连续日期后保存，可生成每天同一时间的固定块。")
                        color: Theme.tertiaryText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                FieldLabel {
                    visible: true
                    text: qsTr("备注")
                }
                TextArea {
                    id: notesField
                    visible: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 88
                    color: Theme.primaryText
                    selectedTextColor: "white"
                    selectionColor: Theme.accent
                    placeholderText: qsTr("补充资料、章节或完成标准")
                    placeholderTextColor: Theme.mutedText
                    font.pixelSize: 13
                    wrapMode: TextEdit.WordWrap
                    background: FieldBackground {}
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    FieldLabel {
                        Layout.fillWidth: true
                        text: qsTr("当前时间块")
                    }

                    Text {
                        visible: (root.task.blockTotal || 0) > 1
                        text: qsTr("第 %1 / %2 块").arg(root.task.blockOrdinal || 1).arg(root.task.blockTotal || 1)
                        color: Theme.accentBright
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }

                DayRangePills {
                    id: dayPicker
                    Layout.fillWidth: true
                    startIndex: root.rangeStartDayIndex
                    endIndex: root.rangeEndDayIndex
                    onRangeChanged: function(startIndex, endIndex) {
                        root.rangeStartDayIndex = startIndex
                        root.rangeEndDayIndex = endIndex
                    }
                    options: [qsTr("今天"), qsTr("明天"), qsTr("后天"), qsTr("第4天"), qsTr("第5天"), qsTr("第6天"), qsTr("第7天")]
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    TimeSelectButton {
                        Layout.fillWidth: true
                        value: root.editStartTime || "09:00"
                        onClicked: root.openStartPicker("editStart", root.editStartTime)
                    }

                    Text {
                        text: "-"
                        color: Theme.mutedText
                        font.pixelSize: 16
                    }

                    TimeSelectButton {
                        Layout.fillWidth: true
                        value: root.editEndTime || "10:30"
                        onClicked: root.openEndPicker("editEnd", root.editEndTime)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: true
                        FieldLabel { text: qsTr("优先级") }
                        OptionPills {
                            id: priorityPicker
                            Layout.fillWidth: true
                            onCurrentIndexChanged: {
                                if (currentIndex === 2) {
                                    root.selectedCategoryColor = Theme.highPriority
                                }
                            }
                            options: [qsTr("低"), qsTr("中"), qsTr("高")]
                        }
                    }
                }

                FieldLabel { text: qsTr("课程分类") }
                TextField {
                    id: categoryField
                    Layout.fillWidth: true
                    color: Theme.primaryText
                    selectedTextColor: "white"
                    selectionColor: Theme.accent
                    placeholderText: qsTr("学习 / 高数 / 英语")
                    placeholderTextColor: Theme.mutedText
                    font.pixelSize: 13
                    background: FieldBackground {}
                }

                FieldLabel { text: qsTr("颜色") }
                ColorSwatches {
                    Layout.fillWidth: true
                    colors: root.categoryColors
                    selectedColor: root.selectedCategoryColor
                    onColorPicked: function(color) { root.selectedCategoryColor = color }
                }

                FieldLabel {
                    visible: true
                    text: qsTr("偏好学习时间")
                }
                OptionPills {
                    id: preferredPicker
                    visible: true
                    Layout.fillWidth: true
                    options: [qsTr("上午"), qsTr("下午"), qsTr("晚上")]
                }

                FieldLabel {
                    visible: true
                    text: qsTr("自动调度偏好")
                }
                Rectangle {
                    visible: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: 8
                    color: Theme.surfaceMuted
                    border.width: 1
                    border.color: Theme.borderSoft

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("允许 Scheduler 自动排程")
                                color: Theme.primaryText
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: qsTr("关闭后仅保留你手动拖拽的时间块")
                                color: Theme.tertiaryText
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
                    visible: true
                    Layout.fillWidth: true
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("截止类型") }
                        OptionPills {
                            id: deadlineTypePicker
                            Layout.fillWidth: true
                            onCurrentIndexChanged: {
                                if (currentIndex === 1) {
                                    eventLockToggle.checked = true
                                    statusText.color = Theme.warning
                                    statusText.text = qsTr("硬截止已自动固定当前时间块")
                                }
                            }
                            options: [qsTr("软"), qsTr("硬")]
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("学习强度") }
                        OptionPills {
                            id: effortPicker
                            Layout.fillWidth: true
                            options: [qsTr("轻"), qsTr("中"), qsTr("重")]
                        }
                    }
                }

                RowLayout {
                    visible: true
                    Layout.fillWidth: true
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("最短块") }
                        OptionPills {
                            id: minChunkPicker
                            Layout.fillWidth: true
                            options: [qsTr("30"), qsTr("45"), qsTr("60")]
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("理想块") }
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
                    color: Theme.success
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            visible: root.hasTask
            Layout.fillWidth: true
            spacing: 10

            ActionButton {
                visible: root.task.canCompleteBlock === true || root.task.canUndoCompleteBlock === true
                Layout.fillWidth: true
                text: root.task.canUndoCompleteBlock === true ? qsTr("取消完成") : qsTr("完成当前块")
                onClicked: {
                    var result = root.task.canUndoCompleteBlock === true
                        ? ScheduleService.reopenBlock(root.task.blockId || 0)
                        : ScheduleService.completeBlock(root.task.blockId || 0)
                    statusText.color = result && result.ok ? Theme.success : Theme.error
                    statusText.text = result && result.message ? result.message : qsTr("已处理")
                }
            }

            ActionButton {
                Layout.fillWidth: true
                text: qsTr("解释排程")
                onClicked: {
                    var result = ScheduleService.explainSelectedSchedule()
                    statusText.text = result.summary || result.message || qsTr("暂无排程解释")
                }
            }

            ActionButton {
                Layout.fillWidth: true
                text: qsTr("移除任务")
                danger: true
                visible: !root.readOnly
                onClicked: deleteChoicePopup.open()
            }
        }

        ActionButton {
            visible: root.hasTask && !root.readOnly
            Layout.fillWidth: true
            text: qsTr("保存时间块")
            onClicked: {
                var savedTitle = titleField.text
                var savedNotes = notesField.text
                var savedDeadline = root.task.deadlineText || ""
                var savedCategory = categoryField.text
                var savedDayIndex = root.rangeStartDayIndex
                var savedEndDayIndex = root.rangeEndDayIndex
                var savedStartTime = root.editStartTime
                var savedEndTime = root.editEndTime
                var savedPriority = priorityPicker.currentIndex
                var savedPreferred = root.preferredValues[preferredPicker.currentIndex]
                var savedAutoSchedule = autoScheduleToggle.checked
                var savedDeadlineType = deadlineTypePicker.currentIndex
                var savedMinChunk = root.minChunkValues[minChunkPicker.currentIndex]
                var savedIdealChunk = root.idealChunkValues[idealChunkPicker.currentIndex]
                var savedEffort = effortPicker.currentIndex
                var savedEventLocked = savedDeadlineType === 1 ? true : eventLockToggle.checked
                var originalDayIndex = root.task.blockDayIndex || 0
                var originalEndDayIndex = root.task.blockEndDayIndex || originalDayIndex
                var originalStartTime = root.task.blockStartText || ""
                var originalEndTime = root.task.blockEndText || ""
                var originalLocked = !!root.task.isLocked

                var startParts = String(savedStartTime).split(":")
                var endParts = String(savedEndTime).split(":")
                var startMinute = Number(startParts[0]) * 60 + Number(startParts[1])
                var endMinute = Number(endParts[0]) * 60 + Number(endParts[1])
                if (startParts.length !== 2 || endParts.length !== 2 || isNaN(startMinute) || isNaN(endMinute) || endMinute <= startMinute) {
                    statusText.color = Theme.error
                    statusText.text = qsTr("结束时间必须晚于起始时间")
                    return
                }
                var selectedDayCount = Math.max(1, Math.abs(savedEndDayIndex - savedDayIndex) + 1)
                var scheduledEstimatedMinutes = Math.max(30, (endMinute - startMinute) * selectedDayCount)

                var updateResult = ScheduleService.updateTask(
                    root.task.id,
                    savedTitle,
                    savedNotes,
                    savedDeadline,
                    scheduledEstimatedMinutes,
                    savedPriority,
                    savedCategory,
                    root.selectedCategoryColor,
                    savedPreferred,
                    savedAutoSchedule,
                    savedDeadlineType,
                    savedMinChunk,
                    savedIdealChunk,
                    savedEffort
                )
                if (!updateResult.ok) {
                    statusText.color = Theme.error
                    statusText.text = updateResult.message || qsTr("保存失败")
                    return
                }

                var hasExistingBlock = Number(root.task.blockId || 0) > 0
                var blockChanged = savedDayIndex !== originalDayIndex
                    || savedEndDayIndex !== originalEndDayIndex
                    || savedStartTime !== originalStartTime
                    || savedEndTime !== originalEndTime
                    || savedEventLocked !== originalLocked
                var shouldSaveBlock = !hasExistingBlock || blockChanged

                if (shouldSaveBlock) {
                    var moveResult = ScheduleService.scheduleSelectedTaskBlocks(savedDayIndex, savedEndDayIndex, savedStartTime, savedEndTime, savedEventLocked)
                    statusText.color = moveResult.ok ? Theme.success : Theme.error
                    statusText.text = moveResult.ok ? qsTr("已保存并重排日历") : (moveResult.message || qsTr("移动失败"))
                    if (moveResult.ok) {
                        root.offerFocusBufferIfNeeded(savedEffort)
                    }
                    return
                }

                statusText.color = Theme.success
                statusText.text = qsTr("已保存并重排日历")
                root.offerFocusBufferIfNeeded(savedEffort)
            }
        }

        ActionButton {
            visible: root.readOnly && root.hasTask && !root.task.completed
            Layout.fillWidth: true
            text: qsTr("补记为已完成")
            onClicked: {
                var result = ScheduleService.completeBlock(root.task.blockId || 0)
                statusText.color = result.ok ? Theme.success : Theme.error
                statusText.text = result.message || ""
            }
        }
    }

    Popup {
        id: focusBufferPopup
        width: Math.min(root.width - 24, 312)
        height: 330
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 18
            color: Theme.glassSurface
            border.width: 1
            border.color: Theme.accentBright
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: 12
                    color: Theme.infoSurface
                    Text {
                        anchors.centerIn: parent
                        text: "AI"
                        color: Theme.accentBright
                        font.weight: Font.Black
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: root.focusBufferSuggestion.title || qsTr("专注缓冲建议")
                    color: Theme.primaryText
                    font.pixelSize: 17
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.focusBufferSuggestion.description || ""
                color: Theme.secondaryText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                text: root.focusBufferSuggestion.impact || ""
                color: Theme.success
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 74
                radius: 10
                color: Theme.surfaceMuted
                border.width: 1
                border.color: Theme.borderSoft
                Text {
                    anchors.fill: parent
                    anchors.margins: 10
                    text: root.focusBufferSuggestion.aiExplanation
                        || qsTr("Chrona 会保留硬截止与固定安排，并为高强度学习减少前后任务切换。")
                    color: Theme.tertiaryText
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                }
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ActionButton {
                    Layout.fillWidth: true
                    muted: true
                    text: qsTr("暂不调整")
                    onClicked: focusBufferPopup.close()
                }
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("确认执行")
                    onClicked: {
                        var result = ScheduleService.applyFocusBufferSuggestion(root.focusBufferSuggestion)
                        statusText.color = result && result.ok ? Theme.success : Theme.error
                        statusText.text = result && result.message ? result.message : qsTr("执行失败")
                        if (result && result.ok) {
                            focusBufferPopup.close()
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: deleteChoicePopup
        width: 310
        height: 220
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 14
            color: Theme.surfaceElevated
            border.width: 1
            border.color: Theme.border
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12
            Text { text: qsTr("如何移除这个任务？"); color: Theme.primaryText; font.pixelSize: 17; font.weight: Font.DemiBold }
            Text {
                Layout.fillWidth: true
                text: qsTr("归档会保留历史日程；彻底删除会同时清除关联历史记录。")
                color: Theme.secondaryText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Item { Layout.fillHeight: true }
            ActionButton {
                Layout.fillWidth: true
                text: qsTr("归档并保留历史")
                onClicked: {
                    var result = ScheduleService.archiveTask(root.task.id)
                    statusText.text = result.message || ""
                    deleteChoicePopup.close()
                }
            }
            ActionButton {
                Layout.fillWidth: true
                text: qsTr("彻底删除任务与历史")
                danger: true
                onClicked: {
                    var result = ScheduleService.deleteTask(root.task.id)
                    statusText.text = result.message || ""
                    deleteChoicePopup.close()
                }
            }
        }
    }

    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    TimeWheelPicker {
        id: startWheel
        title: qsTr("选择起始时间")
        onAccepted: function(value) {
            if (root.activeTimeTarget === "draftStart") {
                root.draftStartTime = value
            } else {
                root.editStartTime = value
            }
        }
    }

    TimeWheelPicker {
        id: endWheel
        title: qsTr("选择终止时间")
        onAccepted: function(value) {
            if (root.activeTimeTarget === "draftEnd") {
                root.draftEndTime = value
            } else {
                root.editEndTime = value
            }
        }
    }

    component FieldLabel: Text {
        color: Theme.tertiaryText
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    component FieldBackground: Rectangle {
        radius: 8
        color: Theme.surfaceMuted
        border.width: 1
        border.color: Theme.borderSoft
    }

    component OptionPills: Rectangle {
        id: pills
        property var options: []
        property int currentIndex: 0

        Layout.preferredHeight: 42
        radius: 8
        color: Theme.surfaceMuted
        border.width: 1
        border.color: Theme.borderSoft

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
                    color: pills.currentIndex === index ? Theme.surfaceSelected : "transparent"
                    border.width: pills.currentIndex === index ? 1 : 0
                    border.color: Theme.accentBright
                    Behavior on color { ColorAnimation { duration: 140 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: pills.currentIndex === index ? Theme.primaryText : Theme.tertiaryText
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

    component DayRangePills: Rectangle {
        id: pills
        property var options: []
        property int startIndex: 0
        property int endIndex: 0
        signal rangeChanged(int startIndex, int endIndex)

        function choose(index) {
            startIndex = index
            endIndex = index
            rangeChanged(index, index)
        }

        Layout.preferredHeight: 42
        radius: 8
        color: Theme.surfaceMuted
        border.width: 1
        border.color: Theme.borderSoft
        clip: true

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
                    readonly property bool selected: index >= Math.min(pills.startIndex, pills.endIndex) && index <= Math.max(pills.startIndex, pills.endIndex)
                    color: selected ? Theme.surfaceSelected : "transparent"
                    border.width: selected ? 1 : 0
                    border.color: Theme.accentBright

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.width { NumberAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: parent.selected ? Theme.primaryText : Theme.tertiaryText
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: pills.choose(index)
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
        color: mouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
        border.width: 1
        border.color: mouse.containsMouse ? Theme.infoBorder : Theme.borderSoft

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
                color: Theme.primaryText
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Text {
                text: "⌄"
                color: Theme.accentBright
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
        color: toggle.checked ? Theme.accentBright : Theme.surfacePressed
        border.width: 1
        border.color: toggle.checked ? Theme.accentBright : Theme.border

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Rectangle {
            width: 22
            height: 22
            radius: 11
            x: toggle.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.accentText
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

    component ColorSwatches: Rectangle {
        id: swatches
        property var colors: []
        property string selectedColor: Theme.accentBright
        signal colorPicked(string color)

        Layout.preferredHeight: 32
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            spacing: 8

            Repeater {
                model: swatches.colors
                delegate: Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    radius: 12
                    color: modelData
                    border.width: String(modelData).toLowerCase() === swatches.selectedColor.toLowerCase() ? 2 : 1
                    border.color: String(modelData).toLowerCase() === swatches.selectedColor.toLowerCase() ? Theme.primaryText : Theme.border
                    scale: swatchMouse.containsMouse ? 1.08 : 1

                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Behavior on border.width { NumberAnimation { duration: 120 } }

                    MouseArea {
                        id: swatchMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: swatches.colorPicked(modelData)
                    }
                }
            }

            Item { Layout.fillWidth: true }
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
            ? (mouse.containsMouse ? Theme.dangerSurfaceHover : Theme.dangerSurface)
            : muted ? (mouse.containsMouse ? Theme.surfaceHover : Theme.surface)
            : (mouse.containsMouse ? "#8B99FF" : Theme.accentBright)
        border.width: muted || danger ? 1 : 0
        border.color: danger ? Theme.dangerBorder : Theme.border
        Behavior on color { ColorAnimation { duration: 140 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: button.danger ? Theme.error : button.muted ? Theme.secondaryText : "white"
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
