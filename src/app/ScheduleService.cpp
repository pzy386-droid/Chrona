#include "app/ScheduleService.h"

#include <QFuture>
#include <QHash>
#include <QSet>
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

QString deadlineText(const QDateTime& value)
{
    return value.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QTime parseClockTime(const QString& input)
{
    QTime value = QTime::fromString(input.trimmed(), QStringLiteral("HH:mm"));
    if (!value.isValid()) {
        value = QTime::fromString(input.trimmed(), QStringLiteral("H:mm"));
    }
    return value;
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
        {QStringLiteral("source"), source},
        {QStringLiteral("explanation"), message}
    };
}
}

ScheduleService::ScheduleService(TaskRepository tasks, CalendarRepository calendar, TimeBlockRepository blocks, StudyFrameRepository studyFrames, AIService* aiService, QObject* parent)
    : QObject(parent)
    , m_tasks(std::move(tasks))
    , m_calendar(std::move(calendar))
    , m_blocks(std::move(blocks))
    , m_studyFrames(std::move(studyFrames))
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

QVariantMap ScheduleService::selectedTask() const
{
    const auto task = m_taskModel.taskById(m_selectedTaskId);
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
        map.insert(QStringLiteral("blockOrdinal"), block->blockOrdinal);
        map.insert(QStringLiteral("blockTotal"), block->blockTotal);
    }
    return map;
}

QVariantList ScheduleService::studyFrames() const
{
    QVariantList frames;
    for (const auto& frame : m_studyFrames.allFrames()) {
        frames.push_back(studyFrameToMap(frame));
    }
    return frames;
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

QVariantList ScheduleService::scheduleIssues() const
{
    return m_scheduleIssues;
}

void ScheduleService::reschedule()
{
    const ScheduleWindow window = currentWindow();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> locked = m_blocks.lockedBlocksBetween(window.start, window.end);
    const QVector<StudyFrame> frames = m_studyFrames.enabledFrames();
    const QVector<Task> tasks = subtractLockedBlockCapacity(m_tasks.activeTasks(), locked);

    const ScheduleResult result = m_scheduler.generateSchedule(tasks, events, locked, frames, window, m_config);
    const int runId = m_blocks.createScheduleRun(window, QStringLiteral("manual_or_startup"));
    m_blocks.replaceAutoBlocks(window, result.generatedBlocks, runId);
    m_tasks.updateScheduleStatuses(result.scheduledTaskIds, result.unscheduledTaskIds);
    m_unscheduledTaskIds = result.unscheduledTaskIds;
    m_scheduleIssues.clear();
    for (const auto& issue : result.issues) {
        const QVariantMap issueMap = {
            {"taskId", issue.taskId},
            {"title", issue.title},
            {"code", issue.code},
            {"reason", issue.reason},
            {"fixHint", issue.fixHint},
            {"remainingMinutes", issue.remainingMinutes}
        };
        m_scheduleIssues.push_back(issueMap);
    }
    reload();
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
        return {{"ok", false}, {"message", QObject::tr("截止时间格式应为 yyyy-MM-dd HH:mm")}};
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
        return {{"ok", false}, {"message", QObject::tr("任务更新失败")}};
    }

    const int previousBlockId = m_selectedBlockId;
    reschedule();
    if (previousBlockId > 0 && m_timelineModel.itemById(previousBlockId)) {
        selectTimelineItem(taskId, previousBlockId);
    } else {
        selectTask(taskId);
    }
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

QVariantMap ScheduleService::updateSelectedEvent(const QString& title, int dayIndex, const QString& startText, const QString& endText, bool locked, const QString& categoryName)
{
    if (m_selectedBlockId >= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个固定时间块")}};
    }

    const QString trimmedTitle = title.trimmed();
    const QTime startTime = parseClockTime(startText);
    const QTime endTime = parseClockTime(endText);
    if (trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入标题")}};
    }
    if (dayIndex < 0 || dayIndex > 6 || !startTime.isValid() || !endTime.isValid() || endTime <= startTime) {
        return {{"ok", false}, {"message", QObject::tr("结束时间必须晚于起始时间")}};
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
        return {{"ok", false}, {"message", QObject::tr("固定时间更新失败")}};
    }

    reschedule();
    m_selectedBlockId = -eventId;
    m_timelineModel.setSelectedItemId(m_selectedBlockId);
    return {{"ok", true}, {"message", QObject::tr("固定时间已更新")}};
}

QVariantMap ScheduleService::addStudyFrame(const QString& name, int dayIndex, const QString& startText, const QString& endText, const QString& categoryName, const QString& energyLevel)
{
    const QTime start = QTime::fromString(startText.trimmed(), QStringLiteral("HH:mm"));
    const QTime end = QTime::fromString(endText.trimmed(), QStringLiteral("HH:mm"));
    if (name.trimmed().isEmpty() || dayIndex < 0 || dayIndex > 6 || !start.isValid() || !end.isValid() || end <= start) {
        return {{"ok", false}, {"message", QObject::tr("Study Frame 时间或名称无效")}};
    }

    const ScheduleWindow window = currentWindow();
    StudyFrame frame;
    frame.name = name.trimmed();
    frame.dayOfWeek = window.start.date().addDays(dayIndex).dayOfWeek();
    frame.startTime = start;
    frame.endTime = end;
    frame.energyLevel = energyLevel.trimmed().isEmpty() ? QStringLiteral("medium") : energyLevel.trimmed();
    frame.enabled = true;

    if (!m_studyFrames.createFrame(frame, categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("Study Frame 创建失败")}};
    }

    reschedule();
    return {{"ok", true}, {"message", QObject::tr("Study Frame 已保存并重新排程")}};
}

