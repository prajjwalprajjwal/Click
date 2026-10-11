# Clicker Web Flasher & Release Infrastructure

Browser firmware installer and device hub for the open-source Clicker device ecosystem. Click 4 (RP2354 / RP2350) updates feature **native in-browser WebUSB flashing** via Picoboot (with drag-and-drop UF2 volume and desktop Picotool fallbacks); legacy Click 1 (ESP32) updates use Web Serial via [ESP Web Tools](https://esphome.github.io/esp-web-tools/).

---

## 🌟 Key Features

1. **Device Personalization & Custom Name Field**:
   - Includes a dedicated **Device Name** input field with a seamless **Detect Device** action.
   - When a device is connected (via USB hotplug, "Detect Device", or "Sync Telemetry"), its current custom name or unique silicon identity (`Click-<last4>`) **automatically populates the field**.
   - The field is only changed when you manually edit it. Leaving it unedited or blank guarantees the device is never renamed to generic `CLICKER`, fully preserving its existing name or hardware identity.
   - Custom names are embedded into the firmware image prior to flashing and safely stored in hardware storage so they display on the OLED bootscreen and persist across reflashes.
2. **Safe Updates vs. Optional Factory Wipe**:
   - **Default Mode (Keep Data)**: Flashes firmware while preserving all lifetime clicks, unlocked milestones, and local settings.
   - **Factory Wipe Checkbox**: An optional checkbox (**unchecked by default**) allows executing a complete chip wipe when transferring ownership or recovering corrupted storage.
3. **Firmware Release History & Dropdown Archive**:
   - Shows the **latest 2 firmware releases** directly in the manual downloads table with instant links for `firmware.uf2`, `firmware.bin`, and `factory_firmware.bin`.
   - Collapses older archived releases into an interactive **dropdown button**, keeping the interface clean while preserving full access to past binaries.
4. **Live Community Leaderboard & Boulder Sync**:
   - Integrates directly with the Cloudflare Worker serverless backend (`/api/sync` and `/api/leaderboard`).
   - Queries telemetry (`clicks`, `flappy`, `just_ten`, `uptime_hrs`) via `\r\nGET_STATS\r\n` and submits verified deltas to the Community Boulder.
   - Shows live podiums and top 10 rankings for Sisyphus, Flappy Bird, and Just Ten.
5. **In-Browser WebUSB Picoboot Flashing (RP2354A / RP2350)**:
   - Direct 1-click in-browser flashing for RP2354 using the integrated WebUSB Picoboot driver (`web_flasher/lib/picoboot/`).
   - Connect the device in BOOTSEL mode (hold ACTION button while plugging in USB) and flash directly without installing desktop utilities, drivers, or Python.
   - Includes full fallback options: drag-and-drop `firmware.uf2` to the mounted `RP2350` USB mass storage volume, or run desktop helper scripts (`flash-rp2350.bat` / `flash-rp2350.sh`).

---

## 💻 How to Run & Open Locally

To run the Web Flasher on your local computer (e.g. for offline use or local development):

### Method 1: Using the Built-In Python Server (Recommended)
From the repository root, run:
```bash
python3 tools/flasher/serve_flasher.py
```
*(On Windows, you can also simply double-click [`run_flasher.bat`](../run_flasher.bat).)*

- The script automatically detects an available port (default `8080`), starts the local HTTP server, and **automatically launches your default browser** directly to:
  👉 **`http://localhost:8080/web_flasher/index.html`**

### Method 2: Serving Directly from the `web_flasher` Directory
If you prefer running Python's built-in HTTP server directly:
```bash
# From the repository root:
python3 -m http.server 8080 --directory web_flasher
```
Then open your browser and navigate to:
👉 **`http://localhost:8080/`** (or `http://localhost:8080/index.html`)

> [!IMPORTANT]
> **Browser Compatibility**:  
> Web flashing utilizes the Web Serial and WebUSB standards, which require a **Chromium-based browser**:
> - ✅ Google Chrome (Recommended)
> - ✅ Microsoft Edge
> - ✅ Brave
> - ✅ Opera
> 
> *(Safari and Firefox do not currently support Web Serial / WebUSB APIs).* 

---

## Directory Architecture

```text
Click/
├── web_flasher/
│   ├── index.html                  # Modern WebUSB/WebSerial flasher UI & device hub
│   ├── flasher.html                # Full-featured standalone flasher UI
│   ├── style.css                   # Cyberpunk terminal responsive styling
│   ├── rp2350-flash.css            # WebUSB installer & modal responsive styling
│   ├── rp2350-flash.js             # In-browser WebUSB Picoboot flasher & BOOTSEL flow
│   ├── app.js                      # WebSerial telemetry discovery, rename & leaderboard sync
│   ├── lib/                        # In-browser flashing drivers
│   │   ├── picoboot/               # Native WebUSB Picoboot protocol driver (RP2350/RP2354)
│   │   └── uf2.js                  # In-browser UF2 binary encoder/decoder
│   ├── flash-rp2350.bat            # Windows Picotool helper script
│   ├── flash-rp2350.sh             # Linux/macOS Picotool helper script
│   ├── manifest.json               # WebSerial firmware manifest
│   ├── versions.json               # Release catalog with hashes and download URLs
│   ├── firmware.bin                # Active app-only binary (offset 0x10000)
│   ├── firmware.uf2                # Active RP2354 UF2 image (BOOTSEL WebUSB / drag-and-drop)
│   ├── factory_firmware.bin        # Merged factory binary (offset 0x0)
│   ├── bootloader.bin              # 2nd stage bootloader (offset 0x1000)
│   ├── partitions.bin              # Partition table (offset 0x8000)
│   └── releases/                   # Multi-version release archive
│       ├── v0.1.1/                 # Release v0.1.1 (UF2 + BIN + manifest)
│       ├── v0.1.0/                 # Release v0.1.0 (BIN + manifest)
│       └── v0.0.1/                 # Initial release archive
├── tools/
│   ├── flasher/                    # Local HTTP server & desktop CLI utilities
│   │   ├── serve_flasher.py        # Zero-dependency localhost Python web server
│   │   └── flash_device.py         # Desktop serial flashing script
│   └── release/                    # Interactive release packager & semver bumper
│       └── release_manager.py
├── run_flasher.bat                 # 1-Click Windows localhost web flasher runner
├── flash_device.bat                # 1-Click Windows desktop CLI flasher
└── build_release.bat               # 1-Click Interactive release compiler & packager
```

---

## 1-Click Launchers

- **Run Web Flasher**: Double-click [`run_flasher.bat`](../run_flasher.bat) or run `python3 tools/flasher/serve_flasher.py` &rarr; Opens `http://localhost:8080/web_flasher/index.html`.
- **Desktop CLI Flasher**: Double-click [`flash_device.bat`](../flash_device.bat) &rarr; Interactive COM port detection & data-safe flashing.
- **RP2350 Picotool Update**: After installing a build with `ENABLE_PICOTOOL_USB`, run `web_flasher/flash-rp2350.bat` or `bash web_flasher/flash-rp2350.sh` with `click-rp2350.uf2` beside the helper.
- **Build Release**: Double-click [`build_release.bat`](../build_release.bat) &rarr; Compiles firmware via PlatformIO, builds `factory_firmware.bin` via `esptool merge_bin`, updates manifests, and syncs binaries.

---

## Web Hosting Deployment

Deploy all files in `web_flasher/` alongside either HTML page. The RP2350 UF2 installer validates `INFO_UF2.TXT` and the image family before copying to a selected BOOTSEL volume. The Picotool path requires Picotool on the host and firmware built with `ENABLE_PICOTOOL_USB`; the browser provides the command and host helpers but cannot execute native programs. The first firmware carrying the reset interface must be installed with UF2 or another bootloader method.

### 1. Astro / Next.js / SvelteKit (Static Hosting)
Copy `releases/` and `web_flasher/` into your static site's `public/` folder. `flasher.html` will be accessible directly at `https://flashclick.uprajjwal.com.np`.

### 2. Nginx / Reverse Proxy Configuration
```nginx
server {
    server_name flashclick.uprajjwal.com.np;
    root /var/www/click/web_flasher;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /releases {
        alias /var/www/click/releases;
    }
}
```
