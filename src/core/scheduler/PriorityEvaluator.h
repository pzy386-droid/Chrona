#pragma once

#include "core/models/Task.h"

class PriorityEvaluator {
public:
    int score(const Task& task, const QDateTime& now) const;
};
