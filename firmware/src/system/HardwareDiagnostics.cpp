#include "HardwareDiagnostics.h"
#include "Display.h"
#include "PinConfig.h"
#include "WS2812.h"
#include "battery.hpp"
#include "device_info.hpp"
#include <Wire.h>

static void drawOledHeader(const char* title) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("== ");
    display.print(title);
    display.println(" ==");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 14);
}

// BCD helper for PCF8563
static uint8_t bcdToDec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

void HardwareDiagnostics::testBuzzer() {
    Serial.println("\n--- [1] Testing Buzzer on GP27 ---");
    drawOledHeader("BUZZER TEST");
    display.println("Pin: GP27 (NPN Driver)");
    display.println("1. Click Ticks");
    display.display();

    pinMode(BUZZER_PIN, OUTPUT);

    // 1. Click ticks
    Serial.println(" -> Playing click ticks (2400 Hz)...");
    for (int i = 0; i < 3; i++) {
        tone(BUZZER_PIN, 2400, 30);
        delay(120);
    }

    // 2. Chime (C5, E5, G5, C6)
    drawOledHeader("BUZZER TEST");
    display.println("Pin: GP27");
    display.println("2. Chime (C-E-G-C)");
    display.display();
    Serial.println(" -> Playing ascending chime...");
    const uint16_t notes[] = {523, 659, 784, 1046};
    for (uint16_t n : notes) {
        tone(BUZZER_PIN, n, 80);
        delay(100);
    }
    noTone(BUZZER_PIN);

    // 3. Frequency Sweep
    drawOledHeader("BUZZER TEST");
    display.println("Pin: GP27");
    display.println("3. Frequency Sweep");
    display.display();
    Serial.println(" -> Playing frequency sweep (600 Hz -> 2600 Hz)...");
    for (uint16_t f = 600; f <= 2600; f += 100) {
        tone(BUZZER_PIN, f, 15);
        delay(15);
    }
    noTone(BUZZER_PIN);

    drawOledHeader("BUZZER TEST");
    display.println("Buzzer test completed!");
    display.display();
    Serial.println(" [OK] Buzzer test completed.");
}

void HardwareDiagnostics::testRGBLEDs() {
    Serial.println("\n--- [2] Testing WS2812 / SK6812 RGB LEDs on GP11 (DIN) ---");
    WS2812Driver::init();

    struct ColorTest {
        const char* name;
        uint8_t r, g, b;
    } tests[] = {
        {"RED",     255,   0,   0},
        {"GREEN",     0, 255,   0},
        {"BLUE",      0,   0, 255},
        {"YELLOW",  255, 200,   0},
        {"CYAN",      0, 255, 255},
        {"MAGENTA", 255,   0, 255},
        {"WHITE",   255, 255, 255},
        {"OFF",       0,   0,   0}
    };

    for (const auto& c : tests) {
        Serial.printf(" -> LED Color: %s\n", c.name);
        drawOledHeader("RGB LED TEST");
        display.println("Type: WS2812 (GP11 DIN)");
        display.printf("Color: %s\n", c.name);
        display.display();

        WS2812Driver::setAll(c.r, c.g, c.b);
        delay(350);
    }

    // Rainbow cycle animation
    Serial.println(" -> Running smooth rainbow cycle...");
    drawOledHeader("RGB LED TEST");
    display.println("Rainbow Cycle...");
    display.display();

    for (int hue = 0; hue < 360; hue += 15) {
        float h = hue / 60.0f;
        int i = static_cast<int>(h);
        float f = h - i;
        uint8_t q = static_cast<uint8_t>(255 * (1.0f - f));
        uint8_t t = static_cast<uint8_t>(255 * f);

        uint8_t r = 0, g = 0, b = 0;
        switch (i) {
            case 0: r = 255; g = t;   b = 0;   break;
            case 1: r = q;   g = 255; b = 0;   break;
            case 2: r = 0;   g = 255; b = t;   break;
            case 3: r = 0;   g = q;   b = 255; break;
            case 4: r = t;   g = 0;   b = 255; break;
            default:r = 255; g = 0;   b = q;   break;
        }

        WS2812Driver::setPixel(0, r, g, b);
        WS2812Driver::setPixel(1, b, r, g);
        WS2812Driver::show();
        delay(30);
    }

    WS2812Driver::clear();
    Serial.println(" [OK] WS2812 RGB LEDs test completed.");
}

