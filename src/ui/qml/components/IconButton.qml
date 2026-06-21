import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property string iconText: ""
    property string tooltip: ""
    signal clicked()

    width: 32
    height: 32
    radius: 8
    color: mouse.containsMouse ? Theme.surfaceHover : "transparent"

    Behavior on color { ColorAnimation { duration: 140 } }

    Text {
        anchors.centerIn: parent
        text: root.iconText
        color: Theme.secondaryText
        font.pixelSize: 14
        font.weight: Font.DemiBold
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }

    ToolTip.visible: mouse.containsMouse && root.tooltip.length > 0
    ToolTip.text: root.tooltip
    ToolTip.delay: 500
}
