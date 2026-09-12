# Click Screen Asset Directory

Drop 1-bit monochrome PNG files into this directory.  
The pre-build pipeline automatically converts them into optimized C PROGMEM bitmap headers on every compilation.

---

## ⚡ Automated Pipeline Integration

Asset conversion is integrated directly into the PlatformIO build system via `platformio.ini`:
```ini
extra_scripts = pre:firmware/scripts/pre_build_assets.py
build_flags = -I firmware/src/generated_assets
```
Whenever `pio run` executes, `pre_build_assets.py` triggers `convert_assets.py` to regenerate any modified graphics before the compiler runs. You do not need to run manual conversion scripts during regular development.

To run conversion manually:
```bash
python firmware/scripts/convert_assets.py
```

---

## Naming Conventions & Symbol Generation

### A — Clicker Milestone Screens *(128×64 px, 1-bit)*
| File | Display Condition | Generated Header | Generated Symbol |
|:---|:---|:---|:---|
| `1.png` | 1st click milestone | `screens/generated/screen_1.h` | `img_1_bmp`, `IMG_1_WIDTH`, `IMG_1_HEIGHT` |
| `5.png` | 5th click milestone | `screens/generated/screen_5.h` | `img_5_bmp`, `IMG_5_WIDTH`, `IMG_5_HEIGHT` |
| `10.png` | 10th click milestone | `screens/generated/screen_10.h` | `img_10_bmp`, `IMG_10_WIDTH`, `IMG_10_HEIGHT` |
| `50.png` | 50th click milestone | `screens/generated/screen_50.h` | `img_50_bmp`, `IMG_50_WIDTH`, `IMG_50_HEIGHT` |
| `100.png` | 100th click milestone | `screens/generated/screen_100.h` | `img_100_bmp`, `IMG_100_WIDTH`, `IMG_100_HEIGHT` |

Milestones are automatically registered in `firmware/src/screens/generated/registry.inc`.

---

### B — App Boot, Start & Game-Over Screens *(128×64 px, 1-bit)*
| File | Used By | Generated Header | Generated Symbol |
|:---|:---|:---|:---|
| `bootscreen.png` | System power-on startup splash | `generated_assets/bootscreen.h` | `bootscreen_bmp` |
| `FlappyBirdApplet_start.png` | Flappy Bird idle/start screen | `generated_assets/FlappyBirdApplet_start.h` | `flappybirdapplet_start_bmp` |
| `FlappyBirdApplet_end.png` | Flappy Bird game-over screen | `generated_assets/FlappyBirdApplet_end.h` | `flappybirdapplet_end_bmp` |
| `justten_start.png` | Just 10 Seconds start screen | `generated_assets/justten_start.h` | `justten_start_bmp` |
| `justten_end.png` | Just 10 Seconds score screen | `generated_assets/justten_end.h` | `justten_end_bmp` |

---

### C — Sprites & Custom Bitmaps *(Arbitrary Dimensions)*
| File | Used By | Generated Header | Generated Symbol |
|:---|:---|:---|:---|
| `FlappyBirdApplet_char.png` (10×8) | Flappy Bird player sprite | `generated_assets/FlappyBirdApplet_char.h` | `flappybirdapplet_char_bmp` |

> Note: Sisyphus boulder and walking/pushing sprite sheets are maintained directly in `firmware/src/clicker/SisyphusSprites.h` for sub-pixel animation performance.

---

## Using Generated Assets in Applets

All app assets are centralized in `all_assets.h`:

```cpp
#include "all_assets.h"  // Pulls in all generated headers

// Example: Draw boot splash
display.drawBitmap(0, 0, bootscreen_bmp, BOOTSCREEN_WIDTH, BOOTSCREEN_HEIGHT, SSD1306_WHITE);

// Example: Draw Flappy Bird start banner
display.drawBitmap(0, 0, flappybirdapplet_start_bmp, 
                   FLAPPYBIRDAPPLET_START_WIDTH, FLAPPYBIRDAPPLET_START_HEIGHT, 
                   SSD1306_WHITE);

// Example: Draw Flappy Bird character sprite
display.drawBitmap(playerX, playerY, flappybirdapplet_char_bmp, 
                   FLAPPYBIRDAPPLET_CHAR_WIDTH, FLAPPYBIRDAPPLET_CHAR_HEIGHT, 
                   SSD1306_WHITE);
```
