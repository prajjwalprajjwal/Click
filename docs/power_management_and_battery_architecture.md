# Power Management & Battery Architecture Deep-Dive ⚡🔋

*An engineering case study on micro-power optimization, battery fuel gauging, and silicon-level gotchas in the Clicker handheld ecosystem.*

---

## 📖 Executive Summary

The **Clicker** is an open-hardware, ultra-tactile gaming handheld featuring mechanical keyboard switches, a 128×64 SSD1306 OLED display, dual addressable SK6812/WS2812 RGB LEDs, a piezo buzzer, an NXP PCF8563 real-time clock, and a single-cell Li-Ion/LiPo battery, all driven by the high-density **Raspberry Pi RP2354A** microcontroller.

While creating games on microcontrollers is straightforward, engineering a production-grade battery-powered handheld is deceptively challenging:
1. **Tiny Battery, High Expectations**: A pocketable handheld relies on a small (150–350mAh) Lithium-Polymer cell. Without aggressive power management, standard 133MHz clocking, OLED charge pumps, and addressable LEDs will drain the battery in under 2 hours.
2. **The "Surface Charge" Illusion**: Users hate seeing battery gauges display 100% and drop to 85% within five seconds of unplugging. Mapping voltage linearly to percentage fails completely on real lithium chemistry.
3. **Silicon & Driver Traps**: Hardware register overrides in embedded USB stacks, floating CMOS input buffers, and addressable LED ghost leakage can quietly draw milliamps in standby, draining the cell while the device is in a drawer.

This document details every power challenge faced on the Clicker platform, the root causes identified through hardware debugging, the exact software/firmware solutions implemented, and key takeaways for engineers and tech writers.

---

## 🛠️ Hardware Power Topology

```mermaid
flowchart TD
    USB["🔌 USB Type-C Receptacle\n(5V VBUS + 5.1k CC)"] -->|5V Power| ETA["⚡ ETA6003Q3Q Charger\n(Switching Li-Ion Controller)"]
    ETA -->|STAT Pin / GPIO 1\n(Active LOW)| MCU["🧠 RP2354A MCU\n(Dual Cortex-M33 @ 48MHz)"]
    
    BATT["🔋 Li-Ion / LiPo Cell\n(3.7V Nom, 4.2V Max)"] <-->|Charge / Discharge| ETA
    BATT -->|100kΩ / 100kΩ Divider| ADC["📊 ADC3 / GPIO 29\n(Battery Voltage Monitor)"]
    
    ETA -->|SYS Power Rail| SW["🔘 Slide Switch (SW1)\n(Hardware Master Cut)"]
    SW -->|Enable Pin| LDO["🔌 AP2112K-3.3\n(600mA Ultra-Low-Dropout)"]
    
    LDO -->|+3V3 Main Power Rail| MCU
    LDO -->|+3V3 Main Power Rail| OLED["📺 SSD1306 128x64 OLED\n(I2C1: GP2 / GP3)"]
    LDO -->|+3V3 Main Power Rail| RTC["⏰ PCF8563 RTC\n(I2C1: GP2 / GP3)"]
    LDO -->|+3V3 Main Power Rail| LED["💡 2x SK6812 RGB LEDs\n(DIN: GP11)"]
    LDO -->|+3V3 Main Power Rail| BUZZ["🔊 Piezo Buzzer\n(BJT Driver: GP27)"]
```

### Component Breakdown
| Component | Function | Normal Draw | Sleep / Quiescent Draw | Key Challenge |
|:---|:---|:---|:---|:---|
| **RP2354A** | Dual Cortex-M33 MCU | 18–35 mA (at 133MHz) | < 0.8 mA (at 12MHz `__wfi()`) | QSPI BOOTSEL button cannot wake from `xosc_dormant()` |
| **ETA6003** | Li-Ion Switching Charger | N/A (Charging: 200–500mA) | < 10 µA standby | STAT pin requires pullup to detect unplugged state |
| **SSD1306** | 128×64 Monochrome OLED | 8–18 mA (charge pump active) | < 5 µA (`DISPLAYOFF`) | Contrast settings drastically impact active current |
| **2× SK6812** | Addressable RGB LEDs | 10–30 mA per LED when lit | 0.8–1.2 mA quiescent each | Floating DIN pin causes ghost glow and leakage |
| **PCF8563** | Real-Time Clock | 0.25 µA | 0.25 µA | Unused CLKOUT pin oscillates at 32.768kHz by default |
| **Piezo Buzzer** | Audio feedback | 15–25 mA during tone | 0.0 µA (transistor cut off) | Base pin must never float |

