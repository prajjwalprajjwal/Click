#include "clicker/ClickCounter.h"
#include <cstring>
#include <cstdio>
#include <cctype>

ClickCounter::ClickCounter() {
    reset();
}

void ClickCounter::syncLegacyValues() {
    cachedLifetime = 0;
    const char* p = digits;
    while (*p) {
        if (!isdigit((unsigned char)*p)) break;
        if (cachedLifetime > (UINT64_MAX - (*p - '0')) / 10) {
            cachedLifetime = UINT64_MAX;
            break;
        }
        cachedLifetime = cachedLifetime * 10 + (*p - '0');
        p++;
    }
    completedCycles = static_cast<uint32_t>(cachedLifetime / CLICKER_CYCLE_LENGTH);
}

void ClickCounter::load(uint64_t lifetime, uint32_t completed) {
    snprintf(digits, sizeof(digits), "%llu", (unsigned long long)lifetime);
    cachedLifetime = lifetime;
    completedCycles = completed;
}

void ClickCounter::setString(const char* str) {
    if (!str || !*str) {
        reset();
        return;
    }

    // Skip any leading non-digits or spaces
    while (*str && !isdigit((unsigned char)*str)) {
        str++;
    }
    // Skip leading zeroes, but leave one zero if the number is "0"
    while (*str == '0' && isdigit((unsigned char)*(str + 1))) {
        str++;
    }

    if (!*str) {
        reset();
        return;
    }

    size_t len = 0;
    while (str[len] && isdigit((unsigned char)str[len]) && len < (MAX_DIGITS - 1)) {
        digits[len] = str[len];
        len++;
    }
    digits[len] = '\0';
    syncLegacyValues();
}

void ClickCounter::reset() {
    digits[0] = '0';
    digits[1] = '\0';
    cachedLifetime = 0;
    completedCycles = 0;
}

bool ClickCounter::increment() {
    size_t len = strlen(digits);
    int i = static_cast<int>(len) - 1;

    // Ripple carry backwards
    while (i >= 0 && digits[i] == '9') {
        digits[i] = '0';
        i--;
    }

    if (i >= 0) {
        digits[i]++;
    } else {
        // All were 9s e.g. "999" -> "1000"
        if (len + 1 >= MAX_DIGITS) {
            return false; // Safety limit
        }
        memmove(digits + 1, digits, len + 1);
        digits[0] = '1';
    }

    syncLegacyValues();
    return true;
}

uint32_t ClickCounter::getCycleClicks() const {
    return static_cast<uint32_t>(cachedLifetime - (static_cast<uint64_t>(completedCycles) * CLICKER_CYCLE_LENGTH));
}

uint32_t ClickCounter::getCurrentCycleNumber() const {
    if (cachedLifetime == 0) {
        return 1;
    }
    return completedCycles + 1;
}

void ClickCounter::setLifetimeClicks(uint64_t value) {
    load(value, static_cast<uint32_t>(value / CLICKER_CYCLE_LENGTH));
}
