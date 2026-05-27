#include "core/scheduler/CapacityAnalyzer.h"

#include <algorithm>

QVector<TimeInterval> CapacityAnalyzer::freeIntervals(
    const ScheduleWindow& window,
    const SchedulingConfig& config,
    const QVector<CalendarEvent>& fixedEvents,
    const QVector<TimeBlock>& lockedBlocks) const
{
    QVector<TimeInterval> free;
    QDate day = window.start.date();
    while (day <= window.end.date()) {
        QDateTime start(day, QTime(config.workdayStartHour, 0));
        QDateTime end(day, QTime(config.workdayEndHour, 0));
        start = qMax(start, window.start);
        end = qMin(end, window.end);
        if (start < end) {
            free.push_back({start, end});
        }
        day = day.addDays(1);
    }

    QVector<TimeInterval> busy;
    for (const auto& event : fixedEvents) {
        busy.push_back({event.start, event.end});
    }
    for (const auto& block : lockedBlocks) {
        busy.push_back({block.start, block.end});
    }

    std::sort(busy.begin(), busy.end(), [](const TimeInterval& a, const TimeInterval& b) {
        return a.start < b.start;
    });

    for (const auto& interval : busy) {
        free = subtractInterval(free, interval);
    }

    free.erase(std::remove_if(free.begin(), free.end(), [&config](const TimeInterval& interval) {
        return interval.minutes() < config.minimumBlockMinutes;
    }), free.end());

    return free;
}

QVector<TimeInterval> CapacityAnalyzer::subtractInterval(const QVector<TimeInterval>& intervals, const TimeInterval& busy)
{
    QVector<TimeInterval> result;
    for (const auto& interval : intervals) {
        if (busy.end <= interval.start || busy.start >= interval.end) {
            result.push_back(interval);
            continue;
        }

        if (busy.start > interval.start) {
            result.push_back({interval.start, qMin(busy.start, interval.end)});
        }
        if (busy.end < interval.end) {
            result.push_back({qMax(busy.end, interval.start), interval.end});
        }
    }
    return result;
}