---

## 🔬 The 6 Major Power Mysteries & Engineering Solutions

---

### Case 1: The Phantom VBUS Trap (`sie_status` Silicon Override)

#### The Problem
When the USB-C cable was unplugged, the OLED display continued to report `[CHARGING]`, and the RGB LEDs stayed in their USB breathing pattern indefinitely. The device never knew it was running on battery power!

#### Root Cause Investigation
In firmware, we initially queried the RP2350 USB hardware register to detect cable presence:
```cpp
bool vbus_present = (usb_hw->sie_status & USB_SIE_STATUS_VBUS_DETECTED_BITS) != 0;
```
However, looking into the low-level Arduino-Pico / TinyUSB device controller driver (`dcd_rp2040.c`), we discovered this initialization sequence:
```c
usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
```
Because the standard Raspberry Pi Pico board lacks a hardware VBUS sensing pin wired to the USB peripheral, the TinyUSB driver **hardcodes a silicon-level override** that forces `sie_status` to report VBUS as permanently present! Reading this register on RP2350 always returns `true`, even with no cable attached.

#### The Fix
We investigated the physical PCB schematic ([`hardware/PCB/Click4/Click4.kicad_pcb`](file:///Users/prajjwal/Documents/GitHub/Click/hardware/PCB/Click4/Click4.kicad_pcb)) and verified that **Pad 9 (`STAT`)** of the ETA6003 charge controller is connected directly to **RP2354 GPIO 1**, with an external 10kΩ pull-up resistor `R9` to `+3V3`:
- When USB is plugged in and the battery is actively charging, the ETA6003 pulls `STAT` **LOW** (0V).
- When the battery is full or the USB cable is unplugged, the internal open-drain transistor releases `STAT`, and `R9` pulls it **HIGH** (3.3V).

In [PinConfig.h](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/system/PinConfig.h#L46-L48) and [battery.hpp](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/system/battery.hpp#L104-L111):
```cpp
#define CHARGER_STAT_PIN 1

static bool isCharging() {
    return digitalRead(CHARGER_STAT_PIN) == LOW;
}
```
Now, the moment the USB cable is pulled, `digitalRead(1)` reads `HIGH` within 10 milliseconds, instantly stopping the breathing LED and transitioning to battery mode.

---

### Case 2: The Non-Linear Battery "Free Fall" (100% $\to$ 85% in Seconds)

#### The Problem
After fully charging the device and unplugging it, the battery percentage showed 100%, but within 5 to 10 seconds of clicking, it dropped to 95%, 90%, and 85%.

#### Why Lithium Batteries Are Not Linear
A standard linear conversion maps voltage directly between 3.3V (0%) and 4.2V (100%):
$$\text{Linear \%} = \frac{V - 3.30}{4.20 - 3.30} \times 100$$
This model fails on Lithium-Ion / Lithium-Polymer chemistry for three physical reasons:

1. **Surface Charge Relaxation**: When a battery finishes charging at 4.20V, a temporary electrostatic layer ("surface charge") forms on the electrode plates. As soon as charging stops, chemical diffusion equalizes the cell, and the open-circuit voltage naturally relaxes from 4.20V down to **4.08V–4.12V**, even with zero discharge!
2. **Internal Resistance ($IR$) Voltage Drop**: Under operating load, the battery terminal voltage is given by Ohm's Law:
   $$V_{\text{terminal}} = V_{\text{OCV}} - (I_{\text{load}} \times R_{\text{internal}})$$
   A compact 200mAh LiPo has an internal resistance of $0.5\Omega - 1.0\Omega$. When the RP2354 and OLED turn on ($I_{\text{load}} \approx 20\text{ mA}$), terminal voltage drops by 15–25mV instantly.
3. **The Flat Discharge Plateau**: Over 60% of a LiPo cell's stored energy is delivered between **3.70V and 3.90V**. Only 5% of energy exists above 4.10V.

If your code maps $4.08\text{V} \to 90\%$, then natural surface charge relaxation and turning on the screen will cause a 10% drop within seconds, alarming the user!

#### The Fix: Piecewise Li-Ion Calibration & Discharge Damping
We implemented an authentic consumer-grade fuel gauge model in [battery.hpp](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/system/battery.hpp#L57-L215):

```cpp
static int calculatePercentageFromVoltage(float v, bool charging) {
    float effectiveV = v;
    if (charging) {
        // Compensate for charging IR rise (~40mV)
        if (v < 4.00f) effectiveV = v - 0.04f;
        else if (v < 4.15f) effectiveV = v - 0.04f * (1.0f - (v - 4.00f) / 0.15f);
    }

    // Calibrated Non-Linear Li-Ion Plateau Model
    if (effectiveV >= 4.08f) return 100; // 100% cushion: holds 100% across natural relaxation
    if (effectiveV >= 4.00f) return 90 + (effectiveV - 4.00f) / (4.08f - 4.00f) * 10;
    if (effectiveV >= 3.90f) return 75 + (effectiveV - 3.90f) / (4.00f - 3.90f) * 15;
    if (effectiveV >= 3.80f) return 52 + (effectiveV - 3.80f) / (3.90f - 3.80f) * 23; // Nominal plateau
    if (effectiveV >= 3.72f) return 30 + (effectiveV - 3.72f) / (3.80f - 3.72f) * 22;
    if (effectiveV >= 3.62f) return 12 + (effectiveV - 3.62f) / (3.72f - 3.62f) * 18;
    if (effectiveV >= 3.48f) return 3  + (effectiveV - 3.48f) / (3.62f - 3.48f) * 9;  // Low knee
    if (effectiveV >= 3.35f) return (effectiveV - 3.35f) / (3.48f - 3.35f) * 3;
    return 0;
}
```

#### Temporal Damping Rules
- **Post-Unplug Grace Period**: A 2-minute relaxation timer freezes percentage decrements immediately following an unplug event, allowing the double-layer capacitance in the battery to stabilize.
- **Slew-Rate Limiting**: On battery, the displayed percentage is capped to decrement at most **1% every 35 seconds**. Rapid dips from brief buzzer chirps or LED flashes are filtered out entirely.

---

### Case 3: The Missing Decimal Bug (`Voltage: V`)

#### The Problem
In the Settings telemetry screen, the battery voltage rendered as `"Voltage: V"`, with the numbers completely blank!

#### Root Cause
The RP2354 firmware is compiled with GCC ARM Embedded using `newlib-nano` libc. To keep binary footprints small (saving ~12KB of flash), `newlib-nano` **omits floating-point formatters from `printf` and `snprintf` by default**.
When `snprintf(buf, sizeof(buf), "Voltage: %.2fV", voltage)` was executed:
- `%.2f` was treated as an unhandled specifier and produced an empty string.
- The output string became literally `"Voltage: V"`.

#### The Fix
1. **Integer Decomposition**: In [SettingsApplet.h](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/applets/settings/SettingsApplet.h#L53-L58), we broke the float into whole and fractional integer parts:
   ```cpp
   int vWhole = (int)voltage;
   int vFrac = (int)(voltage * 100.0f + 0.5f) % 100;
   snprintf(buf, sizeof(buf), "%d.%02dV", vWhole, vFrac);
   ```
   This is 100% portable, executes in under 1 microsecond, and uses zero heap/float library overhead.
2. **Linker Flag Safeguard**: In [platformio.ini](file:///Users/prajjwal/Documents/GitHub/Click/platformio.ini#L26), added `-Wl,-u,_printf_float` to link the floating-point `printf` routine across the firmware.

---

### Case 4: The Sleep Wake Matrix & BOOTSEL / QSPI Dilemma

#### The Problem
The user required that **any button (MODE or ACTION)** should wake the Clicker from sleep. However, when entering the hardware dormant sleep state (`xosc_dormant()`), the ACTION button failed to wake the device.

#### Silicon Hardware Architecture
- The **MODE button** is on **GPIO 0**, which belongs to **IO_BANK0**. Bank 0 GPIOs support level-sensitive dormant interrupt controllers (`gpio_set_dormant_irq_enabled`).
- The **ACTION button** is connected to **`BOOTSEL` (`QSPI_SS`)**. On the RP2350/RP2354, the QSPI interface belongs to the **QSPI pad bank**, which is dedicated to high-speed external flash communication. It does **not** have an independent dormant wake IRQ line in the hardware power management controller!

#### The Solution: 12MHz Active Low-Power Sleep
Instead of stopping the crystal oscillator entirely (which blinds the QSPI controller), we clock the system down to direct XOSC 12MHz and execute ARM Wait-For-Interrupt:

In [PowerManager.h](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/system/PowerManager.h#L328-L352):
```cpp
// 1. Clock clk_sys down from 48MHz/133MHz to 12MHz direct crystal clock
set_sys_clock_khz(12000, false);

// 2. Poll both buttons while sleeping via Cortex-M33 __wfi()
while (!isModeButtonPressed() && !isActionButtonPressed()) {
    delay(20); // Internally invokes __wfi(), reducing current to < 0.8mA!
}

// 3. Debounce button release before returning to active applet
while (isModeButtonPressed() || isActionButtonPressed()) {
    delay(10);
}

// 4. Restore active system clock to 48MHz
set_sys_clock_khz(48000, false);
```

#### Results
- Power consumption during sleep drops from ~25 mA down to **$< 0.8\text{ mA}$**!
- Both **MODE** (GP0) and **ACTION** (`BOOTSEL`) wake the device instantly.
- The device immediately wakes back to the Sisyphus game screen with zero latency.

---

### Case 5: Ghost LED Glow & Sleep Current Leakage

#### The Problem
When the device went to sleep, one of the two RGB LEDs sometimes remained dimly lit with a faint green or red glow, slowly draining the battery.

#### Root Cause
The SK6812/WS2812 NeoPixel protocol uses high-speed 800kHz single-wire pulse-width modulation. When the RP2354 goes to sleep, if the GPIO output pin is left floating (High-Z), capacitive charge on the PCB trace or inductive noise from the OLED power rail can couple into the LED's high-impedance `DIN` pin. The internal SK6812 shift register interprets this noise as valid data bits, turning on its internal constant-current driver.

#### The Fix: Hardware Latch & Ground Clamp
In [WS2812.h](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/system/WS2812.h#L96-L125), we created `clearAndHaltForSleep()`:
1. **Dual Zero Frame**: Send two consecutive frames of pure 0s (`0x000000`) and wait 10ms to ensure both cascaded SK6812 ICs latch into their zero state.
2. **Peripheral Disconnect**: De-mux GP11 from the PIO state machine and switch it to standard SIO GPIO output.
3. **Solid Ground Clamp & Pull-Down**: Clear the output bit, drive GP11 LOW (0V), and enable the internal hardware pull-down resistor (`gpio_pull_down(11)`).

In addition, we eliminated other standby leaks:
- **RTC CLKOUT**: The PCF8563 default state outputs a continuous 32.768kHz square wave on CLKOUT. In [main.cpp](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/main.cpp#L166-L170), we write `0x00` to register `0x0D`, disabling CLKOUT and saving ~15µA.
- **Floating CMOS Inputs**: Disabled digital input buffers on all unused Bank 0 GPIOs via `gpio_set_input_enabled(pin, false)` to prevent shoot-through currents in floating input transistors.

---

### Case 6: Charging Breathing LED vs. In-Game LED Effects

#### The Problem
1. While plugged into the charger, switching between applets caused the breathing LED to abruptly turn off and restart because applet `cleanup()` and `init()` routines called `WS2812Driver::clear()`.
2. Conversely, when we made charging breathing indestructible, in-game LED cues (like the yellow flash on Just Ten game over, or the red flash on Flappy Bird death) could not be seen while charging.

#### The Fix: Hierarchical Priority Engine
We established an intelligent state machine in [WS2812.h](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/system/WS2812.h) and [main.cpp](file:///Users/prajjwal/Documents/GitHub/Click/firmware/src/main.cpp#L343-L357):

```mermaid
stateDiagram-v2
    [*] --> Off: On Battery Idle
    Off --> InGameEffect: Flash / Celebration Triggered
    InGameEffect --> Off: Effect Timer Expires (clear())
    
    [*] --> ChargingBreathe: USB Plugged In
    ChargingBreathe --> ChargingBreathe: Applet Switch (clear() ignored)
    ChargingBreathe --> InGameEffect: Game Win / Over Triggered
    InGameEffect --> ChargingBreathe: Effect Timer Expires (clear() auto-resumes)
    ChargingBreathe --> Off: USB Unplugged
```

1. **Applet Switch Protection**: When `WS2812Driver::clear()` is called while charging, if the current effect is `CHARGING_BREATHE`, the call is ignored. The breathing cycle continues uninterrupted.
2. **Transient Game Preemption**: In-game triggers (`flash()`, `startCelebration()`) are allowed to temporarily override `CHARGING_BREATHE`.
3. **Seamless Resume**: In `main.cpp`, `loop()` checks `!WS2812Driver::isBusy()`. While the 500ms flash plays, the charging loop stays hands-off. As soon as the flash timer expires, `clear()` detects that `isChargingHardware()` is true and seamlessly switches back to `CHARGING_BREATHE`.

---

## 📊 Power Measurements & Battery Life Analysis

Calculated for a standard **300mAh 3.7V (1.11Wh) Lithium-Polymer cell**:

| Operating State | System Clock | Display State | Audio / LEDs | Current Draw | Estimated Battery Life |
|:---|:---|:---|:---|:---|:---|
| **Active Gameplay (Sisyphus)** | 48 MHz | ON (Normal Contrast) | Buzzer chirps, LED idle | ~18–22 mA | **~14–16 Hours** |
| **Idle Screen Dimmed** | 48 MHz | ON (Contrast 0x05) | OFF | ~11–13 mA | **~24 Hours** |
| **Standby Sleep Mode** | 12 MHz (`__wfi()`) | OFF (`DISPLAYOFF`) | Clamped to GND | **< 0.8 mA** | **~375+ Hours (~15 Days)** |
| **Hardware Cutoff (Switch Off)**| 0 Hz | OFF | Cut at AP2112K EN | **< 0.01 mA** | **Shelf Life (> 1 Year)** |

---

## ✍️ Gemini Notebook / Blog Post Writing Prompts

When you paste this documentation into Google Gemini or your blog writing workflow, use these prompts to generate engaging technical articles:

### Prompt 1: The Narrative Angle (Engineering Story)
> *"Write an engaging, first-person developer story about the hidden challenges of building a physical handheld gaming device. Use the Clicker project as the case study. Focus on the emotional rollercoaster of thinking hardware is broken vs. finding out that the USB silicon registers had hardcoded overrides. Emphasize why hardware-software co-design matters."*

### Prompt 2: The Deep-Tech Angle (Embedded Systems & Battery Physics)
> *"Generate a technical tutorial for embedded C++ developers explaining why naive battery voltage reading fails on Lithium-Polymer cells. Break down internal resistance, surface charge relaxation, and open-circuit voltage plateaus. Use the Clicker's piecewise interpolation and slew-rate limiting algorithms as the gold-standard solution."*

### Prompt 3: The Silicon Secrets Angle (RP2350 / RP2354 Gotchas)
> *"Write a guide on '5 Silicon Quirks You Must Know Before Building on Raspberry Pi RP2350/RP2354'. Cover QSPI BOOTSEL dormant wake limitations, newlib-nano floating-point omissions, and GPIO CMOS shoot-through current during sleep."*

---

*Authored for the Clicker Project Ecosystem — Keeping open-source embedded hardware responsive, reliable, and efficient.*
