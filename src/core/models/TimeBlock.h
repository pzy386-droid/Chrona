#pragma once

#include "core/models/ScheduleTypes.h"

#include <QJsonObject>

struct TimeBlock {
    int id = 0;
    int taskId = 0;
    QDateTime start;
    QDateTime end;
    BlockSource source = BlockSource::Auto;
    int scheduleRunId = 0;
    QString createdAt;
    QString explanation;
    QDateTime completedAt;

    QJsonObject toJson() const;
    static TimeBlock fromJson(const QJsonObject& object);
};
