#include "app/ScheduleService.h"

#include <QFuture>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QtGlobal>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <utility>

namespace {
QString colorForPriority(Priority priority, const QString& categoryColor)
{
    if (!categoryColor.isEmpty()) {
        return categoryColor;
    }
    switch (priority) {
    case Priority::High:
        return QStringLiteral("#FF7A59");
    case Priority::Medium:
        return QStringLiteral("#7C8CFF");
    case Priority::Low:
        return QStringLiteral("#7D8BA3");
    }
    return QStringLiteral("#7C8CFF");
}

QString blockSourceKey(BlockSource source)
{
    switch (source) {
    case BlockSource::Auto:
        return QStringLiteral("auto");
    case BlockSource::UserDragged:
        return QStringLiteral("manual");
    case BlockSource::Locked:
        return QStringLiteral("locked");
    }
    return QStringLiteral("auto");
}

QString eventSourceKey(EventType type)
{
    switch (type) {
    case EventType::Course:
        return QStringLiteral("course");
    case EventType::FixedEvent:
        return QStringLiteral("fixed");
    case EventType::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("fixed");
}

QString focusSubtitle(const TimelineItem& item)
{
    return item.start.time().toString(QStringLiteral("hh:mm")) + QStringLiteral(" - ") + item.end.time().toString(QStringLiteral("hh:mm"));
}

QString deadlineText(const QDateTime& value)
{
    return value.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QDateTime parseDateTimeText(const QString& input)
{
    QDateTime value = QDateTime::fromString(input.trimmed(), QStringLiteral("yyyy-MM-dd HH:mm"));
    if (!value.isValid()) {
        value = QDateTime::fromString(input.trimmed(), Qt::ISODate);
    }
    return value;
}

QTime parseClockTime(const QString& input)
{
    QTime value = QTime::fromString(input.trimmed(), QStringLiteral("HH:mm"));
    if (!value.isValid()) {
        value = QTime::fromString(input.trimmed(), QStringLiteral("H:mm"));
    }
    return value;
}

QVector<Task> subtractLockedBlockCapacity(QVector<Task> tasks, const QVector<TimeBlock>& lockedBlocks, QVector<int>* fullyCoveredTaskIds = nullptr)
{
    QHash<int, int> scheduledMinutesByTask;
    for (const auto& block : lockedBlocks) {
        if (block.taskId <= 0 || block.completedAt.isValid() || !block.start.isValid() || !block.end.isValid() || block.end <= block.start) {
            continue;
        }
        scheduledMinutesByTask[block.taskId] += static_cast<int>(block.start.secsTo(block.end) / 60);
    }

    for (auto& task : tasks) {
        const int alreadyScheduled = scheduledMinutesByTask.value(task.id, 0);
        if (alreadyScheduled > 0) {
            task.remainingMinutes = qMax(0, task.remainingMinutes - alreadyScheduled);
            if (task.remainingMinutes == 0 && fullyCoveredTaskIds) {
                fullyCoveredTaskIds->push_back(task.id);
            }
        }
    }
    return tasks;
}

void appendUniqueIds(QVector<int>& target, const QVector<int>& ids)
{
    QSet<int> existing;
    for (const int id : target) {
        existing.insert(id);
    }
    for (const int id : ids) {
        if (id > 0 && !existing.contains(id)) {
            target.push_back(id);
            existing.insert(id);
        }
    }
}

QVariantMap parsedDraftToMap(const ParsedTaskDraft& draft, const QString& source, const QString& message)
{
    return {
        {QStringLiteral("title"), draft.title},
        {QStringLiteral("notes"), draft.notes},
        {QStringLiteral("deadline"), deadlineText(draft.deadline)},
        {QStringLiteral("estimatedMinutes"), draft.estimatedMinutes},
        {QStringLiteral("priority"), static_cast<int>(draft.priority)},
        {QStringLiteral("categoryName"), draft.categoryName},
        {QStringLiteral("preferredStudyTime"), draft.preferredStudyTime},
        {QStringLiteral("hasTimeAnchor"), draft.hasTimeAnchor},
        {QStringLiteral("scheduledStart"), draft.scheduledStart.isValid() ? deadlineText(draft.scheduledStart) : QString()},
        {QStringLiteral("scheduledEnd"), draft.scheduledEnd.isValid() ? deadlineText(draft.scheduledEnd) : QString()},
        {QStringLiteral("source"), source},
        {QStringLiteral("explanation"), message}
    };
}

QString normalizedDeepSeekKey(const QString& input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    static const QRegularExpression keyPattern(QStringLiteral("(sk-[A-Za-z0-9_-]+)"));
    const QRegularExpressionMatch match = keyPattern.match(trimmed);
    return match.hasMatch() ? match.captured(1).trimmed() : trimmed;
}

bool containsCjk(const QString& text)
{
    for (const QChar ch : text) {
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) {
            return true;
        }
    }
    return false;
}

bool isAsciiOnly(const QString& text)
{
    for (const QChar ch : text) {
        if (ch.unicode() > 0x007F) {
            return false;
        }
    }
    return true;
}

bool aiDraftLooksUseful(const QString& input, const QVariantMap& draft)
{
    const QString title = draft.value(QStringLiteral("title")).toString().trimmed();
    const QString category = draft.value(QStringLiteral("categoryName")).toString().trimmed();
    const QString deadlineText = draft.value(QStringLiteral("deadline")).toString().trimmed();
    QDateTime deadline = QDateTime::fromString(deadlineText, QStringLiteral("yyyy-MM-dd HH:mm"));
    if (!deadline.isValid()) {
        deadline = QDateTime::fromString(deadlineText, Qt::ISODate);
    }
    if (title.isEmpty() || category.isEmpty() || !deadline.isValid()) {
        return false;
    }
    if (containsCjk(input) && isAsciiOnly(title + category)) {
        return false;
    }
    const QString normalizedTitle = title.toLower();
    const QString normalizedCategory = category.toLower();
    if (normalizedTitle == QStringLiteral("complete task") || normalizedTitle == QStringLiteral("presentation")
        || normalizedCategory == QStringLiteral("general") || normalizedCategory == QStringLiteral("work")) {
        return false;
    }
    return true;
}

bool containsReplacementMarker(const QString& text)
{
    return text.contains(QChar(0xFFFD)) || text.contains(QStringLiteral("??"));
}

QString repairedTitleFromInput(const QString& input)
{
    QString title = input.trimmed();
    title.remove(QRegularExpression(QStringLiteral("^(璇穦甯垜|甯垜瀹夋帓|瀹夋帓|鍒涘缓|鏂板缓|鍔犱竴涓獆娣诲姞)")));
    return title.trimmed().isEmpty() ? input.trimmed() : title.trimmed();
}
}

ScheduleService::ScheduleService(TaskRepository tasks, CalendarRepository calendar, DeadlineReminderRepository deadlines, TimeBlockRepository blocks,
                                 StudyFrameRepository studyFrames, SettingsRepository settings, BackupService backup,
                                 AIService* aiService, QObject* parent)
    : QObject(parent)
    , m_tasks(std::move(tasks))
    , m_calendar(std::move(calendar))
    , m_deadlines(std::move(deadlines))
    , m_blocks(std::move(blocks))
    , m_studyFrames(std::move(studyFrames))
    , m_settings(std::move(settings))
    , m_backup(std::move(backup))
    , m_aiService(aiService)
{
}

QObject* ScheduleService::timelineModel()
{
    return &m_timelineModel;
}

QObject* ScheduleService::taskModel()
{
    return &m_taskModel;
}

QVariantMap ScheduleService::focusItem() const
{
    return m_focusItem;
}

QVariantMap ScheduleService::capacityStats() const
{
    return m_capacityStats;
}

QVariantMap ScheduleService::aiStatus() const
{
    const QString provider = m_aiService ? m_aiService->providerName() : QStringLiteral("local");
    const bool configured = m_aiService && m_aiService->isConfigured();
    return {
        {QStringLiteral("provider"), provider},
        {QStringLiteral("configured"), configured},
        {QStringLiteral("mode"), configured ? QStringLiteral("ai") : QStringLiteral("local")},
        {QStringLiteral("label"), configured ? QObject::tr("DeepSeek 已连接") : QObject::tr("本地规则模式")},
        {QStringLiteral("hint"), configured ? QObject::tr("自然语言输入将优先由 DeepSeek 解析") : QObject::tr("点击 AI 状态连接 DeepSeek，未连接时使用本地规则")}
    };
}

QVariantMap ScheduleService::selectedTask() const
{
    std::optional<Task> task;
    for (const Task& candidate : m_tasks.allTasks()) {
        if (candidate.id == m_selectedTaskId) {
            task = candidate;
            break;
        }
    }
    if (!task) {
        return {};
    }
    QVariantMap map = taskToMap(*task);
    std::optional<TimelineItem> block;
    if (m_selectedBlockId > 0) {
        block = m_timelineModel.itemById(m_selectedBlockId);
    }
    if (!block) {
        block = m_timelineModel.firstBlockForTask(m_selectedTaskId);
    }
    if (block) {
        map.insert(QStringLiteral("blockId"), block->id);
        map.insert(QStringLiteral("blockDayIndex"), block->dayIndex);
        map.insert(QStringLiteral("blockEndDayIndex"), block->dayIndex + qMax(1, block->spanDays) - 1);
        map.insert(QStringLiteral("blockStartText"), block->start.time().toString(QStringLiteral("HH:mm")));
        map.insert(QStringLiteral("blockEndText"), block->end.time().toString(QStringLiteral("HH:mm")));
        map.insert(QStringLiteral("blockTimeRange"), focusSubtitle(*block));
        map.insert(QStringLiteral("isLocked"), block->locked);
        map.insert(QStringLiteral("kind"), block->kind);
        map.insert(QStringLiteral("source"), block->source);
        map.insert(QStringLiteral("lockActive"), block->locked || block->eventLocked);
        map.insert(QStringLiteral("canMove"), block->canMove);
        map.insert(QStringLiteral("canResize"), block->canResize);
        map.insert(QStringLiteral("canLock"), block->canLock);
        map.insert(QStringLiteral("blockOrdinal"), block->blockOrdinal);
        map.insert(QStringLiteral("blockTotal"), block->blockTotal);
        map.insert(QStringLiteral("completed"), block->completed);
    }
    return map;
}

QVariantMap ScheduleService::selectedDetail() const
{
    if (m_selectedBlockId != 0) {
        const auto item = m_timelineModel.itemById(m_selectedBlockId);
        if (item && item->event) {
            return eventToDetailMap(*item);
        }
    }

    QVariantMap task = selectedTask();
    if (!task.isEmpty()) {
        task.insert(QStringLiteral("kind"), QStringLiteral("task"));
    }
    return task;
}

int ScheduleService::unscheduledCount() const
{
    return m_unscheduledTaskIds.size();
}

int ScheduleService::urgentDeadlineCount() const
{
    int count = 0;
    const QDate today = QDate::currentDate();
    for (const DeadlineReminder& reminder : m_deadlines.reminders()) {
        if (reminder.status != DeadlineReminderStatus::Open || !reminder.dueAt.isValid()) {
            continue;
        }
        const int daysLeft = today.daysTo(reminder.dueAt.date());
        if (daysLeft <= reminder.remindDaysBefore) {
            ++count;
        }
    }
    return count;
}

QVariantList ScheduleService::deadlineReminders() const
{
    QVariantList list;
    for (const DeadlineReminder& reminder : m_deadlines.reminders()) {
        list.push_back(deadlineReminderToMap(reminder));
    }
    return list;
}

QVariantList ScheduleService::scheduleIssues() const
{
    return m_scheduleIssues;
}

bool ScheduleService::demoModeEnabled() const
{
    return m_settings.value(QStringLiteral("demoModeEnabled"), QStringLiteral("false")) == QStringLiteral("true");
}

QString ScheduleService::persistenceError() const
{
    return m_persistenceError;
}

QString ScheduleService::selectedDateText() const
{
    return m_selectedDate.toString(QStringLiteral("yyyy-MM-dd"));
}

int ScheduleService::normalizeManualBlockBackedTasks(const ScheduleWindow& window, const QVector<TimeBlock>& manualBlocks)
{
    Q_UNUSED(window);

    struct ManualSchedule {
        int minutes = 0;
        QDateTime latestEnd;
    };

    QHash<int, ManualSchedule> manualByTask;
    for (const auto& block : manualBlocks) {
        if (block.taskId <= 0 || block.source == BlockSource::Auto || block.completedAt.isValid()
            || !block.start.isValid() || !block.end.isValid() || block.end <= block.start) {
            continue;
        }
        auto& schedule = manualByTask[block.taskId];
        schedule.minutes += static_cast<int>(block.start.secsTo(block.end) / 60);
        if (!schedule.latestEnd.isValid() || block.end > schedule.latestEnd) {
            schedule.latestEnd = block.end;
        }
    }

    if (manualByTask.isEmpty()) {
        return 0;
    }

    int repairedCount = 0;
    const QVector<Task> tasks = m_tasks.activeTasks();
    for (auto task : tasks) {
        const ManualSchedule schedule = manualByTask.value(task.id);
        if (schedule.minutes <= 0) {
            continue;
        }

        const int normalizedMinutes = qMax(30, schedule.minutes);
        QDateTime normalizedDeadline = task.deadline;
        if (schedule.latestEnd.isValid() && (!normalizedDeadline.isValid() || schedule.latestEnd > normalizedDeadline)) {
            normalizedDeadline = schedule.latestEnd;
        }

        if (task.estimatedMinutes == normalizedMinutes
            && task.remainingMinutes == normalizedMinutes
            && task.deadline == normalizedDeadline) {
            continue;
        }

        task.estimatedMinutes = normalizedMinutes;
        task.estimatedHours = normalizedMinutes / 60.0;
        task.remainingMinutes = normalizedMinutes;
        task.deadline = normalizedDeadline;
        if (m_tasks.updateTask(task, task.categoryName)) {
            ++repairedCount;
        }
    }

    return repairedCount;
}

QVariantMap ScheduleService::reschedule()
{
    const ScheduleWindow window = currentWindow();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> locked = m_blocks.lockedBlocksBetween(window.start, window.end);
    const QVector<StudyFrame> frames = m_studyFrames.enabledFrames();
    normalizeManualBlockBackedTasks(window, locked);
    QVector<int> manuallyCoveredTaskIds;
    const QVector<Task> tasks = subtractLockedBlockCapacity(m_tasks.activeTasks(), locked, &manuallyCoveredTaskIds);

    const ScheduleResult result = m_scheduler.generateSchedule(tasks, events, locked, frames, window, m_config);
    QVector<int> scheduledTaskIds = result.scheduledTaskIds;
    appendUniqueIds(scheduledTaskIds, manuallyCoveredTaskIds);
    if (!m_blocks.commitSchedule(window, result.generatedBlocks, scheduledTaskIds,
                                 result.unscheduledTaskIds, QStringLiteral("manual_or_startup"))) {
        m_persistenceError = m_blocks.lastError();
        reload();
        return {{"ok", false}, {"message", QObject::tr("閲嶆柊瑙勫垝淇濆瓨澶辫触锛?1").arg(m_persistenceError)}};
    }
    m_persistenceError.clear();
    m_unscheduledTaskIds = result.unscheduledTaskIds;
    m_scheduleIssues.clear();
    for (const auto& issue : result.issues) {
        const QVariantMap issueMap = {
            {"taskId", issue.taskId},
            {"title", issue.title},
            {"code", issue.code},
            {"reason", issue.reason},
            {"fixHint", issue.fixHint},
            {"estimatedMinutes", issue.estimatedMinutes},
            {"scheduledMinutes", issue.scheduledMinutes},
            {"remainingMinutes", issue.remainingMinutes},
            {"deadlineText", deadlineText(issue.deadline)},
            {"priority", issue.priority},
            {"categoryName", issue.categoryName}
        };
        m_scheduleIssues.push_back(issueMap);
    }
    reload();
    return {
        {"ok", m_unscheduledTaskIds.isEmpty()},
        {"message", m_unscheduledTaskIds.isEmpty()
            ? QObject::tr("已重新规划，所有任务都已排入日程")
            : QObject::tr("已重新规划，仍有 %1 个任务需要调整").arg(m_unscheduledTaskIds.size())},
        {"unscheduledCount", m_unscheduledTaskIds.size()}
    };
}

void ScheduleService::selectTask(int taskId)
{
    m_selectedTaskId = taskId;
    const auto block = m_timelineModel.firstBlockForTask(taskId);
    m_selectedBlockId = block ? block->id : 0;
    m_taskModel.setSelectedTaskId(taskId);
    m_timelineModel.setSelectedTaskId(taskId);
    emit selectedTaskChanged();
}

void ScheduleService::selectTimelineItem(int taskId, int blockId)
{
    m_selectedTaskId = taskId;
    m_selectedBlockId = blockId;
    m_taskModel.setSelectedTaskId(taskId);
    m_timelineModel.setSelectedItemId(blockId);
    emit selectedTaskChanged();
}

void ScheduleService::clearSelection()
{
    m_selectedTaskId = 0;
    m_selectedBlockId = 0;
    m_taskModel.setSelectedTaskId(0);
    m_timelineModel.setSelectedTaskId(0);
    m_timelineModel.setSelectedItemId(0);
    emit selectedTaskChanged();
}

QVariantMap ScheduleService::startFocus()
{
    const int taskId = m_focusItem.value(QStringLiteral("taskId")).toInt();
    const int blockId = m_focusItem.value(QStringLiteral("blockId")).toInt();
    if (taskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("褰撳墠娌℃湁鍙紑濮嬬殑浠诲姟")}};
    }
    if (blockId > 0) {
        selectTimelineItem(taskId, blockId);
    } else {
        selectTask(taskId);
    }
    return {{"ok", true}, {"message", QObject::tr("已进入当前专注")}, {"taskId", taskId}, {"blockId", blockId}};
}

