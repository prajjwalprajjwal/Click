#pragma once

#include <Arduino.h>
#include "PinConfig.h"

struct SoundNote {
    uint16_t freq;    // Frequency in Hz (0 = rest/silence)
    uint16_t durMs;   // Duration in ms
};

/**
 * @brief Non-blocking Audio Sound Effects Manager.
 * Driven by hardware buzzer (GP27).
 */
class SoundFX {
public:
    static const uint8_t MAX_NOTES = 8;

    static void init() {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        pinMode(BUZZER_PIN, OUTPUT);
        noTone(BUZZER_PIN);
#endif
        seqActive = false;
        seqIndex = 0;
        seqCount = 0;
    }

    static void playTone(uint16_t freq, uint16_t durMs) {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        stopSequence();
        tone(BUZZER_PIN, freq);
        currentToneEndMs = millis() + durMs;
        isPlayingTone = true;
#endif
    }

    static void stop() {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        stopSequence();
        noTone(BUZZER_PIN);
        isPlayingTone = false;
#endif
    }

    static bool isPlaying() {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        return seqActive || isPlayingTone;
#else
        return false;
#endif
    }

    static void playSequence(const SoundNote* notes, uint8_t count) {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        if (!notes || count == 0) return;
        if (count > MAX_NOTES) count = MAX_NOTES;
        for (uint8_t i = 0; i < count; i++) {
            sequence[i] = notes[i];
        }
        seqCount = count;
        seqIndex = 0;
        seqActive = true;
        startNote(0);
#endif
    }

    // --- Sound Presets ---

    // Quick tactile click / UI navigation
    static void playClick() {
        playTone(2800, 10);
    }

    // Sisyphus forward push heave (silent or subtle)
    static void playSisyphusPush() {
        // Kept for backward compatibility if needed
    }

    // Sisyphus downhill boulder roll rumble tick
    static void playSisyphusRollTick() {
        // Kept for backward compatibility if needed
    }

    // Sisyphus starts rolling downhill: short 3 beeps
    static void playSisyphusDownhill() {
        static const SoundNote downhillNotes[] = {
            {1046, 25}, // High C6
            {0, 20},    // Pause
            {880, 25},  // A5
            {0, 20},    // Pause
            {784, 35}   // G5
        };
        playSequence(downhillNotes, 5);
    }

    // Sisyphus reaches bottom: light pleasant chime
    static void playSisyphusBottomChime() {
        static const SoundNote chimeNotes[] = {
            {1318, 35}, // E6
            {0, 15},
            {1760, 65}  // A6 light bell chime
        };
        playSequence(chimeNotes, 3);
    }

    // Flappy Bird: wing flap chirp
    static void playFlap() {
        static const SoundNote flapNotes[] = {
            {900, 15},
            {1400, 20}
        };
        playSequence(flapNotes, 2);
    }

    // Flappy Bird: pipe passed score chime
    static void playScore() {
        static const SoundNote scoreNotes[] = {
            {1046, 40}, // C6
            {1318, 65}  // E6
        };
        playSequence(scoreNotes, 2);
    }

    // Flappy Bird: crash / game over slide
    static void playGameOver() {
        static const SoundNote crashNotes[] = {
            {320, 60},
            {220, 80},
            {130, 160}
        };
        playSequence(crashNotes, 3);
    }

    // Just Ten: counting start tick
    static void playJustTenStart() {
        playTone(880, 20);
    }

    // Just Ten: Action button released (standard release tone)
    static void playJustTenRelease() {
        static const SoundNote releaseNotes[] = {
            {1318, 40}, // E6
            {0, 20},    // Rest
            {988, 65}   // B5
        };
        playSequence(releaseNotes, 3);
    }

    // Just Ten: Within 5% Accuracy Celebration Fanfare!
    static void playCelebrationFanfare() {
        static const SoundNote fanfareNotes[] = {
            {523, 80},  // C5
            {659, 80},  // E5
            {784, 80},  // G5
            {0, 30},    // Rest
            {1046, 260} // C6 triumphant sustain
        };
        playSequence(fanfareNotes, 5);
    }

    // Startup Chime
    static void playStartup() {
        static const SoundNote bootNotes[] = {
            {1046, 45},
            {1318, 45},
            {1568, 90}
        };
        playSequence(bootNotes, 3);
    }

    // Non-blocking update loop called from main loop()
    static void update() {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        uint32_t now = millis();
        if (seqActive) {
            if (now >= noteEndMs) {
                seqIndex++;
                if (seqIndex < seqCount) {
                    startNote(seqIndex);
                } else {
                    stopSequence();
                    noTone(BUZZER_PIN);
                }
            }
        } else if (isPlayingTone && now >= currentToneEndMs) {
            isPlayingTone = false;
            noTone(BUZZER_PIN);
        }
#endif
    }

private:
    static inline SoundNote sequence[MAX_NOTES];
    static inline uint8_t seqCount = 0;
    static inline uint8_t seqIndex = 0;
    static inline uint32_t noteEndMs = 0;
    static inline bool seqActive = false;

    static inline bool isPlayingTone = false;
    static inline uint32_t currentToneEndMs = 0;

    static void startNote(uint8_t idx) {
#if defined(BUZZER_PIN) && BUZZER_PIN >= 0
        if (idx >= seqCount) return;
        uint16_t freq = sequence[idx].freq;
        uint16_t dur = sequence[idx].durMs;
        if (freq == 0) {
            noTone(BUZZER_PIN);
        } else {
            tone(BUZZER_PIN, freq);
        }
        noteEndMs = millis() + dur;
#endif
    }

    static void stopSequence() {
        seqActive = false;
        seqCount = 0;
        seqIndex = 0;
    }
};
