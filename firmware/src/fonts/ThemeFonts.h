#ifndef THEME_FONTS_H
#define THEME_FONTS_H

#include <Adafruit_GFX.h>
#include <stdint.h>

#include "fonts/generated/rajdhani12pt7b.h"
#include "fonts/generated/rajdhani18pt7b.h"
#include "fonts/generated/rajdhani24pt7b.h"
#include "fonts/generated/rajdhani32pt7b.h"
#include "fonts/generated/rajdhani40pt7b.h"

class ThemeFonts {
public:
    static void set(const GFXfont* font);
    static void reset();

    static void measure(const GFXfont* font, const char* text, int16_t& x1, int16_t& y1, uint16_t& w, uint16_t& h);
    static void drawCentered(const GFXfont* font, const char* text, int16_t centerY);
    static void drawCenteredBestFit(const GFXfont* primaryFont, const GFXfont* fallbackFont, const char* text,
                                    int16_t centerY, uint16_t maxWidth = 124);
};

#endif // THEME_FONTS_H
