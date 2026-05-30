#pragma once

#include <QTime>
#include <QString>
#include <optional>

struct StudyFrame {
    int id = 0;
    QString name;
    int dayOfWeek = 1;
    QTime startTime;
    QTime endTime;
    std::optional<int> categoryId;
    QString categoryName;
    QString categoryColor;
    QString energyLevel;
    bool enabled = true;
};

