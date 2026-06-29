# Chrona

Chrona 是一个基于 **Qt 6 / Qt Quick / C++20 / SQLite / DeepSeek API** 的 AI-native 学习时间调度桌面应用。

它不是传统 Todo List，而是一个以时间轴为核心的个人学习助手：用户输入任务、课程、考试、偏好或一句自然语言，Chrona 会把信息转换成可确认的时间块，并通过本地 C++ 调度器保证冲突、固定安排、优先级、容量和用户偏好都被正确处理。

## 项目定位

Chrona 想回答的问题是：

> 我今天应该做什么？什么时候做？如果计划冲突了，系统能不能帮我自动整理？

项目已经从“大学生学习规划助手”升级为“有记忆的个人学习助手”。核心原则如下：

- **Calendar First**：时间轴比任务列表更重要，任务最终都落到日程中。
- **AI 负责理解，本地负责正确性**：DeepSeek 负责自然语言理解和建议生成，最终落库、冲突检查、移动和拆分由 C++ 调度器完成。
- **用户确认后才写入数据**：AI 只生成草稿和调整方案，用户确认后才修改 SQLite。
- **时间块是核心交互对象**：拖动、拉伸、固定、拆分、级联避让都围绕 TimeBlock 展开。
- **记忆驱动个性化**：Chrona 保存用户偏好和学习模式，让后续规划更贴近个人习惯。

## 功能概览

### 1. 时间轴与时间块

- 支持日视图 / 周视图。
- 支持 24 小时时间范围显示。
- 时间块支持拖动。
- 时间块支持四向拉伸：上边缘调整开始时间，下边缘调整结束时间，左边缘向前扩展日期范围，右边缘向后扩展日期范围。
- 支持跨天连续时间块的视觉合并。
- 当前时间线显示在时间块上层，并带轻量粒子流光效果。
- 启动时视图以当前时间线为视觉锚点，而不是固定停在 0 点。
- 缩放行距时会尽量保持当前时间线附近的视觉位置。

### 2. 冲突处理与级联重排

- 向下拉伸撞到普通时间块时，后续时间块会自动向下顺延，保持原时长不变。
- 向上拉伸撞到普通时间块时，上方时间块会自动向上移动，保持原时长不变。
- 向左 / 向右延伸跨日期时，冲突时间块会寻找当天最近空档自动避让。
- 固定时间块、硬截止任务、已完成时间块不会被自动移动。
- 如果空间不足或遇到不可移动约束，整次操作会回滚，避免出现半保存状态。
- 数据库写入使用事务保证一致性。

### 3. AI Quick Add

- 支持一句话创建任务或时间块。
- 支持一句话拆出多个草稿，例如“后天下午吃晚饭，大后天早上赶飞机”。
- 多草稿以列表形式展示，用户可以单个确认、单个取消，也可以全部确认。
- AI 草稿会展示标题、时间、时长、优先级、分类、备注和解释。
- 草稿写入前会检查建议时段是否可用，如果冲突则尝试寻找附近空档。
- 如果未配置 DeepSeek Key，会回退到本地 NLP 规则解析。

### 4. DeepSeek 接入

- `DeepSeekProvider` 实现 `AIService` 抽象接口。
- 支持自然语言任务解析。
- 支持日程调整建议。
- 支持每日计划建议。
- 支持结合当前任务、时间块、固定状态、学习强度、截止类型和 AI 记忆生成上下文。
- API Key 只保存在本地设置或环境变量中，不提交到仓库。

### 5. 优先级、截止类型与固定块

- 任务优先级分为低 / 中 / 高。
- 高优先级任务自动使用红色 `#EF4444`，并带更醒目的边框和轻微发光效果。
- 用户切换为高优先级时，会自动打开“固定当前块”。
- 用户仍然可以手动关闭固定开关，保存时以用户当前开关状态为准。
- 硬截止任务会被视为更强约束，调度器不会随意移动。
- 固定时间块显示锁定状态，解锁后恢复拖动和拉伸。

### 6. AI 记忆库

- Chrona 会记录学习偏好、完成模式和用户手动输入的长期偏好。
- 用户可以在“AI 记忆”页面手动添加偏好，例如“晚上 10 点后不要再安排时间块”。
- 记忆保存在本地 SQLite 中。
- 后续 AI 规划会把记忆作为上下文参考。
- 用户可以随时清空记忆。

### 7. 数据洞察 Dashboard

- 新增“数据洞察”页面。
- 支持今日 / 本周统计。
- 展示学习时间、完成率、恢复间隔、AI 记忆数量。
- 提供学习 / 恢复 / 空闲时间分布图。
- 提供每日学习趋势条形图。
- 提供今日 AI 总结卡片：完成情况、未完成数量、明日建议。
- 提供高动态优先级任务列表。

### 8. DDL 提醒与首次登录

- 支持独立 DDL 提醒页面。
- 支持 DDL 新增、完成、归档和删除。
- 侧边栏展示紧急 DDL 数量。
- 支持首次登录引导和用户名保存。
- 登录状态、主题、DeepSeek Key 等设置保存在本地。

### 9. UI 与主题

