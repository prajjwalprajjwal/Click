import re

with open('web_flasher/index.html', 'r', encoding='utf-8') as f:
    html = f.read()

# Remove Turbo Flash hook script
html = re.sub(r'<!-- ESP32 Turbo Flash Engine Hook[\s\S]*?</script>', '', html)
html = re.sub(r'<!-- ESP Web Tools -->\s*<script[^>]*esp-web-tools[^>]*></script>', '', html)

# Remove CH340 banner
html = re.sub(r'<!-- CH340 First Attempt Reassurance Alert Banner -->[\s\S]*?</div>\s*</div>', '', html)

# Remove alternative flashing section (Option 2 and 3 are ESP32 specific)
html = re.sub(r'<!-- Alternative Flashing & Browser Options Section -->[\s\S]*?<!-- Community Leaderboard', '<!-- Community Leaderboard', html)

# Update subtitle
html = html.replace('Clicker ESP32 and RP2350 devices.', 'Clicker RP2354 devices.')

# Update description
html = html.replace('Browser firmware installer for Clicker ESP32 and RP2350 devices.', 'Browser firmware installer for Clicker RP2354 devices.')

# Update target architecture info
html = html.replace('Target</div>\n              <div id="meta-version" class="meta-value">v0.1.0</div>', 'Target</div>\n              <div id="meta-version" class="meta-value">RP2354</div>')
html = html.replace('App Partition</div>\n              <div id="meta-offset" class="meta-value">0x10000</div>', 'Boot Mode</div>\n              <div id="meta-offset" class="meta-value">BOOTSEL</div>')

# Update hardware guidance
html = html.replace('Choose the device serial connection from the popup list', 'Choose the RP2354 PICOBOOT device from the popup list')
html = html.replace('Standard USB-to-UART bridge (CP210x or CH340)', 'Native USB PICOBOOT interface (RP2350/RP2354)')

with open('web_flasher/index.html', 'w', encoding='utf-8') as f:
    f.write(html)
