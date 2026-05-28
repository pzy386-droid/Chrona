# Chrona 产品迭代日志

## v0.2.0 - AI Scheduling Iteration

本次迭代将 Chrona 从基础学习时间轴原型，推进为具备 AI 任务解析、可确认调度草稿和更完整 Time Block 编辑能力的智能学习调度系统。

Chrona 的核心体验继续保持 Calendar-Centric：用户不需要围绕一个巨大任务列表工作，而是通过自然语言、固定时间和时间块交互，让系统自动生成并维护学习日程。

### 核心升级

- 引入 `AIService` 解耦层，为 DeepSeek、规则解析和后续更多模型能力提供统一接口。
- 新增 `DeepSeekProvider`，支持通过 DeepSeek API 将自然语言输入解析为任务草稿。
- Quick Add 从“直接创建任务”升级为“生成草稿 -> 用户确认 -> 写入调度系统”的产品流程。
- 任务详情面板支持直接编辑当前 Time Block 的日期、起始时间和终止时间。
- 新增自定义深色时间滚轮组件，统一用于任务详情和固定时间弹窗。
- 固定时间弹窗支持设置起止时间、分类和锁定状态，并作为 Scheduler 的避让约束。
- Timeline 选中逻辑升级，点击具体时间块后可精确编辑对应 Time Block。
- README 补充本次迭代说明，明确 AI 只负责解析和建议，不直接写数据库。

### 产品价值

- 用户可以用更自然的方式输入学习任务，例如“周五晚上复习英语考试”。
- 系统先生成可确认的任务草稿，降低 AI 误写入的风险。
- 用户可以直接调整某个时间块的起止时间，而不是通过预计时长间接影响排程。
- 固定课程、实验课和考试安排可以更清晰地成为 Scheduler 的硬约束。
- UI 继续向 Motion / Linear / Arc 风格靠近，减少系统原生控件带来的割裂感。

### 架构规范

- 保持 Qt 6 / Qt Quick / QML / C++20 / SQLite 技术栈。
- 保持 `QML UI -> App Services -> Scheduling Engine -> Models -> SQLite` 分层。
- AI 能力通过 `AIService` 独立接入，不污染 Scheduler 和数据库写入逻辑。
- Scheduler 仍然是产品核心，AI 只提供输入解析和后续建议能力。

### 验证

- QML 结构检查通过。
- `cmake --build --preset qt-mingw-debug` 构建通过。

### 下一步方向

- 为 Scheduler 补充单元测试，覆盖优先级、DDL、固定事件避让和容量不足。
- 将 AI 草稿确认弹窗继续打磨为更统一的 Motion 风格 UI。
- 增加 Scheduler 解释能力，让用户知道为什么任务被安排在某个时间。
- 完善固定事件的编辑和删除能力。
- 补齐英文翻译和更多本地化日期格式。
