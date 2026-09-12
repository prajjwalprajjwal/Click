#ifndef CLICK_COUNTER_H
#define CLICK_COUNTER_H

#include <stdint.h>
#include <stddef.h>
#include "ClickerConfig.h"

class ClickCounter {
public:
    static const size_t MAX_DIGITS = 128; // Up to 127 decimal digits without overflow

private:
    char digits[MAX_DIGITS];
    uint64_t cachedLifetime = 0;
    uint32_t completedCycles = 0;

    void syncLegacyValues();

public:
    ClickCounter();

    void load(uint64_t lifetime, uint32_t completed);
    void setString(const char* str);
    const char* getString() const { return digits; }

    void reset();
    bool increment();

    uint64_t getLifetimeClicks() const { return cachedLifetime; }
    uint32_t getCompletedCycles() const { return completedCycles; }

    uint32_t getCycleClicks() const;
    uint32_t getCurrentCycleNumber() const;

    void setLifetimeClicks(uint64_t value);
};

#endif // CLICK_COUNTER_H