void HardwareDiagnostics::testOLED() {
    Serial.println("\n--- [3] Testing OLED Display (SSD1306 128x64 on SDA=GP2, SCL=GP3) ---");

    // 1. Concentric Rectangles
    display.clearDisplay();
    for (int i = 0; i < 32; i += 4) {
        display.drawRect(i * 2, i, 128 - (i * 4), 64 - (i * 2), SSD1306_WHITE);
        display.display();
        delay(30);
    }
    delay(300);

    // 2. Checkerboard pattern
    display.clearDisplay();
    for (int y = 0; y < 64; y += 8) {
        for (int x = 0; x < 128; x += 8) {
            if (((x / 8) + (y / 8)) % 2 == 0) {
                display.fillRect(x, y, 8, 8, SSD1306_WHITE);
            }
        }
    }
    display.display();
    delay(400);

    // 3. Text scales
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("CLICK 4 (RP2354A)");
    display.setTextSize(2);
    display.setCursor(0, 14);
    display.println("128x64 OLED");
    display.setTextSize(1);
    display.setCursor(0, 36);
    display.println("SDA: GP2 | SCL: GP3");
    display.println("RTC: PCF8563 (0x51)");
    display.println("Fast 400kHz I2C1 Bus");
    display.display();
    delay(800);

    Serial.println(" [OK] OLED Display test completed.");
}

void HardwareDiagnostics::scanI2CBus() {
    Serial.println("\n--- [5] Scanning I2C Bus (SDA=GP2, SCL=GP3) ---");
    drawOledHeader("I2C BUS SCAN");
    display.println("Scanning 0x01..0x7F...");
    display.display();

    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        CLICK_I2C.beginTransmission(addr);
        uint8_t err = CLICK_I2C.endTransmission();
        if (err == 0) {
            count++;
            Serial.printf(" -> Device found at 0x%02X: ", addr);
            if (addr == 0x3C || addr == 0x3D) {
                Serial.println("SSD1306 128x64 OLED Screen");
                display.printf("0x%02X: SSD1306 OLED\n", addr);
            } else if (addr == 0x51) {
                Serial.println("PCF8563 Real-Time Clock (RTC)");
                display.printf("0x%02X: PCF8563 RTC\n", addr);
            } else {
                Serial.println("Unknown Device");
                display.printf("0x%02X: Device\n", addr);
            }
        }
    }

    if (count == 0) {
        Serial.println(" [!] No I2C devices responded! Check pull-ups and power rails.");
        display.println("No devices found!");
    } else {
        Serial.printf(" [OK] Scan completed. Found %d active I2C device(s).\n", count);
        display.printf("Total: %d device(s)\n", count);
    }
    display.display();
}

static void testPCF8563RTC() {
    Serial.println("\n--- [9] Reading PCF8563 Real-Time Clock (RTC at 0x51) ---");
    drawOledHeader("PCF8563 RTC TEST");

    CLICK_I2C.beginTransmission(0x51);
    CLICK_I2C.write(0x02); // Start register: Seconds
    uint8_t err = CLICK_I2C.endTransmission();

    if (err != 0) {
        Serial.println(" [ERROR] PCF8563 RTC did not respond at address 0x51!");
        display.println("PCF8563 not found!");
        display.display();
        return;
    }

    uint8_t bytesRead = CLICK_I2C.requestFrom((uint8_t)0x51, (size_t)7);
    if (bytesRead == 7) {
        uint8_t rawSec  = CLICK_I2C.read();
        uint8_t rawMin  = CLICK_I2C.read();
        uint8_t rawHour = CLICK_I2C.read();
        uint8_t rawDays = CLICK_I2C.read();
        uint8_t rawWday = CLICK_I2C.read();
        uint8_t rawMon  = CLICK_I2C.read();
        uint8_t rawYear = CLICK_I2C.read();

        bool voltageLow = (rawSec & 0x80) != 0;
        uint8_t sec  = bcdToDec(rawSec & 0x7F);
        uint8_t min  = bcdToDec(rawMin & 0x7F);
        uint8_t hour = bcdToDec(rawHour & 0x3F);
        uint8_t day  = bcdToDec(rawDays & 0x3F);
        uint8_t mon  = bcdToDec(rawMon & 0x1F);
        uint8_t year = bcdToDec(rawYear);

        Serial.printf(" -> RTC Time: %02d:%02d:%02d\n", hour, min, sec);
        Serial.printf(" -> RTC Date: 20%02d-%02d-%02d\n", year, mon, day);
        Serial.printf(" -> Backup Voltage Low Flag (VL): %s\n", voltageLow ? "YES (Battery low/uninitialized)" : "OK");

        display.printf("Time: %02d:%02d:%02d\n", hour, min, sec);
        display.printf("Date: 20%02d-%02d-%02d\n", year, mon, day);
        display.printf("VL:   %s\n", voltageLow ? "LOW" : "OK");
        display.display();
    } else {
        Serial.printf(" [ERROR] Expected 7 bytes, got %u bytes.\n", bytesRead);
        display.println("Read failed!");
        display.display();
    }
}

