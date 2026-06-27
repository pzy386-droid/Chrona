import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    color: Theme.appBackground

    signal loginCompleted()

    property string statusText: ""

    function submit() {
        var result = ScheduleService.completeFirstLogin(nameField.text)
        if (result.ok) {
            statusText = ""
            loginCompleted()
        } else {
            statusText = result.message || qsTr("登录失败")
        }
    }

    Component.onCompleted: nameField.forceActiveFocus()

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.dark ? "#11151D" : "#F7F9FC" }
            GradientStop { position: 1.0; color: Theme.dark ? "#171C27" : "#E8EEF8" }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 64
        spacing: 48

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 620
            spacing: 20

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                text: "Chrona"
                color: Theme.strongText
                font.pixelSize: 58
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("把任务、课程、DDL 和今天的节奏放在同一个时间轴里。")
                color: Theme.secondaryText
                font.pixelSize: 20
                lineHeight: 1.3
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.topMargin: 22
                spacing: 12

                Repeater {
                    model: [
                        {title: qsTr("计划"), color: Theme.accentBright},
                        {title: qsTr("专注"), color: Theme.success},
                        {title: "DDL", color: Theme.warning}
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        Layout.preferredWidth: 86
                        Layout.preferredHeight: 34
                        radius: 8
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.borderSoft

                        Row {
                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.color
                            }

                            Text {
                                text: modelData.title
                                color: Theme.primaryText
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }

        Rectangle {
            Layout.preferredWidth: 430
            Layout.maximumWidth: 430
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: loginColumn.implicitHeight + 56
            radius: 8
            color: Theme.glassSurface
            border.width: 1
            border.color: Theme.glassBorder

            ColumnLayout {
                id: loginColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 28
                spacing: 18

                Text {
                    Layout.fillWidth: true
                    text: qsTr("欢迎登录")
                    color: Theme.strongText
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("输入一个昵称，Chrona 会记住你的首次登录状态。")
                    color: Theme.secondaryText
                    font.pixelSize: 14
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                }

                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    placeholderText: qsTr("昵称")
                    text: ScheduleService.userName
                    color: Theme.primaryText
                    placeholderTextColor: Theme.mutedText
                    selectedTextColor: Theme.white
                    selectionColor: Theme.accentBright
                    font.pixelSize: 15
                    maximumLength: 32
                    selectByMouse: true
                    onAccepted: root.submit()
                    background: Rectangle {
                        radius: 8
                        color: Theme.inputBackground
                        border.width: 1
                        border.color: nameField.activeFocus ? Theme.accentBright : Theme.border
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.statusText.length > 0
                    text: root.statusText
                    color: Theme.error
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: 8
                    color: loginMouse.pressed ? Theme.accent : loginMouse.containsMouse ? Theme.accentBright : Theme.accent

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("进入 Chrona")
                        color: Theme.accentText
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: loginMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.submit()
                    }
                }
            }
        }
    }
}
