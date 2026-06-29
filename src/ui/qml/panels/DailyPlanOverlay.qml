import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root

    visible: false
    color: Theme.overlay
    z: 200

    property string mode: "daily"
    property string pendingMode: ""
    property var plan: ({})
    property var selectedSuggestion: ({})
    property bool loading: false
    property string executionMessage: ""
    property bool executionOk: false
    property point dragOffset: Qt.point(0, 0)

    signal executionFinished(bool ok, string message)

    function modeKey() {
        return mode === "suggestions" ? "schedule_suggestions" : "daily_plan"
    }

    function centerPanel() {
        panel.x = Math.round((root.width - panel.width) / 2)
        panel.y = Math.round((root.height - panel.height) / 2)
    }

    function startLoad() {
        loading = true
        pendingMode = modeKey()
        plan = {
            title: mode === "suggestions" ? qsTr("日程调整建议") : qsTr("今日计划"),
            summary: qsTr("Chrona AI 正在分析任务、冲突和可用容量。"),
            riskLevel: "low",
            currentFocus: "",
            reasons: [],
            suggestions: [],
            provider: "chrona",
            model: qsTr("Chrona AI")
        }
        selectedSuggestion = ({})
        executionMessage = ""
        executionOk = false
        centerPanel()
        ScheduleService.requestSchedulePlan(pendingMode)
    }

    function applyPlan(result) {
        loading = false
        plan = result || ({})
        selectedSuggestion = plan.suggestions && plan.suggestions.length > 0 ? plan.suggestions[0] : ({})
    }

    onVisibleChanged: if (visible) startLoad()

    Connections {
        target: ScheduleService
        function onSchedulePlanReady(mode, result) {
            if (!root.visible || mode !== root.pendingMode) {
                return
            }
            root.applyPlan(result)
        }
    }

    MouseArea { anchors.fill: parent }

    Rectangle {
        id: panel
        width: Math.min(root.width - 48, 780)
        height: Math.min(root.height - 48, 640)
        radius: 22
        color: Theme.glassSurface
        border.color: root.loading ? Theme.accentBright : Theme.border
        border.width: root.loading ? 1.4 : 1
        clip: true
        scale: dragArea.pressed ? 1.012 : 1.0

        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 180 } }
        Behavior on border.width { NumberAnimation { duration: 180 } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.dark ? "#3E1D2537" : "#D2FFFFFF" }
            GradientStop { position: 0.52; color: Theme.dark ? "#32111620" : "#B2F8FAFD" }
            GradientStop { position: 1.0; color: Theme.dark ? "#40101722" : "#C2E7EDF5" }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: Theme.glassBorder
        }

        MouseArea {
            id: dragArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 74
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: root.dragOffset = Qt.point(mouse.x, mouse.y)
            onPositionChanged: {
                if (!pressed) {
                    return
                }
                panel.x = Math.max(12, Math.min(root.width - panel.width - 12, panel.x + mouse.x - root.dragOffset.x))
                panel.y = Math.max(12, Math.min(root.height - panel.height - 12, panel.y + mouse.y - root.dragOffset.y))
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 46
                    radius: 16
                    color: Theme.surfaceHover
                    border.width: 1
                    border.color: "#5060FF"

                    Text {
                        anchors.centerIn: parent
                        text: "AI"
                        color: Theme.accentBright
                        font.pixelSize: 14
                        font.weight: Font.Black
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: root.plan.title || (root.mode === "suggestions" ? qsTr("日程调整建议") : qsTr("今日计划"))
                        color: Theme.primaryText
                        font.pixelSize: 25
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("来源：Chrona AI · 确认前不会修改任何数据")
                        color: Theme.tertiaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                PlanButton {
                    text: qsTr("关闭")
                    muted: true
                    onClicked: root.visible = false
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.loading ? 132 : 96
                radius: 16
                color: root.plan.riskLevel === "high" ? Theme.dangerSurface : Theme.surfaceMuted
                border.width: 1
                border.color: root.plan.riskLevel === "high" ? Theme.dangerBorder : Theme.border
                clip: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    Item {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        visible: root.loading

                        Rectangle {
                            anchors.fill: parent
                            radius: 21
                            color: "transparent"
                            border.width: 2
                            border.color: Theme.border
                        }

                        Rectangle {
                            width: 9
                            height: 9
                            radius: 5
                            color: Theme.accentBright
                            x: 16
                            y: -1
                        }

                        RotationAnimation on rotation {
                            running: root.loading
                            loops: Animation.Infinite
                            from: 0
                            to: 360
                            duration: 920
                            easing.type: Easing.Linear
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: root.loading ? qsTr("正在生成建议") : (root.plan.currentFocus || qsTr("当前建议"))
                            color: Theme.primaryText
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.plan.summary || qsTr("Chrona AI 会优先参考当前时间块、固定事件和未排入任务。")
                            color: Theme.secondaryText
                            font.pixelSize: 12
                            lineHeight: 1.25
                            wrapMode: Text.WordWrap
                            maximumLineCount: root.loading ? 3 : 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                InfoCard {
                    Layout.fillWidth: true
                    title: qsTr("分析依据")
                    lines: root.loading
                           ? [qsTr("读取任务与时间块"), qsTr("检查容量与冲突"), qsTr("生成可确认操作")]
                           : (root.plan.reasons || [])
                }

                Rectangle {
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 112
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text {
                            text: qsTr("状态")
                            color: Theme.secondaryText
                            font.pixelSize: 12
                        }

                        Text {
                            text: root.plan.riskLevel === "high" ? qsTr("需调整") : qsTr("可执行")
                            color: root.plan.riskLevel === "high" ? Theme.error : Theme.success
                            font.pixelSize: 22
                            font.weight: Font.Bold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.loading ? qsTr("分析中") : qsTr("手动确认后执行")
                            color: Theme.tertiaryText
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Text {
                text: qsTr("可确认的调整")
                color: Theme.primaryText
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 10
                model: root.loading ? [] : (root.plan.suggestions || [])

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 86
                    radius: 14
                    color: suggestionMouse.containsMouse || root.selectedSuggestion === modelData ? Theme.surfaceHover : Theme.surfaceMuted
                    border.width: 1
                    border.color: root.selectedSuggestion === modelData ? Theme.accentBright : Theme.borderSoft
                    scale: root.selectedSuggestion === modelData ? 1.008 : 1.0

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 10
                            color: root.selectedSuggestion === modelData ? Theme.surfaceSelected : Theme.surfaceHover
                            border.width: 1
                            border.color: Theme.border

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
                                text: modelData.title || qsTr("调整建议")
                                color: Theme.primaryText
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.description || ""
                                color: Theme.secondaryText
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.impact || ""
                                color: Theme.success
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    MouseArea {
                        id: suggestionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectedSuggestion = modelData
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.executionMessage.length > 0
                text: root.executionMessage
                color: root.executionOk ? Theme.success : Theme.error
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: qsTr("未经确认，Chrona AI 不会创建、移动或修改任何数据。")
                    color: Theme.tertiaryText
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                PlanButton {
                    text: qsTr("确认执行")
                    enabled: !root.loading && Boolean(root.selectedSuggestion && root.selectedSuggestion.actionType)
                    opacity: enabled ? 1 : 0.45
                    onClicked: {
                        root.executionMessage = ""
                        root.executionOk = false
                        var result = ScheduleService.applyScheduleSuggestion(root.selectedSuggestion)
                        var ok = Boolean(result && result.ok)
                        var message = result && result.message
                                      ? result.message
                                      : (ok ? qsTr("调整已执行") : qsTr("执行失败，请重新生成建议"))
                        root.executionMessage = message
                        root.executionOk = ok
                        root.executionFinished(ok, message)
                        if (ok) {
                            root.visible = false
                        }
                    }
                }
            }
        }
    }

    component InfoCard: Rectangle {
        id: infoCard
        property string title: ""
        property var lines: []

        Layout.preferredHeight: 112
        radius: 14
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Text {
                text: infoCard.title
                color: Theme.primaryText
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Repeater {
                model: infoCard.lines

                Text {
                    Layout.fillWidth: true
                    text: "· " + modelData
                    color: Theme.secondaryText
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }
    }

    component PlanButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        property bool enabled: true
        signal clicked()

        Layout.preferredWidth: 104
        Layout.preferredHeight: 38
        radius: 10
        color: muted ? (mouse.containsMouse ? Theme.surfaceHover : Theme.surfaceElevated) : (mouse.containsMouse ? "#8795FF" : Theme.accentBright)
        border.width: muted ? 1 : 0
        border.color: Theme.border

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on opacity { NumberAnimation { duration: 140 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: muted ? Theme.primaryText : Theme.accentText
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            enabled: button.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }
}
