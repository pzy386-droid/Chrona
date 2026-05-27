import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var options: []
    property int selectedIndex: 0

    width: Math.max(112, options.length * 56)
    height: 38
    radius: 8
    color: "#161A23"
    border.color: "#2A3142"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: root.options

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: root.selectedIndex === index ? "#252C3E" : "transparent"

                Behavior on color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: root.selectedIndex === index ? "#E6EAF2" : "#9AA4B2"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.selectedIndex = index
                }
            }
        }
    }
}
