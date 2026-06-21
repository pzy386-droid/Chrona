#include "app/ScheduleService.h"
#include "core/scheduler/CascadePlanner.h"

#include <QFuture>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QSet>
#include <QMap>
#include <QtGlobal>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <utility>

namespace {
QString colorForPriority(Priority priority, const QString& categoryColor)
{
    if (priority == Priority::High) {
        return QStringLiteral("#EF4444");
    }
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

QVariantMap focusItemMap(const TimelineItem& item, const QDateTime& now)
{
    const qint64 startMs = item.start.toMSecsSinceEpoch();
    const qint64 endMs = item.end.toMSecsSinceEpoch();
    return {
        {"title", item.title},
        {"subtitle", focusSubtitle(item)},
        {"color", item.color},
        {"taskId", item.taskId},
        {"blockId", item.id},
        {"startText", item.start.toString(QStringLiteral("yyyy-MM-dd HH:mm"))},
        {"endText", item.end.toString(QStringLiteral("yyyy-MM-dd HH:mm"))},
        {"startMs", startMs},
        {"endMs", endMs},
        {"durationSeconds", qMax<qint64>(60, item.start.secsTo(item.end))},
        {"remainingSeconds", qMax<qint64>(0, now.secsTo(item.end))},
        {"canStart", now >= item.start && now < item.end},
        {"canComplete", now >= item.end},
        {"isCurrent", item.start <= now && now < item.end},
        {"isPast", now >= item.end}
    };
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
    title.remove(QRegularExpression(QStringLiteral("^(请|请帮我|帮我安排|帮我|安排|创建|新建|加一个|添加)")));
    return title.trimmed().isEmpty() ? input.trimmed() : title.trimmed();
}

void appendBusyInterval(QMap<QDate, QVector<QPair<int, int>>>& intervals,
                        const QDateTime& start, const QDateTime& end,
                        const ScheduleWindow& window)
{
    if (!start.isValid() || !end.isValid() || end <= start) {
        return;
    }
    const QDateTime clippedStart = qMax(start, window.start);
    const QDateTime clippedEnd = qMin(end, window.end);
    if (clippedEnd <= clippedStart) {
        return;
    }

    for (QDate date = clippedStart.date(); date <= clippedEnd.date(); date = date.addDays(1)) {
        const QDateTime dayStart(date, QTime(0, 0));
        const QDateTime dayEnd(date.addDays(1), QTime(0, 0));
        const QDateTime segmentStart = qMax(clippedStart, dayStart);
        const QDateTime segmentEnd = qMin(clippedEnd, dayEnd);
        if (segmentEnd <= segmentStart) {
            continue;
        }
        const int startMinute = static_cast<int>(dayStart.secsTo(segmentStart) / 60);
        const int endMinute = static_cast<int>(dayStart.secsTo(segmentEnd) / 60);
        intervals[date].push_back({startMinute, endMinute});
    }
}

int mergedBusyMinutes(QVector<QPair<int, int>> intervals)
{
    if (intervals.isEmpty()) {
        return 0;
    }
    std::sort(intervals.begin(), intervals.end(), [](const auto& left, const auto& right) {
        return left.first < right.first || (left.first == right.first && left.second < right.second);
    });

    int total = 0;
    int currentStart = intervals.first().first;
    int currentEnd = intervals.first().second;
    for (int i = 1; i < intervals.size(); ++i) {
        if (intervals.at(i).first <= currentEnd) {
            currentEnd = qMax(currentEnd, intervals.at(i).second);
        } else {
            total += currentEnd - currentStart;
            currentStart = intervals.at(i).first;
            currentEnd = intervals.at(i).second;
        }
    }
    return total + currentEnd - currentStart;
}
}

ScheduleService::ScheduleService(TaskRepository tasks, CalendarRepository calendar, TimeBlockRepository blocks,
                                 StudyFrameRepository studyFrames, SettingsRepository settings, BackupService backup,
                                 AIService* aiService, QObject* parent)
    : QObject(parent)
    , m_tasks(std::move(tasks))
    , m_calendar(std::move(calendar))
    , m_blocks(std::move(blocks))
    , m_studyFrames(std::move(studyFrames))
    , m_settings(std::move(settings))
    , m_backup(std::move(backup))
    , m_aiService(aiService)
{
    m_selectedDate = QDate::currentDate();
    m_lastObservedDate = m_selectedDate;
    m_followToday = true;
    m_capacityRefreshTimer.setInterval(60 * 1000);
    connect(&m_capacityRefreshTimer, &QTimer::timeout, this, [this] {
        const QDate today = QDate::currentDate();
        const bool dateChanged = today != m_lastObservedDate;
        m_lastObservedDate = today;
        if (dateChanged && m_followToday && m_selectedDate != today) {
            m_selectedDate = today;
            m_selectedTaskId = 0;
            m_selectedBlockId = 0;
            reload();
            emit selectedDateChanged();
            return;
        }
        reload();
    });
    m_capacityRefreshTimer.start();
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
        {QStringLiteral("label"), configured ? QObject::tr("Chrona AI 已连接") : QObject::tr("本地规则模式")},
        {QStringLiteral("hint"), configured ? QObject::tr("自然语言输入将优先由 Chrona AI 解析") : QObject::tr("点击 AI 状态连接 Chrona AI，未连接时使用本地规则")}
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
        const QDateTime now = QDateTime::currentDateTime();
        const bool hasExplicitBlockSelection = m_selectedBlockId > 0;
        map.insert(QStringLiteral("blockId"), block->id);
        map.insert(QStringLiteral("blockDayIndex"), block->dayIndex);
        map.insert(QStringLiteral("blockEndDayIndex"), block->dayIndex + qMax(1, block->spanDays) - 1);
        map.insert(QStringLiteral("blockStartText"), block->start.time().toString(QStringLiteral("HH:mm")));
        map.insert(QStringLiteral("blockEndText"), block->end.time().toString(QStringLiteral("HH:mm")));
        map.insert(QStringLiteral("blockStartMs"), block->start.toMSecsSinceEpoch());
        map.insert(QStringLiteral("blockEndMs"), block->end.toMSecsSinceEpoch());
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
        map.insert(QStringLiteral("blockIsCurrent"), block->start <= now && now < block->end);
        map.insert(QStringLiteral("blockIsPast"), now >= block->end);
        map.insert(QStringLiteral("blockCompleted"), block->completed);
        map.insert(QStringLiteral("canCompleteBlock"), hasExplicitBlockSelection && !block->completed && now >= block->end);
        map.insert(QStringLiteral("canUndoCompleteBlock"), hasExplicitBlockSelection && block->completed);
    }
    return map;
}

