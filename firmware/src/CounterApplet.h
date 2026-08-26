#ifndef COUNTER_APPLET_H
#define COUNTER_APPLET_H

#include <stdint.h>
#include "Applet.h"
#include "clicker/ClickCounter.h"
#include "clicker/PersistenceManager.h"
#include "clicker/MilestoneManager.h"
#include "clicker/CounterRenderer.h"
#include "clicker/MilestonePresenter.h"

class CounterApplet : public Applet {
private:
    ClickCounter counter;
    PersistenceManager persistence;
    MilestoneManager milestones;
    CounterRenderer renderer;
    MilestonePresenter presenter;

    uint32_t milestoneFlags[MILESTONE_FLAG_WORDS] = {0};
    bool storageReady = false;

    void loadState();
    void persistNow();
    void persistIfNeeded(bool forceMilestone);
    void handleClick();
    void processPendingEvents();
    void handleMilestoneEvent(const MilestoneEventInfo& info);
    void resetAll();

#if CLICKER_DEBUG
    void debugSimulateCount(uint64_t target);
    void processDebugSerial();
#endif

public:
    void init() override;
    void update() override;
    void draw() override;
    void cleanup() override;
    void onPrepareSleep() override;

    void onActionClick() override;
    void onBothHeld() override;

    void preloadState();
};

#endif // COUNTER_APPLET_H