- 现代 Qt Quick/QML 界面。
- 支持深色 / 浅色模式切换。
- 使用太阳 / 月亮图标表达主题切换。
- 左侧导航聚焦有效页面：AI 今日计划、时间轴、DDL 提醒、月历总览、数据洞察、AI 记忆。
- 时间块具备 handler、锁定图标、选中态、优先级视觉层级。
- 左下角“结束今日”按钮改为更克制的扁平玻璃风格。

## 技术栈

- C++20
- Qt 6.6+
- Qt Quick / QML
- Qt SQL / SQLite
- Qt Concurrent
- Qt Network
- CMake Presets
- DeepSeek API

## 目录结构

```text
Chrona/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── requirements.txt
├── src/
│   ├── ai/                  # AIService 抽象与 DeepSeek Provider
│   ├── app/                 # AppController、ScheduleService、QML Models
│   ├── core/
│   │   ├── models/          # Task、TimeBlock、CalendarEvent、DeadlineReminder 等领域模型
│   │   └── scheduler/       # 调度、容量、优先级、冲突级联处理
│   ├── database/            # SQLite 连接、迁移、Repository、备份
│   ├── managers/            # NLP、OCR、Scheduler 管理层
│   └── ui/qml/              # QML 页面、组件和面板
└── tests/                   # 数据库、NLP、调度器测试
```

## 环境要求

推荐环境：

- Windows 10 / Windows 11
- Qt 6.6.x MinGW 64-bit
- CMake 3.21+
- Ninja
- MinGW-w64 g++ with C++20 support

本机已验证过的组合：

- Qt: `C:\Qt\6.6.3\mingw_64`
- MSYS2 MinGW: `D:\msys2\mingw64`
- Build preset: `qt-mingw-debug`

## 构建与运行

### 1. 配置环境变量

PowerShell 示例：

```powershell
$env:PATH='C:\Qt\6.6.3\mingw_64\bin;D:\msys2\mingw64\bin;' + $env:PATH
```

### 2. 配置 CMake

```powershell
cmake --preset qt-mingw-debug
```

如需干净重配：

```powershell
cmake --fresh --preset qt-mingw-debug
```

### 3. 编译

```powershell
cmake --build --preset qt-mingw-debug
```

### 4. 运行

```powershell
.\build\qt-mingw-debug\Chrona.exe
```

## DeepSeek API 配置

Chrona 支持两种方式读取 DeepSeek Key：

### 方式一：环境变量

```powershell
$env:DEEPSEEK_API_KEY='sk-xxxxxxxxxxxxxxxx'
```

### 方式二：应用内保存

在应用内连接 Chrona AI 后，Key 会写入本地设置，不会提交到 GitHub。

## 数据库位置

应用数据使用 SQLite，本地数据库通常位于 Qt 标准应用数据目录下。数据库迁移由 `src/database/Migrations.cpp` 管理，当前 schema 版本为 `7`。

主要数据表包括：

- `tasks`
- `time_blocks`
- `calendar_events`
- `study_frames`
- `deadline_reminders`
- `ai_memories`
- `app_settings`

## 测试

项目包含 Qt Test 测试目标：

```powershell
cmake --build --preset qt-mingw-debug
ctest --test-dir build/qt-mingw-debug --output-on-failure
```

当前测试覆盖重点：

- 数据库迁移一致性。
- Repository 基础读写。
- NLP 自然语言解析。
- 调度器容量与冲突处理。

## Git 忽略规则

仓库不提交以下内容：

- `build/`
- Qt / CMake / Ninja 生成文件
- 本地数据库
- 日志
- `.env`
- DeepSeek API Key
- IDE 临时文件

## 最近更新

### v0.5.1 - 2026-06-29

本次更新合并了队友提交的首次登录、DDL 提醒能力，并保留当前主线的 AI 记忆库、数据洞察和时间轴重构。

- 新增 DDL 提醒页面与紧急 DDL 统计。
- 新增首次登录状态与用户名保存。
- 数据库迁移升级到 v7：v6 用于 DDL 提醒，v7 用于 AI 记忆库。
- 导航栏保留有效页面，移除无用的课程和当前专注入口。
- 完善 README，补充构建、运行、AI 配置、数据库和更新说明。

### v0.5.0 - 2026-06-29

- 新增 AI 记忆库页面。
- 新增数据洞察 Dashboard。
- 新增每日 AI 总结卡片。
- 支持一句话生成多个时间块草稿。
- 支持单个确认、单个取消和全部确认。
- 支持用户手动添加长期偏好。
- 高优先级时间块自动红色高亮，并默认固定。
- 修复红色只能用于高优先级的规则。
- 修复拆分时间块后删除一个小块导致整组消失的问题。
- 修复 AI 建议执行后未真正写入日程的问题。
- 修复空白点击后右侧详情面板遮挡布局的问题。
- 完善时间块冲突级联移动与回滚逻辑。

### v0.4.0

- 重构时间轴 UI。
- 增加深浅色主题。
- 增加四向 handler。
- 增加当前时间线流光粒子效果。
- 增加行距、列距调节。
- 增加启动时以当前时间线为基准定位。

## 注意事项

- AI 生成的内容不会直接写入数据库，必须用户确认。
- 固定时间块和硬截止任务会作为强约束处理。
- 高优先级任务默认固定，但用户可以手动取消固定。
- 如果调度器无法安全调整，会拒绝本次操作并保留原数据。
- DeepSeek Key 不要写入 README、代码或提交历史。