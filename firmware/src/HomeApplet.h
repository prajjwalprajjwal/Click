#ifndef HOME_APPLET_H
#define HOME_APPLET_H

#include <stdint.h>
#include "Applet.h"

class HomeApplet : public Applet {
private:
    static const uint8_t FLAKE_COUNT = 48;
    static const uint8_t SCREEN_W = 128;
    static const uint8_t MAX_SNOW_HEIGHT = 4;

    struct Snowflake {
        float x;
        float y;
        float vx;
        float vy;
    };

    Snowflake flakes[FLAKE_COUNT];
    uint8_t snowDepth[SCREEN_W] = {0};
    uint32_t unlockFlags = 0;

    void initGround();
    void initSnowflakes();
    void respawnFlake(Snowflake& flake);
    int16_t surfaceY(int16_t x) const;
    void depositSnow(int16_t x);
    void updateSnowflakes();
    void drawGround() const;
    void drawSnowflakes() const;
    void drawUnlockElements();
    void drawPatternSymbol(int16_t cx, int16_t cy, uint8_t scale) const;

public:
    void init() override;
    void update() override;
    void draw() override;

    void applyUnlockState(uint32_t unlockFlags);
};

#endif // HOME_APPLET_H
