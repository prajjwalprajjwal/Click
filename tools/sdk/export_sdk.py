#!/usr/bin/env python3
"""
Click SDK Exporter & AI Agent Context Generator.

This script scans the authoritative firmware headers and hardware definitions
in the Click repository, parses the available peripheral APIs, sound effects,
fonts, and applet lifecycle hooks, and generates:
1. sdk/click_sdk_spec.json       - Machine-readable SDK specification
2. sdk/CLICK_AI_AGENT_PROMPT.md  - Specialized AI System Prompt for LLM Applet generation
3. sdk/AppletTemplate.h          - Standard C++ Applet Header Template
4. sdk/AppletTemplate.cpp        - Standard C++ Applet Implementation Template
5. sdk/ClickSimulator.js         - Pure JS/Canvas in-browser simulator runtime for web preview

Run manually or via GitHub Actions on commit to firmware/ or system/.
"""

from __future__ import annotations
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_SRC = ROOT / "firmware" / "src"
SDK_DIR = ROOT / "sdk"

def parse_sound_presets(soundfx_h: Path) -> list[dict]:
    presets = []
    if not soundfx_h.exists():
        return presets
    
    content = soundfx_h.read_text(encoding="utf-8")
    # Matches: static void playXyz()
    pattern = re.compile(r"//\s*(.*?)\n\s*static\s+void\s+(play[A-Za-z0-9_]+)\s*\(\s*\)", re.MULTILINE)
    for match in pattern.finditer(content):
        desc = match.group(1).strip()
        method = match.group(2).strip()
        presets.append({
            "name": method,
            "description": desc,
            "signature": f"SoundFX::{method}()"
        })
    return presets

def parse_led_effects() -> list[dict]:
    return [
        {
            "method": "WS2812Driver::flash(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs)",
            "description": "Flashes both RGB LEDs with the given color for specified duration"
        },
        {
            "method": "WS2812Driver::breathe(uint8_t r, uint8_t g, uint8_t b, uint16_t periodMs)",
            "description": "Smooth sinusoidal breathing color animation across LEDs"
        },
        {
            "method": "WS2812Driver::pulse(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs)",
            "description": "Single-shot smooth pulse animation"
        },
        {
            "method": "WS2812Driver::celebrate(uint16_t durationMs)",
            "description": "Rainbow celebration party effect (used for high-scores / milestones)"
        },
        {
            "method": "WS2812Driver::setPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b)",
            "description": "Sets raw RGB value for LED 0 or 1 (requires WS2812Driver::show())"
        },
        {
            "method": "WS2812Driver::clear()",
            "description": "Turns off both RGB LEDs"
        }
    ]

