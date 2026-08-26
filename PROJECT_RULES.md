# Project Rules & AI Agent Guidelines

> [!IMPORTANT]
> **STRICT COMPLIANCE REQUIRED FOR ALL AI ASSISTANTS & AUTOMATED AGENTS**  
> These rules protect hardware configurations, non-volatile storage (NVS) memory partitions, display timings, and release integrity for the Clicker ESP32 project.

---

## 1. Protected Subsystems (CODE FREEZE - DO NOT MODIFY WITHOUT EXPLICIT USER CONSENT)

The following files and subsystems are locked and **must not be edited, refactored, or overwritten** unless the user explicitly requests changes in a specific prompt:

### A. NVS Storage & Persistent State
- `include/clicker/ClickCounter.h` / `src/clicker/ClickCounter.cpp`
- `include/clicker/MilestoneManager.h` / `src/clicker/MilestoneManager.cpp`
- Any storage retention logic utilizing `Preferences.h` (dual-key backup, wear-leveling, NVS namespaces).

### B. Hardware Pin & Display Configuration
- `platformio.ini` (board definitions, upload speeds, framework versions, build flags).
- Display initialization sequences and I2C pin mappings in `include/Display.h` and `src/main.cpp`.
- Hardware button pin definitions in `include/InputManager.h`.

### C. Web Flasher Core Engine
- `web_flasher/index.html` & `web_flasher/flasher.html` (core Web Serial bindings and `<esp-web-install-button>` logic).
- `web_flasher/app.js` (dynamic manifest and prefix resolution algorithms).
- `tools/flasher/serve_flasher.py` & `run_flasher.bat`.

---

## 2. Release & Flashing Workflow Boundary

- **NEVER manually create, delete, edit, or overwrite files inside `releases/`**.
- All official versioning, binary packaging, manifest generation, and multi-version retention must be performed **exclusively** through `build_release.bat`.
- For desktop flashing, use `flash_device.bat` to ensure safe offset handling.

---

## 3. Directory Layout & Purpose

```
Clicker_Code/
├── .pio/                 # PlatformIO build artifacts & libraries
├── assets/               # Raw graphic assets (logos, badges, icons)
├── include/              # C++ header files & contracts
├── src/                  # C++ implementation files
├── releases/             # Versioned release archives (managed by build_release.bat)
│   ├── v1.0.0/           # Active release binaries & manifest.json
│   └── versions.json     # Release metadata & version tracking
├── web_flasher/          # Web Serial browser flasher (hosted or local)
│   ├── index.html        # Main Web Serial interface
│   ├── style.css         # Modern dark theme styles
│   ├── app.js            # Controller & version selector logic
│   └── flasher.html      # Modular embeddable flasher component
├── tools/                # Python developer utilities
│   ├── flasher/          # Desktop CLI flasher & local web server
│   │   ├── flash_device.py
│   │   ├── serve_flasher.py
│   │   └── boot_app0.bin
│   ├── release/          # Semantic versioning & packaging engine
│   │   └── release_manager.py
│   └── converters/       # Graphic & font conversion utilities
│       ├── png_to_oled.py
│       └── ttf_to_gfxfont.py
├── build_release.bat     # [1-Click] Interactive release builder & version bumper
├── flash_device.bat      # [1-Click] Interactive desktop CLI flasher
├── run_flasher.bat       # [1-Click] Launch local web flasher & browser
├── CHANGELOG.md          # Master architecture and release log
├── PROJECT_RULES.md      # Code freeze rules & AI guidelines
└── README.md             # Project documentation & quick start guide
```

---

## 4. Standard Development & Testing Workflow

When developing or debugging new features:

1. **Compile Check**:
   ```bash
   pio run -e esp32doit-devkit-v1
   ```
2. **Direct Serial Upload**:
   ```bash
   pio run --target upload
   pio device monitor
   ```
3. **Official Release Creation** (Only when instructed by the user):
   ```powershell
   .\build_release.bat
   ```
4. **Git Version Control**:
   ```bash
   git add .
   git commit -m "Your descriptive commit message"
   git push
   ```
