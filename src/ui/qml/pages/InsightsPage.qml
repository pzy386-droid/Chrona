import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    color: Theme.appBackground

    property string period: "week"
    property var dashboard: ({})

    function reload() {
        dashboard = ScheduleService.insightsDashboard(period)
        distributionCanvas.requestPaint()
        barsCanvas.requestPaint()
    }

    Component.onCompleted: reload()

    Connections {
        target: ScheduleService
        function onDataChanged() { root.reload() }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.max(920, parent.width)
            spacing: 16
            anchors.margins: 22

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 92
                radius: 20
                color: Theme.surfaceElevated
                border.width: 1
                border.color: Theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 16

                    Rectangle {
                        Layout.preferredWidth: 52
                        Layout.preferredHeight: 52
                        radius: 18
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#5271FF" }
                            GradientStop { position: 1; color: "#22C55E" }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "◒"
                            color: "white"
                            font.pixelSize: 25
                            font.weight: Font.Bold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Text {
                            text: qsTr("学习数据洞察")
                            color: Theme.strongText
                            font.pixelSize: 24
                            font.weight: Font.Bold
                        }

                        Text {
                            text: qsTr("%1 · 时间分布、执行质量与 Chrona AI 复盘").arg(root.dashboard.rangeLabel || "")
                            color: Theme.secondaryText
                            font.pixelSize: 12
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 44
                        radius: 14
                        color: Theme.surfaceMuted
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 4

                            Repeater {
                                model: [
                                    { label: qsTr("今日"), value: "day" },
                                    { label: qsTr("本周"), value: "week" }
                                ]

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 11
                                    color: root.period === modelData.value ? Theme.surfaceSelected : "transparent"
                                    border.width: root.period === modelData.value ? 1 : 0
                                    border.color: Theme.accentBright

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: root.period === modelData.value ? Theme.primaryText : Theme.tertiaryText
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.period = modelData.value
                                            root.reload()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 12
                rowSpacing: 12

                MetricCard {
                    label: qsTr("学习时间")
                    value: formatMinutes(root.dashboard.learningMinutes || 0)
                    detail: qsTr("真实时间块合计")
                    accent: "#5271FF"
                }
                MetricCard {
                    label: qsTr("完成率")
                    value: (root.dashboard.completionRate || 0) + "%"
                    detail: qsTr("已完成 %1").arg(formatMinutes(root.dashboard.completedMinutes || 0))
                    accent: "#22C55E"
                }
                MetricCard {
                    label: qsTr("恢复间隔")
                    value: formatMinutes(root.dashboard.restMinutes || 0)
                    detail: qsTr("按时间块间隔估算")
                    accent: "#F59E0B"
                }
                MetricCard {
                    label: qsTr("AI 记忆")
                    value: String(root.dashboard.memoryCount || 0)
                    detail: qsTr("正在参与后续规划")
                    accent: "#A78BFA"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 330
                spacing: 14

                InsightCard {
                    Layout.preferredWidth: parent.width * 0.42
                    Layout.fillHeight: true
                    title: qsTr("时间分布")
                    subtitle: qsTr("工作时段内的学习、恢复和空闲比例")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        anchors.topMargin: 70
                        anchors.bottomMargin: 18
                        spacing: 20

                        Canvas {
                            id: distributionCanvas
                            Layout.preferredWidth: 210
                            Layout.preferredHeight: 210

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                var values = [
                                    Number(root.dashboard.learningMinutes || 0),
                                    Number(root.dashboard.restMinutes || 0),
                                    Number(root.dashboard.freeMinutes || 0)
                                ]
                                var colors = ["#5271FF", "#F59E0B", Theme.dark ? "#334155" : "#CBD5E1"]
                                var total = Math.max(1, values[0] + values[1] + values[2])
                                var start = -Math.PI / 2
                                for (var i = 0; i < values.length; ++i) {
                                    var angle = Math.PI * 2 * values[i] / total
                                    ctx.beginPath()
                                    ctx.strokeStyle = colors[i]
                                    ctx.lineWidth = 25
                                    ctx.lineCap = "round"
                                    ctx.arc(width / 2, height / 2, 73, start + 0.025, start + angle - 0.025)
                                    ctx.stroke()
                                    start += angle
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 2

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: root.formatMinutes(root.dashboard.learningMinutes || 0)
                                    color: Theme.strongText
                                    font.pixelSize: 24
                                    font.weight: Font.Bold
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("专注学习")
                                    color: Theme.secondaryText
                                    font.pixelSize: 11
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 14

                            DistributionLegend { colorValue: "#5271FF"; label: qsTr("学习"); minutes: root.dashboard.learningMinutes || 0 }
                            DistributionLegend { colorValue: "#F59E0B"; label: qsTr("恢复"); minutes: root.dashboard.restMinutes || 0 }
                            DistributionLegend { colorValue: Theme.dark ? "#475569" : "#CBD5E1"; label: qsTr("空闲"); minutes: root.dashboard.freeMinutes || 0 }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("高优先级任务占用 %1").arg(root.formatMinutes(root.dashboard.highPriorityMinutes || 0))
                                color: Theme.secondaryText
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                InsightCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: root.period === "day" ? qsTr("今日执行") : qsTr("每日学习趋势")
                    subtitle: qsTr("浅色为计划，绿色为已完成")

                    Canvas {
                        id: barsCanvas
                        anchors.fill: parent
                        anchors.leftMargin: 24
                        anchors.rightMargin: 20
                        anchors.topMargin: 78
                        anchors.bottomMargin: 35

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            var bars = root.dashboard.dailyBars || []
                            if (!bars.length) return
                            var maxValue = 60
                            for (var i = 0; i < bars.length; ++i)
                                maxValue = Math.max(maxValue, Number(bars[i].plannedMinutes || 0))
                            var slot = width / bars.length
                            var barWidth = Math.min(46, slot * 0.48)
                            ctx.font = "11px sans-serif"
                            ctx.textAlign = "center"
                            for (var j = 0; j < bars.length; ++j) {
                                var x = slot * j + slot / 2
                                var plannedHeight = (height - 28) * Number(bars[j].plannedMinutes || 0) / maxValue
                                var doneHeight = (height - 28) * Number(bars[j].completedMinutes || 0) / maxValue
                                ctx.fillStyle = Theme.dark ? "#334155" : "#DDE5F3"
                                ctx.fillRect(x - barWidth / 2, height - 24 - plannedHeight, barWidth, plannedHeight)
                                ctx.fillStyle = "#22C55E"
                                ctx.fillRect(x - barWidth / 2, height - 24 - doneHeight, barWidth, doneHeight)
                                ctx.fillStyle = Theme.secondaryText
                                ctx.fillText(bars[j].dayLabel || bars[j].dateText, x, height - 6)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 20
                    color: Theme.dark ? "#171B2A" : "#F0F4FF"
                    border.width: 1
                    border.color: Theme.dark ? "#394269" : "#C9D4FF"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                width: 42
                                height: 42
                                radius: 14
                                color: Theme.surfaceSelected
                                Text { anchors.centerIn: parent; text: "AI"; color: Theme.accentBright; font.weight: Font.Black }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: qsTr("今日 AI 总结"); color: Theme.strongText; font.pixelSize: 18; font.weight: Font.Bold }
                                Text { text: qsTr("结合执行数据与记忆生成"); color: Theme.secondaryText; font.pixelSize: 11 }
                            }

                            Text {
                                text: qsTr("完成 %1 · 未完成 %2")
                                    .arg(root.dashboard.completedCount || 0)
                                    .arg(root.dashboard.unfinishedCount || 0)
                                color: Theme.success
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.dashboard.aiSummary || ""
                            color: Theme.primaryText
                            font.pixelSize: 14
                            lineHeight: 1.35
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: qsTr("明日优先建议")
                            color: Theme.secondaryText
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Repeater {
                            model: root.dashboard.tomorrowSuggestions || []
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                radius: 11
                                color: Theme.surface
                                border.width: 1
                                border.color: Theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12

                                    Rectangle {
                                        width: 7
                                        height: 7
                                        radius: 4
                                        color: modelData.level === "critical" ? Theme.error
                                            : modelData.level === "high" ? Theme.warning : Theme.accentBright
                                    }
                                    Text { Layout.fillWidth: true; text: modelData.title; color: Theme.primaryText; font.pixelSize: 12; elide: Text.ElideRight }
                                    Text { text: qsTr("动态分 %1").arg(modelData.score); color: Theme.tertiaryText; font.pixelSize: 10 }
                                }
                            }
                        }
                    }
                }

                InsightCard {
                    Layout.preferredWidth: 360
                    Layout.fillHeight: true
                    title: qsTr("Chrona 记忆库")
                    subtitle: qsTr("只保存结构化偏好与学习模式")

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        anchors.topMargin: 72
                        anchors.bottomMargin: 16
                        spacing: 9

                        Repeater {
                            model: root.dashboard.recentMemories || []
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: 10
                                color: Theme.surfaceMuted

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 2
                                    Text { width: parent.width; text: modelData.content; color: Theme.primaryText; font.pixelSize: 11; elide: Text.ElideRight }
                                    Text { text: modelData.kind + " · " + Number(modelData.weight || 0).toFixed(1); color: Theme.tertiaryText; font.pixelSize: 9 }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        Text {
                            visible: (root.dashboard.memoryCount || 0) === 0
                            Layout.fillWidth: true
                            text: qsTr("完成任务或调整偏好后，记忆会自动形成。")
                            color: Theme.tertiaryText
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    function formatMinutes(minutes) {
        var value = Math.max(0, Number(minutes || 0))
        if (value < 60) return Math.round(value) + qsTr(" 分钟")
        var hours = Math.floor(value / 60)
        var remain = Math.round(value % 60)
        return remain > 0 ? hours + "h " + remain + "m" : hours + "h"
    }

    component MetricCard: Rectangle {
        property string label: ""
        property string value: ""
        property string detail: ""
        property color accent: Theme.accentBright

        Layout.fillWidth: true
        Layout.preferredHeight: 116
        radius: 18
        color: Theme.surfaceElevated
        border.width: 1
        border.color: Theme.border

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 5
            Text { text: label; color: Theme.secondaryText; font.pixelSize: 12 }
            Text { text: value; color: accent; font.pixelSize: 27; font.weight: Font.Bold }
            Text { text: detail; color: Theme.tertiaryText; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
        }
    }

    component InsightCard: Rectangle {
        default property alias content: contentItem.data
        property string title: ""
        property string subtitle: ""

        radius: 20
        color: Theme.surfaceElevated
        border.width: 1
        border.color: Theme.border

        Column {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 18
            spacing: 3
            Text { text: title; color: Theme.strongText; font.pixelSize: 17; font.weight: Font.Bold }
            Text { text: subtitle; color: Theme.tertiaryText; font.pixelSize: 11 }
        }

        Item { id: contentItem; anchors.fill: parent }
    }

    component DistributionLegend: RowLayout {
        property color colorValue: Theme.accentBright
        property string label: ""
        property int minutes: 0

        Layout.fillWidth: true
        Rectangle { width: 10; height: 10; radius: 5; color: colorValue }
        Text { Layout.fillWidth: true; text: label; color: Theme.secondaryText; font.pixelSize: 12 }
        Text { text: root.formatMinutes(minutes); color: Theme.primaryText; font.pixelSize: 12; font.weight: Font.DemiBold }
    }
}