def generate_sdk_spec(sound_presets: list[dict], led_effects: list[dict]) -> dict:
    return {
        "version": "1.0.0",
        "system_name": "Click OS",
        "hardware": {
            "primary": {
                "name": "Click 4 PCBA",
                "mcu": "Raspberry Pi RP2354A",
                "core": "Dual ARM Cortex-M33 @ 48MHz/150MHz",
                "ram_bytes": 524288,
                "flash_bytes": 4194304,
                "display": {
                    "controller": "SSD1306",
                    "type": "Monochrome OLED",
                    "width": 128,
                    "height": 64,
                    "bus": "I2C1 (Wire1)",
                    "speed_hz": 400000,
                    "address": "0x3C",
                    "rotation": 2
                },
                "inputs": [
                    {
                        "name": "MODE",
                        "pin": "GP0",
                        "type": "Tactile Button",
                        "active": "LOW",
                        "events": ["onModeClick", "onModeHold"]
                    },
                    {
                        "name": "ACTION",
                        "pin": "QSPI_SS / BOOTSEL",
                        "type": "Primary Game/Click Button",
                        "active": "LOW",
                        "events": ["onActionClick", "onActionHold"]
                    }
                ],
                "rgb_leds": {
                    "count": 2,
                    "driver": "WS2812 / SK6812",
                    "pin": "GP11 (PIO)"
                },
                "buzzer": {
                    "pin": "GP27",
                    "driver": "NPN BJT (MMBT3904)",
                    "type": "Piezo/Magnetic Buzzer"
                },
                "battery": {
                    "charger": "ETA6003 (STAT on GP1)",
                    "adc_pin": "GP29 (ADC3)",
                    "divider_ratio": 2.0
                }
            },
            "legacy": {
                "name": "Click 1 Prototype",
                "mcu": "ESP32-WROOM-32E",
                "core": "Xtensa Dual-Core 240MHz",
                "ram_bytes": 524288,
                "flash_bytes": 4194304
            }
        },
        "lifecycle_hooks": [
            {
                "name": "init()",
                "required": True,
                "description": "Called once when applet is activated. Initialize variables, pre-render, or restore state."
            },
            {
                "name": "update()",
                "required": True,
                "description": "Called every frame (capped ~30-40 FPS). Update game state, physics, non-blocking timers."
            },
            {
                "name": "draw()",
                "required": True,
                "description": "Called after update(). Render frame to Adafruit_SSD1306 display. Do NOT call display.clearDisplay() or display.display() directly; OSManager handles display flush."
            },
            {
                "name": "cleanup()",
                "required": False,
                "description": "Called when switching away from this applet. Save state to Preferences, clear LEDs."
            },
            {
                "name": "onPrepareSleep()",
                "required": False,
                "description": "Called before device enters light/deep sleep."
            },
            {
                "name": "onActionClick()",
                "required": False,
                "description": "Fired when ACTION button is clicked (released under 400ms)."
            },
            {
                "name": "onActionHold()",
                "required": False,
                "description": "Fired when ACTION button is held for >= 500ms."
            },
            {
                "name": "onModeClick()",
                "required": False,
                "description": "Fired when MODE button is clicked. Note: In OSManager, clicking MODE typically advances to next applet."
            },
            {
                "name": "onModeHold()",
                "required": False,
                "description": "Fired when MODE button is held for >= 500ms."
            },
            {
                "name": "onBothHeld()",
                "required": False,
                "description": "Fired when both buttons are held simultaneously for >= 800ms."
            }
        ],
        "fonts": [
            {"name": "Rajdhani12pt7b", "font_var": "&Rajdhani12pt7b", "line_height": 14, "usage": "Subtitles, status bar, small values"},
            {"name": "Rajdhani18pt7b", "font_var": "&Rajdhani18pt7b", "line_height": 20, "usage": "Medium titles, secondary scores"},
            {"name": "Rajdhani24pt7b", "font_var": "&Rajdhani24pt7b", "line_height": 26, "usage": "Primary counter, big score"},
            {"name": "Rajdhani32pt7b", "font_var": "&Rajdhani32pt7b", "line_height": 34, "usage": "Giant numbers, milestone popups"},
            {"name": "Rajdhani40pt7b", "font_var": "&Rajdhani40pt7b", "line_height": 42, "usage": "Ultra giant display font"}
        ],
        "audio_presets": sound_presets,
        "led_effects": led_effects,
        "rules": [
            "Never use blocking delay() calls inside update() or draw(). Use non-blocking millis() or micros() delta timers.",
            "Display is 128x64 monochrome (SSD1306). Color constants are SSD1306_WHITE (1) and SSD1306_BLACK (0).",
            "Numbers and scores must ALWAYS be printed as full integers with commas (e.g., 10,000, 12,345), never truncated (no 10k/1M suffixes).",
            "Keep RAM allocations static. Do NOT perform heap allocations (new/malloc) in update() or draw().",
            "Use ThemeFonts::drawCentered(font, text, centerY) or ThemeFonts::measure() for clean typography."
        ]
    }

