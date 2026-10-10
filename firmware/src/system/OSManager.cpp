#include "OSManager.h"
#include "Display.h"
#include "PowerManager.h"
#include "WS2812.h"
#include "SoundFX.h"
#include "battery.hpp"

OSManager::OSManager() = default;

void OSManager::init() {
#if defined(TARGET_RP2354) && HAS_POWMAN_TIMER
    PowerManager::dispatch_pstate_resume();
#endif
    inputManager.init();
    recordActivity();
    
    if (appletCount > 0) {
        currentApplet = applets[0];
        currentApplet->init();
    }
    
    Serial.println("[OSManager] Initialized. Home screen default. Screensaver: 3m, Sleep: 10m");
}

void OSManager::update() {
    inputManager.update();
    
    // Only record activity continuously while display is on
    if (displayOn && (isActionButtonPressed() || isModeButtonPressed())) {
        recordActivity();
    }
    
    // Check if we should transition to sleep states
    checkSleepConditions();
    
    // Only update applet if awake and display is active
    if (sleepState == AWAKE && displayOn && currentApplet) {
        currentApplet->update();
    }
}

void OSManager::draw() {
    if (sleepState == AWAKE && displayOn && currentApplet) {
        currentApplet->draw();
    }
}

bool OSManager::recordActivity(uint8_t button) {
    uint32_t now = millis();
    bool wokeDisplay = false;

    // Double-click same-button wake handling if display is off during standby
    if (!displayOn && sleepState == AWAKE) {
        if (button == 0) {
            return true; // Background timer or continuous contact doesn't wake screen
        }

        if (firstWakeClickTime == 0 || (now - firstWakeClickTime) > 500 || firstWakeClickButton != button) {
            // First click on this button, or previous window expired, or different button pressed:
            // Record this button and start 500ms window.
            // Do NOT turn on screen!
            firstWakeClickTime = now;
            firstWakeClickButton = button;
            return true; // Consume click so it doesn't trigger applet actions
        } else if (firstWakeClickButton == button && (now - firstWakeClickTime) <= 500) {
            // Second click on the SAME button within 500ms window!
            firstWakeClickTime = 0;
            firstWakeClickButton = 0;
            displayOn = true;
            isDimmed = false;
            lastActivityTime = now;
            display.ssd1306_command(SSD1306_DISPLAYON);
            display.ssd1306_command(SSD1306_SETCONTRAST);
            display.ssd1306_command(0x3F);
            if (currentApplet) {
                currentApplet->init();
                currentApplet->draw();
            }
            display.display();
            return true;
        }
    }

    lastActivityTime = now;
    firstWakeClickTime = 0;
    firstWakeClickButton = 0;

    if (isDimmed) {
        isDimmed = false;
        display.ssd1306_command(SSD1306_SETCONTRAST);
        display.ssd1306_command(0x3F); // Restore active contrast
    }

    // Wake if in light sleep or deep sleep
    if (sleepState == LIGHT_SLEEP || sleepState == DEEP_SLEEP) {
        wakeFromLightSleep();
        wokeDisplay = true;
    }

    return wokeDisplay;
}

