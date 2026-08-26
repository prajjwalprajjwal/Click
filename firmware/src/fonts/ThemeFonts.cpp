#include "fonts/ThemeFonts.h"
#include "Display.h"

void ThemeFonts::set(const GFXfont* font) {
    display.setFont(font);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
}

void ThemeFonts::reset() {
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
}

void ThemeFonts::measure(const GFXfont* font, const char* text, int16_t& x1, int16_t& y1, uint16_t& w, uint16_t& h) {
    if (font != nullptr) {
        set(font);
    } else {
        reset();
    }
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
}

void ThemeFonts::drawCentered(const GFXfont* font, const char* text, int16_t centerY) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    measure(font, text, x1, y1, w, h);

    const int16_t x = static_cast<int16_t>((128 - static_cast<int16_t>(w)) / 2 - x1);
    const int16_t y = static_cast<int16_t>(centerY - (static_cast<int16_t>(h) / 2) - y1);
    display.setCursor(x, y);
    display.print(text);
}

void ThemeFonts::drawCenteredBestFit(const GFXfont* primaryFont, const GFXfont* fallbackFont, const char* text,
                                     int16_t centerY, uint16_t maxWidth) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    measure(primaryFont, text, x1, y1, w, h);
    if (w <= maxWidth) {
        const int16_t x = static_cast<int16_t>((128 - static_cast<int16_t>(w)) / 2 - x1);
        const int16_t y = static_cast<int16_t>(centerY - (static_cast<int16_t>(h) / 2) - y1);
        display.setCursor(x, y);
        display.print(text);
        return;
    }
    drawCentered(fallbackFont, text, centerY);
}
