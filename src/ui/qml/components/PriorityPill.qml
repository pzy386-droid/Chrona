import QtQuick

Rectangle {
    id: root
    property int priority: 1

    width: 38
    height: 20
    radius: 6
    color: priority === 2 ? Theme.dangerSurface : priority === 1 ? Theme.infoSurface : Theme.surfacePressed
    border.width: 1
    border.color: priority === 2 ? Theme.dangerBorder : priority === 1 ? Theme.accentBright : Theme.border

    Text {
        anchors.centerIn: parent
        text: priority === 2 ? qsTr("高") : priority === 1 ? qsTr("中") : qsTr("低")
        color: priority === 2 ? Theme.error : priority === 1 ? Theme.accentBright : Theme.secondaryText
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