void OSManager::checkSleepConditions() {
    uint32_t now = millis();
    uint32_t idleTime = now - lastActivityTime;
    bool charging = BatteryManager::isCharging();

    // While charging via USB, keep MCU active so multi-color breathing LED continues.
    // Turn display OFF after 15s to protect OLED from burn-in.
    if (charging) {
        if (displayOn && idleTime >= lightSleepTimeout) {
            displayOn = false;
            display.ssd1306_command(SSD1306_DISPLAYOFF);
        } else if (!isDimmed && displayOn && idleTime >= 8000) {
            isDimmed = true;
            display.ssd1306_command(SSD1306_SETCONTRAST);
            display.ssd1306_command(0x05);
        }
        return;
    }

    // On battery: 3-stage power saving profile
    // Stage 1 (8s): Idle Dimming - drop OLED contrast to 0x05 (saves ~60% display current)
    // Stage 2 (15s - lightSleepTimeout): Display OFF (OLED panel in standby)
    // Stage 3 (30s - deepSleepTimeout): True DEEP SLEEP (MCU down-clocks to 12MHz, < 1mA, ARM WFI)
    if (sleepState == AWAKE) {
        if (!isDimmed && idleTime >= 8000) {
            isDimmed = true;
            display.ssd1306_command(SSD1306_SETCONTRAST);
            display.ssd1306_command(0x05); // Idle dimming: drops OLED current by ~60%
        }
        if (idleTime >= deepSleepTimeout) {
            Serial.println("[OSManager] Entering DEEP SLEEP (30s idle, < 1mA)...");
            enterDeepSleep();
            return;
        } else if (idleTime >= lightSleepTimeout && displayOn) {
            displayOn = false;
            display.ssd1306_command(SSD1306_DISPLAYOFF);
        }
    }
}

void OSManager::enterLightSleep() {
    if (sleepState == LIGHT_SLEEP) return;

    if (currentApplet) {
        currentApplet->onPrepareSleep();
    }

    sleepState = LIGHT_SLEEP;
    displayOn = false;
    
    // Solidly extinguish and clamp LEDs to GND before entering sleep
    WS2812Driver::clearAndHaltForSleep();
    SoundFX::stop();

    Serial.println("[OSManager] Entering low power sleep (wake on double-click)...");

    // Execute peripheral clamp and enter double-click wake loop
    PowerManager::enterLightSleep();

    // CPU resumes here immediately upon double-click wakeup
    wakeFromLightSleep();
}

void OSManager::wakeFromLightSleep() {
    if (sleepState != LIGHT_SLEEP && sleepState != DEEP_SLEEP) return;
    sleepState = AWAKE;
    displayOn = true;
    isDimmed = false;

    // Always wake up directly to the Sisyphus game screen (applet index 0)
    if (appletCount > 0) {
        if (currentApplet && currentAppletIndex != 0) {
            currentApplet->cleanup();
        }
        currentAppletIndex = 0;
        currentApplet = applets[0];
        if (currentApplet) {
            currentApplet->init();
        }
    }

    // Turn display back on cleanly
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0x3F);

    // Solutions 43, 44, 45: Preserve low-power controller configuration across wake
    display.ssd1306_command(0xD5);
    display.ssd1306_command(0x80);
    display.ssd1306_command(0xD9);
    display.ssd1306_command(0x22);
    display.ssd1306_command(0xDB);
    display.ssd1306_command(0x20);

    // Re-initialize NeoPixel PIO engine after wake
    WS2812Driver::reinit();

    // Redraw the current screen frame immediately to clear blank buffer
    if (currentApplet) {
        currentApplet->draw();
    }
    display.display();

    // Update last activity time to prevent immediate re-entry
    lastActivityTime = millis();
    firstWakeClickTime = 0;
    firstWakeClickButton = 0;

    Serial.println("[OSManager] Light sleep: Woken up via Double-Click, Display ON");
}

void OSManager::enterDeepSleep() {
    enterLightSleep();
}

void OSManager::registerApplet(Applet* applet) {
    if (appletCount < MAX_APPLETS) {
        applets[appletCount++] = applet;
    }
}

void OSManager::switchToApplet(uint8_t index) {
    if (index >= appletCount) return;
    
    recordActivity();  // Reset idle timer on applet switch
    
    if (currentApplet) {
        currentApplet->cleanup();
    }
    
    currentAppletIndex = index;
    currentApplet = applets[index];
    currentApplet->init();
}

void OSManager::switchToNextApplet() {
    if (appletCount == 0) return;
    uint8_t nextIndex = (currentAppletIndex + 1) % appletCount;
    switchToApplet(nextIndex);
}
