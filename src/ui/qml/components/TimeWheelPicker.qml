import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    property string title: qsTr("选择时间")
    property string timeText: "09:00"
    property bool suppressApply: false
    signal accepted(string timeText)

    width: 232
    height: 232
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openWith(time) {
        var parts = String(time || "09:00").split(":")
        var h = Number(parts[0])
        var m = Number(parts[1])
        if (isNaN(h)) h = 9
        if (isNaN(m)) m = 0
        root.suppressApply = true
        hourWheel.setIndex(Math.max(0, Math.min(23, h)))
        minuteWheel.setIndex(Math.max(0, Math.min(minuteWheel.count - 1, Math.round(m / minuteWheel.step))))
        root.suppressApply = false
        root.timeText = currentTimeText()
        root.open()
    }

    function currentTimeText() {
        return String(hourWheel.selectedValue()).padStart(2, "0") + ":" + String(minuteWheel.selectedValue()).padStart(2, "0")
    }

    function applyCurrent() {
        var value = currentTimeText()
        if (!root.suppressApply && root.timeText !== value) {
            root.timeText = value
            root.accepted(value)
        }
    }

    background: Rectangle {
        radius: 18
        color: "#151A23"
        border.width: 1
        border.color: "#30384C"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 9

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: root.title
                color: "#E6EAF2"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Text {
                text: root.currentTimeText()
                color: "#7C8CFF"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 128
            radius: 14
            color: "#171C27"
            border.width: 1
            border.color: "#30384C"
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                height: 32
                radius: 10
                color: "#26304A"
                border.width: 1
                border.color: "#7C8CFF"
                opacity: 0.82
            }

            Row {
                anchors.centerIn: parent
                width: 116
                height: parent.height
                spacing: 0

                WheelList {
                    id: hourWheel
                    width: 48
                    height: parent.height
                    count: 24
                    step: 1
                }

                Text {
                    width: 20
                    height: parent.height
                    text: ":"
                    color: "#9AA4B2"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                WheelList {
                    id: minuteWheel
                    width: 48
                    height: parent.height
                    count: 12
                    step: 5
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("滑动选择，点击外部返回")
            color: "#7F8A9E"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }
    }

    component WheelList: ListView {
        id: wheel
        property int count: 24
        property int step: 1
        property int selectedIndex: 0
        readonly property int itemSize: 32

        model: count
        clip: true
        interactive: true
        flickDeceleration: 15000
        maximumFlickVelocity: 420
        boundsBehavior: Flickable.StopAtBounds
        topMargin: (height - itemSize) / 2
        bottomMargin: (height - itemSize) / 2
        snapMode: ListView.SnapOneItem
        highlightMoveDuration: 130
        preferredHighlightBegin: height / 2 - itemSize / 2
        preferredHighlightEnd: height / 2 + itemSize / 2
        highlightRangeMode: ListView.ApplyRange
        cacheBuffer: 300

        function setIndex(index) {
            selectedIndex = index
            currentIndex = index
            positionViewAtIndex(index, ListView.Center)
            root.applyCurrent()
        }

        function selectedValue() {
            return selectedIndex * step
        }

        function updateSelected() {
            var idx = indexAt(width / 2, contentY + height / 2)
            if (idx < 0) {
                idx = selectedIndex
            }
            selectedIndex = Math.max(0, Math.min(count - 1, idx))
        }

        onContentYChanged: {
            if (moving || flicking) {
                updateSelected()
            }
        }
        onMovementEnded: {
            updateSelected()
            root.applyCurrent()
        }
        onFlickEnded: {
            updateSelected()
            root.applyCurrent()
        }

        delegate: Item {
            width: wheel.width
            height: wheel.itemSize
            readonly property real distance: Math.abs(index - wheel.selectedIndex)

            Text {
                anchors.centerIn: parent
                text: String(index * wheel.step).padStart(2, "0")
                color: distance === 0 ? "#F2F5FF" : "#6F7B91"
                opacity: distance === 0 ? 1 : distance === 1 ? 0.58 : 0.28
                font.pixelSize: distance === 0 ? 22 : 14
                font.weight: distance === 0 ? Font.DemiBold : Font.Normal
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
