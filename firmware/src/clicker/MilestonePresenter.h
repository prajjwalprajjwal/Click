#ifndef MILESTONE_PRESENTER_H
#define MILESTONE_PRESENTER_H

#include <stdint.h>
#include "clicker/MilestoneTypes.h"

struct CelebrationScreen;

class MilestonePresenter {
private:
    static const uint32_t SCREEN_HOLD_MS = 3000;

    bool active = false;
    const CelebrationScreen* currentScreen = nullptr;
    uint32_t stepStartMs = 0;

public:
    void start(const MilestoneEventInfo& eventInfo);
    void update(uint32_t nowMs);
    void draw();

    bool isActive() const { return active; }
    void cancel();
};

#endif // MILESTONE_PRESENTER_H
