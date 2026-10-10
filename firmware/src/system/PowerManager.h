#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "Display.h"
#include "InputManager.h"
#include "PinConfig.h"
#include "WS2812.h"

#if defined(ESP32)
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/uart.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <esp_phy_init.h>
#include <driver/adc.h>

#ifndef UART_TX0_PIN
#define UART_TX0_PIN   GPIO_NUM_1
#endif
#ifndef UART_RX0_PIN
#define UART_RX0_PIN   GPIO_NUM_3
#endif
#elif defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040)
#include <hardware/clocks.h>
#include <hardware/pll.h>
#include <hardware/xosc.h>
#include <hardware/structs/rosc.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hardware/adc.h>
#include <hardware/structs/scb.h>
#include <pico/runtime_init.h>
#if defined(TARGET_RP2354)
#include <hardware/powman.h>
#endif
#endif

class PowerManager {
public:
    /**
     * @brief Clear stuck I2C bus by sending 9 SCL clock pulses and generating STOP condition.
     */
    static void clearI2CBus(uint8_t sdaPin = I2C_SDA, uint8_t sclPin = I2C_SCL) {
#if defined(ESP32)
        pinMode(sdaPin, INPUT_PULLUP);
        pinMode(sclPin, OUTPUT_OPEN_DRAIN);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(10);

        for (uint8_t i = 0; i < 9; i++) {
            digitalWrite(sclPin, LOW);
            delayMicroseconds(5);
            digitalWrite(sclPin, HIGH);
            delayMicroseconds(5);
        }

        pinMode(sdaPin, OUTPUT_OPEN_DRAIN);
        digitalWrite(sdaPin, LOW);
        delayMicroseconds(5);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(sdaPin, HIGH);
        delayMicroseconds(10);

        pinMode(sdaPin, INPUT_PULLUP);
        pinMode(sclPin, INPUT_PULLUP);
        delayMicroseconds(10);
#else
        pinMode(sdaPin, INPUT_PULLUP);
        pinMode(sclPin, OUTPUT);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(10);

        for (uint8_t i = 0; i < 9; i++) {
            digitalWrite(sclPin, LOW);
            delayMicroseconds(5);
            digitalWrite(sclPin, HIGH);
            delayMicroseconds(5);
        }

        pinMode(sdaPin, OUTPUT);
        digitalWrite(sdaPin, LOW);
        delayMicroseconds(5);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(5);
        digitalWrite(sdaPin, HIGH);
        delayMicroseconds(10);

        pinMode(sdaPin, INPUT_PULLUP);
        pinMode(sclPin, INPUT_PULLUP);
        delayMicroseconds(10);
#endif
    }