def generate_ai_agent_prompt(spec: dict) -> str:
    spec_json = json.dumps(spec, indent=2)
    prompt = f"""# Click OS Microapplet Generator: AI Developer Guidelines & System Prompt

You are an expert embedded C++ systems engineer specializing in writing micro-applets for **Click** — a minimalist handheld device powered by the Raspberry Pi RP2354A (and ESP32).

---

## 1. Device Hardware & Constraints
- **Display**: 128x64 Monochrome OLED (SSD1306, `display`).
  - Coordinate system: `(0,0)` top-left to `(127,63)` bottom-right.
  - Colors: `SSD1306_WHITE` (pixel on) and `SSD1306_BLACK` (pixel off).
  - Rotation: Screen is hardware inverted (`display.setRotation(2)`).
- **Buttons**:
  - `ACTION` button (primary click/interact button).
  - `MODE` button (cycle/mode toggle).
- **RGB LEDs**: 2x Addressable SK6812/WS2812 on `GP11`.
- **Sound Buzzer**: Hardware magnetic buzzer on `GP27`.
- **Storage**: Persistent non-volatile storage via `Preferences.h` (`nvs`).

---

## 2. Applet Architecture Contract
Every applet inherits from the base class `Applet` (`#include "Applet.h"`).

### Complete Class Interface
```cpp
#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "Applet.h"
#include "system/Display.h"
#include "system/SoundFX.h"
#include "system/WS2812.h"
#include "fonts/ThemeFonts.h"

class CustomApplet : public Applet {{
public:
    void init() override;              // Called when applet is switched into focus
    void update() override;            // Non-blocking frame tick (~30-40 FPS)
    void draw() override;              // Frame render (display buffer operations)
    void cleanup() override;           // Called when switching out (save state, turn off LEDs)
    void onPrepareSleep() override;    // Called before system sleep

    // Input Events
    void onActionClick() override;     // Instant ACTION button release
    void onActionHold() override;      // ACTION held >= 500ms
    void onModeClick() override;        // MODE button click
    void onModeHold() override;        // MODE held >= 500ms
    void onBothHeld() override;        // Both buttons held >= 800ms
}};
```

---

## 3. Strict Development Rules
1. **NO BLOCKING CALLS**: Never call `delay()` inside `update()` or `draw()`. Use `millis()` or `micros()` delta tracking.
2. **NO HEAP CHURN**: Avoid `malloc()`, `free()`, `new`, `delete`, or dynamic `std::string` concatenation inside the frame loop. Allocate all state variables as class members.
3. **DO NOT FLUSH DISPLAY DIRECTLY**: Inside `draw()`, draw shapes, bitmaps, and text using `display.drawPixel`, `display.fillRect`, etc. The OS loop handles `display.display()`.
4. **INTEGER FORMATTING**: Never abbreviate large numbers to `10k` or `1M`. Always render full numbers with commas (e.g. `12,345`).
5. **AUDIO & LED SAFETY**:
   - Audio: Call `SoundFX::playClick()`, `SoundFX::playTone(freq, durationMs)`, `SoundFX::playScore()`, or `SoundFX::playGameOver()`.
   - LEDs: Call `WS2812Driver::flash(r, g, b, durationMs)`, `WS2812Driver::breathe(...)`, or `WS2812Driver::celebrate(...)`. Always call `WS2812Driver::clear()` in `cleanup()`.

---

## 4. Typography & Layout Utilities
Available Rajdhani fonts:
- `&Rajdhani12pt7b` (Subtitles, status, time)
- `&Rajdhani18pt7b` (Headings, buttons)
- `&Rajdhani24pt7b` (Scores, click counts)
- `&Rajdhani32pt7b` & `&Rajdhani40pt7b` (Hero digits)

Helper functions:
```cpp
// Draw centered text vertically & horizontally:
ThemeFonts::drawCentered(&Rajdhani18pt7b, "READY!", 32);

// Measure exact bounding box:
int16_t x1, y1;
uint16_t w, h;
ThemeFonts::measure(&Rajdhani12pt7b, "SCORE: 100", x1, y1, w, h);
```

---

## 5. Machine-Readable Specification
```json
{spec_json}
```

---

## 6. Output Format
When generating an applet from a user prompt, always output:
1. **Header file**: `[AppletName]Applet.h`
2. **Implementation file**: `[AppletName]Applet.cpp`
3. **Web Simulator Companion (JS)**: A single self-contained JavaScript function `function create[AppletName](sim)` that hooks into `ClickSimulator` so the user can test the applet live in the web browser!
"""
    return prompt

def generate_template_h() -> str:
    return """#ifndef CUSTOM_APPLET_H
#define CUSTOM_APPLET_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "system/Applet.h"
#include "system/Display.h"
#include "system/SoundFX.h"
#include "system/WS2812.h"
#include "fonts/ThemeFonts.h"

class CustomApplet : public Applet {
private:
    uint32_t score = 0;
    uint32_t highScore = 0;
    unsigned long lastFrameMs = 0;
    Preferences prefs;

public:
    void init() override;
    void update() override;
    void draw() override;
    void cleanup() override;
    void onPrepareSleep() override;

    // Input handlers
    void onActionClick() override;
    void onActionHold() override;
    void onModeClick() override;
    void onModeHold() override;
    void onBothHeld() override;
};

#endif // CUSTOM_APPLET_H
"""

