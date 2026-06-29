#include "core/scheduler/PriorityEvaluator.h"

#include <QtGlobal>

namespace {
int basePriority(Priority priority)
{
    switch (priority) {
    case Priority::High: return 850;
    case Priority::Medium: return 500;
    case Priority::Low: return 220;
    }
    return 500;
}

QString levelForScore(int score)
{
    if (score >= 1700) return QStringLiteral("critical");
    if (score >= 1150) return QStringLiteral("high");
    if (score >= 700) return QStringLiteral("medium");
    return QStringLiteral("low");
}
}

int PriorityEvaluator::score(const Task& task, const QDateTime& now) const
{
    int value = basePriority(task.priority);
    const qint64 minutesUntilDeadline = now.secsTo(task.deadline) / 60;

    if (minutesUntilDeadline < 0) value += 1500;
    else if (minutesUntilDeadline <= 6 * 60) value += 1100;
    else if (minutesUntilDeadline <= 24 * 60) value += 800;
    else if (minutesUntilDeadline <= 72 * 60) value += 500;
    else if (minutesUntilDeadline <= 7 * 24 * 60) value += 260;
    else if (minutesUntilDeadline <= 14 * 24 * 60) value += 100;

    if (minutesUntilDeadline > 0) {
        const double workloadRatio = task.remainingMinutes / static_cast<double>(qMax<qint64>(30, minutesUntilDeadline));
        value += qMin(900, qRound(workloadRatio * 1800.0));
    }
    value += qMin(task.remainingMinutes / 30, 16) * 10;
    value += qBound(0, task.effortLevel, 2) * 45;
    if (task.deadlineType == DeadlineType::Hard) value += 320;
    if (task.scheduleStatus == ScheduleStatus::CouldNotFit) value += 260;
    if (task.startDate.isValid() && task.startDate > now) {
        value -= qMin(static_cast<int>(now.secsTo(task.startDate) / 60), 24 * 60) / 10;
    }
    return qMax(0, value);
}

QVariantMap PriorityEvaluator::details(const Task& task, const QDateTime& now) const
{
    const int value = score(task, now);
    const qint64 minutesUntilDeadline = now.secsTo(task.deadline) / 60;
    QStringList reasons;
    if (minutesUntilDeadline < 0) reasons.push_back(QObject::tr("已超过截止时间"));
    else if (minutesUntilDeadline <= 24 * 60) reasons.push_back(QObject::tr("24 小时内截止"));
    else if (minutesUntilDeadline <= 72 * 60) reasons.push_back(QObject::tr("三天内截止"));
    if (task.deadlineType == DeadlineType::Hard) reasons.push_back(QObject::tr("硬截止不可移动"));
    if (task.scheduleStatus == ScheduleStatus::CouldNotFit) reasons.push_back(QObject::tr("当前容量未完整排入"));
    if (task.remainingMinutes >= 180) reasons.push_back(QObject::tr("剩余工作量较大"));
    if (task.priority == Priority::High) reasons.push_back(QObject::tr("用户标记高优先级"));
    if (reasons.isEmpty()) reasons.push_back(QObject::tr("按常规节奏推进"));

    return {
        {QStringLiteral("score"), value},
        {QStringLiteral("level"), levelForScore(value)},
        {QStringLiteral("reasons"), reasons},
        {QStringLiteral("minutesUntilDeadline"), minutesUntilDeadline}
    };
}