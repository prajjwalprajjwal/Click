#!/usr/bin/env bash
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
UF2_FILE="$SCRIPT_DIR/click-rp2350.uf2"
PICOTOOL=$(command -v picotool || true)

if [ ! -f "$UF2_FILE" ] && [ -f "$SCRIPT_DIR/firmware.uf2" ]; then
    UF2_FILE="$SCRIPT_DIR/firmware.uf2"
fi

if [ -z "$PICOTOOL" ] && [ -x "$HOME/.platformio/packages/tool-picotool-rp2040-earlephilhower/picotool" ]; then
    PICOTOOL="$HOME/.platformio/packages/tool-picotool-rp2040-earlephilhower/picotool"
fi

if [ -z "$PICOTOOL" ]; then
    echo "picotool was not found. Install picotool or PlatformIO's RP2040 tool package."
    exit 1
fi

if [ ! -f "$UF2_FILE" ]; then
    echo "UF2 image not found beside this script: $UF2_FILE"
    echo "Download click-rp2350.uf2 into the same folder and retry."
    exit 1
fi

echo "Updating RP2350 over USB with picotool. The device will briefly reset and reboot."
exec "$PICOTOOL" load -f -u -v -x "$UF2_FILE"