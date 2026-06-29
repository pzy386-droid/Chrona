import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root

    property bool collapsed: false
    property string currentPage: "timeline"
    signal navigateRequested(string page)
    signal dailyPlanRequested()

    color: Theme.sidebarBackground

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 40
                height: 40
                radius: 14
                gradient: Gradient {
                    GradientStop { position: 0; color: "#8B5CF6" }
                    GradientStop { position: 1; color: "#6366F1" }
                }

                Text {
                    anchors.centerIn: parent
                    text: "C"
                    color: "white"
                    font.pixelSize: 18
                    font.bold: true
                }
            }

            Text {
                Layout.fillWidth: true
                visible: !root.collapsed
                text: "Chrona"
                color: Theme.strongText
                font.pixelSize: 19
                font.bold: true
            }

            IconButton {
                iconText: root.collapsed ? ">" : "<"
                tooltip: qsTr("收起侧栏")
                onClicked: root.collapsed = !root.collapsed
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    {label: qsTr("AI 今日计划"), mark: "#8B99FF", action: "dailyPlan"},
                    {label: qsTr("时间线"), mark: Theme.accent, action: "timeline"},
                    {label: qsTr("DDL 提醒"), mark: "#F59E0B", action: "deadlines"},
                    {label: qsTr("月历总览"), mark: "#A78BFA", action: "month"},
                    {label: qsTr("数据洞察"), mark: "#22C55E", action: "insights"},
                    {label: "\u0041\u0049 \u8bb0\u5fc6", mark: "#A78BFA", action: "memory"},
                ]

                delegate: Rectangle {
                    id: navItem
                    Layout.fillWidth: true
                    height: 44
                    radius: 12
                    property bool selected: modelData.action === root.currentPage
                    color: hover.containsMouse ? Theme.surfaceHover : selected ? Theme.surfaceElevated : "transparent"
                    scale: hover.containsMouse ? 1.03 : 1
                    border.width: selected ? 1 : 0
                    border.color: Theme.accent

                    Behavior on scale { NumberAnimation { duration: 120 } }
                    Behavior on color { ColorAnimation { duration: 150 } }

                    MouseArea {
                        id: hover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modelData.action === "dailyPlan") {
                                root.dailyPlanRequested()
                            } else if (modelData.action === "timeline" || modelData.action === "month" || modelData.action === "deadlines" || modelData.action === "insights" || modelData.action === "memory") {
                                root.navigateRequested(modelData.action)
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: modelData.mark
                        }

                        Text {
                            visible: !root.collapsed
                            Layout.fillWidth: true
                            text: modelData.label
                            color: selected ? Theme.strongText : Theme.secondaryText
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            visible: !root.collapsed && modelData.action === "deadlines" && ScheduleService.urgentDeadlineCount > 0
                            Layout.preferredWidth: Math.max(22, countText.implicitWidth + 12)
                            Layout.preferredHeight: 20
                            radius: 8
                            color: "#3A2612"
                            border.width: 1
                            border.color: "#F59E0B"

                            Text {
                                id: countText
                                anchors.centerIn: parent
                                text: ScheduleService.urgentDeadlineCount
                                color: "#FFD58A"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }
        }

        Text {
            visible: !root.collapsed
            Layout.fillWidth: true
            text: qsTr("当日日程")
            color: Theme.mutedText
            font.pixelSize: 12
            font.bold: true
        }

        ListView {
            id: taskList
            visible: !root.collapsed
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: ScheduleService.dailyItems

            delegate: Rectangle {
                width: ListView.view.width
                height: 70
                radius: 12
                color: modelData.selected ? Theme.surfaceHover : Theme.surfaceElevated
                border.width: modelData.selected ? 1 : 0
                border.color: Theme.accent
                scale: mouse.containsMouse ? 1.02 : 1

                Behavior on scale { NumberAnimation { duration: 120 } }
                Behavior on color { ColorAnimation { duration: 150 } }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: ScheduleService.selectTimelineItem(modelData.taskId, modelData.id)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: modelData.title
                        color: Theme.strongText
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            width: 34
                            height: 20
                            radius: 7
                            color: modelData.isEvent
                                ? (Theme.dark ? "#25233A" : "#F0EAFE")
                                : (Theme.dark ? "#1D2C35" : "#E8F5EF")
                            border.width: 1
                            border.color: modelData.color || Theme.accentBright

                            Text {
                                anchors.centerIn: parent
                                text: modelData.isEvent ? qsTr("日程") : qsTr("任务")
                                color: modelData.color || Theme.secondaryText
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.scheduleText
                            color: Theme.tertiaryText
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: taskList.count === 0
                text: qsTr("当天暂无日程")
                color: Theme.mutedText
                font.pixelSize: 12
            }
        }

        Item {
            Layout.fillHeight: true
            visible: root.collapsed
        }

        Rectangle {
            id: finishDayBtn
            Layout.fillWidth: true
            Layout.preferredHeight: root.collapsed ? 50 : 52
            Layout.minimumHeight: root.collapsed ? 50 : 52
            Layout.bottomMargin: 8
            radius: 13
            color: finishDayMouse.pressed
                ? Theme.surfaceSelected
                : (finishDayMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceElevated)
            border.width: 1
            border.color: finishDayMouse.containsMouse ? Theme.accentBright : Theme.border
            scale: finishDayMouse.pressed ? 0.99 : 1

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }
            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.collapsed ? 0 : 15
                anchors.rightMargin: root.collapsed ? 0 : 15
                spacing: root.collapsed ? 0 : 10

                Rectangle {
                    Layout.preferredWidth: root.collapsed ? parent.width : 28
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    radius: 9
                    color: Theme.surfaceSelected

                    Text {
                        anchors.centerIn: parent
                        text: "\u2713"
                        color: Theme.accentBright
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }

                Text {
                    visible: !root.collapsed
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: "\u7ed3\u675f\u4eca\u65e5"
                    color: Theme.strongText
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                id: finishDayMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: eveningReviewOverlay.visible = true
            }
        }
    }
}
