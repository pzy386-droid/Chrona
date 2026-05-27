#include "app/ScheduleService.h"

#include <QHash>
#include <QtGlobal>
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

QString focusSubtitle(const TimelineItem& item)
{
    return item.start.time().toString(QStringLiteral("hh:mm")) + QStringLiteral(" - ") + item.end.time().toString(QStringLiteral("hh:mm"));
}

QVector<Task> subtractLockedBlockCapacity(QVector<Task> tasks, const QVector<TimeBlock>& lockedBlocks)
{
    QHash<int, int> scheduledMinutesByTask;
    for (const auto& block : lockedBlocks) {
        if (block.taskId <= 0 || !block.start.isValid() || !block.end.isValid() || block.end <= block.start) {
            continue;
        }
        scheduledMinutesByTask[block.taskId] += static_cast<int>(block.start.secsTo(block.end) / 60);
    }

    for (auto& task : tasks) {
        const int alreadyScheduled = scheduledMinutesByTask.value(task.id, 0);
        if (alreadyScheduled > 0) {
            task.remainingMinutes = qMax(0, task.remainingMinutes - alreadyScheduled);
        }
    }
    return tasks;
}

QString deadlineText(const QDateTime& value)
{
    return value.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}
}

ScheduleService::ScheduleService(TaskRepository tasks, CalendarRepository calendar, TimeBlockRepository blocks, QObject* parent)
    : QObject(parent)
    , m_tasks(std::move(tasks))
    , m_calendar(std::move(calendar))
    , m_blocks(std::move(blocks))
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

QVariantMap ScheduleService::selectedTask() const
{
    const auto task = m_taskModel.taskById(m_selectedTaskId);
    if (!task) {
        return {};
    }
    return taskToMap(*task);
}

int ScheduleService::unscheduledCount() const
{
    return m_unscheduledTaskIds.size();
}

void ScheduleService::reschedule()
{
    const ScheduleWindow window = currentWindow();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> locked = m_blocks.lockedBlocksBetween(window.start, window.end);
    const QVector<Task> tasks = subtractLockedBlockCapacity(m_tasks.activeTasks(), locked);

    const ScheduleResult result = m_scheduler.generateSchedule(tasks, events, locked, window, m_config);
    const int runId = m_blocks.createScheduleRun(window, QStringLiteral("manual_or_startup"));
    m_blocks.replaceAutoBlocks(window, result.generatedBlocks, runId);
    m_unscheduledTaskIds = result.unscheduledTaskIds;
    reload();
}

void ScheduleService::selectTask(int taskId)
{
    m_selectedTaskId = taskId;
    m_taskModel.setSelectedTaskId(taskId);
    m_timelineModel.setSelectedTaskId(taskId);
    emit selectedTaskChanged();
}

QVariantMap ScheduleService::startFocus()
{
    const int taskId = m_focusItem.value(QStringLiteral("taskId")).toInt();
    if (taskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("当前没有可开始的任务")}};
    }
    selectTask(taskId);
    return {{"ok", true}, {"message", QObject::tr("已进入当前专注")}, {"taskId", taskId}};
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

QVariantMap ScheduleService::updateTask(int taskId, const QString& title, const QString& notes, const QString& deadlineInput, int estimatedMinutes, int priority, const QString& categoryName, const QString& preferredStudyTime)
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
        return {{"ok", false}, {"message", QObject::tr("截止时间格式应为 yyyy-MM-dd HH:mm")}};
    }

    Task task;
    task.id = taskId;
    task.title = trimmedTitle;
    task.notes = notes.trimmed();
    task.deadline = deadline;
    task.estimatedMinutes = qMax(30, estimatedMinutes);
    task.estimatedHours = task.estimatedMinutes / 60.0;
    task.remainingMinutes = task.estimatedMinutes;
    task.priority = static_cast<Priority>(qBound(0, priority, 2));
    task.preferredStudyTime = preferredStudyTime.trimmed().isEmpty() ? QStringLiteral("evening") : preferredStudyTime.trimmed();

    if (!m_tasks.updateTask(task, categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("任务更新失败")}};
    }

    reschedule();
    selectTask(taskId);
    return {{"ok", true}, {"message", QObject::tr("任务已更新并重新排程")}};
}

QVariantMap ScheduleService::deleteTask(int taskId)
{
    if (taskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务")}};
    }
    if (!m_tasks.deleteTask(taskId)) {
        return {{"ok", false}, {"message", QObject::tr("任务删除失败")}};
    }
    if (m_selectedTaskId == taskId) {
        m_selectedTaskId = 0;
    }
    reschedule();
    return {{"ok", true}, {"message", QObject::tr("任务已删除")}};
}

QVariantMap ScheduleService::addFixedEvent(const QString& title, int dayOffset, int startMinute, int durationMinutes, bool locked, const QString& categoryName)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入固定时间标题")}};
    }
    if (dayOffset < 0 || dayOffset > 6 || startMinute < 0 || startMinute >= 24 * 60 || durationMinutes <= 0 || startMinute + durationMinutes > 24 * 60) {
        return {{"ok", false}, {"message", QObject::tr("固定时间范围无效")}};
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
        return {{"ok", false}, {"message", QObject::tr("固定时间创建失败")}};
    }

    reschedule();
    return {{"ok", true}, {"message", QObject::tr("固定时间已加入日程")}};
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

