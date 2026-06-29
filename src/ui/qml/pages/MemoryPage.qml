import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chrona

Rectangle {
    id: root
    color: Theme.appBackground

    property var overview: ({ count: 0, items: [] })
    property bool confirmClear: false
    property string statusText: ""
    property string preferenceText: ""

    function reload() {
        overview = ScheduleService.memoryOverview()
    }

    Component.onCompleted: reload()

    Connections {
        target: ScheduleService
        function onDataChanged() { root.reload() }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.max(760, parent.width)
            spacing: 16
            anchors.margins: 24

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 126
                radius: 22
                color: Theme.surfaceElevated
                border.width: 1
                border.color: Theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 16

                    Rectangle {
                        width: 62
                        height: 62
                        radius: 20
                        color: Theme.surfaceSelected
                        border.width: 1
                        border.color: Theme.accentBright

                        Text {
                            anchors.centerIn: parent
                            text: "AI"
                            color: Theme.accentBright
                            font.pixelSize: 20
                            font.weight: Font.Black
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            text: "\u0043\u0068\u0072\u006f\u006e\u0061 \u0041\u0049 \u8bb0\u5fc6\u5e93"
                            color: Theme.strongText
                            font.pixelSize: 25
                            font.weight: Font.Bold
                        }
                        Text {
                            text: "\u8bb0\u5f55\u4f60\u7684\u5b66\u4e60\u504f\u597d\u3001\u5b8c\u6210\u6a21\u5f0f\u548c\u65f6\u95f4\u8282\u594f\uff0c\u7528\u4e8e\u540e\u7eed\u89c4\u5212\u3002"
                            color: Theme.secondaryText
                            font.pixelSize: 12
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: String(root.overview.count || 0)
                            color: "#A78BFA"
                            font.pixelSize: 32
                            font.weight: Font.Bold
                        }
                        Text {
                            text: "\u6761\u6709\u6548\u8bb0\u5fc6"
                            color: Theme.tertiaryText
                            font.pixelSize: 11
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 158
                radius: 20
                color: Theme.surfaceElevated
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "\u544a\u8bc9 Chrona \u600e\u4e48\u5e2e\u4f60\u6700\u597d"
                                color: Theme.strongText
                                font.pixelSize: 18
                                font.weight: Font.Bold
                            }

                            Text {
                                text: "\u4f8b\u5982\uff1a\u665a\u4e0a10\u70b9\u540e\u4e0d\u8981\u518d\u5b89\u6392\u65f6\u95f4\u5757 / \u82f1\u8bed\u542c\u529b\u5c3d\u91cf\u653e\u5728\u65e9\u4e0a / \u5468\u65e5\u4e0d\u6392\u91cd\u4efb\u52a1"
                                color: Theme.secondaryText
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }

                        Rectangle {
                            width: 104
                            height: 40
                            radius: 12
                            color: addPrefMouse.containsMouse ? Theme.accentBright : "#5B6CFF"
                            opacity: preferenceInput.text.trim().length >= 4 ? 1.0 : 0.45

                            Text {
                                anchors.centerIn: parent
                                text: "\u52a0\u5165\u8bb0\u5fc6"
                                color: "white"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                            }

                            MouseArea {
                                id: addPrefMouse
                                anchors.fill: parent
                                enabled: preferenceInput.text.trim().length >= 4
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var result = ScheduleService.addAIMemoryPreference(preferenceInput.text)
                                    root.statusText = result.message || ""
                                    if (result.ok) {
                                        preferenceInput.text = ""
                                        root.reload()
                                    }
                                }
                            }
                        }
                    }

                    TextArea {
                        id: preferenceInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 58
                        placeholderText: "\u5199\u4e0b\u4f60\u5e0c\u671b AI \u957f\u671f\u8bb0\u4f4f\u7684\u504f\u597d..."
                        text: root.preferenceText
                        wrapMode: TextArea.Wrap
                        color: Theme.primaryText
                        placeholderTextColor: Theme.tertiaryText
                        font.pixelSize: 13
                        background: Rectangle {
                            radius: 14
                            color: Theme.surfaceMuted
                            border.width: 1
                            border.color: preferenceInput.activeFocus ? Theme.accentBright : Theme.border
                        }
                        onTextChanged: root.preferenceText = text
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: "\u8bb0\u5fc6\u6761\u76ee"
                    color: Theme.strongText
                    font.pixelSize: 18
                    font.weight: Font.Bold
                }

                Text {
                    visible: root.statusText.length > 0
                    text: root.statusText
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }

                Rectangle {
                    width: root.confirmClear ? 128 : 100
                    height: 38
                    radius: 11
                    color: clearMouse.containsMouse
                        ? (Theme.dark ? "#352125" : "#FFF0EE")
                        : Theme.surfaceElevated
                    border.width: 1
                    border.color: root.confirmClear ? Theme.error : Theme.border

                    Text {
                        anchors.centerIn: parent
                        text: root.confirmClear ? "\u518d\u6b21\u70b9\u51fb\u786e\u8ba4" : "\u6e05\u7a7a\u8bb0\u5fc6"
                        color: root.confirmClear ? Theme.error : Theme.secondaryText
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: clearMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!root.confirmClear) {
                                root.confirmClear = true
                                return
                            }
                            var result = ScheduleService.clearAIMemory()
                            root.statusText = result.message || ""
                            root.confirmClear = false
                            root.reload()
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(460, memoryList.contentHeight + 24)
                radius: 20
                color: Theme.surfaceElevated
                border.width: 1
                border.color: Theme.border

                ListView {
                    id: memoryList
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 9
                    clip: true
                    interactive: contentHeight > height
                    model: root.overview.items || []

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 82
                        radius: 14
                        color: memoryMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceMuted
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            Rectangle {
                                width: 38
                                height: 38
                                radius: 12
                                color: modelData.kind === "preference"
                                    ? (Theme.dark ? "#2B2540" : "#F1ECFF")
                                    : modelData.kind === "completion"
                                        ? (Theme.dark ? "#17342B" : "#E8F8F1")
                                        : (Theme.dark ? "#202D3D" : "#EAF2FC")

                                Text {
                                    anchors.centerIn: parent
                                    text: root.kindLabel(modelData.kind).charAt(0)
                                    color: modelData.kind === "preference" ? "#A78BFA"
                                        : modelData.kind === "completion" ? Theme.success : Theme.accentBright
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.content || ""
                                    color: Theme.primaryText
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    spacing: 10
                                    Text {
                                        text: root.kindLabel(modelData.kind)
                                        color: Theme.secondaryText
                                        font.pixelSize: 10
                                    }
                                    Text {
                                        text: "\u6743\u91cd " + Number(modelData.weight || 0).toFixed(2)
                                        color: Theme.tertiaryText
                                        font.pixelSize: 10
                                    }
                                    Text {
                                        text: root.formatTime(modelData.updatedAt)
                                        color: Theme.mutedText
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: memoryMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: memoryList.count === 0
                        width: Math.min(parent.width - 60, 520)
                        horizontalAlignment: Text.AlignHCenter
                        text: "\u8fd8\u6ca1\u6709\u5f62\u6210\u8bb0\u5fc6\u3002\u5b8c\u6210\u4efb\u52a1\u3001\u8c03\u6574\u65f6\u95f4\u5757\u6216\u4fee\u6539\u5b66\u4e60\u504f\u597d\u540e\uff0cChrona \u4f1a\u81ea\u52a8\u5b66\u4e60\u3002"
                        color: Theme.tertiaryText
                        font.pixelSize: 13
                        lineHeight: 1.4
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                radius: 16
                color: Theme.surfaceMuted
                border.width: 1
                border.color: Theme.border

                Text {
                    anchors.fill: parent
                    anchors.margins: 16
                    text: "\u9690\u79c1\u8bf4\u660e\uff1a\u8bb0\u5fc6\u4fdd\u5b58\u5728\u672c\u5730\u6570\u636e\u5e93\u4e2d\uff0c\u53ea\u5728\u751f\u6210\u65e5\u7a0b\u5efa\u8bae\u65f6\u4f5c\u4e3a\u8f6f\u504f\u597d\u4f7f\u7528\u3002\u4f60\u53ef\u4ee5\u968f\u65f6\u6e05\u7a7a\u3002"
                    color: Theme.secondaryText
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    function kindLabel(kind) {
        if (kind === "preference") return "\u504f\u597d"
        if (kind === "completion") return "\u5b8c\u6210"
        if (kind === "study_pattern") return "\u8282\u594f"
        return "\u8bb0\u5fc6"
    }

    function formatTime(value) {
        if (!value) return ""
        var date = new Date(value)
        if (isNaN(date.getTime())) return String(value)
        return Qt.formatDateTime(date, "MM-dd HH:mm")
    }
}