QVariantMap ScheduleService::selectedDetail() const
{
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

QVariantList ScheduleService::dailyItems() const
{
    QVariantList result;
    result.reserve(m_dailyItems.size());
    for (const QVariant& value : m_dailyItems) {
        QVariantMap item = value.toMap();
        const int itemId = item.value(QStringLiteral("id")).toInt();
        const int taskId = item.value(QStringLiteral("taskId")).toInt();
        item.insert(QStringLiteral("selected"),
                    itemId == m_selectedBlockId
                        || (m_selectedBlockId == 0 && taskId > 0 && taskId == m_selectedTaskId));
        result.push_back(item);
    }
    return result;
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
    const QDateTime coverageStart(QDate::currentDate().addYears(-1), QTime(0, 0));
    const QDateTime coverageEnd = window.end.addYears(1);
    const QVector<TimeBlock> manualCoverage = m_blocks.lockedBlocksBetween(coverageStart, coverageEnd);
    const QVector<StudyFrame> frames = m_studyFrames.enabledFrames();
    normalizeManualBlockBackedTasks(window, locked);
    QVector<int> manuallyCoveredTaskIds;
    const QVector<Task> tasks = subtractLockedBlockCapacity(m_tasks.activeTasks(), manualCoverage, &manuallyCoveredTaskIds);

    const ScheduleResult result = m_scheduler.generateSchedule(tasks, events, locked, frames, window, m_config);
    QVector<int> scheduledTaskIds = result.scheduledTaskIds;
    appendUniqueIds(scheduledTaskIds, manuallyCoveredTaskIds);
    if (!m_blocks.commitSchedule(window, result.generatedBlocks, scheduledTaskIds,
                                 result.unscheduledTaskIds, QStringLiteral("manual_or_startup"))) {
        m_persistenceError = m_blocks.lastError();
        reload();
        return {{"ok", false}, {"message", QObject::tr("重新规划保存失败：%1").arg(m_persistenceError)}};
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
        return {{"ok", false}, {"message", QObject::tr("当前没有可开始的任务")}};
    }
    const qint64 startMs = m_focusItem.value(QStringLiteral("startMs")).toLongLong();
    const qint64 endMs = m_focusItem.value(QStringLiteral("endMs")).toLongLong();
    const qint64 nowMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
    if (startMs > 0 && nowMs < startMs) {
        return {{"ok", false}, {"message", QObject::tr("这个时间块还没开始")}};
    }
    if (endMs > 0 && nowMs >= endMs) {
        return {{"ok", false}, {"message", QObject::tr("这个时间块已经结束，可以直接标记完成")}};
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

    const auto item = m_timelineModel.itemById(blockId);
    if (item && QDateTime::currentDateTime() < item->end) {
        return {{"ok", false}, {"message", QObject::tr("时间块还没有完整经过，暂时不能标记完成")}};
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
    return {{"ok", true}, {"message", QObject::tr("已完成当前时间块")}};
}

QVariantMap ScheduleService::reopenBlock(int blockId)
{
    if (blockId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("当前没有可取消完成的时间块")}};
    }

    if (!m_blocks.setBlockCompleted(blockId, false)) {
        return {{"ok", false}, {"message", QObject::tr("取消完成失败")}};
    }

    const int selectedTaskId = m_selectedTaskId;
    m_selectedBlockId = blockId;
    reload();
    if (m_timelineModel.itemById(blockId)) {
        selectTimelineItem(selectedTaskId, blockId);
    } else if (selectedTaskId > 0) {
        selectTask(selectedTaskId);
    }
    return {{"ok", true}, {"message", QObject::tr("已取消完成当前时间块")}};
}

QVariantMap ScheduleService::updateTask(int taskId, const QString& title, const QString& notes, const QString& deadlineInput, int estimatedMinutes, int priority,
                                        const QString& categoryName, const QString& categoryColor, const QString& preferredStudyTime, bool autoScheduleEnabled, int deadlineType,
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
    task.colorOverride = task.priority == Priority::High
        ? QStringLiteral("#EF4444")
        : categoryColor.trimmed();
    task.categoryColor = task.colorOverride;
    task.preferredStudyTime = preferredStudyTime.trimmed().isEmpty() ? QStringLiteral("evening") : preferredStudyTime.trimmed();

    if (!m_tasks.updateTask(task, categoryName)) {
        return {{"ok", false}, {"message", QObject::tr("任务更新失败：%1").arg(m_tasks.lastError())}};
    }

    const int previousBlockId = m_selectedBlockId;
    if (task.deadlineType == DeadlineType::Hard && previousBlockId > 0
        && !m_blocks.setBlockSource(previousBlockId, BlockSource::Locked)) {
        return {{"ok", false}, {"message", QObject::tr("任务已更新，但硬截止时间块固定失败：%1").arg(m_blocks.lastError())}};
    }
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
    m_followToday = date == QDate::currentDate();
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

QVariantMap ScheduleService::goToToday()
{
    const QDate today = QDate::currentDate();
    m_followToday = true;
    m_lastObservedDate = today;
    if (m_selectedDate == today) {
        return {{"ok", true}, {"dateText", selectedDateText()}};
    }
    m_selectedDate = today;
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
            {"message", QObject::tr("分类颜色更新失败：%1").arg(m_tasks.lastError())}
        };
    }
    reload();
    return {{"ok", true}, {"message", QObject::tr("分类颜色已更新")}};
}

