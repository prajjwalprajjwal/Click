# Project Rules & AI Agent Guidelines

> [!IMPORTANT]
> **STRICT COMPLIANCE REQUIRED FOR ALL AI ASSISTANTS & AUTOMATED AGENTS**  
> These rules protect hardware configurations, non-volatile storage (NVS) memory partitions, display timings, release integrity, and hardware pin mappings for the Clicker project.

---

## 1. Protected Subsystems (CODE FREEZE - DO NOT MODIFY WITHOUT EXPLICIT CONSENT)

The following files and subsystems are locked and **must not be edited, refactored, or overwritten** unless the user explicitly requests changes in a specific prompt:

### A. NVS Storage & Persistent State
- `firmware/src/applets/clicker/ClickCounter.h` / `firmware/src/applets/clicker/ClickCounter.cpp`
- `firmware/src/applets/clicker/MilestoneManager.h` / `firmware/src/applets/clicker/MilestoneManager.cpp`
- `firmware/src/applets/clicker/PersistenceManager.h` / `firmware/src/applets/clicker/PersistenceManager.cpp`
- Any storage retention logic utilizing `Preferences.h` (dual-key `nvs_a`/`nvs_b` backup, checksum validation, wear-leveling, NVS namespaces).

### B. Hardware Pin Profiles & Core System
- `platformio.ini` (board definitions, upload speeds, framework versions, build flags). Default build environment is `env:rp2354`.
- **Core System Files** (`firmware/src/system/`):
  - `system/PinConfig.h`: Authoritative pin routing for both RP2354 (primary) and ESP32 (legacy prototype).
  - `system/Display.h`: Hardware I2C display singleton (`Wire1` on RP2354, `Wire` on ESP32 at 400kHz). **NEVER** use `GPIO 36` (VP) for I2C clock on ESP32.
  - `system/InputManager.h` / `.cpp`: Debounced button state machine & hold triggers (`isActionButtonPressed()`, `isModeButtonPressed()`).
  - `system/battery.hpp`: Multi-sample trimmed mean ADC filtering, non-linear Li-Ion SoC curve, and ETA6003 charge state monitoring.
  - `system/PowerManager.h`: Micro-power sleep orchestration, same-button double-click wake protection (500ms window on the same key to reject accidental single presses or cross-key pocket bumps), 12MHz direct XOSC down-clocking in deep sleep (< 1mA), I2C bus preservation (0µA idle), ARM Cortex-M33 `__wfi()` sleep, and WS2812/Buzzer grounding clamps.
  - `system/WS2812.h`: PIO state machine driver for 2× WS2812/SK6812 addressable RGB LEDs on `GP11`, with charging breathe vs in-game priority management and sleep clamp.
  - `system/SoundFX.h`: Hardware audio buzzer driver on `GP27` (driven via MMBT3904 NPN BJT).
  - `system/HardwareDiagnostics.h` / `.cpp`: Comprehensive automated & interactive hardware diagnostic test suite (`CLICK_HW_TEST_MENU=1`).
  - `system/device_info.hpp` / `.cpp`: Hardware silicon identification (RP2354 16-hex Unique Board ID / ESP32 12-hex eFuse MAC) & custom bootscreen name personalization.
  - `system/OSManager.h` / `.cpp`: Applet scheduler, 3-stage sleep lifecycle (8s idle dimming to contrast 0x05, 15s screen off, 30s true deep sleep with 12MHz down-clocking).
  - `system/Applet.h`: Base class interface contract for all applets.

