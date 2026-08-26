#include "screens/OledScreen.h"
#include <stddef.h>
#include "screens/generated/registry.inc"

const CelebrationScreen* findCelebrationScreen(uint64_t lifetimeClicks) {
    for (size_t i = 0; i < CELEBRATION_SCREEN_COUNT; i++) {
        if (CELEBRATION_SCREENS[i].atClicks == lifetimeClicks) {
            return &CELEBRATION_SCREENS[i];
        }
    }
    return nullptr;
}
