#pragma once
#include <Arduino.h>
#include "PinConfig.h"

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
        // While charging in constant-current phase, there is a small IR drop (~30mV).
        // In constant-voltage phase (approaching 4.20V), current tapers down to ~10mA,
        // so IR drop drops to 0. We taper any IR compensation to 0 at 4.15V.
        float effectiveV = v;
        if (charging) {
            if (v < 4.00f) {
                effectiveV = v - 0.03f;
            } else if (v < 4.15f) {
                effectiveV = v - 0.03f * (1.0f - (v - 4.00f) / 0.15f);
            }
        }

        // Calibrated non-linear Li-Ion SoC curve:
        // Fully charged cell rests at 4.15V - 4.20V
        if (effectiveV >= 4.15f) return 100;
        if (effectiveV >= 4.08f) return 90 + static_cast<int>((effectiveV - 4.08f) / (4.15f - 4.08f) * 10.0f);
        if (effectiveV >= 3.98f) return 76 + static_cast<int>((effectiveV - 3.98f) / (4.08f - 3.98f) * 14.0f);
        if (effectiveV >= 3.88f) return 58 + static_cast<int>((effectiveV - 3.88f) / (3.98f - 3.88f) * 18.0f);
        if (effectiveV >= 3.80f) return 42 + static_cast<int>((effectiveV - 3.80f) / (3.88f - 3.80f) * 16.0f);
        if (effectiveV >= 3.73f) return 26 + static_cast<int>((effectiveV - 3.73f) / (3.80f - 3.73f) * 16.0f);
        if (effectiveV >= 3.65f) return 14 + static_cast<int>((effectiveV - 3.65f) / (3.73f - 3.65f) * 12.0f);
        if (effectiveV >= 3.52f) return 5 + static_cast<int>((effectiveV - 3.52f) / (3.65f - 3.52f) * 9.0f);
        if (effectiveV >= 3.35f) return static_cast<int>((effectiveV - 3.35f) / (3.52f - 3.35f) * 5.0f);
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
        // ETA6003 STAT pin actively pulls LOW during active charge cycle
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

        // If charging just completed (STAT pin went HIGH, but battery is >= 4.14V):
        // Treat as fully charged 100%!
        if (!charging && v >= 4.14f) {
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
            // LiPo terminal voltage relaxes 30-80mV during the first 60 seconds after unplugging.
            // Hold the percentage steady to prevent optical drops.
            bool inUnplugRelaxation = (unplugTime > 0 && (now - unplugTime < 60000));

            if (inUnplugRelaxation) {
                // Do not allow decrease during post-unplug relaxation
            } else if (v < 3.45f) {
                // Near dead battery cutoff: update immediately to warn user
                displayedPercentage = rawPct;
                lastPctUpdateTime = now;
            } else if (rawPct < displayedPercentage) {
                // Smooth discharge rate: at most 1% drop every 25 seconds
                if (now - lastPctUpdateTime >= 25000) {
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
