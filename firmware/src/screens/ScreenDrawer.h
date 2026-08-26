#ifndef SCREEN_DRAWER_H
#define SCREEN_DRAWER_H

#include <stdint.h>
#include "screens/OledScreen.h"

class ScreenDrawer {
public:
    static void drawFullScreen(const uint8_t* data, uint8_t width, uint8_t height, int16_t x = 0, int16_t y = 0);
    static void drawCelebration(const CelebrationScreen* screen);
};

#endif // SCREEN_DRAWER_H
