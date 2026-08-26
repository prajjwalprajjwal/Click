#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>

#define MODE_BUTTON_PIN 14
#define ACTION_BUTTON_PIN 32
#define ACTION_HOLD_THRESHOLD_MS 1000
#define MODE_HOLD_THRESHOLD_MS 1000
#define BOTH_HOLD_THRESHOLD_MS 4000

class InputManager {
private:
    uint32_t actionPressedTime = 0;
    uint32_t modePressedTime = 0;
    uint32_t bothPressedTime = 0;
    bool actionPressed = false;
    bool modePressed = false;
    bool actionLongHeldReported = false;
    bool modeLongHeldReported = false;
    bool bothHeldReported = false;

public:
    InputManager();
    void init();
    void update();

    void (*onActionClick)() = nullptr;
    void (*onActionHold)() = nullptr;
    void (*onModeClick)() = nullptr;
    void (*onModeHold)() = nullptr;
    void (*onBothHeld)() = nullptr;
};

#endif // INPUT_MANAGER_H