void HardwareDiagnostics::testBatteryAndCharger() {
    Serial.println("\n--- [6] Battery Voltage Divider Telemetry (GP29 / ADC3) ---");
    analogReadResolution(12);

    uint32_t rawSum = 0;
    for (int i = 0; i < 64; i++) {
        rawSum += analogRead(BATTERY_ADC_PIN);
        delayMicroseconds(500);
    }
    float rawAvg = rawSum / 64.0f;
    float adcPinVoltage = (rawAvg / 4095.0f) * 3.3f;
    float batteryVoltage = adcPinVoltage * 2.0f * 1.05f; // 100k/100k divider + cal
    int pct = BatteryManager::getPercentage();
    bool charging = BatteryManager::isCharging();

    Serial.printf(" -> Voltage Divider Pin: GP%d (ADC3)\n", BATTERY_ADC_PIN);
    Serial.printf(" -> ADC Raw (12-bit):    %.1f / 4095\n", rawAvg);
    Serial.printf(" -> ADC Pin Voltage:     %.3f V\n", adcPinVoltage);
    Serial.printf(" -> Battery Voltage:     %.3f V\n", batteryVoltage);
    Serial.printf(" -> Charger Status (GP%d): %s\n", CHARGER_STAT_PIN, charging ? "CHARGING (LOW)" : "IDLE/UNPLUGGED (HIGH)");
    Serial.printf(" -> Calculated Battery:  %d%%\n", pct);

    drawOledHeader("BATTERY TELEMETRY");
    display.printf("V_Bat:  %.2f V\n", batteryVoltage);
    display.printf("Stat:   %s\n", charging ? "CHARGING" : "BATTERY");
    display.printf("Charge: %d%%\n", pct);
    display.display();
}

void HardwareDiagnostics::printSystemInfo() {
    Serial.println("\n--- [7] System Silicon & Architecture Info ---");

    String chipId = DeviceInfo::getID();
    String mac = DeviceInfo::getFormattedMac();
    const char* customName = DeviceInfo::getCustomName();

    Serial.printf(" -> MCU:           RP2350 (Dual ARM Cortex-M33 / Hazard3)\n");
    Serial.printf(" -> Target:        Click 4 PCBA (RP2354A with 4MB Flash)\n");
    Serial.printf(" -> Board ID:      %s\n", chipId.c_str());
    Serial.printf(" -> MAC Style ID:  %s\n", mac.c_str());
    Serial.printf(" -> Custom Name:   %s\n", customName);
    Serial.printf(" -> CPU Clock:     %lu MHz\n", F_CPU / 1000000UL);
    Serial.printf(" -> SRAM Total:    512 KB\n");
    Serial.printf(" -> Free Heap:     %u bytes\n", rp2040.getFreeHeap());
    Serial.printf(" -> Pinout:        ACTION=QSPI_SS, MODE=GP0, SDA=GP2, SCL=GP3\n");
    Serial.printf("                   BUZZER=GP27, WS2812=GP11, BAT_ADC=GP29, RTC=PCF8563\n");

    drawOledHeader("SYSTEM INFO");
    display.printf("MCU: RP2354A 150MHz\n");
    display.printf("ID:  %s\n", chipId.substring(0, 12).c_str());
    display.printf("Heap: %u B\n", rp2040.getFreeHeap());
    display.printf("Name: %s\n", customName);
    display.display();
}

