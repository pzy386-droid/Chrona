#pragma once

#include "core/models/CalendarEvent.h"
#include "core/models/TimeBlock.h"

class CapacityAnalyzer {
public:
    QVector<TimeInterval> freeIntervals(
        const ScheduleWindow& window,
        const SchedulingConfig& config,
        const QVector<CalendarEvent>& fixedEvents,
        const QVector<TimeBlock>& lockedBlocks) const;

private:
    static QVector<TimeInterval> subtractInterval(const QVector<TimeInterval>& intervals, const TimeInterval& busy);
};