QVariantMap ScheduleService::stopFocus()
{
    return {{"ok", true}, {"message", QObject::tr("已取消专注")}};
}

void ScheduleService::completeTask(int taskId)
{
    if (m_tasks.completeTask(taskId)) {
        if (m_selectedTaskId == taskId) {
            m_selectedTaskId = 0;
        }
        reschedule();
    }
}

QVariantMap ScheduleService::completeBlock(int blockId)
{
    if (blockId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("当前没有可完成的时间块")}};
    }

    if (!m_blocks.completeBlock(blockId)) {
        return {{"ok", false}, {"message", QObject::tr("时间块完成失败")}};
    }

    const int selectedTaskId = m_selectedTaskId;
    m_selectedBlockId = blockId;
    reschedule();
    if (m_timelineModel.itemById(blockId)) {
        selectTimelineItem(selectedTaskId, blockId);
    }
    return {{"ok", true}, {"message", QObject::tr("宸插畬鎴愬綋鍓嶆椂闂村潡")}};
}

QVariantMap ScheduleService::updateTask(int taskId, const QString& title, const QString& notes, const QString& deadlineInput, int estimatedMinutes, int priority,
                                        const QString& categoryName, const QString& preferredStudyTime, bool autoScheduleEnabled, int deadlineType,
                                        int minChunkMinutes, int idealChunkMinutes, int effortLevel)
{
    const QString trimmedTitle = title.trimmed();
    if (taskId <= 0 || trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务")}};
    }

    QDateTime deadline = QDateTime::fromString(deadlineInput.trimmed(), QStringLiteral("yyyy-MM-dd HH:mm"));
    if (!deadline.isValid()) {
        deadline = QDateTime::fromString(deadlineInput.trimmed(), Qt::ISODate);
    }
    if (!deadline.isValid()) {
        return {{"ok", false}, {"message", QObject::tr("鎴鏃堕棿鏍煎紡搴斾负 yyyy-MM-dd HH:mm")}};
    }

    const ScheduleWindow scheduleWindow = currentWindow();
    const QVector<TimeBlock> currentBlocks = m_blocks.blocksBetween(scheduleWindow.start, scheduleWindow.end);
    for (const auto& block : currentBlocks) {
        if (block.taskId == taskId && block.end.isValid() && block.end > deadline) {
            deadline = block.end;
        }
    }

    Task task;
    if (const auto existing = m_taskModel.taskById(taskId)) {
        task = *existing;
    }
    task.id = taskId;
    task.title = trimmedTitle;
    task.notes = notes.trimmed();
    task.deadline = deadline;
    if (!task.startDate.isValid()) {
        task.startDate = QDateTime(QDate::currentDate(), QTime(0, 0));
    }
    task.deadlineType = static_cast<DeadlineType>(qBound(0, deadlineType, 1));
    task.autoScheduleEnabled = autoScheduleEnabled;
    task.estimatedMinutes = qMax(30, estimatedMinutes);
    task.estimatedHours = task.estimatedMinutes / 60.0;
    task.remainingMinutes = task.estimatedMinutes;
    task.minChunkMinutes = qBound(15, minChunkMinutes, 180);
    task.idealChunkMinutes = qMax(task.minChunkMinutes, qBound(30, idealChunkMinutes, 240));
    task.effortLevel = qBound(0, effortLevel, 2);
    task.priority = static_cast<Priority>(qBound(0, priority, 2));
    task.preferredStudyTime = preferredStudyTime.trimmed().isEmpty() ? QStringLiteral("evening") : preferredStudyTime.trimmed();

    if (!m_tasks.updateTask(task, categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("浠诲姟鏇存柊澶辫触锛?1").arg(m_tasks.lastError())}};
    }

    const int previousBlockId = m_selectedBlockId;
    reschedule();
    if (previousBlockId > 0 && m_timelineModel.itemById(previousBlockId)) {
        selectTimelineItem(taskId, previousBlockId);
    } else {
        selectTask(taskId);
    }
    return {{"ok", true}, {"message", QObject::tr("浠诲姟宸叉洿鏂板苟閲嶆柊鎺掔▼")}};
}

QVariantMap ScheduleService::deleteTask(int taskId)
{
    if (taskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务")}};
    }
    if (!m_tasks.deleteTask(taskId)) {
        return {{"ok", false}, {"message", QObject::tr("浠诲姟鍒犻櫎澶辫触")}};
    }
    if (m_selectedTaskId == taskId) {
        m_selectedTaskId = 0;
    }
    reschedule();
    return {{"ok", true}, {"message", QObject::tr("任务已删除")}};
}

QVariantMap ScheduleService::archiveTask(int taskId)
{
    if (taskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务")}};
    }
    if (!m_tasks.archiveTask(taskId)) {
        return {{"ok", false}, {"message", QObject::tr("任务归档失败")}};
    }
    if (m_selectedTaskId == taskId) {
        m_selectedTaskId = 0;
        m_selectedBlockId = 0;
    }
    reschedule();
    return {{"ok", true}, {"message", QObject::tr("任务已归档，历史日程仍会保留")}};
}

QVariantMap ScheduleService::setSelectedDate(const QString& dateText)
{
    const QDate date = QDate::fromString(dateText, QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid()) {
        return {{"ok", false}, {"message", QObject::tr("日期无效")}};
    }
    if (m_selectedDate == date) {
        return {{"ok", true}, {"dateText", selectedDateText()}};
    }
    m_selectedDate = date;
    m_selectedTaskId = 0;
    m_selectedBlockId = 0;
    reload();
    emit selectedDateChanged();
    return {{"ok", true}, {"dateText", selectedDateText()}};
}

