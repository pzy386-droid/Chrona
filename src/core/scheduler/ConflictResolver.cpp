#include "core/scheduler/ConflictResolver.h"

namespace {
bool overlaps(const QDateTime& aStart, const QDateTime& aEnd, const QDateTime& bStart, const QDateTime& bEnd)
{
    return aStart < bEnd && bStart < aEnd;
}
}

bool ConflictResolver::canPlace(const TimeInterval& candidate, const QVector<CalendarEvent>& fixedEvents, const QVector<TimeBlock>& lockedBlocks) const
{
    for (const auto& event : fixedEvents) {
        if (overlaps(candidate.start, candidate.end, event.start, event.end)) {
            return false;
        }
    }
    for (const auto& block : lockedBlocks) {
        if (overlaps(candidate.start, candidate.end, block.start, block.end)) {
            return false;
        }
    }
    return true;
}
