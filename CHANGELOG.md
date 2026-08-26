# Clicker ESP32 Project Changelog & Architecture Master Log

> [!NOTE]
> **Living Document & AI Agent Instruction**: This document serves as the master record of all major architectural, memory storage, web updater, and firmware release decisions. AI agents and developers must append new entries to this changelog whenever significant system, storage, or release infrastructure changes occur.

---

## Core Architectural Decisions

### 1. NVS Partition Storage & Data Preservation Strategy
- **Partition Location**: `0x9000` (length `0x5000` / 20 KB).
- **Application Binary (`app0`)**: `0x10000` (65536 decimal, length `0x140000` / 1.25 MB).
- **Retention Guarantee**: Standard firmware updates strictly write to offset `0x10000` (`app0`). The `0x9000` NVS partition is preserved across updates, retaining all user lifetime clicks, completed cycles, unlocked milestones, and settings via `Preferences.h`.
- **Power Efficiency**: Zero idle network activity (no Wi-Fi/OTA background threads in C++ firmware) to maintain battery efficiency.

### 2. Dual-Binary Distribution System
- **`firmware.bin` (Offset `0x10000`)**: Application-only binary used by Web Serial manifests and the `flash_device` CLI for data-preserving updates.
- **`factory_firmware.bin` (Offset `0x0`)**: All-in-one merged image combining bootloader (`0x1000`), partition table (`0x8000`), boot selector (`0xE000`), and application code (`0x10000`). Used for third-party tools ([web.esphome.io](https://web.esphome.io), raw `esptool.py` at `0x0`, and blank board recovery).

### 3. Supported Flashing Channels vs Third-Party Tools
- **Supported Data-Preserving Channels**:
  * **Official Web Flasher** (`web_flasher/index.html` or custom domain `click.uprajjwal.com.np/flash`): Safe browser-based updating with optional erase prompt.
  * **Desktop CLI Flasher** (`flash_device.bat` / `tools/flasher/flash_device.py`): Interactive terminal tool with safe offset `0x10000` preservation.
- **Third-Party Tool Caveat (`web.esphome.io`)**: Inherently destructive because it forces a full chip erase, wiping the `0x9000` NVS storage. Documented as a full factory reset / blank board recovery method only.

### 4. Release & Automation Architecture
- **Interactive Release Manager (`build_release.bat`)**: Automated compilation, 3-part binary packaging, semantic version bumping (`Major`, `Minor`, `Patch`), and 5-version retention in `releases/`.
- **Merged Binary Builder**: Automated creation of `factory_firmware.bin` (offset `0x0`) via `esptool merge_bin` to prevent unbootable devices on third-party flashers.

---

## Release History & Changes

### [v1.0.0] - 2026-08-20

#### Added
- **Interactive Desktop CLI Flasher (`flash_device.bat` / `tools/flash_device.py`)**:
  - Auto-detects connected ESP32 serial ports.
  - Interactively prompts users to choose between:
    * `[1] Standard Update`: Flashes `firmware.bin` to `0x10000` without erasing flash, keeping all click stats intact.
    * `[2] Clean Factory Reset`: Runs full chip erase and writes `factory_firmware.bin` to `0x0`.
- **All-in-One Factory Binary Merger (`factory_firmware.bin`)**:
  - Automatically created via `esptool merge_bin` combining bootloader (`0x1000`), partitions (`0x8000`), `boot_app0` (`0xE000`), and app binary (`0x10000`) into a single file at `0x0`.
  - Fixes boot loop issues (`RTCWDT_RTC_RESET` / `invalid header`) when users flash single binaries using third-party tools (e.g. `web.esphome.io`).
- **Interactive Release Manager (`build_release.bat` / `tools/release_manager.py`)**:
  - Interactive CLI prompting for Major (`X.0.0`), Minor (`X.Y.0`), Patch (`X.Y.Z`), or custom version bumps.
  - Automatically compiles firmware, packages release folders, creates 3-part manifests, and syncs `web_flasher/`.
- **Web Flasher Interface (`web_flasher/index.html` & `web_flasher/flasher.html`)**:
  - Interactive modal with `"new_install_prompt_erase": true` enabling browser-based choice between update and factory reset.
  - Local manifest and binary co-location in `web_flasher/` to eliminate relative path traversal errors.
  - One-click launcher [`run_flasher.bat`](run_flasher.bat) for zero-config local HTTP serving and browser auto-launch.
- **Protected File Rules (`PROJECT_RULES.md`)**:
  - Code freeze locks on NVS storage logic, display and hardware initializations, and release directories.

#### Optimized & Cleaned
- Reorganized `tools/` directory into dedicated submodules:
  * `tools/flasher/`: Desktop CLI flasher (`flash_device.py`), local web server (`serve_flasher.py`), and `boot_app0.bin`.
  * `tools/release/`: Interactive release manager & semver bumper (`release_manager.py`).
  * `tools/converters/`: Bitmap & font converter scripts (`png_to_oled.py`, `ttf_to_gfxfont.py`).
- Cleaned up root launchers:
  * [`build_release.bat`](build_release.bat) &rarr; Interactive release manager.
  * [`flash_device.bat`](flash_device.bat) &rarr; Interactive desktop CLI flasher.
  * [`run_flasher.bat`](run_flasher.bat) &rarr; Local Web Serial server launcher.
- Removed redundant temporary scripts and obsolete packaging files.
- Removed dead code (`wakeFromDeepSleep()` in `OSManager`).
- Replaced hardcoded applet buffer arrays with fixed `constexpr uint8_t MAX_APPLETS = 4`.
- Centralized OLED display singleton references into `Display.h`.
- Removed redundant `#include <Arduino.h>` headers from pure interfaces.
- Optimized snow rendering in `HomeApplet` and cached milestone celebration screen lookups in `MilestonePresenter`.