bool ScheduleService::setStudyFrameEnabled(int frameId, bool enabled)
{
    if (!m_studyFrames.setEnabled(frameId, enabled)) {
        return false;
    }
    reschedule();
    return true;
}

bool ScheduleService::deleteStudyFrame(int frameId)
{
    if (!m_studyFrames.deleteFrame(frameId)) {
        return false;
    }
    reschedule();
    return true;
}

QVariantMap ScheduleService::scheduleSelectedEventBlocks(const QString& title, int startDayIndex, int endDayIndex, const QString& startText, const QString& endText, bool locked, const QString& categoryName)
{
    if (m_selectedBlockId >= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个固定时间块")}};
    }

    const QString trimmedTitle = title.trimmed();
    const QTime startTime = parseClockTime(startText);
    const QTime endTime = parseClockTime(endText);
    if (trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入标题")}};
    }
    if (!startTime.isValid() || !endTime.isValid() || endTime <= startTime) {
        return {{"ok", false}, {"message", QObject::tr("结束时间必须晚于起始时间")}};
    }

    const int firstDay = qBound(0, qMin(startDayIndex, endDayIndex), 6);
    const int lastDay = qBound(0, qMax(startDayIndex, endDayIndex), 6);
    const int currentEventId = qAbs(m_selectedBlockId);
    const ScheduleWindow window = currentWindow();
    const QVector<CalendarEvent> allEvents = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> blocks = m_blocks.blocksBetween(window.start, window.end);

    QSet<int> editableEventIds;
    QHash<int, int> reusableEventByDay;
    int selectedReusableEventId = 0;
    for (const auto& event : allEvents) {
        if (event.id == currentEventId) {
            selectedReusableEventId = event.id;
            editableEventIds.insert(event.id);
            continue;
        }
        if (event.title.trimmed() != trimmedTitle) {
            continue;
        }
        const int dayIndex = static_cast<int>(window.start.date().daysTo(event.start.date()));
        if (dayIndex >= firstDay && dayIndex <= lastDay && !reusableEventByDay.contains(dayIndex)) {
            reusableEventByDay.insert(dayIndex, event.id);
            editableEventIds.insert(event.id);
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
            return {{"ok", false}, {"message", QObject::tr("固定时间超出当前排程范围")}};
        }
        if (!m_conflicts.canPlace({event.start, event.end}, otherEvents, blocks)) {
            return {{"ok", false}, {"message", QObject::tr("连续固定时间存在冲突，请调整日期或者时间")}};
        }

        otherEvents.push_back(event);
        planned.push_back({event.id, event});
    }

    int selectedEventId = 0;
    for (const auto& plannedEvent : planned) {
        int eventId = plannedEvent.existingId;
        CalendarEvent event = plannedEvent.event;
        if (eventId > 0) {
            event.id = eventId;
            if (!m_calendar.updateEvent(event, categoryName)) {
                return {{"ok", false}, {"message", QObject::tr("连续固定时间更新失败")}};
            }
        } else {
            if (!m_calendar.createEvent(event, categoryName)) {
                return {{"ok", false}, {"message", QObject::tr("连续固定时间创建失败")}};
            }
        }
        if (selectedEventId == 0) {
            selectedEventId = eventId > 0 ? eventId : currentEventId;
        }
    }

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

QVariantMap ScheduleService::setSelectedBlockLocked(bool locked)
{
    if (m_selectedBlockId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个学习时间块")}};
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
    return {{"ok", true}, {"message", locked ? QObject::tr("已固定当前时间块") : QObject::tr("已取消固定，保留当前位置")}};
}

QVariantMap ScheduleService::moveSelectedTaskBlock(int dayIndex, const QString& startText, const QString& endText)
{
    if (m_selectedTaskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务")}};
    }

    const QTime start = parseClockTime(startText);
    const QTime end = parseClockTime(endText);
    if (!start.isValid() || !end.isValid() || end <= start) {
        return {{"ok", false}, {"message", QObject::tr("结束时间必须晚于起始时间")}};
    }

    const int startMinute = start.hour() * 60 + start.minute();
    const int duration = static_cast<int>(start.secsTo(end) / 60);
    if (m_selectedBlockId <= 0) {
        const ScheduleWindow window = currentWindow();
        if (dayIndex < 0 || dayIndex > 6) {
            return {{"ok", false}, {"message", QObject::tr("日期范围无效")}};
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
        return {{"ok", true}, {"message", QObject::tr("已创建当前时间块")}};
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
        return {{"ok", false}, {"message", QObject::tr("结束时间必须晚于起始时间")}};
    }

    const ScheduleWindow window = currentWindow();
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<TimeBlock> allBlocks = m_blocks.blocksBetween(window.start, window.end);

    QSet<int> editableBlockIds;
    QHash<int, int> reusableBlockByDay;
    int selectedReusableBlockId = 0;
    for (const auto& block : allBlocks) {
        if (block.id == m_selectedBlockId) {
            selectedReusableBlockId = block.id;
            editableBlockIds.insert(block.id);
            continue;
        }
        if (block.taskId != m_selectedTaskId) {
            continue;
        }
        const int dayIndex = static_cast<int>(window.start.date().daysTo(block.start.date()));
        if (dayIndex >= firstDay && dayIndex <= lastDay && !reusableBlockByDay.contains(dayIndex)) {
            reusableBlockByDay.insert(dayIndex, block.id);
            editableBlockIds.insert(block.id);
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
            return {{"ok", false}, {"message", QObject::tr("连续时间块存在冲突，请调整日期或时间")}};
        }

        TimeBlock probe;
        probe.taskId = m_selectedTaskId;
        probe.start = blockStart;
        probe.end = blockEnd;
        probe.source = locked ? BlockSource::Locked : BlockSource::UserDragged;
        otherBlocks.push_back(probe);
        planned.push_back({reusableBlockByDay.value(dayIndex, 0), blockStart, blockEnd});
    }

    int selectedBlockId = 0;
    for (const auto& block : planned) {
        int blockId = block.existingId;
        if (blockId > 0) {
            if (!m_blocks.moveBlock(blockId, block.start, block.end)
                || !m_blocks.setBlockSource(blockId, locked ? BlockSource::Locked : BlockSource::UserDragged)) {
                return {{"ok", false}, {"message", QObject::tr("连续时间块更新失败")}};
            }
        } else {
            TimeBlock created;
            created.taskId = m_selectedTaskId;
            created.start = block.start;
            created.end = block.end;
            created.source = locked ? BlockSource::Locked : BlockSource::UserDragged;
            blockId = m_blocks.createBlock(created);
            if (blockId <= 0) {
                return {{"ok", false}, {"message", QObject::tr("连续时间块创建失败")}};
            }
        }
        if (selectedBlockId == 0) {
            selectedBlockId = blockId;
        }
    }

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

    if (m_aiService) {
        QFuture<AIParseResult> future = m_aiService->parseNaturalLanguageTask(input);
        future.waitForFinished();
        const AIParseResult aiResult = future.result();
        if (aiResult.supported && !aiResult.taskDraft.isEmpty()) {
            QVariantMap draft = aiResult.taskDraft;
            draft.insert(QStringLiteral("source"), aiResult.provider.isEmpty() ? QStringLiteral("ai") : aiResult.provider);
            return {
                {"ok", true},
                {"message", aiResult.message},
                {"source", draft.value(QStringLiteral("source"))},
                {"draft", draft}
            };
        }
    }

    const ParsedTaskDraft draft = m_parser.parse(input);
    if (!draft.valid) {
        return {{"ok", false}, {"message", draft.message}};
    }
    return {
        {"ok", true},
        {"message", QObject::tr("已生成本地规则任务草稿，请确认后加入日程")},
        {"source", QStringLiteral("local")},
        {"draft", parsedDraftToMap(draft, QStringLiteral("local"), draft.message)}
    };
}

QVariantMap ScheduleService::createTaskFromDraft(const QVariantMap& draft)
{
    const QString title = draft.value(QStringLiteral("title")).toString().trimmed();
    QDateTime deadline = QDateTime::fromString(draft.value(QStringLiteral("deadline")).toString().trimmed(), QStringLiteral("yyyy-MM-dd HH:mm"));
    if (!deadline.isValid()) {
        deadline = QDateTime::fromString(draft.value(QStringLiteral("deadline")).toString().trimmed(), Qt::ISODate);
    }
    if (title.isEmpty() || !deadline.isValid()) {
        return {{"ok", false}, {"message", QObject::tr("任务草稿缺少标题或有效截止时间")}};
    }

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
    if (!m_tasks.createTask(task, categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("任务创建失败")}};
    }

    reschedule();
    return {
        {"ok", true},
        {"message", QObject::tr("任务已加入日程并重新排程")},
        {"title", task.title},
        {"deadline", deadlineText(task.deadline)},
        {"category", categoryName}
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
        item.subtitle = event.categoryName.isEmpty() ? QObject::tr("课程 / 固定时间") : event.categoryName;
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
        item.blockOrdinal = blockOrdinalsById.value(block.id, 1);
        item.blockTotal = blockTotalsByTask.value(block.taskId, 1);
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
        {"categoryColor", colorForPriority(task.priority, task.categoryColor)}
    };
}

QVariantMap ScheduleService::studyFrameToMap(const StudyFrame& frame) const
{
    const ScheduleWindow window = currentWindow();
    int dayIndex = -1;
    for (int i = 0; i < 7; ++i) {
        if (window.start.date().addDays(i).dayOfWeek() == frame.dayOfWeek) {
            dayIndex = i;
            break;
        }
    }

    const int startMinute = frame.startTime.hour() * 60 + frame.startTime.minute();
    const int endMinute = frame.endTime.hour() * 60 + frame.endTime.minute();
    return {
        {"id", frame.id},
        {"name", frame.name},
        {"dayIndex", dayIndex},
        {"dayOfWeek", frame.dayOfWeek},
        {"startText", frame.startTime.toString(QStringLiteral("HH:mm"))},
        {"endText", frame.endTime.toString(QStringLiteral("HH:mm"))},
        {"startMinute", startMinute},
        {"durationMinutes", qMax(0, endMinute - startMinute)},
        {"categoryName", frame.categoryName},
        {"energyLevel", frame.energyLevel},
        {"enabled", frame.enabled},
        {"color", frame.categoryColor.isEmpty() ? QStringLiteral("#7C8CFF") : frame.categoryColor}
    };
}

QVariantMap ScheduleService::eventToDetailMap(const TimelineItem& item) const
{
    return {
        {"kind", QStringLiteral("event")},
        {"id", qAbs(item.id)},
        {"blockId", item.id},
        {"title", item.title},
        {"notes", QString()},
        {"deadlineText", deadlineText(item.end)},
        {"priority", 1},
        {"categoryName", item.subtitle == QObject::tr("课程 / 固定时间") ? QObject::tr("课程") : item.subtitle},
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
        {"blockOrdinal", 1},
        {"blockTotal", 1}
    };
}
