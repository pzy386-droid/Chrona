import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    property bool collapsed: false
    color: "#10131A"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 34
                height: 34
                radius: 10
                color: "#7C8CFF"

                Text {
                    anchors.centerIn: parent
                    text: "C"
                    color: "white"
                    font.pixelSize: 17
                    font.weight: Font.Bold
                }
            }

            Text {
                Layout.fillWidth: true
                visible: !root.collapsed
                text: "Chrona"
                color: "#E6EAF2"
                font.pixelSize: 18
                font.weight: Font.DemiBold
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

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    {label: qsTr("时间轴"), mark: "#7C8CFF"},
                    {label: qsTr("当前专注"), mark: "#6FD6A7"},
                    {label: qsTr("课程"), mark: "#FF8A65"}
                ]

                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    radius: 8
                    color: index === 0 ? "#1A1F2B" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 11
                        anchors.rightMargin: 11
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
                            color: index === 0 ? "#E6EAF2" : "#9AA4B2"
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
            font.weight: Font.DemiBold
        }

        ListView {
            visible: !root.collapsed
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: ScheduleService.taskModel

            delegate: Rectangle {
                width: ListView.view.width
                height: 68
                radius: 8
                color: selected ? "#202638" : "#151923"
                border.width: selected ? 1 : 0
                border.color: "#7C8CFF"

                Behavior on color { ColorAnimation { duration: 160 } }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: ScheduleService.selectTask(id)
                    onEntered: parent.color = selected ? "#202638" : "#1A1F2B"
                    onExited: parent.color = selected ? "#202638" : "#151923"
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: title
                        color: "#E6EAF2"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        PriorityPill { priority: model.priority }

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
        // 1. 占位弹簧：因为展开时 ListView 会占满空间，这个弹簧保证在侧边栏“折叠”时，按钮依然被压在最底下
                Item {
                    Layout.fillHeight: true
                    visible: root.collapsed
                }

                // 2. 晚间复盘按钮
                Button {
                    id: finishDayBtn
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 8
                    Layout.fillWidth: true
                    height: 40

                    // 自定义按钮背景
                    background: Rectangle {
                        color: finishDayBtn.hovered ? "#2D1F24" : "transparent"
                        border.color: finishDayBtn.hovered ? "#4ADE80" : "#30384C"
                        border.width: 1
                        radius: 8
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    // 自定义按钮内容（适配侧边栏折叠状态）
                    contentItem: Item {
                        anchors.fill: parent
                        Row {
                            anchors.centerIn: parent
                            spacing: 8
                            Text {
                                text: "🌙"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                visible: !root.collapsed // 侧边栏收起时隐藏文字
                                text: "结束今日"
                                color: "#E6EAF2"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    onClicked: {
                        eveningReviewOverlay.visible = true
                    }
                }
    }
}
