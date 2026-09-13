#ifndef FLAPPY_BIRD_APPLET_H
#define FLAPPY_BIRD_APPLET_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "Applet.h"

// Automatically includes pre-converted user PNG screen assets
#if __has_include("generated_assets/all_assets.h")
  #include "generated_assets/all_assets.h"
#endif

// Bird sprite — uses FlappyBirdApplet_char.png if converted, otherwise falls back to hardcoded 14x9 bitmap
#ifdef GENERATED_FLAPPYBIRDAPPLET_CHAR_H
  #define BIRD_WIDTH  FLAPPYBIRDAPPLET_CHAR_WIDTH
  #define BIRD_HEIGHT FLAPPYBIRDAPPLET_CHAR_HEIGHT
  #define bird_bmp    FlappyBirdApplet_char_bmp
#else
  // Fallback: hardcoded 14x9 pixel bird sprite
  #define BIRD_WIDTH  14
  #define BIRD_HEIGHT 9
  static const uint8_t bird_bmp[] PROGMEM = {
      0xE0, 0x07, 0xAE, 0x08, 0x91, 0x0A, 0x91, 0x08,
      0x21, 0x0F, 0x3E, 0x3C, 0x04, 0x3C, 0x08, 0x02,
      0xF0, 0x01
  };
#endif

class FlappyBirdApplet : public Applet {
private:
    enum GameState : uint8_t {
        STATE_IDLE     = 0,
        STATE_PLAYING  = 1,
        STATE_GAMEOVER = 2
    };

    GameState gameState = STATE_IDLE;

    // Physics Engine Variables
    float birdX        = 20.0f;
    float birdY        = 24.0f;
    float velocity     = 0.0f;
    static constexpr float kGravity     = 0.14f; // Smooth drop rate
    static constexpr float kJumpImpulse = -1.9f; // Responsive jump

    // Pipe Parameters
    static constexpr int   NUM_PIPES   = 2;
    static constexpr int   PIPE_WIDTH  = 8;
    static constexpr int   GAP_HEIGHT  = 28;
    static constexpr float PIPE_SPEED  = 1.0f;

    float pipeX[NUM_PIPES];
    int   pipeGapY[NUM_PIPES];

    // Non-blocking timer (~40 FPS execution loop)
    unsigned long lastFrameTime = 0;
    static constexpr unsigned long FRAME_INTERVAL_MS = 25;

    // Game over input cooldown (prevents accidental immediate restart)
    unsigned long gameOverTime = 0;
    static constexpr unsigned long GAMEOVER_COOLDOWN_MS = 1500;

    volatile bool jumpRequested = false;

    // NVS Memory & High Score
    uint16_t currentScore = 0;
    uint16_t highScore    = 0;
    Preferences prefs;

    void resetGame();
    void handleGameOver();
    void updateAndDraw(Adafruit_SSD1306& disp);

public:
    void init()    override;
    void update()  override;
    void draw()    override;
    void cleanup() override;

    // Input callback override matching Click OSManager architecture
    void onActionClick() override {
        // Discard action click during game-over cooldown
        if (gameState == STATE_GAMEOVER && (millis() - gameOverTime < GAMEOVER_COOLDOWN_MS)) {
            return;
        }
        jumpRequested = true;
    }

    void preloadState();
    uint16_t getHighScore() const { return highScore; }
};

#endif // FLAPPY_BIRD_APPLET_H
