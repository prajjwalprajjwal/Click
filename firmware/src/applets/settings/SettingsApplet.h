#ifndef SETTINGS_APPLET_H
#define SETTINGS_APPLET_H

#include "Applet.h"
#include "Display.h"
#include "battery.hpp"
#include "device_info.hpp"
#include "fonts/ThemeFonts.h"

class SettingsApplet : public Applet {
private:
    uint8_t subPage = 0; // 0: Power & Battery, 1: System Info

public:
    void init() override {
        subPage = 0;
        BatteryManager::init();
    }

    void update() override {}

    void draw() override {
        display.clearDisplay();

        if (subPage == 0) {
            // Sub-page 0: Battery & Power Stats
            ThemeFonts::drawCentered(&Rajdhani18pt7b, "SETTINGS", 16);

            int pct = BatteryManager::getPercentage();
            bool charging = BatteryManager::isCharging();
            float voltage = BatteryManager::getSmoothVoltage();

            char buf[32];
            if (charging) {
                snprintf(buf, sizeof(buf), "Bat: %d%% [CHARGING]", pct);
            } else if (pct >= 99 && voltage >= 4.12f) {
                snprintf(buf, sizeof(buf), "Bat: 100%% [CHARGED]");
            } else {
                snprintf(buf, sizeof(buf), "Bat: %d%% [BATTERY]", pct);
            }
            ThemeFonts::drawCentered(&Rajdhani12pt7b, buf, 34);

            snprintf(buf, sizeof(buf), "%.2fV  |  ETA IC", voltage);
            ThemeFonts::drawCentered(&Rajdhani12pt7b, buf, 48);

            // Visual battery bar
            display.drawRect(20, 54, 88, 7, SSD1306_WHITE);
            int barW = (pct * 84) / 100;
            if (barW > 84) barW = 84;
            if (barW > 0) {
                display.fillRect(22, 56, barW, 3, SSD1306_WHITE);
            }
        } else {
            // Sub-page 1: System / Hardware Info (Rajdhani Theme)
            ThemeFonts::drawCentered(&Rajdhani18pt7b, "SYSTEM INFO", 16);

            String serialStr = DeviceInfo::getID();

            char buf[32];
#if defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040)
            snprintf(buf, sizeof(buf), "FW: v1.0.3 (RP2354)");
#else
            snprintf(buf, sizeof(buf), "FW: v1.0.3 (ESP32)");
#endif
            ThemeFonts::drawCentered(&Rajdhani12pt7b, buf, 32);

            snprintf(buf, sizeof(buf), "ID: %s", serialStr.c_str());
            ThemeFonts::drawCentered(&Rajdhani12pt7b, buf, 46);

            float voltage = BatteryManager::getSmoothVoltage();
            bool charging = BatteryManager::isCharging();
            snprintf(buf, sizeof(buf), "%.2fV | %s", voltage, charging ? "CHARGING" : "BATTERY");
            ThemeFonts::drawCentered(&Rajdhani12pt7b, buf, 60);
        }

        display.display();
    }

    void onActionClick() override {
        // Pressing ACTION opens/toggles the System / Hardware Info Screen
        subPage = (subPage == 0) ? 1 : 0;
    }

    void onActionHold() override {}
    void onModeClick() override {}
    void onModeHold() override {}
    void onBothHeld() override {}
};

#endif // SETTINGS_APPLET_H