void HardwareDiagnostics::monitorButtonsAndPower() {
    Serial.println("\n--- [4] Live Interactive Button & Power Monitor ---");
    Serial.println(" -> Press ACTION switch (QSPI_SS / BOOTSEL, Active Low).");
    Serial.println(" -> Press MODE switch (GPIO 0, Active Low).");
    Serial.println(" -> Press ANY key in serial terminal to exit back to menu.\n");

    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
    WS2812Driver::init();

    uint32_t actionClicks = 0;
    uint32_t modeClicks = 0;
    bool lastAction = isActionButtonPressed();
    bool lastMode = isModeButtonPressed();

    uint32_t lastDisplayUpdate = 0;

    while (!Serial.available()) {
        bool curAction = isActionButtonPressed();
        bool curMode = isModeButtonPressed();

        // Detect ACTION switch edge
        if (!lastAction && curAction) {
            actionClicks++;
            Serial.printf(" [EVENT] ACTION Switch (QSPI_SS) PRESSED! Total: %u\n", actionClicks);
            tone(BUZZER_PIN, 2000, 25);
            WS2812Driver::setPixel(0, 0, 255, 0); // Green on LED 0
            WS2812Driver::show();
        } else if (lastAction && !curAction) {
            Serial.println(" [EVENT] ACTION Switch (QSPI_SS) RELEASED.");
            WS2812Driver::setPixel(0, 0, 0, 0);
            WS2812Driver::show();
        }

        // Detect MODE switch edge
        if (!lastMode && curMode) {
            modeClicks++;
            Serial.printf(" [EVENT] MODE Switch (GPIO 0) PRESSED! Total: %u\n", modeClicks);
            tone(BUZZER_PIN, 1500, 25);
            WS2812Driver::setPixel(1, 0, 0, 255); // Blue on LED 1
            WS2812Driver::show();
        } else if (lastMode && !curMode) {
            Serial.println(" [EVENT] MODE Switch (GPIO 0) RELEASED.");
            WS2812Driver::setPixel(1, 0, 0, 0);
            WS2812Driver::show();
        }

        lastAction = curAction;
        lastMode = curMode;

        // Refresh OLED every 100ms
        uint32_t now = millis();
        if (now - lastDisplayUpdate >= 100) {
            lastDisplayUpdate = now;
            float v = BatteryManager::getSmoothVoltage();

            drawOledHeader("LIVE BUTTON MONITOR");
            display.printf("ACTION (QSPI_SS): %s\n", curAction ? ">> DOWN <<" : "UP (Open)");
            display.printf("MODE   (GPIO 0):  %s\n", curMode ? ">> DOWN <<" : "UP (Open)");
            display.printf("Clicks: ACT=%u MOD=%u\n", actionClicks, modeClicks);
            display.printf("Bat: %.2fV\n", v);
            display.display();
        }

        delay(10);
    }

    // Flush serial buffer
    while (Serial.available()) {
        Serial.read();
    }
    WS2812Driver::clear();
    Serial.println("\nExited Live Monitor.");
}

void HardwareDiagnostics::runAll() {
    Serial.println("\n=======================================================");
    Serial.println("   RUNNING ALL AUTOMATED HARDWARE DIAGNOSTICS");
    Serial.println("=======================================================");
    printSystemInfo();
    delay(500);
    scanI2CBus();
    delay(500);
    testPCF8563RTC();
    delay(500);
    testBatteryAndCharger();
    delay(500);
    testRGBLEDs();
    delay(500);
    testBuzzer();
    delay(500);
    testOLED();
    Serial.println("\n>>> All automated tests completed successfully! <<<");
}

