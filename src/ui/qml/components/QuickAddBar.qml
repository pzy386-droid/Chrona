import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var capacityStats: ({})
    property var aiStatus: ({})
    property string recommendationTitle: qsTr("AI 推荐")
    property string recommendationBody: qsTr("14:00 - 15:30 适合高认知学习")
    property string recommendationDelta: "+18%"

    signal addRequested(string text)
    signal imagePreviewRequested(string fileUrl)
    signal recommendationRequested()
    signal aiConfigRequested()

    readonly property bool compact: width < 1020
    readonly property bool dense: width < 860

    height: dense ? 96 : 116
    radius: 12
    color: Theme.surface
    clip: true
    border.width: 1
    border.color: dropArea.containsDrag ? Theme.accentBright : Theme.surfaceHover

    Behavior on border.color { ColorAnimation { duration: 160 } }

    DropArea {
        id: dropArea
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.urls.length > 0) {
                root.imagePreviewRequested(String(drop.urls[0]))
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Rectangle {
            visible: !root.dense
            Layout.preferredWidth: root.compact ? 218 : 260
            Layout.fillHeight: true
            radius: 10
            color: recommendationMouse.containsMouse ? Theme.surfaceHover : Theme.surface
            border.width: 1
            border.color: recommendationMouse.containsMouse ? Theme.border : Theme.border
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }

            MouseArea {
                id: recommendationMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.recommendationRequested()
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: 12
                    color: Theme.surfaceHover

                    Text {
                        anchors.centerIn: parent
                        text: "AI"
                        color: "#8B99FF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: root.recommendationTitle
                            color: Theme.primaryText
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            text: root.recommendationDelta
                            color: "#4ADE80"
                            font.pixelSize: 18
                            font.weight: Font.Bold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.recommendationBody
                        color: Theme.secondaryText
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: root.dense ? 320 : 270
            radius: 10
            color: Theme.surface
            border.width: 1
            border.color: Theme.surfaceHover

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 10

                Text {
                    id: aiBadge
                    text: "AI"
                    color: root.aiStatus.configured ? Theme.accentBright : Theme.secondaryText
                    font.pixelSize: 14
                    font.weight: Font.Bold

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.aiConfigRequested()
                    }
                }

                Text {
                    id: aiStatusText
                    visible: !root.compact
                    text: root.aiStatus.label || qsTr("本地规则")
                    color: root.aiStatus.configured ? Theme.success : Theme.secondaryText
                    font.pixelSize: 10
                    Layout.maximumWidth: 92
                    elide: Text.ElideRight

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.aiConfigRequested()
                    }
                }

                TextField {
                    id: input
                    Layout.fillWidth: true
                    placeholderText: qsTr("例如：明天下午两点写高数作业")
                    color: Theme.primaryText
                    placeholderTextColor: Theme.mutedText
                    font.pixelSize: 14
                    background: Item {}
                    onAccepted: {
                        if (text.trim().length > 0) {
                            root.addRequested(text.trim())
                            text = ""
                        }
                    }
                }

                CapsuleButton {
                    Layout.preferredWidth: root.compact ? 64 : 76
                    text: root.compact ? qsTr("规划") : qsTr("AI 规划")
                    onClicked: {
                        if (input.text.trim().length > 0) {
                            root.addRequested(input.text.trim())
                            input.text = ""
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: !root.compact
            Layout.preferredWidth: 188
            Layout.fillHeight: true
            radius: 10
            color: dropArea.containsDrag ? Theme.infoSurface : Theme.surface
            border.width: 1
            border.color: dropArea.containsDrag ? Theme.accentBright : Theme.surfaceHover

            Column {
                anchors.centerIn: parent
                spacing: 5

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("拖拽截图识别")
                    color: Theme.primaryText
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Text {
                    width: parent.parent.width - 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("课程表 / 作业通知 / 考试安排")
                    color: Theme.secondaryText
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            visible: !root.dense
            Layout.preferredWidth: root.compact ? 142 : 164
            Layout.fillHeight: true
            radius: 10
            color: Theme.surface
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: root.capacityStats.message || qsTr("学习容量健康")
                    color: root.capacityStats.overloaded ? Theme.error : Theme.success
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("当日已排 %1 分钟").arg(root.capacityStats.todayScheduledMinutes || 0)
                    color: Theme.secondaryText
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("本周 %1 小时 · %2%")
                        .arg(((root.capacityStats.weekScheduledMinutes || 0) / 60).toFixed(1))
                        .arg(root.capacityStats.weekLoadPercent || 0)
                    color: Theme.secondaryText
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }
    }

    component CapsuleButton: Rectangle {
        id: button
        property alias text: label.text
        signal clicked()

        implicitWidth: 76
        height: 38
        radius: 8
        color: mouse.containsMouse ? "#8B99FF" : Theme.accentBright
        Behavior on color { ColorAnimation { duration: 140 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 13
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
