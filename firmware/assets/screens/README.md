# Celebration screens

Drop **128×64-friendly PNG** images here, then convert them for the OLED:

```bash
pip install pillow
python tools/png_to_oled.py
```

## Naming

Use the click count in the filename so the firmware knows when to show it:

- `100.png` → celebration at **100** clicks
- `celebrate_5000.png` → celebration at **5000** clicks
- `100000.png` → celebration at **100000** clicks

## Output

Converted bitmaps are written to `include/screens/generated/` and registered automatically.

Rebuild and flash after converting.

## Built-in screens

Run `python tools/png_to_oled.py --defaults` to regenerate the default CLIX-inspired art for:
10, 100, 5000, 10000, 50000, 100000 clicks.
