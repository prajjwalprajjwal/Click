#include "FlappyBirdApplet.h"
#include "Display.h"

void FlappyBirdApplet::init() {
    jumpRequested = false;
    gameState     = STATE_IDLE;

    // Read High Score from ESP32 NVS memory (0x9000 region)
    prefs.begin("click_stats", true);
    highScore = prefs.getUInt("flappy_hi", 0);
    prefs.end();

    resetGame();
}

void FlappyBirdApplet::update() {}

void FlappyBirdApplet::draw() {
    extern Adafruit_SSD1306 display;
    updateAndDraw(display);
}

void FlappyBirdApplet::cleanup() {
    gameState     = STATE_IDLE;
    jumpRequested = false;
}

void FlappyBirdApplet::resetGame() {
    birdX        = 20.0f;
    birdY        = 24.0f;
    velocity     = 0.0f;
    currentScore = 0;

    // Spawn pipes far enough right to prevent instant start death
    for (int i = 0; i < NUM_PIPES; ++i) {
        pipeX[i]    = 140.0f + (i * 75.0f);
        pipeGapY[i] = random(6, 64 - GAP_HEIGHT - 6);
    }
}

void FlappyBirdApplet::handleGameOver() {
    gameState = STATE_GAMEOVER;
    gameOverTime = millis();
    jumpRequested = false; // Discard any pending jump request on game over

    if (currentScore > highScore) {
        highScore = currentScore;
        // Save new high score into NVS unresetable storage
        prefs.begin("click_stats", false);
        prefs.putUInt("flappy_hi", highScore);
        prefs.end();
    }
}

void FlappyBirdApplet::updateAndDraw(Adafruit_SSD1306& disp) {
    unsigned long now = millis();

    // Read action trigger from InputManager event callback
    bool pressed  = jumpRequested;
    jumpRequested = false;

    // Non-blocking frame loop
    if (now - lastFrameTime >= FRAME_INTERVAL_MS) {
        lastFrameTime = now;

        if (gameState == STATE_PLAYING) {
            if (pressed) velocity = kJumpImpulse;
            velocity += kGravity;
            birdY    += velocity;

            // Screen floor/ceiling bounds
            if (birdY < 0.0f || birdY + BIRD_HEIGHT >= 64) {
                handleGameOver();
            }

            // Pipe collisions
            for (int i = 0; i < NUM_PIPES; ++i) {
                pipeX[i] -= PIPE_SPEED;

                if (pipeX[i] < -PIPE_WIDTH) {
                    pipeX[i]    = 128.0f;
                    pipeGapY[i] = random(6, 64 - GAP_HEIGHT - 6);
                    ++currentScore;
                }

                bool xOverlap = (pipeX[i] < birdX + BIRD_WIDTH) &&
                                (pipeX[i] + PIPE_WIDTH > birdX);
                bool yOutside = (birdY < pipeGapY[i]) ||
                                (birdY + BIRD_HEIGHT > pipeGapY[i] + GAP_HEIGHT);
                if (xOverlap && yOutside) handleGameOver();
            }

        } else if (gameState == STATE_IDLE) {
            if (pressed) {
                resetGame();
                gameState = STATE_PLAYING;
            }
        } else if (gameState == STATE_GAMEOVER) {
            // Only allow restart after cooldown has elapsed
            if (pressed && (now - gameOverTime >= GAMEOVER_COOLDOWN_MS)) {
                resetGame();
                gameState = STATE_PLAYING;
            }
        }
    }

    disp.clearDisplay();

    // --- DRAWING RENDER LAYER ---
    if (gameState == STATE_IDLE) {
#ifdef GENERATED_FLAPPYBIRDAPPLET_START_H
        // RENDER USER CUSTOM DESIGN START SCREEN (128x64)
        disp.drawBitmap(0, 0, FlappyBirdApplet_start_bmp,
                        FLAPPYBIRDAPPLET_START_WIDTH, FLAPPYBIRDAPPLET_START_HEIGHT,
                        SSD1306_WHITE);
#else
        // Fallback layout if start image header is not generated
        disp.drawBitmap(57, 12, bird_bmp, BIRD_WIDTH, BIRD_HEIGHT, SSD1306_WHITE);
        disp.setTextSize(1);
        disp.setTextColor(SSD1306_WHITE);
        disp.setCursor(30, 32);
        disp.print(F("FLAPPY BIRD"));
        disp.setCursor(2, 48);
        disp.print(F("PRESS ACTION TO START"));
#endif

    } else if (gameState == STATE_PLAYING) {
        // Draw Bird Sprite
        disp.drawBitmap((int)birdX, (int)birdY, bird_bmp, BIRD_WIDTH, BIRD_HEIGHT, SSD1306_WHITE);

        // Draw Pipes
        for (int i = 0; i < NUM_PIPES; ++i) {
            int px = (int)pipeX[i];
            disp.fillRect(px, 0, PIPE_WIDTH, pipeGapY[i], SSD1306_WHITE);
            disp.fillRect(px, pipeGapY[i] + GAP_HEIGHT, PIPE_WIDTH, 64 - (pipeGapY[i] + GAP_HEIGHT), SSD1306_WHITE);
        }

        // HUD Score (Y=6 avoids top boundary crop)
        disp.setTextSize(1);
        disp.setTextColor(SSD1306_WHITE);
        disp.setCursor(4, 10);
        disp.print(currentScore);

    } else if (gameState == STATE_GAMEOVER) {
#ifdef GENERATED_FLAPPYBIRDAPPLET_END_H
        // RENDER USER CUSTOM DESIGN END SCREEN (128x64)
        disp.drawBitmap(0, 0, FlappyBirdApplet_end_bmp,
                        FLAPPYBIRDAPPLET_END_WIDTH, FLAPPYBIRDAPPLET_END_HEIGHT,
                        SSD1306_WHITE);

        // Dynamically overlay score data on top of user custom screen graphic
        disp.setTextSize(1);
        disp.setTextColor(SSD1306_WHITE);
        disp.setCursor(58, 36);
        disp.print(F("SCORE: "));
        disp.print(currentScore);
        disp.setCursor(58, 48);
        disp.print(F("BEST:  "));
        disp.print(highScore);
#else
        // Fallback layout
        disp.setTextSize(1);
        disp.setTextColor(SSD1306_WHITE);
        disp.setCursor(37, 8);
        disp.print(F("GAME OVER"));
        disp.setCursor(26, 24);
        disp.print(F("SCORE: "));
        disp.print(currentScore);
        disp.setCursor(26, 36);
        disp.print(F("BEST:  "));
        disp.print(highScore);
        disp.setCursor(8, 50);
        disp.print(F("PRESS ACTION TO RETRY"));
#endif
    }

    disp.display();
}
