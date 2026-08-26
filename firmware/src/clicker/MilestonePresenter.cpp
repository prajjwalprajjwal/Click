#include "clicker/MilestonePresenter.h"
#include <Arduino.h>
#include "screens/OledScreen.h"
#include "screens/ScreenDrawer.h"

void MilestonePresenter::start(const MilestoneEventInfo& eventInfo) {
    const CelebrationScreen* screen = findCelebrationScreen(eventInfo.lifetimeClicks);
    if (screen == nullptr) {
        return;
    }

    active = true;
    currentScreen = screen;
    stepStartMs = millis();
}

void MilestonePresenter::cancel() {
    active = false;
    currentScreen = nullptr;
}

void MilestonePresenter::update(uint32_t nowMs) {
    if (active && (nowMs - stepStartMs) >= SCREEN_HOLD_MS) {
        cancel();
    }
}

void MilestonePresenter::draw() {
    if (active && currentScreen != nullptr) {
        ScreenDrawer::drawCelebration(currentScreen);
    }
}
