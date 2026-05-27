import QtQuick

Rectangle {
    id: root
    property int priority: 1

    width: 38
    height: 20
    radius: 6
    color: priority === 2 ? "#36211F" : priority === 1 ? "#1E2640" : "#222B3A"
    border.width: 1
    border.color: priority === 2 ? "#FF7A59" : priority === 1 ? "#7C8CFF" : "#6D7A92"

    Text {
        anchors.centerIn: parent
        text: priority === 2 ? qsTr("高") : priority === 1 ? qsTr("中") : qsTr("低")
        color: priority === 2 ? "#FFB09B" : priority === 1 ? "#B8C0FF" : "#AEB8CA"
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
