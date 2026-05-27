#include "core/scheduler/TimeBlockGenerator.h"

#include <QtGlobal>

int TimeBlockGenerator::nextChunkMinutes(int remainingMinutes, int freeMinutes, const SchedulingConfig& config) const
{
    const int target = qMin(config.preferredBlockMinutes, remainingMinutes);
    const int chunk = qMin(target, freeMinutes);
    if (chunk < config.minimumBlockMinutes && remainingMinutes > config.minimumBlockMinutes) {
        return 0;
    }
    return chunk;
}
