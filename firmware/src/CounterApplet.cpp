#include "CounterApplet.h"
#include "Display.h"
#include "HomeApplet.h"
#include "clicker/ClickerConfig.h"
#include "clicker/SisyphusSprites.h"

extern HomeApplet homeApplet;

void CounterApplet::loadState() {
  if (!storageReady) {
    storageReady = persistence.begin();
  }
  if (!storageReady) {
    Serial.println("[SISYPHUS] NVS unavailable, running in RAM only");
    counter.reset();
    climbProgress = 0;
    worldProgress = 0.0f;
    boulderRotPhase = 0;
    memset(milestoneFlags, 0, sizeof(milestoneFlags));
    frameDirty = true;
    return;
  }

  persistence.load(counter, milestoneFlags);
  uint32_t savedOffset = persistence.getTerrainOffset();
  worldProgress = static_cast<float>(savedOffset);
  boulderRotPhase = static_cast<uint8_t>(savedOffset % BOULDER_FRAMES);

  homeApplet.applyUnlockState(persistence.getHomeUnlockFlags());
  animMode = SisyphusAnimMode::IDLE_WALKING;
  pushFrame = 0;
  pendingPushes = 0;
  climbProgress = 0; // Fresh baseline on applet startup/cycle
  frameDirty = true;
  lastFrameTime = millis();

  Serial.print("[SISYPHUS] count = ");
  Serial.println(counter.getString());
}

void CounterApplet::persistNow() {
  if (!storageReady) {
    return;
  }
  persistence.setTerrainOffset(static_cast<uint32_t>(worldProgress));
  persistence.flush(counter, milestoneFlags);
}

void CounterApplet::persistIfNeeded(bool force) {
  if (!storageReady) {
    return;
  }

  uint32_t now = millis();
  if (force || persistence.shouldPersist(now)) {
    persistence.setTerrainOffset(static_cast<uint32_t>(worldProgress));
    persistence.flush(counter, milestoneFlags);
    persistence.clearPeriodicCounter();
  }
}

void CounterApplet::handleClick() {
  // 1. Increment counter immediately (pure decimal arithmetic, no integer
  // overflow)
  if (!counter.increment()) {
    return;
  }

  uint32_t now = millis();
  persistence.onClickRecorded(now);

#if CLICKER_DEBUG
  Serial.print("[SISYPHUS] count = ");
  Serial.println(counter.getString());
#endif

  // Check home unlock milestones
  uint64_t lifetime = counter.getLifetimeClicks();
  uint32_t unlocks = persistence.getHomeUnlockFlags();
  if (lifetime >= 100)
    unlocks |= 0x01;
  if (lifetime >= 500)
    unlocks |= 0x02;
  if (lifetime >= 5000)
    unlocks |= 0x04;
  if (lifetime >= 10000)
    unlocks |= 0x08;
  if (lifetime >= 75000)
    unlocks |= 0x10;
  if (lifetime >= 30000)
    unlocks |= 0x20;
  if (lifetime >= 175000)
    unlocks |= 0x40;
  if (unlocks != persistence.getHomeUnlockFlags()) {
    persistence.setHomeUnlockFlags(unlocks);
    homeApplet.applyUnlockState(unlocks);
  }

  // 2. Advance boulder rotation and persistent uphill position per click
  boulderRotPhase =
      static_cast<uint8_t>((boulderRotPhase + 1) % BOULDER_FRAMES);
  climbProgress += 2;    // Persistent uphill movement step
  worldProgress += 1.0f; // Distant clouds/mountain parallax

  // 3. Queue push animation
  if (pendingPushes < 5) {
    pendingPushes++;
  }

  // Start push heave immediately
  if (animMode != SisyphusAnimMode::PUSHING) {
    animMode = SisyphusAnimMode::PUSHING;
    pushFrame = 0;
    lastFrameTime = now;
    frameDirty = true;
  }
}

void CounterApplet::updateAnimation(uint32_t now) {
  if (animMode == SisyphusAnimMode::PUSHING) {
    // Paced push heave animation: 25 FPS (40ms per frame)
    if ((now - lastFrameTime) >= PUSH_FRAME_INTERVAL_MS) {
      lastFrameTime = now;
      pushFrame++;

      if (pushFrame >= 4) {
        // Completed one 4-frame push cycle:
        if (pendingPushes > 0) {
          pendingPushes--;
        }

        if (pendingPushes > 0) {
          // Direct chain into next queued push
          pushFrame = 0;
        } else {
          // Transition to idle upright posture AT CURRENT CLIMBED POSITION
          animMode = SisyphusAnimMode::IDLE_WALKING;
          pushFrame = 0;
        }
      }
      frameDirty = true;
    }
  } else {
    // Idle state: position is fully preserved, no snapping backward!
    if ((now - lastFrameTime) >= WALK_FRAME_INTERVAL_MS) {
      lastFrameTime = now;
    }
  }
}

