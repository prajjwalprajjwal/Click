#include "OSManager.h"
#include "Display.h"
#include "PowerManager.h"
#include "WS2812.h"
#include "SoundFX.h"
#include "battery.hpp"

OSManager::OSManager() = default;

void OSManager::init() {
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
    
    // Any physical button contact keeps device active
    if (isActionButtonPressed() || isModeButtonPressed()) {
        recordActivity();
    }
    
    // Check if we should transition to sleep states
    checkSleepConditions();
    
    // Only update applet if awake
    if (sleepState == AWAKE && currentApplet) {
        currentApplet->update();
    }
}

void OSManager::draw() {
    if (sleepState == AWAKE && displayOn && currentApplet) {
        currentApplet->draw();
    }
}

bool OSManager::recordActivity() {
    lastActivityTime = millis();
    bool wokeDisplay = false;

    // Wake display if turned off during charging idle standby
    if (!displayOn && sleepState == AWAKE) {
        displayOn = true;
        isDimmed = false;
        display.ssd1306_command(SSD1306_DISPLAYON);
        display.ssd1306_command(SSD1306_SETCONTRAST);
        display.ssd1306_command(0x3F);
        if (currentApplet) {
            currentApplet->draw();
        }
        wokeDisplay = true;
    } else if (isDimmed) {
        isDimmed = false;
        display.ssd1306_command(SSD1306_SETCONTRAST);
        display.ssd1306_command(0x3F); // Restore active contrast
    }

    // Wake if in light sleep
    if (sleepState == LIGHT_SLEEP) {
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
    // Turn display OFF after 20s to protect OLED from burn-in.
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

    // On battery: perform idle dimming and true soft kill dormant sleep (< 0.1mA)
    if (sleepState == AWAKE) {
        if (!isDimmed && idleTime >= 8000) {
            isDimmed = true;
            display.ssd1306_command(SSD1306_SETCONTRAST);
            display.ssd1306_command(0x05); // Idle dimming: drops OLED current by ~60%
        }
        if (idleTime >= deepSleepTimeout) {
            Serial.println("[OSManager] Entering DEEP SLEEP (45s idle)");
            enterDeepSleep();
            return;
        } else if (idleTime >= lightSleepTimeout) {
            Serial.println("[OSManager] Entering DORMANT SLEEP (20s idle, < 0.1mA)...");
            enterLightSleep();
            return;
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
    
    // Solidly extinguish and clamp LEDs to GND before entering dormant sleep
    WS2812Driver::clearAndHaltForSleep();
    SoundFX::stop();

    Serial.println("[OSManager] Light sleep: Entering low power sleep (wake on button press)...");

    // Execute GPIO hold, power domain teardown, RF off, and enter hardware dormant mode
    PowerManager::enterLightSleep();

    // CPU resumes here immediately upon GPIO wakeup
    wakeFromLightSleep();
}

void OSManager::wakeFromLightSleep() {
    if (sleepState != LIGHT_SLEEP) return;
    sleepState = AWAKE;
    displayOn = true;
    isDimmed = false;
    
    // Explicitly re-initialize display driver instance post-wake
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, true);
    display.setRotation(2);
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0x3F);

    // Re-initialize NeoPixel PIO engine after wake
    WS2812Driver::reinit();

    // Update last activity time to prevent immediate re-entry
    recordActivity();

    // Redraw the current screen frame immediately to clear blank buffer
    if (currentApplet) {
        currentApplet->draw();
    }

    Serial.println("[OSManager] Light sleep: Woken up, Display re-initialized & ON");
}

void OSManager::enterDeepSleep() {
    if (currentApplet) {
        currentApplet->onPrepareSleep();
    }

    sleepState = DEEP_SLEEP;
    displayOn = false;
    
    WS2812Driver::clearAndHaltForSleep();
    SoundFX::stop();

    // Turn off display
    display.clearDisplay();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    
#if defined(ESP32)
    // Configure RTC GPIO wakeup & pull-ups for both buttons (D14 and D32)
    rtc_gpio_init((gpio_num_t)MODE_BUTTON_PIN);
    rtc_gpio_set_direction((gpio_num_t)MODE_BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)MODE_BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)MODE_BUTTON_PIN);

    rtc_gpio_init((gpio_num_t)ACTION_BUTTON_PIN);
    rtc_gpio_set_direction((gpio_num_t)ACTION_BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)ACTION_BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)ACTION_BUTTON_PIN);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)MODE_BUTTON_PIN, 0);  // Wake on LOW (button press)
    esp_sleep_enable_ext1_wakeup(1ULL << ACTION_BUTTON_PIN, ESP_EXT1_WAKEUP_ALL_LOW);  // D32 also wakes on LOW
    
    Serial.println("[OSManager] Deep sleep: entering... (wake on button press)");
    delay(100);
    
    // Enter deep sleep
    esp_deep_sleep_start();
#else
    Serial.println("[OSManager] Deep sleep: entering dormant sleep mode... (wake on button press)");
    PowerManager::enterLightSleep();
    wakeFromLightSleep();
#endif
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
