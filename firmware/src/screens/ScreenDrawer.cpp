#include "screens/ScreenDrawer.h"
#include "Display.h"

void ScreenDrawer::drawFullScreen(const uint8_t* data, uint8_t width, uint8_t height, int16_t x, int16_t y) {
    if (data == nullptr) {
        return;
    }
    display.drawBitmap(x, y, data, width, height, SSD1306_WHITE);
}

void ScreenDrawer::drawCelebration(const CelebrationScreen* screen) {
    if (screen == nullptr || screen->data == nullptr) {
        return;
    }

    display.clearDisplay();
    drawFullScreen(screen->data, screen->width, screen->height, 0, 0);
    display.display();
}
