#include "core/scheduler/TaskScheduler.h"

#include <algorithm>

namespace {
struct QueueItem {
    Task task;
    int score = 0;
};
}

ScheduleResult TaskScheduler::generateSchedule(
    const QVector<Task>& tasks,
    const QVector<CalendarEvent>& fixedEvents,
    const QVector<TimeBlock>& lockedBlocks,
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

        for (int i = 0; i < free.size() && remaining > 0; ++i) {
            auto& slot = free[i];
            if (slot.start >= item.task.deadline) {
                continue;
            }

            const QDateTime latestEnd = qMin(slot.end, item.task.deadline);
            int freeMinutes = static_cast<int>(slot.start.secsTo(latestEnd) / 60);
            if (freeMinutes < config.minimumBlockMinutes) {
                continue;
            }

            const int chunk = m_blockGenerator.nextChunkMinutes(remaining, freeMinutes, config);
            if (chunk <= 0) {
                continue;
            }

            TimeBlock block;
            block.taskId = item.task.id;
            block.start = slot.start;
            block.end = slot.start.addSecs(chunk * 60);
            block.source = BlockSource::Auto;
            result.generatedBlocks.push_back(block);

            remaining -= chunk;
            placedAny = true;
            slot.start = block.end.addSecs(config.breakMinutes * 60);
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
