#pragma once
#include <Arduino.h>

enum ButtonEvent {
    NONE,
    MODE_CLICK,
    MODE_HOLD_5S,
    ACTION_CLICK,
    ACTION_HOLD_3S
};

class InputHandler {
private:
    uint32_t modePressStart = 0;
    bool modeHandled = false;

    uint32_t actionPressStart = 0;
    bool actionHandled = false;

public:
    ButtonEvent update(int modePin, int actionPin) {
        uint32_t now = millis();

        // -----------------------------------------
        // MODE Button Handler (Click vs 5s Hold)
        // -----------------------------------------
        if (digitalRead(modePin) == LOW) {
            if (modePressStart == 0) {
                modePressStart = now;
                modeHandled = false;
            } else if ((now - modePressStart >= 5000) && !modeHandled) {
                modeHandled = true;
                return MODE_HOLD_5S;
            }
        } else {
            if (modePressStart > 0 && !modeHandled) {
                modePressStart = 0;
                return MODE_CLICK;
            }
            modePressStart = 0;
        }

        // -----------------------------------------
        // ACTION Button Handler (Click vs 3s Hold)
        // -----------------------------------------
        if (digitalRead(actionPin) == LOW) {
            if (actionPressStart == 0) {
                actionPressStart = now;
                actionHandled = false;
            } else if ((now - actionPressStart >= 3000) && !actionHandled) {
                actionHandled = true;
                return ACTION_HOLD_3S;
            }
        } else {
            if (actionPressStart > 0 && !actionHandled) {
                actionPressStart = 0;
                return ACTION_CLICK;
            }
            actionPressStart = 0;
        }

        return NONE;
    }
};
