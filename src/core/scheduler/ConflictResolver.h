#pragma once

#include "core/models/CalendarEvent.h"
#include "core/models/TimeBlock.h"

class ConflictResolver {
public:
    bool canPlace(const TimeInterval& candidate, const QVector<CalendarEvent>& fixedEvents, const QVector<TimeBlock>& lockedBlocks) const;
};
