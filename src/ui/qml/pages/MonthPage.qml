import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    color: Theme.appBackground

    property int shownYear: new Date().getFullYear()
    property int shownMonth: new Date().getMonth() + 1
    property var days: []

    signal dayRequested(string dateText)

    function reloadMonth() {
        days = ScheduleService.monthOverview(shownYear, shownMonth)
    }

    function selectToday() {
        var now = new Date()
        shownYear = now.getFullYear()
        shownMonth = now.getMonth() + 1
        reloadMonth()
    }

    Component.onCompleted: reloadMonth()

    Connections {
        target: ScheduleService
        function onDataChanged() {
            root.reloadMonth()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Rectangle {
            id: header
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            radius: 16
            color: Theme.surfaceElevated
            border.width: 1
            border.color: Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("月历总览")
                        color: Theme.strongText
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("回顾学习记录，快速定位任意一天")
                        color: Theme.tertiaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 178
                    Layout.preferredHeight: 38
                    radius: 11
                    color: monthSelectMouse.containsMouse ? Theme.surfaceHover : Theme.canvasBackground
                    border.width: 1
                    border.color: monthPickerPopup.opened ? Theme.accentBright : Theme.border

                    Behavior on color { ColorAnimation { duration: 140 } }
                    Behavior on border.color { ColorAnimation { duration: 140 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 13
                        anchors.rightMargin: 11
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: root.shownYear + qsTr("年 ") + root.shownMonth + qsTr("月")
                            color: Theme.primaryText
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text {
                            text: "⌄"
                            color: Theme.accentBright
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            rotation: monthPickerPopup.opened ? 180 : 0
                            Behavior on rotation {
                                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                            }
                        }
                    }

                    MouseArea {
                        id: monthSelectMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: monthPickerPopup.openWith(root.shownYear, root.shownMonth)
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 82
                    Layout.preferredHeight: 38
                    radius: 11
                    color: todayMouse.containsMouse ? "#8B99FF" : Theme.accent

                    Behavior on color { ColorAnimation { duration: 140 } }

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("今天")
                        color: "white"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: todayMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectToday()
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 7
            columnSpacing: 1
            rowSpacing: 1

            Repeater {
                model: [qsTr("周一"), qsTr("周二"), qsTr("周三"), qsTr("周四"), qsTr("周五"), qsTr("周六"), qsTr("周日")]

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: Theme.canvasBackground

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: index >= 5 ? "#A78BFA" : Theme.tertiaryText
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 7
            rows: 6
            columnSpacing: 6
            rowSpacing: 6

            Repeater {
                model: root.days

                delegate: Rectangle {
                    id: cell
                    required property var modelData
                    readonly property var dayData: modelData

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 92
                    radius: 10
                    color: cellMouse.containsMouse ? Theme.surfaceHover : (dayData.isSelected ? Theme.surfaceSelected : Theme.surfaceElevated)
                    border.width: dayData.isToday || dayData.isSelected ? 1 : 0
                    border.color: dayData.isToday ? Theme.accentBright : Theme.border
                    opacity: dayData.inMonth ? 1 : 0.48

                    Behavior on color { ColorAnimation { duration: 120 } }
                    Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 5

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: cell.dayData.day
                                color: cell.dayData.isToday ? Theme.accentText : Theme.primaryText
                                font.pixelSize: 14
                                font.weight: cell.dayData.isToday ? Font.Bold : Font.DemiBold
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                visible: cell.dayData.plannedMinutes > 0
                                text: cell.dayData.completedMinutes + "/" + cell.dayData.plannedMinutes + qsTr(" 分")
                                color: cell.dayData.completionRate >= 80 ? "#6FD6A7" : Theme.tertiaryText
                                font.pixelSize: 10
                            }
                        }

                        Repeater {
                            model: Math.min(3, cell.dayData.entries.length)

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 18
                                radius: 4
                                color: cell.dayData.entries[index].color
                                opacity: cell.dayData.entries[index].completed ? 0.48 : 0.82

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 4
                                    verticalAlignment: Text.AlignVCenter
                                    text: cell.dayData.entries[index].title
                                    color: Theme.primaryText
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Text {
                            visible: cell.dayData.moreCount > 0
                            text: qsTr("还有 %1 项").arg(cell.dayData.moreCount)
                            color: Theme.accentBright
                            font.pixelSize: 10
                        }

                        Item { Layout.fillHeight: true }
                    }

                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.dayRequested(cell.dayData.dateText)
                    }
                }
            }
        }
    }

    MonthWheelPicker {
        id: monthPickerPopup
        x: Math.max(12, root.width / 2 - width / 2)
        y: 84

        onAccepted: function(year, month) {
            root.shownYear = year
            root.shownMonth = month
            root.reloadMonth()
        }
    }
}
