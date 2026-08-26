# Clicker Project 🎮

Official repository for the ESP32-based Clicker device, including hardware designs, firmware, automated build pipelines, and web flashing tools.

---

## 📁 Repository Structure

```text
clicker-project/
├── README.md                      <-- Master documentation and entry point
├── hardware/
│   ├── vector_designs.ai          <-- Vector artwork & Illustrator asset files
│   └── enclosure/                 <-- 3D printable files / STLs / housing
├── firmware/
│   ├── assets/
│   │   └── screens/               <-- Raw .png / .bmp screen graphics
│   ├── scripts/
│   │   ├── convert_assets.py      <-- Auto-converts ALL images in screens/
│   │   ├── flash.sh               <-- Flashing script for macOS / Linux
│   │   └── flash.bat              <-- Flashing script for Windows
│   └── src/                       <-- Modular C++ firmware source code
│       ├── device_info.hpp        <-- Unique eFuse MAC hardware identifier
│       ├── battery.hpp            <-- Battery percentage & charging monitor
│       ├── input.hpp              <-- Hold-duration button state machine
│       └── generated_assets/      <-- Auto-generated C header bitmaps
├── web_flasher/                   <-- WebSerial flasher site (flashclick.uprajjwal.com.np)
│   ├── index.html                 <-- Web app entry point
│   ├── app.js                     <-- WebSerial flashing & device ID engine
│   ├── manifest.json              <-- ESP Web Tools configuration
│   └── versions.json              <-- Version catalog & firmware binaries
├── tools/                         <-- Python utilities & build pipeline
│   ├── release/release_manager.py <-- Interactive version bump & binary packager
│   ├── flasher/serve_flasher.py   <-- Localhost web server for web_flasher
│   └── converters/                <-- Font & asset conversion utilities
├── build_release.bat              <-- 1-Click Interactive firmware build & release packager
├── run_flasher.bat                <-- 1-Click Localhost web flasher server runner
├── flash_device.bat               <-- 1-Click Desktop command-line flasher
└── docs/                          <-- Technical architecture & database specs
```

---

## 🚀 Quick Start & Flashing

### 1. Web Flasher (Easiest)
* **Online**: Visit [flashclick.uprajjwal.com.np](https://flashclick.uprajjwal.com.np)
* **Localhost**: Double-click [`run_flasher.bat`](run_flasher.bat) (or run `python tools/flasher/serve_flasher.py`).

### 2. Desktop Command Line Flashing
* **Windows**: Double-click [`flash_device.bat`](flash_device.bat) or [`firmware/scripts/flash.bat`](firmware/scripts/flash.bat).
* **macOS / Linux**:
  ```bash
  cd firmware/scripts
  chmod +x flash.sh
  ./flash.sh
  ```

---

## 🛠️ Building & Releasing Firmware

### 1. Automated Asset Conversion
Drop `.png` or `.bmp` files into `firmware/assets/screens/` and run:
```bash
cd firmware/scripts
python convert_assets.py
```

### 2. Interactive Firmware Release Packager
Double-click [`build_release.bat`](build_release.bat) to bump version (Major/Minor/Patch), compile via PlatformIO, package factory binaries, and update `web_flasher/` and `releases/` automatically.

---

## 🕹️ Controls & Navigation

* **MODE Button (Short Press)**: Cycle through **Home (Snowfall)**, **Clicker Game**, and **10-Second Hold Game**.
* **MODE Button (Hold 5s)**: Open **Settings & Battery Status** screen.
* **ACTION Button (Short Press)**: Perform game action / increment score.
* **ACTION Button (Hold 3s)**: Reset current game counter.
* **Both Buttons (Hold 4s)**: Factory reset all counters and milestone flags.

---

## 🌐 Community & Leaderboards

Live global leaderboard and statistics: [click.uprajjwal.com.np](https://click.uprajjwal.com.np).
