#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <cmath>
#include "PinConfig.h"

enum class LedEffect : uint8_t {
    NONE = 0,
    STATIC_COLOR,
    FLASH,
    BREATHE,
    PULSE_ONCE,
    CHARGING_BREATHE,
    CELEBRATION
};

/**
 * @brief Driver and FX Engine for 2x WS2812 Addressable RGB LEDs (GP11 DIN).
 */
class WS2812Driver {
public:
    static const uint8_t NUM_LEDS = 2;

    static void init() {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) {
            pixels.begin();
            pixels.setBrightness(60); // 0-255 default brightness
            pixels.clear();
            pixels.show();
            initialized = true;
            currentEffect = LedEffect::NONE;
        }
#endif
    }

    static void reinit() {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        pixels.begin();
        pixels.setBrightness(60);
        pixels.clear();
        pixels.show();
        initialized = true;
        currentEffect = LedEffect::NONE;
#endif
    }

    static void setPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized || index >= NUM_LEDS) return;
        pixels.setPixelColor(index, pixels.Color(r, g, b));
#endif
    }

    static void setBrightness(uint8_t b) {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        pixels.setBrightness(b);
#endif
    }

    static void show() {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        pixels.show();
#endif
    }

    static void setAll(uint8_t r, uint8_t g, uint8_t b) {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            pixels.setPixelColor(i, pixels.Color(r, g, b));
        }
        pixels.show();
        currentEffect = LedEffect::STATIC_COLOR;