QVariantMap ScheduleService::createScheduledTask(const QString& title, int dayOffset, int startMinute, int durationMinutes, bool locked, const QString& categoryName, const QString& categoryColor)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请输入时间块标题")}};
    }
    if (dayOffset < 0 || dayOffset > 6 || startMinute < 0 || startMinute >= 24 * 60 || durationMinutes <= 0 || startMinute + durationMinutes > 24 * 60) {
        return {{"ok", false}, {"message", QObject::tr("时间块范围无效")}};
    }

    const ScheduleWindow window = displayWindow();
    const QDateTime start(window.start.date().addDays(dayOffset), QTime(0, 0).addSecs(startMinute * 60));
    const QDateTime end = start.addSecs(durationMinutes * 60);
    if (!m_conflicts.canPlace({start, end}, {}, m_blocks.blocksBetween(window.start, window.end))) {
        return {{"ok", false}, {"message", QObject::tr("这个时间段已有安排")}};
    }

    Task task;
    task.title = trimmedTitle;
    task.notes = QObject::tr("手动创建的完整时间块");
    task.startDate = QDateTime(start.date(), QTime(0, 0));
    task.deadline = end;
    task.deadlineType = DeadlineType::Hard;
    task.estimatedMinutes = durationMinutes;
    task.estimatedHours = durationMinutes / 60.0;
    task.remainingMinutes = durationMinutes;
    task.priority = Priority::Medium;
    task.status = TaskStatus::Scheduled;
    task.preferredStudyTime = QStringLiteral("evening");
    task.autoScheduleEnabled = false;
    task.minChunkMinutes = qMin(60, durationMinutes);
    task.idealChunkMinutes = durationMinutes;
    task.effortLevel = 1;
    task.colorOverride = categoryColor.trimmed();
    task.categoryColor = task.colorOverride;

    const int taskId = m_tasks.createTaskReturningId(task, categoryName);
    if (taskId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("任务创建失败：%1").arg(m_tasks.lastError())}};
    }

    TimeBlock block;
    block.taskId = taskId;
    block.start = start;
    block.end = end;
    block.source = locked ? BlockSource::Locked : BlockSource::UserDragged;
    block.explanation = QObject::tr("手动创建");
    const int blockId = m_blocks.createBlock(block);
    if (blockId <= 0) {
        m_tasks.deleteTask(taskId);
        return {{"ok", false}, {"message", QObject::tr("时间块创建失败")}};
    }

    reschedule();
    selectTimelineItem(taskId, blockId);
    return {
        {"ok", true},
        {"message", QObject::tr("完整时间块已创建")},
        {"taskId", taskId},
        {"blockId", blockId}
    };
}

bool ScheduleService::setCategoryColor(const QString& categoryName, const QString& color)
{
    return m_calendar.setCategoryColor(categoryName, color);
}

bool ScheduleService::moveBlock(int blockId, int dayIndex, int startMinute, int durationMinutes)
{
    if (blockId <= 0 || dayIndex < 0 || dayIndex > 6 || durationMinutes <= 0) {
        return false;
    }

    const ScheduleWindow window = displayWindow();
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

QVariantMap ScheduleService::moveTimelineItem(int itemId, int dayIndex, int startMinute, int durationMinutes)
{
    if (itemId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("当前时间块无效，请刷新后重试")}};
    }
    const auto item = m_timelineModel.itemById(itemId);
    if (!item) {
        reload();
        return {{"ok", false}, {"message", QObject::tr("当前时间块已变化，请重试")}};
    }
    if (item->locked) {
        return {{"ok", false}, {"message", QObject::tr("该时间块已固定，请先取消固定")}};
    }
    if (item && item->spanDays > 1) {
        const int lastDay = dayIndex + item->spanDays - 1;
        if (lastDay > 6) {
            reload();
            return {{"ok", false}, {"message", QObject::tr("跨日时间块不能超出当前周")}};
        }
        const QTime start = QTime(0, 0).addSecs(startMinute * 60);
        const QTime end = QTime(0, 0).addSecs((startMinute + durationMinutes) * 60);
        m_selectedTaskId = item->taskId;
        m_selectedBlockId = itemId;
        const QVariantMap result = scheduleSelectedTaskBlocks(
            dayIndex, lastDay, start.toString(QStringLiteral("HH:mm")),
            end.toString(QStringLiteral("HH:mm")), item->locked);
        return result;
    }

    if (dayIndex < 0 || dayIndex > 6 || startMinute < 0 || durationMinutes <= 0
        || startMinute + durationMinutes > 24 * 60) {
        reload();
        return {{"ok", false}, {"message", QObject::tr("目标位置超出当天范围")}};
    }

    const ScheduleWindow window = displayWindow();
    const QDateTime start(window.start.date().addDays(dayIndex), QTime(0, 0).addSecs(startMinute * 60));
    const QDateTime end = start.addSecs(durationMinutes * 60);
    QVector<TimeBlock> otherBlocks = m_blocks.blocksBetween(window.start, window.end);
    const auto original = std::find_if(otherBlocks.cbegin(), otherBlocks.cend(), [itemId](const TimeBlock& block) {
        return block.id == itemId;
    });
    if (original == otherBlocks.cend()) {
        reload();
        return {{"ok", false}, {"message", QObject::tr("当前时间块不在显示日期范围内")}};
    }
    otherBlocks.erase(std::remove_if(otherBlocks.begin(), otherBlocks.end(), [itemId](const TimeBlock& block) {
        return block.id == itemId;
    }), otherBlocks.end());

    if (!m_conflicts.canPlace({start, end}, {}, otherBlocks)) {
        reload();
        return {{"ok", false}, {"message", QObject::tr("目标位置已有时间块，请选择真正空白的时段")}};
    }
    if (!m_blocks.moveBlock(itemId, start, end)) {
        reload();
        return {{"ok", false}, {"message", QObject::tr("时间块移动保存失败：%1").arg(m_blocks.lastError())}};
    }

    const int taskId = item->taskId;
    reschedule();
    selectTimelineItem(taskId, itemId);
    return {{"ok", true}, {"message", QObject::tr("时间块已移动")}};
}


