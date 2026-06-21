import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    property int year: new Date().getFullYear()
    property int month: new Date().getMonth() + 1
    property int baseYear: new Date().getFullYear() - 60
    property int yearCount: 71
    property bool suppressApply: false
    signal accepted(int year, int month)

    width: 292
    height: 286
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openWith(y, m) {
        var safeYear = Math.max(baseYear, Math.min(baseYear + yearCount - 1, Number(y) || new Date().getFullYear()))
        var safeMonth = Math.max(1, Math.min(12, Number(m) || 1))
        root.suppressApply = true
        yearWheel.setIndex(safeYear - baseYear, false)
        monthWheel.setIndex(safeMonth - 1, false)
        root.year = safeYear
        root.month = safeMonth
        root.suppressApply = false
        root.open()
    }

    function applyCurrent() {
        if (root.suppressApply) {
            return
        }
        var nextYear = yearWheel.selectedValue()
        var nextMonth = monthWheel.selectedValue()
        if (root.year !== nextYear || root.month !== nextMonth) {
            root.year = nextYear
            root.month = nextMonth
        }
    }

    function acceptCurrent() {
        applyCurrent()
        root.accepted(root.year, root.month)
        root.close()
    }

    background: Rectangle {
        radius: 22
        color: Theme.surfaceMuted
        border.width: 1
        border.color: Theme.border

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 21
            color: "transparent"
            border.width: 1
            border.color: Theme.borderSoft
            opacity: 0.9
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("选择月份")
                color: Theme.primaryText
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Text {
                text: root.year + qsTr("年 ") + root.month + qsTr("月")
                color: Theme.accentBright
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 162
            radius: 16
            color: Theme.canvasBackground
            border.width: 1
            border.color: Theme.border
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                height: 36
                radius: 12
                color: Theme.surfaceSelected
                border.width: 1
                border.color: Theme.accentBright
                opacity: 0.84
            }

            Row {
                anchors.centerIn: parent
                width: 176
                height: parent.height
                spacing: 10

                WheelList {
                    id: yearWheel
                    width: 92
                    height: parent.height
                    count: root.yearCount
                    baseValue: root.baseYear
                    suffix: qsTr("年")
                    onSettled: root.applyCurrent()
                }

                WheelList {
                    id: monthWheel
                    width: 74
                    height: parent.height
                    count: 12
                    baseValue: 1
                    suffix: qsTr("月")
                    onSettled: root.applyCurrent()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 12
                color: cancelMouse.containsMouse ? Theme.surfaceHover : Theme.canvasBackground
                border.width: 1
                border.color: Theme.border

                Text {
                    anchors.centerIn: parent
                    text: qsTr("取消")
                    color: Theme.primaryText
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 12
                color: acceptMouse.containsMouse ? "#8B99FF" : Theme.accentBright

                Text {
                    anchors.centerIn: parent
                    text: qsTr("完成")
                    color: "white"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: acceptMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.acceptCurrent()
                }
            }
        }
    }

    component WheelList: Item {
        id: wheel
        property int count: 12
        property int baseValue: 1
        property string suffix: ""
        property int selectedIndex: 0
        property real animatedOffset: 0
        property real wheelDelta: 0
        property real dragStartY: 0
        property int dragStartIndex: 0
        readonly property int itemSize: 36
        signal settled()

        clip: true

        function clampIndex(index) {
            return Math.max(0, Math.min(count - 1, index))
        }

        function setIndex(index, animated) {
            var next = clampIndex(index)
            if (next === selectedIndex && animated !== false) {
                return
            }
            var direction = next > selectedIndex ? 1 : -1
            selectedIndex = next
            slideAnimation.stop()
            if (animated === false) {
                animatedOffset = 0
            } else {
                animatedOffset = direction * itemSize
                slideAnimation.start()
            }
            settled()
        }

        function selectedValue() {
            return baseValue + selectedIndex
        }

        function stepBy(delta) {
            setIndex(selectedIndex + delta, true)
        }

        Repeater {
            model: 5

            Text {
                readonly property int offset: index - 2
                readonly property int valueIndex: wheel.selectedIndex + offset
                width: wheel.width
                height: wheel.itemSize
                x: 0
                y: wheel.height / 2 - wheel.itemSize / 2 + offset * wheel.itemSize + wheel.animatedOffset
                text: valueIndex >= 0 && valueIndex < wheel.count
                    ? String(wheel.baseValue + valueIndex) + wheel.suffix
                    : ""
                color: offset === 0 ? Theme.primaryText : Theme.mutedText
                opacity: Math.abs(offset) === 0 ? 1 : Math.abs(offset) === 1 ? 0.56 : 0.26
                font.pixelSize: offset === 0 ? 20 : 13
                font.weight: offset === 0 ? Font.DemiBold : Font.Normal
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        NumberAnimation {
            id: slideAnimation
            target: wheel
            property: "animatedOffset"
            to: 0
            duration: 170
            easing.type: Easing.OutCubic
        }

        WheelHandler {
            target: null
            onWheel: function(event) {
                var delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.pixelDelta.y * 8
                if (delta === 0) {
                    event.accepted = true
                    return
                }
                wheel.wheelDelta += delta
                if (Math.abs(wheel.wheelDelta) >= 120) {
                    wheel.stepBy(wheel.wheelDelta > 0 ? -1 : 1)
                    wheel.wheelDelta = 0
                }
                event.accepted = true
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onPressed: {
                wheel.dragStartY = mouse.y
                wheel.dragStartIndex = wheel.selectedIndex
            }
            onPositionChanged: {
                if (!pressed) {
                    return
                }
                var delta = Math.round((wheel.dragStartY - mouse.y) / (wheel.itemSize * 1.35))
                wheel.setIndex(wheel.dragStartIndex + delta, true)
            }
        }
    }
}
