import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    property var task: ({})
    readonly property bool hasTask: task && task.id
    readonly property var preferredValues: ["morning", "afternoon", "evening"]

    function syncFields() {
        if (!hasTask) {
            return
        }
        titleField.text = task.title || ""
        notesField.text = task.notes || ""
        deadlineField.text = task.deadlineText || ""
        durationInput.value = Math.max(30, task.estimatedMinutes || 60)
        priorityPicker.currentIndex = Math.max(0, Math.min(2, task.priority || 0))
        categoryField.text = task.categoryName || ""
        var preferred = task.preferredStudyTime || "evening"
        preferredPicker.currentIndex = Math.max(0, preferredValues.indexOf(preferred))
        statusText.text = ""
    }

    color: "#11151D"
    border.width: 1
    border.color: "#232836"

    onTaskChanged: syncFields()
    Component.onCompleted: syncFields()

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 22
        anchors.bottomMargin: 18
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: qsTr("任务详情")
                color: "#AAB4C6"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Rectangle {
                visible: root.hasTask
                width: 10
                height: 10
                radius: 5
                color: root.task.categoryColor || "#7C8CFF"
                Behavior on color { ColorAnimation { duration: 180 } }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 5
            radius: 3
            color: root.hasTask ? (root.task.categoryColor || "#7C8CFF") : "#252B3A"
            Behavior on color { ColorAnimation { duration: 200 } }
        }

        ColumnLayout {
            visible: !root.hasTask
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                text: qsTr("选择一个时间块")
                color: "#E6EAF2"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("点击时间轴中的学习块，可以在这里查看、编辑并重新排程。")
                color: "#8C96AA"
                font.pixelSize: 13
                lineHeight: 1.35
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillHeight: true }
        }

        Flickable {
            visible: root.hasTask
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: formColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: formColumn
                width: parent.width
                spacing: 13

                FieldLabel { text: qsTr("任务标题") }
                TextField {
                    id: titleField
                    Layout.fillWidth: true
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: qsTr("例如：高数期中复习")
                    placeholderTextColor: "#667187"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    background: FieldBackground {}
                }

                FieldLabel { text: qsTr("备注") }
                TextArea {
                    id: notesField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: qsTr("补充资料、章节或完成标准")
                    placeholderTextColor: "#667187"
                    font.pixelSize: 13
                    wrapMode: TextEdit.WordWrap
                    background: FieldBackground {}
                }

                FieldLabel { text: qsTr("截止时间") }
                TextField {
                    id: deadlineField
                    Layout.fillWidth: true
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: "2026-05-30 23:00"
                    placeholderTextColor: "#667187"
                    font.pixelSize: 13
                    background: FieldBackground {}
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("预计时长") }
                        MinuteStepper {
                            id: durationInput
                            Layout.fillWidth: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        FieldLabel { text: qsTr("优先级") }
                        OptionPills {
                            id: priorityPicker
                            Layout.fillWidth: true
                            options: [qsTr("低"), qsTr("中"), qsTr("高")]
                        }
                    }
                }

                FieldLabel { text: qsTr("课程分类") }
                TextField {
                    id: categoryField
                    Layout.fillWidth: true
                    color: "#E6EAF2"
                    selectedTextColor: "white"
                    selectionColor: "#5968D8"
                    placeholderText: qsTr("学习 / 高数 / 英语")
                    placeholderTextColor: "#667187"
                    font.pixelSize: 13
                    background: FieldBackground {}
                }

                FieldLabel { text: qsTr("偏好学习时间") }
                OptionPills {
                    id: preferredPicker
                    Layout.fillWidth: true
                    options: [qsTr("上午"), qsTr("下午"), qsTr("晚上")]
                }

                Text {
                    id: statusText
                    Layout.fillWidth: true
                    color: "#A9F0C9"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            visible: root.hasTask
            Layout.fillWidth: true
            spacing: 10

            ActionButton {
                Layout.fillWidth: true
                text: qsTr("删除")
                danger: true
                onClicked: {
                    var result = ScheduleService.deleteTask(root.task.id)
                    statusText.text = result.message || ""
                }
            }

            ActionButton {
                Layout.fillWidth: true
                text: qsTr("完成")
                muted: true
                onClicked: ScheduleService.completeTask(root.task.id)
            }
        }

        ActionButton {
            visible: root.hasTask
            Layout.fillWidth: true
            text: qsTr("保存并重新排程")
            onClicked: {
                var result = ScheduleService.updateTask(
                    root.task.id,
                    titleField.text,
                    notesField.text,
                    deadlineField.text,
                    durationInput.value,
                    priorityPicker.currentIndex,
                    categoryField.text,
                    root.preferredValues[preferredPicker.currentIndex]
                )
                statusText.color = result.ok ? "#A9F0C9" : "#FFB09B"
                statusText.text = result.message || ""
            }
        }
    }

    Behavior on width {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }

    component FieldLabel: Text {
        color: "#8D98AB"
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    component FieldBackground: Rectangle {
        radius: 8
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"
    }

    component MinuteStepper: Rectangle {
        id: stepper
        property int value: 60

        Layout.preferredHeight: 42
        radius: 8
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 6
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("%1 分钟").arg(stepper.value)
                color: "#E6EAF2"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            StepButton {
                text: "-"
                onClicked: stepper.value = Math.max(30, stepper.value - 15)
            }

            StepButton {
                text: "+"
                onClicked: stepper.value = Math.min(720, stepper.value + 15)
            }
        }
    }

    component StepButton: Rectangle {
        id: step
        property alias text: label.text
        signal clicked()

        Layout.preferredWidth: 28
        Layout.preferredHeight: 28
        radius: 7
        color: mouse.containsMouse ? "#283147" : "#1B2130"
        border.width: 1
        border.color: "#343E55"

        Behavior on color { ColorAnimation { duration: 120 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: "#C5CBEA"
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: step.clicked()
        }
    }

    component OptionPills: Rectangle {
        id: pills
        property var options: []
        property int currentIndex: 0

        Layout.preferredHeight: 42
        radius: 8
        color: "#151A23"
        border.width: 1
        border.color: "#2C3548"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4

            Repeater {
                model: pills.options

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 7
                    color: pills.currentIndex === index ? "#252D44" : "transparent"
                    border.width: pills.currentIndex === index ? 1 : 0
                    border.color: "#7C8CFF"

                    Behavior on color { ColorAnimation { duration: 140 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: pills.currentIndex === index ? "#E6EAF2" : "#8D98AB"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: pills.currentIndex = index
                    }
                }
            }
        }
    }

    component ActionButton: Rectangle {
        id: button
        property alias text: label.text
        property bool muted: false
        property bool danger: false
        signal clicked()

        Layout.preferredHeight: 42
        radius: 8
        color: danger
            ? (mouse.containsMouse ? "#3A211E" : "#2A1D1B")
            : muted ? (mouse.containsMouse ? "#202638" : "#161A23")
            : (mouse.containsMouse ? "#8B99FF" : "#7C8CFF")
        border.width: muted || danger ? 1 : 0
        border.color: danger ? "#FF7A59" : "#30384C"

        Behavior on color { ColorAnimation { duration: 140 } }

        Text {
            id: label
            anchors.centerIn: parent
            color: button.danger ? "#FFB09B" : button.muted ? "#AAB4C6" : "white"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.clicked()
        }
    }
}
