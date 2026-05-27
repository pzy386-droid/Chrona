#include "core/scheduler/PriorityEvaluator.h"

int PriorityEvaluator::score(const Task& task, const QDateTime& now) const
{
    int value = 0;
    switch (task.priority) {
    case Priority::High:
        value += 1000;
        break;
    case Priority::Medium:
        value += 500;
        break;
    case Priority::Low:
        value += 100;
        break;
    }

    const qint64 hoursUntilDeadline = now.secsTo(task.deadline) / 3600;
    if (hoursUntilDeadline < 0) {
        value += 1200;
    } else if (hoursUntilDeadline <= 24) {
        value += 700;
    } else if (hoursUntilDeadline <= 72) {
        value += 400;
    } else if (hoursUntilDeadline <= 168) {
        value += 150;
    }

    value += qMin(task.remainingMinutes / 30, 12) * 12;
    return value;
}
