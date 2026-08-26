#ifndef CLICK_COUNTER_H
#define CLICK_COUNTER_H

#include <stdint.h>
#include "clicker/ClickerConfig.h"

class ClickCounter {
private:
    uint64_t lifetimeClicks = 0;
    uint32_t completedCycles = 0;

public:
    void load(uint64_t lifetime, uint32_t completed);
    void reset();

    bool increment();

    uint64_t getLifetimeClicks() const { return lifetimeClicks; }
    uint32_t getCompletedCycles() const { return completedCycles; }

    uint32_t getCycleClicks() const;
    uint32_t getCurrentCycleNumber() const;

    void setLifetimeClicks(uint64_t value);
};

#endif // CLICK_COUNTER_H
