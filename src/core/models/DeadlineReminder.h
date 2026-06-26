#pragma once

#include <QDateTime>
#include <QString>

enum class DeadlineReminderStatus {
    Open = 0,
    Done = 1,
    Archived = 2
};

struct DeadlineReminder {
    int id = 0;
    QString title;
    QString notes;
    QDateTime dueAt;
    QString categoryName;
    QString categoryColor;
    int remindDaysBefore = 3;
    DeadlineReminderStatus status = DeadlineReminderStatus::Open;
    QDateTime completedAt;
};
