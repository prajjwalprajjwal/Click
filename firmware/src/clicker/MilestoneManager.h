#ifndef MILESTONE_MANAGER_H
#define MILESTONE_MANAGER_H

#include <stdint.h>
#include "clicker/ClickCounter.h"
#include "clicker/MilestoneTypes.h"

class MilestoneManager {
private:
    uint32_t flags[MILESTONE_FLAG_WORDS] = {0};
    MilestoneEventInfo pendingEvents[4];
    uint8_t pendingHead = 0;
    uint8_t pendingTail = 0;

    bool isTriggered(MilestoneId id) const;
    void markTriggered(MilestoneId id);
    void enqueue(MilestoneEvent event, MilestoneId id, const ClickCounter& counter);
    void checkTable(const ClickCounter& counter, uint64_t& lastCycleCompleteLifetime);

public:
    void checkAfterClick(const ClickCounter& counter, uint64_t& lastCycleCompleteLifetime);
    void loadFlags(const uint32_t loaded[MILESTONE_FLAG_WORDS]);
    void getFlags(uint32_t out[MILESTONE_FLAG_WORDS]) const;

    bool hasPendingEvent() const;
    MilestoneEventInfo popEvent();

    void resetFlags();
};

#endif // MILESTONE_MANAGER_H
