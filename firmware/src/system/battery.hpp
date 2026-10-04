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
    static void init() {
#if defined(CHARGER_STAT_PIN) && (CHARGER_STAT_PIN >= 0)
        pinMode(CHARGER_STAT_PIN, INPUT_PULLUP);
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

        // When actively charging, charger current creates an IR drop (~0.12V-0.18V)
        // across cell internal resistance & protection FETs, elevating terminal voltage.
        float effectiveV = charging ? (v - 0.15f) : v;

        // Calibrated Non-Linear LiPo Discharge Open-Circuit Voltage Curve
        int rawPct = 0;
        if (effectiveV >= 4.18f) rawPct = 100;
        else if (effectiveV >= 4.08f) rawPct = 90 + static_cast<int>((effectiveV - 4.08f) / (4.18f - 4.08f) * 10.0f);
        else if (effectiveV >= 3.98f) rawPct = 78 + static_cast<int>((effectiveV - 3.98f) / (4.08f - 3.98f) * 12.0f);
        else if (effectiveV >= 3.88f) rawPct = 62 + static_cast<int>((effectiveV - 3.88f) / (3.98f - 3.88f) * 16.0f);
        else if (effectiveV >= 3.80f) rawPct = 48 + static_cast<int>((effectiveV - 3.80f) / (3.88f - 3.80f) * 14.0f);
        else if (effectiveV >= 3.73f) rawPct = 32 + static_cast<int>((effectiveV - 3.73f) / (3.80f - 3.73f) * 16.0f);
        else if (effectiveV >= 3.65f) rawPct = 18 + static_cast<int>((effectiveV - 3.65f) / (3.73f - 3.65f) * 14.0f);
        else if (effectiveV >= 3.52f) rawPct = 6 + static_cast<int>((effectiveV - 3.52f) / (3.65f - 3.52f) * 12.0f);
        else if (effectiveV >= 3.35f) rawPct = static_cast<int>((effectiveV - 3.35f) / (3.52f - 3.35f) * 6.0f);
        else rawPct = 0;

        if (rawPct > 100) rawPct = 100;
        if (rawPct < 0) rawPct = 0;

        // First boot initialization: accept initial reading immediately
        if (displayedPercentage < 0) {
            displayedPercentage = rawPct;
            lastPctUpdateTime = now;
            return displayedPercentage;
        }

        // Post-Unplug Surface Charge Relaxation Protection:
        // When unplugged, a LiPo's terminal voltage relaxes 50-150mV over the first 45 seconds.
        // Holding the percentage prevents the alarming "ticking down 1% every second" optical illusion.
        bool inUnplugRelaxation = (!charging && unplugTime > 0 && (now - unplugTime < 45000));

        // Discharge Slew-Rate Limiter:
        // A 150-300mAh battery drawing ~15-25mA discharges at ~10% per hour (1% every 6 minutes).
        // Under heavy gaming (50mA), it discharges 1% every ~2.5 minutes.
        // We strictly limit the rate of displayed percentage drop to at most 1% every 25 seconds
        // (unless voltage is critically low < 3.45V, where immediate drop protects against sudden shutdown).
        if (!charging) {
            if (inUnplugRelaxation) {
                // Do not decrease during immediate post-unplug relaxation period
            } else if (effectiveV < 3.45f) {
                // Near critical cutoff: track raw percentage immediately
                displayedPercentage = rawPct;
                lastPctUpdateTime = now;
            } else if (rawPct < displayedPercentage) {
                // Enforce max drop rate: 1% per 25 seconds
                if (now - lastPctUpdateTime >= 25000) {
                    displayedPercentage--;
                    lastPctUpdateTime = now;
                }
            } else if (rawPct > displayedPercentage && (rawPct - displayedPercentage >= 3)) {
                // Only allow upward drift on battery if raw percentage is consistently higher (>= 3% hysteresis)
                if (now - lastPctUpdateTime >= 30000) {
                    displayedPercentage++;
                    lastPctUpdateTime = now;
                }
            }
        } else {
            // Charging mode: smoothly step up (at most 1% every 8 seconds)
            if (rawPct > displayedPercentage) {
                if (now - lastPctUpdateTime >= 8000) {
                    displayedPercentage++;
                    lastPctUpdateTime = now;
                }
            } else if (rawPct < displayedPercentage && (displayedPercentage - rawPct >= 5)) {
                // Charger load transient compensation
                displayedPercentage--;
                lastPctUpdateTime = now;
            }
        }

        return displayedPercentage;
    }
};