    /**
     * @brief Pre-Sleep Sequence: Teardown peripherals, configure I2C pullups, isolate UART,
     * hold control pins HIGH, disable RF/ADC, configure power domains, and arm wakeup triggers.
     */
    static void prepare_for_light_sleep() {
#if defined(ESP32)
        // 1. Flush UART Console & isolate TX/RX to prevent back-feeding CH340
        Serial.flush();
        uart_wait_tx_idle_polling(UART_NUM_0);

        gpio_reset_pin(UART_TX0_PIN);
        gpio_reset_pin(UART_RX0_PIN);
        gpio_set_direction(UART_TX0_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(UART_TX0_PIN, 0);
        gpio_set_pull_mode(UART_TX0_PIN, GPIO_FLOATING);
        gpio_hold_en(UART_TX0_PIN);

        gpio_set_direction(UART_RX0_PIN, GPIO_MODE_INPUT);
        gpio_set_pull_mode(UART_RX0_PIN, GPIO_PULLDOWN_ONLY);
        gpio_hold_en(UART_RX0_PIN);

        // 2. Safely close I2C bus and keep SDA / SCL at logic HIGH (3.3V)
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        delay(10);

        Wire.end();

        gpio_reset_pin((gpio_num_t)I2C_SDA);
        gpio_reset_pin((gpio_num_t)I2C_SCL);
        gpio_set_direction((gpio_num_t)I2C_SDA, GPIO_MODE_INPUT);
        gpio_set_direction((gpio_num_t)I2C_SCL, GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)I2C_SDA, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode((gpio_num_t)I2C_SCL, GPIO_PULLUP_ONLY);

        // 3. Set OLED RST and EN to HIGH before sleep and hold them
        if (OLED_RST_PIN >= 0) {
            gpio_reset_pin((gpio_num_t)OLED_RST_PIN);
            gpio_set_direction((gpio_num_t)OLED_RST_PIN, GPIO_MODE_OUTPUT);
            gpio_set_level((gpio_num_t)OLED_RST_PIN, 1);
            gpio_hold_en((gpio_num_t)OLED_RST_PIN);
        }

        if (OLED_EN_PIN >= 0) {
            gpio_reset_pin((gpio_num_t)OLED_EN_PIN);
            gpio_set_direction((gpio_num_t)OLED_EN_PIN, GPIO_MODE_OUTPUT);
            gpio_set_level((gpio_num_t)OLED_EN_PIN, 1);
            gpio_hold_en((gpio_num_t)OLED_EN_PIN);
        }

        // 4. Configure Button RTC Wakeup & Pullups (GPIO14 & GPIO32)
        pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
        pinMode(ACTION_BUTTON_PIN, INPUT_PULLUP);
        
        rtc_gpio_init((gpio_num_t)MODE_BUTTON_PIN);
        rtc_gpio_set_direction((gpio_num_t)MODE_BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en((gpio_num_t)MODE_BUTTON_PIN);
        rtc_gpio_pulldown_dis((gpio_num_t)MODE_BUTTON_PIN);

        rtc_gpio_init((gpio_num_t)ACTION_BUTTON_PIN);
        rtc_gpio_set_direction((gpio_num_t)ACTION_BUTTON_PIN, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en((gpio_num_t)ACTION_BUTTON_PIN);
        rtc_gpio_pulldown_dis((gpio_num_t)ACTION_BUTTON_PIN);

        gpio_wakeup_enable((gpio_num_t)MODE_BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable((gpio_num_t)ACTION_BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();

        esp_sleep_enable_ext0_wakeup((gpio_num_t)MODE_BUTTON_PIN, 0); // Wake on LOW (press)
        esp_sleep_enable_ext1_wakeup(1ULL << ACTION_BUTTON_PIN, ESP_EXT1_WAKEUP_ALL_LOW);

        gpio_deep_sleep_hold_en();

        // 5. Power Domains & Subsystem Teardown
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
        esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
        esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);

        #if CONFIG_SOC_ADC_SUPPORTED
        adc_power_release();
        #endif

        esp_wifi_stop();
        #if CONFIG_BT_ENABLED
        esp_bt_controller_disable();
        #endif
#else
        Serial.flush();
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        delay(5);
        CLICK_I2C.end();

        // 1. High-Z with weak pullup (matching external hardware pullups, 0µA current)
        pinMode(I2C_SDA, INPUT_PULLUP);
        pinMode(I2C_SCL, INPUT_PULLUP);
        pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
#if ACTION_BUTTON_PIN >= 0
        pinMode(ACTION_BUTTON_PIN, INPUT_PULLUP);
#endif
#if defined(CHARGER_STAT_PIN) && (CHARGER_STAT_PIN >= 0)
        pinMode(CHARGER_STAT_PIN, INPUT_PULLUP);
#endif

        // 2. Clear and solidly clamp WS2812 DIN to GND (0µA current)
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        WS2812Driver::clearAndHaltForSleep();
#endif

        // 3. Clamp Buzzer NPN base to 0V ground (cutoff, 0µA current)
#if defined(BUZZER_PIN) && (BUZZER_PIN >= 0)
        pinMode(BUZZER_PIN, OUTPUT);
        digitalWrite(BUZZER_PIN, LOW);
#endif

        // 4. Power down ADC core analog converter and disable digital input buffer on battery sense pin
#if defined(BATTERY_ADC_PIN) && (BATTERY_ADC_PIN >= 0)
        adc_run(false);
        hw_clear_bits(&adc_hw->cs, ADC_CS_EN_BITS);
        gpio_set_input_enabled(BATTERY_ADC_PIN, false);
        gpio_disable_pulls(BATTERY_ADC_PIN);
#endif

        // 5. Put unused GPIOs in high impedance and disable their input buffers.
        for (uint pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
            if (pin != MODE_BUTTON_PIN && pin != I2C_SDA && pin != I2C_SCL &&
#if ACTION_BUTTON_PIN >= 0
                pin != ACTION_BUTTON_PIN &&
#endif
                pin != WS2812_PIN && pin != BUZZER_PIN && pin != BATTERY_ADC_PIN &&
                pin != CHARGER_STAT_PIN) {
                gpio_set_dir(pin, false);
                gpio_set_input_enabled(pin, false);
                gpio_disable_pulls(pin);
            }
        }
#endif
    }

    /**
     * @brief Post-Wakeup Sequence: Release pad holds, toggle EN, execute RST drain/POR sequence,
     * and restore UART & Wire.
     */
    static void restore_after_light_sleep() {
#if defined(ESP32)
        // 1. Release ALL GPIO pad holds immediately
        gpio_hold_dis(UART_TX0_PIN);
        gpio_hold_dis(UART_RX0_PIN);
        if (OLED_RST_PIN >= 0) {
            gpio_hold_dis((gpio_num_t)OLED_RST_PIN);
        }
        if (OLED_EN_PIN >= 0) {
            gpio_hold_dis((gpio_num_t)OLED_EN_PIN);
        }
        gpio_deep_sleep_hold_dis();

        // 2. Hardware Enable / Power Pin Toggle (if present)
        if (OLED_EN_PIN >= 0) {
            pinMode(OLED_EN_PIN, OUTPUT);
            digitalWrite(OLED_EN_PIN, LOW);
            delay(20);
            digitalWrite(OLED_EN_PIN, HIGH);
            delay(20);
        }

        // 3. Hardware Reset Sequence: Drain latching gates & allow clean POR
        if (OLED_RST_PIN >= 0) {
            pinMode(OLED_RST_PIN, OUTPUT);
            digitalWrite(OLED_RST_PIN, LOW);
            delay(20); // 20ms low to drain latched charge
            digitalWrite(OLED_RST_PIN, HIGH);
            delay(50); // 50ms Power-On Reset stabilization
        } else {
            delay(20);
        }

        // 4. Restore UART for debugging
        Serial.begin(115200);

        // 5. Restore Button PinModes
        rtc_gpio_deinit((gpio_num_t)MODE_BUTTON_PIN);
        rtc_gpio_deinit((gpio_num_t)ACTION_BUTTON_PIN);
        pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
        pinMode(ACTION_BUTTON_PIN, INPUT_PULLUP);

        // 6. 9-Clock Cycle Bus Clear and Hardware I2C Re-Initialization
        clearI2CBus(I2C_SDA, I2C_SCL);
        CLICK_I2C.begin(I2C_SDA, I2C_SCL, 400000);
#else
        if (OLED_RST_PIN >= 0) {
            pinMode(OLED_RST_PIN, OUTPUT);
            digitalWrite(OLED_RST_PIN, LOW);
            delay(20);
            digitalWrite(OLED_RST_PIN, HIGH);
            delay(50);
        } else {
            delay(20);
        }

        // Re-enable input buffers on active pins
        gpio_set_input_enabled(MODE_BUTTON_PIN, true);
        pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
#if defined(CHARGER_STAT_PIN) && (CHARGER_STAT_PIN >= 0)
        gpio_set_input_enabled(CHARGER_STAT_PIN, true);
        pinMode(CHARGER_STAT_PIN, INPUT_PULLUP);
#endif
#if defined(BATTERY_ADC_PIN) && (BATTERY_ADC_PIN >= 0)
        hw_set_bits(&adc_hw->cs, ADC_CS_EN_BITS);
        gpio_set_input_enabled(BATTERY_ADC_PIN, true);
        pinMode(BATTERY_ADC_PIN, INPUT);
#endif
#if ACTION_BUTTON_PIN >= 0
        pinMode(ACTION_BUTTON_PIN, INPUT_PULLUP);
#endif

        clearI2CBus(I2C_SDA, I2C_SCL);
#if defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040)
        CLICK_I2C.setSDA(I2C_SDA);
        CLICK_I2C.setSCL(I2C_SCL);
        CLICK_I2C.begin();
#else
        CLICK_I2C.begin(I2C_SDA, I2C_SCL);
#endif
        CLICK_I2C.setClock(400000);
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        WS2812Driver::reinit();
#endif
#endif
    }

    /**
     * @brief Full Light Sleep Execution Wrapper with hardware re-initialization
     */
    static void enter_light_sleep() {
        prepare_for_light_sleep();

#if defined(ESP32)
        // Enter Light Sleep (Synchronous execution blocks here until wake event)
        esp_light_sleep_start();
#elif defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040)
        // Wait for both buttons to be released
        while (isModeButtonPressed() || isActionButtonPressed()) {
            delay(10);
        }

        // Switch clk_sys down to direct XOSC 12MHz for ultra-low power consumption (< 1mA)
        set_sys_clock_khz(12000, false);

        // Requirement 1: Any button (MODE or ACTION) wakes the device from sleep!
        while (!isModeButtonPressed() && !isActionButtonPressed()) {
            delay(20); // Invokes ARM Cortex-M33 __wfi()
        }

        // Wait for button release on wake so it doesn't immediately click in the applet
        while (isModeButtonPressed() || isActionButtonPressed()) {
            delay(10);
        }
        // Restore active system clock to 48MHz
        set_sys_clock_khz(48000, false);
#else
        while (!isModeButtonPressed() && !isActionButtonPressed()) {
            delay(10);
        }
#endif

        restore_after_light_sleep();
    }

#if defined(TARGET_RP2354) && HAS_POWMAN_TIMER
    using PstateResumeCallback = void (*)(pstate_bitset_t *sleepState);
    static constexpr uint32_t PSTATE_WAKE_MARKER = 0x43504D50;

    static void pstateResumeCallback(pstate_bitset_t *sleepState) {
        (void)sleepState;
        Serial.println("[PowerManager] Resumed from Powman P-state");
    }

    static void dispatch_pstate_resume() {
        if (!(powman_hw->chip_reset & POWMAN_CHIP_RESET_HAD_SWCORE_PD_BITS) ||
            powman_hw->scratch[5] != PSTATE_WAKE_MARKER) {
            return;
        }

        pstate_bitset_t sleepState = pstate_bitset_none();
        pstate_bitset_from_powman_power_state(&sleepState, powman_hw->scratch[6]);
        PstateResumeCallback callback = reinterpret_cast<PstateResumeCallback>(
            static_cast<uintptr_t>(powman_hw->scratch[7]));

        powman_disable_all_wakeups();
        powman_hw->scratch[5] = 0;
        powman_hw->scratch[6] = 0;
        powman_hw->scratch[7] = 0;

        if (callback) {
            callback(&sleepState);
        }
    }

    static int enter_pstate_sleep(PstateResumeCallback resumeCallback) {
        prepare_for_light_sleep();

        while (isModeButtonPressed() || isActionButtonPressed()) {
            delay(10);
        }

        powman_disable_all_wakeups();
        powman_enable_gpio_wakeup(0, MODE_BUTTON_PIN, false, false);
#if ACTION_BUTTON_PIN >= 0 && ACTION_BUTTON_PIN < NUM_BANK0_GPIOS
        powman_enable_gpio_wakeup(1, ACTION_BUTTON_PIN, false, false);
#endif

#if ACTION_BUTTON_PIN < 0
        Serial.println("[PowerManager] P-state wake: MODE only; ACTION is BOOTSEL/QSPI, not a Powman GPIO");
#elif ACTION_BUTTON_PIN >= NUM_BANK0_GPIOS
        Serial.println("[PowerManager] P-state wake: MODE only; ACTION pin is outside bank-0 GPIOs");
#else
        Serial.println("[PowerManager] P-state wake: MODE and ACTION GPIOs");
#endif

        powman_power_state sleepState = POWMAN_POWER_STATE_NONE;
        
        powman_power_state wakeupState = powman_get_power_state();
        if (!powman_configure_wakeup_state(sleepState, wakeupState)) {
            powman_disable_all_wakeups();
            return -1;
        }

        // Must clear boot registers so bootrom knows to boot normally from flash upon wake
        powman_hw->boot[0] = 0;
        powman_hw->boot[1] = 0;
        powman_hw->boot[2] = 0;
        powman_hw->boot[3] = 0;

        powman_hw->scratch[5] = PSTATE_WAKE_MARKER;
        powman_hw->scratch[6] = sleepState;
        powman_hw->scratch[7] = static_cast<uint32_t>(
            reinterpret_cast<uintptr_t>(resumeCallback));

        Serial.flush();
        int result = powman_set_power_state(sleepState);
        if (result != 0) {
            powman_hw->scratch[5] = 0;
            powman_hw->scratch[6] = 0;
            powman_hw->scratch[7] = 0;
            powman_disable_all_wakeups();
            return result;
        }

        while (true) {
            __wfi();
        }
    }
#endif

    // Backwards compatibility aliases
    static void enterLightSleep() {
        enter_light_sleep();
    }

    static void restoreAfterLightSleep() {
        restore_after_light_sleep();
    }
};

using LightSleepPowerManager = PowerManager;
