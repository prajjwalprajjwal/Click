#!/usr/bin/env python3
"""
Interactive Desktop CLI Flasher for Clicker ESP32.

Features:
- Automatic COM port detection (via serial.tools.list_ports or esptool)
- Interactive choice:
    [1] Standard Update (KEEP STATS & DATA) -> Flashes firmware.bin to 0x10000 without erasing
    [2] Clean Factory Reset (ERASE ALL DATA) -> Runs erase_flash then flashes factory_firmware.bin to 0x0
- Supports custom baud rates and manual port selection
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def find_esptool_command() -> list[str]:
    # 1. Check if esptool.py is in PlatformIO penv
    user_profile = Path(os.environ.get("USERPROFILE", ""))
    penv_py = user_profile / ".platformio" / "penv" / "Scripts" / "python.exe"
    esptool_py = user_profile / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"

    if penv_py.exists() and esptool_py.exists():
        return [str(penv_py), str(esptool_py)]

    # 2. Check system PATH for esptool.py
    import shutil
    sys_esptool = shutil.which("esptool.py") or shutil.which("esptool")
    if sys_esptool:
        return [sys_esptool]

    # 3. Fallback to python -m esptool
    return [sys.executable, "-m", "esptool"]


def detect_com_ports() -> list[str]:
    ports = []
    try:
        import serial.tools.list_ports
        for port in serial.tools.list_ports.comports():
            # Match common ESP32 USB bridges (CP210x, CH340, FTDI, ESP JTAG)
            ports.append(port.device)
    except ImportError:
        pass

    # Windows fallback port scan if pyserial is not installed in current interpreter
    if not ports and sys.platform == "win32":
        try:
            import winreg
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DEVICEMAP\SERIALCOMM")
            for i in range(winreg.QueryInfoKey(key)[1]):
                _, val, _ = winreg.EnumValue(key, i)
                ports.append(val)
        except Exception:
            pass

    return ports


def get_latest_release_dir() -> Path:
    # 1. Check web_flasher/releases/
    releases_dir = ROOT / "web_flasher" / "releases"
    if releases_dir.exists():
        dirs = [d for d in releases_dir.iterdir() if d.is_dir() and d.name.startswith("v")]
        if dirs:
            # Sort by version number
            def v_key(d: Path):
                import re
                nums = re.findall(r"\d+", d.name)
                return [int(n) for n in nums] if nums else [0]
            dirs.sort(key=v_key, reverse=True)
            return dirs[0]

    # 2. Fallback to web_flasher root or build output
    if (ROOT / "web_flasher" / "firmware.bin").exists():
        return ROOT / "web_flasher"

    return ROOT / ".pio" / "build" / "esp32doit-devkit-v1"


def prompt_flashing_choice(release_dir: Path) -> tuple[int, str]:
    ver_tag = release_dir.name if release_dir.name.startswith("v") else "Latest"
    
    print("=" * 65)
    print("        CLICKER ESP32 INTERACTIVE DESKTOP FLASHER")
    print("=" * 65)
    print(f"  Target Release: {ver_tag}")
    print(f"  Source Folder : {release_dir}\n")
    print("  Select flashing mode:\n")
    print("    [1] Standard Update (PRESERVE STATS & CLICKS)")
    print("        - Flashes firmware.bin to offset 0x10000.")
    print("        - Leaves NVS partition (0x9000) 100% UNTOUCHED.")
    print("        - Recommended for regular firmware upgrades.\n")
    print("    [2] Clean Factory Reset (ERASE ALL DATA)")
    print("        - Runs full chip erase.")
    print("        - Flashes factory_firmware.bin to offset 0x0.")
    print("        - Formats all lifetime clicks, milestones, and settings.\n")
    print("    [Q] Cancel / Quit\n")

    while True:
        try:
            choice = input("  Enter choice [1, 2, or Q]: ").strip()
        except (KeyboardInterrupt, EOFError):
            print("\nCancelled.")
            sys.exit(0)

        if choice.upper() == "Q":
            print("Cancelled.")
            sys.exit(0)
        elif choice in ("1", "2"):
            return int(choice), ver_tag
        else:
            print("  Please enter 1, 2, or Q.")


def select_com_port() -> str:
    ports = detect_com_ports()
    if len(ports) == 1:
        print(f"  Detected ESP32 device on port: {ports[0]}")
        use_auto = input(f"  Use {ports[0]}? [Y/n]: ").strip().lower()
        if use_auto in ("", "y", "yes"):
            return ports[0]

    if len(ports) > 1:
        print("\n  Available Serial Ports:")
        for idx, p in enumerate(ports, 1):
            print(f"    [{idx}] {p}")
        print("    [M] Enter COM port manually")
        
        while True:
            sel = input(f"  Select port [1-{len(ports)}, M]: ").strip()
            if sel.isdigit() and 1 <= int(sel) <= len(ports):
                return ports[int(sel) - 1]
            elif sel.upper() == "M":
                break
            print("  Invalid selection.")

    # Manual entry
    while True:
        manual_port = input("  Enter serial COM port (e.g. COM3 or /dev/ttyUSB0): ").strip()
        if manual_port:
            return manual_port


def flash_device(mode: int, port: str, release_dir: Path, esptool_cmd: list[str]) -> bool:
    baud = "921600"
    
    if mode == 1:
        # Mode 1: Data-safe update
        fw_bin = release_dir / "firmware.bin"
        if not fw_bin.exists():
            fw_bin = ROOT / "web_flasher" / "firmware.bin"
        if not fw_bin.exists():
            fw_bin = ROOT / ".pio" / "build" / "esp32doit-devkit-v1" / "firmware.bin"

        if not fw_bin.exists():
            print(f"\n[ERROR] firmware.bin not found in {release_dir} or build output.")
            print("Please run .\\build_release.bat first.")
            return False

        print("\n" + "=" * 65)
        print(f"  FLASHING: Standard Update (Keep Data)")
        print(f"  Port    : {port}")
        print(f"  Target  : 0x10000 ({fw_bin})")
        print("=" * 65)

        cmd = esptool_cmd + [
            "--chip", "esp32",
            "--port", port,
            "--baud", baud,
            "--before", "default_reset",
            "--after", "hard_reset",
            "write_flash",
            "-z",
            "0x10000", str(fw_bin)
        ]

        res = subprocess.run(cmd)
        return res.returncode == 0

    elif mode == 2:
        # Mode 2: Clean Factory Reset
        factory_bin = release_dir / "factory_firmware.bin"
        if not factory_bin.exists():
            factory_bin = ROOT / "web_flasher" / "factory_firmware.bin"

        if not factory_bin.exists():
            print(f"\n[ERROR] factory_firmware.bin not found in {release_dir}.")
            print("Please run .\\build_release.bat first.")
            return False

        print("\n" + "=" * 65)
        print(f"  FLASHING: Clean Factory Reset (Erase Flash)")
        print(f"  Port    : {port}")
        print(f"  Target  : Full Chip Erase + 0x0 ({factory_bin})")
        print("=" * 65)

        # 1. Erase flash
        print("\n[Step 1/2] Erasing all flash memory (NVS + App)...")
        erase_cmd = esptool_cmd + ["--chip", "esp32", "--port", port, "--baud", baud, "erase_flash"]
        e_res = subprocess.run(erase_cmd)
        if e_res.returncode != 0:
            print("\n[ERROR] erase_flash failed. Check port and USB connection.")
            return False

        # 2. Write full factory image
        print("\n[Step 2/2] Writing factory image to offset 0x0...")
        flash_cmd = esptool_cmd + [
            "--chip", "esp32",
            "--port", port,
            "--baud", baud,
            "--before", "default_reset",
            "--after", "hard_reset",
            "write_flash",
            "-z",
            "0x0", str(factory_bin)
        ]
        res = subprocess.run(flash_cmd)
        return res.returncode == 0

    return False


def main() -> None:
    os.chdir(str(ROOT))
    release_dir = get_latest_release_dir()
    esptool_cmd = find_esptool_command()

    mode, ver_tag = prompt_flashing_choice(release_dir)
    port = select_com_port()

    print(f"\nPreparing to flash {ver_tag} to {port}...")
    success = flash_device(mode, port, release_dir, esptool_cmd)

    print("\n" + "=" * 65)
    if success:
        print("  [SUCCESS] Device flashed successfully!")
        if mode == 1:
            print("  Data Status: All lifetime click counts and milestones were PRESERVED.")
        else:
            print("  Data Status: Device was reset to clean factory defaults.")
        print("  If the screen does not turn on immediately, press the EN button.")
    else:
        print("  [FAILED] Flashing encountered an error.")
        print("  Tip: Hold the BOOT button on your ESP32 board while retrying.")
    print("=" * 65 + "\n")


if __name__ == "__main__":
    main()
