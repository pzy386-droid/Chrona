import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona
import "./panels"

ApplicationWindow {
    id: window
    width: 1440
    height: 920
    visible: true
    title: "Chrona"
    color: "#0F1117"

    property bool detailOpen: true
    property string currentPage: "timeline"

    function scrollToFocusBlock() {
        if (timelinePage && typeof timelinePage.scrollToFocusBlock === 'function') {
            timelinePage.scrollToFocusBlock()
        }
    }

    Connections {
        target: ScheduleService

        function onSelectedTaskChanged() {
            window.detailOpen = true
        }
    }

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
                scrollToFocus: window.scrollToFocusBlock
                currentPage: window.currentPage
                onNavigateRequested: function(page) {
                    window.currentPage = page
                    if (page === "month") {
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
                currentIndex: window.currentPage === "month" ? 1 : 0

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
            color: panelOpenMouse.containsMouse ? "#202638" : "#161A23"
            border.width: 1
            border.color: "#30384C"
            z: 100

            Text {
                anchors.centerIn: parent
                text: "›"
                color: "#E6EAF2"
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
        }

        EveningReviewOverlay {
            id: eveningReviewOverlay
            anchors.fill: parent
            visible: false
        }
    }
}