QVariantList ScheduleService::monthOverview(int year, int month) const
{
    const QDate monthStart(year, month, 1);
    if (!monthStart.isValid()) {
        return {};
    }

    const QDate gridStart = monthStart.addDays(1 - monthStart.dayOfWeek());
    const QDate gridEnd = gridStart.addDays(42);
    const QDateTime rangeStart(gridStart, QTime(0, 0));
    const QDateTime rangeEnd(gridEnd, QTime(0, 0));
    const QVector<Task> tasks = m_tasks.allTasks();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(rangeStart, rangeEnd);
    const QVector<TimeBlock> blocks = m_blocks.blocksBetween(rangeStart, rangeEnd);

    QHash<int, Task> taskById;
    for (const Task& task : tasks) {
        taskById.insert(task.id, task);
    }

    QVariantList days;
    days.reserve(42);
    for (int offset = 0; offset < 42; ++offset) {
        const QDate date = gridStart.addDays(offset);
        const QDateTime dayStart(date, QTime(0, 0));
        const QDateTime dayEnd(date.addDays(1), QTime(0, 0));
        int plannedMinutes = 0;
        int completedMinutes = 0;
        QVariantList entries;

        for (const TimeBlock& block : blocks) {
            if (block.end <= dayStart || block.start >= dayEnd || !taskById.contains(block.taskId)) {
                continue;
            }
            const QDateTime overlapStart = qMax(block.start, dayStart);
            const QDateTime overlapEnd = qMin(block.end, dayEnd);
            const int minutes = qMax(0, static_cast<int>(overlapStart.secsTo(overlapEnd) / 60));
            plannedMinutes += minutes;
            if (block.completedAt.isValid()) {
                completedMinutes += minutes;
            }
            const Task task = taskById.value(block.taskId);
            entries.push_back(QVariantMap {
                {QStringLiteral("id"), block.id},
                {QStringLiteral("taskId"), block.taskId},
                {QStringLiteral("title"), task.title},
                {QStringLiteral("timeText"), block.start.time().toString(QStringLiteral("HH:mm"))},
                {QStringLiteral("color"), colorForPriority(task.priority, task.categoryColor)},
                {QStringLiteral("kind"), QStringLiteral("task")},
                {QStringLiteral("completed"), block.completedAt.isValid()}
            });
        }

        for (const CalendarEvent& event : events) {
            if (event.end <= dayStart || event.start >= dayEnd) {
                continue;
            }
            entries.push_back(QVariantMap {
                {QStringLiteral("id"), -event.id},
                {QStringLiteral("taskId"), 0},
                {QStringLiteral("title"), event.title},
                {QStringLiteral("timeText"), event.start.time().toString(QStringLiteral("HH:mm"))},
                {QStringLiteral("color"), event.categoryColor.isEmpty() ? QStringLiteral("#3F4658") : event.categoryColor},
                {QStringLiteral("kind"), QStringLiteral("event")},
                {QStringLiteral("completed"), false}
            });
        }

        std::sort(entries.begin(), entries.end(), [](const QVariant& left, const QVariant& right) {
            const QVariantMap a = left.toMap();
            const QVariantMap b = right.toMap();
            if (a.value(QStringLiteral("kind")) != b.value(QStringLiteral("kind"))) {
                return a.value(QStringLiteral("kind")).toString() == QStringLiteral("task");
            }
            if (a.value(QStringLiteral("completed")) != b.value(QStringLiteral("completed"))) {
                return !a.value(QStringLiteral("completed")).toBool();
            }
            return a.value(QStringLiteral("timeText")).toString() < b.value(QStringLiteral("timeText")).toString();
        });

        days.push_back(QVariantMap {
            {QStringLiteral("dateText"), date.toString(QStringLiteral("yyyy-MM-dd"))},
            {QStringLiteral("day"), date.day()},
            {QStringLiteral("inMonth"), date.month() == month},
            {QStringLiteral("isToday"), date == QDate::currentDate()},
            {QStringLiteral("isSelected"), date == m_selectedDate},
            {QStringLiteral("plannedMinutes"), plannedMinutes},
            {QStringLiteral("completedMinutes"), completedMinutes},
            {QStringLiteral("completionRate"), plannedMinutes > 0 ? qRound(completedMinutes * 100.0 / plannedMinutes) : 0},
            {QStringLiteral("entries"), entries},
            {QStringLiteral("moreCount"), qMax(0, entries.size() - 3)}
        });
    }
    return days;
}

QVariantMap ScheduleService::updateCategoryColor(const QString& categoryName, const QString& color)
{
    if (!m_tasks.updateCategoryColor(categoryName, color)) {
        return {
            {"ok", false},
            {"message", QObject::tr("鍒嗙被棰滆壊鏇存柊澶辫触锛?1").arg(m_tasks.lastError())}
        };
    }
    reload();
    return {{"ok", true}, {"message", QObject::tr("分类颜色已更新")}};
}

QVariantMap ScheduleService::addFixedEvent(const QString& title, int dayOffset, int startMinute, int durationMinutes, bool locked, const QString& categoryName)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入固定时间标题")}};
    }
    if (dayOffset < 0 || dayOffset > 6 || startMinute < 0 || startMinute >= 24 * 60 || durationMinutes <= 0 || startMinute + durationMinutes > 24 * 60) {
        return {{"ok", false}, {"message", QObject::tr("鍥哄畾鏃堕棿鑼冨洿鏃犳晥")}};
    }

    const ScheduleWindow window = currentWindow();
    CalendarEvent event;
    event.title = trimmedTitle;
    event.start = QDateTime(window.start.date().addDays(dayOffset), QTime(0, 0).addSecs(startMinute * 60));
    event.end = event.start.addSecs(durationMinutes * 60);
    event.type = EventType::Course;
    event.locked = locked;

    if (!m_conflicts.canPlace({event.start, event.end}, m_calendar.eventsBetween(window.start, window.end), m_blocks.blocksBetween(window.start, window.end))) {
        return {{"ok", false}, {"message", QObject::tr("这个时间段已有安排")}};
    }
    if (!m_calendar.createEvent(event, categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("鍥哄畾鏃堕棿鍒涘缓澶辫触锛?1").arg(m_calendar.lastError())}};
    }

    reschedule();
    return {{"ok", true}, {"message", QObject::tr("固定时间已加入日程")}};
}

QVariantMap ScheduleService::addDeadlineReminder(const QString& title, const QString& dueText, const QString& notes, const QString& categoryName, int remindDaysBefore)
{
    const QString trimmedTitle = title.trimmed();
    QDateTime dueAt = parseDateTimeText(dueText);
    if (!dueAt.isValid()) {
        const QDate dueDate = QDate::fromString(dueText.trimmed(), QStringLiteral("yyyy-MM-dd"));
        if (dueDate.isValid()) {
            dueAt = QDateTime(dueDate, QTime(23, 59));
        }
    }
    if (trimmedTitle.isEmpty() || !dueAt.isValid()) {
        return {{"ok", false}, {"message", QObject::tr("请输入标题和有效截止日期")}};
    }

    DeadlineReminder reminder;
    reminder.title = trimmedTitle;
    reminder.notes = notes.trimmed();
    reminder.dueAt = dueAt;
    reminder.categoryName = categoryName.trimmed().isEmpty() ? QStringLiteral("DDL") : categoryName.trimmed();
    reminder.remindDaysBefore = qBound(0, remindDaysBefore, 365);
    const int reminderId = m_deadlines.createReminder(reminder);
    if (reminderId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("DDL 提醒创建失败：%1").arg(m_deadlines.lastError())}};
    }
    emit dataChanged();
    return {{"ok", true}, {"message", QObject::tr("DDL 提醒已添加")}, {"id", reminderId}};
}

QVariantMap ScheduleService::updateDeadlineReminder(int reminderId, const QString& title, const QString& dueText, const QString& notes, const QString& categoryName, int remindDaysBefore)
{
    const QString trimmedTitle = title.trimmed();
    QDateTime dueAt = parseDateTimeText(dueText);
    if (!dueAt.isValid()) {
        const QDate dueDate = QDate::fromString(dueText.trimmed(), QStringLiteral("yyyy-MM-dd"));
        if (dueDate.isValid()) {
            dueAt = QDateTime(dueDate, QTime(23, 59));
        }
    }
    if (reminderId <= 0 || trimmedTitle.isEmpty() || !dueAt.isValid()) {
        return {{"ok", false}, {"message", QObject::tr("DDL 提醒信息不完整")}};
    }

    DeadlineReminder reminder;
    reminder.id = reminderId;
    reminder.title = trimmedTitle;
    reminder.notes = notes.trimmed();
    reminder.dueAt = dueAt;
    reminder.categoryName = categoryName.trimmed().isEmpty() ? QStringLiteral("DDL") : categoryName.trimmed();
    reminder.remindDaysBefore = qBound(0, remindDaysBefore, 365);
    if (!m_deadlines.updateReminder(reminder)) {
        return {{"ok", false}, {"message", QObject::tr("DDL 提醒更新失败：%1").arg(m_deadlines.lastError())}};
    }
    emit dataChanged();
    return {{"ok", true}, {"message", QObject::tr("DDL 提醒已更新")}};
}

QVariantMap ScheduleService::completeDeadlineReminder(int reminderId)
{
    if (!m_deadlines.setStatus(reminderId, DeadlineReminderStatus::Done)) {
        return {{"ok", false}, {"message", QObject::tr("DDL 提醒完成失败")}};
    }
    emit dataChanged();
    return {{"ok", true}, {"message", QObject::tr("DDL 已标记完成")}};
}

QVariantMap ScheduleService::archiveDeadlineReminder(int reminderId)
{
    if (!m_deadlines.setStatus(reminderId, DeadlineReminderStatus::Archived)) {
        return {{"ok", false}, {"message", QObject::tr("DDL 提醒归档失败")}};
    }
    emit dataChanged();
    return {{"ok", true}, {"message", QObject::tr("DDL 已归档")}};
}

QVariantMap ScheduleService::deleteDeadlineReminder(int reminderId)
{
    if (!m_deadlines.deleteReminder(reminderId)) {
        return {{"ok", false}, {"message", QObject::tr("DDL 提醒删除失败")}};
    }
    emit dataChanged();
    return {{"ok", true}, {"message", QObject::tr("DDL 已删除")}};
}

bool ScheduleService::setCategoryColor(const QString& categoryName, const QString& color)
{
    return m_calendar.setCategoryColor(categoryName, color);
}

QVariantMap ScheduleService::updateSelectedEvent(const QString& title, int dayIndex, const QString& startText, const QString& endText, bool locked, const QString& categoryName)
{
    if (m_selectedBlockId >= 0) {
        return {{"ok", false}, {"message", QObject::tr("璇峰厛閫夋嫨涓€涓浐瀹氭椂闂村潡")}};
    }

    const QString trimmedTitle = title.trimmed();
    const QTime startTime = parseClockTime(startText);
    const QTime endTime = parseClockTime(endText);
    if (trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入标题")}};
    }
    if (dayIndex < 0 || dayIndex > 6 || !startTime.isValid() || !endTime.isValid() || endTime <= startTime) {
        return {{"ok", false}, {"message", QObject::tr("缁撴潫鏃堕棿蹇呴』鏅氫簬璧峰鏃堕棿")}};
    }

    const int eventId = qAbs(m_selectedBlockId);
    const ScheduleWindow window = currentWindow();
    CalendarEvent event;
    event.id = eventId;
    event.title = trimmedTitle;
    event.start = QDateTime(window.start.date().addDays(dayIndex), startTime);
    event.end = QDateTime(window.start.date().addDays(dayIndex), endTime);
    event.locked = locked;
    event.type = EventType::Course;

    QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    events.erase(std::remove_if(events.begin(), events.end(), [eventId](const CalendarEvent& current) {
        return current.id == eventId;
    }), events.end());

    if (!m_conflicts.canPlace({event.start, event.end}, events, m_blocks.blocksBetween(window.start, window.end))) {
        return {{"ok", false}, {"message", QObject::tr("这个时间段已有安排")}};
    }
    if (!m_calendar.updateEvent(event, categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("鍥哄畾鏃堕棿鏇存柊澶辫触锛?1").arg(m_calendar.lastError())}};
    }

    reschedule();
    m_selectedBlockId = -eventId;
    m_timelineModel.setSelectedItemId(m_selectedBlockId);
    return {{"ok", true}, {"message", QObject::tr("固定时间已更新")}};
}

