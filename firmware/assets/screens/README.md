# Click — Screen Asset Directory

Drop 1-bit monochrome PNG files here.  
`convert_assets.py` will auto-convert them to C PROGMEM headers on every build.

---

## Naming Conventions

### A — Clicker Milestone Images
| File | Displayed at |
|------|-------------|
| `1.png` | 1st click |
| `5.png` | 5th click |
| `10.png` | 10th click |
| `50.png` | 50th click |
| `100.png` | 100th click |

Generated symbol: `img_<N>_bmp`  (e.g. `img_10_bmp`, `IMG_10_WIDTH`, `IMG_10_HEIGHT`)

---

### B — App Start Screens  *(128×64 px, 1-bit)*
| File | Used by |
|------|---------|
| `flappybird_start.png` | Flappy Bird idle/start state |
| `justten_start.png` | Just 10 Seconds idle/start state |
| `clicker_start.png` | Clicker boot screen |

Generated symbol: `<appname>_start_bmp`

---

### C — App End / Game-Over Screens  *(128×64 px, 1-bit)*
| File | Used by |
|------|---------|
| `flappybird_end.png` | Flappy Bird game-over screen |
| `justten_end.png` | Just 10 Seconds result screen |

Generated symbol: `<appname>_end_bmp`

> If no `_end.png` exists for an applet, the applet falls back to its  
> coded game-over UI — no image is shown.

---

### D — Sprites & Icons  *(arbitrary size)*
| File | Used by |
|------|---------|
| `flappybird_icon.png` | Flappy Bird player sprite |

Generated symbol: `<appname>_<name>_bmp`

---

## Using Generated Assets in Applets

```cpp
#include "all_assets.h"  // single include — pulls in everything

// Example: draw a start screen
display.drawBitmap(0, 0, flappybird_start_bmp,
                   FLAPPYBIRD_START_WIDTH, FLAPPYBIRD_START_HEIGHT,
                   SSD1306_WHITE);

// Example: draw a milestone image (click count 10)
display.drawBitmap(0, 0, img_10_bmp,
                   IMG_10_WIDTH, IMG_10_HEIGHT,
                   SSD1306_WHITE);
```
