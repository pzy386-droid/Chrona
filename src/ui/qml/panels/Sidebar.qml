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
    signal focusRequested()
    signal coursesRequested()

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
                iconText: root.collapsed ? "›" : "‹"
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
                    {label: qsTr("时间轴"), mark: Theme.accent, action: "timeline"},
                    {label: qsTr("月历总览"), mark: "#A78BFA", action: "month"},
                    {label: qsTr("当前专注"), mark: "#00D68F", action: "focus"},
                    {label: qsTr("课程"), mark: "#FFB547", action: "courses"}
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
                            if (modelData.action === "focus") {
                                root.focusRequested()
                            } else if (modelData.action === "courses") {
                                root.coursesRequested()
                            } else if (modelData.action === "dailyPlan") {
                                root.dailyPlanRequested()
                            } else if (modelData.action === "timeline" || modelData.action === "month") {
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
                            text: modelData.label
                            color: selected ? Theme.strongText : Theme.secondaryText
                            font.pixelSize: 13
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
            Layout.preferredHeight: root.collapsed ? 56 : 60
            Layout.minimumHeight: root.collapsed ? 56 : 60
            Layout.bottomMargin: 8
            radius: root.collapsed ? 16 : 18
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: Theme.dark
                        ? (finishDayMouse.pressed ? "#2B3442" : (finishDayMouse.containsMouse ? "#3A4555" : "#303A48"))
                        : (finishDayMouse.pressed ? "#D5DEE9" : (finishDayMouse.containsMouse ? "#F8FBFF" : "#EDF3F9"))
                }
                GradientStop {
                    position: 0.5
                    color: Theme.dark
                        ? (finishDayMouse.pressed ? "#202833" : (finishDayMouse.containsMouse ? "#303A48" : "#28323F"))
                        : (finishDayMouse.pressed ? "#CAD5E2" : (finishDayMouse.containsMouse ? "#EEF4FA" : "#E5ECF4"))
                }
                GradientStop {
                    position: 1
                    color: Theme.dark
                        ? (finishDayMouse.pressed ? "#161D27" : (finishDayMouse.containsMouse ? "#222B37" : "#1D2632"))
                        : (finishDayMouse.pressed ? "#BECAD8" : (finishDayMouse.containsMouse ? "#E3EAF2" : "#D8E2EC"))
                }
            }
            border.width: 1
            border.color: Theme.dark
                ? (finishDayMouse.pressed ? "#657080" : (finishDayMouse.containsMouse ? "#A6B2C3" : "#788596"))
                : (finishDayMouse.pressed ? "#91A0B2" : (finishDayMouse.containsMouse ? "#8FA3BA" : "#B5C2D1"))
            scale: finishDayMouse.pressed ? 0.975 : (finishDayMouse.containsMouse ? 1.012 : 1)
            transform: Translate { y: finishDayMouse.pressed ? 2 : 0 }
            clip: true

            Behavior on scale { NumberAnimation { duration: finishDayMouse.pressed ? 70 : 180; easing.type: Easing.OutCubic } }
            Behavior on border.color { ColorAnimation { duration: 130 } }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Math.max(1, parent.radius - 2)
                color: "transparent"
                border.width: 1
                border.color: Theme.glassHighlight
                opacity: finishDayMouse.pressed ? 0.12 : 0.3

                Behavior on opacity { NumberAnimation { duration: 100 } }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.topMargin: 4
                height: parent.height * 0.42
                radius: parent.radius - 5
                gradient: Gradient {
                    GradientStop { position: 0; color: "#F8FBFF" }
                    GradientStop { position: 1; color: "#00FFFFFF" }
                }
                opacity: finishDayMouse.pressed ? 0.05 : (finishDayMouse.containsMouse ? 0.2 : 0.14)

                Behavior on opacity { NumberAnimation { duration: 110 } }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.bottomMargin: 4
                height: parent.height * 0.28
                radius: parent.radius - 7
                color: Theme.dark ? "#08101B" : "#64748B"
                opacity: finishDayMouse.pressed ? (Theme.dark ? 0.3 : 0.12) : (Theme.dark ? 0.14 : 0.07)

                Behavior on opacity { NumberAnimation { duration: 110 } }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.collapsed ? 0 : 16
                anchors.rightMargin: root.collapsed ? 0 : 16
                spacing: root.collapsed ? 0 : 10

                Rectangle {
                    Layout.preferredWidth: root.collapsed ? parent.width : 30
                    Layout.preferredHeight: 30
                    Layout.alignment: Qt.AlignVCenter
                    radius: 15
                    color: Theme.dark
                        ? (finishDayMouse.pressed ? "#4A5565" : "#EAF1FC")
                        : (finishDayMouse.pressed ? "#AEBAC8" : "#FFFFFF")
                    opacity: finishDayMouse.pressed ? 0.3 : (Theme.dark ? 0.18 : 0.72)
                    border.width: 1
                    border.color: Theme.glassBorder

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        color: Theme.primaryText
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                }

                Text {
                    visible: !root.collapsed
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("结束今日")
                    color: finishDayMouse.pressed ? Theme.secondaryText : Theme.strongText
                    font.pixelSize: 15
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
