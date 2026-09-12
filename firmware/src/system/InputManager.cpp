#include "InputManager.h"

InputManager::InputManager() = default;

void InputManager::init() {
    pinMode(ACTION_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
}

void InputManager::update() {
    uint32_t now = millis();

    bool actionCurrentlyPressed = digitalRead(ACTION_BUTTON_PIN) == LOW;
    bool modeCurrentlyPressed = digitalRead(MODE_BUTTON_PIN) == LOW;
    bool bothCurrentlyPressed = actionCurrentlyPressed && modeCurrentlyPressed;

    if (!bothCurrentlyPressed) {
        bothPressedTime = 0;
        if (!actionCurrentlyPressed && !modeCurrentlyPressed) {
            bothHeldReported = false;
        }
    } else if (bothPressedTime == 0) {
        bothPressedTime = now;
    }

    if (actionCurrentlyPressed && !actionPressed) {
        actionPressed = true;
        actionPressedTime = now;
        actionLongHeldReported = false;
    } else if (!actionCurrentlyPressed && actionPressed) {
        actionPressed = false;
        if (!actionLongHeldReported && !bothHeldReported && !modeCurrentlyPressed && onActionClick) {
            onActionClick();
        }
        actionLongHeldReported = false;
    } else if (actionPressed && !modeCurrentlyPressed && !actionLongHeldReported &&
               (now - actionPressedTime) >= ACTION_HOLD_THRESHOLD_MS) {
        actionLongHeldReported = true;
        if (onActionHold) {
            onActionHold();
        }
    }

    if (modeCurrentlyPressed && !modePressed) {
        modePressed = true;
        modePressedTime = now;
        modeLongHeldReported = false;
    } else if (!modeCurrentlyPressed && modePressed) {
        modePressed = false;
        if (!modeLongHeldReported && !bothHeldReported && !actionCurrentlyPressed && onModeClick) {
            onModeClick();
        }
        modeLongHeldReported = false;
    } else if (modePressed && !actionCurrentlyPressed && !modeLongHeldReported &&
               (now - modePressedTime) >= MODE_HOLD_THRESHOLD_MS) {
        modeLongHeldReported = true;
        if (onModeHold) {
            onModeHold();
        }
    }

    if (bothCurrentlyPressed && !bothHeldReported &&
        bothPressedTime != 0 && (now - bothPressedTime) >= BOTH_HOLD_THRESHOLD_MS) {
        bothHeldReported = true;
        if (onBothHeld) {
            onBothHeld();
        }
    }
}
