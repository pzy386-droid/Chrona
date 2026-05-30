import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    anchors.fill: parent
    color: "#B00A0E17" // 半透明深色背景，拉满仪式感
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

        // 2. 中间数据看板区域 (今日固定事件、自动安排、容量健康)
        Column {
            id: contentColumn
            anchors { top: titleText.bottom; topMargin: 32; left: parent.left; right: parent.right; margins: 32 }
            spacing: 20

            // 容量健康度进度条 (可以复用你们已有的进度条组件样式)
            Row {
                spacing: 16
                Text { text: "大脑容量状态:"; color: "#94A3B8"; font.pixelSize: 14 }
                // 假设后端传过来的今日容量是 scheduleService.todayCapacity
                Text { text: "80% (健康)"; color: "#4ADE80"; font.weight: Font.Bold }
            }

            // 今日待办简要列表 view
            Text { text: "📅 今日排程概览："; color: "#E6EAF2"; font.weight: Font.DemiBold }

            // 无法安排的任务警告 (如果有的话)
            Rectangle {
                width: parent.width; height: 60
                color: "#2D1F24" // 淡淡的暗红色警告背景
                radius: 8
                border.color: "#991B1B"
                visible: true // 根据后端是否有未排入任务决定
                Text {
                    anchors.centerIn: parent
                    text: "⚠️ 警告：有 2 项低优先级任务因时间冲突无法自动排入今日。"; color: "#F87171"
                }
            }
        }

        // 3. 底部三个动作按钮 (接受计划、调整重点、推迟低优先级)
        Row {
            anchors { bottom: parent.bottom; bottomMargin: 24; horizontalCenter: parent.horizontalCenter }
            spacing: 16

            Button {
                text: "推迟冲突任务"
                // onClicked: scheduleService.postponeUnscheduledTasks()
            }
            Button {
                text: "调整重点"
                onClicked: root.visible = false // 或者去呼出详情页
            }
            Button {
                text: "🔥 接受并开启今日"
                highlighted: true // 让这个按钮更显眼
                onClicked: {
                    // 调用后端盖章确认接口
                    root.visible = false
                }
            }
        }
    }
}