#!/usr/bin/env python3
"""
Convert PNG images to SSD1306 OLED bitmap headers for the Clicker firmware.

Usage:
  python tools/png_to_oled.py                    # convert all assets/screens/*.png
  python tools/png_to_oled.py assets/screens/my_art.png
  python tools/png_to_oled.py --defaults         # regenerate built-in celebration screens

Output: include/screens/generated/<name>.h
Registry: include/screens/generated/registry.inc (included by ScreenRegistry.cpp)

Images are resized to 128x64, converted to 1-bit, Adafruit GFX drawBitmap format.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install pillow")
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "screens"
OUT_DIR = ROOT / "include" / "screens" / "generated"
REGISTRY = OUT_DIR / "registry.inc"

W, H = 128, 64


def sanitize(name: str) -> str:
    name = Path(name).stem
    name = re.sub(r"[^a-zA-Z0-9_]", "_", name)
    if not name or name[0].isdigit():
        name = "screen_" + name
    return name.upper()


def image_to_bitmap(img: Image.Image, invert: bool = False) -> list[int]:
    img = img.convert("L").resize((W, H), Image.Resampling.LANCZOS)
    row_bytes = (W + 7) // 8
    out: list[int] = []
    for y in range(H):
        for bx in range(row_bytes):
            byte = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x >= W:
                    continue
                pixel = img.getpixel((x, y))
                lit = pixel <= 127 if invert else pixel > 127
                if lit:
                    byte |= 0x80 >> bit
            out.append(byte)
    return out


def write_header(symbol: str, bitmap: list[int], source: str) -> Path:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / f"{symbol.lower()}.h"
    lines = [
        f"// Auto-generated from {source}",
        f"#ifndef {symbol}_H",
        f"#define {symbol}_H",
        "",
        "#include <Arduino.h>",
        "#include <stdint.h>",
        "",
        f"static const uint8_t {symbol}_DATA[] PROGMEM = {{",
    ]
    for i in range(0, len(bitmap), 16):
        chunk = bitmap[i : i + 16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines += [
        "};",
        "",
        f"#define {symbol}_WIDTH 128",
        f"#define {symbol}_HEIGHT 64",
        "",
        f"#endif // {symbol}_H",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def set_pixel(bitmap: list[int], x: int, y: int) -> None:
    if x < 0 or x >= W or y < 0 or y >= H:
        return
    row_bytes = (W + 7) // 8
    idx = y * row_bytes + x // 8
    bitmap[idx] |= 0x80 >> (x % 8)


def blank_bitmap() -> list[int]:
    return [0] * ((W + 7) // 8 * H)


def generate_defaults() -> list[tuple[str, str, bool]]:
    entries: list[tuple[str, str, bool]] = []

    # PIX — four pixels (CLIX-style origin)
    bm = blank_bitmap()
    for px, py in [(62, 48), (66, 48), (62, 52), (66, 52)]:
        set_pixel(bm, px, py)
    write_header("SCREEN_PIX_FOUR", bm, "built-in pix_four")
    entries.append(("SCREEN_PIX_FOUR", "10", False))

    # Signal at 100 — radiating corners
    bm = blank_bitmap()
    for i in range(0, 128, 8):
        set_pixel(bm, i, 0)
        set_pixel(bm, i, 63)
    for i in range(0, 64, 8):
        set_pixel(bm, 0, i)
        set_pixel(bm, 127, i)
    for d in range(-20, 21, 4):
        set_pixel(bm, 64 + d, 32 + abs(d) // 2)
        set_pixel(bm, 64 + d, 32 - abs(d) // 2)
    write_header("SCREEN_SIGNAL_100", bm, "built-in signal_100")
    entries.append(("SCREEN_SIGNAL_100", "100", False))

    # Pattern at 5000 — diamond symbol
    bm = blank_bitmap()
    cx, cy = 64, 32
    for r in range(4, 28, 3):
        for a in range(0, 360, 15):
            import math
            x = int(cx + r * math.cos(math.radians(a)))
            y = int(cy + r * math.sin(math.radians(a)) * 0.6)
            set_pixel(bm, x, y)
    for dx in range(-4, 5):
        for dy in range(-4, 5):
            if abs(dx) + abs(dy) <= 5:
                set_pixel(bm, cx + dx, cy + dy)
    write_header("SCREEN_PATTERN_5000", bm, "built-in pattern_5000")
    entries.append(("SCREEN_PATTERN_5000", "5000", False))

    # Awake at 10000 — burst field
    bm = blank_bitmap()
    for ring in range(2, 30, 4):
        for a in range(0, 360, 20):
            import math
            x = int(64 + ring * math.cos(math.radians(a)))
            y = int(32 + ring * math.sin(math.radians(a)))
            set_pixel(bm, x, y)
    write_header("SCREEN_AWAKE_10000", bm, "built-in awake_10000")
    entries.append(("SCREEN_AWAKE_10000", "10000", False))

    # Halfway 50000 — horizon + sun
    bm = blank_bitmap()
    for x in range(128):
        set_pixel(bm, x, 42)
        if x % 6 == 0:
            set_pixel(bm, x, 41)
    for r in range(8):
        for a in range(0, 360, 12):
            import math
            x = int(64 + r * math.cos(math.radians(a)))
            y = int(22 + r * math.sin(math.radians(a)))
            set_pixel(bm, x, y)
    write_header("SCREEN_HALFWAY_50000", bm, "built-in halfway_50000")
    entries.append(("SCREEN_HALFWAY_50000", "50000", False))

    # Complete 100000 — filled diamond frame
    bm = blank_bitmap()
    for t in range(-30, 31):
        set_pixel(bm, 64 + t, 32 + abs(t) // 2)
        set_pixel(bm, 64 + t, 32 - abs(t) // 2)
    for x in range(20, 108):
        if x % 5 == 0:
            set_pixel(bm, x, 8)
            set_pixel(bm, x, 56)
    write_header("SCREEN_COMPLETE_100000", bm, "built-in complete_100000")
    entries.append(("SCREEN_COMPLETE_100000", "100000", False))

    return entries


def convert_png(path: Path, invert: bool = False) -> tuple[str, str] | None:
    symbol = sanitize(path.name)
    bm = image_to_bitmap(Image.open(path), invert=invert)
    write_header(symbol, bm, path.name)
    m = re.search(r"(\d+)", path.stem)
    clicks = m.group(1) if m else "0"
    return (symbol, clicks)


def write_registry(entries: list[tuple[str, str]]) -> None:
    lines = [
        "// Auto-generated — do not edit. Run tools/png_to_oled.py",
        "#ifndef SCREEN_REGISTRY_INC",
        "#define SCREEN_REGISTRY_INC",
        "#include <stddef.h>",
        '#include "screens/OledScreen.h"',
        "",
    ]
    seen_symbols = set()
    by_clicks: dict[int, tuple[str, str]] = {}
    for symbol, clicks in entries:
        if clicks == "0":
            continue
        by_clicks[int(clicks)] = (symbol, clicks)

    for _clicks, (symbol, _c) in sorted(by_clicks.items()):
        if symbol in seen_symbols:
            continue
        seen_symbols.add(symbol)
        lines.append(f'#include "screens/generated/{symbol.lower()}.h"')
    lines.append("")
    lines.append("static const CelebrationScreen CELEBRATION_SCREENS[] = {")
    for clicks, (symbol, clicks_str) in sorted(by_clicks.items()):
        lines.append(
            f"    {{ {clicks_str}ULL, {symbol}_DATA, {symbol}_WIDTH, {symbol}_HEIGHT, \"{symbol}\" }},"
        )
    lines.append("};")
    lines.append("")
    lines.append("static const size_t CELEBRATION_SCREEN_COUNT = sizeof(CELEBRATION_SCREENS) / sizeof(CELEBRATION_SCREENS[0]);")
    lines.append("")
    lines.append("#endif // SCREEN_REGISTRY_INC")
    lines.append("")
    REGISTRY.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert PNG to OLED bitmap headers")
    parser.add_argument("files", nargs="*", help="PNG files (default: assets/screens/*.png)")
    parser.add_argument("--defaults", action="store_true", help="Regenerate built-in screens")
    parser.add_argument("--invert", action="store_true", help="Invert colors (for dark art on white background)")
    args = parser.parse_args()

    entries: dict[int, tuple[str, str]] = {}

    if args.defaults:
        for symbol, clicks, _user_art in generate_defaults():
            entries[int(clicks)] = (symbol, clicks)

    paths: list[Path] = []
    if args.files:
        paths = [Path(p) for p in args.files]
    elif ASSETS.exists():
        paths = sorted(ASSETS.glob("*.png"))

    for path in paths:
        if not path.exists():
            print(f"Skip missing: {path}")
            continue
        result = convert_png(path, invert=args.invert)
        if result:
            symbol, clicks = result
            entries[int(clicks)] = (symbol, clicks)
            print(f"OK {path.name} -> {result[0]} (clicks={clicks})")

    if not entries:
        print("No screens generated.")
        return

    write_registry(list(entries.values()))
    print(f"Registry: {REGISTRY}")


if __name__ == "__main__":
    main()