#if CLICKER_DEBUG
void CounterApplet::debugSimulateCount(const char *target) {
  counter.setString(target);
  frameDirty = true;
  persistIfNeeded(true);

  Serial.print("[SISYPHUS] Simulated count = ");
  Serial.println(counter.getString());
}

void CounterApplet::processDebugSerial() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (line.equalsIgnoreCase("info")) {
    Serial.print("[SISYPHUS] count = ");
    Serial.println(counter.getString());
    return;
  }

  if (line.startsWith("sim ")) {
    debugSimulateCount(line.substring(4).c_str());
    return;
  }

  Serial.println("[SISYPHUS] Debug commands: info | sim <count>");
}
#endif

void CounterApplet::resetAll() {
  counter.reset();
  climbProgress = 0;
  worldProgress = 0.0f;
  boulderRotPhase = 0;
  pendingPushes = 0;
  animMode = SisyphusAnimMode::IDLE_WALKING;
  pushFrame = 0;
  memset(milestoneFlags, 0, sizeof(milestoneFlags));

  persistence.setHomeUnlockFlags(0);
  persistence.setLastCycleCompleteLifetime(0);
  persistence.setTerrainOffset(0);
  persistence.markDirty();

  homeApplet.applyUnlockState(0);
  persistNow();
  frameDirty = true;

  Serial.println("[SISYPHUS] Reset - count cleared");
}

void CounterApplet::preloadState() { loadState(); }

void CounterApplet::init() {
  preloadState();
  animMode = SisyphusAnimMode::IDLE_WALKING;
  pushFrame = 0;
  pendingPushes = 0;
  climbProgress = 0; // Reset position when cycled through app or rebooted
  frameDirty = true;
  lastFrameTime = millis();

#if CLICKER_DEBUG
  Serial.println("[SISYPHUS] Debug enabled. Commands: info | sim <count>");
#endif
}

void CounterApplet::update() {
  uint32_t now = millis();
  updateAnimation(now);
  persistIfNeeded(false);

#if CLICKER_DEBUG
  processDebugSerial();
#endif

  // Yield CPU time to FreeRTOS scheduler so IDLE task and background threads
  // stay fed
  yield();
}

void CounterApplet::draw() {
  uint32_t now = millis();
  // Enforce 30 FPS ceiling to prevent I2C bus flooding and allow CPU breathing
  // room
  if (!frameDirty || (now - lastDisplayTime) < MIN_DISPLAY_INTERVAL_MS) {
    return;
  }
  lastDisplayTime = now;
  frameDirty = false;

  // Calculate persistent screen positions on organic slope:
  int16_t charX = 0;
  int16_t bldX = 0;
  int32_t hillOffset = 0;

  if (climbProgress <= 18) {
    // Starts on flat ground plane at base (charX=18, bldX=38), climbs onto the
    // hill
    charX = 18 + (climbProgress * 24) / 18;
    bldX = 38 + (climbProgress * 26) / 18;
    hillOffset = 0;
  } else {
    // Steady climbing position on the bottom-right hill:
    charX = 42;
    bldX = 64;
    hillOffset = climbProgress - 18;
  }

  const uint8_t *sisBmp = nullptr;
  uint8_t sisW = 0;
  uint8_t sisH = 0;

  if (animMode == SisyphusAnimMode::IDLE_WALKING) {
    // Resting posture: braced with one hand holding the rock so it doesn't fall
    // down!
    sisBmp = sisyphus_resting_bmp;
    sisW = SISYPHUS_REST_WIDTH;
    sisH = SISYPHUS_REST_HEIGHT;
  } else {
    // Push heave frames: 0, 1, 2, 3 -> dynamic two-legged strides
    sisBmp =
        (const uint8_t *)pgm_read_ptr(&(sisyphus_push_frames[pushFrame % 4]));
    sisW = SISYPHUS_PUSH_WIDTH;
    sisH = SISYPHUS_PUSH_HEIGHT;
  }

  // Render scene with bottom-right corner hill, downward moving flat ground
  // plane, and resting brace
  renderer.renderScene(counter.getString(), sisBmp, sisW, sisH, charX, bldX,
                       boulderRotPhase, hillOffset, worldProgress,
                       climbProgress);

  // Exactly ONE I2C transfer per frame
  display.display();
  // Yield to FreeRTOS scheduler immediately after heavy 25ms I2C transfer
  delay(1);
}

void CounterApplet::cleanup() { persistNow(); }

void CounterApplet::onPrepareSleep() { persistNow(); }

void CounterApplet::onActionClick() { handleClick(); }

void CounterApplet::onBothHeld() { resetAll(); }
