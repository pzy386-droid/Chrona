import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    visible: false
    color: "#B00A0E17"
    z: 200

    property string mode: "daily"
    property var plan: ({})
    property var selectedSuggestion: ({})

    function reload() {
        plan = mode === "suggestions"
            ? ScheduleService.previewScheduleSuggestions()
            : ScheduleService.aiTodayPlan()
        selectedSuggestion = plan.suggestions && plan.suggestions.length > 0 ? plan.suggestions[0] : ({})
    }

    onVisibleChanged: if (visible) reload()
    MouseArea { anchors.fill: parent }

    Rectangle {
        width: Math.min(parent.width - 48, 760)
        height: Math.min(parent.height - 48, 680)
        radius: 16
        color: "#11151D"
        border.color: "#30384C"
        border.width: 1
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: root.plan.title || (root.mode === "suggestions" ? qsTr("智能排程建议") : qsTr("AI 今日计划"))
                        color: "#E6EAF2"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("来源：%1 · %2")
                            .arg(root.plan.provider === "deepseek" ? "DeepSeek" : qsTr("本地规则"))
                            .arg(root.plan.model || "")
                        color: "#7C8CFF"
                        font.pixelSize: 11
                    }
                }
                Button { text: qsTr("关闭"); onClicked: root.visible = false }
            }

            Text {
                Layout.fillWidth: true
                text: root.plan.summary || qsTr("正在分析当前任务与日程")
                color: "#C8D0DE"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                radius: 10
                color: root.plan.riskLevel === "high" ? "#2A1D1B" : "#151A23"
                border.width: 1
                border.color: root.plan.riskLevel === "high" ? "#7A4334" : "#2C3548"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    Text { text: qsTr("现在建议"); color: "#8D98AB"; font.pixelSize: 12 }
                    Text {
                        Layout.fillWidth: true
                        text: root.plan.currentFocus || qsTr("检查今日计划并按优先级执行")
                        color: "#E6EAF2"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                }
            }

            Text { text: qsTr("分析依据"); color: "#E6EAF2"; font.pixelSize: 14; font.weight: Font.DemiBold }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(130, contentHeight)
                clip: true
                spacing: 6
                model: root.plan.reasons || []
                delegate: Text {
                    width: ListView.view.width
                    text: "• " + modelData
                    color: "#9AA4B2"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            Text { text: qsTr("可确认的调整"); color: "#E6EAF2"; font.pixelSize: 14; font.weight: Font.DemiBold }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: root.plan.suggestions || []
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 86
                    radius: 10
                    color: suggestionMouse.containsMouse || root.selectedSuggestion === modelData ? "#202638" : "#151A23"
                    border.width: 1
                    border.color: root.selectedSuggestion === modelData ? "#7C8CFF" : "#2C3548"
                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 5
                        Text { width: parent.width; text: modelData.title || qsTr("调整建议"); color: "#E6EAF2"; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        Text { width: parent.width; text: modelData.description || ""; color: "#9AA4B2"; font.pixelSize: 11; elide: Text.ElideRight }
                        Text { width: parent.width; text: modelData.impact || ""; color: "#A9F0C9"; font.pixelSize: 11; elide: Text.ElideRight }
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

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("未经确认，AI 不会创建、移动或修改任何数据。")
                    color: "#7D8798"
                    font.pixelSize: 11
                }
                Button {
                    text: qsTr("确认执行")
                    highlighted: true
                    enabled: root.selectedSuggestion && root.selectedSuggestion.actionType
                    onClicked: {
                        var result = ScheduleService.applyScheduleSuggestion(root.selectedSuggestion)
                        if (result && result.ok) root.reload()
                    }
                }
            }
        }
    }
}
