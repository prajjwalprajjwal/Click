#include "device_info.hpp"

// Placed in flash rodata with unique prefix and suffix for browser-side binary patching
__attribute__((used)) const DeviceNameSignature g_device_name_signature = {
    CLICK_NAME_MAGIC_PREFIX,
    "CLICKER",
    CLICK_NAME_MAGIC_SUFFIX
};
