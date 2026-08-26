#ifndef TIMING_GAME_APPLET_H
#define TIMING_GAME_APPLET_H

#include <stdint.h>
#include <stddef.h>
#include "Applet.h"

class TimingGameApplet : public Applet {
private:
    enum State {
        IDLE,
        COUNTING,
        RESULT
    };

    State state = IDLE;
    bool wasPressing = false;
    uint64_t holdStartUs = 0;
    uint64_t holdDurationUs = 0;
    uint32_t resultDisplayTime = 0;

    static void formatSeconds4(uint64_t micros, char* buffer, size_t bufferSize);
    static void formatDeviation4(int64_t diffMicros, char* buffer, size_t bufferSize);
    void drawIdle() const;
    void drawCounting(uint32_t nowMs) const;
    void drawResult() const;
    bool isActionPressed() const;

public:
    void init() override;
    void update() override;
    void draw() override;
    void cleanup() override;

    void onActionClick() override;
    void onBothHeld() override;
};

#endif // TIMING_GAME_APPLET_H
