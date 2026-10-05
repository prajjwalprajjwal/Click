#include <Arduino.h>
#include <Wire.h>

// Core System
#include "system/Display.h"
#include "system/OSManager.h"
#include "system/device_info.hpp"
#include "system/WS2812.h"
#include "system/SoundFX.h"

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

Adafruit_SSD1306 display(128, 64, &CLICK_I2C, OLED_RST_PIN);

OSManager osManager;
HomeApplet homeApplet;
CounterApplet counterApplet;
TimingGameApplet timingGameApplet;
SettingsApplet settingsApplet;
FlappyBirdApplet flappyBirdApplet;

void onModeButtonClick() {
    if (osManager.recordActivity()) {
        return; // Screen was asleep/off; first click wakes the screen silently
    }
    // Requirement 2: Do NOT beep just before Sisyphus game!
    uint8_t curIdx = osManager.getCurrentAppletIndex();
    uint8_t nextIdx = (curIdx + 1) % 4; // 0 is Sisyphus
    if (nextIdx != 0) {
        SoundFX::playClick(); // Only click when switching into other applets!
    }
    Applet* applet = osManager.getCurrentApplet();
    if (applet == &homeApplet) {
        osManager.switchToApplet(0); // Exit screensaver back to Clicker
        return;
    }
    if (applet) {
        applet->onModeClick();
    }
    osManager.switchToNextApplet();
}

void onModeButtonHold() {
    if (osManager.recordActivity()) {
        return;
    }
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
    if (osManager.recordActivity()) {
        return; // Screen was off; wake display
    }
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
    if (osManager.recordActivity()) {
        return;
    }
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
    if (osManager.recordActivity()) {
        return;
    }
    Applet* applet = osManager.getCurrentApplet();
    if (applet) {
        applet->onBothHeld();
    }
}

static Preferences sysPrefs;
static uint32_t totalUptimeMinutes = 0;
static uint32_t lastUptimeCheckMs = 0;
static void handleSerialTelemetry();

#if defined(CLICK_HW_TEST_MENU)
#include "system/HardwareDiagnostics.h"
#endif

void setup() {
#if defined(CLICK_HW_TEST_MENU)
    HardwareDiagnostics::run();
#endif

    Serial.begin(115200);

    // Early preload of state so telemetry can answer immediately even during boot
    counterApplet.preloadState();
    flappyBirdApplet.preloadState();
    timingGameApplet.preloadState();
    sysPrefs.begin("sys_uptime", false);
    totalUptimeMinutes = sysPrefs.getUInt("minutes", 0);
    lastUptimeCheckMs = millis();

    // Responsive early delay loop checking for incoming telemetry
    for (int i = 0; i < 50; i++) {
        handleSerialTelemetry();
        delay(10);
    }

#if defined(ARDUINO_ARCH_RP2040)
    CLICK_I2C.setSDA(OLED_SDA_PIN);
    CLICK_I2C.setSCL(OLED_SCL_PIN);
    CLICK_I2C.begin();
#else
    CLICK_I2C.begin(OLED_SDA_PIN, OLED_SCL_PIN);
#endif

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (1) {
            handleSerialTelemetry();
            delay(100);
        }
    }

    display.setRotation(2); // Rotate display 180 degrees
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0x3F); // High-contrast crisp white without burning 20mA

    // Solutions 43, 44, 45: SSD1306 Low-Power Controller Configuration
    display.ssd1306_command(0xD5); // Set Display Clock Divide Ratio / Oscillator Frequency
    display.ssd1306_command(0x80); // Optimal lower-power oscillator frequency
    display.ssd1306_command(0xD9); // Set Pre-charge Period
    display.ssd1306_command(0x22); // Phase 1: 2 DCLKs, Phase 2: 2 DCLKs (drops charge pump load)
    display.ssd1306_command(0xDB); // Set VCOMH Deselect Level
    display.ssd1306_command(0x20); // 0.77 x Vcc (reduces drive current)

    // Solution 73: Disable PCF8563 RTC CLKOUT pin to eliminate continuous 32.768kHz high-frequency switching
    CLICK_I2C.beginTransmission(0x51);
    CLICK_I2C.write(0x0D); // CLKOUT_control register
    CLICK_I2C.write(0x00); // 0 = Disable CLKOUT (tri-state pin, 0µA current)
    CLICK_I2C.endTransmission();

    // Initialize RGB LEDs, Sound Buzzer, and Battery Monitoring
    WS2812Driver::init();
    SoundFX::init();
    BatteryManager::init();
    SoundFX::playStartup(); // Requirement 2: Startup chime on boot
    WS2812Driver::flash(0, 180, 255, 500);

    // Set 400kHz Fast I2C mode for smooth SSD1306 refresh
    CLICK_I2C.setClock(400000);

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

    // Bootscreen delay (500ms): service serial telemetry & LED timing, then shut LED off cleanly
    for (int i = 0; i < 50; i++) {
        handleSerialTelemetry();
        WS2812Driver::update();
        delay(10);
    }
    WS2812Driver::clear();

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
}