QVariantMap ScheduleService::scheduleSelectedEventBlocks(const QString& title, int startDayIndex, int endDayIndex, const QString& startText, const QString& endText, bool locked, const QString& categoryName)
{
    if (m_selectedBlockId >= 0) {
        return {{"ok", false}, {"message", QObject::tr("璇峰厛閫夋嫨涓€涓浐瀹氭椂闂村潡")}};
    }

    const QString trimmedTitle = title.trimmed();
    const QTime startTime = parseClockTime(startText);
    const QTime endTime = parseClockTime(endText);
    if (trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入标题")}};
    }
    if (!startTime.isValid() || !endTime.isValid() || endTime <= startTime) {
        return {{"ok", false}, {"message", QObject::tr("缁撴潫鏃堕棿蹇呴』鏅氫簬璧峰鏃堕棿")}};
    }

    const int firstDay = qBound(0, qMin(startDayIndex, endDayIndex), 6);
    const int lastDay = qBound(0, qMax(startDayIndex, endDayIndex), 6);
    const int currentEventId = qAbs(m_selectedBlockId);
    const ScheduleWindow window = currentWindow();
    const QVector<CalendarEvent> allEvents = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> blocks = m_blocks.blocksBetween(window.start, window.end);

    CalendarEvent selectedEvent;
    bool selectedEventFound = false;
    for (const auto& event : allEvents) {
        if (event.id == currentEventId) {
            selectedEvent = event;
            selectedEventFound = true;
            break;
        }
    }
    if (!selectedEventFound) {
        return {{"ok", false}, {"message", QObject::tr("当前固定时间块不存在，请刷新后重试")}};
    }

    QSet<int> previousSeriesEventIds;
    const QString originalTitle = selectedEvent.title.trimmed();
    const QString originalCategory = selectedEvent.categoryName.trimmed();
    const QTime originalStartTime = selectedEvent.start.time();
    const QTime originalEndTime = selectedEvent.end.time();
    for (const auto& event : allEvents) {
        if (event.type != selectedEvent.type) {
            continue;
        }
        if (event.title.trimmed() != originalTitle) {
            continue;
        }
        if (event.start.time() != originalStartTime || event.end.time() != originalEndTime) {
            continue;
        }
        if (event.categoryName.trimmed().compare(originalCategory, Qt::CaseInsensitive) != 0) {
            continue;
        }
        previousSeriesEventIds.insert(event.id);
    }

    QSet<int> editableEventIds;
    editableEventIds.unite(previousSeriesEventIds);
    QHash<int, int> reusableEventByDay;
    int selectedReusableEventId = 0;
    for (const auto& event : allEvents) {
        if (event.id == currentEventId) {
            selectedReusableEventId = event.id;
            continue;
        }
        if (!previousSeriesEventIds.contains(event.id)) {
            continue;
        }
        editableEventIds.insert(event.id);
        const int dayIndex = static_cast<int>(window.start.date().daysTo(event.start.date()));
        if (dayIndex >= firstDay && dayIndex <= lastDay && !reusableEventByDay.contains(dayIndex)) {
            reusableEventByDay.insert(dayIndex, event.id);
        }
    }

    if (selectedReusableEventId > 0 && !reusableEventByDay.contains(firstDay)) {
        reusableEventByDay.insert(firstDay, selectedReusableEventId);
    }

    QVector<CalendarEvent> otherEvents = allEvents;
    otherEvents.erase(std::remove_if(otherEvents.begin(), otherEvents.end(), [&editableEventIds](const CalendarEvent& event) {
        return editableEventIds.contains(event.id);
    }), otherEvents.end());

    struct PlannedEvent {
        int existingId = 0;
        CalendarEvent event;
    };
    QVector<PlannedEvent> planned;
    planned.reserve(lastDay - firstDay + 1);
    for (int dayIndex = firstDay; dayIndex <= lastDay; ++dayIndex) {
        CalendarEvent event;
        event.id = reusableEventByDay.value(dayIndex, 0);
        event.title = trimmedTitle;
        event.start = QDateTime(window.start.date().addDays(dayIndex), startTime);
        event.end = QDateTime(window.start.date().addDays(dayIndex), endTime);
        event.type = EventType::Course;
        event.locked = locked;

        if (event.start < window.start || event.end > window.end || event.end <= event.start) {
            return {{"ok", false}, {"message", QObject::tr("鍥哄畾鏃堕棿瓒呭嚭褰撳墠鎺掔▼鑼冨洿")}};
        }
        if (!m_conflicts.canPlace({event.start, event.end}, otherEvents, blocks)) {
            return {{"ok", false}, {"message", QObject::tr("连续固定时间存在冲突，请调整日期或时间")}};
        }

        otherEvents.push_back(event);
        planned.push_back({event.id, event});
    }

    QVector<CalendarEvent> eventsToSave;
    eventsToSave.reserve(planned.size());
    for (const auto& plannedEvent : planned) {
        CalendarEvent event = plannedEvent.event;
        event.id = plannedEvent.existingId;
        eventsToSave.push_back(event);
    }
    QVector<int> savedEventIds;
    QSet<int> reusedEventIds;
    for (const CalendarEvent& event : eventsToSave) {
        if (event.id > 0) {
            reusedEventIds.insert(event.id);
        }
    }
    QVector<int> removedEventIds;
    for (int eventId : editableEventIds) {
        if (!reusedEventIds.contains(eventId)) {
            removedEventIds.push_back(eventId);
        }
    }
    if (!m_calendar.upsertEvents(eventsToSave, categoryName, removedEventIds, &savedEventIds)) {
        return {
            {"ok", false},
            {"message", QObject::tr("杩炵画鍥哄畾鏃堕棿淇濆瓨澶辫触锛?1").arg(m_calendar.lastError())}
        };
    }
    const int selectedEventId = savedEventIds.isEmpty() ? 0 : savedEventIds.first();

    reschedule();
    if (selectedEventId > 0) {
        m_selectedBlockId = -selectedEventId;
        m_timelineModel.setSelectedItemId(m_selectedBlockId);
        emit selectedTaskChanged();
    }

    return {{"ok", true}, {"message", QObject::tr("已安排 %1 天连续固定时间").arg(lastDay - firstDay + 1)}};
}

bool ScheduleService::moveBlock(int blockId, int dayIndex, int startMinute, int durationMinutes)
{
    if (blockId <= 0 || dayIndex < 0 || dayIndex > 6 || durationMinutes <= 0) {
        return false;
    }

    const ScheduleWindow window = currentWindow();
    if (startMinute < 0 || startMinute + durationMinutes > 24 * 60) {
        reload();
        return false;
    }

    const QDate targetDate = window.start.date().addDays(dayIndex);
    const QDateTime start(targetDate, QTime(0, 0).addSecs(startMinute * 60));
    const QDateTime end = start.addSecs(durationMinutes * 60);

    if (start < window.start || end > window.end || end <= start) {
        reload();
        return false;
    }

    QVector<TimeBlock> otherBlocks = m_blocks.blocksBetween(window.start, window.end);
    const bool blockExists = std::any_of(otherBlocks.begin(), otherBlocks.end(), [blockId](const TimeBlock& block) {
        return block.id == blockId;
    });
    if (!blockExists) {
        reload();
        return false;
    }
    otherBlocks.erase(std::remove_if(otherBlocks.begin(), otherBlocks.end(), [blockId](const TimeBlock& block) {
        return block.id == blockId;
    }), otherBlocks.end());

    if (!m_conflicts.canPlace({start, end}, m_calendar.eventsBetween(window.start, window.end), otherBlocks)) {
        reload();
        return false;
    }

    if (!m_blocks.moveBlock(blockId, start, end)) {
        reload();
        return false;
    }

    reschedule();
    return true;
}

bool ScheduleService::moveTimelineItem(int itemId, int dayIndex, int startMinute, int durationMinutes)
{
    if (itemId > 0) {
        return moveBlock(itemId, dayIndex, startMinute, durationMinutes);
    }

    const int eventId = qAbs(itemId);
    if (eventId <= 0 || dayIndex < 0 || dayIndex > 6 || durationMinutes <= 0) {
        return false;
    }

    const ScheduleWindow window = currentWindow();
    if (startMinute < 0 || startMinute + durationMinutes > 24 * 60) {
        reload();
        return false;
    }

    const QDate targetDate = window.start.date().addDays(dayIndex);
    const QDateTime start(targetDate, QTime(0, 0).addSecs(startMinute * 60));
    const QDateTime end = start.addSecs(durationMinutes * 60);

    QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    auto currentIt = std::find_if(events.begin(), events.end(), [eventId](const CalendarEvent& event) {
        return event.id == eventId;
    });
    if (currentIt == events.end() || currentIt->locked) {
        reload();
        return false;
    }
    events.erase(std::remove_if(events.begin(), events.end(), [eventId](const CalendarEvent& event) {
        return event.id == eventId;
    }), events.end());

    if (!m_conflicts.canPlace({start, end}, events, m_blocks.blocksBetween(window.start, window.end))) {
        reload();
        return false;
    }

    if (!m_calendar.moveEvent(eventId, start, end)) {
        reload();
        return false;
    }

    reschedule();
    return true;
}

QVariantMap ScheduleService::resizeTimelineItemTime(int itemId, int startMinute, int durationMinutes)
{
    const auto item = m_timelineModel.itemById(itemId);
    if (!item) {
        return {{"ok", false}, {"message", QObject::tr("璇峰厛閫夋嫨涓€涓椂闂村潡")}};
    }
    if (startMinute < 0 || durationMinutes <= 0 || startMinute + durationMinutes > 24 * 60) {
        return {{"ok", false}, {"message", QObject::tr("鏃堕棿鑼冨洿鏃犳晥")}};
    }

    const QTime start = QTime(0, 0).addSecs(startMinute * 60);
    const QTime end = QTime(0, 0).addSecs((startMinute + durationMinutes) * 60);
    const QString startText = start.toString(QStringLiteral("HH:mm"));
    const QString endText = end.toString(QStringLiteral("HH:mm"));
    const int firstDay = qBound(0, item->dayIndex, 6);
    const int lastDay = qBound(firstDay, item->dayIndex + qMax(1, item->spanDays) - 1, 6);

    if (itemId > 0) {
        m_selectedTaskId = item->taskId;
        m_selectedBlockId = itemId;
        return scheduleSelectedTaskBlocks(firstDay, lastDay, startText, endText, item->locked);
    }

    m_selectedTaskId = 0;
    m_selectedBlockId = itemId;
    return scheduleSelectedEventBlocks(item->title, firstDay, lastDay, startText, endText, item->eventLocked, item->subtitle);
}

QVariantMap ScheduleService::stretchTimelineItemDays(int itemId, int endDayIndex)
{
    const auto item = m_timelineModel.itemById(itemId);
    if (!item) {
        return {{"ok", false}, {"message", QObject::tr("璇峰厛閫夋嫨涓€涓椂闂村潡")}};
    }

    const int firstDay = qBound(0, item->dayIndex, 6);
    const int lastDay = qBound(firstDay, endDayIndex, 6);
    const QTime start = QTime(0, 0).addSecs(item->startMinute * 60);
    const QTime end = QTime(0, 0).addSecs((item->startMinute + item->durationMinutes) * 60);
    const QString startText = start.toString(QStringLiteral("HH:mm"));
    const QString endText = end.toString(QStringLiteral("HH:mm"));

    if (itemId > 0) {
        m_selectedTaskId = item->taskId;
        m_selectedBlockId = itemId;
        return scheduleSelectedTaskBlocks(firstDay, lastDay, startText, endText, item->locked);
    }

    m_selectedTaskId = 0;
    m_selectedBlockId = itemId;
    return scheduleSelectedEventBlocks(item->title, firstDay, lastDay, startText, endText, item->eventLocked, item->subtitle);
}

bool ScheduleService::setEventLocked(int eventId, bool locked)
{
    const int normalizedId = qAbs(eventId);
    if (normalizedId <= 0) {
        return false;
    }
    if (!m_calendar.setEventLocked(normalizedId, locked)) {
        return false;
    }
    reschedule();
    return true;
}

