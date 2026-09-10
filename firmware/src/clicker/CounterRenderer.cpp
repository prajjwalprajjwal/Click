#include "clicker/CounterRenderer.h"
#include "clicker/SisyphusSprites.h"
#include "Display.h"
#include <cstdio>
#include <cstring>
#include <cctype>

void CounterRenderer::formatDisplayCount(const char* countStr, char* buffer, size_t bufferSize) const {
    if (!countStr || !buffer || bufferSize == 0) return;

    size_t len = strlen(countStr);
    if (len <= 16) {
        snprintf(buffer, bufferSize, "%s", countStr);
    } else {
        snprintf(buffer, bufferSize, "%c.%.4se%u", countStr[0], countStr + 1, (unsigned)(len - 1));
    }
}

void CounterRenderer::drawBackground(float worldProgress, int16_t climbProgress) const {
    // 1. Simpler pixel-art clouds floating high across the sky
    int16_t c1x = (static_cast<int32_t>(worldProgress * 0.10f) + 15) % 150 - 20;
    display.drawBitmap(c1x, 3, simple_cloud1_bmp, CLOUD1_WIDTH, CLOUD1_HEIGHT, SSD1306_WHITE);

    int16_t c2x = (static_cast<int32_t>(worldProgress * 0.12f) + 85) % 160 - 20;
    display.drawBitmap(c2x, 7, simple_cloud2_bmp, CLOUD2_WIDTH, CLOUD2_HEIGHT, SSD1306_WHITE);

    // 2. Far-away distant mountain peaks in upper background (parallax 0.15x)
    int16_t mtnX = - (static_cast<int32_t>(worldProgress * 0.15f) % DISTANT_MTN_WIDTH);

    const int16_t bytesPerRow = (DISTANT_MTN_WIDTH + 7) / 8;
    for (int16_t rep = 0; rep < 3; rep++) {
        int16_t startX = mtnX + (rep * DISTANT_MTN_WIDTH);
        if (startX > 128 || startX + DISTANT_MTN_WIDTH <= 0) continue;

        for (int16_t my = 0; my < DISTANT_MTN_HEIGHT; my++) {
            int16_t py = 18 + my;
            uint16_t rowStart = my * bytesPerRow;

            for (int16_t byteCol = 0; byteCol < bytesPerRow; byteCol++) {
                uint8_t b = pgm_read_byte(distant_mountains_bmp + rowStart + byteCol);
                if (b == 0) continue; // Fast skip: 8 empty pixels in 1 CPU instruction

                int16_t baseMx = byteCol * 8;
                for (int16_t bit = 0; bit < 8; bit++) {
                    if (b & (0x80 >> bit)) {
                        int16_t px = startX + baseMx + bit;
                        if (px >= 0 && px < 128 && py < getSlopeGroundY(px, climbProgress)) {
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
    BUSH_SMALL,
    BUSH_MED,
    FLOWER,
    MARKER_POST,
    GRASS,
    MOUND
};

struct TerrainItem {
    TerrainItemType type;
    int16_t worldX;
};

static const TerrainItem terrainItems[] = {
    {TerrainItemType::FLOWER,       52},
    {TerrainItemType::ROCK_SM,      66},
    {TerrainItemType::BUSH_SMALL,   82},
    {TerrainItemType::MARKER_POST,  98},
    {TerrainItemType::GRASS,        114},
    {TerrainItemType::ROCK_ROUND,   130},
    {TerrainItemType::BUSH_MED,     150},
    {TerrainItemType::FLOWER,       170},
    {TerrainItemType::MOUND,        195}
};

static inline uint8_t soilHash(uint32_t wx, uint32_t wy) {
    uint32_t h = (wx * 1664525U + wy * 1013904223U + 22695477U);
    h = ((h ^ (h >> 13)) * 1274126177U);
    return (uint8_t)((h ^ (h >> 16)) & 0xFF);
}
} // anonymous namespace

void CounterRenderer::drawHillTerrain(int16_t hillOffset, int16_t climbProgress) const {
    // 1. Starting flat ground plane on the left (x < 38):
    // Moves downwards as soon as movement starts (plain_y = 62 + climbProgress * 3)
    int16_t plain_y = 62 + climbProgress * 3;
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

        // Slope edge
        display.drawPixel(sx, gy, SSD1306_WHITE);

        // Bumpy micro-steps on ridges
        if ((sx + hillOffset) % 6 == 0 && gy > 0) {
            display.drawPixel(sx, gy - 1, SSD1306_WHITE);
        }

        // Subterranean soil specks & natural rock flecks (deterministic pseudo-random hash, NO periodic ruler lines!)
        for (int16_t py = gy + 1; py < 64; py++) {
            int16_t depth = py - gy;
            uint32_t wx = sx + hillOffset;
            uint8_t h = soilHash(wx, py);
            uint8_t thresh = (depth < 6) ? 24 : ((depth < 14) ? 15 : 9);
            if (h < thresh) {
                display.drawPixel(sx, py, SSD1306_WHITE);
            } else if (h > 250 && depth >= 4 && depth <= 18 && sx + 1 < 128) {
                display.drawPixel(sx, py, SSD1306_WHITE);
                display.drawPixel(sx + 1, py, SSD1306_WHITE);
            }
        }
    }

    // 3. Cliff edge on left side of hill (at x=37) when ground plane has moved downwards below screen
    if (plain_y >= 64) {
        int16_t base_y = pgm_read_byte(&hill_corner_profile[38]);
        for (int16_t py = base_y; py < 64; py++) {
            if (py % 3 != 0) {
                display.drawPixel(37, py, SSD1306_WHITE);
            }
        }
    }

    // 4. Bushes, small rocks, flowers, and marker posts along the hill slope
    for (const auto& item : terrainItems) {
        int16_t sx = 38 + ((item.worldX - 38 - hillOffset) % 180);
        if (sx < 38) sx += 180;

        if (sx >= 38 && sx < 124) {
            const uint8_t* bmp = nullptr;
            uint8_t w = 0, h = 0;
            switch (item.type) {
                case TerrainItemType::ROCK_SM:
                    bmp = rock_sm_bmp; w = ROCK_SM_WIDTH; h = ROCK_SM_HEIGHT; break;
                case TerrainItemType::ROCK_ROUND:
                    bmp = rock_round_bmp; w = ROCK_ROUND_WIDTH; h = ROCK_ROUND_HEIGHT; break;
                case TerrainItemType::BUSH_SMALL:
                    bmp = bush_small_bmp; w = BUSH_SMALL_WIDTH; h = BUSH_SMALL_HEIGHT; break;
                case TerrainItemType::BUSH_MED:
                    bmp = bush_med_bmp; w = BUSH_MED_WIDTH; h = BUSH_MED_HEIGHT; break;
                case TerrainItemType::FLOWER:
                    bmp = flower_bmp; w = FLOWER_WIDTH; h = FLOWER_HEIGHT; break;
                case TerrainItemType::MARKER_POST:
                    bmp = marker_post_bmp; w = MARKER_POST_WIDTH; h = MARKER_POST_HEIGHT; break;
                case TerrainItemType::GRASS:
                    bmp = terrain_grass_bmp; w = TERRAIN_GRASS_WIDTH; h = TERRAIN_GRASS_HEIGHT; break;
                case TerrainItemType::MOUND:
                    bmp = terrain_mound_bmp; w = TERRAIN_MOUND_WIDTH; h = TERRAIN_MOUND_HEIGHT; break;
            }
            if (bmp) {
                int16_t midX = sx + (w / 2);
                if (midX < 38) midX = 38;
                if (midX > 127) midX = 127;
                int16_t gy = pgm_read_byte(&hill_corner_profile[midX]);
                int16_t py = gy - h + 1;
                display.drawBitmap(sx, py, bmp, w, h, SSD1306_WHITE);
            }
        }
    }
}

void CounterRenderer::drawBoulder(int16_t bx, int16_t by, uint8_t rotFrame) const {
    const uint8_t* bmp = (const uint8_t*)pgm_read_ptr(&(boulder_frames[rotFrame % BOULDER_FRAMES]));

    // 1. Fill solid opaque interior circle so ground does not show through
    display.fillCircle(bx + 14, by + 14, 13, SSD1306_BLACK);

    // 2. Draw crisp 1-bit boulder outline and rotational surface cracks
    display.drawBitmap(bx, by, bmp, BOULDER_WIDTH, BOULDER_HEIGHT, SSD1306_WHITE);
}

void CounterRenderer::drawSisyphus(int16_t sx, int16_t sy, const uint8_t* spriteBmp, uint8_t width, uint8_t height) const {
    if (!spriteBmp) return;
    display.drawBitmap(sx, sy, spriteBmp, width, height, SSD1306_WHITE);
}

void CounterRenderer::drawCounterText(const char* countStr) const {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 2);
    char buf[32];
    formatDisplayCount(countStr, buf, sizeof(buf));
    display.print(buf);
}

void CounterRenderer::renderScene(
    const char* countStr,
    const uint8_t* sisyphusSprite,
    uint8_t sisyphusWidth,
    uint8_t sisyphusHeight,
    int16_t charX,
    int16_t bldX,
    uint8_t boulderRotFrame,
    int16_t hillOffset,
    float worldProgress,
    int16_t climbProgress)
{
    display.clearDisplay();

    // Layer 1: Subtle background (simpler clouds & distant mountain peaks occluded by hill)
    drawBackground(worldProgress, climbProgress);

    // Layer 2: Bottom-right corner hill with ridges, downward moving starting plane, and items
    drawHillTerrain(hillOffset, climbProgress);

    // Position Boulder along organic ground:
    int16_t bldGroundY = getSlopeGroundY(bldX + 14, climbProgress);
    int16_t by = bldGroundY - 28;

    // Position Sisyphus along organic ground:
    int16_t sisGroundY = getSlopeGroundY(charX + 10, climbProgress);
    int16_t sy = sisGroundY - sisyphusHeight + 1;

    // Layer 3: Sisyphus character (bracing boulder with one hand when resting, heaving when pushing)
    drawSisyphus(charX, sy, sisyphusSprite, sisyphusWidth, sisyphusHeight);

    // Layer 4: Boulder (solid circular stone)
    drawBoulder(bldX, by, boulderRotFrame);

    // Layer 5: Counter text in upper-left corner
    drawCounterText(countStr);
}

