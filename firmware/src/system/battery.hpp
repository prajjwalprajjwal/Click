#pragma once
#include <Arduino.h>

#define BATTERY_ADC_PIN 35
#define CHARGER_STAT_PIN 33

class BatteryManager {
private:
    static inline float filteredVoltage = 0.0f;
    static inline int lastDisplayedPct = -1;

public:
    static void init() {
        pinMode(CHARGER_STAT_PIN, INPUT_PULLUP);
        analogReadResolution(12);
        
        // Prime Exponential Moving Average filter with initial oversamples
        uint32_t rawSum = 0;
        for (int i = 0; i < 32; i++) {
            rawSum += analogRead(BATTERY_ADC_PIN);
            delay(1);
        }
        float raw = rawSum / 32.0f;
        filteredVoltage = (raw / 4095.0f) * 2.0f * 3.3f * 1.05f; // 100k/100k divider ratio + calibration
    }

    static bool isCharging() {
        // ETA IC STAT pin actively pulls LOW during active charge cycle
        return digitalRead(CHARGER_STAT_PIN) == LOW;
    }

    static float getSmoothVoltage() {
        uint32_t rawSum = 0;
        for (int i = 0; i < 32; i++) {
            rawSum += analogRead(BATTERY_ADC_PIN);
            delay(1);
        }
        float currentRaw = rawSum / 32.0f;
        float instantaneousVoltage = (currentRaw / 4095.0f) * 2.0f * 3.3f * 1.05f;

        // Exponential Moving Average (EMA) filter: 92% previous + 8% new
        if (filteredVoltage <= 0.01f) {
            filteredVoltage = instantaneousVoltage;
        } else {
            filteredVoltage = (filteredVoltage * 0.92f) + (instantaneousVoltage * 0.08f);
        }
        return filteredVoltage;
    }

    static float getVoltage() {
        return getSmoothVoltage();
    }

    static int getPercentage() {
        float v = getSmoothVoltage();
        int pct = 0;

        // Piece-wise Non-Linear LiPo Discharge Curve Mapping
        if (v >= 4.15f) pct = 100;
        else if (v >= 4.05f) pct = 90 + static_cast<int>((v - 4.05f) / (4.15f - 4.05f) * 10.0f);
        else if (v >= 3.90f) pct = 70 + static_cast<int>((v - 3.90f) / (4.05f - 3.90f) * 20.0f);
        else if (v >= 3.80f) pct = 50 + static_cast<int>((v - 3.80f) / (3.90f - 3.80f) * 20.0f);
        else if (v >= 3.70f) pct = 30 + static_cast<int>((v - 3.70f) / (3.80f - 3.70f) * 20.0f);
        else if (v >= 3.50f) pct = 10 + static_cast<int>((v - 3.50f) / (3.70f - 3.50f) * 20.0f);
        else if (v >= 3.30f) pct = static_cast<int>((v - 3.30f) / (3.50f - 3.30f) * 10.0f);
        else pct = 0;

        int currentSteppedPct = (pct / 5) * 5; // Quantize to 5% steps

        // Apply hysteresis lock: Only shift displayed percentage if delta >= 5%
        if (lastDisplayedPct == -1 || abs(currentSteppedPct - lastDisplayedPct) >= 5) {
            lastDisplayedPct = currentSteppedPct;
        }

        return lastDisplayedPct;
    }
};