QVariantMap ScheduleService::resizeTimelineItemTime(int itemId, int startMinute, int durationMinutes)
{
    const auto item = m_timelineModel.itemById(itemId);
    if (!item) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个时间块")}};
    }
    if (item->locked || item->completed) {
        return {{"ok", false}, {"message", QObject::tr("固定或已完成的时间块不能调整")}};
    }
    if (itemId <= 0 || startMinute < 0 || durationMinutes <= 0
        || startMinute + durationMinutes > 24 * 60) {
        return {{"ok", false}, {"message", QObject::tr("时间范围无效")}};
    }

    const ScheduleWindow window = displayWindow();
    const int firstDay = qBound(0, item->dayIndex, 6);
    const int lastDay = qBound(firstDay, item->dayIndex + qMax(1, item->spanDays) - 1, 6);
    const QVector<TimeBlock> allBlocks = m_blocks.blocksBetween(window.start, window.end);
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);

    QHash<int, TimeBlock> selectedByDay;
    QSet<int> selectedIds;
    for (const TimeBlock& block : allBlocks) {
        if (block.taskId != item->taskId
            || block.start.time() != item->start.time()
            || block.end.time() != item->end.time()) {
            continue;
        }
        const int day = static_cast<int>(window.start.date().daysTo(block.start.date()));
        if (day < firstDay || day > lastDay) {
            continue;
        }
        selectedByDay.insert(day, block);
        selectedIds.insert(block.id);
    }
    if (!selectedIds.contains(itemId) || selectedByDay.size() != lastDay - firstDay + 1) {
        reload();
        return {{"ok", false}, {"message", QObject::tr("时间块系列已经变化，请刷新后重试")}};
    }

    QVector<TimeBlock> blocksToSave;
    int pushedCount = 0;

    for (int day = firstDay; day <= lastDay; ++day) {
        const QDate date = window.start.date().addDays(day);
        const QDateTime dayStart(date, QTime(0, 0));
        const QDateTime dayEnd(date.addDays(1), QTime(0, 0));
        const QDateTime resizedStart = dayStart.addSecs(startMinute * 60);
        const QDateTime resizedEnd = resizedStart.addSecs(durationMinutes * 60);

        QVector<TimeInterval> fixed;
        for (const CalendarEvent& event : events) {
            if (event.start < dayEnd && dayStart < event.end) {
                fixed.push_back({qMax(event.start, dayStart), qMin(event.end, dayEnd)});
            }
        }

        QVector<TimeBlock> movable;
        for (const TimeBlock& block : allBlocks) {
            if (selectedIds.contains(block.id) || block.start.date() != date) {
                continue;
            }
            if (block.source == BlockSource::Locked || block.completedAt.isValid()) {
                fixed.push_back({block.start, block.end});
            } else {
                movable.push_back(block);
            }
        }
        TimeBlock resized = selectedByDay.value(day);
        const bool expandingUp = resizedStart < resized.start;
        const bool expandingDown = resizedEnd > resized.end;
        resized.start = resizedStart;
        resized.end = resizedEnd;
        resized.source = BlockSource::UserDragged;
        blocksToSave.push_back(resized);

        CascadePlanResult cascade;
        if (expandingUp) {
            cascade = planCascadeUp(resizedStart, resizedEnd, dayStart, movable, fixed);
        } else if (expandingDown) {
            cascade = planCascadeDown(resizedStart, resizedEnd, dayEnd, movable, fixed);
        } else {
            cascade.ok = true;
        }
        if (!cascade.ok) {
            reload();
            QString message;
            if (cascade.error == QStringLiteral("fixed-conflict")) {
                message = QObject::tr("拉伸范围碰到了固定时间块，无法自动挪动");
            } else if (cascade.error == QStringLiteral("day-underflow")) {
                message = QObject::tr("上方空间不足，无法保持所有时间块的原始长度");
            } else {
                message = QObject::tr("下方空间不足，无法保持所有时间块的原始长度");
            }
            return {{"ok", false}, {"message", message}};
        }
        for (TimeBlock block : cascade.movedBlocks) {
            block.source = BlockSource::UserDragged;
            blocksToSave.push_back(block);
            ++pushedCount;
        }
    }

    if (!m_blocks.upsertBlocks(blocksToSave)) {
        reload();
        return {{"ok", false}, {"message", QObject::tr("级联调整保存失败：%1").arg(m_blocks.lastError())}};
    }

    const int selectedTaskId = item->taskId;
    m_selectedTaskId = selectedTaskId;
    m_selectedBlockId = itemId;
    reschedule();
    selectTimelineItem(selectedTaskId, itemId);
    return {
        {"ok", true},
        {"pushedCount", pushedCount},
        {"message", pushedCount > 0
            ? QObject::tr("已调整时间块，并联动移动 %1 个冲突时间块").arg(pushedCount)
            : QObject::tr("时间块长度已调整")}
    };
}

QVariantMap ScheduleService::stretchTimelineItemDays(int itemId, int endDayIndex)
{
    const auto item = m_timelineModel.itemById(itemId);
    if (!item) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个时间块")}};
    }
    return resizeTimelineItemDays(itemId, item->dayIndex, endDayIndex);
}

