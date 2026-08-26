#include "HomeApplet.h"
#include <Arduino.h>
#include <cstring>
#include "Display.h"

void HomeApplet::applyUnlockState(uint32_t flags) {
    unlockFlags = flags;
}

void HomeApplet::initGround() {
    memset(snowDepth, 0, sizeof(snowDepth));
}

void HomeApplet::respawnFlake(Snowflake& flake) {
    flake.x = static_cast<float>(random(0, SCREEN_W));
    flake.y = static_cast<float>(random(-12, -1));
    flake.vy = static_cast<float>(random(80, 180)) / 100.0f;
    flake.vx = static_cast<float>(random(-25, 25)) / 100.0f;
}

void HomeApplet::initSnowflakes() {
    for (uint8_t i = 0; i < FLAKE_COUNT; i++) {
        respawnFlake(flakes[i]);
        flakes[i].y = static_cast<float>(random(-12, 50));
    }
}

int16_t HomeApplet::surfaceY(int16_t x) const {
    if (x < 0) {
        x = 0;
    } else if (x >= SCREEN_W) {
        x = SCREEN_W - 1;
    }
    uint8_t pile = snowDepth[x];
    if (pile > MAX_SNOW_HEIGHT) {
        pile = MAX_SNOW_HEIGHT;
    }
    return static_cast<int16_t>(63 - pile);
}

void HomeApplet::depositSnow(int16_t x) {
    if (x < 0 || x >= SCREEN_W) {
        return;
    }
    if (snowDepth[x] < MAX_SNOW_HEIGHT) {
        snowDepth[x]++;
    }
}

void HomeApplet::updateSnowflakes() {
    const float speedBoost = (unlockFlags & 0x08) ? 1.25f : 1.0f;

    for (uint8_t i = 0; i < FLAKE_COUNT; i++) {
        Snowflake& f = flakes[i];
        f.x += f.vx;
        f.y += f.vy * speedBoost;

        if (f.x < 0.0f) {
            f.x += SCREEN_W;
        } else if (f.x >= SCREEN_W) {
            f.x -= SCREEN_W;
        }

        const int16_t ix = static_cast<int16_t>(f.x);
        if (f.y >= static_cast<float>(surfaceY(ix))) {
            depositSnow(ix);
            respawnFlake(f);
        }
    }
}

void HomeApplet::drawGround() const {
    for (uint8_t x = 0; x < SCREEN_W; x++) {
        uint8_t pile = snowDepth[x];
        if (pile > 0) {
            display.drawFastVLine(x, static_cast<int16_t>(64 - pile), pile, SSD1306_WHITE);
        }
    }
}

void HomeApplet::drawSnowflakes() const {
    for (uint8_t i = 0; i < FLAKE_COUNT; i++) {
        const Snowflake& f = flakes[i];
        if (f.y >= 0.0f && f.y <= 63.0f) {
            display.drawPixel(static_cast<int16_t>(f.x), static_cast<int16_t>(f.y), SSD1306_WHITE);
        }
    }
}

void HomeApplet::drawPatternSymbol(int16_t cx, int16_t cy, uint8_t scale) const {
    const int8_t points[][2] = {{0, -3}, {3, 0}, {0, 3}, {-3, 0}};
    for (uint8_t i = 0; i < 4; i++) {
        int16_t x1 = cx + points[i][0] * scale;
        int16_t y1 = cy + points[i][1] * scale;
        int16_t x2 = cx + points[(i + 1) % 4][0] * scale;
        int16_t y2 = cy + points[(i + 1) % 4][1] * scale;
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
    display.fillRect(cx - 1, cy - 1, 3, 3, SSD1306_WHITE);
}

void HomeApplet::drawUnlockElements() {
    if (unlockFlags & 0x01) {
        display.drawPixel(4, 4, SSD1306_WHITE);
    }
    if (unlockFlags & 0x02) {
        display.drawPixel(123, 4, SSD1306_WHITE);
        display.drawPixel(120, 8, SSD1306_WHITE);
    }
    if (unlockFlags & 0x04) {
        drawPatternSymbol(118, 52, 1);
    }
    if (unlockFlags & 0x10) {
        drawPatternSymbol(10, 52, 2);
    }
    if (unlockFlags & 0x20) {
        int16_t scanY = static_cast<int16_t>((millis() / 40) % 64);
        display.drawFastHLine(0, scanY, SCREEN_W, SSD1306_WHITE);
    }
    if (unlockFlags & 0x40) {
        uint8_t pulse = static_cast<uint8_t>((millis() / 200) % 4);
        display.drawPixel(64 + pulse, 62 - pulse, SSD1306_WHITE);
        display.drawPixel(63 - pulse, 61, SSD1306_WHITE);
    }
}

void HomeApplet::init() {
    initGround();
    initSnowflakes();
}

void HomeApplet::update() {
    updateSnowflakes();
}

void HomeApplet::draw() {
    display.clearDisplay();
    drawGround();
    drawSnowflakes();
    drawUnlockElements();
    display.display();
}
