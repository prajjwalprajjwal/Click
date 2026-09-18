#ifndef COUNTER_APPLET_H
#define COUNTER_APPLET_H

#include <stdint.h>
#include "Applet.h"
#include "ClickCounter.h"
#include "PersistenceManager.h"
#include "CounterRenderer.h"

enum class SisyphusAnimMode : uint8_t {
    IDLE_WALKING,
    PUSHING,
    ROLLING_BACK
};

class CounterApplet : public Applet {
private:
    ClickCounter counter;
    PersistenceManager persistence;
    CounterRenderer renderer;

    uint32_t milestoneFlags[MILESTONE_FLAG_WORDS] = {0};
    bool storageReady = false;

    // Animation & Gameplay State (stored during session)
    SisyphusAnimMode animMode = SisyphusAnimMode::IDLE_WALKING;
    uint8_t pushFrame = 0;
    uint8_t boulderRotPhase = 0;
    uint8_t pendingPushes = 0;

    int32_t climbProgress = 0; // Persistent uphill position during session
    float worldProgress = 0.0f;

    uint32_t lastClickTime = 0;
    uint32_t lastFrameTime = 0;
    uint32_t lastDisplayTime = 0;
    uint32_t lastCloudDriftTime = 0;
    bool frameDirty = true;

    // Paced Frame Timing (25 FPS push / 8 FPS walk / 30 FPS display refresh cap / 10 FPS idle cloud drift / 1s rollback)
    static const uint32_t PUSH_FRAME_INTERVAL_MS = 40;  // 25 FPS
    static const uint32_t WALK_FRAME_INTERVAL_MS = 125; // 8 FPS
    static const uint32_t MIN_DISPLAY_INTERVAL_MS = 33; // 30 FPS ceiling to protect I2C bus
    static const uint32_t CLOUD_DRIFT_INTERVAL_MS = 100; // 10 FPS automatic cloud drift
    static const uint32_t INACTIVITY_ROLLBACK_DELAY_MS = 1000; // 1 second idle before rolling downhill
    static const uint32_t ROLLBACK_FRAME_INTERVAL_MS = 40;     // 25 FPS rollback

    void loadState();
    void persistIfNeeded(bool force);
    void handleClick();
    void resetAll();
    void updateAnimation(uint32_t now);

#if CLICKER_DEBUG
    void debugSimulateCount(const char* target);
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
    void persistNow();
    void setLifetimeClicks(uint64_t val);
    uint64_t getLifetimeClicks() const { return counter.getLifetimeClicks(); }
    const char* getCountString() const { return counter.getString(); }
};

#endif // COUNTER_APPLET_H
