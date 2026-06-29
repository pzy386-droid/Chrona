#pragma once

#include "core/models/Task.h"

#include <QVariantMap>

class PriorityEvaluator {
public:
    int score(const Task& task, const QDateTime& now) const;
    QVariantMap details(const Task& task, const QDateTime& now) const;
};