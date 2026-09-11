#include "clicker/CounterRenderer.h"
#include "Display.h"
#include "clicker/SisyphusSprites.h"
#include <cctype>
#include <cstdio>
#include <cstring>

void CounterRenderer::formatDisplayCount(const char *countStr, char *buffer,
                                         size_t bufferSize) const {
  if (!countStr || !buffer || bufferSize == 0)
    return;

  size_t len = strlen(countStr);
  if (len <= 16) {
    snprintf(buffer, bufferSize, "%s", countStr);
  } else {
    snprintf(buffer, bufferSize, "%c.%.4se%u", countStr[0], countStr + 1,
             (unsigned)(len - 1));
  }
}

void CounterRenderer::drawBackground(float worldProgress,
                                     int32_t climbProgress) const {
  // 1. Simpler pixel-art clouds floating high across the sky
  int16_t c1x = (static_cast<int32_t>(worldProgress * 0.10f) + 15) % 150 - 20;
  display.drawBitmap(c1x, 3, simple_cloud1_bmp, CLOUD1_WIDTH, CLOUD1_HEIGHT,
                     SSD1306_WHITE);

  int16_t c2x = (static_cast<int32_t>(worldProgress * 0.12f) + 85) % 160 - 20;
  display.drawBitmap(c2x, 7, simple_cloud2_bmp, CLOUD2_WIDTH, CLOUD2_HEIGHT,
                     SSD1306_WHITE);

  // 2. Far-away distant mountain peaks in upper background (parallax 0.15x)
  int16_t mtnX =
      -(static_cast<int32_t>(worldProgress * 0.15f) % DISTANT_MTN_WIDTH);

  const int16_t bytesPerRow = (DISTANT_MTN_WIDTH + 7) / 8;
  for (int16_t rep = 0; rep < 3; rep++) {
    int16_t startX = mtnX + (rep * DISTANT_MTN_WIDTH);
    if (startX > 128 || startX + DISTANT_MTN_WIDTH <= 0)
      continue;

    for (int16_t my = 0; my < DISTANT_MTN_HEIGHT; my++) {
      int16_t py = 27 + my;
      uint16_t rowStart = my * bytesPerRow;

      for (int16_t byteCol = 0; byteCol < bytesPerRow; byteCol++) {
        uint8_t b = pgm_read_byte(distant_mountains_bmp + rowStart + byteCol);
        if (b == 0)
          continue; // Fast skip: 8 empty pixels in 1 CPU instruction

        int16_t baseMx = byteCol * 8;
        for (int16_t bit = 0; bit < 8; bit++) {
          if (b & (0x80 >> bit)) {
            int16_t px = startX + baseMx + bit;
            if (px >= 0 && px < 128 &&
                py < getSlopeGroundY(px, climbProgress)) {
              display.drawPixel(px, py, SSD1306_WHITE);
            }
          }
        }
      }
    }
  }
}

void CounterRenderer::drawHillTerrain(int32_t hillOffset,
                                      int32_t climbProgress) const {
  // 1. Starting flat ground plane on the left (x < 38):
  // Moves downwards as soon as movement starts (plain_y = 62 + climbProgress * 3)
  int16_t plain_y = 62 + climbProgress * 3;
  if (plain_y < 64) {
    for (int16_t px = 0; px < 38; px++) {
      display.drawPixel(px, plain_y, SSD1306_WHITE);
    }
  }

  // 2. Hill occupying ~30% in bottom-right corner (from x=38 to 127):
  // Features small ups & downs, rocky ledges, overall uphill from y=63 to y=37
  for (int16_t sx = 38; sx < 128; sx++) {
    int16_t gy = pgm_read_byte(&hill_corner_profile[sx]);

    // Solid crisp slope edge
    display.drawPixel(sx, gy, SSD1306_WHITE);

    // Subtle natural ridge accent
    if ((sx + hillOffset) % 8 == 0 && gy > 0) {
      display.drawPixel(sx, gy - 1, SSD1306_WHITE);
    }
  }

  // 3. Cliff edge on left side of hill (at x=37) when ground plane has moved
  // downwards below screen
  if (plain_y >= 64) {
    int16_t base_y = pgm_read_byte(&hill_corner_profile[38]);
    for (int16_t py = base_y; py < 64; py++) {
      display.drawPixel(37, py, SSD1306_WHITE);
    }
  }
}

void CounterRenderer::drawBoulder(int16_t bx, int16_t by,
                                  uint8_t rotFrame) const {
  const uint8_t *bmp = (const uint8_t *)pgm_read_ptr(
      &(boulder_frames[rotFrame % BOULDER_FRAMES]));

  // 1. Fill solid opaque interior circle so ground does not show through
  display.fillCircle(bx + 14, by + 14, 13, SSD1306_BLACK);

  // 2. Draw crisp 1-bit boulder outline and rotational surface cracks
  display.drawBitmap(bx, by, bmp, BOULDER_WIDTH, BOULDER_HEIGHT, SSD1306_WHITE);
}

void CounterRenderer::drawSisyphus(int16_t sx, int16_t sy,
                                   const uint8_t *spriteBmp, uint8_t width,
                                   uint8_t height) const {
  if (!spriteBmp)
    return;
  display.drawBitmap(sx, sy, spriteBmp, width, height, SSD1306_WHITE);
}

void CounterRenderer::drawCounterText(const char *countStr) const {
  display.setFont(NULL); // Reset to default 5x7 GLCD font
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(2, 2);
  char buf[32];
  formatDisplayCount(countStr, buf, sizeof(buf));
  display.print(buf);
}

void CounterRenderer::renderScene(const char *countStr,
                                  const uint8_t *sisyphusSprite,
                                  uint8_t sisyphusWidth, uint8_t sisyphusHeight,
                                  int16_t charX, int16_t bldX,
                                  uint8_t boulderRotFrame, int32_t hillOffset,
                                  float worldProgress, int32_t climbProgress) {
  display.clearDisplay();

  // Layer 1: Subtle background (simpler clouds & distant mountain peaks
  // occluded by hill)
  drawBackground(worldProgress, climbProgress);

  // Layer 2: Bottom-right corner hill with ridges, downward moving starting
  // plane, and items
  drawHillTerrain(hillOffset, climbProgress);

  // Position Boulder along organic ground:
  int16_t bldGroundY = getSlopeGroundY(bldX + 14, climbProgress);
  int16_t by = bldGroundY - 28;

  // Position Sisyphus along organic ground:
  int16_t sisGroundY = getSlopeGroundY(charX + 10, climbProgress);
  int16_t sy = sisGroundY - sisyphusHeight + 1;

  // Layer 3: Sisyphus character (bracing boulder with one hand when resting,
  // heaving when pushing)
  drawSisyphus(charX, sy, sisyphusSprite, sisyphusWidth, sisyphusHeight);

  // Layer 4: Boulder (solid circular stone)
  drawBoulder(bldX, by, boulderRotFrame);

  // Layer 5: Counter text in upper-left corner
  drawCounterText(countStr);
}
