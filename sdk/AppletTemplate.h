#ifndef CUSTOM_APPLET_H
#define CUSTOM_APPLET_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "system/Applet.h"
#include "system/Display.h"
#include "system/SoundFX.h"
#include "system/WS2812.h"
#include "fonts/ThemeFonts.h"

class CustomApplet : public Applet {
private:
    uint32_t score = 0;
    uint32_t highScore = 0;
    unsigned long lastFrameMs = 0;
    Preferences prefs;

public:
    void init() override;
    void update() override;
    void draw() override;
    void cleanup() override;
    void onPrepareSleep() override;

    // Input handlers
    void onActionClick() override;
    void onActionHold() override;
    void onModeClick() override;
    void onModeHold() override;
    void onBothHeld() override;
};

#endif // CUSTOM_APPLET_H
