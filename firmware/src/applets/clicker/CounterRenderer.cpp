#include "CounterRenderer.h"
#include "Display.h"
#include "SisyphusSprites.h"
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
                                     int32_t climbProgress,
                                     int16_t charX) const {
  // 1. Thin, irregular wispy cloud streaks drifting very slowly across the sky
  // Drifts naturally at a tranquil, slow pace (~1.8 px/sec) and advances subtly with clicks
  uint32_t cloudTravel =
      (millis() / 550) + static_cast<uint32_t>(worldProgress * 0.08f);

  const int16_t cloudLoop = 220; // 220px loop guarantees strictly 1 cloud on screen at a time
  int16_t cyclePos = cloudTravel % cloudLoop;
  uint32_t cycle = cloudTravel / cloudLoop;

  int16_t cx = 135 - cyclePos;
  if (cx > -40 && cx < 128) {
    uint8_t shape = cycle % 3;
    int16_t cy = 6 + ((cycle * 3) % 6);
    const uint8_t *bmp = (shape == 0) ? cloud_wisp_1_bmp
                                       : ((shape == 1) ? cloud_wisp_2_bmp
                                                       : cloud_wisp_3_bmp);
    uint8_t w = (shape == 0) ? CLOUD_WISP_1_WIDTH
                             : ((shape == 1) ? CLOUD_WISP_2_WIDTH
                                             : CLOUD_WISP_3_WIDTH);
    uint8_t h = (shape == 0) ? CLOUD_WISP_1_HEIGHT
                             : ((shape == 1) ? CLOUD_WISP_2_HEIGHT
                                             : CLOUD_WISP_3_HEIGHT);
    display.drawBitmap(cx, cy, bmp, w, h, SSD1306_WHITE);
  }

  // 2. Far-away distant mountain peaks in upper background (slowed parallax: 1px per 4 clicks)
  int16_t mtnOffset =
      (static_cast<int32_t>(worldProgress) / 4) % DISTANT_MTN_WIDTH;
  if (mtnOffset < 0) {
    mtnOffset += DISTANT_MTN_WIDTH;
  }
  int16_t mtnX = -mtnOffset;

  // Mountain extends naturally right up to the back of the character without excessive blank space
  int16_t mtnLimit = charX;

  const int16_t bytesPerRow = (DISTANT_MTN_WIDTH + 7) / 8;
  for (int16_t rep = 0; rep < 2; rep++) {
    int16_t startX = mtnX + (rep * DISTANT_MTN_WIDTH);
    if (startX > mtnLimit || startX + DISTANT_MTN_WIDTH <= 0)
      continue;

    for (int16_t my = 0; my < DISTANT_MTN_HEIGHT; my++) {
      int16_t py = 46 + my;
      uint16_t rowStart = my * bytesPerRow;

      for (int16_t byteCol = 0; byteCol < bytesPerRow; byteCol++) {
        uint8_t b = pgm_read_byte(distant_mountains_bmp + rowStart + byteCol);
        if (b == 0)
          continue; // Fast skip: 8 empty pixels in 1 CPU instruction

        int16_t baseMx = byteCol * 8;
        for (int16_t bit = 0; bit < 8; bit++) {
          if (b & (0x80 >> bit)) {
            int16_t px = startX + baseMx + bit;
            // Draw mountain up to the back of the character, avoiding visual clutter
            if (px >= 0 && px < mtnLimit &&
                py < getSlopeGroundY(px, climbProgress)) {
              display.drawPixel(px, py, SSD1306_WHITE);
            }
          }
        }
      }
    }
  }
}

namespace {
enum class TerrainItemType : uint8_t {
  ROCK_SM,
  ROCK_ROUND,
  STONE_CLUSTER,
  BUSH_SMALL,
  BUSH_MED,
  TREE_PINE,
  FLOWER,
  MARKER_POST,
  GRASS,
};

struct TerrainItem {
  TerrainItemType type;
  int16_t worldX;
};

static const TerrainItem terrainItems[] = {
    {TerrainItemType::TREE_PINE, 48},
    {TerrainItemType::ROCK_SM, 65},
    {TerrainItemType::BUSH_SMALL, 82},
    {TerrainItemType::STONE_CLUSTER, 100},
    {TerrainItemType::FLOWER, 116},
    {TerrainItemType::BUSH_MED, 132},
    {TerrainItemType::TREE_PINE, 150},
    {TerrainItemType::MARKER_POST, 168},
    {TerrainItemType::ROCK_ROUND, 186},
    {TerrainItemType::GRASS, 204},
    {TerrainItemType::STONE_CLUSTER, 222},
    {TerrainItemType::BUSH_SMALL, 240},
};

static inline uint8_t soilHash(uint32_t wx, uint32_t wy) {
  uint32_t h = (wx * 1664525U + wy * 1013904223U + 22695477U);
  h = ((h ^ (h >> 13)) * 1274126177U);
  return (uint8_t)((h ^ (h >> 16)) & 0xFF);
}
} // anonymous namespace

