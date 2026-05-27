#pragma once

#include "core/models/ScheduleTypes.h"

#include <optional>

struct Task {
    int id = 0;
    QString title;
    QString notes;
    QDateTime deadline;
    int estimatedMinutes = 0;
    double estimatedHours = 0.0;
    int remainingMinutes = 0;
    QString preferredStudyTime;
    Priority priority = Priority::Medium;
    TaskStatus status = TaskStatus::Inbox;
    std::optional<int> categoryId;
    QString categoryName;
    QString categoryColor;
};
