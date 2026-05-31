import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    // 默认隐藏，等后续需要显示时再改成 true
    visible: false
    color: "#B00A0E17" // 半透明深色背景
    z: 200 // 确保在最顶层

    // 拦截点击事件，防止穿透到下层时间轴
    MouseArea { anchors.fill: parent }

    // 弹窗主体内容
    Rectangle {
        width: 640
        height: 520
        radius: 16
        color: "#161A23"
        border.color: "#30384C"
        border.width: 1
        anchors.centerIn: parent

        // 1. 头部标题
        Text {
            id: titleText
            text: "☀️ 今日学习计划确认"
            color: "#E6EAF2"
            font.pixelSize: 22
            font.weight: Font.Bold
            anchors { top: parent.top; topMargin: 24; horizontalCenter: parent.horizontalCenter }
        }

        // 2. 中间数据看板区域
        Column {
            id: contentColumn
            anchors { top: titleText.bottom; topMargin: 32; left: parent.left; right: parent.right; margins: 32 }
            spacing: 20

            Row {
                spacing: 16
                Text { text: "大脑容量状态:"; color: "#94A3B8"; font.pixelSize: 14 }
                Text { text: "80% (健康)"; color: "#4ADE80"; font.weight: Font.Bold }
            }

            Text { text: "📅 今日排程概览："; color: "#E6EAF2"; font.weight: Font.DemiBold }

            // 警告区域
            Rectangle {
                width: parent.width; height: 60
                color: "#2D1F24"
                radius: 8
                border.color: "#991B1B"
                Text {
                    anchors.centerIn: parent
                    text: "⚠️ 警告：有 2 项低优先级任务因时间冲突无法自动排入今日。"; color: "#F87171"
                }
            }
        }

        // 3. 底部三个动作按钮
        Row {
            anchors { bottom: parent.bottom; bottomMargin: 24; horizontalCenter: parent.horizontalCenter }
            spacing: 16

            Button {
                text: "推迟冲突任务"
            }
            Button {
                text: "调整重点"
                onClicked: root.visible = false
            }
            Button {
                text: "🔥 接受并开启今日"
                highlighted: true
                onClicked: root.visible = false
            }
        }
    }
}