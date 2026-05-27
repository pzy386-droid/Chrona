import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

ApplicationWindow {
    id: window
    width: 1440
    height: 920
    visible: true
    title: "Chrona"
    color: "#0F1117"

    Rectangle {
        anchors.fill: parent
        color: "#0F1117"

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Sidebar {
                id: sidebar
                Layout.fillHeight: true
                Layout.preferredWidth: collapsed ? 76 : 248
                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                }
            }

            TimelinePage {
                Layout.fillWidth: true
                Layout.fillHeight: true
                leftPadding: 24
                rightPadding: 20
            }

            TaskDetailPanel {
                Layout.fillHeight: true
                Layout.preferredWidth: 332
                task: ScheduleService.selectedTask
            }
        }
    }
}