QVariantMap ScheduleService::resizeTimelineItemDays(int itemId, int startDayIndex, int endDayIndex)
{
    const auto item = m_timelineModel.itemById(itemId);
    if (!item) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个时间块")}};
    }

    const int firstDay = qBound(0, qMin(startDayIndex, endDayIndex), 6);
    const int lastDay = qBound(firstDay, qMax(startDayIndex, endDayIndex), 6);
    const QTime start = QTime(0, 0).addSecs(item->startMinute * 60);
    const QTime end = QTime(0, 0).addSecs((item->startMinute + item->durationMinutes) * 60);
    const QString startText = start.toString(QStringLiteral("HH:mm"));
    const QString endText = end.toString(QStringLiteral("HH:mm"));

    if (itemId <= 0) {
        return {{"ok", false}, {"message", QObject::tr("旧日程块已停用")}};
    }
    m_selectedTaskId = item->taskId;
    m_selectedBlockId = itemId;
    QScopedValueRollback<bool> relocationGuard(m_relocateHorizontalConflicts, true);
    return scheduleSelectedTaskBlocks(firstDay, lastDay, startText, endText, item->locked);
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
        const ScheduleWindow window = displayWindow();
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

    const ScheduleWindow window = displayWindow();
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
        const QTime originalStartTime = selectedBlock.start.time();
        const QTime originalEndTime = selectedBlock.end.time();
        QVector<TimeBlock> previousSeriesCandidates;
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
            previousSeriesCandidates.push_back(block);
        }
        std::sort(previousSeriesCandidates.begin(), previousSeriesCandidates.end(), [](const TimeBlock& a, const TimeBlock& b) {
            return a.start < b.start;
        });
        int selectedSeriesIndex = -1;
        for (int i = 0; i < previousSeriesCandidates.size(); ++i) {
            if (previousSeriesCandidates.at(i).id == selectedBlock.id) {
                selectedSeriesIndex = i;
                break;
            }
        }
        if (selectedSeriesIndex >= 0) {
            int runStart = selectedSeriesIndex;
            int runEnd = selectedSeriesIndex;
            while (runStart > 0
                   && previousSeriesCandidates.at(runStart - 1).start.date().addDays(1)
                       == previousSeriesCandidates.at(runStart).start.date()) {
                --runStart;
            }
            while (runEnd + 1 < previousSeriesCandidates.size()
                   && previousSeriesCandidates.at(runEnd).start.date().addDays(1)
                       == previousSeriesCandidates.at(runEnd + 1).start.date()) {
                ++runEnd;
            }
            for (int i = runStart; i <= runEnd; ++i) {
                previousSeriesBlockIds.insert(previousSeriesCandidates.at(i).id);
            }
        } else {
            previousSeriesBlockIds.insert(selectedBlock.id);
        }
    }

    QSet<int> editableBlockIds;
    editableBlockIds.unite(previousSeriesBlockIds);
    int originalFirstDay = 7;
    int originalLastDay = -1;
    for (const auto& block : allBlocks) {
        if (!previousSeriesBlockIds.contains(block.id)) {
            continue;
        }
        const int dayIndex = static_cast<int>(window.start.date().daysTo(block.start.date()));
        originalFirstDay = qMin(originalFirstDay, dayIndex);
        originalLastDay = qMax(originalLastDay, dayIndex);
    }
    if (!selectedBlockFound) {
        // A manual placement replaces auto-generated blocks for the same task.
        // Otherwise updateTask() can schedule the task first and its own auto
        // block will make the requested manual slot look like a conflict.
        for (const auto& block : allBlocks) {
            if (block.taskId == m_selectedTaskId && block.source == BlockSource::Auto) {
                editableBlockIds.insert(block.id);
            }
        }
    }
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
    QHash<int, TimeBlock> relocatedById;
    planned.reserve(lastDay - firstDay + 1);
    for (int dayIndex = firstDay; dayIndex <= lastDay; ++dayIndex) {
        const QDate targetDate = window.start.date().addDays(dayIndex);
        const QDateTime blockStart(targetDate, start);
        const QDateTime blockEnd(targetDate, end);
        if (blockStart < window.start || blockEnd > window.end || blockEnd <= blockStart) {
            return {{"ok", false}, {"message", QObject::tr("时间块超出当前排程范围")}};
        }
        if (!m_conflicts.canPlace({blockStart, blockEnd}, events, otherBlocks)) {
            const bool addedDay = m_relocateHorizontalConflicts
                && selectedBlockFound
                && (dayIndex < originalFirstDay || dayIndex > originalLastDay);
            if (!addedDay) {
                return {{"ok", false}, {"message", QObject::tr("连续时间块存在冲突，请调整日期或时间")}};
            }

            const QDateTime dayStart(targetDate, QTime(0, 0));
            const QDateTime dayEnd(targetDate.addDays(1), QTime(0, 0));
            QVector<TimeInterval> fixed;
            for (const CalendarEvent& event : events) {
                if (event.start < dayEnd && dayStart < event.end) {
                    fixed.push_back({qMax(event.start, dayStart), qMin(event.end, dayEnd)});
                }
            }
            QVector<TimeBlock> movable;
            for (const TimeBlock& block : otherBlocks) {
                if (block.start.date() != targetDate) {
                    continue;
                }
                if (block.source == BlockSource::Locked || block.completedAt.isValid()) {
                    fixed.push_back({block.start, block.end});
                } else {
                    movable.push_back(block);
                }
            }

            const bool preferUp = dayIndex < originalFirstDay;
            const CascadePlanResult relocation = planHorizontalRelocation(
                blockStart, blockEnd, dayStart, dayEnd, movable, fixed, preferUp);
            if (!relocation.ok) {
                const QString message = relocation.error == QStringLiteral("fixed-conflict")
                    ? QObject::tr("横向延伸碰到了固定时间块，无法自动避让")
                    : QObject::tr("当天没有足够的完整空档容纳冲突时间块");
                return {{"ok", false}, {"message", message}};
            }

            for (TimeBlock moved : relocation.movedBlocks) {
                moved.source = BlockSource::UserDragged;
                relocatedById.insert(moved.id, moved);
                otherBlocks.erase(std::remove_if(otherBlocks.begin(), otherBlocks.end(), [&moved](const TimeBlock& block) {
                    return block.id == moved.id;
                }), otherBlocks.end());
                otherBlocks.push_back(moved);
            }
            if (!m_conflicts.canPlace({blockStart, blockEnd}, events, otherBlocks)) {
                return {{"ok", false}, {"message", QObject::tr("自动避让后仍存在冲突，已取消本次延伸")}};
            }
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
    for (const TimeBlock& moved : relocatedById) {
        blocksToSave.push_back(moved);
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
            {"message", QObject::tr("连续时间块保存失败：%1").arg(m_blocks.lastError())}
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

    return {
        {"ok", true},
        {"movedCount", relocatedById.size()},
        {"message", relocatedById.isEmpty()
            ? QObject::tr("已安排 %1 天连续时间块").arg(lastDay - firstDay + 1)
            : QObject::tr("已横向延伸，并自动避让 %1 个冲突时间块").arg(relocatedById.size())}
    };
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
                && (planningMode == QStringLiteral("direct_time_block") || draft.value(QStringLiteral("hasTimeAnchor")).toBool())) {
                draft.insert(QStringLiteral("planningMode"), QStringLiteral("direct_time_block"));
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
        aiFallbackMessage = QObject::tr("尚未连接 Chrona AI");
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
    task.autoScheduleEnabled = !hasTimeAnchor;
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
        return {{"ok", false}, {"message", QObject::tr("Key 格式看起来不对，请粘贴 sk- 开头的 Chrona AI Key")}};
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
        {"message", normalized.isEmpty() ? QObject::tr("已切换为本地规则模式") : QObject::tr("Chrona AI 已连接，Quick Add 将优先使用 AI 解析")}
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
            {QStringLiteral("preferredStudyTime"), task.preferredStudyTime},
            {QStringLiteral("deadlineType"), static_cast<int>(task.deadlineType)},
            {QStringLiteral("effortLevel"), task.effortLevel},
            {QStringLiteral("autoScheduleEnabled"), task.autoScheduleEnabled}
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
        : (mode == QStringLiteral("daily_plan") ? QObject::tr("Chrona 今日计划") : QObject::tr("日程调整建议"));
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
                QStringLiteral("review_task"),
                QStringLiteral("focus_buffer")
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

void ScheduleService::requestSchedulePlan(const QString& mode)
{
    const QString normalizedMode = mode.trimmed().isEmpty() ? QStringLiteral("daily_plan") : mode.trimmed();
    const ScheduleContext context = buildAIContext(normalizedMode);
    const QVariantMap fallback = localAIPlan(normalizedMode);
    AIService* aiService = m_aiService;
    const bool configured = aiService && aiService->isConfigured();
    QPointer<ScheduleService> self(this);

    auto future = QtConcurrent::run([self, aiService, configured, context, fallback, normalizedMode] {
        QVariantMap plan = fallback;
        if (configured && aiService) {
            QFuture<AISuggestionResult> aiFuture = aiService->suggestScheduleChanges(context);
            aiFuture.waitForFinished();
            const AISuggestionResult aiResult = aiFuture.result();
            if (aiResult.supported && !aiResult.suggestion.isEmpty()) {
                QVariantMap aiPlan = aiResult.suggestion;
                QVariantList validSuggestions;
                const QSet<QString> allowed{
                    QStringLiteral("reschedule"),
                    QStringLiteral("extend_deadline"),
                    QStringLiteral("review_task"),
                    QStringLiteral("focus_buffer")
                };
                for (const QVariant& value : aiPlan.value(QStringLiteral("suggestions")).toList()) {
                    const QVariantMap suggestion = value.toMap();
                    if (allowed.contains(suggestion.value(QStringLiteral("actionType")).toString())) {
                        validSuggestions.push_back(suggestion);
                    }
                }
                aiPlan.insert(QStringLiteral("suggestions"), validSuggestions);
                aiPlan.insert(QStringLiteral("ok"), true);
                aiPlan.insert(QStringLiteral("provider"), aiResult.provider);
                plan = aiPlan;
            }
        }

        QMetaObject::invokeMethod(self.data(), [self, normalizedMode, plan] {
            if (self) {
                emit self->schedulePlanReady(normalizedMode, plan);
            }
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future);
}

QVariantMap ScheduleService::explainSelectedSchedule()
{
    if (selectedDetail().isEmpty()) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个任务或时间块")}};
    }
    return requestAIPlan(QStringLiteral("explain_schedule"));
}

QVariantMap ScheduleService::previewSelectedFocusBuffer()
{
    const auto item = m_timelineModel.itemById(m_selectedBlockId);
    const auto selectedTask = m_taskModel.taskById(m_selectedTaskId);
    if (!item || !selectedTask) {
        return {{"ok", false}, {"message", QObject::tr("请先选择一个时间块")}};
    }
    if (selectedTask->effortLevel < 2) {
        return {{"ok", false}, {"message", QObject::tr("当前任务不是重强度任务")}};
    }

    const ScheduleWindow window = displayWindow();
    const QVector<TimeBlock> allBlocks = m_blocks.blocksBetween(window.start, window.end);
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<Task> tasks = m_tasks.allTasks();
    QHash<int, Task> taskById;
    for (const Task& task : tasks) {
        taskById.insert(task.id, task);
    }

    TimeBlock selectedBlock;
    bool found = false;
    for (const TimeBlock& block : allBlocks) {
        if (block.id == item->id) {
            selectedBlock = block;
            found = true;
            break;
        }
    }
    if (!found) {
        return {{"ok", false}, {"message", QObject::tr("当前时间块已经变化，请刷新后重试")}};
    }

    struct Candidate {
        bool ok = false;
        bool fixedConflict = false;
        QDateTime start;
        QDateTime end;
        CascadePlanResult relocation;
    };
    auto evaluate = [&](const QDateTime& start) {
        Candidate candidate;
        candidate.start = start;
        candidate.end = start.addSecs(selectedBlock.start.secsTo(selectedBlock.end));
        const QDateTime dayStart(start.date(), QTime(0, 0));
        const QDateTime dayEnd(start.date().addDays(1), QTime(0, 0));
        const QDateTime bufferStart = start.addSecs(-60 * 60);
        const QDateTime bufferEnd = candidate.end.addSecs(60 * 60);
        if (bufferStart < dayStart || bufferEnd > dayEnd) {
            return candidate;
        }

        QVector<TimeInterval> fixed;
        QVector<TimeBlock> movable;
        for (const CalendarEvent& event : events) {
            if (event.start < dayEnd && dayStart < event.end) {
                fixed.push_back({qMax(event.start, dayStart), qMin(event.end, dayEnd)});
            }
        }
        for (const TimeBlock& block : allBlocks) {
            if (block.id == selectedBlock.id || block.start.date() != start.date()) {
                continue;
            }
            const bool hardTask = taskById.contains(block.taskId)
                && taskById.value(block.taskId).deadlineType == DeadlineType::Hard;
            if (block.source == BlockSource::Locked || block.completedAt.isValid() || hardTask) {
                fixed.push_back({block.start, block.end});
            } else {
                movable.push_back(block);
            }
        }

        candidate.relocation = planHorizontalRelocation(
            bufferStart, bufferEnd, dayStart, dayEnd, movable, fixed, false);
        candidate.fixedConflict = candidate.relocation.error == QStringLiteral("fixed-conflict");
        candidate.ok = candidate.relocation.ok;
        return candidate;
    };

    Candidate best = evaluate(selectedBlock.start);
    bool movedForHardConstraint = false;
    if (!best.ok) {
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        for (int day = 0; day < 7; ++day) {
            const QDate date = window.start.date().addDays(day);
            for (int minute = 60; minute + static_cast<int>(selectedBlock.start.secsTo(selectedBlock.end) / 60) + 60 <= 24 * 60; minute += 15) {
                const Candidate candidate = evaluate(QDateTime(date, QTime(0, 0).addSecs(minute * 60)));
                if (!candidate.ok) {
                    continue;
                }
                const qint64 distance = qAbs(selectedBlock.start.secsTo(candidate.start));
                if (distance < bestDistance) {
                    best = candidate;
                    bestDistance = distance;
                }
            }
        }
        movedForHardConstraint = best.ok;
    }
    if (!best.ok) {
        return {
            {"ok", false},
            {"message", QObject::tr("本周没有能够保留前后各 1 小时缓冲的完整时段")}
        };
    }

    QString aiExplanation;
    QString provider = QStringLiteral("local");
    if (m_aiService && m_aiService->isConfigured()) {
        QFuture<AISuggestionResult> future = m_aiService->suggestScheduleChanges(buildAIContext(QStringLiteral("focus_buffer")));
        future.waitForFinished();
        const AISuggestionResult aiResult = future.result();
        if (aiResult.supported) {
            aiExplanation = aiResult.suggestion.value(QStringLiteral("summary")).toString();
            provider = aiResult.provider;
        }
    }

    const QString proposedRange = QStringLiteral("%1 - %2")
        .arg(best.start.toString(QStringLiteral("MM-dd HH:mm")),
             best.end.toString(QStringLiteral("HH:mm")));
    return {
        {"ok", true},
        {"actionType", QStringLiteral("focus_buffer")},
        {"taskId", selectedTask->id},
        {"blockId", selectedBlock.id},
        {"title", movedForHardConstraint ? QObject::tr("为重强度任务更换专注时段") : QObject::tr("为重强度任务建立专注缓冲")},
        {"description", movedForHardConstraint
            ? QObject::tr("当前位置受到硬截止或固定安排阻挡，建议改到 %1").arg(proposedRange)
            : QObject::tr("清空任务前后各 1 小时，减少高强度学习的上下文切换")},
        {"impact", best.relocation.movedBlocks.isEmpty()
            ? QObject::tr("无需移动其他时间块")
            : QObject::tr("将移动 %1 个普通时间块，时长保持不变").arg(best.relocation.movedBlocks.size())},
        {"proposedStart", deadlineText(best.start)},
        {"proposedEnd", deadlineText(best.end)},
        {"movedCount", best.relocation.movedBlocks.size()},
        {"requiresMove", movedForHardConstraint},
        {"provider", provider},
        {"aiExplanation", aiExplanation}
    };
}

QVariantMap ScheduleService::applyFocusBufferSuggestion(const QVariantMap& suggestion)
{
    if (suggestion.value(QStringLiteral("actionType")).toString() != QStringLiteral("focus_buffer")) {
        return {{"ok", false}, {"message", QObject::tr("无效的专注缓冲建议")}};
    }
    const int blockId = suggestion.value(QStringLiteral("blockId")).toInt();
    QDateTime proposedStart = parseDateTimeText(suggestion.value(QStringLiteral("proposedStart")).toString());
    if (blockId <= 0 || !proposedStart.isValid()) {
        return {{"ok", false}, {"message", QObject::tr("建议缺少有效的时间块或开始时间")}};
    }

    const ScheduleWindow window = displayWindow();
    const QVector<TimeBlock> allBlocks = m_blocks.blocksBetween(window.start, window.end);
    const QVector<CalendarEvent> events = m_calendar.eventsBetween(window.start, window.end);
    const QVector<Task> tasks = m_tasks.allTasks();
    QHash<int, Task> taskById;
    for (const Task& task : tasks) {
        taskById.insert(task.id, task);
    }

    TimeBlock selectedBlock;
    bool found = false;
    for (const TimeBlock& block : allBlocks) {
        if (block.id == blockId) {
            selectedBlock = block;
            found = true;
            break;
        }
    }
    if (!found || !taskById.contains(selectedBlock.taskId)) {
        return {{"ok", false}, {"message", QObject::tr("建议引用的时间块已不存在")}};
    }

    const qint64 durationSeconds = selectedBlock.start.secsTo(selectedBlock.end);
    const QDateTime proposedEnd = proposedStart.addSecs(durationSeconds);
    const QDateTime dayStart(proposedStart.date(), QTime(0, 0));
    const QDateTime dayEnd(proposedStart.date().addDays(1), QTime(0, 0));
    const QDateTime bufferStart = proposedStart.addSecs(-60 * 60);
    const QDateTime bufferEnd = proposedEnd.addSecs(60 * 60);
    QVector<TimeInterval> fixed;
    QVector<TimeBlock> movable;
    for (const CalendarEvent& event : events) {
        if (event.start < dayEnd && dayStart < event.end) {
            fixed.push_back({qMax(event.start, dayStart), qMin(event.end, dayEnd)});
        }
    }
    for (const TimeBlock& block : allBlocks) {
        if (block.id == selectedBlock.id || block.start.date() != proposedStart.date()) {
            continue;
        }
        const bool hardTask = taskById.contains(block.taskId)
            && taskById.value(block.taskId).deadlineType == DeadlineType::Hard;
        if (block.source == BlockSource::Locked || block.completedAt.isValid() || hardTask) {
            fixed.push_back({block.start, block.end});
        } else {
            movable.push_back(block);
        }
    }
    const CascadePlanResult relocation = planHorizontalRelocation(
        bufferStart, bufferEnd, dayStart, dayEnd, movable, fixed, false);
    if (!relocation.ok) {
        return {{"ok", false}, {"message", QObject::tr("日程已经变化，请重新生成专注缓冲建议")}};
    }

    selectedBlock.start = proposedStart;
    selectedBlock.end = proposedEnd;
    selectedBlock.source = taskById.value(selectedBlock.taskId).deadlineType == DeadlineType::Hard
        ? BlockSource::Locked
        : selectedBlock.source;
    QVector<TimeBlock> blocksToSave{selectedBlock};
    for (TimeBlock moved : relocation.movedBlocks) {
        moved.source = BlockSource::UserDragged;
        blocksToSave.push_back(moved);
    }
    if (!m_blocks.upsertBlocks(blocksToSave)) {
        return {{"ok", false}, {"message", QObject::tr("专注缓冲执行失败：%1").arg(m_blocks.lastError())}};
    }

    const int taskId = selectedBlock.taskId;
    m_selectedTaskId = taskId;
    m_selectedBlockId = selectedBlock.id;
    reschedule();
    selectTimelineItem(taskId, selectedBlock.id);
    return {
        {"ok", true},
        {"message", relocation.movedBlocks.isEmpty()
            ? QObject::tr("已建立前后各 1 小时的专注缓冲")
            : QObject::tr("已建立专注缓冲，并移动 %1 个普通时间块").arg(relocation.movedBlocks.size())}
    };
}

QVariantMap ScheduleService::applyScheduleSuggestion(const QVariantMap& suggestion)
{
    const QString action = suggestion.value(QStringLiteral("actionType")).toString();
    if (action == QStringLiteral("focus_buffer")) {
        return applyFocusBufferSuggestion(suggestion);
    }
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
        ? QObject::tr("今天还没有可复盘的计划时间块")
        : QObject::tr("今日计划 %1 分钟，已完成 %2 分钟，完成率 %3%")
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
            {"message", QObject::tr("数据导出失败：%1").arg(m_backup.lastError())}
        };
    }
    return {{"ok", true}, {"message", QObject::tr("数据已导出")}};
}

QVariantMap ScheduleService::importData(const QString& filePath)
{
    if (!m_backup.importFromFile(filePath)) {
        return {
            {"ok", false},
            {"message", QObject::tr("数据导入失败：%1").arg(m_backup.lastError())}
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
    QHash<int, QDateTime> firstBlockStartByTask;
    QHash<int, QDateTime> firstBlockEndByTask;
    QSet<int> scheduledTaskIdsForSelectedDate;
    for (const TimeBlock& block : blocks) {
        if (block.taskId <= 0 || !block.start.isValid() || !block.end.isValid()) {
            continue;
        }
        if (block.start.date() == m_selectedDate) {
            scheduledTaskIdsForSelectedDate.insert(block.taskId);
        }
        if (!firstBlockStartByTask.contains(block.taskId) || block.start < firstBlockStartByTask.value(block.taskId)) {
            firstBlockStartByTask.insert(block.taskId, block.start);
            firstBlockEndByTask.insert(block.taskId, block.end);
        }
    }
    QHash<int, QString> scheduleTextByTaskId;
    for (auto it = firstBlockStartByTask.constBegin(); it != firstBlockStartByTask.constEnd(); ++it) {
        const QDateTime start = it.value();
        const QDateTime end = firstBlockEndByTask.value(it.key());
        scheduleTextByTaskId.insert(
            it.key(),
            QStringLiteral("%1 %2-%3")
                .arg(start.toString(QStringLiteral("yyyy-MM-dd")),
                     start.time().toString(QStringLiteral("HH:mm")),
                     end.time().toString(QStringLiteral("HH:mm"))));
    }
    m_taskModel.setTasks(tasks);
    m_taskModel.setDailyFilter(m_selectedDate, scheduledTaskIdsForSelectedDate);
    m_taskModel.setScheduleTextByTaskId(scheduleTextByTaskId);
    rebuildTimeline(tasks, events, blocks);
    rebuildCapacityStats(blocks, events);
    emit dataChanged();
    emit selectedTaskChanged();
}

void ScheduleService::rebuildCapacityStats(const QVector<TimeBlock>& blocks, const QVector<CalendarEvent>& events)
{
    const ScheduleWindow window = displayWindow();
    QMap<QDate, QVector<QPair<int, int>>> intervalsByDate;
    for (const auto& block : blocks) {
        appendBusyInterval(intervalsByDate, block.start, block.end, window);
    }
    for (const auto& event : events) {
        appendBusyInterval(intervalsByDate, event.start, event.end, window);
    }

    const QDate displayDate = m_selectedDate;
    const int todayScheduled = mergedBusyMinutes(intervalsByDate.value(displayDate));
    int weekScheduled = 0;
    for (auto it = intervalsByDate.cbegin(); it != intervalsByDate.cend(); ++it) {
        weekScheduled += mergedBusyMinutes(it.value());
    }

    const int dailyCapacity = qMax(1, (m_config.workdayEndHour - m_config.workdayStartHour) * 60);
    int remainingToday = qMax(0, dailyCapacity - todayScheduled);
    if (displayDate == QDate::currentDate()) {
        const QTime now = QTime::currentTime();
        const int currentMinute = qMax(m_config.workdayStartHour * 60, now.hour() * 60 + now.minute());
        const int remainingClock = qMax(0, m_config.workdayEndHour * 60 - currentMinute);
        remainingToday = qMin(remainingToday, remainingClock);
    }

    constexpr int weeklyCapacity = 42 * 60;
    const int dayLoadPercent = qRound(todayScheduled * 100.0 / dailyCapacity);
    const int weekLoadPercent = qRound(weekScheduled * 100.0 / weeklyCapacity);
    const bool overloaded = dayLoadPercent >= 90 || weekLoadPercent >= 100;
    const QString message = overloaded
        ? QObject::tr("本周负载过高")
        : weekLoadPercent >= 80
            ? QObject::tr("本周负载偏高")
            : weekLoadPercent >= 55
                ? QObject::tr("本周节奏充实")
                : QObject::tr("学习容量健康");
    m_capacityStats = {
        {"todayRemainingMinutes", remainingToday},
        {"todayScheduledMinutes", todayScheduled},
        {"weekScheduledMinutes", weekScheduled},
        {"todayLoadPercent", dayLoadPercent},
        {"weekLoadPercent", weekLoadPercent},
        {"weeklyCapacityMinutes", weeklyCapacity},
        {"overloaded", overloaded},
        {"message", message}
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

    m_dailyItems.clear();
    for (const auto& item : items) {
        if (item.dayIndex != 0 || item.hiddenInSpan) {
            continue;
        }
        m_dailyItems.push_back(QVariantMap {
            {QStringLiteral("id"), item.id},
            {QStringLiteral("taskId"), item.taskId},
            {QStringLiteral("title"), item.title},
            {QStringLiteral("subtitle"), item.subtitle},
            {QStringLiteral("scheduleText"),
             item.start.time().toString(QStringLiteral("HH:mm"))
                 + QStringLiteral(" - ")
                 + item.end.time().toString(QStringLiteral("HH:mm"))},
            {QStringLiteral("priority"), item.priority},
            {QStringLiteral("color"), item.color},
            {QStringLiteral("isEvent"), item.event},
            {QStringLiteral("locked"), item.locked || item.eventLocked}
        });
    }

    m_focusItem.clear();
    const QDateTime now = QDateTime::currentDateTime();
    for (const auto& item : items) {
        if (!item.event && !item.completed && item.start <= now && now < item.end) {
            m_focusItem = focusItemMap(item, now);
            break;
        }
    }
    if (m_focusItem.isEmpty()) {
        for (auto it = items.crbegin(); it != items.crend(); ++it) {
            if (!it->event && !it->completed && it->end.date() == now.date() && it->end <= now) {
                m_focusItem = focusItemMap(*it, now);
                break;
            }
        }
    }
    if (m_focusItem.isEmpty()) {
        for (const auto& item : items) {
            if (!item.event && !item.completed && item.start > now) {
                m_focusItem = focusItemMap(item, now);
                break;
            }
        }
    }
    if (m_focusItem.isEmpty()) {
        m_focusItem = {
            {"title", QObject::tr("今天没有自动安排")},
            {"subtitle", QObject::tr("可以添加任务或检查冲突")},
            {"color", QStringLiteral("#7C8CFF")},
            {"taskId", 0},
            {"blockId", 0},
            {"canStart", false},
            {"canComplete", false},
            {"isCurrent", false},
            {"isPast", false},
            {"durationSeconds", 0},
            {"remainingSeconds", 0}
        };
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