QVariantMap ScheduleService::setSelectedBlockLocked(bool locked)
{
    if (m_selectedBlockId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("璇峰厛閫夋嫨涓€涓涔犳椂闂村潡")}};
    }
    if (!m_blocks.setBlockSource(m_selectedBlockId, locked ? BlockSource::Locked : BlockSource::UserDragged)) {
        return {{"ok", false}, {"message", QObject::tr("时间块固定状态更新失败")}};
    }

    const int selectedTaskId = m_selectedTaskId;
    const int selectedBlockId = m_selectedBlockId;
    reschedule();
    if (m_timelineModel.itemById(selectedBlockId)) {
        selectTimelineItem(selectedTaskId, selectedBlockId);
    } else {
        selectTask(selectedTaskId);
    }
    return {{"ok", true}, {"message", locked ? QObject::tr("宸插浐瀹氬綋鍓嶆椂闂村潡") : QObject::tr("宸插彇娑堝浐瀹氾紝淇濈暀褰撳墠浣嶇疆")}};
}

QVariantMap ScheduleService::moveSelectedTaskBlock(int dayIndex, const QString& startText, const QString& endText)
{
    if (m_selectedTaskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务")}};
    }

    const QTime start = parseClockTime(startText);
    const QTime end = parseClockTime(endText);
    if (!start.isValid() || !end.isValid() || end <= start) {
        return {{"ok", false}, {"message", QObject::tr("缁撴潫鏃堕棿蹇呴』鏅氫簬璧峰鏃堕棿")}};
    }

    const int startMinute = start.hour() * 60 + start.minute();
    const int duration = static_cast<int>(start.secsTo(end) / 60);
    if (m_selectedBlockId <= 0) {
        const ScheduleWindow window = currentWindow();
        if (dayIndex < 0 || dayIndex > 6) {
            return {{"ok", false}, {"message", QObject::tr("鏃ユ湡鑼冨洿鏃犳晥")}};
        }

        const QDate targetDate = window.start.date().addDays(dayIndex);
        const QDateTime blockStart(targetDate, start);
        const QDateTime blockEnd(targetDate, end);
        if (blockStart < window.start || blockEnd > window.end) {
            return {{"ok", false}, {"message", QObject::tr("时间块超出当前排程范围")}};
        }
        if (!m_conflicts.canPlace({blockStart, blockEnd}, m_calendar.eventsBetween(window.start, window.end), m_blocks.blocksBetween(window.start, window.end))) {
            return {{"ok", false}, {"message", QObject::tr("移动失败：该时间段可能存在冲突")}};
        }

        TimeBlock block;
        block.taskId = m_selectedTaskId;
        block.start = blockStart;
        block.end = blockEnd;
        block.source = BlockSource::UserDragged;
        const int createdBlockId = m_blocks.createBlock(block);
        if (createdBlockId <= 0) {
            return {{"ok", false}, {"message", QObject::tr("时间块创建失败")}};
        }

        const int selectedTaskId = m_selectedTaskId;
        m_selectedBlockId = createdBlockId;
        reschedule();
        selectTimelineItem(selectedTaskId, createdBlockId);
        return {{"ok", true}, {"message", QObject::tr("宸插垱寤哄綋鍓嶆椂闂村潡")}};
    }

    if (!moveBlock(m_selectedBlockId, dayIndex, startMinute, duration)) {
        return {{"ok", false}, {"message", QObject::tr("移动失败：该时间段可能存在冲突")}};
    }

    return {{"ok", true}, {"message", QObject::tr("时间块已移动并重新排程")}};
}

QVariantMap ScheduleService::scheduleSelectedTaskBlocks(int startDayIndex, int endDayIndex, const QString& startText, const QString& endText, bool locked)
{
    if (m_selectedTaskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务")}};
    }

    const int firstDay = qBound(0, qMin(startDayIndex, endDayIndex), 6);
    const int lastDay = qBound(0, qMax(startDayIndex, endDayIndex), 6);
    const QTime start = parseClockTime(startText);
    const QTime end = parseClockTime(endText);
    if (!start.isValid() || !end.isValid() || end <= start) {
        return {{"ok", false}, {"message", QObject::tr("缁撴潫鏃堕棿蹇呴』鏅氫簬璧峰鏃堕棿")}};
    }

    const ScheduleWindow window = currentWindow();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> allBlocks = m_blocks.blocksBetween(window.start, window.end);

    TimeBlock selectedBlock;
    bool selectedBlockFound = false;
    if (m_selectedBlockId > 0) {
        for (const auto& block : allBlocks) {
            if (block.id == m_selectedBlockId) {
                selectedBlock = block;
                selectedBlockFound = true;
                break;
            }
        }
    }

    QSet<int> previousSeriesBlockIds;
    if (selectedBlockFound) {
        previousSeriesBlockIds.insert(selectedBlock.id);
        const QTime originalStartTime = selectedBlock.start.time();
        const QTime originalEndTime = selectedBlock.end.time();
        for (const auto& block : allBlocks) {
            if (block.taskId != m_selectedTaskId) {
                continue;
            }
            if (block.id != selectedBlock.id && block.source == BlockSource::Auto) {
                continue;
            }
            if (block.start.time() != originalStartTime || block.end.time() != originalEndTime) {
                continue;
            }
            previousSeriesBlockIds.insert(block.id);
        }
    }

    QSet<int> editableBlockIds;
    editableBlockIds.unite(previousSeriesBlockIds);
    QHash<int, int> reusableBlockByDay;
    int selectedReusableBlockId = 0;
    for (const auto& block : allBlocks) {
        if (block.id == m_selectedBlockId) {
            selectedReusableBlockId = block.id;
            continue;
        }
        if (!previousSeriesBlockIds.contains(block.id)) {
            continue;
        }
        editableBlockIds.insert(block.id);
        const int dayIndex = static_cast<int>(window.start.date().daysTo(block.start.date()));
        if (dayIndex >= firstDay && dayIndex <= lastDay && !reusableBlockByDay.contains(dayIndex)) {
            reusableBlockByDay.insert(dayIndex, block.id);
        }
    }

    if (selectedReusableBlockId > 0 && !reusableBlockByDay.contains(firstDay)) {
        reusableBlockByDay.insert(firstDay, selectedReusableBlockId);
    }

    QVector<TimeBlock> otherBlocks = allBlocks;
    otherBlocks.erase(std::remove_if(otherBlocks.begin(), otherBlocks.end(), [&editableBlockIds](const TimeBlock& block) {
        return editableBlockIds.contains(block.id);
    }), otherBlocks.end());

    struct PlannedBlock {
        int existingId = 0;
        QDateTime start;
        QDateTime end;
    };
    QVector<PlannedBlock> planned;
    planned.reserve(lastDay - firstDay + 1);
    for (int dayIndex = firstDay; dayIndex <= lastDay; ++dayIndex) {
        const QDate targetDate = window.start.date().addDays(dayIndex);
        const QDateTime blockStart(targetDate, start);
        const QDateTime blockEnd(targetDate, end);
        if (blockStart < window.start || blockEnd > window.end || blockEnd <= blockStart) {
            return {{"ok", false}, {"message", QObject::tr("时间块超出当前排程范围")}};
        }
        if (!m_conflicts.canPlace({blockStart, blockEnd}, events, otherBlocks)) {
            return {{"ok", false}, {"message", QObject::tr("杩炵画鏃堕棿鍧楀瓨鍦ㄥ啿绐侊紝璇疯皟鏁存棩鏈熸垨鏃堕棿")}};
        }

        TimeBlock probe;
        probe.taskId = m_selectedTaskId;
        probe.start = blockStart;
        probe.end = blockEnd;
        probe.source = locked ? BlockSource::Locked : BlockSource::UserDragged;
        otherBlocks.push_back(probe);
        planned.push_back({reusableBlockByDay.value(dayIndex, 0), blockStart, blockEnd});
    }

    QVector<TimeBlock> blocksToSave;
    blocksToSave.reserve(planned.size());
    for (const auto& block : planned) {
        TimeBlock saved;
        saved.id = block.existingId;
        saved.taskId = m_selectedTaskId;
        saved.start = block.start;
        saved.end = block.end;
        saved.source = locked ? BlockSource::Locked : BlockSource::UserDragged;
        blocksToSave.push_back(saved);
    }
    QVector<int> savedBlockIds;
    QSet<int> reusedBlockIds;
    for (const TimeBlock& block : blocksToSave) {
        if (block.id > 0) {
            reusedBlockIds.insert(block.id);
        }
    }
    QVector<int> removedBlockIds;
    for (int blockId : editableBlockIds) {
        if (!reusedBlockIds.contains(blockId)) {
            removedBlockIds.push_back(blockId);
        }
    }
    if (!m_blocks.upsertBlocks(blocksToSave, removedBlockIds, &savedBlockIds)) {
        return {
            {"ok", false},
            {"message", QObject::tr("杩炵画鏃堕棿鍧椾繚瀛樺け璐ワ細%1").arg(m_blocks.lastError())}
        };
    }
    const int selectedBlockId = savedBlockIds.isEmpty() ? 0 : savedBlockIds.first();
    const int selectedTaskId = m_selectedTaskId;
    reschedule();
    if (selectedBlockId > 0 && m_timelineModel.itemById(selectedBlockId)) {
        selectTimelineItem(selectedTaskId, selectedBlockId);
    } else {
        selectTask(selectedTaskId);
    }

    return {{"ok", true}, {"message", QObject::tr("已安排 %1 天连续时间块").arg(lastDay - firstDay + 1)}};
}

QVariantMap ScheduleService::previewTaskDraft(const QString& input)
{
    if (input.trimmed().isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入任务内容")}};
    }

    QString aiFallbackMessage;
    if (m_aiService && m_aiService->isConfigured()) {
        QFuture<AIParseResult> future = m_aiService->parseNaturalLanguageTask(input);
        future.waitForFinished();
        const AIParseResult aiResult = future.result();
        if (aiResult.supported && !aiResult.taskDraft.isEmpty()) {
            QVariantMap draft = aiResult.taskDraft;
            draft.insert(QStringLiteral("source"), aiResult.provider.isEmpty() ? QStringLiteral("ai") : aiResult.provider);

            const ParsedTaskDraft localHints = m_parser.parse(input);
            const QString planningMode = draft.value(QStringLiteral("planningMode")).toString();
            if (planningMode == QStringLiteral("direct_time_block")) {
                const QString title = draft.value(QStringLiteral("title")).toString().trimmed();
                if (title.isEmpty() || containsReplacementMarker(title) || (containsCjk(input) && isAsciiOnly(title))) {
                    draft.insert(QStringLiteral("title"), repairedTitleFromInput(input));
                }
                const QString category = draft.value(QStringLiteral("categoryName")).toString().trimmed();
                if (localHints.valid && (category.isEmpty() || containsReplacementMarker(category) || isAsciiOnly(category))) {
                    draft.insert(QStringLiteral("categoryName"), localHints.categoryName);
                }
                const QString scheduledEnd = draft.value(QStringLiteral("scheduledEnd")).toString().trimmed();
                if (!scheduledEnd.isEmpty() && draft.value(QStringLiteral("deadline")).toString().trimmed().isEmpty()) {
                    draft.insert(QStringLiteral("deadline"), scheduledEnd);
                }
            }

            if (localHints.valid && localHints.hasTimeAnchor
                && (planningMode == QStringLiteral("direct_time_block") || draft.value(QStringLiteral("hasTimeAnchor")).toBool())
                && draft.value(QStringLiteral("scheduledStart")).toString().trimmed().isEmpty()) {
                draft.insert(QStringLiteral("hasTimeAnchor"), true);
                draft.insert(QStringLiteral("scheduledStart"), deadlineText(localHints.scheduledStart));
                draft.insert(QStringLiteral("scheduledEnd"), deadlineText(localHints.scheduledEnd));
                draft.insert(QStringLiteral("deadline"), deadlineText(localHints.scheduledEnd));
                draft.insert(QStringLiteral("estimatedMinutes"), localHints.estimatedMinutes);
                if (draft.value(QStringLiteral("categoryName")).toString().trimmed().isEmpty()) {
                    draft.insert(QStringLiteral("categoryName"), localHints.categoryName);
                }
            }

            const QDateTime normalizedStart = parseDateTimeText(draft.value(QStringLiteral("scheduledStart")).toString());
            const QDateTime normalizedEnd = parseDateTimeText(draft.value(QStringLiteral("scheduledEnd")).toString());
            if (normalizedStart.isValid()) {
                draft.insert(QStringLiteral("scheduledStart"), deadlineText(normalizedStart));
            }
            if (normalizedEnd.isValid()) {
                draft.insert(QStringLiteral("scheduledEnd"), deadlineText(normalizedEnd));
                if (draft.value(QStringLiteral("deadline")).toString().trimmed().isEmpty()) {
                    draft.insert(QStringLiteral("deadline"), deadlineText(normalizedEnd));
                }
            }

            if (aiDraftLooksUseful(input, draft)) {
                QVariantList drafts = aiResult.taskDrafts;
                if (drafts.isEmpty()) {
                    drafts.push_back(draft);
                }
                for (QVariant& value : drafts) {
                    QVariantMap item = value.toMap();
                    item.insert(QStringLiteral("source"), draft.value(QStringLiteral("source")));
                    value = item;
                }
                return {
                    {"ok", true},
                    {"message", aiResult.message},
                    {"source", draft.value(QStringLiteral("source"))},
                    {"draft", draft},
                    {"drafts", drafts},
                    {"draftCount", drafts.size()}
                };
            }
            aiFallbackMessage = QObject::tr("AI 结果质量不足，已回退到本地规则解析");
        } else {
            aiFallbackMessage = aiResult.message;
        }
    } else if (m_aiService) {
        aiFallbackMessage = QObject::tr("尚未连接 DeepSeek");
    }

    const ParsedTaskDraft parsed = m_parser.parse(input);
    if (!parsed.valid) {
        return {{"ok", false}, {"message", parsed.message}};
    }
    const QString message = aiFallbackMessage.trimmed().isEmpty()
        ? QObject::tr("已生成本地规则任务草稿，请确认后加入日程")
        : QObject::tr("%1，已生成本地规则任务草稿，请确认后加入日程").arg(aiFallbackMessage);
    const QVariantMap draft = parsedDraftToMap(parsed, QStringLiteral("local"), message);
    return {
        {"ok", true},
        {"message", message},
        {"source", QStringLiteral("local")},
        {"draft", draft},
        {"drafts", QVariantList{draft}},
        {"draftCount", 1}
    };
}

