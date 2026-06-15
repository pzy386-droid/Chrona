#pragma once

#include "core/models/ScheduleTypes.h"

#include <optional>

struct Task {
    int id = 0;
    QString title;
    QString notes;
    QDateTime startDate;
    QDateTime deadline;
    int estimatedMinutes = 0;
    double estimatedHours = 0.0;
    int remainingMinutes = 0;
    QString preferredStudyTime;
    DeadlineType deadlineType = DeadlineType::Soft;
    bool autoScheduleEnabled = true;
    int minChunkMinutes = 30;
    int idealChunkMinutes = 90;
    int effortLevel = 1;
    ScheduleStatus scheduleStatus = ScheduleStatus::Unscheduled;
    Priority priority = Priority::Medium;
    TaskStatus status = TaskStatus::Inbox;
    std::optional<int> categoryId;
    QString categoryName;
    QString categoryColor;
    QDateTime completedAt;
};
