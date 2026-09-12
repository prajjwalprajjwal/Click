# Clicker Web Flasher & Release Infrastructure

Web-based firmware updater for the open-source Clicker device ecosystem, designed for deployment on custom domains (such as `https://flashclick.uprajjwal.com.np`) and powered by [ESP Web Tools](https://esphome.github.io/esp-web-tools/) over WebSerial.

---

## Data Preservation & Installation Choices

- **Update (Keep Data)**: Flashes `firmware.bin` to offset `0x10000` (`app0`) while preserving all lifetime clicks, completed cycles, and unlocked milestones in the `nvs` partition (`0x9000`).
- **Clean Install (Erase)**: Enabled via `"new_install_prompt_erase": true` in `manifest.json`. Users can choose to execute a full factory wipe when restoring unbootable units or transferring ownership.

---

## Directory Architecture

```text
Click/
├── web_flasher/
│   ├── index.html                  # Standalone WebSerial flasher interface
│   ├── flasher.html                # Modular self-contained component for site embedding
│   ├── style.css                   # Modern dark UI stylesheet
│   ├── app.js                      # Controller & version selector logic
│   ├── manifest.json               # Standalone root manifest
│   ├── firmware.bin                # Active app-only binary (offset 0x10000)
│   ├── bootloader.bin              # 2nd stage bootloader (offset 0x1000)
│   ├── partitions.bin              # Partition table (offset 0x8000)
│   └── versions.json               # Release catalog with hashes and download URLs
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

- **Run Web Flasher**: Double-click [`run_flasher.bat`](../run_flasher.bat) or run `python tools/flasher/serve_flasher.py` &rarr; Opens `http://localhost:8080/web_flasher/index.html`.
- **Desktop CLI Flasher**: Double-click [`flash_device.bat`](../flash_device.bat) &rarr; Interactive COM port detection & data-safe flashing.
- **Build Release**: Double-click [`build_release.bat`](../build_release.bat) &rarr; Compiles firmware via PlatformIO, builds `factory_firmware.bin` via `esptool merge_bin`, updates manifests, and syncs binaries.

---

## Web Hosting Deployment

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