bool ScheduleService::moveTimelineItem(int itemId, bool isEvent, int dayIndex, int startMinute, int durationMinutes)
{
    if (!isEvent) {
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

QVariantMap ScheduleService::quickAdd(const QString& input)
{
    const ParsedTaskDraft draft = m_parser.parse(input);
    if (!draft.valid) {
        return {{"ok", false}, {"message", draft.message}};
    }

    Task task;
    task.title = draft.title;
    task.notes = draft.notes;
    task.deadline = draft.deadline;
    task.estimatedMinutes = draft.estimatedMinutes;
    task.estimatedHours = draft.estimatedMinutes / 60.0;
    task.remainingMinutes = draft.estimatedMinutes;
    task.priority = draft.priority;
    task.status = TaskStatus::Inbox;
    task.preferredStudyTime = draft.preferredStudyTime;

    if (!m_tasks.createTask(task, draft.categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("任务创建失败")}};
    }

    reschedule();
    return {
        {"ok", true},
        {"message", draft.message},
        {"title", draft.title},
        {"deadline", deadlineText(draft.deadline)},
        {"category", draft.categoryName}
    };
}

QVariantMap ScheduleService::previewImageTask(const QString& fileUrlOrPath)
{
    return m_ocr.previewImage(fileUrlOrPath);
}

ScheduleWindow ScheduleService::currentWindow() const
{
    const QDate today = QDate::currentDate();
    return {QDateTime(today, QTime(0, 0)), QDateTime(today.addDays(7), QTime(23, 59, 59))};
}

void ScheduleService::reload()
{
    const ScheduleWindow window = currentWindow();
    const QVector<Task> tasks = m_tasks.allTasks();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> blocks = m_blocks.blocksBetween(window.start, window.end);
    m_taskModel.setTasks(tasks);
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
        {"message", overloaded ? QObject::tr("本周负载偏高") : QObject::tr("学习容量健康")}
    };
}

void ScheduleService::rebuildTimeline(const QVector<Task>& tasks, const QVector<CalendarEvent>& events, const QVector<TimeBlock>& blocks)
{
    const ScheduleWindow window = currentWindow();
    QHash<int, Task> taskById;
    for (const auto& task : tasks) {
        taskById.insert(task.id, task);
    }

    QVector<TimelineItem> items;
    for (const auto& event : events) {
        TimelineItem item;
        item.id = -event.id;
        item.title = event.title;
        item.subtitle = QObject::tr("课程 / 固定时间");
        item.start = event.start;
        item.end = event.end;
        item.dayIndex = window.start.date().daysTo(event.start.date());
        item.startMinute = event.start.time().hour() * 60 + event.start.time().minute();
        item.durationMinutes = static_cast<int>(event.start.secsTo(event.end) / 60);
        item.color = event.categoryColor.isEmpty() ? QStringLiteral("#3F4658") : event.categoryColor;
        item.event = true;
        item.locked = event.locked;
        item.eventLocked = event.locked;
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
        item.subtitle = task.categoryName.isEmpty() ? QObject::tr("学习任务") : task.categoryName;
        item.start = block.start;
        item.end = block.end;
        item.dayIndex = window.start.date().daysTo(block.start.date());
        item.startMinute = block.start.time().hour() * 60 + block.start.time().minute();
        item.durationMinutes = static_cast<int>(block.start.secsTo(block.end) / 60);
        item.priority = static_cast<int>(task.priority);
        item.color = colorForPriority(task.priority, task.categoryColor);
        item.event = false;
        item.locked = block.source == BlockSource::Locked;
        items.push_back(item);
    }

    std::sort(items.begin(), items.end(), [](const TimelineItem& a, const TimelineItem& b) {
        return a.start < b.start;
    });

    m_focusItem.clear();
    const QDateTime now = QDateTime::currentDateTime();
    for (const auto& item : items) {
        if (!item.event && item.start <= now && now < item.end) {
            m_focusItem = {{"title", item.title}, {"subtitle", focusSubtitle(item)}, {"color", item.color}, {"taskId", item.taskId}};
            break;
        }
    }
    if (m_focusItem.isEmpty()) {
        for (const auto& item : items) {
            if (!item.event && item.start > now) {
                m_focusItem = {{"title", item.title}, {"subtitle", focusSubtitle(item)}, {"color", item.color}, {"taskId", item.taskId}};
                break;
            }
        }
    }
    if (m_focusItem.isEmpty()) {
        m_focusItem = {{"title", QObject::tr("今天没有自动安排")}, {"subtitle", QObject::tr("可以添加任务或检查冲突")}, {"color", QStringLiteral("#7C8CFF")}, {"taskId", 0}};
    }

    m_timelineModel.setItems(items);
    m_timelineModel.setSelectedTaskId(m_selectedTaskId);
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
        {"categoryColor", colorForPriority(task.priority, task.categoryColor)}
    };
}
