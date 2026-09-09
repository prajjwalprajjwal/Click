#include "clicker/CounterRenderer.h"
#include "Display.h"
#include "clicker/ClickerConfig.h"
#include "fonts/ThemeFonts.h"
#include <cstdio>

void CounterRenderer::formatCount(uint64_t count, char *buffer,
                                  size_t bufferSize) const {
  if (bufferSize == 0) {
    return;
  }
  // Always display full count without abbreviation; drawCenteredBestFit will
  // handle scaling.
  snprintf(buffer, bufferSize, "%llu", (unsigned long long)count);
}

bool CounterRenderer::shouldShowRemaining(uint32_t cycleClicks) const {
  return cycleClicks >= 99000 && cycleClicks < 100000;
}

uint64_t CounterRenderer::getRemainingInCycle(uint64_t lifetimeClicks) const {
  return CLICKER_CYCLE_LENGTH - (lifetimeClicks % CLICKER_CYCLE_LENGTH);
}

void CounterRenderer::drawCountCentered(uint64_t count) const {
  char buffer[24];
  formatCount(count, buffer, sizeof(buffer));
  ThemeFonts::drawCenteredBestFit(&Rajdhani40pt7b, &Rajdhani32pt7b, buffer, 32,
                                  124);
}

void CounterRenderer::drawRemaining(uint64_t remaining) {
  display.clearDisplay();
  drawCountCentered(remaining);
  display.display();
}

void CounterRenderer::drawNormal(uint64_t lifetimeClicks,
                                 uint32_t cycleClicks) {
  if (shouldShowRemaining(cycleClicks)) {
    drawRemaining(getRemainingInCycle(lifetimeClicks));
    return;
  }

  display.clearDisplay();
  drawCountCentered(lifetimeClicks);
  display.display();
}
