#pragma once

#include "core/models/CalendarEvent.h"
#include "core/models/Task.h"
#include "core/models/TimeBlock.h"
#include "core/scheduler/CapacityAnalyzer.h"
#include "core/scheduler/PriorityEvaluator.h"
#include "core/scheduler/TimeBlockGenerator.h"

struct ScheduleResult {
    QVector<TimeBlock> generatedBlocks;
    QVector<int> unscheduledTaskIds;
    QVector<int> scheduledTaskIds;
    struct Issue {
        int taskId = 0;
        QString title;

        QString code;
        QString reason;
        QString fixHint;

        int remainingMinutes = 0;
    };
    QVector<Issue> issues;
};

class TaskScheduler {
public:
    ScheduleResult generateSchedule(
        const QVector<Task>& tasks,
        const QVector<CalendarEvent>& fixedEvents,
        const QVector<TimeBlock>& lockedBlocks,
        const ScheduleWindow& window,
        const SchedulingConfig& config) const;

private:
    CapacityAnalyzer m_capacityAnalyzer;
    PriorityEvaluator m_priorityEvaluator;
    TimeBlockGenerator m_blockGenerator;
};
