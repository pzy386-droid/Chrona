#include "core/scheduler/TaskScheduler.h"

#include <algorithm>
#include <QObject>

namespace {
struct QueueItem {
    Task task;
    int score = 0;
};

bool intervalAllowedForTask(const TimeInterval& slot, const Task& task)
{
    if (task.startDate.isValid() && slot.end <= task.startDate) {
        return false;
    }
    return true;
}

QDateTime effectiveSlotStart(const TimeInterval& slot, const Task& task)
{
    if (task.startDate.isValid()) {
        return qMax(slot.start, task.startDate);
    }
    return slot.start;
}

int preferencePenalty(const TimeInterval& slot, const QString& preferredStudyTime)
{
    const int hour = slot.start.time().hour();
    if (preferredStudyTime == QStringLiteral("morning")) {
        return hour >= 6 && hour < 12 ? 0 : 120;
    }
    if (preferredStudyTime == QStringLiteral("afternoon")) {
        return hour >= 12 && hour < 18 ? 0 : 120;
    }
    if (preferredStudyTime == QStringLiteral("evening")) {
        return hour >= 18 && hour <= 23 ? 0 : 120;
    }
    return 0;
}

SchedulingConfig configForTask(const SchedulingConfig& base, const Task& task)
{
    SchedulingConfig config = base;
    config.minimumBlockMinutes = qMax(15, task.minChunkMinutes > 0 ? task.minChunkMinutes : base.minimumBlockMinutes);
    config.preferredBlockMinutes = qMax(config.minimumBlockMinutes, task.idealChunkMinutes > 0 ? task.idealChunkMinutes : base.preferredBlockMinutes);
    return config;
}
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
        if (task.status == TaskStatus::Done || task.status == TaskStatus::Archived || task.remainingMinutes <= 0 || !task.autoScheduleEnabled) {
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
        SchedulingConfig taskConfig = configForTask(config, item.task);
        QVector<int> slotOrder;
        slotOrder.reserve(free.size());
        for (int i = 0; i < free.size(); ++i) {
            slotOrder.push_back(i);
        }
        std::sort(slotOrder.begin(), slotOrder.end(), [&](int a, int b) {
            const int preferenceA = preferencePenalty(free[a], item.task.preferredStudyTime);
            const int preferenceB = preferencePenalty(free[b], item.task.preferredStudyTime);
            if (preferenceA == preferenceB) {
                return free[a].start < free[b].start;
            }
            return preferenceA < preferenceB;
        });

        for (const int slotIndex : slotOrder) {
            if (remaining <= 0 || slotIndex < 0 || slotIndex >= free.size()) {
                break;
            }
            auto& slot = free[slotIndex];
            if (!intervalAllowedForTask(slot, item.task)) {
                continue;
            }

            const QDateTime slotStart = effectiveSlotStart(slot, item.task);
            const QDateTime latestEnd = qMin(slot.end, item.task.deadline);
            if (slotStart >= item.task.deadline) {
                continue;
            }
            int freeMinutes = static_cast<int>(slotStart.secsTo(latestEnd) / 60);

            if (freeMinutes < taskConfig.minimumBlockMinutes) {
                continue;
            }

            SchedulingConfig weightedConfig = taskConfig;
            if (preferencePenalty({slotStart, latestEnd}, item.task.preferredStudyTime) == 0) {
                weightedConfig.preferredBlockMinutes = qMin(taskConfig.preferredBlockMinutes + 30, 180);
            }

            const int chunk = m_blockGenerator.nextChunkMinutes(remaining, freeMinutes, weightedConfig);
            if (chunk <= 0) {
                continue;
            }

            TimeBlock block;
            block.taskId = item.task.id;
            block.start = slotStart;
            block.end = slotStart.addSecs(chunk * 60);
            block.source = BlockSource::Auto;
            result.generatedBlocks.push_back(block);

            remaining -= chunk;
            placedAny = true;
            slot.start = block.end.addSecs(config.breakMinutes * 60);
        }

        if (remaining > 0 || !placedAny) {
            result.unscheduledTaskIds.push_back(item.task.id);
            result.issues.push_back({
                item.task.id,
                item.task.title,
                placedAny ? QObject::tr("可用时间不足，任务只完成了部分排程") : QObject::tr("截止时间前没有足够的可用时间"),
                remaining
            });
        } else {
            result.scheduledTaskIds.push_back(item.task.id);
        }

        free.erase(std::remove_if(free.begin(), free.end(), [&config](const TimeInterval& interval) {
            return interval.minutes() < config.minimumBlockMinutes;
        }), free.end());
    }

    return result;
}