QVariantMap ScheduleService::createTaskFromDraft(const QVariantMap& draft)
{
    const QString title = draft.value(QStringLiteral("title")).toString().trimmed();
    QDateTime deadline = parseDateTimeText(draft.value(QStringLiteral("deadline")).toString());
    if (title.isEmpty() || !deadline.isValid()) {
        return {{"ok", false}, {"message", QObject::tr("任务草稿缺少标题或有效截止时间")}};
    }

    const QString planningMode = draft.value(QStringLiteral("planningMode")).toString();
    const bool hasTimeAnchor = draft.value(QStringLiteral("hasTimeAnchor")).toBool()
        || planningMode == QStringLiteral("direct_time_block");
    QDateTime scheduledStart = parseDateTimeText(draft.value(QStringLiteral("scheduledStart")).toString());
    QDateTime scheduledEnd = parseDateTimeText(draft.value(QStringLiteral("scheduledEnd")).toString());

    Task task;
    task.title = title;
    task.notes = draft.value(QStringLiteral("notes")).toString();
    task.startDate = QDateTime(QDate::currentDate(), QTime(0, 0));
    task.deadlineType = DeadlineType::Soft;
    task.autoScheduleEnabled = true;
    task.deadline = deadline;

    const int estimatedMinutes = draft.contains(QStringLiteral("estimatedMinutes"))
        ? draft.value(QStringLiteral("estimatedMinutes")).toInt()
        : 60;
    task.estimatedMinutes = qMax(30, estimatedMinutes);

    if (hasTimeAnchor) {
        if (!scheduledStart.isValid()) {
            scheduledStart = deadline.addSecs(-task.estimatedMinutes * 60);
        }
        if (!scheduledEnd.isValid() || scheduledEnd <= scheduledStart) {
            scheduledEnd = scheduledStart.addSecs(task.estimatedMinutes * 60);
        }
        task.estimatedMinutes = qMax(30, static_cast<int>(scheduledStart.secsTo(scheduledEnd) / 60));
        deadline = qMax(deadline, scheduledEnd);
        task.deadline = deadline;
    }

    task.estimatedHours = task.estimatedMinutes / 60.0;
    task.remainingMinutes = task.estimatedMinutes;
    task.minChunkMinutes = m_config.minimumBlockMinutes;
    task.idealChunkMinutes = m_config.preferredBlockMinutes;
    task.effortLevel = 1;
    const int priority = draft.contains(QStringLiteral("priority"))
        ? draft.value(QStringLiteral("priority")).toInt()
        : 1;
    task.priority = static_cast<Priority>(qBound(0, priority, 2));
    task.status = TaskStatus::Inbox;
    task.preferredStudyTime = draft.value(QStringLiteral("preferredStudyTime")).toString();
    if (task.preferredStudyTime.trimmed().isEmpty()) {
        task.preferredStudyTime = QStringLiteral("evening");
    }

    QString categoryName = draft.value(QStringLiteral("categoryName")).toString();
    if (categoryName.trimmed().isEmpty()) {
        categoryName = QObject::tr("学习");
    }

    if (hasTimeAnchor) {
        const QVector<CalendarEvent> events = m_calendar.eventsBetween(scheduledStart, scheduledEnd);
        const QVector<TimeBlock> blocks = m_blocks.blocksBetween(scheduledStart, scheduledEnd);
        if (!m_conflicts.canPlace({scheduledStart, scheduledEnd}, events, blocks)) {
            return {{"ok", false}, {"message", QObject::tr("这个时间段已有安排，无法直接创建时间块")}};
        }
    }

    const int createdTaskId = m_tasks.createTaskReturningId(task, categoryName);
    if (createdTaskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("任务创建失败：%1").arg(m_tasks.lastError())}};
    }

    int createdBlockId = 0;
    if (hasTimeAnchor) {
        TimeBlock block;
        block.taskId = createdTaskId;
        block.start = scheduledStart;
        block.end = scheduledEnd;
        block.source = BlockSource::UserDragged;
        block.explanation = QObject::tr("由 AI/Quick Add 识别时间后直接创建");
        createdBlockId = m_blocks.createBlock(block);
        if (createdBlockId <= 0) {
            return {{"ok", false}, {"message", QObject::tr("任务已创建，但时间块创建失败")}};
        }
    }

    reschedule();
    if (createdBlockId > 0) {
        selectTimelineItem(createdTaskId, createdBlockId);
    } else {
        selectTask(createdTaskId);
    }

    return {
        {"ok", true},
        {"message", createdBlockId > 0 ? QObject::tr("已创建任务，并自动生成时间块") : QObject::tr("任务已加入日程并重新排程")},
        {"taskId", createdTaskId},
        {"blockId", createdBlockId},
        {"title", task.title},
        {"deadline", deadlineText(task.deadline)},
        {"scheduledStart", createdBlockId > 0 ? deadlineText(scheduledStart) : QString()},
        {"scheduledEnd", createdBlockId > 0 ? deadlineText(scheduledEnd) : QString()},
        {"category", categoryName}
    };
}

QVariantMap ScheduleService::setDeepSeekApiKey(const QString& apiKey)
{
    const QString normalized = normalizedDeepSeekKey(apiKey);
    if (!apiKey.trimmed().isEmpty() && !normalized.startsWith(QStringLiteral("sk-"))) {
        return {{"ok", false}, {"message", QObject::tr("Key 格式看起来不对，请粘贴 sk- 开头的 DeepSeek API Key")}};
    }
    if (!m_settings.setValue(QStringLiteral("deepseekApiKey"), normalized)) {
        return {{"ok", false}, {"message", QObject::tr("AI 配置保存失败")}};
    }
    if (m_aiService && !m_aiService->setApiKey(normalized)) {
        return {{"ok", false}, {"message", QObject::tr("当前 AI Provider 不支持 API Key 配置")}};
    }
    emit dataChanged();
    return {
        {"ok", true},
        {"message", normalized.isEmpty() ? QObject::tr("已切换为本地规则模式") : QObject::tr("DeepSeek 已连接，Quick Add 将优先使用 AI 解析")}
    };
}

QVariantMap ScheduleService::quickAdd(const QString& input)
{
    const QVariantMap preview = previewTaskDraft(input);
    if (!preview.value(QStringLiteral("ok")).toBool()) {
        return preview;
    }
    return createTaskFromDraft(preview.value(QStringLiteral("draft")).toMap());
}

QVariantMap ScheduleService::createTasksFromDrafts(const QVariantList& drafts)
{
    if (drafts.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("没有可创建的任务草稿")}};
    }
    int created = 0;
    for (const QVariant& value : drafts) {
        const QVariantMap result = createTaskFromDraft(value.toMap());
        if (!result.value(QStringLiteral("ok")).toBool()) {
            return {
                {"ok", false},
                {"createdCount", created},
                {"message", QObject::tr("已创建 %1 个任务，后续草稿失败：%2")
                    .arg(created)
                    .arg(result.value(QStringLiteral("message")).toString())}
            };
        }
        ++created;
    }
    return {
        {"ok", true},
        {"createdCount", created},
        {"message", QObject::tr("已确认并创建 %1 个任务").arg(created)}
    };
}

ScheduleContext ScheduleService::buildAIContext(const QString& mode) const
{
    QVariantList tasks;
    for (const Task& task : m_tasks.activeTasks()) {
        tasks.push_back(QVariantMap{
            {QStringLiteral("id"), task.id},
            {QStringLiteral("title"), task.title},
            {QStringLiteral("deadline"), deadlineText(task.deadline)},
            {QStringLiteral("remainingMinutes"), task.remainingMinutes},
            {QStringLiteral("priority"), static_cast<int>(task.priority)},
            {QStringLiteral("category"), task.categoryName},
            {QStringLiteral("preferredStudyTime"), task.preferredStudyTime}
        });
    }

    const ScheduleWindow window = currentWindow();
    QVariantList events;
    for (const CalendarEvent& event : m_calendar.eventsBetween(window.start, window.end)) {
        events.push_back(QVariantMap{
            {QStringLiteral("title"), event.title},
            {QStringLiteral("start"), deadlineText(event.start)},
            {QStringLiteral("end"), deadlineText(event.end)},
            {QStringLiteral("category"), event.categoryName},
            {QStringLiteral("locked"), event.locked}
        });
    }

    QVariantList blocks;
    for (const TimeBlock& block : m_blocks.blocksBetween(window.start, window.end)) {
        blocks.push_back(QVariantMap{
            {QStringLiteral("id"), block.id},
            {QStringLiteral("taskId"), block.taskId},
            {QStringLiteral("start"), deadlineText(block.start)},
            {QStringLiteral("end"), deadlineText(block.end)},
            {QStringLiteral("source"), blockSourceKey(block.source)},
            {QStringLiteral("completed"), block.completedAt.isValid()}
        });
    }

    ScheduleContext context;
    context.data = {
        {QStringLiteral("mode"), mode},
        {QStringLiteral("now"), deadlineText(QDateTime::currentDateTime())},
        {QStringLiteral("tasks"), tasks},
        {QStringLiteral("events"), events},
        {QStringLiteral("blocks"), blocks},
        {QStringLiteral("issues"), m_scheduleIssues},
        {QStringLiteral("capacity"), m_capacityStats},
        {QStringLiteral("selected"), selectedDetail()}
    };
    return context;
}

