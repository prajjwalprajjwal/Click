# Clicker ESP32 Web Flasher & Release Infrastructure

Web-based firmware updater for the open-source Clicker ESP32 device, designed for deployment on custom domains (e.g. `https://click.uprajjwal.com.np/flash`) and powered by [ESP Web Tools](https://esphome.github.io/esp-web-tools/) over Web Serial.

---

## Data Preservation & Installation Choices

- **Update (Keep Data)**: Flashes `firmware.bin` to `0x10000` (`app0`) while preserving all lifetime clicks, completed cycles, and unlocked milestones in the `nvs` partition (`0x9000`).
- **Clean Install (Erase)**: Enabled via `"new_install_prompt_erase": true` in `manifest.json`. Users can choose to factory reset if desired.

---

## Directory Architecture

```
Clicker_Code/
├── releases/
│   ├── v1.0.0/
│   │   ├── firmware.bin            # App-only binary (Offset 0x10000)
│   │   ├── factory_firmware.bin    # All-in-one merged binary (Offset 0x0)
│   │   ├── bootloader.bin          # 2nd stage bootloader (Offset 0x1000)
│   │   ├── partitions.bin          # Partition table (Offset 0x8000)
│   │   └── manifest.json           # ESP Web Tools manifest
│   └── versions.json               # Central release registry
├── web_flasher/
│   ├── index.html                  # Standalone flasher web page
│   ├── flasher.html                # Modular self-contained flasher for custom site embedding
│   ├── style.css                   # Modern dark UI stylesheet
│   ├── app.js                      # Web Serial controller & version selector
│   ├── manifest.json               # Standalone root manifest
│   └── versions.json               # Cached version registry
└── tools/
    ├── flasher/                    # Desktop flasher & server utilities
    │   ├── flash_device.py
    │   ├── serve_flasher.py
    │   └── boot_app0.bin
    ├── release/                    # Release builder & version manager
    │   └── release_manager.py
    └── converters/                 # Asset & font converters
        ├── png_to_oled.py
        └── ttf_to_gfxfont.py
```

---

## 1-Click Launchers

- **Run Web Flasher**: Double-click [`run_flasher.bat`](file:///d:/Playground/P71%20Studio/Projects/Clickie/Clicker_Code/run_flasher.bat) &rarr; Opens `http://localhost:8080/web_flasher/index.html`.
- **Desktop CLI Flasher**: Double-click [`flash_device.bat`](file:///d:/Playground/P71%20Studio/Projects/Clickie/Clicker_Code/flash_device.bat) &rarr; Interactive port detection & data-safe flash.
- **Build Release**: Double-click [`build_release.bat`](file:///d:/Playground/P71%20Studio/Projects/Clickie/Clicker_Code/build_release.bat) &rarr; Compiles & packages release.

---

## Web Hosting Deployment

### 1. Astro / Next.js / SvelteKit (Static Folder)
Copy `releases/` and `web_flasher/` into your site's `public/` folder. `flasher.html` will be accessible at `https://click.uprajjwal.com.np/flash`.

### 2. Nginx / Caddy Configuration
```nginx
server {
    server_name click.uprajjwal.com.np;
    root /var/www/click;
    index index.html;

    location /flash {
        alias /var/www/click/web_flasher;
        try_files /flasher.html =404;
    }

    location /releases {
        alias /var/www/click/releases;
    }
}
```
