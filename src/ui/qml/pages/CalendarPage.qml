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
    property int focusDurationSeconds: 0
    property int focusRemainingSeconds: 0
    property double focusStartMs: 0
    property double focusEndMs: 0
    property bool focusCanComplete: false
    property var aiDraft: ({})
    property var aiDrafts: []
    property int aiDraftIndex: 0
    property bool aiDraftLoading: false
    readonly property date selectedDateValue: new Date(ScheduleService.selectedDateText + "T00:00:00")
    readonly property date todayValue: {
        var value = new Date()
        value.setHours(0, 0, 0, 0)
        return value
    }
    property bool historicalView: {
        if (viewMode === "day") {
            return selectedDateValue < todayValue
        }
        var weekEnd = new Date(selectedDateValue)
        weekEnd.setDate(weekEnd.getDate() + 6)
        return weekEnd < todayValue
    }
    property int greetingIndex: 0
    readonly property var greetingPool: [
        { icon: "👋", title: qsTr("欢迎回来"), subtitle: qsTr("Chrona 已为今天生成学习计划") },
        { icon: "☕", title: qsTr("今天也慢慢进入状态"), subtitle: qsTr("先看当前专注，再让时间块接住任务") },
        { icon: "🌙", title: qsTr("晚上好，适合收束一下"), subtitle: qsTr("把剩下的学习安排交给 Chrona 整理") },
        { icon: "✨", title: qsTr("新的时间线已经就位"), subtitle: qsTr("重要任务会优先占住清醒时段") },
        { icon: "🧭", title: qsTr("今天从一个清晰目标开始"), subtitle: qsTr("Chrona 会避开冲突并动态重排") },
        { icon: "📚", title: qsTr("学习节奏已准备好"), subtitle: qsTr("先完成眼前这一块，其他交给 Scheduler") },
        { icon: "⚡", title: qsTr("进入高效模式"), subtitle: qsTr("高优先级任务会被放到更合适的位置") },
        { icon: "🌿", title: qsTr("保持轻一点的节奏"), subtitle: qsTr("日程会跟着任务变化自动调整") },
        { icon: "🎯", title: qsTr("聚焦最重要的一件事"), subtitle: qsTr("当前专注会告诉你现在该做什么") },
        { icon: "🪄", title: qsTr("让计划自己长出来"), subtitle: qsTr("输入一句话，Chrona 会生成可确认的草稿") }
    ]

    Component.onCompleted: {
        ScheduleService.goToToday()
        greetingIndex = Math.floor(Math.random() * greetingPool.length)
    }

    function formatFocusTime(seconds) {
        var m = Math.floor(seconds / 60)
        var s = seconds % 60
        return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0")
    }

    function endFocus(completeTask) {
        focusExitConfirmPopup.close()
        var result = ({ ok: true, message: "" })
        if (completeTask && ScheduleService.focusItem.blockId > 0) {
            result = ScheduleService.completeBlock(ScheduleService.focusItem.blockId)
        } else {
            result = ScheduleService.stopFocus()
        }
        if (result && result.ok === false) {
            quickAddToast.kind = "danger"
            quickAddToast.text = result.message || qsTr("操作失败")
            quickAddToast.open()
            return
        }
        root.focusActive = false
        focusPanel.active = false
        focusTimer.stop()
        if (result && result.message) {
            quickAddToast.kind = "success"
            quickAddToast.text = result.message
            quickAddToast.open()
        }
    }

    function showOperationMessage(message, ok) {
        quickAddToast.kind = ok ? "success" : "danger"
        quickAddToast.text = message || (ok ? qsTr("调整已执行") : qsTr("操作失败"))
        quickAddToast.open()
    }

    function beginFocusSession() {
        var item = ScheduleService.focusItem || ({})
        root.focusStartMs = Number(item.startMs || Date.now())
        root.focusEndMs = Number(item.endMs || (Date.now() + 25 * 60 * 1000))
        root.focusDurationSeconds = Math.max(60, Math.round((root.focusEndMs - root.focusStartMs) / 1000))
        root.updateFocusClock()
        root.focusActive = true
        focusPanel.active = true
        focusTimer.restart()
    }

    function updateFocusClock() {
        var now = Date.now()
        root.focusRemainingSeconds = Math.max(0, Math.ceil((root.focusEndMs - now) / 1000))
        root.focusCanComplete = root.focusEndMs > 0 && now >= root.focusEndMs
    }

    function requestEndFocus() {
        if (root.focusActive) {
            focusExitConfirmPopup.open()
        }
    }

    function scrollToFocusBlock() {
        var focusItem = ScheduleService.focusItem
        if (!focusItem || !focusItem.taskId || focusItem.taskId <= 0) {
            console.log("No focus item available")
            return
        }

        console.log("Scrolling to focus block for task:", focusItem.taskId)
        root.viewMode = "week"
        if (focusItem.blockId && focusItem.blockId > 0) {
            ScheduleService.selectTimelineItem(focusItem.taskId, focusItem.blockId)
        } else {
            ScheduleService.selectTask(focusItem.taskId)
        }

        if (!timelineView) {
            console.log("timelineView not found")
            return
        }

        var blockId = focusItem.blockId || 0
        var model = ScheduleService.timelineModel

        if (!model) {
            console.log("Timeline model not found")
            return
        }

        var count = model.rowCount()
        var targetY = 8 * 60 * root.timelineMinuteHeight

        var IdRole = 257
        var TaskIdRole = 258
        var DayIndexRole = 264
        var StartMinuteRole = 265

        for (var i = 0; i < count; i++) {
            try {
                var index = model.index(i, 0)
                var itemId = model.data(index, IdRole)
                var itemDayIndex = model.data(index, DayIndexRole)
                var itemStartMinute = model.data(index, StartMinuteRole)
                var itemTaskId = model.data(index, TaskIdRole)

                var isCurrentView = root.viewMode === "week"
                    ? (itemDayIndex >= 0 && itemDayIndex < 7)
                    : (itemDayIndex === 0)

                if (isCurrentView && (itemId === blockId || (blockId === 0 && itemTaskId === focusItem.taskId))) {
                    targetY = (itemStartMinute - timelineView.dayStartMinute) * root.timelineMinuteHeight
                    console.log("Found block at Y:", targetY)
                    break
                }
            } catch(e) {
                console.log("Error accessing model data at index", i, ":", e)
            }
        }

        var flickable = timelineView.flickable
        if (flickable) {
            var targetContentY = Math.max(0, targetY - flickable.height / 3)
            console.log("Scrolling to Y:", targetContentY)
            flickable.contentY = targetContentY
        } else {
            console.log("Flickable not found")
        }
    }

    function scrollToCourses() {
        var model = ScheduleService.timelineModel
        if (!model || !timelineView) {
            return
        }

        var IdRole = 257
        var TaskIdRole = 258
        var DayIndexRole = 264
        var StartMinuteRole = 265
        var IsEventRole = 269
        var count = model.rowCount()
        var courseId = 0
        var courseTaskId = 0
        var courseDayIndex = 2147483647
        var courseStartMinute = 2147483647

        for (var i = 0; i < count; i++) {
            var index = model.index(i, 0)
            if (!model.data(index, IsEventRole)) {
                continue
            }

            var dayIndex = model.data(index, DayIndexRole)
            var startMinute = model.data(index, StartMinuteRole)
            if (dayIndex < 0 || dayIndex >= 7) {
                continue
            }
            if (dayIndex < courseDayIndex
                    || (dayIndex === courseDayIndex && startMinute < courseStartMinute)) {
                courseId = model.data(index, IdRole)
                courseTaskId = model.data(index, TaskIdRole)
                courseDayIndex = dayIndex
                courseStartMinute = startMinute
            }
        }

        if (courseId === 0) {
            return
        }

        root.viewMode = "week"
        ScheduleService.selectTimelineItem(courseTaskId, courseId)
        var flickable = timelineView.flickable
        if (flickable) {
            var targetY = (courseStartMinute - timelineView.dayStartMinute) * root.timelineMinuteHeight
            flickable.contentY = Math.max(0, targetY - flickable.height / 3)
        }
    }

    function editableDraftFromFields() {
        var durationValue = Number(String(aiDraftDurationText.text).replace(/[^0-9]/g, ""))
        if (isNaN(durationValue) || durationValue <= 0) {
            durationValue = root.aiDraft.estimatedMinutes || 60
        }
        return {
            title: aiDraftTitle.text,
            notes: aiDraftNotes.text,
            deadline: aiDraftDeadline.text,
            estimatedMinutes: Math.max(30, durationValue),
            priority: aiDraftPriority.selectedIndex,
            categoryName: aiDraftCategory.text,
            preferredStudyTime: root.aiDraft.preferredStudyTime || "evening",
            planningMode: root.aiDraft.planningMode || "task_deadline",
            hasTimeAnchor: !!root.aiDraft.hasTimeAnchor,
            scheduledStart: root.aiDraft.scheduledStart || "",
            scheduledEnd: root.aiDraft.scheduledEnd || "",
            source: root.aiDraft.source || "local",
            explanation: aiDraftReason.text
        }
    }

    function submitAiDraft() {
        var result = ScheduleService.createTaskFromDraft(editableDraftFromFields())
        aiDraftPopup.close()
        quickAddToast.kind = result && result.ok ? "success" : "danger"
        quickAddToast.text = result && result.message ? result.message : qsTr("已处理")
        quickAddToast.open()
    }

    function draftTimeText(draft) {
        if (!draft) {
            return ""
        }
        if (draft.scheduledStart && draft.scheduledEnd) {
            return draft.scheduledStart + " - " + String(draft.scheduledEnd).slice(11, 16)
        }
        return draft.deadline || ""
    }

    function draftReasonText(draft) {
        if (!draft) {
            return ""
        }
        var status = draft.scheduleStatusText || ""
        var explanation = draft.explanation || ""
        return status.length > 0 ? status + (explanation.length > 0 ? "\n" + explanation : "") : explanation
    }

    function removeDraftAt(index) {
        var drafts = (root.aiDrafts || []).slice(0)
        if (index < 0 || index >= drafts.length) {
            return
        }
        drafts.splice(index, 1)
        root.aiDrafts = drafts
        if (drafts.length === 0) {
            aiDraftPopup.close()
            return
        }
        root.aiDraft = drafts[0]
    }

    function approveDraftAt(index) {
        var drafts = root.aiDrafts || []
        if (index < 0 || index >= drafts.length) {
            return
        }
        var result = ScheduleService.createTaskFromDraft(drafts[index])
        quickAddToast.kind = result && result.ok ? "success" : "danger"
        quickAddToast.text = result && result.message ? result.message : qsTr("已处理")
        quickAddToast.open()
        if (result && result.ok) {
            removeDraftAt(index)
        }
    }

    function approveAllDrafts() {
        var drafts = (root.aiDrafts || []).filter(function(draft) { return draft && draft.scheduleAvailable !== false })
        var result = ScheduleService.createTasksFromDrafts(drafts)
        aiDraftPopup.close()
        quickAddToast.kind = result && result.ok ? "success" : "danger"
        quickAddToast.text = result && result.message ? result.message : qsTr("已处理")
        quickAddToast.open()
    }
    function showTaskDraftResult(result) {
        root.aiDraftLoading = false
        if (result && result.ok) {
            root.aiDraft = result.draft || ({})
            root.aiDrafts = result.drafts || [root.aiDraft]
            root.aiDraftIndex = 0
            aiDraftTitle.text = root.aiDraft.title || ""
            aiDraftDeadline.text = root.aiDraft.deadline || ""
            aiDraftDurationText.text = String(Math.max(30, root.aiDraft.estimatedMinutes || 60))
            aiDraftPriority.selectedIndex = Math.max(0, Math.min(2, root.aiDraft.priority || 1))
            aiDraftCategory.text = root.aiDraft.categoryName || qsTr("学习")
            aiDraftNotes.text = root.aiDraft.notes || ""
            var scheduleStatus = root.aiDraft.scheduleStatusText || ""
            var explanation = root.aiDraft.explanation || result.message || ""
            aiDraftReason.text = scheduleStatus.length > 0
                ? scheduleStatus + (explanation.length > 0 ? "\n" + explanation : "")
                : explanation
            aiDraftSource.text = root.aiDrafts.length > 1 ? qsTr("Chrona AI 批量草稿") : ((result.source === "deepseek") ? qsTr("Chrona AI 规划草稿") : qsTr("本地规则草稿"))
            if (!aiDraftPopup.opened) {
                aiDraftPopup.open()
            }
        } else {
            aiDraftPopup.close()
            quickAddToast.kind = "danger"
            quickAddToast.text = result && result.message ? result.message : qsTr("解析失败")
            quickAddToast.open()
        }
    }

    Timer {
        id: focusTimer
        interval: 1000
        repeat: true
        running: root.focusActive
        triggeredOnStart: true
        onTriggered: {
            root.updateFocusClock()
        }
    }

    Connections {
        target: ScheduleService
        function onTaskDraftReady(result) {
            root.showTaskDraftResult(result)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding
        anchors.topMargin: 22
        anchors.bottomMargin: 18
        spacing: 12

        Rectangle {
            id: topHeader
            Layout.fillWidth: true
            Layout.preferredHeight: compact ? 64 : 72

            readonly property bool compact: width < 980
            readonly property bool dense: width < 820

            radius: 14

            color: Theme.surfaceElevated
            clip: true

            border.width: 1
            border.color: Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.margins: topHeader.compact ? 10 : 12
                spacing: topHeader.compact ? 8 : 10

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 150
                    spacing: 3

                    Text {
                        text: greetingPool[greetingIndex].icon + " " + greetingPool[greetingIndex].title
                        color: Theme.strongText
                        font.pixelSize: topHeader.compact ? 15 : 18
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: !topHeader.dense
                        text: greetingPool[greetingIndex].subtitle
                        color: Theme.secondaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: false
                        text: Qt.formatDate(new Date(), "yyyy-MM-dd")
                        color: Theme.mutedText
                        font.pixelSize: 11
                    }
                }

                DensityControl {
                    visible: topHeader.width > 620
                    preferredWidth: topHeader.compact ? 126 : 172
                    label: qsTr("行距")
                    value: root.timelineMinuteHeight
                    from: 0.5
                    to: 1.75
                    onValueChangedByUser: function(v) {
                        root.timelineMinuteHeight = v
                    }
                    valueLabel: root.timelineMinuteHeight <= 0.52
                                ? qsTr("24h")
                                : root.timelineMinuteHeight < 1.08
                                  ? qsTr("紧")
                                : root.timelineMinuteHeight > 1.42
                                  ? qsTr("松")
                                  : qsTr("中")
                }

                DensityControl {
                    visible: topHeader.width > 700
                    preferredWidth: topHeader.compact ? 126 : 172
                    label: qsTr("列距")
                    value: root.timelineColumnWidth
                    from: 140
                    to: 260
                    onValueChangedByUser: function(v) {
                        root.timelineColumnWidth = v
                    }
                    valueLabel: root.timelineColumnWidth < 170
                                ? qsTr("紧")
                                : root.timelineColumnWidth > 225
                                  ? qsTr("松")
                                  : qsTr("中")
                }

                SegmentedControl {
                    Layout.preferredWidth: 112
                    Layout.minimumWidth: 112
                    Layout.maximumWidth: 112
                    options: [qsTr("日"), qsTr("周")]
                    selectedIndex: root.viewMode === "day" ? 0 : 1
                    onSelectedIndexChanged: {
                        root.viewMode = selectedIndex === 0
                                        ? "day"
                                        : "week"
                    }
                }

                ThemeToggle {
                    visible: true
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
                root.beginFocusSession()
            }
            onFocusStopRequested: root.requestEndFocus()
            onRescheduleRequested: {
                var result = ScheduleService.reschedule()
                quickAddToast.kind = result.ok ? "success" : "warning"
                quickAddToast.text = result.message || qsTr("已重新规划")
                quickAddToast.open()
            }
        }

        Rectangle {
            visible: false
            Layout.fillWidth: true
            Layout.preferredHeight: 0
            radius: 8
            color: Theme.surface
            border.width: 1
            border.color: Theme.dangerBorder

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                Rectangle {
                    width: 5
                    Layout.fillHeight: true
                    radius: 3
                    color: Theme.error
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("调度冲突")
                            color: Theme.error
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: qsTr("%1 个任务无法完整排入").arg(ScheduleService.unscheduledCount)
                            color: Theme.secondaryText
                            font.pixelSize: 12
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            var issues = ScheduleService.scheduleIssues || []
                            if (issues.length === 0) {
                                return qsTr("请调整截止时间、减少预计时长，或扩大可学习时间。")
                            }
                            var first = issues[0]
                            return qsTr("%1：%2，剩余 %3 分钟未安排")
                                .arg(first.title || qsTr("任务"))
                                .arg(first.reason || qsTr("容量不足"))
                                .arg(first.remainingMinutes || 0)
                        }
                        color: Theme.primaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 36
                    radius: 8
                    color: issueMouse.containsMouse ? Theme.surfaceHover : Theme.canvasBackground
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("重新规划")
                        color: Theme.primaryText
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: issueMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            var result = ScheduleService.reschedule()
                            quickAddToast.kind = result.ok ? "success" : "warning"
                            quickAddToast.text = result.message || qsTr("已重新规划")
                            quickAddToast.open()
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: false
            Layout.fillWidth: true
            Layout.preferredHeight: 0

            radius: 14

            color: Theme.surfaceElevated

            border.width: 1
            border.color: Theme.accent

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16

                Text {
                    text: "🤖"
                    font.pixelSize: 28
                }

                ColumnLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "AI 推荐"
                        color: Theme.strongText
                        font.bold: true
                    }

                    Text {
                        text: "14:00 - 15:30 适合高认知学习"
                        color: Theme.secondaryText
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
            visible: !root.historicalView
            capacityStats: ScheduleService.capacityStats
            aiStatus: ScheduleService.aiStatus
            onRecommendationRequested: {
                dailyPlanOverlay.mode = "suggestions"
                dailyPlanOverlay.visible = true
            }
            onAddRequested: function(text) {
                root.aiDraftLoading = true
                root.aiDraft = ({ planningMode: "thinking" })
                root.aiDrafts = []
                aiDraftTitle.text = ""
                aiDraftDeadline.text = ""
                aiDraftDurationText.text = "60"
                aiDraftCategory.text = ""
                aiDraftNotes.text = ""
                aiDraftReason.text = qsTr("Chrona AI 正在理解你的时间意图...")
                aiDraftSource.text = qsTr("Chrona AI 正在规划")
                aiDraftPopup.open()
                ScheduleService.requestTaskDraft(text)
            }
            onImagePreviewRequested: function(fileUrl) {
                var preview = ScheduleService.previewImageTask(fileUrl)
                ocrText.text = preview.recognizedText || ""
                ocrMessage.text = preview.message || ""
                ocrPopup.open()
            }
            onAiConfigRequested: {
                deepSeekApiKeyField.text = ""
                aiConfigStatus.text = ScheduleService.aiStatus.configured
                    ? qsTr("Chrona AI 当前已连接。留空保存可切回本地规则模式。")
                    : qsTr("填入 Chrona AI Key 后，Quick Add 会优先使用 AI 解析。")
                aiConfigStatus.color = Theme.secondaryText
                aiConfigPopup.open()
            }
        }

        TimelineView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            viewMode: root.viewMode
            model: ScheduleService.timelineModel
            minuteHeight: root.timelineMinuteHeight
            requestedDayColumnWidth: root.timelineColumnWidth
            anchorDate: ScheduleService.selectedDateText
            readOnly: root.historicalView
            onMoveRejected: function(message) {
                quickAddToast.kind = "danger"
                quickAddToast.text = message
                quickAddToast.open()
            }
        }

        Rectangle {
            visible: root.historicalView
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            radius: 10
            color: Theme.surfaceElevated
            border.width: 1
            border.color: Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: qsTr("正在查看 %1 的日程，浏览模式不会改动排程").arg(ScheduleService.selectedDateText)
                    color: Theme.secondaryText
                    font.pixelSize: 12
                }

                Rectangle {
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 30
                    radius: 8
                    color: returnTodayMouse.containsMouse ? Theme.accentBright : Theme.accent
                    Text { anchors.centerIn: parent; text: qsTr("回到今天"); color: "white"; font.pixelSize: 12; font.weight: Font.DemiBold }
                    MouseArea {
                        id: returnTodayMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: ScheduleService.setSelectedDate(Qt.formatDate(new Date(), "yyyy-MM-dd"))
                    }
                }
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
            color: Theme.appBackground
        }

        Item {
            id: ambientField
            anchors.fill: parent

            Rectangle {
                id: ambientGlowA
                width: Math.min(620, parent.width * 0.5)
                height: width
                radius: width / 2
                x: parent.width * 0.18
                y: parent.height * 0.18
                color: ScheduleService.focusItem.color || Theme.accentBright
                opacity: 0.16
                scale: 1.0

                SequentialAnimation on scale {
                    running: root.focusActive
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.08; duration: 4200; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 0.96; duration: 4200; easing.type: Easing.InOutSine }
                }
            }

            Rectangle {
                id: ambientGlowB
                width: Math.min(460, parent.width * 0.36)
                height: width
                radius: width / 2
                x: parent.width * 0.58
                y: parent.height * 0.55
                color: "#65D6A6"
                opacity: 0.10
                scale: 1.0
                SequentialAnimation on scale {
                    running: root.focusActive
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.94; duration: 5200; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.07; duration: 5200; easing.type: Easing.InOutSine }
                }
            }
        }

        Rectangle {
            id: focusGlass
            z: 1
            anchors.centerIn: parent
            width: Math.min(640, parent.width - 80)
            height: Math.min(500, parent.height - 96)
            radius: 34
            color: Theme.glassSurface
            border.width: 1
            border.color: Theme.glassBorder
            clip: true

            Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.dark ? "#2A20283A" : "#B8FFFFFF" }
                GradientStop { position: 0.48; color: Theme.dark ? "#1A111722" : "#96F8FAFD" }
                GradientStop { position: 1.0; color: Theme.dark ? "#30101622" : "#AAE7EDF5" }
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.width: 1
                border.color: Theme.glassBorder
                opacity: 0.75
            }

            Rectangle {
                id: glassSheen
                width: parent.width * 0.35
                height: parent.height * 1.5
                x: -width
                y: -parent.height * 0.2
                rotation: -18
                opacity: 0.18
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#00FFFFFF" }
                    GradientStop { position: 0.5; color: Theme.glassHighlight }
                    GradientStop { position: 1.0; color: "#00FFFFFF" }
                }

                SequentialAnimation on x {
                    running: root.focusActive
                    loops: Animation.Infinite
                    PauseAnimation { duration: 1800 }
                    NumberAnimation { to: focusGlass.width + glassSheen.width; duration: 2600; easing.type: Easing.InOutCubic }
                    PropertyAction { value: -glassSheen.width }
                }
            }

            ColumnLayout {
                id: focusContent
                anchors.fill: parent
                anchors.margins: 30
                spacing: 15

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: qsTr("FOCUS SESSION")
                            color: Theme.tertiaryText
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 2
                        }

                        Text {
                            Layout.fillWidth: true
                            text: ScheduleService.focusItem.title || qsTr("当前任务")
                            color: Theme.primaryText
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 112
                        Layout.preferredHeight: 34
                        radius: 17
                        color: Theme.surfaceMuted
                        border.width: 1
                        border.color: Theme.border

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("沉浸专注")
                            color: Theme.accentBright
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(300, focusGlass.width - 96)
                    Layout.preferredHeight: Layout.preferredWidth

                    readonly property real progress: root.focusDurationSeconds > 0
                                                     ? Math.max(0, Math.min(1, 1 - root.focusRemainingSeconds / root.focusDurationSeconds))
                                                     : 0

                    Canvas {
                        id: focusRing
                        anchors.fill: parent
                        antialiasing: true
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            var cx = width / 2
                            var cy = height / 2
                            var radius = Math.min(width, height) / 2 - 13
                            ctx.lineCap = "round"
                            ctx.lineWidth = 10
                            ctx.strokeStyle = "rgba(255,255,255,0.08)"
                            ctx.beginPath()
                            ctx.arc(cx, cy, radius, 0, Math.PI * 2)
                            ctx.stroke()

                            var accent = ScheduleService.focusItem.color || Theme.accentBright
                            ctx.strokeStyle = accent
                            ctx.shadowBlur = 18
                            ctx.shadowColor = accent
                            ctx.beginPath()
                            ctx.arc(cx, cy, radius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * parent.progress)
                            ctx.stroke()
                        }
                    }

                    Connections {
                        target: root
                        function onFocusRemainingSecondsChanged() { focusRing.requestPaint() }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.74
                        height: width
                        radius: width / 2
                        color: Theme.glassSurface
                        border.width: 1
                        border.color: Theme.glassBorder
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            id: focusTimerText
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.formatFocusTime(root.focusRemainingSeconds)
                            color: Theme.strongText
                            font.pixelSize: 64
                            font.weight: Font.Light
                            font.family: "Consolas"
                            font.letterSpacing: 1
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: ScheduleService.focusItem.subtitle || qsTr("保持当前节奏")
                            color: Theme.secondaryText
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            width: Math.min(300, focusGlass.width - 120)
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Theme.border
                    opacity: 0.9
                    clip: true

                    Rectangle {
                        height: parent.height
                        width: parent.width * (root.focusDurationSeconds > 0
                                               ? Math.max(0, Math.min(1, 1 - root.focusRemainingSeconds / root.focusDurationSeconds))
                                               : 0)
                        radius: 3
                        color: ScheduleService.focusItem.color || Theme.accentBright
                        Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("把注意力留在这一段，Chrona 会替你照看剩下的时间线。")
                    color: Theme.secondaryText
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12

                    FocusButton { text: qsTr("退出专注"); muted: true; onClicked: root.requestEndFocus() }
                    FocusButton {
                        text: root.focusCanComplete ? qsTr("完成") : qsTr("时间未结束")
                        enabled: root.focusCanComplete
                        opacity: enabled ? 1 : 0.46
                        onClicked: root.endFocus(true)
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                anchors.bottomMargin: 28
                height: 1
                color: "#55FFFFFF"
                opacity: 0.22
            }
        }

        Rectangle {
            anchors.centerIn: focusGlass
            width: focusGlass.width + 24
            height: focusGlass.height + 24
            radius: focusGlass.radius + 12
            color: "transparent"
            border.width: 1
            border.color: ScheduleService.focusItem.color || Theme.accentBright
            opacity: 0.18
            z: 0
            scale: 1.0

            SequentialAnimation on scale {
                running: root.focusActive
                loops: Animation.Infinite
                NumberAnimation { to: 1.025; duration: 3000; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 3000; easing.type: Easing.InOutSine }
            }

            SequentialAnimation on opacity {
                running: root.focusActive
                loops: Animation.Infinite
                NumberAnimation { to: 0.30; duration: 3000; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.14; duration: 3000; easing.type: Easing.InOutSine }
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
                text: qsTr("退出专注？")
                color: Theme.primaryText
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("当前专注尚未完成，确定要退出吗？进度将不会保存。")
                color: Theme.secondaryText
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                lineHeight: 1.35
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                FocusButton { text: qsTr("继续专注"); onClicked: focusExitConfirmPopup.close() }
                FocusButton { text: qsTr("退出"); muted: true; onClicked: root.endFocus(false) }
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
            color: quickAddToast.kind === "danger" ? Theme.dangerSurface : Theme.successSurface
            border.width: 1
            border.color: quickAddToast.kind === "danger" ? Theme.dangerBorder : Theme.successBorder
        }
        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 28
            color: quickAddToast.kind === "danger" ? Theme.error : Theme.success
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }

    Popup {
        id: aiDraftPopup
        property point dragOffset: Qt.point(0, 0)
        property bool dragging: false
        width: 560
        height: 540
        modal: true
        focus: true
        x: Math.round(((parent ? parent.width : root.width) - width) / 2)
        y: Math.round(((parent ? parent.height : root.height) - height) / 2)
        onOpened: {
            x = Math.round(((parent ? parent.width : root.width) - width) / 2)
            y = Math.round(((parent ? parent.height : root.height) - height) / 2)
        }
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0
        scale: dragging ? 1.012 : 1.0
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.975; to: 1; duration: 220; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 130; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.985; duration: 130; easing.type: Easing.InCubic }
        }
        background: Rectangle {
            id: aiGlass
            property real breath: 0
            radius: 18
            color: Theme.glassSurface
            border.width: 1
            border.color: aiDraftPopup.dragging ? Theme.accentBright : Theme.infoBorder
            clip: true
            Behavior on border.color { ColorAnimation { duration: 160 } }

            SequentialAnimation on breath {
                loops: Animation.Infinite
                NumberAnimation { from: 0; to: 1; duration: 1800; easing.type: Easing.InOutSine }
                NumberAnimation { from: 1; to: 0; duration: 1800; easing.type: Easing.InOutSine }
            }

            Rectangle {
                x: -90 + aiGlass.breath * 18
                y: -90
                width: 230
                height: 230
                radius: 115
                color: Theme.accentBright
                opacity: 0.11 + aiGlass.breath * 0.08
                scale: 1 + aiGlass.breath * 0.035
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.dark ? "#AFC5FF" : "#405AA8"
                opacity: 0.34 + aiGlass.breath * 0.24
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 17
                color: Theme.glassSurface
            }
        }

        ColumnLayout {
            z: 2
            anchors.fill: parent
            anchors.margins: 26
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    id: aiOrb
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: 21
                    color: Theme.infoSurface
                    border.width: 1
                    border.color: Theme.accentBright
                    opacity: 0.92
                    scale: 1 + aiGlass.breath * 0.035

                    Text {
                        anchors.centerIn: parent
                        text: "AI"
                        color: Theme.accentBright
                        font.pixelSize: 13
                        font.weight: Font.Black
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        id: aiDraftSource
                        Layout.fillWidth: true
                        text: qsTr("Chrona AI 规划草稿")
                        color: Theme.accentText
                        font.pixelSize: 21
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("AI 已理解意图，确认后才写入数据库")
                        color: Theme.secondaryText
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 30
                    radius: 15
                    color: Theme.surfaceMuted
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: root.aiDraft.planningMode === "direct_time_block" ? qsTr("时间块") : qsTr("任务")
                        color: Theme.success
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
                opacity: 0.8
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.aiDrafts.length > 1
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("识别到 %1 个时间意图，可逐个确认或全部加入").arg(root.aiDrafts.length)
                        color: Theme.secondaryText
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: root.aiDrafts
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 112
                            radius: 14
                            color: modelData.scheduleAvailable === false ? Theme.dangerSurface : Theme.surface
                            border.width: 1
                            border.color: modelData.scheduleAvailable === false ? Theme.dangerBorder : Theme.border

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 12

                                Rectangle {
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                                    radius: 17
                                    color: Theme.infoSurface
                                    border.width: 1
                                    border.color: Theme.accentBright
                                    Text {
                                        anchors.centerIn: parent
                                        text: index + 1
                                        color: Theme.accentBright
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.title || qsTr("未命名时间块")
                                        color: Theme.primaryText
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.draftTimeText(modelData)
                                        color: Theme.secondaryText
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: (modelData.categoryName || qsTr("学习")) + " · " + Math.max(30, modelData.estimatedMinutes || 60) + qsTr(" 分钟")
                                        color: Theme.tertiaryText
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: root.draftReasonText(modelData).length > 0
                                        text: root.draftReasonText(modelData)
                                        color: modelData.scheduleAvailable === false ? Theme.error : Theme.success
                                        font.pixelSize: 11
                                        maximumLineCount: 1
                                        elide: Text.ElideRight
                                    }
                                }

                                ColumnLayout {
                                    Layout.preferredWidth: 84
                                    spacing: 8
                                    FocusButton {
                                        Layout.preferredWidth: 84
                                        Layout.preferredHeight: 34
                                        text: qsTr("加入")
                                        enabled: modelData.scheduleAvailable !== false
                                        onClicked: root.approveDraftAt(index)
                                    }
                                    FocusButton {
                                        Layout.preferredWidth: 84
                                        Layout.preferredHeight: 34
                                        text: qsTr("取消")
                                        muted: true
                                        onClicked: root.removeDraftAt(index)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            TextField { id: aiDraftTitle; visible: root.aiDrafts.length <= 1; Layout.fillWidth: true; placeholderText: qsTr("任务标题"); color: Theme.primaryText; background: PopupFieldBackground {} }
            TextField { id: aiDraftDeadline; visible: root.aiDrafts.length <= 1; Layout.fillWidth: true; placeholderText: "yyyy-MM-dd HH:mm"; color: Theme.primaryText; background: PopupFieldBackground {} }

            RowLayout {
                visible: root.aiDrafts.length <= 1
                Layout.fillWidth: true
                TextField {
                    id: aiDraftDurationText
                    Layout.fillWidth: true
                    text: "60"
                    color: Theme.primaryText
                    placeholderText: qsTr("分钟")
                    validator: IntValidator { bottom: 30; top: 720 }
                    background: PopupFieldBackground {}
                    rightPadding: 54
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("分钟")
                        color: Theme.tertiaryText
                        font.pixelSize: 12
                    }
                }
                SegmentedControl {
                    id: aiDraftPriority
                    options: [qsTr("低"), qsTr("中"), qsTr("高")]
                    selectedIndex: 1
                }
            }

            TextField { id: aiDraftCategory; visible: root.aiDrafts.length <= 1; Layout.fillWidth: true; placeholderText: qsTr("课程分类"); color: Theme.primaryText; background: PopupFieldBackground {} }
            TextArea {
                id: aiDraftNotes
                visible: root.aiDrafts.length <= 1
                Layout.fillWidth: true
                Layout.preferredHeight: 74
                color: Theme.primaryText
                wrapMode: TextEdit.WordWrap
                placeholderText: ""
                background: PopupFieldBackground {}

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 14
                    anchors.topMargin: 10
                    text: qsTr("备注")
                    color: Theme.mutedText
                    font.pixelSize: 12
                    visible: aiDraftNotes.text.length === 0
                    z: 2
                }
            }
            Rectangle {
                visible: root.aiDrafts.length <= 1
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                radius: 12
                color: Theme.glassSurface
                border.width: 1
                border.color: Theme.border

                Text {
                    id: aiDraftReason
                    anchors.fill: parent
                    anchors.margins: 14
                    color: Theme.secondaryText
                    font.pixelSize: 12
                    lineHeight: 1.22
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                }
            }

            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: root.aiDrafts.length > 1
                        ? qsTr("可逐个确认，也可以一次加入所有可用草稿")
                        : qsTr("Chrona AI 只生成草稿，不直接改数据库")
                    color: Theme.tertiaryText
                    font.pixelSize: 11
                }
                FocusButton {
                    text: root.aiDrafts.length > 1 ? qsTr("全部取消") : qsTr("取消")
                    muted: true
                    onClicked: aiDraftPopup.close()
                }
                FocusButton {
                    text: root.aiDrafts.length > 1
                        ? qsTr("全部加入")
                        : (root.aiDraft.scheduleAvailable === false ? "\u6682\u65e0\u53ef\u7528\u65f6\u6bb5" : "\u52a0\u5165\u65e5\u7a0b")
                    enabled: root.aiDrafts.length > 1
                        ? root.aiDrafts.some(function(draft) { return draft && draft.scheduleAvailable !== false })
                        : root.aiDraft.scheduleAvailable !== false
                    onClicked: root.aiDrafts.length > 1 ? root.approveAllDrafts() : root.submitAiDraft()
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 18
            color: Theme.glassSurface
            visible: root.aiDraftLoading
            opacity: root.aiDraftLoading ? 1 : 0
            z: 20
            Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea { anchors.fill: parent }

            Column {
                anchors.centerIn: parent
                spacing: 16

                Item {
                    width: 48
                    height: 48
                    anchors.horizontalCenter: parent.horizontalCenter

                    Rectangle {
                        anchors.fill: parent
                        radius: 24
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.border
                    }

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: Theme.accentBright
                        x: 19
                        y: -1
                    }

                    RotationAnimation on rotation {
                        running: root.aiDraftLoading
                        loops: Animation.Infinite
                        from: 0
                        to: 360
                        duration: 900
                        easing.type: Easing.Linear
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Chrona AI 正在整理时间意图")
                    color: Theme.primaryText
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                Text {
                    width: 260
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("会先生成草稿，确认后才写入数据库")
                    color: Theme.tertiaryText
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 76
            z: 4
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: {
                aiDraftPopup.dragging = true
                aiDraftPopup.dragOffset = Qt.point(mouse.x, mouse.y)
            }
            onReleased: aiDraftPopup.dragging = false
            onCanceled: aiDraftPopup.dragging = false
            onPositionChanged: {
                if (!pressed) {
                    return
                }
                var maxX = (aiDraftPopup.parent ? aiDraftPopup.parent.width : root.width) - aiDraftPopup.width - 12
                var maxY = (aiDraftPopup.parent ? aiDraftPopup.parent.height : root.height) - aiDraftPopup.height - 12
                aiDraftPopup.x = Math.max(12, Math.min(maxX, aiDraftPopup.x + mouse.x - aiDraftPopup.dragOffset.x))
                aiDraftPopup.y = Math.max(12, Math.min(maxY, aiDraftPopup.y + mouse.y - aiDraftPopup.dragOffset.y))
            }
        }
    }

    Popup {
        id: aiConfigPopup
        width: 460
        height: 278
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupBackground {}

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 13

            Text {
                Layout.fillWidth: true
                text: qsTr("AI 接入设置")
                color: Theme.primaryText
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Chrona 只让 AI 解析任务草稿。保存并确认后，任务才会写入数据库并进入 Scheduler。")
                color: Theme.secondaryText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                lineHeight: 1.35
            }

            TextField {
                id: deepSeekApiKeyField
                Layout.fillWidth: true
                placeholderText: qsTr("Chrona AI Key")
                echoMode: TextInput.Password
                color: Theme.primaryText
                placeholderTextColor: Theme.mutedText
                background: PopupFieldBackground {}
            }

            Text {
                id: aiConfigStatus
                Layout.fillWidth: true
                color: Theme.secondaryText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FocusButton { text: qsTr("取消"); muted: true; onClicked: aiConfigPopup.close() }
                FocusButton {
                    text: qsTr("保存")
                    onClicked: {
                        var result = ScheduleService.setDeepSeekApiKey(deepSeekApiKeyField.text)
                        aiConfigStatus.color = result.ok ? Theme.success : Theme.error
                        aiConfigStatus.text = result.message || ""
                        if (result.ok) {
                            quickAddToast.kind = "success"
                            quickAddToast.text = result.message || qsTr("AI 配置已保存")
                            quickAddToast.open()
                            aiConfigPopup.close()
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
        background: PopupBackground {}

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14
            Text { text: qsTr("OCR 识别结果"); color: Theme.primaryText; font.pixelSize: 20; font.weight: Font.DemiBold }
            Text { id: ocrMessage; Layout.fillWidth: true; color: Theme.secondaryText; font.pixelSize: 13; wrapMode: Text.WordWrap }
            TextArea { id: ocrText; Layout.fillWidth: true; Layout.fillHeight: true; color: Theme.primaryText; font.pixelSize: 14; wrapMode: TextEdit.WordWrap; background: PopupFieldBackground {} }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FocusButton { text: qsTr("取消"); muted: true; onClicked: ocrPopup.close() }
                FocusButton {
                    text: qsTr("解析")
                    onClicked: {
                        ocrPopup.close()
                        var result = ScheduleService.previewTaskDraft(ocrText.text)
                        if (result && result.ok) {
                            root.aiDraft = result.draft || ({})
                            root.aiDrafts = result.drafts || [root.aiDraft]
                            aiDraftTitle.text = root.aiDraft.title || ""
                            aiDraftDeadline.text = root.aiDraft.deadline || ""
                            aiDraftDurationText.text = String(Math.max(30, root.aiDraft.estimatedMinutes || 60))
                            aiDraftPriority.selectedIndex = Math.max(0, Math.min(2, root.aiDraft.priority || 1))
                            aiDraftCategory.text = root.aiDraft.categoryName || qsTr("学习")
                            aiDraftNotes.text = root.aiDraft.notes || ""
                            var scheduleStatus = root.aiDraft.scheduleStatusText || ""
            var explanation = root.aiDraft.explanation || result.message || ""
            aiDraftReason.text = scheduleStatus.length > 0
                ? scheduleStatus + (explanation.length > 0 ? "\n" + explanation : "")
                : explanation
                            aiDraftSource.text = root.aiDrafts.length > 1 ? qsTr("Chrona AI 批量草稿") : ((result.source === "deepseek") ? qsTr("Chrona AI 规划草稿") : qsTr("本地规则草稿"))
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
        property int preferredWidth: 188
        signal valueChangedByUser(real value)

        Layout.preferredWidth: preferredWidth
        Layout.preferredHeight: 38
        radius: 8
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8
            Text { text: density.label; color: Theme.secondaryText; font.pixelSize: 12; font.weight: Font.DemiBold }
            Slider { Layout.fillWidth: true; from: density.from; to: density.to; value: density.value; live: true; onMoved: density.valueChangedByUser(value) }
            Text { text: density.valueLabel; color: Theme.primaryText; font.pixelSize: 12; font.weight: Font.DemiBold }
        }
    }

    component PopupBackground: Rectangle {
        radius: 10
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    component PopupFieldBackground: Rectangle {
        radius: 10
        color: Theme.glassSurface
        border.width: 1
        border.color: Theme.border
    }

    component DarkPills: Rectangle {
        id: pills
        property var options: []
        property int currentIndex: 0

        Layout.preferredHeight: 42
        radius: 8
        color: Theme.surface
        border.width: 1
        border.color: Theme.surfaceHover

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

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: pills.currentIndex === index ? Theme.primaryText : Theme.tertiaryText
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
                text: "v"
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

    component FocusButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        property bool enabled: true
        signal clicked()

        width: label.text.length > 3 ? 104 : 76
        height: 38
        radius: 8
        color: !enabled ? Theme.controlDisabled : muted ? (mouse.containsMouse ? Theme.surfaceHover : Theme.surface) : (mouse.containsMouse ? Theme.accent : Theme.accentBright)
        border.width: muted ? 1 : 0
        border.color: Theme.border
        Behavior on color { ColorAnimation { duration: 140 } }
        Text { id: label; anchors.centerIn: parent; color: !button.enabled ? Theme.mutedText : muted ? Theme.secondaryText : "white"; font.pixelSize: 13; font.weight: Font.DemiBold }
        MouseArea { id: mouse; anchors.fill: parent; enabled: button.enabled; hoverEnabled: true; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: button.clicked() }
    }
}
