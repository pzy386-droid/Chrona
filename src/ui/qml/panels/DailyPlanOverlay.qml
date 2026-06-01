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
        width: 640
        height: 520
        radius: 16
        color: "#161A23"
        border.color: "#30384C"
        border.width: 1
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 18

            Text {
                Layout.fillWidth: true
                text: qsTr("今日学习计划确认")
                color: "#E6EAF2"
                font.pixelSize: 22
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: 10
                color: "#10141C"
                border.color: "#252B3A"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    Text {
                        text: qsTr("容量状态")
                        color: "#94A3B8"
                        font.pixelSize: 14
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("80% 健康")
                        color: "#4ADE80"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("今日排程概览")
                color: "#E6EAF2"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: 10
                color: "#2D1F24"
                border.color: "#991B1B"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 32
                    text: qsTr("有 2 项低优先级任务可能无法自动排入今日。")
                    color: "#F87171"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                Button { text: qsTr("推迟冲突任务") }
                Button {
                    text: qsTr("调整重点")
                    onClicked: root.visible = false
                }
                Button {
                    text: qsTr("接受并开始今天")
                    highlighted: true
                    onClicked: root.visible = false
                }
            }
        }
    }
}
