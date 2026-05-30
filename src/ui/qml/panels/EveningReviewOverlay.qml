import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    visible: false // 默认隐藏，等侧边栏按钮点击后才会显示
    color: "#B00A0E17"
    z: 200

    // 拦截点击事件，防止点到后面的日历
    MouseArea { anchors.fill: parent }

    Rectangle {
        width: 640
        height: 460
        radius: 16
        color: "#161A23"
        border.color: "#30384C"
        border.width: 1
        anchors.centerIn: parent

        // 1. 标题
        Text {
            id: titleText
            text: "🌙 晚间复盘：今天辛苦了"
            color: "#E6EAF2"
            font.pixelSize: 22
            font.weight: Font.Bold
            anchors { top: parent.top; topMargin: 24; horizontalCenter: parent.horizontalCenter }
        }

        Column {
            anchors { top: titleText.bottom; topMargin: 32; left: parent.left; right: parent.right; margins: 32 }
            spacing: 24

            // 2. 数据看板
            Row {
                spacing: 60
                anchors.horizontalCenter: parent.horizontalCenter

                Column {
                    spacing: 8
                    Text { text: "今日专注时长"; color: "#94A3B8"; font.pixelSize: 14; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: "120 分钟"; color: "#A855F7"; font.pixelSize: 32; font.weight: Font.Bold; anchors.horizontalCenter: parent.horizontalCenter }
                }
                Column {
                    spacing: 8
                    Text { text: "完成任务数"; color: "#94A3B8"; font.pixelSize: 14; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: "5 项"; color: "#4ADE80"; font.pixelSize: 32; font.weight: Font.Bold; anchors.horizontalCenter: parent.horizontalCenter }
                }
            }

            Rectangle { width: parent.width; height: 1; color: "#30384C" }

            // 3. 顺延提醒
            Text { text: "🔄 顺延与自动调整："; color: "#E6EAF2"; font.weight: Font.DemiBold }

            Rectangle {
                width: parent.width; height: 60
                color: "#1E293B"
                radius: 8
                Text {
                    anchors.centerIn: parent
                    text: "有 2 项未完成的非固定任务已由 AI 自动排程至明天。"; color: "#94A3B8"
                }
            }
        }

        // 4. 结束按钮
        Button {
            text: "💤 确认并结束今日学习"
            anchors { bottom: parent.bottom; bottomMargin: 24; horizontalCenter: parent.horizontalCenter }
            highlighted: true
            onClicked: {
                root.visible = false // 点击后关闭弹窗
            }
        }
    }
}