#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

enum class Priority {
    Low = 0,
    Medium = 1,
    High = 2
};

enum class TaskStatus {
    Inbox = 0,
    Scheduled = 1,
    Active = 2,
    Done = 3,
    Archived = 4
};

enum class EventType {
    Course = 0,
    FixedEvent = 1,
    Unavailable = 2
};

enum class BlockSource {
    Auto = 0,
    UserDragged = 1,
    Locked = 2
};

struct ScheduleWindow {
    QDateTime start;
    QDateTime end;
};

struct SchedulingConfig {
    int workdayStartHour = 8;
    int workdayEndHour = 23;
    int minimumBlockMinutes = 30;
    int preferredBlockMinutes = 90;
    int breakMinutes = 10;
};

struct TimeInterval {
    QDateTime start;
    QDateTime end;

    int minutes() const
    {
        return static_cast<int>(start.secsTo(end) / 60);
    }
};
