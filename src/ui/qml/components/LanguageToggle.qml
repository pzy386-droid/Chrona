import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property string currentLocale: "zh_CN"
    signal localeRequested(string locale)

    width: 86
    height: 38
    radius: 8
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: [
                {label: "中", locale: "zh_CN"},
                {label: "EN", locale: "en_US"}
            ]

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: root.currentLocale === modelData.locale ? Theme.accentBright : "transparent"

                Behavior on color { ColorAnimation { duration: 160 } }

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: root.currentLocale === modelData.locale ? "white" : Theme.secondaryText
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.localeRequested(modelData.locale)
                }
            }
        }
    }
}