QVariantMap ScheduleService::localAIPlan(const QString& mode) const
{
    QVariantList reasons;
    QVariantList suggestions;
    const QString currentFocus = m_focusItem.value(QStringLiteral("title")).toString();
    const QString riskLevel = m_unscheduledTaskIds.isEmpty() ? QStringLiteral("low") : QStringLiteral("high");
    const QString title = mode == QStringLiteral("explain_schedule")
        ? QObject::tr("排程解释")
        : (mode == QStringLiteral("daily_plan") ? QObject::tr("AI 今日计划") : QObject::tr("智能排程建议"));
    QString summary = m_unscheduledTaskIds.isEmpty()
        ? QObject::tr("当前计划可执行，优先完成临近截止且优先级较高的任务。")
        : QObject::tr("当前有 %1 个任务未能完整排入，需要调整截止时间或可用容量。").arg(m_unscheduledTaskIds.size());

    if (mode == QStringLiteral("explain_schedule")) {
        const QVariantMap detail = selectedDetail();
        summary = detail.isEmpty()
            ? QObject::tr("请先选择一个时间块。")
            : QObject::tr("该安排由本地 Scheduler 基于截止时间、优先级、固定事件和可用容量生成。AI 只解释，不直接修改数据库。");
        reasons.push_back(QObject::tr("本地排程结果是最终事实来源。"));
        reasons.push_back(QObject::tr("锁定块和固定事件会优先保留。"));
    } else {
        if (!currentFocus.isEmpty()) {
            reasons.push_back(QObject::tr("当前建议先完成：%1").arg(currentFocus));
        }
        for (const QVariant& value : m_scheduleIssues) {
            const QVariantMap issue = value.toMap();
            reasons.push_back(QObject::tr("%1：%2").arg(
                issue.value(QStringLiteral("title")).toString(),
                issue.value(QStringLiteral("reason")).toString()));
            const int taskId = issue.value(QStringLiteral("taskId")).toInt();
            const auto task = m_taskModel.taskById(taskId);
            if (task) {
                suggestions.push_back(QVariantMap{
                    {QStringLiteral("title"), QObject::tr("延长“%1”的截止时间").arg(task->title)},
                    {QStringLiteral("description"), issue.value(QStringLiteral("fixHint")).toString()},
                    {QStringLiteral("impact"), QObject::tr("为 Scheduler 增加一天可用容量")},
                    {QStringLiteral("actionType"), QStringLiteral("extend_deadline")},
                    {QStringLiteral("taskId"), taskId},
                    {QStringLiteral("proposedDeadline"), deadlineText(task->deadline.addDays(1))}
                });
            }
        }
        if (suggestions.isEmpty()) {
            suggestions.push_back(QVariantMap{
                {QStringLiteral("title"), QObject::tr("按当前约束重新排程")},
                {QStringLiteral("description"), QObject::tr("保留固定事件和用户锁定时间块，刷新自动时间块。")},
                {QStringLiteral("impact"), QObject::tr("不会修改任务内容")},
                {QStringLiteral("actionType"), QStringLiteral("reschedule")},
                {QStringLiteral("taskId"), 0},
                {QStringLiteral("proposedDeadline"), QString()}
            });
        }
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("title"), title},
        {QStringLiteral("summary"), summary},
        {QStringLiteral("riskLevel"), riskLevel},
        {QStringLiteral("currentFocus"), currentFocus},
        {QStringLiteral("reasons"), reasons},
        {QStringLiteral("suggestions"), suggestions},
        {QStringLiteral("provider"), QStringLiteral("local")},
        {QStringLiteral("model"), QObject::tr("本地规则")}
    };
}

QVariantMap ScheduleService::requestAIPlan(const QString& mode)
{
    if (m_aiService && m_aiService->isConfigured()) {
        QFuture<AISuggestionResult> future = m_aiService->suggestScheduleChanges(buildAIContext(mode));
        future.waitForFinished();
        const AISuggestionResult result = future.result();
        if (result.supported && !result.suggestion.isEmpty()) {
            QVariantMap plan = result.suggestion;
            QVariantList validSuggestions;
            const QSet<QString> allowed{
                QStringLiteral("reschedule"),
                QStringLiteral("extend_deadline"),
                QStringLiteral("review_task")
            };
            for (const QVariant& value : plan.value(QStringLiteral("suggestions")).toList()) {
                const QVariantMap suggestion = value.toMap();
                if (allowed.contains(suggestion.value(QStringLiteral("actionType")).toString())) {
                    validSuggestions.push_back(suggestion);
                }
            }
            plan.insert(QStringLiteral("suggestions"), validSuggestions);
            plan.insert(QStringLiteral("ok"), true);
            plan.insert(QStringLiteral("provider"), result.provider);
            return plan;
        }
    }
    return localAIPlan(mode);
}

QVariantMap ScheduleService::aiTodayPlan()
{
    return requestAIPlan(QStringLiteral("daily_plan"));
}

QVariantMap ScheduleService::previewScheduleSuggestions()
{
    return requestAIPlan(QStringLiteral("schedule_suggestions"));
}

QVariantMap ScheduleService::explainSelectedSchedule()
{
    if (selectedDetail().isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务或时间块")}};
    }
    return requestAIPlan(QStringLiteral("explain_schedule"));
}

QVariantMap ScheduleService::applyScheduleSuggestion(const QVariantMap& suggestion)
{
    const QString action = suggestion.value(QStringLiteral("actionType")).toString();
    if (action == QStringLiteral("reschedule")) {
        return reschedule();
    }

    const int taskId = suggestion.value(QStringLiteral("taskId")).toInt();
    const auto taskValue = m_taskModel.taskById(taskId);
    if (!taskValue) {
        return {{"ok", false}, {"message", QObject::tr("建议引用的任务不存在")}};
    }
    if (action == QStringLiteral("review_task")) {
        selectTask(taskId);
        return {{"ok", true}, {"message", QObject::tr("已打开任务详情")}};
    }
    if (action != QStringLiteral("extend_deadline")) {
        return {{"ok", false}, {"message", QObject::tr("不支持的 AI 建议操作")}};
    }

    QDateTime proposed = parseDateTimeText(suggestion.value(QStringLiteral("proposedDeadline")).toString());
    Task task = *taskValue;
    if (!proposed.isValid() || proposed <= task.deadline || proposed > task.deadline.addDays(30)) {
        return {{"ok", false}, {"message", QObject::tr("建议的截止时间无效或超出允许范围")}};
    }
    task.deadline = proposed;
    if (!m_tasks.updateTask(task, task.categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("任务调整失败：%1").arg(m_tasks.lastError())}};
    }
    reschedule();
    selectTask(taskId);
    return {{"ok", true}, {"message", QObject::tr("已确认延期并重新排程")}};
}

