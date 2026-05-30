#pragma once

#include "core/models/ScheduleTypes.h"

#include <optional>

struct CalendarEvent {
    int id = 0;
    QString title;
    QDateTime start;
    QDateTime end;
    EventType type = EventType::FixedEvent;
    std::optional<int> categoryId;
    QString categoryName;
    QString categoryColor;
    bool locked = true;
};