void CounterRenderer::drawHillTerrain(int32_t hillOffset,
                                      int32_t climbProgress) const {
  // 1. Starting flat ground plane on the left (x < 38):
  // Moves downwards as soon as movement starts (plain_y = 62 + climbProgress * 3)
  int16_t plain_y = 62 + static_cast<int16_t>(climbProgress) * 3;
  if (plain_y < 64) {
    for (int16_t px = 0; px < 38; px++) {
      display.drawPixel(px, plain_y, SSD1306_WHITE);
      for (int16_t py = plain_y + 1; py < 64; py++) {
        if (soilHash(px, py) < 18) {
          display.drawPixel(px, py, SSD1306_WHITE);
        }
      }
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

    // Subterranean organic soil specks & natural granular earth
    uint32_t wx = sx + hillOffset;
    for (int16_t py = gy + 1; py < 64; py++) {
      int16_t depth = py - gy;
      uint8_t h = soilHash(wx, py);
      uint8_t thresh = (depth < 6) ? 20 : ((depth < 14) ? 12 : 6);
      if (h < thresh) {
        display.drawPixel(sx, py, SSD1306_WHITE);
      }
    }
  }

  // 3. Cliff edge on left side of hill (at x=37) when ground plane has moved
  // downwards below screen
  if (plain_y >= 64) {
    int16_t base_y = pgm_read_byte(&hill_corner_profile[38]);
    for (int16_t py = base_y; py < 64; py++) {
      if (py % 3 != 0) {
        display.drawPixel(37, py, SSD1306_WHITE);
      }
    }
  }

  // 4. Actual terrain features: small trees, bushes, stones, stone clusters, flowers, grass
  const int16_t loopSpan = 210;
  for (const auto &item : terrainItems) {
    int16_t sx = 38 + ((item.worldX - 38 - hillOffset) % loopSpan);
    if (sx < 38)
      sx += loopSpan;

    if (sx >= 38 && sx < 124) {
      const uint8_t *bmp = nullptr;
      uint8_t w = 0, h = 0;
      switch (item.type) {
      case TerrainItemType::ROCK_SM:
        bmp = rock_sm_bmp;
        w = ROCK_SM_WIDTH;
        h = ROCK_SM_HEIGHT;
        break;
      case TerrainItemType::ROCK_ROUND:
        bmp = rock_round_bmp;
        w = ROCK_ROUND_WIDTH;
        h = ROCK_ROUND_HEIGHT;
        break;
      case TerrainItemType::STONE_CLUSTER:
        bmp = stone_cluster_bmp;
        w = STONE_CLUSTER_WIDTH;
        h = STONE_CLUSTER_HEIGHT;
        break;
      case TerrainItemType::BUSH_SMALL:
        bmp = bush_small_bmp;
        w = BUSH_SMALL_WIDTH;
        h = BUSH_SMALL_HEIGHT;
        break;
      case TerrainItemType::BUSH_MED:
        bmp = bush_med_bmp;
        w = BUSH_MED_WIDTH;
        h = BUSH_MED_HEIGHT;
        break;
      case TerrainItemType::TREE_PINE:
        bmp = tree_pine_bmp;
        w = TREE_PINE_WIDTH;
        h = TREE_PINE_HEIGHT;
        break;
      case TerrainItemType::FLOWER:
        bmp = flower_bmp;
        w = FLOWER_WIDTH;
        h = FLOWER_HEIGHT;
        break;
      case TerrainItemType::MARKER_POST:
        bmp = marker_post_bmp;
        w = MARKER_POST_WIDTH;
        h = MARKER_POST_HEIGHT;
        break;
      case TerrainItemType::GRASS:
        bmp = terrain_grass_bmp;
        w = TERRAIN_GRASS_WIDTH;
        h = TERRAIN_GRASS_HEIGHT;
        break;
      }
      if (bmp) {
        int16_t midX = sx + (w / 2);
        if (midX < 38)
          midX = 38;
        if (midX > 127)
          midX = 127;
        int16_t gy = pgm_read_byte(&hill_corner_profile[midX]);
        int16_t py = gy - h + 1;
        display.drawBitmap(sx, py, bmp, w, h, SSD1306_WHITE);
      }
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
  drawBackground(worldProgress, climbProgress, charX);

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
