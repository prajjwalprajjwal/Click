#!/usr/bin/env python3
"""
===========================================================================
convert_assets.py -- Click Firmware Unified Asset Conversion Pipeline
===========================================================================
Scans firmware/assets/screens/ for PNG files and converts them into:

  1. PROGMEM .h headers in firmware/src/generated_assets/
     (for applet start/end screens and sprites)

  2. CelebrationScreen .h + registry.inc update in firmware/src/screens/generated/
     (for CounterApplet milestone images)

  3. all_assets.h master include in firmware/src/generated_assets/

  4. Automatically updates timestamps of relevant .cpp files in firmware/src/
     so PlatformIO / SCons always recompiles changed images without caching old UI!

===========================================================================
"""

import os
import re
import glob
import sys

try:
    from PIL import Image
except ImportError:
    print("[convert_assets] ERROR: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR      = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
ASSETS_DIR       = os.path.join(PROJECT_DIR, "firmware", "assets", "screens")
GEN_ASSETS_DIR   = os.path.join(PROJECT_DIR, "firmware", "src", "generated_assets")
GEN_SCREENS_DIR  = os.path.join(PROJECT_DIR, "firmware", "src", "screens", "generated")
SRC_DIR          = os.path.join(PROJECT_DIR, "firmware", "src")

MILESTONE_RE = re.compile(r'^\d+$')   # e.g. "10", "1", "100"

# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

def make_c_id(stem):
    """Convert filename stem to a valid C identifier."""
    ident = re.sub(r'[^a-zA-Z0-9_]', '_', stem)
    if ident and ident[0].isdigit():
        ident = "img_" + ident
    return ident


def validate_name(stem):
    if MILESTONE_RE.match(stem):
        return  # OK: milestone number e.g. "10"
    if stem.lower() in ("bootscreen", "boot_screen", "boot"):
        return  # OK: bootscreen asset
    if re.match(r'^[A-Za-z][A-Za-z0-9]*(_[A-Za-z0-9]+)+$', stem):
        return
    print("  [WARN] '{}' does not match a known naming convention.".format(stem))
    print("         Expected: <number>.png  OR  bootscreen.png  OR  <ClassName>_start/end/<sprite>.png")


def touch_related_cpp(stem):
    """
    Touch corresponding .cpp files in firmware/src/ so PlatformIO / SCons
    guarantees that object files are recompiled with the updated bitmap.
    """
    cpp_to_touch = []

    if stem.lower() in ("bootscreen", "boot_screen", "boot"):
        cpp_to_touch.append(os.path.join(SRC_DIR, "main.cpp"))
    elif stem.startswith("FlappyBirdApplet"):
        cpp_to_touch.append(os.path.join(SRC_DIR, "FlappyBirdApplet.cpp"))
    elif stem.startswith("TimingGameApplet"):
        cpp_to_touch.append(os.path.join(SRC_DIR, "TimingGameApplet.cpp"))
    elif stem.startswith("HomeApplet"):
        cpp_to_touch.append(os.path.join(SRC_DIR, "HomeApplet.cpp"))
    elif stem.startswith("CounterApplet") or MILESTONE_RE.match(stem):
        cpp_to_touch.append(os.path.join(SRC_DIR, "CounterApplet.cpp"))
        cpp_to_touch.append(os.path.join(SRC_DIR, "screens", "OledScreen.cpp"))
    else:
        # Fallback: check if AppName.cpp exists
        app_name = stem.split("_")[0]
        possible_cpp = os.path.join(SRC_DIR, app_name + ".cpp")
        if os.path.exists(possible_cpp):
            cpp_to_touch.append(possible_cpp)
        else:
            cpp_to_touch.append(os.path.join(SRC_DIR, "main.cpp"))

    for p in cpp_to_touch:
        if os.path.exists(p):
            try:
                os.utime(p, None)
                print("  [TOUCH] {} (recompile triggered)".format(os.path.basename(p)))
            except Exception:
                pass


def write_file_if_changed(out_path, new_content, stem):
    """
    Writes new_content to out_path. If file is new or content changed,
    touches the matching .cpp source file to trigger a clean recompile.
    """
    changed = True
    if os.path.exists(out_path):
        try:
            with open(out_path, "r", encoding="utf-8") as f:
                old_content = f.read()
            if old_content == new_content:
                changed = False
        except Exception:
            changed = True

    if changed:
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(new_content)
        touch_related_cpp(stem)

    return changed


def load_and_preprocess_image(png_path, is_fullscreen=False):
    """
    Loads a PNG and converts it to a 1-bit monochrome PIL Image for OLED SSD1306.

    Handles:
      - RGBA / transparency: composites over solid black (alpha=0 -> black/OFF, opaque white -> white/ON)
      - Grayscale / RGB: converts to grayscale and thresholds (> 64 -> white/ON)
      - Fullscreen (_start, _end, bootscreen, milestone): resizes to exactly 128x64 if necessary
    """
    raw = Image.open(png_path)

    if raw.mode in ('RGBA', 'LA') or (raw.mode == 'P' and 'transparency' in raw.info):
        rgba = raw.convert('RGBA')
        bg = Image.new('RGBA', rgba.size, (0, 0, 0, 255))
        composite = Image.alpha_composite(bg, rgba).convert('L')
    else:
        composite = raw.convert('L')

    if is_fullscreen and composite.size != (128, 64):
        orig = composite.size
        composite = composite.resize((128, 64), Image.LANCZOS)
        print("  [FIX] {} resized {}x{} -> 128x64 to match display".format(
            os.path.basename(png_path), orig[0], orig[1]))

    # Threshold: > 64 brightness is OLED ON (1 / 255), <= 64 is OLED OFF (0)
    bw_img = composite.point(lambda p: 255 if p > 64 else 0, mode='1')
    return bw_img


def image_to_xbm_bytes(img):
    """
    Convert a 1-bit PIL Image to Adafruit GFX PROGMEM byte array.

    Adafruit GFX drawBitmap() format:
      - MSB first: bit 7 of each byte = leftmost pixel in that group of 8
      - White pixel (255) = bit 1 = OLED pixel ON
      - Black pixel (0)   = bit 0 = OLED pixel OFF
      - Rows are byte-aligned (padded to next byte boundary)
    """
    width, height = img.size
    bytes_per_row = (width + 7) // 8
    byte_array = []
    for y in range(height):
        for b in range(bytes_per_row):
            byte_val = 0
            for bit in range(8):
                x = b * 8 + bit
                if x < width:
                    pixel = img.getpixel((x, y))
                    if pixel != 0:
                        byte_val |= (0x80 >> bit)  # MSB-first: bit7=leftmost
            byte_array.append(byte_val)
    return width, height, byte_array


def format_hex_block(byte_array, indent="  "):
    lines = []
    for i in range(0, len(byte_array), 16):
        chunk = byte_array[i:i + 16]
        lines.append(indent + ", ".join("0x{:02X}".format(b) for b in chunk))
    return ",\n".join(lines)


# ---------------------------------------------------------------------------
# Output A: Milestone image → screens/generated/screen_<N>.h
# ---------------------------------------------------------------------------

def milestone_to_screen_header(png_path, output_dir):
    """Convert a milestone PNG (e.g. 10.png) to a CelebrationScreen .h file."""
    img = load_and_preprocess_image(png_path, is_fullscreen=True)
    width, height, byte_array = image_to_xbm_bytes(img)

    stem  = os.path.splitext(os.path.basename(png_path))[0]   # "10"
    macro = "SCREEN_{}".format(stem.upper())   # "SCREEN_10"
    guard = "{}_H".format(macro)               # "SCREEN_10_H"
    fname = "screen_{}.h".format(stem)         # "screen_10.h"

    hex_block = format_hex_block(byte_array)

    content = (
        "// AUTO-GENERATED by convert_assets.py -- DO NOT EDIT MANUALLY\n"
        "// Source: {src}  ({w}x{h} px) -- CounterApplet milestone at {n} clicks\n"
        "\n"
        "#ifndef {guard}\n"
        "#define {guard}\n"
        "\n"
        "#include <Arduino.h>\n"
        "\n"
        "#define {macro}_WIDTH   {w}\n"
        "#define {macro}_HEIGHT  {h}\n"
        "\n"
        "static const uint8_t {macro}_DATA[] PROGMEM = {{\n"
        "{data}\n"
        "}};\n"
        "\n"
        "#endif // {guard}\n"
    ).format(
        src=os.path.basename(png_path), w=width, h=height, n=stem,
        guard=guard, macro=macro, data=hex_block
    )

    os.makedirs(output_dir, exist_ok=True)
    out_path = os.path.join(output_dir, fname)
    write_file_if_changed(out_path, content, stem)

    return stem, fname, macro, width, height


def regenerate_registry(output_dir, milestone_entries):
    """
    Regenerate registry.inc from all milestone entries collected this run.
    milestone_entries: list of (n_str, fname, macro, width, height)
    """
    milestone_entries.sort(key=lambda e: int(e[0]))

    includes = "\n".join(
        '#include "screens/generated/{}"'.format(e[1]) for e in milestone_entries
    )

    screens = "\n".join(
        "    {{ {}ULL, {}_DATA, {}_WIDTH, {}_HEIGHT, \"{}\" }},".format(
            e[0], e[2], e[2], e[2], e[2]
        )
        for e in milestone_entries
    )

    count = len(milestone_entries)

    content = (
        "// AUTO-GENERATED by convert_assets.py -- DO NOT EDIT MANUALLY\n"
        "#ifndef SCREEN_REGISTRY_INC\n"
        "#define SCREEN_REGISTRY_INC\n"
        "\n"
        "#include <stddef.h>\n"
        '#include "screens/OledScreen.h"\n'
        "\n"
        "{includes}\n"
        "\n"
        "static const CelebrationScreen CELEBRATION_SCREENS[] = {{\n"
        "{screens}\n"
        "}};\n"
        "\n"
        "static const size_t CELEBRATION_SCREEN_COUNT = {count};\n"
        "\n"
        "#endif // SCREEN_REGISTRY_INC\n"
    ).format(includes=includes, screens=screens, count=count)

    registry_path = os.path.join(output_dir, "registry.inc")
    write_file_if_changed(registry_path, content, "CounterApplet")
    print("  [OK] registry.inc updated ({} milestone screens)".format(count))


# ---------------------------------------------------------------------------
# Output B: App start/end/sprite PNG → generated_assets/<stem>.h
# ---------------------------------------------------------------------------

def asset_to_header(png_path, output_dir):
    """Convert an app asset PNG to a PROGMEM .h file in generated_assets/."""
    stem = os.path.splitext(os.path.basename(png_path))[0]
    is_fullscreen = stem.endswith("_start") or stem.endswith("_end") or stem.lower() in ("bootscreen", "boot_screen", "boot")

    img = load_and_preprocess_image(png_path, is_fullscreen=is_fullscreen)
    width, height, byte_array = image_to_xbm_bytes(img)

    c_id     = make_c_id(stem)
    var_name = c_id + "_bmp"
    w_name   = c_id.upper() + "_WIDTH"
    h_name   = c_id.upper() + "_HEIGHT"
    guard    = "GENERATED_{}_H".format(c_id.upper())
    fname    = stem + ".h"
    out_path = os.path.join(output_dir, fname)

    hex_block = format_hex_block(byte_array)

    content = (
        "// AUTO-GENERATED by convert_assets.py -- DO NOT EDIT MANUALLY\n"
        "// Source: {src}  ({w}x{h} px)\n"
        "//\n"
        "// Usage:\n"
        "//   display.drawBitmap(x, y, {var}, {wn}, {hn}, SSD1306_WHITE);\n"
        "\n"
        "#ifndef {guard}\n"
        "#define {guard}\n"
        "\n"
        "#include <Arduino.h>\n"
        "\n"
        "#define {wn}  {w}\n"
        "#define {hn}  {h}\n"
        "\n"
        "// Adafruit GFX monochrome bitmap (MSB-first, 1 = OLED pixel ON)\n"
        "const uint8_t {var}[] PROGMEM = {{\n"
        "{data}\n"
        "}};\n"
        "\n"
        "#endif // {guard}\n"
    ).format(
        src=os.path.basename(png_path), w=width, h=height,
        guard=guard, wn=w_name, hn=h_name, var=var_name, data=hex_block
    )

    os.makedirs(output_dir, exist_ok=True)
    write_file_if_changed(out_path, content, stem)

    print("  [OK] {:<42s} -> {}  ({}x{}, {} bytes)".format(
        os.path.basename(png_path), fname, width, height, len(byte_array)))
    return fname


# ---------------------------------------------------------------------------
# Output C: all_assets.h master include
# ---------------------------------------------------------------------------

def generate_master_header(output_dir, header_files):
    guard    = "GENERATED_ALL_ASSETS_H"
    includes = "\n".join('#include "{}"'.format(h) for h in sorted(header_files))

    content = (
        "// AUTO-GENERATED by convert_assets.py -- DO NOT EDIT MANUALLY\n"
        "// Single include for all converted app bitmap assets.\n"
        "// Usage in any applet:  #include \"all_assets.h\"\n"
        "\n"
        "#ifndef {guard}\n"
        "#define {guard}\n"
        "\n"
        "{includes}\n"
        "\n"
        "#endif // {guard}\n"
    ).format(guard=guard, includes=includes)

    os.makedirs(output_dir, exist_ok=True)
    master_path = os.path.join(output_dir, "all_assets.h")
    write_file_if_changed(master_path, content, "all_assets")
    print("  [OK] all_assets.h updated ({} app assets)".format(len(header_files)))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("=" * 62)
    print("  Click Unified Asset Conversion Pipeline")
    print("  Scanning : {}".format(ASSETS_DIR))
    print("=" * 62)

    if not os.path.isdir(ASSETS_DIR):
        print("[WARN] Assets directory not found. Creating it.")
        os.makedirs(ASSETS_DIR, exist_ok=True)
        generate_master_header(GEN_ASSETS_DIR, [])
        return

    png_files = sorted(glob.glob(os.path.join(ASSETS_DIR, "*.png")))

    if not png_files:
        print("[INFO] No PNG files found. Nothing to convert.")
        generate_master_header(GEN_ASSETS_DIR, [])
        return

    milestone_entries = []   # (n_str, fname, macro, w, h)
    app_headers       = []   # header filenames for all_assets.h
    errors            = []

    for png_path in png_files:
        stem = os.path.splitext(os.path.basename(png_path))[0]
        validate_name(stem)

        try:
            if MILESTONE_RE.match(stem):
                # --- Milestone image ---
                result = milestone_to_screen_header(png_path, GEN_SCREENS_DIR)
                milestone_entries.append(result)
                print("  [OK] {:<42s} -> screens/generated/screen_{}.h  (milestone)".format(
                    os.path.basename(png_path), stem))
            else:
                # --- App start/end/sprite ---
                fname = asset_to_header(png_path, GEN_ASSETS_DIR)
                app_headers.append(fname)
        except Exception as e:
            print("  [ERR] {} : {}".format(os.path.basename(png_path), e))
            errors.append(png_path)

    # Regenerate counter registry
    if milestone_entries:
        regenerate_registry(GEN_SCREENS_DIR, milestone_entries)
    elif os.path.isdir(GEN_SCREENS_DIR):
        open(os.path.join(GEN_SCREENS_DIR, "registry.inc"), "w").close()

    # Write all_assets.h
    generate_master_header(GEN_ASSETS_DIR, app_headers)

    print("=" * 62)
    print("  Milestones : {}   App assets : {}   Errors : {}".format(
        len(milestone_entries), len(app_headers), len(errors)))
    print("=" * 62)

    if errors:
        sys.exit(1)


if __name__ == "__main__":
    main()
