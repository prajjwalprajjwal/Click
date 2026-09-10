#include "clicker/PersistenceManager.h"
#include "clicker/ClickerConfig.h"
#include <cstring>
#include <cstdio>

static uint64_t packLifetime(uint32_t lo, uint32_t hi) {
    return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
}

static void unpackLifetime(uint64_t value, uint32_t& lo, uint32_t& hi) {
    lo = static_cast<uint32_t>(value & 0xFFFFFFFFULL);
    hi = static_cast<uint32_t>(value >> 32);
}

bool PersistenceManager::begin() {
    return prefs.begin(CLICKER_NVS_NAMESPACE, false);
}

bool PersistenceManager::load(ClickCounter& counter, uint32_t milestoneFlags[MILESTONE_FLAG_WORDS]) {
    if (!prefs.isKey(CLICKER_NVS_KEY_COUNT_STR) && !prefs.isKey(CLICKER_NVS_KEY_LIFETIME_LO)) {
        counter.reset();
        memset(milestoneFlags, 0, MILESTONE_FLAG_WORDS * sizeof(uint32_t));
        homeUnlockFlags = 0;
        lastCycleCompleteLifetime = 0;
        terrainOffset = 0;
        dirty = false;
        clicksSincePersist = 0;
        lastClickTime = 0;
        lastPersistTime = millis();
        return false;
    }

    if (prefs.isKey(CLICKER_NVS_KEY_COUNT_STR)) {
        String savedStr = prefs.getString(CLICKER_NVS_KEY_COUNT_STR, "0");
        counter.setString(savedStr.c_str());
    } else {
        // Fallback to legacy uint64 format
        uint32_t lo = prefs.getUInt(CLICKER_NVS_KEY_LIFETIME_LO, 0);
        uint32_t hi = prefs.getUInt(CLICKER_NVS_KEY_LIFETIME_HI, 0);
        uint64_t lifetime = packLifetime(lo, hi);
        uint32_t completed = prefs.getUInt(CLICKER_NVS_KEY_COMPLETED_CYCLES, 0);
        counter.load(lifetime, completed);
    }

    terrainOffset = prefs.getUInt(CLICKER_NVS_KEY_TERRAIN_OFFSET, 0);

    size_t expectedSize = MILESTONE_FLAG_WORDS * sizeof(uint32_t);
    size_t readSize = prefs.getBytesLength(CLICKER_NVS_KEY_MILESTONES);
    if (readSize == expectedSize) {
        prefs.getBytes(CLICKER_NVS_KEY_MILESTONES, milestoneFlags, expectedSize);
    } else {
        memset(milestoneFlags, 0, expectedSize);
    }

    homeUnlockFlags = prefs.getUInt(CLICKER_NVS_KEY_HOME_UNLOCKS, 0);

    uint32_t lo = prefs.getUInt(CLICKER_NVS_KEY_LAST_CYCLE, 0);
    uint32_t hi = prefs.getUInt(CLICKER_NVS_KEY_LAST_CYCLE_HI, 0);
    lastCycleCompleteLifetime = packLifetime(lo, hi);

    dirty = false;
    clicksSincePersist = 0;
    lastClickTime = 0;
    lastPersistTime = millis();

    // Cache initial saved state to deduplicate subsequent writes
    strncpy(lastSavedCountStr, counter.getString(), sizeof(lastSavedCountStr) - 1);
    lastSavedCountStr[sizeof(lastSavedCountStr) - 1] = '\0';
    lastSavedTerrainOffset = terrainOffset;
    lastSavedHomeUnlocks = homeUnlockFlags;
    lastSavedLastCycle = lastCycleCompleteLifetime;
    memcpy(lastSavedMilestones, milestoneFlags, sizeof(lastSavedMilestones));

    return true;
}

