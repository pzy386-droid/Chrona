#include "managers/Scheduler.h"

QList<TimeBlock> Scheduler::generateSchedule(
    const QList<Task>& tasks,
    const QList<CalendarEvent>& events,
    const QList<TimeBlock>& lockedBlocks,
    const QList<StudyFrame>& studyFrames,
    const ScheduleWindow& window,
    const SchedulingConfig& config) const
{
    const ScheduleResult result = m_scheduler.generateSchedule(tasks, events, lockedBlocks, studyFrames, window, config);
    return result.generatedBlocks;
}

QList<TimeBlock> Scheduler::getTodayBlocks(const QList<TimeBlock>& blocks) const
{
    QList<TimeBlock> todayBlocks;
    const QDate today = QDate::currentDate();
    for (const auto& block : blocks) {
        if (block.start.date() == today) {
            todayBlocks.push_back(block);
        }
    }
    return todayBlocks;
}

QList<TimeBlock> Scheduler::getWeekBlocks(const QList<TimeBlock>& blocks, const QDate& weekStart) const
{
    QList<TimeBlock> weekBlocks;
    const QDate weekEnd = weekStart.addDays(7);
    for (const auto& block : blocks) {
        if (block.start.date() >= weekStart && block.start.date() < weekEnd) {
            weekBlocks.push_back(block);
        }
    }
    return weekBlocks;
}