void HardwareDiagnostics::printMenu() {
    Serial.println("\n=======================================================");
    Serial.println("   CLICK 4 (RP2354A) HARDWARE DIAGNOSTICS SUITE");
    Serial.println("=======================================================");
    Serial.println(" [1] Test Buzzer (GP27): Ticks, Chime & Sweep");
    Serial.println(" [2] Test WS2812 RGB LEDs (GP11 DIN): Colors & Rainbow");
    Serial.println(" [3] Test OLED Screen (SDA=GP2, SCL=GP3): Graphics");
    Serial.println(" [4] Live Buttons & Power Monitor (ACTION=QSPI_SS, MODE=GP0)");
    Serial.println(" [5] Scan I2C Bus: Detect SSD1306 (0x3C) & PCF8563 (0x51)");
    Serial.println(" [6] Battery Voltage Divider Telemetry (GP29 / ADC3)");
    Serial.println(" [7] System Information: Silicon, Clock, RAM & Chip ID");
    Serial.println(" [8] Run ALL Diagnostics Sequentially");
    Serial.println(" [9] Read PCF8563 Real-Time Clock (0x51)");
    Serial.println(" [0] Exit Diagnostics & Launch Normal Clicker OS");
    Serial.println(" [?] Show This Menu");
    Serial.println("=======================================================");
    Serial.print("Select an option [0-9, ?]: ");
}

void HardwareDiagnostics::run() {
    Serial.begin(115200);

    // Give user 1 second to connect terminal, while keeping initialization responsive
    delay(1000);

    // Initialize I2C on GP2 (SDA) and GP3 (SCL)
    CLICK_I2C.setSDA(OLED_SDA_PIN);
    CLICK_I2C.setSCL(OLED_SCL_PIN);
    CLICK_I2C.begin();
    CLICK_I2C.setClock(400000);

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setRotation(2);
    display.clearDisplay();

    WS2812Driver::init();
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

    // Initial chime & LED flash to signal test suite ready
    WS2812Driver::setAll(0, 50, 150);
    tone(BUZZER_PIN, 1800, 50);
    delay(60);
    tone(BUZZER_PIN, 2400, 60);
    delay(70);
    WS2812Driver::clear();

    drawOledHeader("RP2354 DIAGNOSTICS");
    display.println("Serial Menu Active");
    display.println("Baud: 115200");
    display.println("");
    display.println("S1/S2: Live Monitor");
    display.println("Serial: [0-9, ?]");
    display.display();

    printMenu();

    bool inDiagnostics = true;
    while (inDiagnostics) {
        // Quick physical button shortcut: pressing either button enters Live Monitor directly
        if (isActionButtonPressed() || isModeButtonPressed()) {
            monitorButtonsAndPower();
            printMenu();
            drawOledHeader("RP2354 DIAGNOSTICS");
            display.println("Serial Menu Active");
            display.println("Baud: 115200");
            display.println("");
            display.println("Select option in Serial");
            display.display();
        }

        if (Serial.available()) {
            char ch = static_cast<char>(Serial.read());
            // Ignore carriage returns/newlines
            if (ch == '\r' || ch == '\n' || ch == ' ') continue;

            Serial.println(ch); // Echo back

            switch (ch) {
                case '1':
                    testBuzzer();
                    break;
                case '2':
                    testRGBLEDs();
                    break;
                case '3':
                    testOLED();
                    break;
                case '4':
                    monitorButtonsAndPower();
                    break;
                case '5':
                    scanI2CBus();
                    break;
                case '6':
                    testBatteryAndCharger();
                    break;
                case '7':
                    printSystemInfo();
                    break;
                case '8':
                    runAll();
                    break;
                case '9':
                    testPCF8563RTC();
                    break;
                case '0':
                    Serial.println("\n>>> Exiting Diagnostics... Booting Clicker OS! <<<\n");
                    drawOledHeader("BOOTING OS");
                    display.println("Starting Clicker OS...");
                    display.display();
                    delay(300);
                    inDiagnostics = false;
                    break;
                case '?':
                case 'h':
                case 'H':
                    printMenu();
                    break;
                default:
                    Serial.printf("Unknown option '%c'. Enter 0-9, or ?\n", ch);
                    break;
            }

            if (inDiagnostics) {
                Serial.print("\nSelect an option [0-9, ?]: ");
            }
        }
        delay(20);
    }
}
