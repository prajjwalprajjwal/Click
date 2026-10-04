#pragma once

#include <Arduino.h>

class HardwareDiagnostics {
public:
    static void run();

    static void printMenu();
    static void testBuzzer();
    static void testRGBLEDs();
    static void testOLED();
    static void monitorButtonsAndPower();
    static void scanI2CBus();
    static void testBatteryAndCharger();
    static void printSystemInfo();
    static void runAll();
};
