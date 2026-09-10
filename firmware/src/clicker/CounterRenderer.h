#ifndef COUNTER_RENDERER_H
#define COUNTER_RENDERER_H

#include "clicker/SisyphusSprites.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class CounterRenderer {
private:
  void formatDisplayCount(const char *countStr, char *buffer,
                          size_t bufferSize) const;
  void drawBackground(float worldProgress, int16_t climbProgress) const;
  void drawHillTerrain(int16_t hillOffset, int16_t climbProgress) const;
  void drawBoulder(int16_t bx, int16_t by, uint8_t rotFrame) const;
  void drawSisyphus(int16_t sx, int16_t sy, const uint8_t *spriteBmp,
                    uint8_t width, uint8_t height) const;
  void drawCounterText(const char *countStr) const;

public:
  // Ground profile:
  // - Hill occupies ~30% in bottom-right corner (from x=38 to 127) with ridges
  // and shelves, overall uphill.
  // - On left (x < 38): starts with flat ground plane at y=62, which moves
  // downwards out of frame when movement starts.
  static inline int16_t getSlopeGroundY(int16_t x, int16_t climbProgress) {
    if (x >= 38) {
      if (x > 127)
        x = 127;
      return pgm_read_byte(&hill_corner_profile[x]);
    } else {
      int16_t plain_y = 62 + climbProgress * 3;
      return plain_y;
    }
  }

  void renderScene(const char *countStr, const uint8_t *sisyphusSprite,
                   uint8_t sisyphusWidth, uint8_t sisyphusHeight, int16_t charX,
                   int16_t bldX, uint8_t boulderRotFrame, int16_t hillOffset,
                   float worldProgress, int16_t climbProgress);
};

#endif // COUNTER_RENDERER_H
