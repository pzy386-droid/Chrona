import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    visible: false
    color: "#B00A0E17"
    z: 200

    MouseArea { anchors.fill: parent }

    Rectangle {
        width: 620
        height: 430
        radius: 22
        color: "#11151D"
        border.color: "#30384C"
        border.width: 1
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("晚间复盘")
                        color: "#E6EAF2"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("用于结束当天学习状态，回顾专注时间，并把未完成的非固定任务交回 Scheduler 下次重规划。")
                        color: "#8D98AB"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: 9
                    color: closeMouse.containsMouse ? "#202638" : "#151A23"
                    border.width: 1
                    border.color: "#2C3548"

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: "#AAB4C6"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.visible = false
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                StatCard {
                    Layout.fillWidth: true
                    label: qsTr("今日专注")
                    value: qsTr("120 分钟")
                    accent: "#A855F7"
                }

                StatCard {
                    Layout.fillWidth: true
                    label: qsTr("完成任务")
                    value: qsTr("5 项")
                    accent: "#4ADE80"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 74
                radius: 12
                color: "#151A23"
                border.width: 1
                border.color: "#2C3548"

                Text {
                    anchors.fill: parent
                    anchors.margins: 16
                    text: qsTr("未完成的非固定任务不会被删除；它们会在下一次自动重规划时继续进入时间轴。固定时间和锁定时间块保持不变。")
                    color: "#AAB4C6"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("稍后再说")
                    muted: true
                    onClicked: root.visible = false
                }

                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("确认结束今日")
                    onClicked: root.visible = false
                }
            }
        }
    }

    component StatCard: Rectangle {
        property string label: ""
        property string value: ""
        property color accent: "#7C8CFF"

        Layout.preferredHeight: 104
        radius: 14
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: label
                color: "#8D98AB"
                font.pixelSize: 12
            }

            Text {
                Layout.fillWidth: true
                text: value
                color: accent
                font.pixelSize: 30
                font.weight: Font.Bold
            }
        }
    }

    component ActionButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        signal clicked()

        Layout.preferredHeight: 46
        radius: 12
        color: muted
            ? (mouse.containsMouse ? "#202638" : "#151A23")
            : (mouse.containsMouse ? "#8B99FF" : "#7C8CFF")
        border.width: muted ? 1 : 0
        border.color: "#30384C"

        Behavior on color { ColorAnimation { duration: 140 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: muted ? "#AAB4C6" : "white"
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }
}
