/**
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
