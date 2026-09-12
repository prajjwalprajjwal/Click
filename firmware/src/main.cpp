#include <Arduino.h>
#include <Wire.h>

// Core System
#include "system/Display.h"
#include "system/OSManager.h"
#include "system/device_info.hpp"

// Modular Applets
#include "applets/clicker/CounterApplet.h"
#include "applets/timing_game/TimingGameApplet.h"
#include "applets/flappy_bird/FlappyBirdApplet.h"
#include "applets/settings/SettingsApplet.h"
#include "applets/screensaver/HomeApplet.h"

// Typography
#include "fonts/ThemeFonts.h"

#if __has_include("generated_assets/bootscreen.h")
  #include "generated_assets/bootscreen.h"
#endif

Adafruit_SSD1306 display(128, 64, &Wire, -1);

OSManager osManager;
HomeApplet homeApplet;
CounterApplet counterApplet;
TimingGameApplet timingGameApplet;
SettingsApplet settingsApplet;
FlappyBirdApplet flappyBirdApplet;

void onModeButtonClick() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet == &settingsApplet || applet == &homeApplet) {
        osManager.switchToApplet(0); // Exit settings / screensaver back to Clicker
        return;
    }
    if (applet) {
        applet->onModeClick();
    }
    osManager.switchToNextApplet();
}

void onModeButtonHold() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet) {
        applet->onModeHold();
    }
    // Hold MODE button toggles Settings page
    if (applet == &settingsApplet) {
        osManager.switchToApplet(0); // Return to Clicker
    } else {
        osManager.switchToApplet(3); // Jump to Settings
    }
}

void onActionButtonClick() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet == &homeApplet) {
        osManager.switchToApplet(0); // Wake from screensaver to Clicker
        return;
    }
    if (applet) {
        applet->onActionClick();
    }
}

void onActionButtonHold() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet == &homeApplet) {
        osManager.switchToApplet(0); // Wake from screensaver to Clicker
        return;
    }
    if (applet) {
        applet->onActionHold();
    }
}

void onBothButtonsHeld() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet) {
        applet->onBothHeld();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500); // let the oled init properly

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (1) {
            delay(1000);
        }
    }

    // Set 400kHz Fast I2C mode for smooth SSD1306 refresh
    Wire.setClock(400000);

    // Boot screen: display custom device name (default: CLICKER) and "Starting up..."
    display.clearDisplay();

    const char* bootName = DeviceInfo::getCustomName();
    if (!bootName || bootName[0] == '\0') {
        bootName = "CLICKER";
    }

    const GFXfont* nameFont = &Rajdhani24pt7b;
    int16_t xA = 0, yA = 0, xB = 0, yB = 0;
    uint16_t wA = 0, hA = 0, wB = 0, hB = 0;
    ThemeFonts::measure(nameFont, bootName, xA, yA, wA, hA);
    if (wA + 1 > 120) {
        nameFont = &Rajdhani18pt7b;
        ThemeFonts::measure(nameFont, bootName, xA, yA, wA, hA);
    }
    if (wA + 1 > 120) {
        nameFont = &Rajdhani12pt7b;
        ThemeFonts::measure(nameFont, bootName, xA, yA, wA, hA);
    }

    const GFXfont* subFont = &Rajdhani12pt7b;
    ThemeFonts::measure(subFont, "Starting up...", xB, yB, wB, hB);

    // Treat name and "Starting up..." as a single unified visual group
    const int16_t gap = 4;
    const int16_t effectiveWA = static_cast<int16_t>(wA + 1);
    const int16_t totalH = static_cast<int16_t>(hA + gap + hB);
    const int16_t topY = static_cast<int16_t>((64 - totalH) / 2);

    // Pixel-perfect baseline cursor calculation for both texts
    const int16_t cursorYA = static_cast<int16_t>(topY - yA);
    const int16_t cursorYB = static_cast<int16_t>(topY + hA + gap - yB);

    const int16_t cursorXA = static_cast<int16_t>((128 - effectiveWA) / 2 - xA);
    const int16_t cursorXB = static_cast<int16_t>((128 - static_cast<int16_t>(wB)) / 2 - xB);

    // Draw device name with thicker / bold weight (1px horizontal overstrike)
    display.setFont(nameFont);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(cursorXA, cursorYA);
    display.print(bootName);
    display.setCursor(cursorXA + 1, cursorYA);
    display.print(bootName);

    // Draw subtitle
    display.setFont(subFont);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(cursorXB, cursorYB);
    display.print("Starting up...");

    display.display();
    delay(1000);

    // Register playable main applets (index 0 is active on boot)
    osManager.registerApplet(&counterApplet);     // index 0: Clicker (Default on boot)
    osManager.registerApplet(&timingGameApplet);  // index 1: Just Ten
    osManager.registerApplet(&flappyBirdApplet);  // index 2: Flappy Bird
    osManager.registerApplet(&settingsApplet);    // index 3: Settings (Hold MODE)
    
    // Set snowfall applet as screensaver on inactivity
    osManager.setScreensaverApplet(&homeApplet);

    osManager.init();
    counterApplet.preloadState();

    InputManager* inputMgr = osManager.getInputManager();
    inputMgr->onModeClick    = onModeButtonClick;
    inputMgr->onModeHold     = onModeButtonHold;
    inputMgr->onActionClick  = onActionButtonClick;
    inputMgr->onActionHold   = onActionButtonHold;
    inputMgr->onBothHeld     = onBothButtonsHeld;
}

void loop() {
    osManager.update();
    osManager.draw();
    // Yield execution to FreeRTOS scheduler so IDLE task and TWDT watchdog on Core 1 are fed!
    delay(2);
}