void ScheduleService::requestTaskDraft(const QString& input)
{
    QPointer<ScheduleService> self(this);
    auto future = QtConcurrent::run([self, input] {
        if (!self) {
            return;
        }
        const QVariantMap result = self->previewTaskDraft(input);
        QMetaObject::invokeMethod(self, [self, result] {
            if (self) {
                emit self->taskDraftReady(result);
            }
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

QVariantMap ScheduleService::previewImageTask(const QString& fileUrlOrPath)
{
    return m_ocr.previewImage(fileUrlOrPath);
}

QVariantMap ScheduleService::eveningReview() const
{
    const QDate today = QDate::currentDate();
    const QDateTime dayStart(today, QTime(0, 0));
    const QDateTime dayEnd(today.addDays(1), QTime(0, 0));
    const QVector<Task> tasks = m_tasks.allTasks();
    const QVector<TimeBlock> blocks = m_blocks.blocksBetween(dayStart, dayEnd);

    QHash<int, Task> taskById;
    QHash<int, int> todayMinutesByTask;
    int plannedMinutes = 0;
    int completedMinutes = 0;
    for (const auto& task : tasks) {
        taskById.insert(task.id, task);
    }
    for (const auto& block : blocks) {
        if (block.taskId <= 0 || !taskById.contains(block.taskId)) {
            continue;
        }
        const int minutes = static_cast<int>(block.start.secsTo(block.end) / 60);
        plannedMinutes += minutes;
        todayMinutesByTask[block.taskId] += minutes;
        if (block.completedAt.isValid()) {
            completedMinutes += minutes;
        }
    }

    QVariantList completedTasks;
    QVariantList unfinishedTasks;
    QVariantList carriedOverTasks;
    QSet<int> seen;
    for (auto it = todayMinutesByTask.constBegin(); it != todayMinutesByTask.constEnd(); ++it) {
        const Task task = taskById.value(it.key());
        QVariantMap map = taskToMap(task);
        map.insert(QStringLiteral("todayPlannedMinutes"), it.value());
        if (task.status == TaskStatus::Done) {
            completedTasks.push_back(map);
        } else {
            unfinishedTasks.push_back(map);
            carriedOverTasks.push_back(map);
        }
        seen.insert(task.id);
    }

    for (const auto& task : tasks) {
        if (seen.contains(task.id) || task.status == TaskStatus::Done || task.status == TaskStatus::Archived) {
            continue;
        }
        if (task.scheduleStatus == ScheduleStatus::CouldNotFit || task.deadline.date() <= today.addDays(1)) {
            carriedOverTasks.push_back(taskToMap(task));
        }
    }

    const int completionRate = plannedMinutes > 0
        ? qBound(0, qRound(completedMinutes * 100.0 / plannedMinutes), 100)
        : 0;
    const QString summary = plannedMinutes <= 0
        ? QObject::tr("浠婂ぉ杩樻病鏈夊彲澶嶇洏鐨勮鍒掓椂闂村潡")
        : QObject::tr("浠婃棩璁″垝 %1 鍒嗛挓锛屽凡瀹屾垚 %2 鍒嗛挓锛屽畬鎴愮巼 %3%")
              .arg(plannedMinutes)
              .arg(completedMinutes)
              .arg(completionRate);

    return {
        {QStringLiteral("dateText"), today.toString(QStringLiteral("yyyy-MM-dd"))},
        {QStringLiteral("plannedMinutes"), plannedMinutes},
        {QStringLiteral("completedMinutes"), completedMinutes},
        {QStringLiteral("completionRate"), completionRate},
        {QStringLiteral("completedTasks"), completedTasks},
        {QStringLiteral("unfinishedTasks"), unfinishedTasks},
        {QStringLiteral("carriedOverTasks"), carriedOverTasks},
        {QStringLiteral("unfinishedCount"), unfinishedTasks.size()},
        {QStringLiteral("carriedOverCount"), carriedOverTasks.size()},
        {QStringLiteral("summary"), summary}
    };
}

bool ScheduleService::setDemoModeEnabled(bool enabled)
{
    const bool ok = m_settings.setValue(QStringLiteral("demoModeEnabled"), enabled ? QStringLiteral("true") : QStringLiteral("false"));
    if (ok) {
        emit dataChanged();
    }
    return ok;
}

QVariantMap ScheduleService::exportData(const QString& filePath)
{
    if (!m_backup.exportToFile(filePath)) {
        return {
            {"ok", false},
            {"message", QObject::tr("鏁版嵁瀵煎嚭澶辫触锛?1").arg(m_backup.lastError())}
        };
    }
    return {{"ok", true}, {"message", QObject::tr("数据已导出")}};
}

QVariantMap ScheduleService::importData(const QString& filePath)
{
    if (!m_backup.importFromFile(filePath)) {
        return {
            {"ok", false},
            {"message", QObject::tr("鏁版嵁瀵煎叆澶辫触锛?1").arg(m_backup.lastError())}
        };
    }
    m_selectedTaskId = 0;
    m_selectedBlockId = 0;
    m_unscheduledTaskIds.clear();
    m_scheduleIssues.clear();
    m_persistenceError.clear();
    reload();
    return {{"ok", true}, {"message", QObject::tr("数据已导入")}};
}

ScheduleWindow ScheduleService::currentWindow() const
{
    const QDate today = QDate::currentDate();
    return {QDateTime(today, QTime(0, 0)), QDateTime(today.addDays(7), QTime(23, 59, 59))};
}

ScheduleWindow ScheduleService::displayWindow() const
{
    return {
        QDateTime(m_selectedDate, QTime(0, 0)),
        QDateTime(m_selectedDate.addDays(7), QTime(0, 0))
    };
}

void ScheduleService::reload()
{
    const ScheduleWindow window = displayWindow();
    const QVector<Task> tasks = m_tasks.allTasks();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> blocks = m_blocks.blocksBetween(window.start, window.end);
    m_taskModel.setTasks(m_tasks.activeTasks());
    rebuildTimeline(tasks, events, blocks);
    rebuildCapacityStats(blocks);
    emit dataChanged();
    emit selectedTaskChanged();
}

void ScheduleService::rebuildCapacityStats(const QVector<TimeBlock>& blocks)
{
    const QDate today = QDate::currentDate();
    int todayScheduled = 0;
    int weekScheduled = 0;
    for (const auto& block : blocks) {
        const int minutes = static_cast<int>(block.start.secsTo(block.end) / 60);
        if (block.start.date() == today) {
            todayScheduled += minutes;
        }
        if (block.start.date() >= today && block.start.date() < today.addDays(7)) {
            weekScheduled += minutes;
        }
    }

    const QTime now = QTime::currentTime();
    int remainingToday = 0;
    if (now.hour() < m_config.workdayEndHour) {
        const int currentMinute = qMax(m_config.workdayStartHour * 60, now.hour() * 60 + now.minute());
        remainingToday = qMax(0, m_config.workdayEndHour * 60 - currentMinute);
    }

    const bool overloaded = todayScheduled > remainingToday + 120 || weekScheduled > 36 * 60;
    m_capacityStats = {
        {"todayRemainingMinutes", remainingToday},
        {"todayScheduledMinutes", todayScheduled},
        {"weekScheduledMinutes", weekScheduled},
        {"overloaded", overloaded},
        {"message", overloaded ? QObject::tr("鏈懆璐熻浇鍋忛珮") : QObject::tr("瀛︿範瀹归噺鍋ュ悍")}
    };
}

void ScheduleService::rebuildTimeline(const QVector<Task>& tasks, const QVector<CalendarEvent>& events, const QVector<TimeBlock>& blocks)
{
    const ScheduleWindow window = displayWindow();
    QHash<int, Task> taskById;
    QHash<int, int> blockTotalsByTask;
    QHash<int, int> blockOrdinalsById;
    for (const auto& task : tasks) {
        taskById.insert(task.id, task);
    }
    QVector<TimeBlock> orderedBlocks = blocks;
    std::sort(orderedBlocks.begin(), orderedBlocks.end(), [](const TimeBlock& a, const TimeBlock& b) {
        if (a.taskId == b.taskId) {
            return a.start < b.start;
        }
        return a.taskId < b.taskId;
    });
    for (const auto& block : orderedBlocks) {
        if (block.taskId <= 0 || !taskById.contains(block.taskId)) {
            continue;
        }
        const int ordinal = blockTotalsByTask.value(block.taskId, 0) + 1;
        blockTotalsByTask.insert(block.taskId, ordinal);
        blockOrdinalsById.insert(block.id, ordinal);
    }

    QVector<TimelineItem> items;
    for (const auto& event : events) {
        TimelineItem item;
        item.id = -event.id;
        item.title = event.title;
        item.subtitle = event.categoryName.isEmpty() ? QObject::tr("璇剧▼ / 鍥哄畾鏃堕棿") : event.categoryName;
        item.start = event.start;
        item.end = event.end;
        item.dayIndex = window.start.date().daysTo(event.start.date());
        item.startMinute = event.start.time().hour() * 60 + event.start.time().minute();
        item.durationMinutes = static_cast<int>(event.start.secsTo(event.end) / 60);
        item.color = event.categoryColor.isEmpty() ? QStringLiteral("#3F4658") : event.categoryColor;
        item.event = true;
        item.kind = QStringLiteral("event");
        item.source = eventSourceKey(event.type);
        item.locked = event.locked;
        item.eventLocked = event.locked;
        item.canMove = !event.locked;
        item.canResize = !event.locked;
        item.canLock = true;
        items.push_back(item);
    }

    for (const auto& block : blocks) {
        if (!taskById.contains(block.taskId)) {
            continue;
        }
        const Task task = taskById.value(block.taskId);
        TimelineItem item;
        item.id = block.id;
        item.taskId = block.taskId;
        item.title = task.title;
        item.subtitle = task.categoryName.isEmpty() ? QObject::tr("瀛︿範浠诲姟") : task.categoryName;
        item.start = block.start;
        item.end = block.end;
        item.dayIndex = window.start.date().daysTo(block.start.date());
        item.startMinute = block.start.time().hour() * 60 + block.start.time().minute();
        item.durationMinutes = static_cast<int>(block.start.secsTo(block.end) / 60);
        item.priority = static_cast<int>(task.priority);
        item.color = colorForPriority(task.priority, task.categoryColor);
        item.event = false;
        item.kind = QStringLiteral("task");
        item.source = blockSourceKey(block.source);
        item.locked = block.source == BlockSource::Locked;
        item.eventLocked = false;
        item.canMove = block.source != BlockSource::Locked;
        item.canResize = block.source != BlockSource::Locked;
        item.canLock = true;
        item.blockOrdinal = blockOrdinalsById.value(block.id, 1);
        item.blockTotal = blockTotalsByTask.value(block.taskId, 1);
        item.explanation = block.explanation;
        item.completed = block.completedAt.isValid();
        items.push_back(item);
    }

    std::sort(items.begin(), items.end(), [](const TimelineItem& a, const TimelineItem& b) {
        return a.start < b.start;
    });

    QHash<QString, QVector<int>> spanGroups;
    for (int i = 0; i < items.size(); ++i) {
        const auto& item = items.at(i);
        const QString identity = item.event
            ? QStringLiteral("event:%1:%2").arg(item.title, item.subtitle)
            : QStringLiteral("task:%1").arg(item.taskId);
        const QString key = QStringLiteral("%1:%2:%3")
            .arg(identity)
            .arg(item.startMinute)
            .arg(item.durationMinutes);
        spanGroups[key].push_back(i);
    }

    for (const auto& groupIndexes : spanGroups) {
        if (groupIndexes.size() < 2) {
            continue;
        }
        QVector<int> sortedIndexes = groupIndexes;
        std::sort(sortedIndexes.begin(), sortedIndexes.end(), [&items](int a, int b) {
            return items.at(a).dayIndex < items.at(b).dayIndex;
        });

        int runStart = 0;
        while (runStart < sortedIndexes.size()) {
            int runEnd = runStart;
            while (runEnd + 1 < sortedIndexes.size()
                   && items.at(sortedIndexes.at(runEnd + 1)).dayIndex == items.at(sortedIndexes.at(runEnd)).dayIndex + 1) {
                ++runEnd;
            }

            const int span = runEnd - runStart + 1;
            if (span > 1) {
                const int leadIndex = sortedIndexes.at(runStart);
                items[leadIndex].spanDays = span;
                for (int i = runStart + 1; i <= runEnd; ++i) {
                    items[sortedIndexes.at(i)].hiddenInSpan = true;
                }
            }
            runStart = runEnd + 1;
        }
    }

    m_focusItem.clear();
    const QDateTime now = QDateTime::currentDateTime();
    for (const auto& item : items) {
        if (!item.event && !item.completed && item.start <= now && now < item.end) {
            m_focusItem = {{"title", item.title}, {"subtitle", focusSubtitle(item)}, {"color", item.color}, {"taskId", item.taskId}, {"blockId", item.id}};
            break;
        }
    }
    if (m_focusItem.isEmpty()) {
        for (const auto& item : items) {
            if (!item.event && !item.completed && item.start > now) {
                m_focusItem = {{"title", item.title}, {"subtitle", focusSubtitle(item)}, {"color", item.color}, {"taskId", item.taskId}, {"blockId", item.id}};
                break;
            }
        }
    }
    if (m_focusItem.isEmpty()) {
        m_focusItem = {{"title", QObject::tr("今天没有自动安排")}, {"subtitle", QObject::tr("可以添加任务或检查冲突")}, {"color", QStringLiteral("#7C8CFF")}, {"taskId", 0}};
    }

    m_timelineModel.setItems(items);
    if (m_selectedBlockId != 0) {
        m_timelineModel.setSelectedItemId(m_selectedBlockId);
    } else {
        m_timelineModel.setSelectedTaskId(m_selectedTaskId);
    }
}

QVariantMap ScheduleService::taskToMap(const Task& task) const
{
    return {
        {"id", task.id},
        {"title", task.title},
        {"notes", task.notes},
        {"deadlineText", deadlineText(task.deadline)},
        {"estimatedMinutes", task.estimatedMinutes},
        {"remainingMinutes", task.remainingMinutes},
        {"priority", static_cast<int>(task.priority)},
        {"categoryName", task.categoryName},
        {"preferredStudyTime", task.preferredStudyTime},
        {"startDateText", deadlineText(task.startDate)},
        {"deadlineType", static_cast<int>(task.deadlineType)},
        {"autoScheduleEnabled", task.autoScheduleEnabled},
        {"minChunkMinutes", task.minChunkMinutes},
        {"idealChunkMinutes", task.idealChunkMinutes},
        {"effortLevel", task.effortLevel},
        {"scheduleStatus", static_cast<int>(task.scheduleStatus)},
        {"status", static_cast<int>(task.status)},
        {"categoryColor", colorForPriority(task.priority, task.categoryColor)},
        {"completedAt", task.completedAt.isValid() ? deadlineText(task.completedAt) : QString()}
    };
}

QVariantMap ScheduleService::deadlineReminderToMap(const DeadlineReminder& reminder) const
{
    const QDate today = QDate::currentDate();
    const int daysLeft = reminder.dueAt.isValid() ? today.daysTo(reminder.dueAt.date()) : 0;
    const bool open = reminder.status == DeadlineReminderStatus::Open;
    const bool overdue = open && daysLeft < 0;
    const bool reminderDue = open && daysLeft <= reminder.remindDaysBefore;
    QString urgency = QStringLiteral("normal");
    if (overdue) {
        urgency = QStringLiteral("overdue");
    } else if (reminderDue) {
        urgency = daysLeft <= 1 ? QStringLiteral("critical") : QStringLiteral("soon");
    }

    return {
        {"id", reminder.id},
        {"title", reminder.title},
        {"notes", reminder.notes},
        {"dueText", deadlineText(reminder.dueAt)},
        {"dueDateText", reminder.dueAt.date().toString(QStringLiteral("yyyy-MM-dd"))},
        {"categoryName", reminder.categoryName.isEmpty() ? QStringLiteral("DDL") : reminder.categoryName},
        {"categoryColor", reminder.categoryColor.isEmpty() ? QStringLiteral("#F59E0B") : reminder.categoryColor},
        {"remindDaysBefore", reminder.remindDaysBefore},
        {"daysLeft", daysLeft},
        {"status", static_cast<int>(reminder.status)},
        {"completedAt", reminder.completedAt.isValid() ? deadlineText(reminder.completedAt) : QString()},
        {"overdue", overdue},
        {"reminderDue", reminderDue},
        {"urgency", urgency}
    };
}

QVariantMap ScheduleService::eventToDetailMap(const TimelineItem& item) const
{
    return {
        {"id", qAbs(item.id)},
        {"blockId", item.id},
        {"title", item.title},
        {"notes", QString()},
        {"deadlineText", deadlineText(item.end)},
        {"priority", 1},
        {"categoryName", item.subtitle == QObject::tr("璇剧▼ / 鍥哄畾鏃堕棿") ? QObject::tr("璇剧▼") : item.subtitle},
        {"categoryColor", item.color},
        {"preferredStudyTime", QStringLiteral("evening")},
        {"deadlineType", 1},
        {"autoScheduleEnabled", false},
        {"minChunkMinutes", m_config.minimumBlockMinutes},
        {"idealChunkMinutes", m_config.preferredBlockMinutes},
        {"effortLevel", 1},
        {"blockDayIndex", item.dayIndex},
        {"blockEndDayIndex", item.dayIndex + qMax(1, item.spanDays) - 1},
        {"blockStartText", item.start.time().toString(QStringLiteral("HH:mm"))},
        {"blockEndText", item.end.time().toString(QStringLiteral("HH:mm"))},
        {"blockTimeRange", focusSubtitle(item)},
        {"isLocked", item.eventLocked},
        {"kind", item.kind},
        {"source", item.source},
        {"lockActive", item.locked || item.eventLocked},
        {"canMove", item.canMove},
        {"canResize", item.canResize},
        {"canLock", item.canLock},
        {"blockOrdinal", 1},
        {"blockTotal", 1},
        {"completed", false}
    };
}