static void handleSerialTelemetry() {
    if (!Serial.available()) return;
    static char cmdBuf[64];
    static uint8_t cmdIdx = 0;
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (cmdIdx > 0) {
                cmdBuf[cmdIdx] = '\0';
                // Case-insensitive uppercase conversion
                for (uint8_t i = 0; i < cmdIdx; i++) {
                    if (cmdBuf[i] >= 'a' && cmdBuf[i] <= 'z') {
                        cmdBuf[i] -= 32;
                    }
                }
                if (strstr(cmdBuf, "GET_STATS") != nullptr || strstr(cmdBuf, "STATS") != nullptr) {
                    // Flush any active session clicks immediately to NVS before reading
                    counterApplet.persistNow();

                    String idStr = DeviceInfo::getID();
                    const char* chipId = idStr.c_str();
                    const char* dName = DeviceInfo::getCustomName();
                    if (!dName || dName[0] == '\0') {
                        dName = "CLICKER";
                    }

                    uint64_t clicks = counterApplet.getLifetimeClicks();
                    uint16_t flappy = flappyBirdApplet.getHighScore();
                    double justTen = timingGameApplet.getBestTimeSec();
                    uint32_t uptimeHrs = totalUptimeMinutes / 60;

                    Serial.printf("{\"event\":\"stats\",\"name\":\"%s\",\"chip_id\":\"%s\",\"clicks\":%llu,\"flappy\":%u,\"just_ten\":%.4f,\"uptime_hrs\":%u}\n",
                                  dName, chipId, clicks, flappy, justTen, uptimeHrs);
                } else if (strstr(cmdBuf, "SET_CLICKS") != nullptr) {
                    char* p = strstr(cmdBuf, "SET_CLICKS") + 10;
                    while (*p == ' ' || *p == '=') p++;
                    uint64_t val = strtoull(p, nullptr, 10);
                    if (val > 0) {
                        counterApplet.setLifetimeClicks(val);
                        Serial.printf("{\"event\":\"clicks_updated\",\"clicks\":%llu}\n", val);
                    }
                } else if (strstr(cmdBuf, "SET_STATS") != nullptr || strstr(cmdBuf, "RESTORE") != nullptr) {
                    char* pClicks = strstr(cmdBuf, "CLICKS=");
                    if (pClicks) {
                        uint64_t val = strtoull(pClicks + 7, nullptr, 10);
                        if (val > counterApplet.getLifetimeClicks()) {
                            counterApplet.setLifetimeClicks(val);
                        }
                    }
                    char* pFlappy = strstr(cmdBuf, "FLAPPY=");
                    if (pFlappy) {
                        uint16_t val = static_cast<uint16_t>(strtoul(pFlappy + 7, nullptr, 10));
                        flappyBirdApplet.setHighScore(val);
                    }
                    char* pJustTen = strstr(cmdBuf, "JUST_TEN=");
                    if (pJustTen) {
                        double sec = strtod(pJustTen + 9, nullptr);
                        if (sec > 0.0) {
                            timingGameApplet.setBestTimeUs(static_cast<uint32_t>(sec * 1000000.0));
                        }
                    }
                    Serial.printf("{\"event\":\"stats_restored\",\"clicks\":%llu,\"flappy\":%u,\"just_ten\":%.4f}\n",
                                  counterApplet.getLifetimeClicks(),
                                  flappyBirdApplet.getHighScore(),
                                  timingGameApplet.getBestTimeSec());
                } else if (strstr(cmdBuf, "PING") != nullptr) {
                    Serial.println("{\"event\":\"pong\"}");
                }
                cmdIdx = 0;
            }
        } else if (cmdIdx < sizeof(cmdBuf) - 1) {
            cmdBuf[cmdIdx++] = c;
        } else {
            // Buffer overflow recovery
            cmdIdx = 0;
        }
    }
}

void loop() {
    uint32_t frameStart = millis();

    // During charging, maintain an uninterrupted breathing LED of multi-colors,
    // but allow game effects (flashes, celebrations) to temporarily display
    static bool wasCharging = false;
    bool charging = BatteryManager::isCharging();
    if (charging) {
        if (!WS2812Driver::isChargingBreatheActive() && !WS2812Driver::isBusy()) {
            WS2812Driver::startChargingBreathe();
        }
    } else if (wasCharging) {
        WS2812Driver::stopChargingBreathe();
    }
    wasCharging = charging;

    SoundFX::update();
    WS2812Driver::update();

    osManager.update();
    osManager.draw();

    // Check serial telemetry requests (non-blocking)
    handleSerialTelemetry();

    // Track active runtime (persist once every hour to safeguard flash longevity)
    uint32_t now = millis();
    if (now - lastUptimeCheckMs >= 60000) {
        lastUptimeCheckMs = now;
        totalUptimeMinutes++;
        if (totalUptimeMinutes % 60 == 0) {
            sysPrefs.putUInt("minutes", totalUptimeMinutes);
        }
    }

    // Dynamic low-power frame pacing:
    // Cap rendering to ~30 FPS (33ms). Sleeping during idle frame time invokes
    // ARM __wfi() (Wait For Interrupt), dramatically cutting active MCU current!
    uint32_t elapsed = millis() - frameStart;
    if (elapsed < 33) {
        delay(33 - elapsed);
    } else {
        delay(2);
    }
}
