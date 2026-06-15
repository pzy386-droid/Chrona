#include "core/scheduler/TaskScheduler.h"

#include <QObject>
#include <QStringList>
#include <algorithm>

namespace {
struct QueueItem {
    Task task;
    int score = 0;
};

struct CandidateSlot {
    TimeInterval interval;
    QString matchReason;
    int rank = 0;
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
    const QString preferred = preferredStudyTime.trimmed().toLower();
    if (preferred.isEmpty()) {
        return 0;
    }

    QTime preferredStart;
    QTime preferredEnd;
    if (preferred == QStringLiteral("morning")) {
        preferredStart = QTime(6, 0);
        preferredEnd = QTime(12, 0);
    } else if (preferred == QStringLiteral("afternoon")) {
        preferredStart = QTime(12, 0);
        preferredEnd = QTime(18, 0);
    } else if (preferred == QStringLiteral("evening")) {
        preferredStart = QTime(18, 0);
        preferredEnd = QTime(23, 59, 59);
    } else {
        return 0;
    }

    const QDateTime windowStart(slot.start.date(), preferredStart);
    const QDateTime windowEnd(slot.start.date(), preferredEnd);
    return slot.end > windowStart && slot.start < windowEnd ? 0 : 120;
}

QDateTime preferredSlotStart(const TimeInterval& slot, const QString& preferredStudyTime)
{
    if (preferencePenalty(slot, preferredStudyTime) != 0) {
        return slot.start;
    }

    const QString preferred = preferredStudyTime.trimmed().toLower();
    if (preferred == QStringLiteral("morning")) {
        return qMax(slot.start, QDateTime(slot.start.date(), QTime(6, 0)));
    }
    if (preferred == QStringLiteral("afternoon")) {
        return qMax(slot.start, QDateTime(slot.start.date(), QTime(12, 0)));
    }
    if (preferred == QStringLiteral("evening")) {
        return qMax(slot.start, QDateTime(slot.start.date(), QTime(18, 0)));
    }
    return slot.start;
}

SchedulingConfig configForTask(const SchedulingConfig& base, const Task& task)
{
    SchedulingConfig config = base;
    config.minimumBlockMinutes = qMax(15, task.minChunkMinutes > 0 ? task.minChunkMinutes : base.minimumBlockMinutes);
    config.preferredBlockMinutes = qMax(config.minimumBlockMinutes, task.idealChunkMinutes > 0 ? task.idealChunkMinutes : base.preferredBlockMinutes);
    return config;
}

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

int frameMatchRank(const StudyFrame& frame, const Task& task)
{
    if (!frame.enabled) {
        return 0;
    }
    if (frame.categoryId && task.categoryId && *frame.categoryId == *task.categoryId) {
        return 3;
    }
    if (!frame.categoryName.trimmed().isEmpty() && frame.categoryName.trimmed().compare(task.categoryName.trimmed(), Qt::CaseInsensitive) == 0) {
        return 3;
    }
    return preferredTimeMatches(task.preferredStudyTime, frame.startTime, frame.endTime) ? 2 : 0;
}

QVector<CandidateSlot> matchingFrameIntervals(const Task& task, const QVector<StudyFrame>& frames, const TimeInterval& slot)
{
    QVector<CandidateSlot> intervals;
    const int dayOfWeek = slot.start.date().dayOfWeek();
    for (const auto& frame : frames) {
        const int rank = frameMatchRank(frame, task);
        if (frame.dayOfWeek != dayOfWeek || !frame.startTime.isValid() || !frame.endTime.isValid() || frame.endTime <= frame.startTime || rank == 0) {
            continue;
        }

        const QDateTime frameStart(slot.start.date(), frame.startTime);
        const QDateTime frameEnd(slot.start.date(), frame.endTime);
        const QDateTime start = qMax(slot.start, frameStart);
        const QDateTime end = qMin(slot.end, frameEnd);
        if (start < end) {
            intervals.push_back({{start, end}, rank == 3 ? QObject::tr("匹配学习时段") : QObject::tr("匹配学习偏好时间"), rank});
        }
    }
    std::sort(intervals.begin(), intervals.end(), [](const CandidateSlot& a, const CandidateSlot& b) {
        if (a.rank == b.rank) {
            return a.interval.start < b.interval.start;
        }
        return a.rank > b.rank;
    });
    return intervals;
}

QString blockExplanation(const Task& task, int score, const QDateTime& now, const QString& slotReason, const TimeInterval& slot, const QString& preferredStudyTime)
{
    QStringList reasons;
    if (now.daysTo(task.deadline) <= 2) {
        reasons.push_back(QObject::tr("DDL最近"));
    }
    if (task.priority == Priority::High || score >= 1000) {
        reasons.push_back(QObject::tr("优先级高"));
    }
    if (!slotReason.isEmpty()) {
        reasons.push_back(slotReason);
    } else if (preferencePenalty(slot, preferredStudyTime) == 0 && !preferredStudyTime.trimmed().isEmpty()) {
        reasons.push_back(QObject::tr("匹配学习偏好时间"));
    }
    reasons.push_back(QObject::tr("避开固定日程"));
    return reasons.join(QStringLiteral(" · "));
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
        const SchedulingConfig taskConfig = configForTask(config, item.task);

        for (int pass = 0; pass < 2 && remaining > 0; ++pass) {
            const bool framesOnly = pass == 0;
            bool placedInPass = true;

            while (remaining > 0 && placedInPass) {
                placedInPass = false;
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
                    const TimeInterval slot = free.at(slotIndex);
                    if (!intervalAllowedForTask(slot, item.task)) {
                        continue;
                    }

                    QVector<CandidateSlot> candidates = framesOnly
                        ? matchingFrameIntervals(item.task, studyFrames, slot)
                        : QVector<CandidateSlot>{{slot, QString(), 1}};
                    if (framesOnly && candidates.isEmpty()) {
                        continue;
                    }

                    for (const auto& candidateSlot : candidates) {
                        QDateTime slotStart = effectiveSlotStart({preferredSlotStart(candidateSlot.interval, item.task.preferredStudyTime), candidateSlot.interval.end}, item.task);
                        const QDateTime latestEnd = qMin(candidateSlot.interval.end, item.task.deadline);
                        if (slotStart >= latestEnd && !candidateSlot.matchReason.isEmpty()) {
                            slotStart = effectiveSlotStart(candidateSlot.interval, item.task);
                        }
                        if (slotStart >= item.task.deadline) {
                            continue;
                        }

                        const int freeMinutes = static_cast<int>(slotStart.secsTo(latestEnd) / 60);
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
                        block.explanation = blockExplanation(item.task, item.score, now, candidateSlot.matchReason, {slotStart, latestEnd}, item.task.preferredStudyTime);
                        result.generatedBlocks.push_back(block);

                        remaining -= chunk;
                        placedAny = true;
                        placedInPass = true;
                        consumeFreeInterval(free, slotIndex, block.start, block.end, config.breakMinutes);
                        break;
                    }
                    if (placedInPass) {
                        break;
                    }
                }
            }
        }

        if (remaining > 0 || !placedAny) {
            const int scheduledMinutes = qMax(0, item.task.remainingMinutes - remaining);
            result.unscheduledTaskIds.push_back(item.task.id);
            result.issues.push_back({
                item.task.id,
                item.task.title,
                "insufficient_capacity",
                placedAny ? QObject::tr("可用时间不足，任务只完成了部分排程")
                          : QObject::tr("截止时间前没有足够的可用时间"),
                placedAny ? QObject::tr("减少任务时长或延长截止时间")
                          : QObject::tr("尝试延长截止时间"),
                item.task.estimatedMinutes,
                scheduledMinutes,
                remaining,
                item.task.deadline,
                static_cast<int>(item.task.priority),
                item.task.categoryName
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
