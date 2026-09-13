#include "TimingGameApplet.h"
#include <Arduino.h>
#include <cmath>
#include "InputManager.h"
#include "Display.h"
#include "fonts/ThemeFonts.h"

void TimingGameApplet::preloadState() {
    prefs.begin("click_stats", true);
    bestDeviationMs = prefs.getUInt("just_ten_best", 0);
    prefs.end();
}

void TimingGameApplet::init() {
    state = IDLE;
    wasPressing = false;
    holdStartUs = 0;
    holdDurationUs = 0;
    resultDisplayTime = 0;
    preloadState();
}

bool TimingGameApplet::isActionPressed() const {
    return digitalRead(ACTION_BUTTON_PIN) == LOW;
}

void TimingGameApplet::formatSeconds4(uint64_t micros, char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "%.4fs", static_cast<double>(micros) / 1000000.0);
}

void TimingGameApplet::formatDeviation4(int64_t diffMicros, char* buffer, size_t bufferSize) {
    const double diff = static_cast<double>(diffMicros) / 1000000.0;
    if (diff >= 0.0) {
        snprintf(buffer, bufferSize, "+%.4fs", diff);
    } else {
        snprintf(buffer, bufferSize, "-%.4fs", -diff);
    }
}

void TimingGameApplet::drawIdle() const {
#ifdef GENERATED_TIMINGGAMEAPPLET_START_H
    // RENDER USER CUSTOM START SCREEN (128x64)
    display.drawBitmap(0, 0, TimingGameApplet_start_bmp,
                       TIMINGGAMEAPPLET_START_WIDTH, TIMINGGAMEAPPLET_START_HEIGHT,
                       SSD1306_WHITE);
#else
    // Fallback: Rajdhani text layout
    ThemeFonts::drawCentered(&Rajdhani24pt7b, "JUST TEN", 16);
    ThemeFonts::drawCentered(&Rajdhani12pt7b, "Hold the button", 44);
    ThemeFonts::drawCentered(&Rajdhani12pt7b, "for 10 seconds", 56);
#endif
}

void TimingGameApplet::drawCounting(uint32_t nowMs) const {
    // Slower, smooth breathing animation (~2.8s per cycle)
    const float breath = 0.5f + 0.5f * sinf(static_cast<float>(nowMs) * 0.0022f);
    const int16_t outerRadius = static_cast<int16_t>(7.0f + breath * 15.0f);
    const int16_t innerRadius = static_cast<int16_t>(outerRadius / 2);

    display.drawCircle(64, 32, outerRadius, SSD1306_WHITE);
    if (innerRadius > 1) {
        display.drawCircle(64, 32, innerRadius, SSD1306_WHITE);
    }
    display.drawPixel(64, 32, SSD1306_WHITE);
}

void TimingGameApplet::drawResult() const {
    char line[24];
    formatSeconds4(holdDurationUs, line, sizeof(line));

    const int64_t targetUs = 10000000LL;
    char devLine[24];
    formatDeviation4(static_cast<int64_t>(holdDurationUs) - targetUs, devLine, sizeof(devLine));

#ifdef GENERATED_TIMINGGAMEAPPLET_END_H
    // RENDER USER CUSTOM END SCREEN (128x64)
    display.drawBitmap(0, 0, TimingGameApplet_end_bmp,
                       TIMINGGAMEAPPLET_END_WIDTH, TIMINGGAMEAPPLET_END_HEIGHT,
                       SSD1306_WHITE);

    // Overlay dynamic score values on top of custom design
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(26, 28);
    display.print(line);
    display.setCursor(26, 40);
    display.print(devLine);
#else
    // Fallback: Rajdhani text layout
    ThemeFonts::drawCenteredBestFit(&Rajdhani32pt7b, &Rajdhani24pt7b, line, 20, 124);
    ThemeFonts::drawCenteredBestFit(&Rajdhani18pt7b, &Rajdhani12pt7b, devLine, 52, 124);
#endif
}

void TimingGameApplet::update() {
    const uint32_t now = millis();
    const bool pressing = isActionPressed();

    if (state == IDLE && pressing && !wasPressing) {
        state = COUNTING;
        holdStartUs = micros();
    } else if (state == COUNTING && wasPressing && !pressing) {
        holdDurationUs = micros() - holdStartUs;
        state = RESULT;
        resultDisplayTime = now;

        int64_t diffUs = static_cast<int64_t>(holdDurationUs) - 10000000LL;
        uint32_t devMs = static_cast<uint32_t>(std::abs(diffUs) / 1000);
        if (devMs == 0) devMs = 1; // 1ms for perfect hit to ensure it is > 0 in SQL
        if (bestDeviationMs == 0 || devMs < bestDeviationMs) {
            bestDeviationMs = devMs;
            prefs.begin("click_stats", false);
            prefs.putUInt("just_ten_best", bestDeviationMs);
            prefs.end();
        }
    }

    wasPressing = pressing;

    if (state == RESULT && (now - resultDisplayTime) > 3500) {
        state = IDLE;
    }
}

void TimingGameApplet::draw() {
    display.clearDisplay();

    switch (state) {
        case IDLE:
            drawIdle();
            break;
        case COUNTING:
            drawCounting(millis());
            break;
        case RESULT:
            drawResult();
            break;
    }

    display.display();
}

void TimingGameApplet::cleanup() {
    state = IDLE;
    wasPressing = false;
}

void TimingGameApplet::onActionClick() {
    if (state == RESULT) {
        state = IDLE;
    }
}

void TimingGameApplet::onBothHeld() {
    state = IDLE;
    wasPressing = false;
}
