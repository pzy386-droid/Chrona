# Chrona 开发计划

## 产品定位

Chrona 是一个 AI-native 学习时间调度桌面应用，核心不是待办清单，而是自动学习时间规划系统。

用户输入任务、截止时间、课程安排和可用时间后，系统自动生成学习时间块，并在日程变化后重新规划。

## MVP 范围

1. Calendar Timeline
   - Day / Week View
   - 时间块绝对定位
   - 当前时间线
   - 拖拽移动 Time Block

2. Task System
   - 标题
   - 备注
   - 截止时间
   - 预计时长
   - 优先级
   - 课程分类
   - 状态

3. Auto Scheduling Engine
   - 固定课程/事件避让
   - greedy scheduling
   - priority + deadline 评分
   - 长任务拆块
   - 容量不足时返回未调度任务

4. Dynamic Rescheduling
   - 新增任务后重排
   - 修改 DDL 后重排
   - 新增固定时间后重排
   - 拖拽 Time Block 后保留用户约束

5. Focus UI
   - 当前最应该做什么
   - 沉浸专注模式
   - 完成任务后自动刷新日程

6. Quick Add
   - 中文自然语言规则解析
   - 示例：明天下午两点写高数作业
   - OCR 识别入口预留

## 架构

```text
QML UI Layer
  -> C++ App Services / ViewModels
  -> Scheduling Engine
  -> Task / Calendar / TimeBlock Models
  -> SQLite Persistence
```

## 技术要求

- Qt 6.6+
- Qt Quick / QML
- C++20
- SQLite
- CMake
- 禁止 Electron / Web 技术栈
- 禁止 Qt Widgets

## UI 方向

参考 Motion、Linear、Arc Browser、Notion Calendar：

- 深色主题
- Calendar-Centric
- 大量留白
- 紧凑但不拥挤
- 流畅 hover、拖拽、面板过渡动画
- 右侧详情面板不打断时间轴
- Focus 模块回答“现在该做什么”

## 后续路线

1. Scheduler 单元测试
2. 固定事件完整编辑/删除
3. 工作时间和调度偏好设置
4. 英文翻译补齐
5. OCR 真实识别能力
6. DeepSeek API 接入
7. 根据 Motion 截图持续迭代 UI 密度、交互和动画
