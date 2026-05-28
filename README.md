# Chrona

Chrona 是一个基于 Qt 6 / Qt Quick / C++20 / SQLite 的桌面端 AI-native 学习时间调度系统。

它不是传统 Todo App，而是一个 Calendar-Centric 的自动学习规划工具：系统会根据任务优先级、截止时间、预计时长、课程/固定事件和用户可用时间，自动生成学习 Time Blocks，并在任务或日程变化后重新排程。

## 项目定位

Chrona 的核心目标是回答一个问题：

> 现在最应该学什么？应该安排在什么时候？

MVP 阶段重点模仿 Motion 的产品理念：

- 时间轴优先，而不是任务列表优先
- 任务自动进入日程
- 固定课程和事件作为不可用时间
- 高优先级和临近 DDL 的任务优先安排
- 用户拖拽后的时间块会成为新的调度约束
- 当前专注模块持续提示下一步行动

## 技术栈

- Qt 6.6+
- Qt Quick / QML
- C++20
- SQLite
- CMake

不使用 Electron、Web 技术栈或 Qt Widgets。

## 核心功能

- Day / Week 日历时间轴
- 自动生成学习 Time Blocks
- 任务优先级、DDL、预计时长和课程分类
- 固定课程/事件避让
- 拖拽移动时间块并进行冲突校验
- 当前专注面板与番茄钟式沉浸专注界面
- 学习容量分析：今日已排、本周负载、容量健康状态
- Quick Add 自然语言添加任务
- OCR 截图识别接口预留
- 中文 / 英文语言切换入口
- AIService 抽象与 DeepSeek API Provider，可在本地规则和大模型解析之间解耦切换

## 架构

```text
QML UI Layer
  -> C++ App Services / ViewModels
  -> Scheduling Engine
  -> Task / Calendar / TimeBlock Models
  -> SQLite Persistence
```

主要目录：

```text
src/
  app/              应用服务、ViewModel、语言服务
  ai/               AIService 抽象与后续模型接入预留
  core/
    models/         Task、CalendarEvent、TimeBlock 等核心模型
    scheduler/      自动调度引擎
  database/         SQLite 数据库、迁移与 Repository
  managers/         NLP 快速解析、OCR 预览、课程设计兼容管理器
  ui/qml/           Qt Quick 页面、面板与组件
  i18n/             Qt Linguist 翻译文件
```

## 调度策略

当前使用轻量 greedy scheduling：

1. 读取未完成任务、课程/固定事件、用户锁定 TimeBlock。
2. 将课程和固定事件转换为不可用时间。
3. 按优先级、DDL 紧迫度、任务时长压力计算任务分数。
4. 将任务拆成 30 到 90 分钟左右的学习块。
5. 在 deadline 前寻找最早可用时间插入。
6. 若容量不足，则记录为未调度任务。
7. 当新增任务、修改任务、拖拽时间块或新增固定事件时自动重新排程。

默认策略：

- 调度范围：今天起 7 天
- 工作时间：08:00 - 23:00
- 最小时间块：30 分钟
- 推荐时间块：90 分钟
- 固定事件永远优先避让
- 用户拖拽后的块会保留为用户约束

## 构建环境

需要安装：

- Qt 6.6 或更高版本，包含 `Quick`、`Sql`、`Concurrent`、`LinguistTools`
- CMake 3.21+
- Ninja 或其他 CMake 支持的生成器
- MinGW 或 MSVC C++ 编译器

Windows + Qt MinGW 示例：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.6.3/mingw_64
cmake --build build
```

如果使用 Qt Creator，也可以直接打开 `CMakeLists.txt`，选择 Qt 6.6+ Kit 后构建运行。

## 运行

构建完成后运行生成的 `Chrona.exe`。

首次启动会自动创建本地 SQLite 数据库，并插入示例任务和课程事件。数据库位于 Qt 的应用数据目录中，不会提交到仓库。

## 当前状态

Chrona 目前处于 MVP 原型阶段，已经具备可运行的时间轴、任务系统、自动排程、固定时间、任务编辑、专注模式、自然语言快速添加和 DeepSeek 草稿解析接口。

## 本次迭代更新

本次迭代继续围绕 Motion 风格的 Calendar-Centric 调度体验推进，重点不是增加普通 CRUD，而是让任务、时间块和 AI 解析形成完整闭环：

- 接入 `AIService` 解耦层与 `DeepSeekProvider`，Quick Add 会先生成可确认的任务草稿，再写入本地模型和 Scheduler。
- 任务详情面板支持直接编辑当前 Time Block 的日期、起始时间和终止时间，不再让用户间接调整预计时长。
- 新增深色自定义时间滚轮组件，统一用于任务详情和固定时间弹窗，避免系统原生白底控件破坏整体风格。
- 固定课程/事件支持通过统一弹窗设置起止时间、分类和锁定状态，并继续作为 Scheduler 的避让约束。
- 保持 Qt Quick / QML + C++20 + SQLite 分层架构，AI 只负责解析建议，不直接写数据库，符合项目“自动学习时间调度系统”的核心规范。

后续计划：

- 补充 Scheduler 单元测试
- 完善固定事件编辑/删除
- 完善英文翻译
- 增加用户可配置的工作时间和调度偏好
- 接入真实 OCR 或系统截图解析
- 扩展 DeepSeek 的调度建议能力，例如解释为什么这样安排、容量不足时给出调整方案

## 说明

本项目优先服务课程设计和桌面端产品原型展示，因此强调：

- 面向对象设计
- 清晰分层架构
- 本地 SQLite 持久化
- Qt Quick 现代桌面 UI
- 自动学习时间调度作为核心体验
