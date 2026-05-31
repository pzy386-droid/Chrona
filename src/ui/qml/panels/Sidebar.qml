import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root

    property bool collapsed: false

    color: "#0B0E14"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 18

        // ===== Logo =====

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 40
                height: 40
                radius: 14

                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: "#8B5CF6"
                    }

                    GradientStop {
                        position: 1
                        color: "#6366F1"
                    }
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

                color: "#FFFFFF"

                font.pixelSize: 19
                font.bold: true
            }

            IconButton {
                iconText: root.collapsed ? ">" : "<"
                tooltip: qsTr("折叠侧栏")
                onClicked: root.collapsed = !root.collapsed
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#232836"
        }

        // ===== 导航 =====

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    {label: qsTr("时间轴"), mark: "#6C63FF"},
                    {label: qsTr("当前专注"), mark: "#00D68F"},
                    {label: qsTr("课程"), mark: "#FFB547"}
                ]

                delegate: Rectangle {
                    id: navItem

                    Layout.fillWidth: true
                    height: 44

                    radius: 12

                    property bool selected: index === 0

                    color: hover.containsMouse
                           ? "#202638"
                           : selected
                             ? "#161B22"
                             : "transparent"

                    scale: hover.containsMouse ? 1.03 : 1

                    border.width: selected ? 1 : 0
                    border.color: "#6C63FF"

                    Behavior on scale {
                        NumberAnimation {
                            duration: 120
                        }
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }

                    MouseArea {
                        id: hover

                        anchors.fill: parent
                        hoverEnabled: true
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

                            color: selected
                                   ? "#FFFFFF"
                                   : "#A6B0C3"

                            font.pixelSize: 13
                        }
                    }
                }
            }
        }

        Text {
            visible: !root.collapsed

            Layout.fillWidth: true

            text: qsTr("任务")

            color: "#677184"

            font.pixelSize: 12
            font.bold: true
        }

        // ===== 任务列表 =====

        ListView {
            visible: !root.collapsed

            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            spacing: 8

            model: ScheduleService.taskModel

            delegate: Rectangle {
                width: ListView.view.width
                height: 70

                radius: 12

                color: selected
                       ? "#202638"
                       : "#161B22"

                border.width: selected ? 1 : 0
                border.color: "#6C63FF"

                scale: mouse.containsMouse ? 1.02 : 1

                Behavior on scale {
                    NumberAnimation {
                        duration: 120
                    }
                }

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }

                MouseArea {
                    id: mouse

                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: ScheduleService.selectTask(id)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12

                    spacing: 6

                    Text {
                        Layout.fillWidth: true

                        text: title

                        color: "#FFFFFF"

                        font.pixelSize: 13

                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        PriorityPill {
                            priority: model.priority
                        }

                        Text {
                            Layout.fillWidth: true

                            text: deadlineText

                            color: "#8E98AA"

                            font.pixelSize: 11

                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
            visible: root.collapsed
        }

        // ===== 结束今日 =====

        Button {
            id: finishDayBtn

            Layout.fillWidth: true
            Layout.bottomMargin: 8

            height: 60

            background: Rectangle {
                radius: 18

                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: "#312E81"
                    }

                    GradientStop {
                        position: 1
                        color: "#4F46E5"
                    }
                }

                border.width: finishDayBtn.hovered ? 1 : 0

                border.color: "#818CF8"

                scale: finishDayBtn.hovered ? 1.03 : 1

                Behavior on scale {
                    NumberAnimation {
                        duration: 120
                    }
                }
            }

            contentItem: Item {
                anchors.fill: parent

                Row {
                    anchors.centerIn: parent

                    spacing: 8

                    Text {
                        text: "🌙"
                        font.pixelSize: 20
                    }

                    Text {
                        visible: !root.collapsed

                        text: "结束今日"

                        color: "#FFFFFF"

                        font.pixelSize: 16
                        font.bold: true
                    }
                }
            }

            onClicked: {
                eveningReviewOverlay.visible = true
            }
        }
    }
}