import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona
import "./panels"
import "./pages"

ApplicationWindow {
    id: window
    width: 1440
    height: 920
    visible: true
    title: "Chrona"
    color: Theme.appBackground

    property bool detailOpen: false
    property string currentPage: "timeline"

    Connections {
        target: ScheduleService

        function onSelectedTaskChanged() {
            Qt.callLater(function() {
                var detail = ScheduleService.selectedDetail || ({})
                window.detailOpen = Number(detail.id || 0) > 0
            })
        }
    }

    Rectangle {
        id: appShell
        anchors.fill: parent
        visible: ScheduleService.firstLoginCompleted
        enabled: visible
        color: Theme.appBackground

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Sidebar {
                id: sidebar
                Layout.fillHeight: true
                Layout.preferredWidth: collapsed ? 76 : 248
                currentPage: window.currentPage
                onNavigateRequested: function(page) {
                    window.currentPage = page
                    if (page === "month" || page === "deadlines") {
                        window.detailOpen = false
                    }
                }
                onDailyPlanRequested: {
                    dailyPlanOverlay.mode = "daily"
                    dailyPlanOverlay.visible = true
                }

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.currentPage === "month" ? 1 : window.currentPage === "deadlines" ? 2 : window.currentPage === "insights" ? 3 : window.currentPage === "memory" ? 4 : 0

                TimelinePage {
                    id: timelinePage
                    leftPadding: 24
                    rightPadding: 20
                }

                MonthPage {
                    id: monthPage
                    onDayRequested: function(dateText) {
                        ScheduleService.setSelectedDate(dateText)
                        timelinePage.viewMode = "day"
                        window.currentPage = "timeline"
                        window.detailOpen = false
                    }
                }

                DeadlinePage {
                    id: deadlinePage
                }

                InsightsPage {
                    id: insightsPage
                }

                MemoryPage {
                    id: memoryPage
                }
            }

            TaskDetailPanel {
                id: detailPanel
                Layout.fillHeight: true
                Layout.preferredWidth: window.detailOpen && window.currentPage === "timeline" ? 332 : 0
                clip: true
                opacity: window.detailOpen && window.currentPage === "timeline" ? 1 : 0
                task: ScheduleService.selectedDetail
                readOnly: ScheduleService.selectedDateText !== Qt.formatDate(new Date(), "yyyy-MM-dd")
                onCloseRequested: window.detailOpen = false

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
                }

                Behavior on opacity {
                    NumberAnimation { duration: 160 }
                }
            }
        }

        Rectangle {
            visible: !window.detailOpen && window.currentPage === "timeline"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 16
            width: 38
            height: 78
            radius: 12
            color: panelOpenMouse.containsMouse ? Theme.surfaceHover : Theme.surface
            border.width: 1
            border.color: Theme.border
            z: 100

            Text {
                anchors.centerIn: parent
                text: "\u203A"
                color: Theme.primaryText
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: panelOpenMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: window.detailOpen = true
            }
        }

        DailyPlanOverlay {
            id: dailyPlanOverlay
            anchors.fill: parent
            visible: false
            onExecutionFinished: function(ok, message) {
                if (timelinePage && typeof timelinePage.showOperationMessage === "function") {
                    timelinePage.showOperationMessage(message, ok)
                }
            }
        }

        EveningReviewOverlay {
            id: eveningReviewOverlay
            anchors.fill: parent
            visible: false
        }
    }

    LoginPage {
        anchors.fill: parent
        visible: !ScheduleService.firstLoginCompleted
        enabled: visible
        onLoginCompleted: {
            window.currentPage = "timeline"
            window.detailOpen = true
        }
    }
}
