import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    width: 86
    height: 38
    radius: 10
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    Rectangle {
        x: ThemeService.dark ? parent.width / 2 : 4
        y: 4
        width: parent.width / 2 - 4
        height: parent.height - 8
        radius: 8
        color: ThemeService.dark ? "#26324A" : "#FFF4C7"
        border.width: 1
        border.color: ThemeService.dark ? "#465778" : "#E7C766"

        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 180 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Canvas {
                id: sunIcon
                anchors.centerIn: parent
                width: 20
                height: 20

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = ThemeService.dark ? "#91A6D4" : "#D69B16"
                    ctx.fillStyle = ctx.strokeStyle
                    ctx.lineWidth = 1.8
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    ctx.arc(10, 10, 3.6, 0, Math.PI * 2)
                    ctx.fill()
                    for (var i = 0; i < 8; ++i) {
                        var angle = i * Math.PI / 4
                        ctx.beginPath()
                        ctx.moveTo(10 + Math.cos(angle) * 6.1, 10 + Math.sin(angle) * 6.1)
                        ctx.lineTo(10 + Math.cos(angle) * 8.3, 10 + Math.sin(angle) * 8.3)
                        ctx.stroke()
                    }
                }

                Connections {
                    target: ThemeService
                    function onDarkChanged() { sunIcon.requestPaint() }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Canvas {
                id: moonIcon
                anchors.centerIn: parent
                width: 20
                height: 20

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.fillStyle = ThemeService.dark ? "#EEF3FF" : "#94A3B8"
                    ctx.beginPath()
                    ctx.arc(9, 10, 6.4, Math.PI * 0.35, Math.PI * 1.65, false)
                    ctx.arc(12.3, 8.2, 5.5, Math.PI * 1.48, Math.PI * 0.52, true)
                    ctx.closePath()
                    ctx.fill()
                }

                Connections {
                    target: ThemeService
                    function onDarkChanged() { moonIcon.requestPaint() }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: ThemeService.toggle()
    }
}
