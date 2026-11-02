#include "CustomApplet.h"

void CustomApplet::init() {
    score = 0;
    lastFrameMs = millis();
    
    // Restore persistent high score
    prefs.begin("custom_app", false);
    highScore = prefs.getUInt("high_score", 0);
    prefs.end();

    // Welcome LED pulse & click sound
    WS2812Driver::flash(0, 200, 255, 150);
    SoundFX::playClick();
}

void CustomApplet::update() {
    unsigned long now = millis();
    if (now - lastFrameMs < 30) {
        return; // ~33 FPS frame pacing
    }
    lastFrameMs = now;

    // Non-blocking physics / game state updates here
}

void CustomApplet::draw() {
    display.clearDisplay();

    // Title
    ThemeFonts::set(&Rajdhani12pt7b);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 14);
    display.print("MICRO APPLET");

    // Score display
    char scoreBuf[32];
    snprintf(scoreBuf, sizeof(scoreBuf), "%lu", (unsigned long)score);
    ThemeFonts::drawCentered(&Rajdhani24pt7b, scoreBuf, 40);

    // Footer hint
    display.setFont(nullptr); // default 5x7 font
    display.setTextSize(1);
    display.setCursor(18, 55);
    display.print("[ACTION] TO PLAY");
}

void CustomApplet::onActionClick() {
    score++;
    SoundFX::playClick();
    WS2812Driver::flash(50, 255, 50, 50);

    if (score > highScore) {
        highScore = score;
        prefs.begin("custom_app", false);
        prefs.putUInt("high_score", highScore);
        prefs.end();
    }
}

void CustomApplet::onActionHold() {
    // Reset or secondary action
    score = 0;
    SoundFX::playTone(440, 100);
}

void CustomApplet::onModeClick() {
    // Mode toggle
}

void CustomApplet::onModeHold() {
}

void CustomApplet::onBothHeld() {
}

void CustomApplet::cleanup() {
    WS2812Driver::clear();
}

void CustomApplet::onPrepareSleep() {
    WS2812Driver::clear();
}
