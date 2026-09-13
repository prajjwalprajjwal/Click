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
- `platformio.ini` (board definitions, upload speeds, framework versions, build flags).
- **Core System Files** (`firmware/src/system/`):
  - `system/Display.h` (Hardware I2C: SDA on `GPIO 21`, SCL on `GPIO 22`). **NEVER** use `GPIO 36` (VP) for I2C clock.
  - `system/InputManager.h` / `.cpp` (Button debouncing & hold state machine).
  - `system/battery.hpp` (Battery ADC on `GPIO 35`, Charger STAT on `GPIO 33`).
  - `system/PowerManager.h` (UART pad hold isolation, sleep power domains, 9-cycle I2C bus recovery pulse).
  - `system/OSManager.h` / `.cpp` (Applet lifecycle, screensaver & sleep timeouts).
  - `system/Applet.h` (Base class contract for all applets).
- **Active Prototyping Hardware (Click 1: ESP32-WROOM-32E)**:
  - *Note*: All current code and builds in `firmware/` run on this physical prototype board.
  - Microcontroller: ESP32-WROOM-32E (Xtensa Dual-Core 240MHz, 4MB Flash)
  - Display I2C: SDA = GPIO 21, SCL = GPIO 22 (400kHz Fast I2C; *NEVER GPIO 36*)
  - User Inputs: MODE = GPIO 14, ACTION = GPIO 32 (RTC wakeup inputs)
  - Power & Battery: STAT = GPIO 33 (Active LOW), BAT_ADC = GPIO 35 (100k/100k divider)
  - Power Isolation: UART TX0 = GPIO 1, UART RX0 = GPIO 3 (isolated during sleep)
- **Upcoming Hardware Revision (Click 4 / V4 PCB)**:
  - *Note*: PCB design files in `hardware/PCB/Click4/` pending fabrication from JLCPCB.
  - Microcontroller: RP2354A (QFN-56 package, 30 GPIOs: GP0-GP29)
  - Display I2C: SDA = GP20, SCL = GP21
  - User Inputs: MODE = GP14, ACTION = GP32 (or assigned GP)
  - Power & Battery: STAT = GP3 (or assigned GP), BAT_ADC = GP29 (ADC3)
  - Audio & LEDs: Buzzer = GP27, APA102 Data = GP11, APA102 Clock = GP12
  - Voltage Regulator: AP2112K-3.3 with EN tied to physical slide switch

### C. Web Flasher Core Engine
- `web_flasher/index.html` & `web_flasher/flasher.html` (core WebSerial bindings and `<esp-web-install-button>` logic).
- `web_flasher/app.js` (dynamic manifest resolution and eFuse MAC query engine).
- `tools/flasher/serve_flasher.py` & `run_flasher.bat`.

---

## 2. Core Architectural & Firmware Constraints

1. **Number Formatting Integrity**:
   - Scores and click counts must **always display as full integer values** with commas (e.g. `9,999`, `10,000`, `10,001`, `12,345`).
   - **Never truncate or abbreviate** numbers using suffixes like `10K`, `10k+`, or `1M`. The screen layout is explicitly engineered to render full numeric strings.
2. **FreeRTOS Watchdog (TWDT) Compliance**:
   - The `loop()` function in `firmware/src/main.cpp` must maintain `delay(2)` to yield execution to the FreeRTOS scheduler on Core 1. Removing this delay starves the `IDLE1` task and triggers hardware Task Watchdog Timer panics.
3. **Power Management & Sleep Safety**:
   - In `PowerManager.h`, UART console pins must be isolated via `gpio_hold_en` before entering light sleep to prevent back-powering USB-to-UART converter ICs (e.g. CH340).
   - Display I2C pins (SDA/SCL) must be held with pull-ups (`GPIO_PULLUP_ONLY`) rather than driven LOW to avoid latching the SSD1306 OLED controller.
   - Upon wake, a 9-clock-cycle SCL pulse train must precede `Wire.begin()` to recover hung I2C bus slaves.
4. **Asset Build Pipeline**:
   - All screen graphics must be placed in `firmware/assets/screens/` as 128×64 1-bit PNG images.
   - Never edit files in `firmware/src/generated_assets/` directly. They are regenerated automatically on every build by `firmware/scripts/pre_build_assets.py`.

---

## 3. Release & Flashing Workflow Boundary

- **NEVER manually create, delete, edit, or overwrite files inside `releases/`**.
- All official versioning, binary packaging, manifest generation, and multi-version retention must be performed **exclusively** through `build_release.bat` or `python tools/release/release_manager.py`.
- For desktop flashing, use `flash_device.bat` or `firmware/scripts/flash.sh` to ensure safe offset handling.
- Application updates must be written to offset `0x10000` (`app0`) to preserve the `0x9000` NVS partition.

---

## 4. Repository Layout & Purpose

```text
Click/
├── platformio.ini                    # Build configurations & asset pre-hooks
├── hardware/                         # Schematics, PCBs & artwork
│   ├── PCB/
│   │   ├── Click1/                   # Active prototype KiCad PCB (ESP32-WROOM-32E)
│   │   └── Click4/                   # Upcoming JLCPCB PCBA design (RP2354A V0.0.1)
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
│       ├── applets/                  # Isolated, plug-and-play applets
│       ├── fonts/                    # GFX typography
│       └── generated_assets/         # Auto-generated bitmaps
├── web_flasher/                      # Browser WebSerial updater
│   ├── index.html                    # Web flasher UI
│   ├── app.js                        # Device discovery & manifest controller
│   └── manifest.json                 # ESP Web Tools manifest
├── tools/                            # Python build tools & local servers
│   ├── flasher/                      # CLI flasher & local web server
│   ├── release/                      # Interactive semver release manager
│   └── converters/                   # Asset & font converters
├── docs/                             # Architecture & hardware specs
│   ├── hardware_and_pinouts.md       # Full electrical pinouts (ESP32 & RP2354A)
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

## 5. Standard Development & Testing Workflow

When developing or debugging new features:

1. **Compile Check** (automatically runs asset conversion pipeline):
   ```bash
   pio run -e esp32doit-devkit-v1
   ```
2. **Direct Serial Upload**:
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
