#pragma once

#include "core/models/TimeBlock.h"

#include <algorithm>
#include <limits>

struct CascadePlanResult {
    bool ok = false;
    QString error;
    QVector<TimeBlock> movedBlocks;
};

inline CascadePlanResult planCascadeDown(const QDateTime& selectedStart,
                                         const QDateTime& selectedEnd,
                                         const QDateTime& dayEnd,
                                         QVector<TimeBlock> movableBlocks,
                                         QVector<TimeInterval> fixedIntervals)
{
    CascadePlanResult result;
    if (!selectedStart.isValid() || !selectedEnd.isValid() || selectedEnd <= selectedStart
        || !dayEnd.isValid() || selectedEnd > dayEnd) {
        result.error = QStringLiteral("invalid-range");
        return result;
    }

    std::sort(fixedIntervals.begin(), fixedIntervals.end(), [](const TimeInterval& a, const TimeInterval& b) {
        return a.start < b.start;
    });
    std::sort(movableBlocks.begin(), movableBlocks.end(), [](const TimeBlock& a, const TimeBlock& b) {
        return a.start < b.start;
    });

    for (const TimeInterval& interval : fixedIntervals) {
        if (selectedStart < interval.end && interval.start < selectedEnd) {
            result.error = QStringLiteral("fixed-conflict");
            return result;
        }
    }

    QDateTime cursor = selectedEnd;
    for (TimeBlock block : movableBlocks) {
        if (block.end <= selectedStart || block.start >= cursor) {
            continue;
        }

        const qint64 durationSeconds = block.start.secsTo(block.end);
        QDateTime nextStart = cursor;
        bool advanced = true;
        while (advanced) {
            advanced = false;
            const QDateTime nextEnd = nextStart.addSecs(durationSeconds);
            for (const TimeInterval& interval : fixedIntervals) {
                if (nextStart < interval.end && interval.start < nextEnd) {
                    nextStart = interval.end;
                    advanced = true;
                    break;
                }
            }
        }

        const QDateTime nextEnd = nextStart.addSecs(durationSeconds);
        if (nextEnd > dayEnd) {
            result.error = QStringLiteral("day-overflow");
            result.movedBlocks.clear();
            return result;
        }

        block.start = nextStart;
        block.end = nextEnd;
        result.movedBlocks.push_back(block);
        cursor = nextEnd;
    }

    result.ok = true;
    return result;
}

inline CascadePlanResult planCascadeUp(const QDateTime& selectedStart,
                                       const QDateTime& selectedEnd,
                                       const QDateTime& dayStart,
                                       QVector<TimeBlock> movableBlocks,
                                       QVector<TimeInterval> fixedIntervals)
{
    CascadePlanResult result;
    if (!selectedStart.isValid() || !selectedEnd.isValid() || selectedEnd <= selectedStart
        || !dayStart.isValid() || selectedStart < dayStart) {
        result.error = QStringLiteral("invalid-range");
        return result;
    }

    std::sort(fixedIntervals.begin(), fixedIntervals.end(), [](const TimeInterval& a, const TimeInterval& b) {
        return a.start > b.start;
    });
    std::sort(movableBlocks.begin(), movableBlocks.end(), [](const TimeBlock& a, const TimeBlock& b) {
        return a.start > b.start;
    });

    for (const TimeInterval& interval : fixedIntervals) {
        if (selectedStart < interval.end && interval.start < selectedEnd) {
            result.error = QStringLiteral("fixed-conflict");
            return result;
        }
    }

    QDateTime cursor = selectedStart;
    for (TimeBlock block : movableBlocks) {
        if (block.start >= selectedEnd || block.end <= cursor) {
            continue;
        }

        const qint64 durationSeconds = block.start.secsTo(block.end);
        QDateTime nextEnd = cursor;
        bool retreated = true;
        while (retreated) {
            retreated = false;
            const QDateTime nextStart = nextEnd.addSecs(-durationSeconds);
            for (const TimeInterval& interval : fixedIntervals) {
                if (nextStart < interval.end && interval.start < nextEnd) {
                    nextEnd = interval.start;
                    retreated = true;
                    break;
                }
            }
        }

        const QDateTime nextStart = nextEnd.addSecs(-durationSeconds);
        if (nextStart < dayStart) {
            result.error = QStringLiteral("day-underflow");
            result.movedBlocks.clear();
            return result;
        }

        block.start = nextStart;
        block.end = nextEnd;
        result.movedBlocks.push_back(block);
        cursor = nextStart;
    }

    result.ok = true;
    return result;
}

inline CascadePlanResult planHorizontalRelocation(const QDateTime& selectedStart,
                                                  const QDateTime& selectedEnd,
                                                  const QDateTime& dayStart,
                                                  const QDateTime& dayEnd,
                                                  QVector<TimeBlock> movableBlocks,
                                                  QVector<TimeInterval> fixedIntervals,
                                                  bool preferUp)
{
    CascadePlanResult result;
    if (!selectedStart.isValid() || !selectedEnd.isValid() || selectedEnd <= selectedStart
        || selectedStart < dayStart || selectedEnd > dayEnd) {
        result.error = QStringLiteral("invalid-range");
        return result;
    }
    for (const TimeInterval& interval : fixedIntervals) {
        if (selectedStart < interval.end && interval.start < selectedEnd) {
            result.error = QStringLiteral("fixed-conflict");
            return result;
        }
    }

    QVector<TimeBlock> colliding;
    QVector<TimeInterval> occupied = fixedIntervals;
    occupied.push_back({selectedStart, selectedEnd});
    for (const TimeBlock& block : movableBlocks) {
        if (block.start < selectedEnd && selectedStart < block.end) {
            colliding.push_back(block);
        } else {
            occupied.push_back({block.start, block.end});
        }
    }
    if (colliding.isEmpty()) {
        result.ok = true;
        return result;
    }

    std::sort(colliding.begin(), colliding.end(), [preferUp](const TimeBlock& a, const TimeBlock& b) {
        return preferUp ? a.start > b.start : a.start < b.start;
    });

    constexpr int snapMinutes = 15;
    for (TimeBlock block : colliding) {
        const int durationMinutes = static_cast<int>(block.start.secsTo(block.end) / 60);
        bool found = false;
        QDateTime bestStart;
        qint64 bestDistance = std::numeric_limits<qint64>::max();
        bool bestPreferred = false;

        for (QDateTime candidate = dayStart;
             candidate.addSecs(durationMinutes * 60) <= dayEnd;
             candidate = candidate.addSecs(snapMinutes * 60)) {
            const QDateTime candidateEnd = candidate.addSecs(durationMinutes * 60);
            bool overlaps = false;
            for (const TimeInterval& interval : occupied) {
                if (candidate < interval.end && interval.start < candidateEnd) {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) {
                continue;
            }

            const qint64 distance = qAbs(block.start.secsTo(candidate));
            const bool preferred = preferUp ? candidate < block.start : candidate > block.start;
            if (!found || distance < bestDistance
                || (distance == bestDistance && preferred && !bestPreferred)) {
                found = true;
                bestStart = candidate;
                bestDistance = distance;
                bestPreferred = preferred;
            }
        }

        if (!found) {
            result.error = QStringLiteral("no-horizontal-space");
            result.movedBlocks.clear();
            return result;
        }

        block.start = bestStart;
        block.end = bestStart.addSecs(durationMinutes * 60);
        result.movedBlocks.push_back(block);
        occupied.push_back({block.start, block.end});
    }

    result.ok = true;
    return result;
}