#endif
    }

    static void clear() {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        pixels.clear();
        pixels.show();
        currentEffect = LedEffect::NONE;
#endif
    }

    // Bulletproof shutdown sequence before MCU dormant sleep:
    // Flushes PIO FIFO, latches 0s, and clamps DIN to 0V GND with pull-down
    static void clearAndHaltForSleep() {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        currentEffect = LedEffect::NONE;
        
        // 1. Send all zeroes to the LEDs
        pixels.clear();
        pixels.show();
        delay(5);
        
        // 2. Send second clear frame to guarantee both SK6812 chips latch zero
        pixels.clear();
        pixels.show();
        delay(5);
        
        // 3. Clear SIO output register bit BEFORE switching pin multiplexer to SIO.
        // This guarantees zero high-glitch pulse on GP11 when transitioning from PIO.
#if defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2350)
        sio_hw->gpio_clr = (1u << WS2812_PIN);
        gpio_set_dir(WS2812_PIN, GPIO_OUT);
        gpio_set_function(WS2812_PIN, GPIO_FUNC_SIO);
        gpio_put(WS2812_PIN, 0);
        gpio_pull_down(WS2812_PIN);
#else
        digitalWrite(WS2812_PIN, LOW);
        pinMode(WS2812_PIN, OUTPUT);
        digitalWrite(WS2812_PIN, LOW);
#endif
#endif
    }

    // Direct solid red for Sisyphus movement
    static void glowRed(bool active) {
        if (active) {
            setAll(255, 0, 0);
        } else {
            clear();
        }
    }

    // Temporary non-blocking color flash
    static void flash(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs) {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        setAll(r, g, b);
        currentEffect = LedEffect::FLASH;
        effectEndMs = millis() + durationMs;
#endif
    }

    // Smooth sinusoidal breathing glow
    static void startBreathe(uint8_t r, uint8_t g, uint8_t b, uint16_t periodMs = 2000) {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        breatheR = r;
        breatheG = g;
        breatheB = b;
        breathePeriod = periodMs > 0 ? periodMs : 2000;
        currentEffect = LedEffect::BREATHE;
#endif
    }

    // Gentle single-cycle ambient breath (fades in and out smoothly, then clears)
    static void pulseOnce(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs = 1200) {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        pulseR = r;
        pulseG = g;
        pulseB = b;
        pulseStartMs = millis();
        pulseDurationMs = durationMs > 0 ? durationMs : 1200;
        currentEffect = LedEffect::PULSE_ONCE;
#endif
    }

    // High energy dual-LED rainbow party celebration!
    static void startCelebration(uint16_t durationMs = 3000) {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        currentEffect = LedEffect::CELEBRATION;
        effectEndMs = millis() + durationMs;
        rainbowHue = 0;
#endif
    }

    // Multi-color breathing animation while charging via USB
    static void startChargingBreathe() {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        currentEffect = LedEffect::CHARGING_BREATHE;
#endif
    }

    static bool isChargingBreatheActive() {
        return (currentEffect == LedEffect::CHARGING_BREATHE);
    }

    static bool isCelebrationActive() {
        return (currentEffect == LedEffect::CELEBRATION);
    }

    static bool isBusy() {
        return (currentEffect != LedEffect::NONE && currentEffect != LedEffect::CHARGING_BREATHE);
    }

    // Periodic update loop called in main loop()
    static void update() {
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
        if (!initialized) return;
        uint32_t now = millis();

        if (currentEffect == LedEffect::FLASH) {
            if (now >= effectEndMs) {
                clear();
            }
        } else if (currentEffect == LedEffect::PULSE_ONCE) {
            uint32_t elapsed = now - pulseStartMs;
            if (elapsed >= pulseDurationMs) {
                clear();
            } else if (now - lastUpdateMs >= 20) {
                lastUpdateMs = now;
                // Sinusoidal bell curve (0.0 -> 1.0 -> 0.0)
                float progress = (float)elapsed / (float)pulseDurationMs; // 0.0 to 1.0
                float intensity = sinf(progress * 3.14159265f); // Smooth sine pulse
                uint8_t r = static_cast<uint8_t>(pulseR * intensity);
                uint8_t g = static_cast<uint8_t>(pulseG * intensity);
                uint8_t b = static_cast<uint8_t>(pulseB * intensity);
                for (uint8_t i = 0; i < NUM_LEDS; i++) {
                    pixels.setPixelColor(i, pixels.Color(r, g, b));
                }
                pixels.show();
            }
        } else if (currentEffect == LedEffect::CHARGING_BREATHE) {
            if (now - lastUpdateMs >= 25) {
                lastUpdateMs = now;
                chargingHue += 240; // Smooth continuous multicolor hue cycling
                // Sinusoidal breathing curve (25% to 100% brightness)
                float angle = (float)(now % 3200) / 3200.0f * 6.283185f;
                float breath = 0.25f + 0.75f * (0.5f + 0.5f * sinf(angle));
                uint8_t val = static_cast<uint8_t>(200 * breath);
                // Dual-LED complementary rotating rainbow hues
                pixels.setPixelColor(0, pixels.ColorHSV(chargingHue, 240, val));
                pixels.setPixelColor(1, pixels.ColorHSV(chargingHue + 21845, 240, val)); // 120-degree phase offset
                pixels.show();
            }
        } else if (currentEffect == LedEffect::CELEBRATION) {
            if (now >= effectEndMs) {
                clear();
            } else if (now - lastUpdateMs >= 20) {
                lastUpdateMs = now;
                rainbowHue += 1200; // Fast vivid cycle
                // LED 0 and LED 1 complementary rotating hues
                pixels.setPixelColor(0, pixels.ColorHSV(rainbowHue, 255, 230));
                pixels.setPixelColor(1, pixels.ColorHSV(rainbowHue + 32768, 255, 230));
                pixels.show();
            }
        } else if (currentEffect == LedEffect::BREATHE) {
            if (now - lastUpdateMs >= 25) {
                lastUpdateMs = now;
                float angle = (float)(now % breathePeriod) / (float)breathePeriod * 6.283185f;
                float intensity = 0.5f + 0.5f * sinf(angle); // 0.0 to 1.0
                uint8_t r = static_cast<uint8_t>(breatheR * intensity);
                uint8_t g = static_cast<uint8_t>(breatheG * intensity);
                uint8_t b = static_cast<uint8_t>(breatheB * intensity);
                for (uint8_t i = 0; i < NUM_LEDS; i++) {
                    pixels.setPixelColor(i, pixels.Color(r, g, b));
                }
                pixels.show();
            }
        }
#endif
    }

private:
#if defined(WS2812_PIN) && (WS2812_PIN >= 0)
    static inline Adafruit_NeoPixel pixels{NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800};
#endif
    static inline bool initialized = false;
    static inline LedEffect currentEffect = LedEffect::NONE;
    static inline uint32_t effectEndMs = 0;
    static inline uint32_t lastUpdateMs = 0;
    static inline uint16_t rainbowHue = 0;
    static inline uint16_t chargingHue = 0;

    static inline uint8_t pulseR = 0;
    static inline uint8_t pulseG = 0;
    static inline uint8_t pulseB = 0;
    static inline uint32_t pulseStartMs = 0;
    static inline uint16_t pulseDurationMs = 1200;

    static inline uint8_t breatheR = 0;
    static inline uint8_t breatheG = 0;
    static inline uint8_t breatheB = 0;
    static inline uint16_t breathePeriod = 2000;
};
