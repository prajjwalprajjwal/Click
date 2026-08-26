#include "CounterApplet.h"
#include "clicker/ClickerConfig.h"
#include "HomeApplet.h"

extern HomeApplet homeApplet;

void CounterApplet::loadState() {
    if (!storageReady) {
        storageReady = persistence.begin();
    }
    if (!storageReady) {
        Serial.println("[Counter] NVS unavailable, running in RAM only");
        counter.reset();
        milestones.resetFlags();
        memset(milestoneFlags, 0, sizeof(milestoneFlags));
        return;
    }

    persistence.load(counter, milestoneFlags);
    milestones.loadFlags(milestoneFlags);

#if CLICKER_DEBUG
    Serial.print("[Counter] Loaded lifetime=");
    Serial.print((unsigned long long)counter.getLifetimeClicks());
    Serial.print(" cycles=");
    Serial.print(counter.getCompletedCycles());
    Serial.print(" cycleClicks=");
    Serial.println(counter.getCycleClicks());
#endif
}

void CounterApplet::persistNow() {
    if (!storageReady) {
        return;
    }

    milestones.getFlags(milestoneFlags);
    persistence.flush(counter, milestoneFlags);
}

void CounterApplet::persistIfNeeded(bool forceMilestone) {
    if (!storageReady) {
        return;
    }

    uint32_t now = millis();
    if (forceMilestone || persistence.shouldPersist(now)) {
        milestones.getFlags(milestoneFlags);
        persistence.flush(counter, milestoneFlags);
        persistence.clearPeriodicCounter();
    }
}

void CounterApplet::handleMilestoneEvent(const MilestoneEventInfo& info) {
    if (info.event == MilestoneEvent::NONE) {
        return;
    }

    switch (info.event) {
        case MilestoneEvent::FIRST_SIGNAL:
            persistence.setHomeUnlockFlags(persistence.getHomeUnlockFlags() | 0x01);
            break;
        case MilestoneEvent::SIGNAL_02:
            persistence.setHomeUnlockFlags(persistence.getHomeUnlockFlags() | 0x02);
            break;
        case MilestoneEvent::PATTERN_FOUND:
            persistence.setHomeUnlockFlags(persistence.getHomeUnlockFlags() | 0x04);
            break;
        case MilestoneEvent::AWAKE:
            persistence.setHomeUnlockFlags(persistence.getHomeUnlockFlags() | 0x08);
            break;
        case MilestoneEvent::READY_75000:
            persistence.setHomeUnlockFlags(persistence.getHomeUnlockFlags() | 0x10);
            break;
        case MilestoneEvent::TRANSITION_30000:
            persistence.setHomeUnlockFlags(persistence.getHomeUnlockFlags() | 0x20);
            break;
        case MilestoneEvent::SOMETHING_DIFFERENT_175000:
            persistence.setHomeUnlockFlags(persistence.getHomeUnlockFlags() | 0x40);
            break;
        default:
            break;
    }

    homeApplet.applyUnlockState(persistence.getHomeUnlockFlags());
    presenter.start(info);
    persistIfNeeded(true);
}

void CounterApplet::processPendingEvents() {
    while (milestones.hasPendingEvent()) {
        handleMilestoneEvent(milestones.popEvent());
    }
}

void CounterApplet::handleClick() {
    if (!counter.increment()) {
        return;
    }

    uint32_t now = millis();
    persistence.onClickRecorded(now);

    uint64_t lastCycleComplete = persistence.getLastCycleCompleteLifetime();
    milestones.checkAfterClick(counter, lastCycleComplete);
    persistence.setLastCycleCompleteLifetime(lastCycleComplete);

    processPendingEvents();
    persistIfNeeded(false);
}

#if CLICKER_DEBUG
void CounterApplet::debugSimulateCount(uint64_t target) {
    counter.setLifetimeClicks(target);

    uint64_t lastCycleComplete = persistence.getLastCycleCompleteLifetime();
    if (target > 0 && (target % CLICKER_CYCLE_LENGTH) == 0) {
        lastCycleComplete = target - CLICKER_CYCLE_LENGTH;
    } else {
        lastCycleComplete = (target / CLICKER_CYCLE_LENGTH) * CLICKER_CYCLE_LENGTH;
    }
    persistence.setLastCycleCompleteLifetime(lastCycleComplete);

    milestones.checkAfterClick(counter, lastCycleComplete);
    persistence.setLastCycleCompleteLifetime(lastCycleComplete);

    processPendingEvents();
    persistIfNeeded(true);

    Serial.print("[Counter] Simulated lifetime=");
    Serial.print((unsigned long long)counter.getLifetimeClicks());
    Serial.print(" cycle=");
    Serial.print(counter.getCurrentCycleNumber());
    Serial.print(" cycleClicks=");
    Serial.println(counter.getCycleClicks());
}

void CounterApplet::processDebugSerial() {
    if (!Serial.available()) {
        return;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
        return;
    }

    if (line.equalsIgnoreCase("info")) {
        Serial.print("[Counter] lifetime=");
        Serial.print((unsigned long long)counter.getLifetimeClicks());
        Serial.print(" completedCycles=");
        Serial.print(counter.getCompletedCycles());
        Serial.print(" cycleClicks=");
        Serial.print(counter.getCycleClicks());
        Serial.print(" cycle=");
        Serial.println(counter.getCurrentCycleNumber());
        return;
    }

    if (line.startsWith("sim ")) {
        uint64_t target = strtoull(line.substring(4).c_str(), nullptr, 10);
        debugSimulateCount(target);
        return;
    }

    if (line.startsWith("+")) {
        uint64_t delta = strtoull(line.substring(1).c_str(), nullptr, 10);
        uint64_t target = counter.getLifetimeClicks() + delta;
        debugSimulateCount(target);
        return;
    }

    Serial.println("[Counter] Debug commands: info | sim <count> | +<delta>");
}
#endif

void CounterApplet::resetAll() {
    counter.reset();
    milestones.resetFlags();
    memset(milestoneFlags, 0, sizeof(milestoneFlags));
    persistence.setHomeUnlockFlags(0);
    persistence.setLastCycleCompleteLifetime(0);
    persistence.markDirty();
    presenter.cancel();
    homeApplet.applyUnlockState(0);
    persistNow();

#if CLICKER_DEBUG
    Serial.println("[Counter] Reset — lifetime count cleared");
#endif
}

void CounterApplet::preloadState() {
    loadState();
    homeApplet.applyUnlockState(persistence.getHomeUnlockFlags());
}

void CounterApplet::init() {
    preloadState();

#if CLICKER_DEBUG
    Serial.println("[Counter] Debug enabled. Commands: info | sim <count> | +<delta>");
#endif
}

void CounterApplet::update() {
    uint32_t now = millis();
    presenter.update(now);
    persistIfNeeded(false);

#if CLICKER_DEBUG
    processDebugSerial();
#endif
}

void CounterApplet::draw() {
    if (presenter.isActive()) {
        presenter.draw();
        return;
    }

    renderer.drawNormal(
        counter.getLifetimeClicks(),
        counter.getCycleClicks());
}

void CounterApplet::cleanup() {
    persistNow();
}

void CounterApplet::onPrepareSleep() {
    persistNow();
}

void CounterApplet::onActionClick() {
    handleClick();
}

void CounterApplet::onBothHeld() {
    resetAll();
}
