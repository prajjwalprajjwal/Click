#pragma once
#include <Arduino.h>
#include "PinConfig.h"
#include "SoundFX.h"
#include "WS2812.h"

#if defined(TARGET_RP2354) || defined(ARDUINO_ARCH_RP2040)
#include <hardware/structs/usb.h>
#include <hardware/regs/usb.h>
#endif

class BatteryManager {
private:
    static inline float filteredVoltage = 0.0f;
    static inline bool initialized = false;
    static inline uint32_t lastSampleTime = 0;
    static inline int displayedPercentage = -1;
    static inline uint32_t lastPctUpdateTime = 0;
    static inline bool wasCharging = false;
    static inline uint32_t unplugTime = 0;

    // Helper: Sort array for median calculation
    static void insertionSort(float arr[], int n) {
        for (int i = 1; i < n; i++) {
            float key = arr[i];
            int j = i - 1;
            while (j >= 0 && arr[j] > key) {
                arr[j + 1] = arr[j];
                j = j - 1;
            }
            arr[j + 1] = key;
        }
    }

    // Sample raw hardware ADC with 16-sample trimmed mean noise rejection
    static float readHardwareVoltage() {
        const int SAMPLES = 16;
        float rawSamples[SAMPLES];

        for (int i = 0; i < SAMPLES; i++) {
            uint32_t raw = analogRead(BATTERY_ADC_PIN);
            // 12-bit ADC (0..4095), 3.3V reference, 100k/100k voltage divider (2.0x)
            rawSamples[i] = (raw / 4095.0f) * 2.0f * 3.3f;
            delayMicroseconds(40);
        }

        // Sort to drop lowest 4 and highest 4 outliers (noise spikes from OLED charge pump)
        insertionSort(rawSamples, SAMPLES);
        float sum = 0.0f;
        for (int i = 4; i < 12; i++) {
            sum += rawSamples[i];
        }
        return sum / 8.0f;
    }

public:
    static int calculatePercentageFromVoltage(float v, bool charging) {
        float effectiveV = v;
        if (charging) {
            // While charging, terminal voltage is elevated by ~30-50mV due to charging current
            if (v < 4.00f) {
                effectiveV = v - 0.04f;
            } else if (v < 4.15f) {
                effectiveV = v - 0.04f * (1.0f - (v - 4.00f) / 0.15f);
            }
        }

        // Realistic non-linear Li-Ion / LiPo SoC curve matching consumer fuel gauges:
        // Fully charged resting / active cell stays at 100% down to 4.08V.
        // The vast capacity sits between 3.70V and 4.00V, avoiding sudden cliff drops.
        if (effectiveV >= 4.08f) return 100;
        if (effectiveV >= 4.00f) return 90 + static_cast<int>((effectiveV - 4.00f) / (4.08f - 4.00f) * 10.0f);
        if (effectiveV >= 3.90f) return 75 + static_cast<int>((effectiveV - 3.90f) / (4.00f - 3.90f) * 15.0f);
        if (effectiveV >= 3.80f) return 52 + static_cast<int>((effectiveV - 3.80f) / (3.90f - 3.80f) * 23.0f);
        if (effectiveV >= 3.72f) return 30 + static_cast<int>((effectiveV - 3.72f) / (3.80f - 3.72f) * 22.0f);
        if (effectiveV >= 3.62f) return 12 + static_cast<int>((effectiveV - 3.62f) / (3.72f - 3.62f) * 18.0f);
        if (effectiveV >= 3.48f) return 3 + static_cast<int>((effectiveV - 3.48f) / (3.62f - 3.48f) * 9.0f);
        if (effectiveV >= 3.35f) return static_cast<int>((effectiveV - 3.35f) / (3.48f - 3.35f) * 3.0f);
        return 0;
    }

public:
    static void init() {
#if defined(CHARGER_STAT_PIN) && (CHARGER_STAT_PIN >= 0)
        pinMode(CHARGER_STAT_PIN, INPUT_PULLUP);
#endif
#if defined(BATTERY_ADC_PIN) && (BATTERY_ADC_PIN >= 0)
        pinMode(BATTERY_ADC_PIN, INPUT);
#endif
        analogReadResolution(12);

        // Prime the filter immediately with clean hardware read
        filteredVoltage = readHardwareVoltage();
        wasCharging = isCharging();
        initialized = true;
        lastSampleTime = millis();
        lastPctUpdateTime = millis();
        displayedPercentage = -1;
        getPercentage(); // Establish initial SoC
    }

