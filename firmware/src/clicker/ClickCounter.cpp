#include "clicker/ClickCounter.h"

void ClickCounter::load(uint64_t lifetime, uint32_t completed) {
    lifetimeClicks = lifetime;
    completedCycles = completed;
}

void ClickCounter::reset() {
    lifetimeClicks = 0;
    completedCycles = 0;
}

bool ClickCounter::increment() {
    if (lifetimeClicks == UINT64_MAX) {
        return false;
    }

    lifetimeClicks++;
    completedCycles = static_cast<uint32_t>(lifetimeClicks / CLICKER_CYCLE_LENGTH);
    return true;
}

uint32_t ClickCounter::getCycleClicks() const {
    return static_cast<uint32_t>(lifetimeClicks - (static_cast<uint64_t>(completedCycles) * CLICKER_CYCLE_LENGTH));
}

uint32_t ClickCounter::getCurrentCycleNumber() const {
    if (lifetimeClicks == 0) {
        return 1;
    }
    return completedCycles + 1;
}

void ClickCounter::setLifetimeClicks(uint64_t value) {
    lifetimeClicks = value;
    completedCycles = static_cast<uint32_t>(lifetimeClicks / CLICKER_CYCLE_LENGTH);
}
