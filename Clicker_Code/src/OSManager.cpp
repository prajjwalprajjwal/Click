#include "OSManager.h"
#include "Display.h"
#include "PowerManager.h"

OSManager::OSManager() = default;

void OSManager::init() {
    inputManager.init();
    recordActivity();
    
    if (appletCount > 0) {
        currentApplet = applets[0];
        currentApplet->init();
    }
    
    Serial.println("[OSManager] Initialized. Home screen default. Light sleep: 20s, Deep sleep: 45s");
}

void OSManager::update() {
    inputManager.update();
    
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

void OSManager::recordActivity() {
    lastActivityTime = millis();
    
    // Wake if in light sleep
    if (sleepState == LIGHT_SLEEP) {
        wakeFromLightSleep();
    }
}

void OSManager::checkSleepConditions() {
    uint32_t now = millis();
    uint32_t idleTime = now - lastActivityTime;
    
    if (sleepState == AWAKE) {
        if (currentApplet != nullptr && currentApplet != applets[0] && idleTime >= lightSleepTimeout) {
            Serial.println("[OSManager] Returning to default home/snowfall screen after 20s idle");
            switchToApplet(0);
            return;
        }

        if (idleTime >= deepSleepTimeout) {
            Serial.println("[OSManager] Entering DEEP SLEEP (45s idle)");
            enterDeepSleep();
        } else if (idleTime >= lightSleepTimeout) {
            Serial.println("[OSManager] Entering LIGHT SLEEP (20s idle)");
            enterLightSleep();
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
    
    // Turn off display controller
    display.clearDisplay();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    display.display();
    
    Serial.println("[OSManager] Light sleep: Entering low power sleep...");

    // Execute GPIO hold, power domain teardown, RF off, and enter esp_light_sleep_start()
    PowerManager::enterLightSleep();

    // CPU resumes here immediately upon GPIO wakeup
    wakeFromLightSleep();
}

void OSManager::wakeFromLightSleep() {
    if (sleepState != LIGHT_SLEEP) return;
    sleepState = AWAKE;
    displayOn = true;
    
    // Explicitly re-initialize display driver instance post-wake
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, true);
    display.ssd1306_command(SSD1306_DISPLAYON);

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
    
    // Turn off display
    display.clearDisplay();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    display.display();
    
    // Configure GPIO wakeup for both buttons (D14 and D32)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_14, 0);  // Wake on LOW (button press)
    esp_sleep_enable_ext1_wakeup(1ULL << 32, ESP_EXT1_WAKEUP_ALL_LOW);  // D32 also wakes on LOW
    
    Serial.println("[OSManager] Deep sleep: entering... (wake on button press)");
    delay(100);
    
    // Enter deep sleep
    esp_deep_sleep_start();
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
