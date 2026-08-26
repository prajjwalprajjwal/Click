#!/usr/bin/env python3
"""Convert Rajdhani TTF to Adafruit GFX font headers."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import freetype
except ImportError:
    print("Install freetype-py: pip install freetype-py")
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TTF = ROOT / "assets" / "fonts" / "Rajdhani-Regular.ttf"
OUT_DIR = ROOT / "include" / "fonts" / "generated"

FIRST = 0x20
LAST = 0x7E


def generate_font(ttf_path: Path, point_size: int, symbol: str) -> None:
    face = freetype.Face(str(ttf_path))
    # 72 DPI standard for Adafruit GFX
    face.set_char_size(point_size * 64, 0, 72, 0)

    bitmap_data: list[int] = []
    glyph_meta: list[tuple[int, int, int, int, int, int]] = []

    current_byte = 0
    bit_count = 0

    for code in range(FIRST, LAST + 1):
        char = chr(code)
        face.load_char(char, freetype.FT_LOAD_TARGET_MONO)
        face.glyph.render(freetype.FT_RENDER_MODE_MONO)

        g = face.glyph
        bmp = g.bitmap
        width = bmp.width
        height = bmp.rows
        x_adv = face.glyph.advance.x >> 6
        x_off = face.glyph.bitmap_left
        y_off = 1 - face.glyph.bitmap_top

        glyph_offset = len(bitmap_data)

        if width == 0 or height == 0:
            glyph_meta.append((glyph_offset, 0, 0, x_adv, 0, 0))
            continue

        raw = bytes(bmp.buffer)
        for y in range(height):
            for x in range(width):
                byte_idx = y * bmp.pitch + (x // 8)
                bit_val = 1 if (raw[byte_idx] & (0x80 >> (x % 8))) else 0
                current_byte = (current_byte << 1) | bit_val
                bit_count += 1
                if bit_count == 8:
                    bitmap_data.append(current_byte)
                    current_byte = 0
                    bit_count = 0

        # Adafruit GFX bit-packing pads to the next byte boundary at the end of each glyph
        if bit_count > 0:
            current_byte <<= (8 - bit_count)
            bitmap_data.append(current_byte)
            current_byte = 0
            bit_count = 0

        glyph_meta.append((glyph_offset, width, height, x_adv, x_off, y_off))

    y_advance = face.size.height >> 6
    if y_advance == 0:
        y_advance = point_size + 4

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    header = OUT_DIR / f"{symbol.lower()}.h"

    lines = [
        f"// Auto-generated from {ttf_path.name} at {point_size}pt",
        f"#ifndef {symbol}_H",
        f"#define {symbol}_H",
        "",
        "#include <Adafruit_GFX.h>",
        "",
        f"const uint8_t {symbol}Bitmaps[] PROGMEM = {{",
    ]

    for i in range(0, len(bitmap_data), 12):
        chunk = bitmap_data[i : i + 12]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")

    lines += [
        "};",
        "",
        f"const GFXglyph {symbol}Glyphs[] PROGMEM = {{",
    ]

    for idx, meta in enumerate(glyph_meta):
        offset, width, height, x_adv, x_off, y_off = meta
        c = chr(FIRST + idx)
        if c == "\\":
            c_disp = "'\\\\'"
        elif c == "'":
            c_disp = "'\\''"
        else:
            c_disp = f"'{c}'"
        lines.append(
            f"    {{ {offset:5d}, {width:3d}, {height:3d}, {x_adv:3d}, {x_off:4d}, {y_off:4d} }},"
            f"   // 0x{FIRST + idx:02X} {c_disp}"
        )

    lines += [
        "};",
        "",
        f"const GFXfont {symbol} PROGMEM = {{",
        f"    (uint8_t*){symbol}Bitmaps,",
        f"    (GFXglyph*){symbol}Glyphs,",
        f"    0x{FIRST:02X}, 0x{LAST:02X}, {y_advance} }};",
        "",
        f"#endif // {symbol}_H",
        "",
    ]

    header.write_text("\n".join(lines), encoding="utf-8")
    print(f"OK {symbol} ({point_size}pt) -> {header}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Rajdhani GFX fonts")
    parser.add_argument("--ttf", type=Path, default=DEFAULT_TTF)
    args = parser.parse_args()

    if not args.ttf.exists():
        print(f"Missing font: {args.ttf}")
        sys.exit(1)

    sizes = [
        (10, "Rajdhani10pt7b"),
        (12, "Rajdhani12pt7b"),
        (18, "Rajdhani18pt7b"),
        (24, "Rajdhani24pt7b"),
        (32, "Rajdhani32pt7b"),
        (40, "Rajdhani40pt7b"),
    ]

    for pt, symbol in sizes:
        generate_font(args.ttf, pt, symbol)


if __name__ == "__main__":
    main()
