import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    color: "#0F1117"
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
        yearPicker.currentIndex = 60
        monthPicker.currentIndex = shownMonth - 1
        reloadMonth()
    }

    Component.onCompleted: reloadMonth()

    Connections {
        target: ScheduleService
        function onDataChanged() { root.reloadMonth() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            radius: 14
            color: "#161B22"
            border.width: 1
            border.color: "#2A3140"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text { text: qsTr("月历总览"); color: "#FFFFFF"; font.pixelSize: 20; font.weight: Font.DemiBold }
                    Text { text: qsTr("回顾学习记录，快速定位任意一天"); color: "#8F9AB0"; font.pixelSize: 12 }
                }

                ComboBox {
                    id: yearPicker
                    Layout.preferredWidth: 112
                    model: Array.from({length: 71}, function(_, i) { return new Date().getFullYear() - 60 + i })
                    currentIndex: 60
                    onActivated: {
                        root.shownYear = Number(currentText)
                        root.reloadMonth()
                    }
                    contentItem: Text { text: yearPicker.displayText + qsTr("年"); color: "#E6EAF2"; verticalAlignment: Text.AlignVCenter; leftPadding: 12 }
                    background: Rectangle { radius: 9; color: "#11151D"; border.width: 1; border.color: "#30384C" }
                }

                ComboBox {
                    id: monthPicker
                    Layout.preferredWidth: 92
                    model: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
                    currentIndex: root.shownMonth - 1
                    onActivated: {
                        root.shownMonth = Number(currentText)
                        root.reloadMonth()
                    }
                    contentItem: Text { text: monthPicker.displayText + qsTr("月"); color: "#E6EAF2"; verticalAlignment: Text.AlignVCenter; leftPadding: 12 }
                    background: Rectangle { radius: 9; color: "#11151D"; border.width: 1; border.color: "#30384C" }
                }

                Rectangle {
                    Layout.preferredWidth: 82
                    Layout.preferredHeight: 38
                    radius: 9
                    color: todayMouse.containsMouse ? "#8B99FF" : "#6C63FF"
                    Text { anchors.centerIn: parent; text: qsTr("今天"); color: "white"; font.pixelSize: 13; font.weight: Font.DemiBold }
                    MouseArea { id: todayMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.selectToday() }
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
                    color: "#11151D"
                    Text { anchors.centerIn: parent; text: modelData; color: index >= 5 ? "#A78BFA" : "#7F8A9E"; font.pixelSize: 12; font.weight: Font.DemiBold }
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
                    color: cellMouse.containsMouse ? "#1B2130" : (modelData.isSelected ? "#1A2033" : "#131821")
                    border.width: modelData.isToday || modelData.isSelected ? 1 : 0
                    border.color: modelData.isToday ? "#7C8CFF" : "#4B5674"
                    opacity: modelData.inMonth ? 1 : 0.48

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 5

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: cell.dayData.day
                                color: cell.dayData.isToday ? "#FFFFFF" : "#C8D0DE"
                                font.pixelSize: 14
                                font.weight: cell.dayData.isToday ? Font.Bold : Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                visible: cell.dayData.plannedMinutes > 0
                                text: cell.dayData.completedMinutes + "/" + cell.dayData.plannedMinutes + qsTr(" 分")
                                color: cell.dayData.completionRate >= 80 ? "#6FD6A7" : "#8F9AB0"
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
                                    color: "#F7F8FC"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Text {
                            visible: cell.dayData.moreCount > 0
                            text: qsTr("还有 %1 项").arg(cell.dayData.moreCount)
                            color: "#7C8CFF"
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
}
