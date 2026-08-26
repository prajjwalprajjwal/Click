#include "clicker/MilestoneManager.h"
#include "clicker/ClickerConfig.h"
#include <cstring>

static const MilestoneTrigger kMilestones[] = {
    {10, MilestoneId::M10, MilestoneEvent::HELLO_10},
    {25, MilestoneId::M25, MilestoneEvent::KEEP_GOING_25},
    {50, MilestoneId::M50, MilestoneEvent::ANIMATION_50},
    {69, MilestoneId::M69, MilestoneEvent::NICE_69},
    {100, MilestoneId::M100, MilestoneEvent::FIRST_SIGNAL},
    {111, MilestoneId::M111, MilestoneEvent::GLITCH_111},
    {250, MilestoneId::M250, MilestoneEvent::STILL_HERE_250},
    {500, MilestoneId::M500, MilestoneEvent::SIGNAL_02},
    {666, MilestoneId::M666, MilestoneEvent::PROBABLY_FINE_666},
    {1000, MilestoneId::M1000, MilestoneEvent::WE_HEARD_YOU_1000},
    {2000, MilestoneId::M2000, MilestoneEvent::I_REMEMBER_2000},
    {5000, MilestoneId::M5000, MilestoneEvent::PATTERN_FOUND},
    {10000, MilestoneId::M10000, MilestoneEvent::AWAKE},
    {12345, MilestoneId::M12345, MilestoneEvent::COUNTED_12345},
    {20000, MilestoneId::M20000, MilestoneEvent::WHY_DO_YOU_CLICK},
    {25000, MilestoneId::M25000, MilestoneEvent::MYSTERIOUS_BAR_25000},
    {30000, MilestoneId::M30000, MilestoneEvent::TRANSITION_30000},
    {40000, MilestoneId::M40000, MilestoneEvent::ALMOST_40000},
    {50000, MilestoneId::M50000, MilestoneEvent::HALFWAY},
    {60000, MilestoneId::M60000, MilestoneEvent::STILL_GOING_60000},
    {69420, MilestoneId::M69420, MilestoneEvent::NICE_69420},
    {75000, MilestoneId::M75000, MilestoneEvent::READY_75000},
    {80000, MilestoneId::M80000, MilestoneEvent::REMAIN_80000},
    {90000, MilestoneId::M90000, MilestoneEvent::DONT_STOP_90000},
    {99000, MilestoneId::M99000, MilestoneEvent::FINAL_APPROACH},
    {99900, MilestoneId::M99900, MilestoneEvent::FINAL_99900},
    {99990, MilestoneId::M99990, MilestoneEvent::FINAL_99990},
    {99999, MilestoneId::M99999, MilestoneEvent::FINAL_99999},
    {100000, MilestoneId::M100000, MilestoneEvent::CYCLE_COMPLETE},
    {100001, MilestoneId::M100001, MilestoneEvent::YOU_CAME_BACK_100001},
    {110000, MilestoneId::M110000, MilestoneEvent::CYCLE_02_110000},
    {125000, MilestoneId::M125000, MilestoneEvent::AGAIN_125000},
    {150000, MilestoneId::M150000, MilestoneEvent::HALFWAY_AGAIN_150000},
    {175000, MilestoneId::M175000, MilestoneEvent::SOMETHING_DIFFERENT_175000},
    {199999, MilestoneId::M199999, MilestoneEvent::FINAL_199999},
    {200000, MilestoneId::M200000, MilestoneEvent::CYCLE_02_COMPLETE},
};

static const size_t kMilestoneCount = sizeof(kMilestones) / sizeof(kMilestones[0]);

static uint8_t nextIndex(uint8_t index) {
    return static_cast<uint8_t>((index + 1) % 4);
}

bool MilestoneManager::isTriggered(MilestoneId id) const {
    uint8_t bit = static_cast<uint8_t>(id);
    uint8_t word = bit / 32;
    uint8_t offset = bit % 32;
    if (word >= MILESTONE_FLAG_WORDS) {
        return false;
    }
    return (flags[word] & (1UL << offset)) != 0;
}

void MilestoneManager::markTriggered(MilestoneId id) {
    uint8_t bit = static_cast<uint8_t>(id);
    uint8_t word = bit / 32;
    uint8_t offset = bit % 32;
    if (word < MILESTONE_FLAG_WORDS) {
        flags[word] |= (1UL << offset);
    }
}

void MilestoneManager::enqueue(MilestoneEvent event, MilestoneId id, const ClickCounter& counter) {
    uint8_t nextTail = nextIndex(pendingTail);
    if (nextTail == pendingHead) {
        return;
    }

    pendingEvents[pendingTail].event = event;
    pendingEvents[pendingTail].id = id;
    pendingEvents[pendingTail].lifetimeClicks = counter.getLifetimeClicks();
    pendingEvents[pendingTail].cycleNumber = counter.getCurrentCycleNumber();
    pendingEvents[pendingTail].cycleClicks = counter.getCycleClicks();
    pendingTail = nextTail;
}

void MilestoneManager::checkTable(const ClickCounter& counter, uint64_t& lastCycleCompleteLifetime) {
    uint64_t lifetime = counter.getLifetimeClicks();

    for (size_t i = 0; i < kMilestoneCount; i++) {
        const MilestoneTrigger& trigger = kMilestones[i];
        if (lifetime != trigger.lifetimeClicks) {
            continue;
        }
        if (isTriggered(trigger.id)) {
            continue;
        }

        markTriggered(trigger.id);
        enqueue(trigger.event, trigger.id, counter);

        if (trigger.event == MilestoneEvent::CYCLE_COMPLETE ||
            trigger.event == MilestoneEvent::CYCLE_02_COMPLETE) {
            lastCycleCompleteLifetime = lifetime;
        }
    }

    // Cycle 3+ completions (300000, 400000, ...) — deduped via lastCycleCompleteLifetime.
    if (lifetime > 200000ULL && (lifetime % CLICKER_CYCLE_LENGTH) == 0 &&
        lifetime != lastCycleCompleteLifetime) {
        lastCycleCompleteLifetime = lifetime;
        enqueue(MilestoneEvent::CYCLE_COMPLETE, MilestoneId::M100000, counter);
    }
}

void MilestoneManager::loadFlags(const uint32_t loaded[MILESTONE_FLAG_WORDS]) {
    memcpy(flags, loaded, MILESTONE_FLAG_WORDS * sizeof(uint32_t));
}

void MilestoneManager::getFlags(uint32_t out[MILESTONE_FLAG_WORDS]) const {
    memcpy(out, flags, MILESTONE_FLAG_WORDS * sizeof(uint32_t));
}

void MilestoneManager::checkAfterClick(const ClickCounter& counter, uint64_t& lastCycleCompleteLifetime) {
    checkTable(counter, lastCycleCompleteLifetime);
}

bool MilestoneManager::hasPendingEvent() const {
    return pendingHead != pendingTail;
}

MilestoneEventInfo MilestoneManager::popEvent() {
    MilestoneEventInfo info;
    if (!hasPendingEvent()) {
        return info;
    }

    info = pendingEvents[pendingHead];
    pendingHead = nextIndex(pendingHead);
    return info;
}

void MilestoneManager::resetFlags() {
    memset(flags, 0, sizeof(flags));
    pendingHead = 0;
    pendingTail = 0;
}
