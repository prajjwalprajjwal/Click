#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

echo "==================================================="
echo "       Clicker ESP32 Firmware Flasher (macOS/Linux)"
echo "==================================================="
echo ""

if ! command -v python3 &> /dev/null && ! command -v python &> /dev/null; then
    echo "[ERROR] Python is not installed or not found in PATH!"
    echo "Please install Python 3 and esptool: pip install esptool"
    exit 1
fi

PY_CMD="python3"
if ! command -v python3 &> /dev/null; then
    PY_CMD="python"
fi

if [ -f "../../tools/flasher/flash_device.py" ]; then
    $PY_CMD ../../tools/flasher/flash_device.py "$@"
else
    echo "Flashing firmware via esptool.py..."
    esptool.py --chip esp32 write_flash 0x1000 ../../.pio/build/esp32doit-devkit-v1/bootloader.bin 0x8000 ../../.pio/build/esp32doit-devkit-v1/partitions.bin 0x10000 ../../.pio/build/esp32doit-devkit-v1/firmware.bin
fi

echo ""
echo "[SUCCESS] Flashing complete!"
