#pragma once
#include <Arduino.h>
#include <esp_system.h>

class DeviceInfo {
public:
    static String getID() {
        uint64_t chipid = ESP.getEfuseMac();
        char idStr[13];
        snprintf(idStr, sizeof(idStr), "%04X%08X", 
                 (uint16_t)(chipid >> 32), 
                 (uint32_t)chipid);
        return String(idStr); // Returns a clean 12-char hex ID like "A4CF1289BC01"
    }

    static String getFormattedMac() {
        uint64_t chipid = ESP.getEfuseMac();
        uint8_t* mac = (uint8_t*)&chipid;
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
    }
};
