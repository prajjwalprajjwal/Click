# Clicker Project 🎮

Official repository for the Clicker handheld device ecosystem, encompassing production firmware, hardware schematics, PCB designs, automated asset build pipelines, and browser-based WebSerial flashing tools.

---

## 📁 Repository Structure

```text
Click/
├── README.md                          <-- Master documentation and system overview
├── PROJECT_RULES.md                  <-- Protected subsystems, NVS rules & coding freeze
├── CHANGELOG.md                      <-- Comprehensive release history & architecture decisions
├── platformio.ini                    <-- PlatformIO configuration & automated build flags
├── build_release.bat                 <-- 1-Click Interactive firmware release packager
├── run_flasher.bat                   <-- 1-Click Localhost WebSerial server runner
├── flash_device.bat                  <-- 1-Click Desktop CLI serial flasher
├── hardware/                         <-- Electrical schematics & mechanical designs
│   ├── vector_designs.ai             <-- Vector graphics & enclosure laser/artwork
│   └── PCB/                          <-- KiCad PCB manufacturing & CAD files
│       ├── Click1/                   <-- Active prototype KiCad PCB & schematic (ESP32-WROOM-32E)
│       ├── Click4/                   <-- Next-Gen RP2354A PCB (JLCPCB PCBA Production V0.0.1)
│       └── Resources/                <-- Edge cut DXF files and mechanical templates
├── firmware/                         <-- Modular C++ embedded firmware
│   ├── assets/screens/               <-- 1-bit screen bitmaps & sprites (.png)
│   ├── scripts/
│   │   ├── pre_build_assets.py       <-- PlatformIO pre-build hook (auto-converts assets)
│   │   ├── convert_assets.py         <-- Converts PNGs to C PROGMEM bitmap headers
│   │   ├── flash.sh                  <-- Flashing utility for macOS / Linux
│   │   └── flash.bat                 <-- Flashing utility for Windows
│   └── src/                          <-- Application source code & applets
│       ├── main.cpp                  <-- Firmware entry point, interrupts & OS loop
│       ├── OSManager.h / .cpp        <-- Applet scheduler & sleep lifecycle manager
│       ├── PowerManager.h            <-- Light sleep, UART isolation & I2C bus recovery
│       ├── Display.h                 <-- SSD1306 128x64 display singleton & fast I2C
│       ├── InputManager.h / .cpp     <-- Debounced button state machine & holds
│       ├── battery.hpp               <-- Piece-wise LiPo curve & EMA filter
│       ├── device_info.hpp           <-- Unique eFuse MAC hardware identifier
│       ├── CounterApplet.h / .cpp    <-- Sisyphus uphill clicker game applet
│       ├── TimingGameApplet.h / .cpp <-- "Just 10 Seconds" accuracy timing game
│       ├── FlappyBirdApplet.h / .cpp <-- Flappy Bird obstacle navigation game
│       ├── SettingsApplet.h          <-- Hardware stats, voltage & device ID
│       ├── HomeApplet.h / .cpp       <-- Snowfall particle screensaver
│       ├── clicker/                  <-- Sisyphus physics & persistence engine
│       │   ├── ClickCounter.h / .cpp <-- Counter logic & full-integer formatting
│       │   ├── CounterRenderer.h/.cpp<-- Mountain ascent, stamina bar & boulder physics
│       │   ├── SisyphusSprites.h     <-- 25 FPS pushing & walking sprite animations
│       │   ├── MilestoneManager.h    <-- Celebration triggers & unlocked tiers
│       │   └── PersistenceManager.h  <-- Wear-leveled dual-key NVS backup storage
│       ├── fonts/                    <-- Custom Adafruit GFX typography headers
│       └── generated_assets/         <-- Auto-generated C bitmap headers
├── web_flasher/                      <-- WebSerial flashing site (flashclick.uprajjwal.com.np)
│   ├── index.html                    <-- Web application entry point
│   ├── app.js                        <-- WebSerial engine & dynamic device ID query
│   ├── manifest.json                 <-- ESP Web Tools manifest configuration
│   └── versions.json                 <-- Firmware release registry
├── tools/                            <-- Build & developer utilities
│   ├── release/release_manager.py    <-- Interactive version bumper & binary packager
│   ├── flasher/serve_flasher.py      <-- Zero-config HTTP server for web_flasher
│   └── converters/                   <-- TTF font and image conversion scripts
└── docs/                             <-- Architectural specifications
    ├── hardware_and_pinouts.md       <-- Detailed pin profiles (ESP32 vs RP2354A)
    ├── device_identification_and_leaderboard.md <-- eFuse MAC & database schema
    └── leaderboard_architecture.md   <-- API synchronization & score submission
```

---

## ⚡ Hardware Platforms & Pin Profiles

Detailed technical schematics and electrical specifications can be found in [`docs/hardware_and_pinouts.md`](docs/hardware_and_pinouts.md).

> [!NOTE]
> **Active Hardware & Production Status**:
> - **Active Prototype (Click 1)**: Up until now, boards have been fabricated manually. The **ESP32-WROOM-32E** was chosen because it is simpler and matches local prototyping resources. **All current firmware, active code, pin assignments, and WebSerial flashing run on this physical Click 1 board.** Design files are located in `hardware/PCB/Click1/`.
> - **Upcoming Production Hardware (Click 4 - V0.0.1)**: The design for the **Click 4** PCB is complete and prepared for turnkey assembly (PCBA) via **JLCPCB for the V0.0.1 production run**. It features the RP2354A MCU. Firmware adaptations for the RP2354A will be developed once the manufactured boards are in hand. Design files are in `hardware/PCB/Click4/`.

