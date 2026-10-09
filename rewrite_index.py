import re

with open('web_flasher/index.html', 'r', encoding='utf-8') as f:
    html = f.read()

# Replace "ESP32 Web Serial" with "RP2354 WebUSB"
html = html.replace('ESP32 Web Serial', 'RP2354 WebUSB')

# Replace "Web Serial Ready:" with "WebUSB Ready:"
html = html.replace('Web Serial Ready:', 'WebUSB Ready:')

# Update esp-web-tools button to our new button
# Remove esp-web-tools block
old_esp_btn = '''        <!-- ESP Web Tools Button -->
        <div class="flasher-action">
          <esp-web-install-button id="esp-install-btn" manifest="manifest.json" baud-rate="460800">
            <button slot="activate" class="btn-primary" id="activate-install-btn">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
                <circle cx="12" cy="12" r="10"></circle>
                <line x1="12" y1="8" x2="12" y2="12"></line>
                <line x1="12" y1="16" x2="12.01" y2="16"></line>
              </svg>
              <span>Connect & Flash Clicker</span>
            </button>
            <div slot="unsupported"
              style="padding:0.85rem 1rem;background:rgba(245,158,11,0.08);border:1px solid rgba(245,158,11,0.3);border-radius:8px;color:#fbbf24;font-size:0.88rem;line-height:1.5;text-align:left;">
              Web flashing requires a Chromium-based browser (Chrome, Edge, Brave, or Opera) — Firefox and Safari don't
              support this. Please switch browsers, or use the Desktop CLI / flash_device.bat option instead.
            </div>
            <div slot="not-allowed"
              style="padding:0.75rem;background:rgba(244,63,94,0.08);border-radius:6px;color:var(--accent-rose);font-size:0.82rem;text-align:center;">
              Web Serial requires a secure context (HTTPS) or localhost.
            </div>
          </esp-web-install-button>
        </div>'''

new_btn = '''        <!-- WebUSB Flasher Button -->
        <div class="flasher-action">
            <button class="btn-primary" id="webusb-install-btn">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
                <circle cx="12" cy="12" r="10"></circle>
                <line x1="12" y1="8" x2="12" y2="12"></line>
                <line x1="12" y1="16" x2="12.01" y2="16"></line>
              </svg>
              <span>Connect & Flash Clicker (RP2354)</span>
            </button>
            <div id="webusb-status" style="margin-top:0.75rem; font-size:0.85rem; color:var(--text-secondary); text-align:center;"></div>
        </div>'''

html = html.replace(old_esp_btn, new_btn)

# Remove the rp2350-flash-panel section
panel_start = html.find('<section class="rp2350-flash-panel" aria-labelledby="rp2350-flash-title">')
if panel_start != -1:
    panel_end = html.find('</section>', panel_start) + len('</section>')
    
    # Hide the old ESP32 info in a details block at the bottom instead of completely removing
    # The prompt said "remove the esp32 flasher as it's obsolete now, or just keep it as a footnote"
    # We will just replace the panel with the ESP32 footnote.
    esp32_footnote = '''
    <details style="margin-top: 1rem; margin-bottom: 1rem; padding: 1rem; background: var(--bg-inset); border: 1px solid var(--border-subtle); border-radius: 8px;">
      <summary style="cursor: pointer; font-weight: 600; color: var(--text-secondary);">Legacy ESP32 Flasher (Obsolete)</summary>
      <div style="margin-top: 1rem;">
        <p>The ESP32 version of the Clicker is obsolete. If you still have one, you can use the legacy <a href="flasher.html" style="color: var(--accent-cyan);">ESP32 Flasher page</a>.</p>
      </div>
    </details>
    '''
    
    html = html[:panel_start] + esp32_footnote + html[panel_end:]

with open('web_flasher/index.html', 'w', encoding='utf-8') as f:
    f.write(html)
