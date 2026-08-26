#ifndef COUNTER_RENDERER_H
#define COUNTER_RENDERER_H

#include <stdint.h>
#include <stddef.h>

class CounterRenderer {
private:
    void formatCount(uint64_t count, char* buffer, size_t bufferSize) const;
    void drawCountCentered(uint64_t count) const;

public:
    bool shouldShowRemaining(uint32_t cycleClicks) const;
    uint64_t getRemainingInCycle(uint64_t lifetimeClicks) const;

    void drawNormal(uint64_t lifetimeClicks, uint32_t cycleClicks);
    void drawRemaining(uint64_t remaining);
};

#endif // COUNTER_RENDERER_H