- **Primary Production Hardware (Click 4 PCBA - Raspberry Pi RP2354A)**:
  - *Authoritative Reference*: [`firmware/src/system/PinConfig.h`](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/system/PinConfig.h) & [`docs/hardware_and_pinouts.md`](file:///Users/prajjwal/Documents/GitHub/Click/docs/hardware_and_pinouts.md)
  - *Status*: Primary active production platform. Default PlatformIO environment: `env:rp2354`. PCBA design in `hardware/PCB/Click4/` (`Click4.kicad_sch`, `Click4.kicad_pcb`, `Click4.step`).
  - Microcontroller: Raspberry Pi RP2354A (QFN-56 package, 30 GPIOs: GP0–GP29, Dual ARM Cortex-M33 @ 48MHz/150MHz, 512KB SRAM, 4MB on-chip QSPI Flash).
  - Display & RTC I2C Bus (`CLICK_I2C = Wire1`):
    - **SDA**: `GP2` (I2C1 SDA, Pin 4)
    - **SCL**: `GP3` (I2C1 SCL, Pin 5)
    - Bus Speed: 400kHz Fast I2C
    - Peripherals: 128×64 SSD1306 OLED (`0x3C`) + NXP PCF8563 Real-Time Clock (`0x51`)
  - User Inputs:
    - **MODE Button (`S1`)**: `GP0` (Active LOW, internal pull-up, level-sensitive wake capable)
    - **ACTION Button (`S2`)**: `BOOTSEL` / `QSPI_SS` (Active LOW, dual-purpose game click & bootloader pin)
  - Power & Battery:
    - **Battery Voltage Sense**: `GP29` (ADC3 / Pad 41) via 100kΩ / 100kΩ divider (2.0x factor, 12-bit ADC)
    - **Charger Status (`STAT`)**: `GP1` (Pad 3) driven by ETA6003 `STAT` pin (Active LOW while charging, 10kΩ pullup `R9` to +3V3)
    - **Battery Charger**: ETA6003Q3Q switching Li-Ion charging controller (`U5`) with 2.2µH inductor (`L3`)
    - **Voltage Regulator**: Diodes Inc. AP2112K-3.3 (`U2`) 600mA LDO (Hardware slide switch `SW1` directly toggles `EN` line)
  - Visual & Auditory Feedback:
    - **RGB Lighting**: 2× SK6812/WS2812 addressable RGB LEDs on `GP11` (DIN) driven by PIO state machine
    - **Audio Buzzer**: Magnetic buzzer driven via MMBT3904 NPN BJT on `GP27`
  - USB Interface:
    - Native USB 2.0 Full Speed on 16-pin Type-C receptacle (`J1`) with 5.1kΩ CC resistors.
    - Native CDC serial telemetry & WebSerial flashing at 115200 baud, Picotool USB boot reset support (`ENABLE_PICOTOOL_USB=1`).

- **Legacy Prototyping Hardware (Click 1 Prototype - ESP32-WROOM-32E)**:
  - *Status*: Maintained for backward compatibility under `env:esp32doit-devkit-v1`. KiCad files in `hardware/PCB/Click1/`.
  - Microcontroller: ESP32-WROOM-32E (Xtensa Dual-Core 240MHz, 4MB Flash).
  - Display I2C: SDA = `GPIO 21`, SCL = `GPIO 22` (`Wire`, 400kHz Fast I2C; *NEVER GPIO 36*).
  - User Inputs: MODE = `GPIO 14` (RTC ext0 wake), ACTION = `GPIO 32` (RTC ext1 wake).
  - Power & Battery: STAT = `GPIO 33` (Active LOW), BAT_ADC = `GPIO 35` (100k/100k divider).
  - Power Isolation: UART TX0 = `GPIO 1`, UART RX0 = `GPIO 3` (isolated with pad holds in sleep).

### C. Web Flasher Core Engine
- `web_flasher/index.html` & `web_flasher/flasher.html` (core WebSerial bindings and `<esp-web-install-button>` logic).
- `web_flasher/app.js` (dynamic manifest resolution and eFuse MAC query engine).
- `tools/flasher/serve_flasher.py` & `run_flasher.bat`.

---

## 2. Locked Low-Level Firmware, Power & Wake Architecture (IMMUTABLE CORE)

> [!CAUTION]
> **STRICT IMMUTABILITY DIRECTIVE — DO NOT TOUCH OR REFACTOR**  
> The power management, wake mechanisms, clock frequencies, peripheral grounding clamps, and sleep cycles specified below are **FROZEN IN SILICON LOGIC**.
> 
> 1. **Build On Top Only**: All future firmware changes, new applets, features, UI updates, network sync routines, animations, and graphics MUST be built **strictly on top of these foundations without replacing, modifying, or degrading them**.
> 2. **Explicit User Prompt Requirement**: AI agents and developers are **strictly prohibited** from altering or refactoring any code in `PowerManager.h`, `OSManager.h` (sleep/wake methods), `InputManager.h`/`.cpp`, or low-level sleep hooks unless the user **specifically and explicitly instructs to change a power or wake setting in that specific prompt**.
> 3. **Surgical Precision Rule**: If the user explicitly requests a change to power or wake settings, you must change **ONLY the exact parameter or logic specifically requested**. Do NOT rewrite, simplify, clean up, or touch any other low-level mechanisms.

### A. 3-Stage Sleep Lifecycle Profile
- **Stage 1 (8 seconds idle)**: OLED Contrast Dimming to `0x05`. Drops display current draw by ~60% while keeping content legible.
- **Stage 2 (15 seconds idle)**: Display Panel Standby (`lightSleepTimeout = 15000`). Sends `SSD1306_DISPLAYOFF`. MCU remains responsive.
- **Stage 3 (30 seconds idle)**: True Deep Sleep (`deepSleepTimeout = 30000`). Full micro-power sleep orchestration with down-clocking and ARM `__wfi()` loops (< 1mA total standby current).
- **USB Charging Override**: When plugged into USB power (`BatteryManager::isCharging() == true`), the device never enters deep sleep so the multi-color charging breathe LED remains active; the OLED turns off at 15s to prevent burn-in.

### B. Same-Button Double-Click Wake Protection
- **Timeout Window**: Exactly **500ms** (`DOUBLE_CLICK_WINDOW_MS = 500`).
- **Same-Button Verification**: Waking requires double-clicking the **SAME button** (`MODE`-`MODE` or `ACTION`-`ACTION`).
- **Cross-Key Cancellation**: Pressing one key followed by the other key (e.g. `MODE` then `ACTION`, or `ACTION` then `MODE`) is detected as accidental contact (e.g. in a pocket or bag). The wake sequence is immediately cancelled and the MCU remains asleep.
- **Silent First Click**: The first click wakes the MCU core internally, but does **NOT** turn on the OLED display, WS2812 LEDs, or buzzer. Peripherals power up only upon confirmation of the second click on the same key within 500ms.
- **Continuous Hold Rejection**: Holds lasting longer than 3 seconds are rejected to prevent continuous pressure in pockets from waking the device.
- **Direct Sisyphus Return**: Waking from sleep always returns directly to the default Sisyphus Clicker game (Applet index 0) with a clean display buffer refresh.

### C. Low-Power Deep Sleep Hardware State (< 1mA)
- **Direct 12MHz XOSC Down-Clocking**: On sleep entry, the system clock is stepped down from 48MHz/150MHz directly to 12MHz direct crystal via `set_sys_clock_khz(12000, false)`. On confirmed wake, the clock is restored to 48MHz (`set_sys_clock_khz(48000, false)`).
- **CPU Sleep**: Executes ARM Cortex-M33 `__wfi()` (Wait For Interrupt) inside low-frequency polling loops.
- **Zero-Leakage Peripheral Clamping**:
  - **WS2812 RGB LEDs (`GP11`)**: `WS2812Driver::clearAndHaltForSleep()` latches zeros, de-muxes the pin to SIO GPIO, drives 0V ground, and enables the internal pull-down resistor to prevent ghost lighting and parasitic drain.
  - **Audio Buzzer (`GP27`)**: Buzzer output pin is driven to 0V ground with internal pull-down enabled to prevent any leakage through the MMBT3904 NPN BJT driver.
  - **Unused GPIOs**: Digital CMOS input buffers on unused Bank 0 GPIOs are disabled to eliminate floating gate leakage.
  - **ADC Shutdown**: Battery ADC is powered down during sleep.
  - **RTC Clockout**: PCF8563 RTC continuous 32.768kHz square-wave output on `CLKOUT` is disabled (saving ~15µA).

### D. I2C Bus Preservation Across Sleep & Wake (`Wire1`)
- **Bus Integrity**: On RP2354, `CLICK_I2C` (`Wire1` on `GP2` SDA, `GP3` SCL) must remain initialized and held HIGH by external 4.7kΩ pullups to 3.3V (0µA idle quiescent current).
- **NEVER Reset Bus on Sleep/Wake**: Never call `clearI2CBus()`, bit-bang SCL, or re-invoke `display.begin()` during sleep/wake transitions. Doing so desynchronizes the SSD1306 controller while powered, causing complete device lockups that require toggling the physical ON/OFF switch (`SW1`).
- **Instant Display Restore**: Sleep issues `SSD1306_DISPLAYOFF` (< 5µA OLED panel standby), and wake issues `SSD1306_DISPLAYON` and re-applies low-power clock divider registers in $< 1\text{ms}$.

### E. Battery & Charging Architecture
- **Hardware Charge Sense**: ETA6003 `STAT` pin on `GP1` (Pad 3) with 10kΩ external pullup `R9` is the single source of truth for battery charging status (Active LOW while charging). USB silicon SIE register polling must NOT be used.
- **Battery Sensing**: 100kΩ / 100kΩ voltage divider on `GP29` (ADC3) with multi-sample trimmed mean filtering and non-linear Li-Ion SoC plateau mapping in `battery.hpp`.

---

## 3. Core Architectural & Firmware Constraints

1. **Number Formatting Integrity**:
   - Scores and click counts must **always display as full integer values** with commas (e.g. `9,999`, `10,000`, `10,001`, `12,345`).
   - **Never truncate or abbreviate** numbers using suffixes like `10K`, `10k+`, or `1M`. The screen layout is explicitly engineered to render full numeric strings.
2. **Dynamic Low-Power Frame Pacing & RTOS Yielding**:
   - The `loop()` function in `firmware/src/main.cpp` enforces frame pacing capped to ~30 FPS (33ms). Sleeping during idle frame time invokes ARM Cortex-M33 `__wfi()` (Wait For Interrupt) on RP2354A, significantly cutting active MCU power.
   - For ESP32 builds, `delay(2)` yields execution to the FreeRTOS scheduler on Core 1 to prevent Task Watchdog Timer (TWDT) panics on `IDLE1`.
3. **Power Management & Sleep Safety**:
   - **RP2354A Sleep Architecture**: Operates under Section 2 above: 500ms same-button double-click wake, 12MHz direct XOSC down-clocking with ARM `__wfi()` (< 1mA standby current), and return directly to the Sisyphus game screen.
   - **ESP32 Sleep Safety**: In `PowerManager.h`, UART console pins must be isolated via `gpio_hold_en` before entering light sleep to prevent back-powering CH340. Display I2C pins (SDA/SCL) must be held with pull-ups (`GPIO_PULLUP_ONLY`) rather than driven LOW. A 9-clock-cycle SCL pulse train must precede `Wire.begin()` on wake to clear stuck slaves.
4. **Asset Build Pipeline**:
   - All screen graphics must be placed in `firmware/assets/screens/` as 128×64 1-bit PNG images.
   - Never edit files in `firmware/src/generated_assets/` directly. They are regenerated automatically on every build by `firmware/scripts/pre_build_assets.py`.

---

## 4. Release & Flashing Workflow Boundary

- **NEVER manually create, delete, edit, or overwrite files inside `releases/`**.
- All official versioning, binary packaging, manifest generation, and multi-version retention must be performed **exclusively** through `build_release.bat` or `python tools/release/release_manager.py`.
- For desktop flashing, use `flash_device.bat` or `firmware/scripts/flash.sh` to ensure safe offset handling.
- Application updates must be written to offset `0x10000` (`app0`) to preserve the `0x9000` NVS partition on ESP32, or appropriate flash partition on RP2354.

---

## 5. Repository Layout & Purpose

```text
Click/
├── platformio.ini                    # Build configurations & asset pre-hooks (default: rp2354)
├── hardware/                         # Schematics, PCBs & artwork
│   ├── PCB/
│   │   ├── Click4/                   # Active production PCBA (Raspberry Pi RP2354A)
│   │   ├── Click1/                   # Legacy prototype KiCad PCB (ESP32-WROOM-32E)
│   │   └── Resources/                # Mechanical templates and edge cut DXFs
│   └── vector_designs.ai             # Laser cutting & case artwork
├── server/                           # Cloudflare Worker + D1 Serverless Backend
│   ├── worker.js                     # Edge API routes (/api/leaderboard, /api/sync)
│   ├── schema.sql                    # D1 SQLite schema (devices, global_stats, sync_log)
│   └── wrangler.toml                 # Cloudflare Worker deployment configuration
├── firmware/                         # Embedded C++ firmware
│   ├── assets/screens/               # Raw PNG images (128x64, 1-bit)
│   ├── scripts/                      # Pre-build asset converter & flasher scripts
│   └── src/                          # Application source code
│       ├── main.cpp                  # Entry point & applet registry
│       ├── system/                   # Core OS, drivers & power management
│       │   ├── PinConfig.h           # Authoritative pinouts (RP2354 & ESP32)
│       │   ├── Display.h             # SSD1306 OLED singleton & 400kHz fast I2C
│       │   ├── InputManager.h / .cpp # Button debouncer & unified button readers
│       │   ├── battery.hpp           # Battery ADC, LiPo curve & ETA6003 STAT
│       │   ├── PowerManager.h        # 12MHz XOSC sleep, __wfi(), and bus recovery
│       │   ├── WS2812.h              # 2x SK6812/WS2812 RGB LED driver (GP11 DIN)
│       │   ├── SoundFX.h             # Non-blocking audio buzzer driver (GP27)
│       │   ├── HardwareDiagnostics.h / .cpp # Diagnostics test suite (CLICK_HW_TEST_MENU)
│       │   ├── device_info.hpp / .cpp# Unique board ID & custom bootscreen name
│       │   ├── OSManager.h / .cpp    # Applet scheduler, idle dimming & sleep lifecycle
│       │   └── Applet.h              # Base class interface for all applets
│       ├── applets/                  # Isolated, plug-and-play applets
│       │   ├── clicker/              # Sisyphus uphill clicker game & physics engine
│       │   ├── timing_game/          # "Just 10 Seconds" accuracy timing game
│       │   ├── flappy_bird/          # Flappy Bird arcade obstacle game
│       │   ├── settings/             # Hardware status, voltage & device ID screen
│       │   └── screensaver/          # Snowfall particle animation on idle
│       ├── fonts/                    # GFX typography (Rajdhani font family)
│       ├── screens/                  # Milestone screen renderers
│       └── generated_assets/         # Auto-generated bitmaps
├── web_flasher/                      # Browser WebSerial updater & leaderboard hub
│   ├── index.html                    # Web flasher UI
│   ├── flasher.html                  # Full-featured WebSerial personalizer & sync
│   ├── app.js                        # Device discovery, WebSerial & leaderboard sync
│   ├── manifest.json                 # WebSerial firmware manifest
│   └── versions.json                 # Firmware release catalog
├── tools/                            # Python build tools & local servers
│   ├── flasher/                      # CLI flasher & local web server
│   ├── release/                      # Interactive semver release manager
│   └── converters/                   # Asset & font converters
├── docs/                             # Architecture & hardware specs
│   ├── hardware_and_pinouts.md       # Authoritative electrical pinouts (RP2354A & ESP32)
│   ├── power_management_and_battery_architecture.md # Power, sleep & battery deep-dive
│   ├── device_identification_and_leaderboard.md # Unique ID & DB architecture
│   └── leaderboard_architecture.md   # Cloud sync & API integration
├── build_release.bat                 # [1-Click] Interactive release bumper
├── flash_device.bat                  # [1-Click] Desktop CLI serial flasher
├── run_flasher.bat                   # [1-Click] Launch local web flasher server
├── CHANGELOG.md                      # Master architecture & release history
├── PROJECT_RULES.md                  # Strict guidelines & code freeze rules
└── README.md                         # Master documentation entry point
```

---

## 6. Standard Development & Testing Workflow

When developing or debugging new features:

1. **Compile Check (Active Target: RP2354A)**:
   ```bash
   # Default target (Click 4: Raspberry Pi RP2354A)
   pio run -e rp2354

   # Debug target (with interactive Hardware Diagnostics suite enabled)
   pio run -e rp2354-debug

   # Legacy prototype target (Click 1: ESP32-WROOM-32E)
   pio run -e esp32doit-devkit-v1
   ```
2. **Direct Serial Upload & Monitoring**:
   ```bash
   pio run --target upload
   pio device monitor -b 115200
   ```
3. **Official Release Creation** (Only when instructed by the user):
   ```powershell
   .\build_release.bat
   ```
4. **Git Version Control**:
   ```bash
   git add .
   git commit -m "Descriptive commit message"
   git push
   ```