    static bool isCharging() {
#if defined(CHARGER_STAT_PIN) && (CHARGER_STAT_PIN >= 0)
        // ETA6003 STAT pin (GP1): actively pulls LOW during active charging; pulled HIGH via R9 when unplugged
        return digitalRead(CHARGER_STAT_PIN) == LOW;
#else
        return false;
#endif
    }

    static float getSmoothVoltage() {
        if (!initialized) {
            init();
        }

        uint32_t now = millis();

        // Solutions 6 & 27: Freeze ADC sampling during audio or LED pulses to prevent measuring IR battery droop!
        if (SoundFX::isPlaying() || WS2812Driver::isBusy()) {
            return filteredVoltage;
        }

        // Sample ADC periodically (every 1.5 seconds) to avoid burning power & continuous jitter
        if (now - lastSampleTime >= 1500 || filteredVoltage <= 0.5f) {
            lastSampleTime = now;
            float instV = readHardwareVoltage();

            if (filteredVoltage <= 0.5f) {
                filteredVoltage = instV;
            } else {
                // Smooth moving average
                filteredVoltage = (filteredVoltage * 0.85f) + (instV * 0.15f);
            }
        }
        return filteredVoltage;
    }

    static float getVoltage() {
        return getSmoothVoltage();
    }

    static int getPercentage() {
        float v = getSmoothVoltage();
        bool charging = isCharging();
        uint32_t now = millis();

        // Detect charger disconnect (falling edge of isCharging)
        if (wasCharging && !charging) {
            unplugTime = now; // Mark the moment USB was unplugged
        }
        wasCharging = charging;

        int rawPct = calculatePercentageFromVoltage(v, charging);
        if (rawPct > 100) rawPct = 100;
        if (rawPct < 0) rawPct = 0;

        // If charging just completed or disconnected with full voltage (>= 4.08V):
        if (!charging && v >= 4.08f) {
            rawPct = 100;
        }

        // First boot initialization: accept initial reading immediately
        if (displayedPercentage < 0) {
            displayedPercentage = rawPct;
            lastPctUpdateTime = now;
            return displayedPercentage;
        }

        if (charging) {
            // CHARGING MODE:
            // 1. Percentage must NEVER decrease while connected to charger!
            // 2. Step upward smoothly (at most 1% every 5 seconds) to avoid jumpy displays.
            if (rawPct > displayedPercentage) {
                if (now - lastPctUpdateTime >= 5000) {
                    displayedPercentage++;
                    lastPctUpdateTime = now;
                }
            }
            // If rawPct <= displayedPercentage, strictly hold current displayedPercentage!
        } else {
            // BATTERY DISCHARGE MODE:
            // Post-unplug surface charge relaxation:
            // LiPo terminal voltage relaxes 30-80mV during the first 2 minutes after unplugging.
            // Hold the percentage steady to prevent optical drops.
            bool inUnplugRelaxation = (unplugTime > 0 && (now - unplugTime < 120000));

            if (inUnplugRelaxation) {
                // Do not allow decrease during post-unplug relaxation
            } else if (v < 3.45f) {
                // Near dead battery cutoff: update immediately to warn user
                displayedPercentage = rawPct;
                lastPctUpdateTime = now;
            } else if (rawPct < displayedPercentage) {
                // Smooth discharge rate: at most 1% drop every 35 seconds
                if (now - lastPctUpdateTime >= 35000) {
                    displayedPercentage--;
                    lastPctUpdateTime = now;
                }
            } else if (rawPct > displayedPercentage && (rawPct - displayedPercentage >= 4)) {
                // Only allow upward recovery on battery after deep relaxation with >= 4% hysteresis
                if (now - lastPctUpdateTime >= 40000) {
                    displayedPercentage++;
                    lastPctUpdateTime = now;
                }
            }
        }

        if (displayedPercentage > 100) displayedPercentage = 100;
        if (displayedPercentage < 0) displayedPercentage = 0;

        return displayedPercentage;
    }
};
