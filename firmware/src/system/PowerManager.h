#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/uart.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <esp_phy_init.h>
#include <driver/adc.h>
#include "Display.h"
#include "InputManager.h"

#ifndef UART_TX0_PIN
#define UART_TX0_PIN   GPIO_NUM_1
#endif
#ifndef UART_RX0_PIN
#define UART_RX0_PIN   GPIO_NUM_3
#endif

class PowerManager {
public:
    /**
     * @brief Clear stuck I2C bus by sending 9 SCL clock pulses and generating STOP condition.
     */
    static void clearI2CBus(uint8_t sdaPin = I2C_SDA, uint8_t sclPin = I2C_SCL) {
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
    }

    /**
     * @brief Pre-Sleep Sequence: Teardown peripherals, configure I2C pullups, isolate UART,
     * hold control pins HIGH, disable RF/ADC, configure power domains, and arm wakeup triggers.
     */
    static void prepare_for_light_sleep() {
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
        // Never drive SDA/SCL to 0V while display VCC is 3.3V to prevent state machine lockup!
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
    }

    /**
     * @brief Post-Wakeup Sequence: Release pad holds, toggle EN, execute RST drain/POR sequence,
     * and restore UART & Wire.
     */
    static void restore_after_light_sleep() {
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
        Wire.begin(I2C_SDA, I2C_SCL, 400000);
    }

    /**
     * @brief Full Light Sleep Execution Wrapper with hardware re-initialization
     */
    static void enter_light_sleep() {
        prepare_for_light_sleep();

        // Enter Light Sleep (Synchronous execution blocks here until wake event)
        esp_light_sleep_start();

        restore_after_light_sleep();
    }

    // Backwards compatibility aliases
    static void enterLightSleep() {
        enter_light_sleep();
    }

    static void restoreAfterLightSleep() {
        restore_after_light_sleep();
    }
};

using LightSleepPowerManager = PowerManager;