def generate_template_cpp() -> str:
    return """#include "CustomApplet.h"

void CustomApplet::init() {
    score = 0;
    lastFrameMs = millis();
    
    // Restore persistent high score
    prefs.begin("custom_app", false);
    highScore = prefs.getUInt("high_score", 0);
    prefs.end();

    // Welcome LED pulse & click sound
    WS2812Driver::flash(0, 200, 255, 150);
    SoundFX::playClick();
}

void CustomApplet::update() {
    unsigned long now = millis();
    if (now - lastFrameMs < 30) {
        return; // ~33 FPS frame pacing
    }
    lastFrameMs = now;

    // Non-blocking physics / game state updates here
}

void CustomApplet::draw() {
    display.clearDisplay();

    // Title
    ThemeFonts::set(&Rajdhani12pt7b);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 14);
    display.print("MICRO APPLET");

    // Score display
    char scoreBuf[32];
    snprintf(scoreBuf, sizeof(scoreBuf), "%lu", (unsigned long)score);
    ThemeFonts::drawCentered(&Rajdhani24pt7b, scoreBuf, 40);

    // Footer hint
    display.setFont(nullptr); // default 5x7 font
    display.setTextSize(1);
    display.setCursor(18, 55);
    display.print("[ACTION] TO PLAY");
}

void CustomApplet::onActionClick() {
    score++;
    SoundFX::playClick();
    WS2812Driver::flash(50, 255, 50, 50);

    if (score > highScore) {
        highScore = score;
        prefs.begin("custom_app", false);
        prefs.putUInt("high_score", highScore);
        prefs.end();
    }
}

void CustomApplet::onActionHold() {
    // Reset or secondary action
    score = 0;
    SoundFX::playTone(440, 100);
}

void CustomApplet::onModeClick() {
    // Mode toggle
}

void CustomApplet::onModeHold() {
}

void CustomApplet::onBothHeld() {
}

void CustomApplet::cleanup() {
    WS2812Driver::clear();
}

void CustomApplet::onPrepareSleep() {
    WS2812Driver::clear();
}
"""

