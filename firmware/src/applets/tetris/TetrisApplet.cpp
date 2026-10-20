#include "TetrisApplet.h"
#include "Display.h"
#include "InputManager.h"
#include "fonts/ThemeFonts.h"
#include "system/WS2812.h"
#include "system/SoundFX.h"

// ---------------------------------------------------------------------------
// Tetromino data (order: I O T S Z J L). Each piece is 4 cells (x,y) inside
// an n x n box; rotation is a clockwise 90 degree turn inside that box.
// ---------------------------------------------------------------------------
namespace {
const int8_t kShapes[7][4][2] = {
    {{0,1},{1,1},{2,1},{3,1}},   // I
    {{0,0},{1,0},{0,1},{1,1}},   // O
    {{1,0},{0,1},{1,1},{2,1}},   // T
    {{1,0},{2,0},{0,1},{1,1}},   // S
    {{0,0},{1,0},{1,1},{2,1}},   // Z
    {{0,0},{0,1},{1,1},{2,1}},   // J
    {{2,0},{0,1},{1,1},{2,1}},   // L
};
const uint8_t kBoxSize[7] = {4, 2, 3, 3, 3, 3, 3};

void getCells(uint8_t type, uint8_t rot, int8_t out[4][2]) {
    const uint8_t n = kBoxSize[type];
    for (uint8_t i = 0; i < 4; i++) {
        int8_t x = kShapes[type][i][0];
        int8_t y = kShapes[type][i][1];
        for (uint8_t r = 0; r < rot; r++) {
            const int8_t nx = static_cast<int8_t>(n - 1 - y);
            const int8_t ny = x;
            x = nx;
            y = ny;
        }
        out[i][0] = x;
        out[i][1] = y;
    }
}

inline void drawBlock(int16_t x, int16_t y) {
    display.fillRect(x, y, 3, 3, SSD1306_WHITE);
}
} // namespace

// ---------------------------------------------------------------------------
// Input: tap/hold detection from raw button state
// ---------------------------------------------------------------------------
void TetrisApplet::ButtonTracker::sync(bool isDown, uint32_t now) {
    // Adopt the current physical state without generating an event, so a
    // button that is already down when a round starts can't cause a stray tap.
    down        = isDown;
    held        = isDown;
    pressedAt   = now;
    lastRepeat  = now;
    suppressTap = false;
}

