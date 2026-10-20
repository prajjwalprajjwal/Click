#ifndef TETRIS_APPLET_H
#define TETRIS_APPLET_H

#include <Arduino.h>
#include <Preferences.h>
#include "Applet.h"

/**
 * Tetris applet for the Click handheld (128x64 SSD1306, MODE + ACTION buttons).
 *
 * Controls (while a round is in progress):
 *   MODE   tap              = move left
 *   ACTION tap              = move right
 *   ACTION hold             = soft drop (auto-repeat)
 *   MODE + ACTION together  = rotate the falling brick
 *   MODE held (~1s)         = leave the round and return to the applet catalog
 *                             (handled by main.cpp, not by this applet)
 *
 * A short MODE press no longer navigates between applets, so both buttons are
 * free for gameplay. Navigation only happens when MODE is held, which main.cpp
 * turns into "advance to next applet". Rotation therefore uses the two-button
 * chord, which lets the whole round be played with both hands on the device.
 * Holding both buttons for the OS 4s "both held" event still quits to the
 * title screen.
 *
 * Button taps/holds are derived from raw button state in update() (same
 * approach as TimingGameApplet), so the 1s OS hold threshold does not apply.
 */
class TetrisApplet : public Applet {
private:
    enum GameState : uint8_t {
        STATE_IDLE     = 0,   // title screen
        STATE_PLAYING  = 1,
        STATE_GAMEOVER = 2
    };

    enum ButtonEvent : uint8_t { EVT_NONE, EVT_TAP, EVT_HOLD };

    // Board geometry: 10x16 cells, 4px pitch (3x3 block + 1px gap) = 40x64 px
    static constexpr uint8_t  kCols   = 10;
    static constexpr uint8_t  kRows   = 16;
    static constexpr uint8_t  kCell   = 4;
    static constexpr int16_t  kBoardX = 2;

    // Input timing (loop runs at ~30 FPS, so these resolve to 2-8 frames)
    static constexpr uint16_t kHoldMs         = 240;
    static constexpr uint16_t kMoveRepeatMs   = 100;
    static constexpr uint16_t kDropRepeatMs   = 66;
    // Both buttons must land within this window to count as the rotate chord
    static constexpr uint16_t kComboWindowMs  = 120;

    // Game over input cooldown (prevents accidental immediate restart / MODE navigation)
    static constexpr uint32_t kGameOverCooldownMs = 1500;

    struct ButtonTracker {
        bool     down        = false;
        bool     held        = false;
        bool     suppressTap = false;
        uint32_t pressedAt   = 0;
        uint32_t lastRepeat  = 0;
        void sync(bool isDown, uint32_t now);
        ButtonEvent step(bool isDown, uint32_t now, uint16_t repeatMs);
        // Discard the tap this press would emit on release (used by the
        // two-button rotate chord so the chord is not read as a move).
        void suppress() { suppressTap = true; }
    };

    GameState gameState = STATE_IDLE;

    // Playfield and active piece
    bool    grid[kRows][kCols];
    uint8_t curType = 0;
    uint8_t nextType = 0;
    uint8_t curRot = 0;
    int8_t  curX = 0;
    int8_t  curY = 0;

    // 7-bag randomizer
    uint8_t bag[7];
    uint8_t bagIndex = 7;

    // Progress
    uint32_t score = 0;
    uint16_t lines = 0;
    uint8_t  level = 1;

    // Timing / input state
    uint32_t lastGravityMs = 0;
    uint32_t gameOverTime  = 0;
    bool     dirty         = true;   // redraw only when something changed (saves I2C power)
    bool     dropLocked    = false;  // ACTION still held from previous piece: ignore soft drop
    bool     newBest       = false;
    bool     bothDownPrev  = false;  // previous frame's two-button chord state
    ButtonTracker modeBtn;
    ButtonTracker actionBtn;

    // NVS High Score
    uint32_t highScore = 0;
    Preferences prefs;

    // Game logic
    void startGame(uint32_t now);
    void spawnPiece(uint32_t now);
    void handleGameOver(uint32_t now);
    void lockPiece(uint32_t now);
    uint8_t clearLines();
    bool collides(uint8_t type, uint8_t rot, int8_t x, int8_t y) const;
    bool tryMove(int8_t dx, int8_t dy);
    bool tryRotate();
    uint8_t nextFromBag();
    uint16_t gravityIntervalMs() const;
    bool commitHighScore();

    // Rendering
    void drawTitle();
    void drawBoard();
    void drawPanel();
    void drawGameOverOverlay();

    static void formatNumber(uint32_t value, char* out, size_t outSize);
    static void drawCenteredText(const char* text, int16_t y);

public:
    void init()    override;
    void update()  override;
    void draw()    override;
    void cleanup() override;
    void onPrepareSleep() override;
    void onBothHeld() override;

    void preloadState();
    uint32_t getHighScore() const { return highScore; }
    void setHighScore(uint32_t value) {
        if (value > highScore) {
            highScore = value;
            prefs.begin("click_stats", false);
            prefs.putUInt("tetris_hi", highScore);
            prefs.end();
        }
    }
};

#endif // TETRIS_APPLET_H