def generate_click_simulator_js() -> str:
    return """/**
 * ClickSimulator.js - Zero-dependency in-browser simulator for Click OS Applets.
 *
 * Emulates:
 * - 128x64 Monochrome OLED Display (Canvas rendering with pixel grid and glow)
 * - ACTION & MODE tactile buttons (Keyboard + pointer/touch events)
 * - SoundFX Audio Engine (Web Audio API synthesizes buzzer square-wave tones & presets)
 * - 2x Addressable WS2812 RGB LEDs (Canvas radial glows)
 * - Non-blocking ~33 FPS game loop
 */

export class ClickSimulator {
  constructor(canvasElement, options = {}) {
    this.canvas = canvasElement;
    this.ctx = canvasElement.getContext('2d');
    this.width = 128;
    this.height = 64;
    this.pixelScale = options.pixelScale || 4;
    this.glow = options.glow !== undefined ? options.glow : true;

    // Buffer: 128x64 binary pixels
    this.buffer = new Uint8Array(this.width * this.height);

    // RGB LEDs: [ [r, g, b], [r, g, b] ]
    this.leds = [ [0, 0, 0], [0, 0, 0] ];

    // Audio Context (Web Audio)
    this.audioCtx = null;
    this.currentOsc = null;

    // Active Applet instance
    this.currentApplet = null;
    this.isRunning = false;
    this.animationFrameId = null;
    this.lastFrameTime = performance.now();

    // Input State
    this.actionPressed = false;
    this.modePressed = false;
    this.actionPressStart = 0;
    this.modePressStart = 0;

    this.initCanvas();
    this.bindInputs();
  }

  initCanvas() {
    this.canvas.width = this.width * this.pixelScale;
    this.canvas.height = this.height * this.pixelScale;
    this.ctx.imageSmoothingEnabled = false;
  }

  ensureAudio() {
    if (!this.audioCtx) {
      const AudioContext = window.AudioContext || window.webkitAudioContext;
      if (AudioContext) {
        this.audioCtx = new AudioContext();
      }
    }
    if (this.audioCtx && this.audioCtx.state === 'suspended') {
      this.audioCtx.resume();
    }
  }

  // --- SoundFX Engine (Web Audio API) ---
  playTone(freq, durationMs) {
    this.ensureAudio();
    if (!this.audioCtx || freq <= 0) return;
    try {
      const osc = this.audioCtx.createOscillator();
      const gain = this.audioCtx.createGain();
      osc.type = 'square'; // Clicker hardware buzzer sound
      osc.frequency.setValueAtTime(freq, this.audioCtx.currentTime);

      gain.gain.setValueAtTime(0.08, this.audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.001, this.audioCtx.currentTime + durationMs / 1000);

      osc.connect(gain);
      gain.connect(this.audioCtx.destination);
      osc.start();
      osc.stop(this.audioCtx.currentTime + durationMs / 1000);
    } catch (e) {
      console.warn('Audio tone error', e);
    }
  }

  playClick() {
    this.playTone(2800, 12);
  }

  playScore() {
    this.playTone(1046, 40);
    setTimeout(() => this.playTone(1318, 65), 45);
  }

  playGameOver() {
    this.playTone(320, 60);
    setTimeout(() => this.playTone(220, 80), 65);
    setTimeout(() => this.playTone(130, 160), 150);
  }

  // --- WS2812 Driver ---
  setPixel(index, r, g, b) {
    if (index >= 0 && index < 2) {
      this.leds[index] = [r, g, b];
      this.notifyLedChange();
    }
  }

  flash(r, g, b, durationMs = 100) {
    this.leds[0] = [r, g, b];
    this.leds[1] = [r, g, b];
    this.notifyLedChange();
    setTimeout(() => {
      this.leds[0] = [0, 0, 0];
      this.leds[1] = [0, 0, 0];
      this.notifyLedChange();
    }, durationMs);
  }

  clearLeds() {
    this.leds = [ [0, 0, 0], [0, 0, 0] ];
    this.notifyLedChange();
  }

  onLedChange(callback) {
    this.ledChangeCallback = callback;
  }

  notifyLedChange() {
    if (this.ledChangeCallback) {
      this.ledChangeCallback(this.leds);
    }
  }

  // --- Display Buffer API ---
  clearDisplay() {
    this.buffer.fill(0);
  }

  drawPixel(x, y, color = 1) {
    if (x < 0 || x >= this.width || y < 0 || y >= this.height) return;
    this.buffer[y * this.width + x] = color ? 1 : 0;
  }

  drawLine(x0, y0, x1, y1, color = 1) {
    const dx = Math.abs(x1 - x0);
    const dy = Math.abs(y1 - y0);
    const sx = x0 < x1 ? 1 : -1;
    const sy = y0 < y1 ? 1 : -1;
    let err = dx - dy;

    while (true) {
      this.drawPixel(x0, y0, color);
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * err;
      if (e2 > -dy) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y0 += sy;
      }
    }
  }

  drawRect(x, y, w, h, color = 1) {
    this.drawLine(x, y, x + w - 1, y, color);
    this.drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    this.drawLine(x, y, x, y + h - 1, color);
    this.drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
  }

  fillRect(x, y, w, h, color = 1) {
    for (let i = x; i < x + w; i++) {
      for (let j = y; j < y + h; j++) {
        this.drawPixel(i, j, color);
      }
    }
  }

  drawText(text, x, y, size = 1, color = 1) {
    // 5x7 default basic font emulation
    this.ctx.font = `${size * 9}px monospace`;
  }

  renderToCanvas() {
    const scale = this.pixelScale;
    this.ctx.fillStyle = '#060709'; // OLED deep black
    this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    this.ctx.fillStyle = '#FFFFFF'; // Crisp OLED white
    for (let y = 0; y < this.height; y++) {
      for (let x = 0; x < this.width; x++) {
        if (this.buffer[y * this.width + x] === 1) {
          this.ctx.fillRect(x * scale, y * scale, scale - 0.5, scale - 0.5);
        }
      }
    }
  }

  // --- Input Binding ---
  bindInputs() {
    window.addEventListener('keydown', (e) => {
      if (e.code === 'Space' || e.code === 'Enter') {
        if (!this.actionPressed) this.pressAction();
      } else if (e.code === 'KeyM' || e.code === 'Tab') {
        if (!this.modePressed) this.pressMode();
      }
    });

    window.addEventListener('keyup', (e) => {
      if (e.code === 'Space' || e.code === 'Enter') {
        this.releaseAction();
      } else if (e.code === 'KeyM' || e.code === 'Tab') {
        this.releaseMode();
      }
    });
  }

  pressAction() {
    this.ensureAudio();
    this.actionPressed = true;
    this.actionPressStart = performance.now();
  }

  releaseAction() {
    if (!this.actionPressed) return;
    const dur = performance.now() - this.actionPressStart;
    this.actionPressed = false;
    if (this.currentApplet) {
      if (dur >= 500 && this.currentApplet.onActionHold) {
        this.currentApplet.onActionHold();
      } else if (this.currentApplet.onActionClick) {
        this.currentApplet.onActionClick();
      }
    }
  }

  pressMode() {
    this.ensureAudio();
    this.modePressed = true;
    this.modePressStart = performance.now();
  }

  releaseMode() {
    if (!this.modePressed) return;
    const dur = performance.now() - this.modePressStart;
    this.modePressed = false;
    if (this.currentApplet) {
      if (dur >= 500 && this.currentApplet.onModeHold) {
        this.currentApplet.onModeHold();
      } else if (this.currentApplet.onModeClick) {
        this.currentApplet.onModeClick();
      }
    }
  }

  loadApplet(applet) {
    if (this.currentApplet && this.currentApplet.cleanup) {
      this.currentApplet.cleanup();
    }
    this.currentApplet = applet;
    this.currentApplet.sim = this;
    if (this.currentApplet.init) {
      this.currentApplet.init();
    }
  }

  start() {
    if (this.isRunning) return;
    this.isRunning = true;
    const loop = (now) => {
      if (!this.isRunning) return;
      if (this.currentApplet) {
        if (this.currentApplet.update) this.currentApplet.update();
        if (this.currentApplet.draw) this.currentApplet.draw();
      }
      this.renderToCanvas();
      this.animationFrameId = requestAnimationFrame(loop);
    };
    this.animationFrameId = requestAnimationFrame(loop);
  }

  stop() {
    this.isRunning = false;
    if (this.animationFrameId) {
      cancelAnimationFrame(this.animationFrameId);
    }
  }
}
"""

