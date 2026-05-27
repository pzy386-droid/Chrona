#pragma once

#include "core/scheduler/TaskScheduler.h"

class Scheduler {
public:
    QList<TimeBlock> generateSchedule(
        const QList<Task>& tasks,
        const QList<CalendarEvent>& events,
        const QList<TimeBlock>& lockedBlocks,
        const ScheduleWindow& window,
        const SchedulingConfig& config) const;

    QList<TimeBlock> getTodayBlocks(const QList<TimeBlock>& blocks) const;
    QList<TimeBlock> getWeekBlocks(const QList<TimeBlock>& blocks, const QDate& weekStart) const;

private:
    TaskScheduler m_scheduler;
};
