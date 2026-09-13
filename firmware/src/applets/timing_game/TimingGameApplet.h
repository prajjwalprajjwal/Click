#ifndef TIMING_GAME_APPLET_H
#define TIMING_GAME_APPLET_H

#include <stdint.h>
#include <stddef.h>
#include <Preferences.h>
#include "Applet.h"

// Automatically includes pre-converted user PNG screen assets
#if __has_include("generated_assets/all_assets.h")
  #include "generated_assets/all_assets.h"
#endif

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
    Preferences prefs;
    uint32_t bestTimeUs = 0;

    static void formatSeconds4(uint64_t micros, char* buffer, size_t bufferSize);
    static void formatDeviation4(int64_t diffMicros, char* buffer, size_t bufferSize);
    void drawIdle() const;
    void drawCounting(uint32_t nowMs) const;
    void drawResult() const;
    bool isActionPressed() const;

public:
    void init() override;
    void preloadState();
    uint32_t getBestTimeUs() const { return bestTimeUs; }
    double getBestTimeSec() const { return static_cast<double>(bestTimeUs) / 1000000.0; }
    uint32_t getBestDeviationMs() const {
        if (bestTimeUs == 0) return 0;
        int64_t diffUs = std::abs(static_cast<int64_t>(bestTimeUs) - 10000000LL);
        return static_cast<uint32_t>(diffUs / 1000);
    }
    void setBestTimeUs(uint32_t us) {
        if (us == 0) return;
        int64_t currentDiff = std::abs(static_cast<int64_t>(us) - 10000000LL);
        int64_t bestDiff = (bestTimeUs == 0) ? -1 : std::abs(static_cast<int64_t>(bestTimeUs) - 10000000LL);
        if (bestTimeUs == 0 || currentDiff < bestDiff) {
            bestTimeUs = us;
            prefs.begin("click_stats", false);
            prefs.putUInt("just_ten_time", bestTimeUs);
            prefs.end();
        }
    }
    void update() override;
    void draw() override;
    void cleanup() override;

    void onActionClick() override;
    void onBothHeld() override;
};

#endif // TIMING_GAME_APPLET_H