bool PersistenceManager::save(const ClickCounter& counter, const uint32_t milestoneFlags[MILESTONE_FLAG_WORDS]) {
    const char* curStr = counter.getString();
    if (strncmp(lastSavedCountStr, curStr, sizeof(lastSavedCountStr)) != 0) {
        prefs.putString(CLICKER_NVS_KEY_COUNT_STR, curStr);
        strncpy(lastSavedCountStr, curStr, sizeof(lastSavedCountStr) - 1);
        lastSavedCountStr[sizeof(lastSavedCountStr) - 1] = '\0';

        // Save legacy values for backwards compatibility if within range
        uint64_t lifetime = counter.getLifetimeClicks();
        if (lifetime != UINT64_MAX) {
            uint32_t lo = 0;
            uint32_t hi = 0;
            unpackLifetime(lifetime, lo, hi);
            prefs.putUInt(CLICKER_NVS_KEY_LIFETIME_LO, lo);
            prefs.putUInt(CLICKER_NVS_KEY_LIFETIME_HI, hi);
            prefs.putUInt(CLICKER_NVS_KEY_COMPLETED_CYCLES, counter.getCompletedCycles());
        }
    }

    if (terrainOffset != lastSavedTerrainOffset) {
        prefs.putUInt(CLICKER_NVS_KEY_TERRAIN_OFFSET, terrainOffset);
        lastSavedTerrainOffset = terrainOffset;
    }

    if (memcmp(lastSavedMilestones, milestoneFlags, sizeof(lastSavedMilestones)) != 0) {
        prefs.putBytes(CLICKER_NVS_KEY_MILESTONES, milestoneFlags, MILESTONE_FLAG_WORDS * sizeof(uint32_t));
        memcpy(lastSavedMilestones, milestoneFlags, sizeof(lastSavedMilestones));
    }

    if (homeUnlockFlags != lastSavedHomeUnlocks) {
        prefs.putUInt(CLICKER_NVS_KEY_HOME_UNLOCKS, homeUnlockFlags);
        lastSavedHomeUnlocks = homeUnlockFlags;
    }

    if (lastCycleCompleteLifetime != lastSavedLastCycle) {
        uint32_t lo = 0;
        uint32_t hi = 0;
        unpackLifetime(lastCycleCompleteLifetime, lo, hi);
        prefs.putUInt(CLICKER_NVS_KEY_LAST_CYCLE, lo);
        prefs.putUInt(CLICKER_NVS_KEY_LAST_CYCLE_HI, hi);
        lastSavedLastCycle = lastCycleCompleteLifetime;
    }

    dirty = false;
    clicksSincePersist = 0;
    lastPersistTime = millis();

#if CLICKER_DEBUG
    Serial.print("[SISYPHUS] saved = ");
    Serial.println(counter.getString());
#endif
    return true;
}

void PersistenceManager::markDirty(uint32_t now) {
    dirty = true;
    lastClickTime = (now != 0) ? now : millis();
}

void PersistenceManager::onClickRecorded(uint32_t now) {
    clicksSincePersist++;
    dirty = true;
    lastClickTime = (now != 0) ? now : millis();
}

bool PersistenceManager::shouldPersist(uint32_t now) const {
    if (!dirty) {
        return false;
    }
    // 1. Debounce inactivity: user stopped clicking for IDLE_DELAY_MS ("counting is done")
    if ((now - lastClickTime) >= CLICKER_PERSIST_IDLE_DELAY_MS) {
        return true;
    }
    // 2. Periodic clicks during continuous clicking session
    if (clicksSincePersist >= CLICKER_PERSIST_EVERY_N_CLICKS) {
        return true;
    }
    // 3. Max time elapsed since last persist during continuous clicking
    if ((now - lastPersistTime) >= CLICKER_PERSIST_MAX_INTERVAL_MS) {
        return true;
    }
    return false;
}

void PersistenceManager::clearPeriodicCounter() {
    clicksSincePersist = 0;
}

bool PersistenceManager::flush(const ClickCounter& counter, const uint32_t milestoneFlags[MILESTONE_FLAG_WORDS]) {
    if (!dirty) {
        return true;
    }
    return save(counter, milestoneFlags);
}