TetrisApplet::ButtonEvent TetrisApplet::ButtonTracker::step(bool isDown, uint32_t now, uint16_t repeatMs) {
    if (isDown && !down) {
        down = true;
        held = false;
        pressedAt = now;
        return EVT_NONE;
    }
    if (!isDown && down) {
        down = false;
        const bool wasHeld = held;
        held = false;
        if (suppressTap) {                       // part of the rotate chord
            suppressTap = false;
            return EVT_NONE;
        }
        return wasHeld ? EVT_NONE : EVT_TAP;     // tap fires on release
    }
    if (isDown && down) {
        if (!held) {
            if (now - pressedAt >= kHoldMs) {
                held = true;
                lastRepeat = now;
                return EVT_HOLD;
            }
        } else if (now - lastRepeat >= repeatMs) {
            lastRepeat = now;
            return EVT_HOLD;
        }
    }
    return EVT_NONE;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void TetrisApplet::preloadState() {
    prefs.begin("click_stats", true);
    highScore = prefs.getUInt("tetris_hi", 0);
    prefs.end();
}

void TetrisApplet::init() {
    gameState  = STATE_IDLE;
    newBest    = false;
    dropLocked = false;
    bothDownPrev = false;
    score      = 0;
    lines      = 0;
    level      = 1;
    memset(grid, 0, sizeof(grid));
    preloadState();

    const uint32_t now = millis();
    modeBtn.sync(isModeButtonPressed(), now);
    actionBtn.sync(isActionButtonPressed(), now);
    dirty = true;
}

void TetrisApplet::cleanup() {
    commitHighScore();
    gameState = STATE_IDLE;
    WS2812Driver::clear();
    SoundFX::stop();
}

void TetrisApplet::onPrepareSleep() {
    // A sleeping device wakes into the Clicker applet, so bank the score now.
    commitHighScore();
}

void TetrisApplet::onBothHeld() {
    // Holding BOTH buttons for the OS 4s "both held" event quits the round and
    // returns to the title screen. (A short MODE hold is the usual way out; it
    // is handled by main.cpp as "return to the applet catalog".)
    commitHighScore();
    gameState = STATE_IDLE;
    newBest   = false;
    WS2812Driver::clear();
    SoundFX::stop();
    dirty = true;
}

bool TetrisApplet::commitHighScore() {
    if (score > highScore) {
        highScore = score;
        prefs.begin("click_stats", false);
        prefs.putUInt("tetris_hi", highScore);
        prefs.end();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Game logic
// ---------------------------------------------------------------------------
uint8_t TetrisApplet::nextFromBag() {
    if (bagIndex >= 7) {
        for (uint8_t i = 0; i < 7; i++) bag[i] = i;
        for (uint8_t i = 6; i > 0; i--) {
            const uint8_t j = static_cast<uint8_t>(random(i + 1));
            const uint8_t t = bag[i];
            bag[i] = bag[j];
            bag[j] = t;
        }
        bagIndex = 0;
    }
    return bag[bagIndex++];
}

uint16_t TetrisApplet::gravityIntervalMs() const {
    const int v = 600 - (static_cast<int>(level) - 1) * 50;
    return static_cast<uint16_t>(v < 80 ? 80 : v);
}

bool TetrisApplet::collides(uint8_t type, uint8_t rot, int8_t x, int8_t y) const {
    int8_t cells[4][2];
    getCells(type, rot, cells);
    for (uint8_t i = 0; i < 4; i++) {
        const int8_t gx = static_cast<int8_t>(x + cells[i][0]);
        const int8_t gy = static_cast<int8_t>(y + cells[i][1]);
        if (gx < 0 || gx >= kCols || gy >= kRows) return true;
        if (gy >= 0 && grid[gy][gx]) return true;
    }
    return false;
}

bool TetrisApplet::tryMove(int8_t dx, int8_t dy) {
    if (collides(curType, curRot, static_cast<int8_t>(curX + dx), static_cast<int8_t>(curY + dy))) {
        return false;
    }
    curX = static_cast<int8_t>(curX + dx);
    curY = static_cast<int8_t>(curY + dy);
    dirty = true;
    return true;
}

bool TetrisApplet::tryRotate() {
    const uint8_t newRot = (curRot + 1) & 3;
    static const int8_t kicks[5] = {0, -1, 1, -2, 2};   // simple wall kicks
    for (uint8_t k = 0; k < 5; k++) {
        const int8_t x = static_cast<int8_t>(curX + kicks[k]);
        if (!collides(curType, newRot, x, curY)) {
            curRot = newRot;
            curX   = x;
            dirty  = true;
            return true;
        }
    }
    return false;
}

void TetrisApplet::startGame(uint32_t now) {
    memset(grid, 0, sizeof(grid));
    score = 0;
    lines = 0;
    level = 1;
    newBest = false;
    dropLocked = false;
    bagIndex = 7;
    randomSeed(micros());

    nextType = nextFromBag();
    gameState = STATE_PLAYING;
    spawnPiece(now);

    bothDownPrev = false;
    modeBtn.sync(isModeButtonPressed(), now);
    actionBtn.sync(isActionButtonPressed(), now);
    dirty = true;
}

void TetrisApplet::spawnPiece(uint32_t now) {
    curType  = nextType;
    nextType = nextFromBag();
    curRot   = 0;
    curX     = static_cast<int8_t>((kCols - kBoxSize[curType]) / 2);
    curY     = (curType == 0) ? -1 : 0;     // I piece sits in row 1 of its box
    lastGravityMs = now;
    dirty = true;
    if (collides(curType, curRot, curX, curY)) {
        handleGameOver(now);
    }
}

uint8_t TetrisApplet::clearLines() {
    uint8_t cleared = 0;
    for (int8_t r = kRows - 1; r >= 0; r--) {
        bool full = true;
        for (uint8_t c = 0; c < kCols; c++) {
            if (!grid[r][c]) { full = false; break; }
        }
        if (!full) continue;
        for (int8_t rr = r; rr > 0; rr--) {
            for (uint8_t c = 0; c < kCols; c++) grid[rr][c] = grid[rr - 1][c];
        }
        for (uint8_t c = 0; c < kCols; c++) grid[0][c] = false;
        cleared++;
        r++;                                  // re-check this row after the shift
    }
    return cleared;
}

void TetrisApplet::lockPiece(uint32_t now) {
    int8_t cells[4][2];
    getCells(curType, curRot, cells);
    for (uint8_t i = 0; i < 4; i++) {
        const int8_t gx = static_cast<int8_t>(curX + cells[i][0]);
        const int8_t gy = static_cast<int8_t>(curY + cells[i][1]);
        if (gy < 0) {                         // locked above the visible board
            handleGameOver(now);
            return;
        }
        grid[gy][gx] = true;
    }

    const uint8_t cleared = clearLines();
    if (cleared > 0) {
        static const uint16_t kLinePoints[5] = {0, 100, 300, 500, 800};
        score += static_cast<uint32_t>(kLinePoints[cleared]) * level;
        lines += cleared;
        level = static_cast<uint8_t>(lines / 10 + 1);

        if (cleared >= 4) {
            SoundFX::playCelebrationFanfare();      // "Tetris!"
            WS2812Driver::startCelebration(500);
        } else {
            SoundFX::playScore();
        }
    }

    dropLocked = actionBtn.down;              // don't carry a held soft-drop into the next piece
    spawnPiece(now);
}

void TetrisApplet::handleGameOver(uint32_t now) {
    gameState    = STATE_GAMEOVER;
    gameOverTime = now;
    dirty        = true;

    if (commitHighScore()) {
        newBest = true;
        SoundFX::playCelebrationFanfare();
        WS2812Driver::startCelebration(500);
    } else {
        SoundFX::playGameOver();
        WS2812Driver::flash(255, 0, 0, 450);
    }
}

// ---------------------------------------------------------------------------
// update(): input + gravity
// ---------------------------------------------------------------------------
void TetrisApplet::update() {
    const uint32_t now = millis();
    const bool modeDown   = isModeButtonPressed();
    const bool actionDown = isActionButtonPressed();

    const ButtonEvent m = modeBtn.step(modeDown, now, kMoveRepeatMs);
    const ButtonEvent a = actionBtn.step(actionDown, now, kDropRepeatMs);

    // Two-button chord = rotate. Both presses must land close together so that
    // adding MODE while already soft-dropping is not mistaken for a chord. The
    // taps produced by the chord are suppressed, otherwise releasing the
    // buttons would also register as a left/right move.
    const bool bothDown = modeBtn.down && actionBtn.down;
    bool bothCombo = false;
    if (bothDown && !bothDownPrev) {
        const uint32_t earliest = (modeBtn.pressedAt < actionBtn.pressedAt)
                                      ? modeBtn.pressedAt
                                      : actionBtn.pressedAt;
        if ((now - earliest) <= kComboWindowMs) {
            bothCombo = true;
            modeBtn.suppress();
            actionBtn.suppress();
            dropLocked = true;      // never soft-drop on the chord's hold
        }
    }
    bothDownPrev = bothDown;

    if (!actionDown) dropLocked = false;

    switch (gameState) {
        case STATE_IDLE:
            // The right button starts a round; MODE and chords are ignored here
            // (a MODE hold is handled by main.cpp as "return to catalog").
            if (a != EVT_NONE) startGame(now);
            break;

        case STATE_GAMEOVER:
            if (a != EVT_NONE && (now - gameOverTime) >= kGameOverCooldownMs) startGame(now);
            break;

        case STATE_PLAYING:
            if (m == EVT_TAP) tryMove(-1, 0);

            if (bothCombo) {
                tryRotate();
            } else if (a == EVT_TAP) {
                tryMove(1, 0);
            } else if (a == EVT_HOLD && !dropLocked) {
                if (tryMove(0, 1)) {
                    score++;                  // soft drop bonus
                    lastGravityMs = now;
                    dirty = true;
                } else {
                    lockPiece(now);
                }
            }

            if (gameState == STATE_PLAYING && (now - lastGravityMs) >= gravityIntervalMs()) {
                lastGravityMs = now;
                if (!tryMove(0, 1)) lockPiece(now);
                dirty = true;
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void TetrisApplet::formatNumber(uint32_t value, char* out, size_t outSize) {
    // Full integer with thousands separators (PROJECT_RULES: never abbreviate)
    char raw[12];
    snprintf(raw, sizeof(raw), "%lu", static_cast<unsigned long>(value));
    const size_t len = strlen(raw);
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < outSize; i++) {
        if (i > 0 && (len - i) % 3 == 0 && o + 1 < outSize) out[o++] = ',';
        if (o + 1 < outSize) out[o++] = raw[i];
    }
    out[o] = '\0';
}

void TetrisApplet::drawCenteredText(const char* text, int16_t y) {
    const int16_t w = static_cast<int16_t>(strlen(text) * 6);   // default 6x8 font
    display.setCursor((128 - w) / 2, y);
    display.print(text);
}

void TetrisApplet::drawTitle() {
    ThemeFonts::drawCenteredBestFit(&Rajdhani24pt7b, &Rajdhani18pt7b, "TETRIS", 12, 124);
    ThemeFonts::reset();
    drawCenteredText("ACT=START", 29);
    drawCenteredText("MODE=L ACT=R", 38);
    drawCenteredText("BOTH=ROTATE", 47);
    drawCenteredText("HOLD MODE=EXIT", 56);
}

void TetrisApplet::drawBoard() {
    // Walls: left, right and floor (no top wall - pieces enter from the top edge)
    display.drawFastVLine(0, 0, 64, SSD1306_WHITE);
    display.drawFastVLine(kBoardX + kCols * kCell, 0, 64, SSD1306_WHITE);
    display.drawFastHLine(0, 63, kBoardX + kCols * kCell + 1, SSD1306_WHITE);

    for (uint8_t r = 0; r < kRows; r++) {
        for (uint8_t c = 0; c < kCols; c++) {
            if (grid[r][c]) drawBlock(kBoardX + c * kCell, r * kCell);
        }
    }

    if (gameState == STATE_PLAYING) {
        int8_t cells[4][2];
        getCells(curType, curRot, cells);
        for (uint8_t i = 0; i < 4; i++) {
            const int8_t gy = static_cast<int8_t>(curY + cells[i][1]);
            if (gy >= 0) {
                drawBlock(kBoardX + (curX + cells[i][0]) * kCell, gy * kCell);
            }
        }
    }
}

void TetrisApplet::drawPanel() {
    char buf[16];

    display.setCursor(50, 0);
    display.print(F("SCORE"));
    formatNumber(score, buf, sizeof(buf));
    display.setCursor(50, 9);
    display.print(buf);

    display.setCursor(50, 22);
    display.print(F("LINES"));
    formatNumber(lines, buf, sizeof(buf));
    display.setCursor(50, 31);
    display.print(buf);

    display.setCursor(98, 22);
    display.print(F("LVL"));
    formatNumber(level, buf, sizeof(buf));
    display.setCursor(98, 31);
    display.print(buf);

    display.setCursor(50, 46);
    display.print(F("NEXT"));
    int8_t cells[4][2];
    getCells(nextType, 0, cells);
    for (uint8_t i = 0; i < 4; i++) {
        drawBlock(92 + cells[i][0] * kCell, 44 + cells[i][1] * kCell);
    }
}

void TetrisApplet::drawGameOverOverlay() {
    char buf[24];
    char num[16];

    display.fillRect(6, 3, 116, 58, SSD1306_BLACK);
    display.drawRect(6, 3, 116, 58, SSD1306_WHITE);

    drawCenteredText("GAME OVER", 8);

    formatNumber(score, num, sizeof(num));
    snprintf(buf, sizeof(buf), "SCORE: %s", num);
    drawCenteredText(buf, 20);

    formatNumber(highScore, num, sizeof(num));
    snprintf(buf, sizeof(buf), "BEST:  %s", num);
    drawCenteredText(buf, 30);

    if (newBest) drawCenteredText("NEW BEST!", 40);
    drawCenteredText("ACTION: RETRY", 50);
}

void TetrisApplet::draw() {
    if (!dirty) return;      // nothing changed: skip the 1KB I2C frame push
    dirty = false;

    display.clearDisplay();
    ThemeFonts::reset();     // default 6x8 font, size 1, white (other applets may leave a custom font set)

    if (gameState == STATE_IDLE) {
        drawTitle();
    } else {
        drawBoard();
        drawPanel();
        if (gameState == STATE_GAMEOVER) drawGameOverOverlay();
    }

    display.display();
}