### Active Prototyping Hardware Specs (Click 1: ESP32-WROOM-32E)
- **Microcontroller**: ESP32-WROOM-32E (Xtensa Dual-Core 240MHz, 4MB Flash)
- **Display I2C**: SDA = GPIO 21, SCL = GPIO 22 (400kHz Fast I2C; *GPIO 36 is input-only*)
- **User Inputs**: MODE = GPIO 14 (RTC ext0 wake), ACTION = GPIO 32 (RTC ext1 wake)
- **Power & Battery**: STAT = GPIO 33 (Active LOW from ETA IC), BAT_ADC = GPIO 35 (100k/100k divider)
- **Serial & Power Isolation**: UART TX0 = GPIO 1, UART RX0 = GPIO 3 (isolated with pad holds in sleep)
- **Sleep States**: Light sleep at 20s (OLED off, I2C pullups held), Deep sleep at 45s (RTC wakeup)

### Upcoming Hardware Pinout & Specs (V4 / Click 4 PCB)
- **Microcontroller**: RP2354A (QFN-56 package, 30 GPIOs: GP0-GP29)
- **Display I2C**: SDA = GP20, SCL = GP21
- **User Inputs**: MODE = GP14, ACTION = GP32 (or assigned GP)
- **Power & Battery**: STAT = GP3 (or assigned GP), BAT_ADC = GP29 (ADC3)
- **Audio & LEDs**: Buzzer = GP27, APA102 Data = GP11, APA102 Clock = GP12
- **Voltage Regulator**: AP2112K-3.3 with EN tied to physical slide switch

---

## 🕹️ Applet Ecosystem & Gameplay

The firmware features an event-driven `OSManager` hosting four built-in applets and an idle screensaver:

### 1. Sisyphus Clicker (Default Applet)
* **Theme**: Sisyphus rolling his eternal boulder up a steep mountain slope.
* **Dynamics**:
  * **Boulder Physics**: Smooth rotation and position tracking based on player clicks.
  * **Stamina & Fatigue**: Continuous clicking builds fatigue; stopping causes Sisyphus to catch his breath.
  * **Full Integer Notation**: No abbreviated suffix truncation (e.g. `10,000+` and higher counts display fully).
  * **Milestone Celebrations**: Overlay banners trigger on reaching milestone tiers (1, 5, 10, 50, 100, 1000, etc.).
  * **Persistence**: Dual-key NVS backup storage (`nvs_a` / `nvs_b`) with checksum verification and wear-leveling.

### 2. Just Ten
* Precision stopwatch game challenging players to hold and release the button at exactly **10.0000 seconds**.
* Displays exact millisecond accuracy and tracks personal records.

### 3. Flappy Bird
* Real-time side-scrolling obstacle game featuring obstacle collision detection, velocity physics, and persistent high scores.

### 4. Settings & System Applet
* Accessible by holding the **MODE** button.
* Displays live battery percentage (quantized in 5% steps with hysteresis), instantaneous battery voltage, active charging status (`CHARGING` / `BATTERY`), unique 48-bit eFuse MAC ID, and firmware build version.

### 5. Snowfall Screensaver
* Ambient particle simulation that activates automatically when the device remains idle prior to entering sleep.

---

## 🎮 Controls & Navigation Guide

| Action | Control | Description |
|:---|:---|:---|
| **Cycle Applets** | `MODE` Button (Short Press) | Switch between **Clicker**, **Just Ten**, and **Flappy Bird**. |
| **Settings Menu** | `MODE` Button (Hold > 1s) | Toggle the **Settings & System Status** screen. |
| **Primary Game Action** | `ACTION` Button (Short Press) | Push boulder / Jump bird / Start & Stop 10s timer. |
| **Reset Game Score** | `ACTION` Button (Hold > 3s) | Reset the score/counter of the currently active applet. |
| **Wake from Sleep** | Either Button (`MODE` or `ACTION`) | Instantly wakes the device from Light Sleep or Deep Sleep. |
| **Factory Master Reset** | **Both Buttons Held** (> 4s) | Erase all NVS partitions, resetting lifetime clicks and milestones. |

---

## 🚀 Building, Flashing & Updating

### 1. Web Flasher (Recommended)
* **Online**: Connect your device and visit [flashclick.uprajjwal.com.np](https://flashclick.uprajjwal.com.np).
* **Local Server**: Double-click [`run_flasher.bat`](run_flasher.bat) or run:
  ```bash
  python tools/flasher/serve_flasher.py
  ```

### 2. Desktop Command-Line Flashing
* **Windows**: Run [`flash_device.bat`](flash_device.bat).
* **macOS / Linux**:
  ```bash
  chmod +x firmware/scripts/flash.sh
  ./firmware/scripts/flash.sh
  ```

### 3. Building via PlatformIO
The project uses PlatformIO with an automated asset compilation hook.
```bash
# Compile firmware (automatically triggers pre-build asset conversion)
pio run -e esp32doit-devkit-v1

# Upload directly over USB serial
pio run --target upload

# Open serial monitor
pio device monitor -b 115200
```

### 4. Automated Asset Pipeline
Screen graphics placed in `firmware/assets/screens/` are converted to C headers before every compile:
```bash
python firmware/scripts/convert_assets.py
```

### 5. Official Release Packaging
To build an official release, bump semantic versions, and generate 3-part manifests for WebSerial:
```bash
# Windows
.\build_release.bat

# Python direct
python tools/release/release_manager.py
```

---

## 🌐 Community & Global Leaderboard

* **Live Leaderboard**: [click.uprajjwal.com.np](https://click.uprajjwal.com.np)
* **Web Flasher**: [flashclick.uprajjwal.com.np](https://flashclick.uprajjwal.com.np)
