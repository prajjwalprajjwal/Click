#ifndef OLED_SCREEN_H
#define OLED_SCREEN_H

#include <stdint.h>

struct CelebrationScreen {
    uint64_t atClicks;
    const uint8_t* data;
    uint8_t width;
    uint8_t height;
    const char* name;
};

const CelebrationScreen* findCelebrationScreen(uint64_t lifetimeClicks);

#endif // OLED_SCREEN_H
