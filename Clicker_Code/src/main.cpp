#include <Arduino.h>
#include <Wire.h>
#include "Display.h"
#include "OSManager.h"
#include "HomeApplet.h"
#include "CounterApplet.h"
#include "TimingGameApplet.h"
#include "fonts/ThemeFonts.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);

OSManager osManager;
HomeApplet homeApplet;
CounterApplet counterApplet;
TimingGameApplet timingGameApplet;

void onModeButtonClick() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet) {
        applet->onModeClick();
    }
    osManager.switchToNextApplet();
}

void onActionButtonClick() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet) {
        applet->onActionClick();
    }
}

void onActionButtonHold() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet) {
        applet->onActionHold();
    }
}

void onBothButtonsHeld() {
    osManager.recordActivity();
    Applet* applet = osManager.getCurrentApplet();
    if (applet) {
        applet->onBothHeld();
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (1) {
            delay(1000);
        }
    }

    // Set 400kHz Fast I2C mode for smooth SSD1306 refresh
    Wire.setClock(400000);

    display.clearDisplay();
    ThemeFonts::drawCentered(&Rajdhani24pt7b, "CLICKER", 22);
    ThemeFonts::drawCentered(&Rajdhani12pt7b, "Starting up...", 48);
    display.display();
    delay(1000);

    osManager.registerApplet(&homeApplet);
    osManager.registerApplet(&counterApplet);
    osManager.registerApplet(&timingGameApplet);
    osManager.init();
    counterApplet.preloadState();

    InputManager* inputMgr = osManager.getInputManager();
    inputMgr->onModeClick = onModeButtonClick;
    inputMgr->onActionClick = onActionButtonClick;
    inputMgr->onActionHold = onActionButtonHold;
    inputMgr->onBothHeld = onBothButtonsHeld;
}

void loop() {
    osManager.update();
    osManager.draw();
    delayMicroseconds(1000);
}
