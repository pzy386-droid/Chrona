# Chrona

Chrona 是一个基于 Qt 6 / Qt Quick / C++20 / SQLite 的桌面端 AI-native 学习时间调度系统。

它不是传统 Todo App，而是一个 Calendar-Centric 的自动学习规划工具：系统会根据任务优先级、截止时间、预计时长、课程、固定事件和用户可用时间，自动生成学习 Time Blocks，并在任务或日程变化后重新排程。

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

## 功能

- Day / Week 日历时间轴
- 自动任务调度
- 任务优先级、DDL、预计时长和课程分类
- 固定课程/事件避让
- DDL 提醒
- 拖拽移动时间块并进行冲突校验
- 当前专注面板与沉浸式专注界面
- 学习容量分析：今日已排、本周负载、容量健康状态
- Quick Add 自然语言添加任务
- OCR 截图识别接口预留
- 中文 / 英文语言切换入口
- AIService 抽象与 DeepSeek API Provider，可在本地规则和大模型解析之间切换

## 目录结构

主要目录：

```text
src/
  app/              应用服务、ViewModel、语言服务
  ai/               AIService 抽象与后续模型接入预留
  core/
    models/         Task、CalendarEvent、TimeBlock 等核心模型
    scheduler/      调度器、容量分析和冲突处理
  database/         SQLite 数据库、迁移与 Repository
  managers/         NLP 快速解析、OCR 预览、课程设计兼容管理器
  ui/qml/           Qt Quick 页面、面板与组件
  i18n/             Qt Linguist 翻译文件
tests/              数据库、NLP 和调度测试
```

## 调度策略

当前使用轻量 greedy scheduling：

1. 读取未完成任务、课程、固定事件、用户锁定 TimeBlock。
2. 将课程和固定事件转换为不可用时间。
3. 按优先级、DDL 紧迫度、任务时长压力计算任务分数。
4. 将任务拆成 30 到 90 分钟左右的学习块。
5. 在 deadline 前寻找最早可用时间插入。
6. 若容量不足，则记录为未调度任务。
7. 当新增任务、修改任务、拖拽时间块或新增固定事件时自动重新排程。

默认策略：

- 调度范围：从今天起 7 天
- 工作时间：08:00 - 23:00
- 最小时间块：30 分钟
- 推荐时间块：90 分钟

## 构建

需要安装：

- Qt 6.6 或更高版本，包含 `Quick`、`Sql`、`Concurrent`、`Network`、`LinguistTools`
- CMake 3.21+
- Ninja 或其他 CMake 支持的生成器

Windows + Qt MinGW 示例：

```powershell
cmake --preset qt-mingw-debug
cmake --build --preset qt-mingw-debug
```

如果使用 Qt Creator，也可以直接打开 `CMakeLists.txt`，选择 Qt 6.6+ Kit 后构建运行。

构建完成后运行生成的 `Chrona.exe`。

首次启动会自动创建本地 SQLite 数据库，并插入示例任务和课程事件。数据库位于 Qt 的应用数据目录中，不会提交到仓库。

## 当前状态

Chrona 目前处于 MVP 原型阶段，已经具备可运行的时间轴、任务系统、自动排程、固定时间、任务编辑、专注模式、自然语言快速添加和 DeepSeek 解析接口。

## 本次迭代更新

### v0.3.0 - AI 草稿解析与 Motion 风格时间块交互

距离上次 `v0.2.0`，本次迭代重点推进两个方向：真实 AI 解析链路，以及更接近 Motion 的时间块操作手感。

- Quick Add 已接入 `DeepSeekProvider`，自然语言会先生成可确认的 AI 草稿，再由用户确认写入数据库。
- AI 草稿支持直接生成 Time Block，例如“后天早课”“明天中午吃饭”会带着实际起止时间进入时间轴。
- DeepSeek 请求改为异步执行，等待时显示加载动画，避免主界面卡住。
- 时间轴支持 24 小时压缩视图，最小行距下以 2 小时间隔显示刻度。
- Time Block 选中后提供轻量 handle：上边缘调整开始时间，下边缘调整结束时间，右边缘调整连续天数。
- 连续跨天时间块上的一次 resize 会同步更新整组每天同一时段，避免合并块被拆散。
- 小高度时间块、handle、标题排版做了紧凑适配，避免文字溢出。
- AI 仍然只负责解析建议，不直接写数据库；最终落库和冲突校验仍由 C++ Scheduler / Repository 完成。

后续计划：

- 完善英文翻译
- 增加用户可配置的工作时间和调度偏好
- 接入真实 OCR 或系统截图解析
- 扩展 DeepSeek 的调度建议能力，例如解释为什么这样安排、容量不足时给出调整方案

## 说明

本项目优先服务课程设计和桌面端产品原型展示，因此强调：

- 面向对象设计
- 本地 SQLite 持久化
- Qt Quick 现代桌面 UI
- 自动学习时间调度作为核心体验
