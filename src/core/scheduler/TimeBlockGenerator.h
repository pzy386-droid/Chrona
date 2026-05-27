#pragma once

#include "core/models/ScheduleTypes.h"

class TimeBlockGenerator {
public:
    int nextChunkMinutes(int remainingMinutes, int freeMinutes, const SchedulingConfig& config) const;
};
