#pragma once

#include <Arduino.h>

#if defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040)
#include <Wire.h>
// ==============================================================================
// Hardware Pinout: Click 4 PCBA (Raspberry Pi RP2354A)
// ==============================================================================
#ifndef I2C_SDA
#define I2C_SDA             2   // GP2: I2C1 SDA (SSD1306 OLED & PCF8563 RTC)
#endif

#ifndef I2C_SCL
#define I2C_SCL             3   // GP3: I2C1 SCL (SSD1306 OLED & PCF8563 RTC)
#endif

#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN        I2C_SDA
#endif

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN        I2C_SCL
#endif

#ifndef OLED_RST_PIN
#define OLED_RST_PIN        -1
#endif

#ifndef OLED_EN_PIN
#define OLED_EN_PIN         -1
#endif

#ifndef MODE_BUTTON_PIN
#define MODE_BUTTON_PIN     0   // GP0: MODE Button (Active LOW)
#endif

#ifndef ACTION_BUTTON_PIN
#define ACTION_BUTTON_PIN   -1  // QSPI_SS / BOOTSEL (Active LOW)
#endif

#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN     29  // GP29: Battery Voltage Sensor (ADC3)
#endif

#ifndef CHARGER_STAT_PIN
#define CHARGER_STAT_PIN    1   // GP1: ETA6003 STAT pin (Active LOW when charging, pulled up to 3.3V via R9)
#endif

#ifndef WS2812_PIN
#define WS2812_PIN          11  // GP11: WS2812 / SK6812 Addressable RGB LEDs (DIN)
#endif

#ifndef RGB_LED_PIN
#define RGB_LED_PIN         WS2812_PIN
#endif

#ifndef RGB_DATA_PIN
#define RGB_DATA_PIN        WS2812_PIN
#endif

#ifndef BUZZER_PIN
#define BUZZER_PIN          27  // GP27: Sound Buzzer (via NPN transistor)
#endif

#define CLICK_I2C           Wire1

#else
// ==============================================================================
// Hardware Pinout: Click 1 Prototype (ESP32-WROOM-32E)
// ==============================================================================
#ifndef I2C_SDA
#define I2C_SDA             21  // GPIO 21: I2C SDA
#endif

#ifndef I2C_SCL
#define I2C_SCL             22  // GPIO 22: I2C SCL
#endif

#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN        I2C_SDA
#endif

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN        I2C_SCL
#endif

#ifndef OLED_RST_PIN
#define OLED_RST_PIN        -1
#endif

#ifndef OLED_EN_PIN
#define OLED_EN_PIN         -1
#endif

#ifndef MODE_BUTTON_PIN
#define MODE_BUTTON_PIN     14  // GPIO 14: MODE Button
#endif

#ifndef ACTION_BUTTON_PIN
#define ACTION_BUTTON_PIN   32  // GPIO 32: ACTION Button
#endif

#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN     35  // GPIO 35: Battery ADC
#endif

#ifndef CHARGER_STAT_PIN
#define CHARGER_STAT_PIN    33  // GPIO 33: ETA6003 STAT
#endif

#ifndef WS2812_PIN
#define WS2812_PIN          -1
#endif

#ifndef BUZZER_PIN
#define BUZZER_PIN          -1
#endif

#define CLICK_I2C           Wire

#endif

// ==============================================================================
// Unified Button Reading Helpers
// ==============================================================================
static inline bool isActionButtonPressed() {
#if defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040)
    return BOOTSEL;
#else
    return digitalRead(ACTION_BUTTON_PIN) == LOW;
#endif
}

static inline bool isModeButtonPressed() {
    return digitalRead(MODE_BUTTON_PIN) == LOW;
}
