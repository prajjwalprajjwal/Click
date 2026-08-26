// Auto-generated — do not edit. Run tools/png_to_oled.py
#ifndef SCREEN_REGISTRY_INC
#define SCREEN_REGISTRY_INC

#include <stddef.h>
#include "screens/OledScreen.h"

#include "screens/generated/screen_10.h"

static const CelebrationScreen CELEBRATION_SCREENS[] = {
    { 10ULL, SCREEN_10_DATA, SCREEN_10_WIDTH, SCREEN_10_HEIGHT, "SCREEN_10" },
};

static const size_t CELEBRATION_SCREEN_COUNT = sizeof(CELEBRATION_SCREENS) / sizeof(CELEBRATION_SCREENS[0]);

#endif // SCREEN_REGISTRY_INC
