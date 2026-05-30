#include "core/scheduler/TaskScheduler.h"

#include <algorithm>

namespace {
struct QueueItem {
    Task task;
    int score = 0;
};

bool preferredTimeMatches(const QString& preferredStudyTime, const QTime& start, const QTime& end)
{
    const QString preferred = preferredStudyTime.trimmed().toLower();
    if (preferred.isEmpty()) {
        return false;
    }

    if (preferred == QStringLiteral("morning")) {
        return start < QTime(12, 0);
    }
    if (preferred == QStringLiteral("afternoon")) {
        return start < QTime(18, 0) && end > QTime(12, 0);
    }
    if (preferred == QStringLiteral("evening")) {
        return end > QTime(18, 0);
    }
    return false;
}

bool frameMatchesTask(const StudyFrame& frame, const Task& task)
{
    if (!frame.enabled) {
        return false;
    }
    if (frame.categoryId && task.categoryId && *frame.categoryId == *task.categoryId) {
        return true;
    }
    if (!frame.categoryName.trimmed().isEmpty() && frame.categoryName.trimmed().compare(task.categoryName.trimmed(), Qt::CaseInsensitive) == 0) {
        return true;
    }
    return preferredTimeMatches(task.preferredStudyTime, frame.startTime, frame.endTime);
}

QVector<TimeInterval> matchingFrameIntervals(const Task& task, const QVector<StudyFrame>& frames, const TimeInterval& slot)
{
    QVector<TimeInterval> intervals;
    const int dayOfWeek = slot.start.date().dayOfWeek();
    for (const auto& frame : frames) {
        if (frame.dayOfWeek != dayOfWeek || !frame.startTime.isValid() || !frame.endTime.isValid() || frame.endTime <= frame.startTime || !frameMatchesTask(frame, task)) {
            continue;
        }

        const QDateTime frameStart(slot.start.date(), frame.startTime);
        const QDateTime frameEnd(slot.start.date(), frame.endTime);
        const QDateTime start = qMax(slot.start, frameStart);
        const QDateTime end = qMin(slot.end, frameEnd);
        if (start < end) {
            intervals.push_back({start, end});
        }
    }
    std::sort(intervals.begin(), intervals.end(), [](const TimeInterval& a, const TimeInterval& b) {
        return a.start < b.start;
    });
    return intervals;
}

void consumeFreeInterval(QVector<TimeInterval>& free, int index, const QDateTime& blockStart, const QDateTime& blockEnd, int breakMinutes)
{
    const TimeInterval original = free.at(index);
    QVector<TimeInterval> replacement;
    if (original.start < blockStart) {
        replacement.push_back({original.start, blockStart});
    }

    const QDateTime nextStart = blockEnd.addSecs(breakMinutes * 60);
    if (nextStart < original.end) {
        replacement.push_back({nextStart, original.end});
    }

    free.removeAt(index);
    for (int i = replacement.size() - 1; i >= 0; --i) {
        free.insert(index, replacement.at(i));
    }
}
}

ScheduleResult TaskScheduler::generateSchedule(
    const QVector<Task>& tasks,
    const QVector<CalendarEvent>& fixedEvents,
    const QVector<TimeBlock>& lockedBlocks,
    const QVector<StudyFrame>& studyFrames,
    const ScheduleWindow& window,
    const SchedulingConfig& config) const
{
    ScheduleResult result;
    QVector<TimeInterval> free = m_capacityAnalyzer.freeIntervals(window, config, fixedEvents, lockedBlocks);
    QVector<QueueItem> queue;
    const QDateTime now = QDateTime::currentDateTime();

    for (const auto& task : tasks) {
        if (task.status == TaskStatus::Done || task.status == TaskStatus::Archived || task.remainingMinutes <= 0) {
            continue;
        }
        queue.push_back({task, m_priorityEvaluator.score(task, now)});
    }

    std::sort(queue.begin(), queue.end(), [](const QueueItem& a, const QueueItem& b) {
        if (a.score == b.score) {
            return a.task.deadline < b.task.deadline;
        }
        return a.score > b.score;
    });

    for (const auto& item : queue) {
        int remaining = item.task.remainingMinutes;
        bool placedAny = false;

        for (int pass = 0; pass < 2 && remaining > 0; ++pass) {
            const bool framesOnly = pass == 0;
            for (int i = 0; i < free.size() && remaining > 0; ++i) {
                const TimeInterval slot = free.at(i);
                if (slot.start >= item.task.deadline) {
                    continue;
                }

                QVector<TimeInterval> candidates = framesOnly
                    ? matchingFrameIntervals(item.task, studyFrames, slot)
                    : QVector<TimeInterval>{slot};
                if (framesOnly && candidates.isEmpty()) {
                    continue;
                }

                for (const auto& candidate : candidates) {
                    const QDateTime latestEnd = qMin(candidate.end, item.task.deadline);
                    int freeMinutes = static_cast<int>(candidate.start.secsTo(latestEnd) / 60);
                    if (freeMinutes < config.minimumBlockMinutes) {
                        continue;
                    }

                    const int chunk = m_blockGenerator.nextChunkMinutes(remaining, freeMinutes, config);
                    if (chunk <= 0) {
                        continue;
                    }

                    TimeBlock block;
                    block.taskId = item.task.id;
                    block.start = candidate.start;
                    block.end = candidate.start.addSecs(chunk * 60);
                    block.source = BlockSource::Auto;
                    result.generatedBlocks.push_back(block);

                    remaining -= chunk;
                    placedAny = true;
                    consumeFreeInterval(free, i, block.start, block.end, config.breakMinutes);
                    --i;
                    break;
                }
            }
        }

        if (remaining > 0 || !placedAny) {
            result.unscheduledTaskIds.push_back(item.task.id);
        }

        free.erase(std::remove_if(free.begin(), free.end(), [&config](const TimeInterval& interval) {
            return interval.minutes() < config.minimumBlockMinutes;
        }), free.end());
    }

    return result;
}
