# Clicker Project Changelog & Architecture Master Log

> [!NOTE]
> **Living Document & AI Agent Instruction**: This document serves as the master record of all major architectural, memory storage, web updater, hardware revision, and firmware release decisions. AI agents and developers must append new entries to this changelog whenever significant system, storage, hardware, or release infrastructure changes occur.

---

## Core Architectural Decisions

### 1. NVS Partition Storage & Data Preservation Strategy
- **Partition Location**: `0x9000` (length `0x5000` / 20 KB).
- **Application Binary (`app0`)**: `0x10000` (65536 decimal, length `0x140000` / 1.25 MB).
- **Retention Guarantee**: Standard firmware updates strictly write to offset `0x10000` (`app0`). The `0x9000` NVS partition is preserved across updates, retaining all user lifetime clicks, completed cycles, unlocked milestones, and settings via `Preferences.h`.
- **Dual-Key Backup & Wear-Leveling**: Click stats and high scores are preserved using alternating NVS keys (`nvs_a` / `nvs_b`) with CRC checksum verification to safeguard against interrupted writes or power drops.
- **Power Efficiency**: Zero idle network activity (no background Wi-Fi/Bluetooth threads in core firmware) to preserve battery life.

### 2. Dual-Binary Distribution System
- **`firmware.bin` (Offset `0x10000`)**: Application-only binary used by WebSerial manifests and the `flash_device` CLI for data-preserving updates.
- **`factory_firmware.bin` (Offset `0x0`)**: All-in-one merged image combining bootloader (`0x1000`), partition table (`0x8000`), boot selector (`0xE000`), and application code (`0x10000`). Used for third-party tools ([web.esphome.io](https://web.esphome.io), raw `esptool.py` at `0x0`, and blank board recovery).

### 3. Supported Flashing Channels vs Third-Party Tools
- **Supported Data-Preserving Channels**:
  * **Official Web Flasher** (`web_flasher/index.html` or custom domain `flashclick.uprajjwal.com.np`): Safe browser-based updating with optional erase prompt.
  * **Desktop CLI Flasher** (`flash_device.bat` / `firmware/scripts/flash.sh`): Interactive terminal tool with safe offset `0x10000` preservation.
- **Third-Party Tool Caveat (`web.esphome.io`)**: Inherently destructive because it forces a full chip erase, wiping the `0x9000` NVS storage. Documented as a full factory reset / blank board recovery method only.

### 4. Release & Automation Architecture
- **Interactive Release Manager (`build_release.bat`)**: Automated compilation, 3-part binary packaging, semantic version bumping (`Major`, `Minor`, `Patch`), and 5-version retention in `releases/`.
- **Merged Binary Builder**: Automated creation of `factory_firmware.bin` (offset `0x0`) via `esptool merge_bin` to prevent unbootable devices on third-party flashers.
- **Automated Pre-Build Assets**: SCons pre-build hook (`firmware/scripts/pre_build_assets.py`) automatically compiles raw PNGs from `firmware/assets/screens/` into C PROGMEM headers prior to PlatformIO compilation.

---

## Release History & Changes

### [v0.1.1] - 2026-09-13

#### Added
- **Edge Serverless Leaderboard Backend (`server/`)**:
  - Zero-cost ($0/month), zero-maintenance edge backend powered by Cloudflare Workers and Cloudflare D1 (Serverless SQLite).
  - Schema defined in `server/schema.sql` (`global_stats`, `devices`, `sync_log`) with fast B-Tree indexing.
  - Endpoints:
    - `GET /api/leaderboard`: Returns real-time community boulder statistics and top 10 rankings for Sisyphus, Flappy Bird, and Just Ten.
    - `POST /api/sync`: Atomic score submission with rate limiting and delta crediting.
    - `GET /api/user`: Queries registered handle and stats by 12-character eFuse MAC.
- **The "Community Boulder" Shared Mechanic**:
  - Transforms solitary clicking into a global cooperative experience: all player clicks are aggregated into a single massive global boulder counter (`global_stats.total_boulder_clicks`).
  - Web interface visualizes the worldwide community progress as a shared ascent up Mount Olympus.
- **Server-Side Anti-Cheat & Human-Speed Rate Limiting**:
  - Algorithms enforce physical human limitations (~15–20 clicks/second maximum sustained rate).
  - Validates delta clicks against time elapsed since the device's previous sync.
  - Blocks impossible score injections and logs all delta claims into `sync_log`.
- **Custom Bootscreen Name & Device Personalization (`device_info.hpp` / `device_info.cpp`)**:
  - Persistent custom device name storage in NVS (`device` namespace).
  - Dynamically renders the owner's custom name (e.g. `"Prajjwal's Click"`) directly on the SSD1306 OLED boot screen.
- **WebSerial Bi-Directional Command Protocol (`main.cpp`)**:
  - Integrated ASCII serial interface over UART0 (115200 baud):
    - `GET_ID`: Returns unique 12-char factory eFuse MAC (`ID:<chip_id>`).
    - `GET_NAME`: Returns saved owner name (`NAME:<name>`).
    - `SET_NAME:<name>`: Writes new owner handle to NVS without reflashing firmware.
    - `GET_STATS`: Extracts live metrics from active applets (`CLICKS:<n>,FLAPPY:<n>,JUST_TEN:<n>`).
    - `RESET_STATS`: Allows zeroing active applet metrics.
- **Unified Web Flasher & Live Leaderboard Hub (`web_flasher/`)**:
  - Re-architected `flasher.html`, `index.html`, and `app.js` with responsive dark cyberpunk aesthetic.
  - Embedded live leaderboard tab with real-time podium cards for all three mini-games.
  - Two-way WebSerial connection tool to personalize device names and sync scores in one click.
  - Automatic fallback to bundled `leaderboard.json` if edge API is unreachable.

---

### [v0.1.0] - 2026-09-12

#### Added
- **Modular Firmware Architecture (`firmware/src/system/` & `firmware/src/applets/`)**:
  - Isolated core system firmware from game and applet logic to make adding new applets trivial without touching core OS/hardware code.
  - Core system layer (`Display.h`, `OSManager`, `PowerManager`, `InputManager`, `battery.hpp`, `device_info.hpp`, `Applet.h`) centralized into `firmware/src/system/`.
  - Applets organized into self-contained directories (`applets/clicker/`, `applets/timing_game/`, `applets/flappy_bird/`, `applets/settings/`, `applets/screensaver/`).
  - Removed obsolete legacy `input.hpp`.
  - Updated `platformio.ini` include flags so system and applet headers are resolved cleanly.
- **Sisyphus Clicker Game Engine (`CounterApplet`, `CounterRenderer`, `SisyphusSprites`)**:
  - Complete thematic overhaul: Sisyphus rolling a boulder up a mountain slope.
  - Multi-frame sprite animations: 25 FPS pushing cycle, 8 FPS walking cycle, and 10 FPS ambient cloud drift.
  - Physics-based boulder rotation and progress tracking.
  - Dynamic stamina and fatigue gauge: Sustained rapid clicking induces physical fatigue; Sisyphus rests and recovers stamina when clicking pauses.
  - Full integer display formatting: Fixed issues where counts over 10,000 were truncated or suffixed with abbreviations; numbers now render with full comma separators (e.g. `10,000+`, `25,480`).
  - Milestone celebration overlays: Graphic banners trigger upon crossing milestones (1, 5, 10, 50, 100, 500, 1000, 5000, 10000+).
- **Flappy Bird Minigame (`FlappyBirdApplet`)**:
  - Full arcade obstacle game featuring real-time collision detection, flapping velocity mechanics, score rendering, and persistent high score tracking.
- **Advanced Power Management Architecture (`PowerManager.h`)**:
  - **Light Sleep (20s Idle)**: Shuts down SSD1306 OLED, releases RF/ADC, and arms RTC GPIO wakeups.
  - **Deep Sleep (45s Idle)**: Complete power domain shutdown with instantaneous button wake.
  - **UART Pad Isolation**: Enforces `gpio_hold_en` on `UART_TX0` (GPIO 1) and `UART_RX0` (GPIO 3) during sleep to eliminate phantom current leakage back-feeding into the CH340 USB-UART converter.
  - **Display Bus Lockup Prevention**: Keeps I2C SDA and SCL pulled HIGH during sleep rather than driven to 0V.
  - **9-Cycle SCL Bus Recovery**: Sends 9 clock pulses on SCL upon wake to unstick any hung I2C slave devices before calling `Wire.begin()`.
- **Hardware Documentation & PCB Archival**:
  - **Active Prototype Board (Click 1)**: Added KiCad schematic and PCB layout files to `hardware/PCB/Click1/`. Documents the manually assembled ESP32-WROOM-32E prototype that actively executes all current embedded firmware in `firmware/`.
  - **Next-Generation Production Board (Click 4 - JLCPCB V0.0.1)**: Added complete turnkey PCBA design files to `hardware/PCB/Click4/` (schematic, PCB layout, and 3D STEP models) prepared for the JLCPCB V0.0.1 manufacturing run.
  - Microcontroller: RP2354A (QFN-56 package, 30 GPIOs: GP0-GP29)
  - Display I2C: SDA = GP20, SCL = GP21
  - User Inputs: MODE = GP14, ACTION = GP32 (or assigned GP)
  - Power & Battery: STAT = GP3 (or assigned GP), BAT_ADC = GP29 (ADC3)
  - Audio & LEDs: Buzzer = GP27, APA102 Data = GP11, APA102 Clock = GP12
  - Voltage Regulator: AP2112K-3.3 with EN tied to physical slide switch
  - *Note*: Firmware adaptations for the RP2354A will be developed once the manufactured boards arrive from JLCPCB.

#### Fixed & Optimized
- **Display Stabilization**: Added 500ms delay in `setup()` to allow SSD1306 power-on reset (POR) capacitors to stabilize before issuing initialization commands.
- **FreeRTOS Watchdog Panic**: Inserted `delay(2)` yield in `main.cpp` `loop()` to feed the Core 1 Task Watchdog Timer (`IDLE1` task).
- **Memory Leaks**: Eliminated heap fragmentation and unreleased display buffers in `CounterRenderer` and `PersistenceManager`.
- **Battery Curve Precision**: Calibrated 32-sample oversampled EMA filter with piece-wise LiPo discharge table and 5% hysteresis lock in `battery.hpp`.

---

### [v1.0.2] - 2026-08-26

#### Added
- Pre-build automated asset conversion pipeline integrated into PlatformIO (`pre_build_assets.py`).
- Pin assignment swaps and power optimization for battery voltage monitoring on GPIO 35 and charger status on GPIO 33.
- Web flasher binary and manifest synchronization.

---

### [v1.0.1] - 2026-08-23

#### Added
- Multi-applet architecture managed by `OSManager`.
- Timing game applet ("Just 10 Seconds") with millisecond accuracy evaluation.
- Snowfall screensaver (`HomeApplet`) activating on system idle.
- Battery percentage and hardware identification display in `SettingsApplet`.

---

### [v1.0.0] - 2026-08-20

#### Added
- Interactive Desktop CLI Flasher (`flash_device.bat` / `tools/flasher/flash_device.py`).
- All-in-One Factory Binary Merger (`factory_firmware.bin` at `0x0`).
- Interactive Release Manager (`build_release.bat` / `tools/release/release_manager.py`).
- Web Flasher Interface (`web_flasher/index.html` & `web_flasher/flasher.html`).
- Code freeze rules and project guidelines (`PROJECT_RULES.md`).