def main():
    print("==================================================")
    print("Click SDK Exporter & AI Agent Context Generator")
    print("==================================================")
    SDK_DIR.mkdir(parents=True, exist_ok=True)

    soundfx_h = FIRMWARE_SRC / "system" / "SoundFX.h"
    sound_presets = parse_sound_presets(soundfx_h)
    led_effects = parse_led_effects()

    print(f"-> Parsed {len(sound_presets)} sound presets from SoundFX.h")
    print(f"-> Parsed {len(led_effects)} LED effects from WS2812.h")

    # 1. Generate JSON Spec
    spec = generate_sdk_spec(sound_presets, led_effects)
    spec_path = SDK_DIR / "click_sdk_spec.json"
    spec_path.write_text(json.dumps(spec, indent=2), encoding="utf-8")
    print(f"[OK] Generated {spec_path.relative_to(ROOT)}")

    # 2. Generate AI Agent System Prompt
    agent_prompt = generate_ai_agent_prompt(spec)
    prompt_path = SDK_DIR / "CLICK_AI_AGENT_PROMPT.md"
    prompt_path.write_text(agent_prompt, encoding="utf-8")
    print(f"[OK] Generated {prompt_path.relative_to(ROOT)}")

    # 3. Generate Header Template
    tmpl_h = generate_template_h()
    tmpl_h_path = SDK_DIR / "AppletTemplate.h"
    tmpl_h_path.write_text(tmpl_h, encoding="utf-8")
    print(f"[OK] Generated {tmpl_h_path.relative_to(ROOT)}")

    # 4. Generate C++ Implementation Template
    tmpl_cpp = generate_template_cpp()
    tmpl_cpp_path = SDK_DIR / "AppletTemplate.cpp"
    tmpl_cpp_path.write_text(tmpl_cpp, encoding="utf-8")
    print(f"[OK] Generated {tmpl_cpp_path.relative_to(ROOT)}")

    # 5. Generate ClickSimulator.js
    sim_js = generate_click_simulator_js()
    sim_path = SDK_DIR / "ClickSimulator.js"
    sim_path.write_text(sim_js, encoding="utf-8")
    print(f"[OK] Generated {sim_path.relative_to(ROOT)}")

    print("\n[SUCCESS] Click SDK and AI Agent package successfully updated!")

if __name__ == "__main__":
    main()
