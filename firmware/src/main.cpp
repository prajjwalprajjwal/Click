#include <Arduino.h>
#include <Wire.h>
#include "Display.h"
#include "OSManager.h"
#include "HomeApplet.h"
#include "CounterApplet.h"
#include "TimingGameApplet.h"
#include "SettingsApplet.h"
#include "fonts/ThemeFonts.h"
#include "FlappyBirdApplet.h"

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
    delay(100);

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (1) {
            delay(1000);
        }
    }

    // Set 400kHz Fast I2C mode for smooth SSD1306 refresh
    Wire.setClock(400000);

    // Boot screen: display bootscreen.png if converted, otherwise fallback to text
    display.clearDisplay();
#ifdef GENERATED_BOOTSCREEN_H
    display.drawBitmap(0, 0, bootscreen_bmp, BOOTSCREEN_WIDTH, BOOTSCREEN_HEIGHT, SSD1306_WHITE);
#else
    ThemeFonts::drawCentered(&Rajdhani24pt7b, "CLICKER", 22);
    ThemeFonts::drawCentered(&Rajdhani12pt7b, "Starting up...", 48);
#endif
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
    delayMicroseconds(1000);
}
