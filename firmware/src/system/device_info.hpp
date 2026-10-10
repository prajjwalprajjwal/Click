#pragma once
#include <Arduino.h>
#include <Preferences.h>
#if defined(ESP32)
#include <esp_system.h>
#elif defined(ARDUINO_ARCH_RP2040)
#include <pico/unique_id.h>
#endif
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
#if defined(ESP32)
        uint64_t chipid = ESP.getEfuseMac();
        char idStr[13];
        snprintf(idStr, sizeof(idStr), "%04X%08X", 
                 (uint16_t)(chipid >> 32), 
                 (uint32_t)chipid);
        return String(idStr); // Returns a clean 12-char hex ID like "A4CF1289BC01"
#elif defined(ARDUINO_ARCH_RP2040)
        pico_unique_board_id_t id;
        pico_get_unique_board_id(&id);
        char idStr[17];
        snprintf(idStr, sizeof(idStr), "%02X%02X%02X%02X%02X%02X%02X%02X",
                 id.id[0], id.id[1], id.id[2], id.id[3],
                 id.id[4], id.id[5], id.id[6], id.id[7]);
        return String(idStr);
#else
        return String("CLICK00000000");
#endif
    }

    static String getFormattedMac() {
#if defined(ESP32)
        uint64_t chipid = ESP.getEfuseMac();
        uint8_t* mac = (uint8_t*)&chipid;
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(macStr);
#elif defined(ARDUINO_ARCH_RP2040)
        pico_unique_board_id_t id;
        pico_get_unique_board_id(&id);
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5]);
        return String(macStr);
#else
        return String("00:00:00:00:00:00");
#endif
    }

    static const char* getCustomName() {
        static char nameBuf[CLICK_NAME_MAX_LEN + 1] = {0};

        // 1. Check if binary rodata signature has a custom flashed name that differs from default "CLICKER"
        bool hasPatchedName = false;
        char patchedName[CLICK_NAME_MAX_LEN + 1] = {0};
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
            if (valid && strcmp(g_device_name_signature.name, "CLICKER") != 0) {
                strncpy(patchedName, g_device_name_signature.name, CLICK_NAME_MAX_LEN);
                patchedName[CLICK_NAME_MAX_LEN] = '\0';
                hasPatchedName = true;
            }
        }

        // 2. Read persistent NVS storage
        Preferences prefs;
        String nvsName = "";
        if (prefs.begin("clicker_cfg", true)) {
            nvsName = prefs.getString("custom_name", "");
            prefs.end();
        }

        // If the binary was explicitly flashed with a brand-new custom name,
        // persist it into NVS immediately so future reflashes without a name never erase it!
        if (hasPatchedName) {
            if (nvsName != patchedName) {
                setCustomName(patchedName);
            }
            strncpy(nameBuf, patchedName, CLICK_NAME_MAX_LEN);
            nameBuf[CLICK_NAME_MAX_LEN] = '\0';
            return nameBuf;
        }

        // 3. Fallback to NVS preference if available (PRESERVE custom name across unpatched reflashes!)
        if (nvsName.length() > 0 && nvsName != "CLICKER") {
            strncpy(nameBuf, nvsName.c_str(), CLICK_NAME_MAX_LEN);
            nameBuf[CLICK_NAME_MAX_LEN] = '\0';
            return nameBuf;
        }

        // 4. Default fallback: Unique name derived from chip ID instead of generic "CLICKER"
        String uid = getID();
        String defaultName = "Click-" + (uid.length() >= 4 ? uid.substring(uid.length() - 4) : uid);
        strncpy(nameBuf, defaultName.c_str(), CLICK_NAME_MAX_LEN);
        nameBuf[CLICK_NAME_MAX_LEN] = '\0';
        return nameBuf;
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
