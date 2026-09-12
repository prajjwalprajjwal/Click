#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <cstring>

#define CLICK_NAME_MAGIC_PREFIX "__CLICK_NAME__:"
#define CLICK_NAME_MAGIC_SUFFIX ":__END_NAME___"
#define CLICK_NAME_MAX_LEN 32

struct DeviceNameSignature {
    char prefix[16];
    char name[CLICK_NAME_MAX_LEN];
    char suffix[16];
};

extern const DeviceNameSignature g_device_name_signature;

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

    static const char* getCustomName() {
        static char nameBuf[CLICK_NAME_MAX_LEN + 1] = {0};

        // 1. First check if binary rodata signature has a custom flashed name
        if (strncmp(g_device_name_signature.prefix, CLICK_NAME_MAGIC_PREFIX, 15) == 0) {
            bool valid = false;
            for (int i = 0; i < CLICK_NAME_MAX_LEN; ++i) {
                char c = g_device_name_signature.name[i];
                if (c == '\0') {
                    if (i > 0) valid = true;
                    break;
                }
                if (c < 32 || c > 126) break; // non-printable ASCII
            }
            if (valid) {
                strncpy(nameBuf, g_device_name_signature.name, CLICK_NAME_MAX_LEN);
                nameBuf[CLICK_NAME_MAX_LEN] = '\0';
                return nameBuf;
            }
        }

        // 2. Fallback to NVS preference if available
        Preferences prefs;
        if (prefs.begin("clicker_cfg", true)) {
            String nvsName = prefs.getString("custom_name", "");
            prefs.end();
            if (nvsName.length() > 0) {
                strncpy(nameBuf, nvsName.c_str(), CLICK_NAME_MAX_LEN);
                nameBuf[CLICK_NAME_MAX_LEN] = '\0';
                return nameBuf;
            }
        }

        return "CLICKER";
    }

    static void setCustomName(const char* newName) {
        if (!newName || newName[0] == '\0') return;
        Preferences prefs;
        if (prefs.begin("clicker_cfg", false)) {
            prefs.putString("custom_name", newName);
            prefs.end();
        }
    }
};
