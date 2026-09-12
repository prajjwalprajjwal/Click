#!/usr/bin/env python3
"""
Interactive CLI Release Manager for Clicker ESP32 Firmware.

Handles:
- Semantic version bumping (Major, Minor, Patch, Custom)
- PlatformIO compilation
- 3-part binary packaging (bootloader, partitions, firmware)
- Merging single all-in-one factory_firmware.bin (offset 0x0)
- Manifest generation with new_install_prompt_erase: true
- 5-version retention in releases/ and web_flasher/
- Registry updates and Git publishing instructions
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BIN = ROOT / ".pio" / "build" / "esp32doit-devkit-v1" / "firmware.bin"
WEB_FLASHER_DIR = ROOT / "web_flasher"
RELEASES_DIR = WEB_FLASHER_DIR / "releases"


def parse_semver(v: str) -> tuple[int, int, int]:
    clean = re.sub(r"^v", "", v.strip())
    parts = clean.split(".")
    try:
        return (
            int(parts[0]) if len(parts) > 0 else 1,
            int(parts[1]) if len(parts) > 1 else 0,
            int(parts[2]) if len(parts) > 2 else 0,
        )
    except ValueError:
        return (1, 0, 0)


def format_semver(major: int, minor: int, patch: int) -> str:
    return f"{major}.{minor}.{patch}"


def get_current_version() -> str:
    # 1. Try reading from web_flasher/versions.json
    v_file = WEB_FLASHER_DIR / "versions.json"
    if v_file.exists():
        try:
            data = json.loads(v_file.read_text(encoding="utf-8"))
            if "latest" in data and data["latest"]:
                return re.sub(r"^v", "", data["latest"].strip())
        except Exception:
            pass

    # 2. Check highest version directory in web_flasher/releases/
    if RELEASES_DIR.exists():
        dirs = [d for d in RELEASES_DIR.iterdir() if d.is_dir() and d.name.startswith("v")]
        if dirs:
            dirs.sort(key=lambda d: parse_semver(d.name), reverse=True)
            return re.sub(r"^v", "", dirs[0].name)

    return "1.0.0"


def find_platformio() -> str:
    pio_path = shutil.which("pio")
    if pio_path:
        return pio_path

    home = Path.home()
    candidates = [
        home / ".platformio" / "penv" / "bin" / "pio",
        home / ".platformio" / "penv" / "bin" / "platformio",
        home / ".platformio" / "penv" / "Scripts" / "pio.exe",
        home / ".platformio" / "penv" / "Scripts" / "platformio.exe",
    ]
    user_profile = os.environ.get("USERPROFILE", "")
    if user_profile:
        candidates.extend([
            Path(user_profile) / ".platformio" / "penv" / "Scripts" / "pio.exe",
            Path(user_profile) / ".platformio" / "penv" / "Scripts" / "platformio.exe",
        ])

    for c in candidates:
        if c.exists():
            return str(c)

    return "pio"


def prompt_version_bump(current_ver: str) -> str:
    major, minor, patch = parse_semver(current_ver)
    bump_major = format_semver(major + 1, 0, 0)
    bump_minor = format_semver(major, minor + 1, 0)
    bump_patch = format_semver(major, minor, patch + 1)

    print("=" * 60)
    print("    CLICKER ESP32 FIRMWARE RELEASE MANAGER")
    print("=" * 60)
    print(f"  Current active version: v{current_ver}\n")
    print("  Select release type:")
    print(f"    [1] Major Release         (v{bump_major})")
    print(f"    [2] Minor / Feature       (v{bump_minor})")
    print(f"    [3] Bug Fix / Patch       (v{bump_patch})")
    print("    [4] Custom Version Tag    (enter manually)")
    print("    [Q] Quit / Cancel\n")

    while True:
        try:
            choice = input("  Enter choice [1-4, Q]: ").strip()
        except (KeyboardInterrupt, EOFError):
            print("\nOperation cancelled.")
            sys.exit(0)

        if choice.upper() == "Q":
            print("Cancelled.")
            sys.exit(0)
        elif choice == "1":
            return bump_major
        elif choice == "2":
            return bump_minor
        elif choice == "3":
            return bump_patch
        elif choice == "4":
            custom = input("  Enter custom version (e.g. 1.2.3 or v1.2.3): ").strip()
            clean_custom = re.sub(r"^v", "", custom)
            if re.match(r"^\d+\.\d+(\.\d+)?", clean_custom):
                return clean_custom
            print("  Invalid version format! Example: 1.0.1")
        else:
            print("  Please enter 1, 2, 3, 4, or Q.")


def build_firmware(pio_cmd: str) -> bool:
    print("\n[1/4] Compiling ESP32 firmware with PlatformIO...")
    cmd = [pio_cmd, "run", "-e", "esp32doit-devkit-v1"]
    res = subprocess.run(cmd, cwd=str(ROOT))
    return res.returncode == 0


def merge_factory_binary(target_dir: Path) -> Path:
    out_file = target_dir / "factory_firmware.bin"
    build_dir = ROOT / ".pio" / "build" / "esp32doit-devkit-v1"
    bootloader_bin = build_dir / "bootloader.bin"
    partitions_bin = build_dir / "partitions.bin"
    firmware_bin = build_dir / "firmware.bin"
    boot_app0_bin = ROOT / "tools" / "flasher" / "boot_app0.bin"

    home = Path.home()
    penv_candidates = [
        home / ".platformio" / "penv" / "bin" / "python3",
        home / ".platformio" / "penv" / "bin" / "python",
        home / ".platformio" / "penv" / "Scripts" / "python.exe",
    ]
    user_profile = os.environ.get("USERPROFILE", "")
    if user_profile:
        penv_candidates.append(Path(user_profile) / ".platformio" / "penv" / "Scripts" / "python.exe")

    penv_py = None
    for p in penv_candidates:
        if p.exists():
            penv_py = p
            break
    if not penv_py:
        penv_py = Path(sys.executable)

    esptool_candidates = [
        home / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py",
        home / ".platformio" / "penv" / "bin" / "esptool.py",
        home / ".platformio" / "penv" / "Scripts" / "esptool.py",
    ]
    if user_profile:
        esptool_candidates.append(Path(user_profile) / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py")

    esptool_py = None
    for e in esptool_candidates:
        if e.exists():
            esptool_py = e
            break

    merged = False
    if penv_py and esptool_py and esptool_py.exists() and boot_app0_bin.exists():
        cmd = [
            str(penv_py), str(esptool_py),
            "--chip", "esp32",
            "merge_bin",
            "-o", str(out_file),
            "--flash_mode", "dio",
            "--flash_freq", "40m",
            "--flash_size", "4MB",
            "0x1000", str(bootloader_bin),
            "0x8000", str(partitions_bin),
            "0xe000", str(boot_app0_bin),
            "0x10000", str(firmware_bin),
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode == 0 and out_file.exists():
            merged = True
            print(f"  + Merged factory_firmware.bin via esptool ({out_file.stat().st_size} bytes, offset 0x0)")

    if not merged:
        segments = [
            (0x1000, bootloader_bin.read_bytes() if bootloader_bin.exists() else b""),
            (0x8000, partitions_bin.read_bytes() if partitions_bin.exists() else b""),
            (0xE000, boot_app0_bin.read_bytes() if boot_app0_bin.exists() else b"\xff" * 8192),
            (0x10000, firmware_bin.read_bytes()),
        ]
        max_len = max(offset + len(data) for offset, data in segments)
        merged_bytes = bytearray([0xFF] * max_len)
        for offset, data in segments:
            merged_bytes[offset : offset + len(data)] = data
        out_file.write_bytes(merged_bytes)
        print(f"  + Merged factory_firmware.bin via binary fallback ({out_file.stat().st_size} bytes, offset 0x0)")

    return out_file


def package_release(new_version: str, max_releases: int = 5) -> Path:
    v_clean = re.sub(r"^v", "", new_version.strip())
    folder_name = f"v{v_clean}"
    target_dir = RELEASES_DIR / folder_name
    target_dir.mkdir(parents=True, exist_ok=True)

    build_dir = ROOT / ".pio" / "build" / "esp32doit-devkit-v1"
    fw_bin = build_dir / "firmware.bin"
    bootloader_bin = build_dir / "bootloader.bin"
    partitions_bin = build_dir / "partitions.bin"

    if not fw_bin.exists():
        print(f"Error: Compiled firmware binary not found at {fw_bin}")
        sys.exit(1)

    print(f"\n[2/4] Packaging binaries into web_flasher/releases/{folder_name}...")
    shutil.copy2(fw_bin, target_dir / "firmware.bin")
    print(f"  + Copied firmware.bin ({fw_bin.stat().st_size} bytes, offset 0x10000)")

    parts = []
    if bootloader_bin.exists():
        shutil.copy2(bootloader_bin, target_dir / "bootloader.bin")
        parts.append({"path": "bootloader.bin", "offset": 4096})
        print("  + Copied bootloader.bin (offset 0x1000 / 4096)")

    if partitions_bin.exists():
        shutil.copy2(partitions_bin, target_dir / "partitions.bin")
        parts.append({"path": "partitions.bin", "offset": 32768})
        print("  + Copied partitions.bin (offset 0x8000 / 32768)")

    parts.append({"path": "firmware.bin", "offset": 65536})

    boot_app0_bin = ROOT / "tools" / "flasher" / "boot_app0.bin"
    if boot_app0_bin.exists():
        shutil.copy2(boot_app0_bin, target_dir / "boot_app0.bin")
        print("  + Copied boot_app0.bin (offset 0xe000 / 57344)")

    # Merge full factory image for 3rd-party flashers (offset 0x0)
    factory_bin = merge_factory_binary(target_dir)

    # 3. Create Manifest (for custom Web Serial flasher)
    print("\n[3/4] Generating ESP Web Tools manifests...")
    manifest_data = {
        "name": "Clicker Device Firmware",
        "version": v_clean,
        "improv": False,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": parts,
            }
        ],
    }

    manifest_path = target_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest_data, indent=2), encoding="utf-8")
    print(f"  + Created {manifest_path}")

    # 4. Prune older releases & maintain up to max_releases
    dirs = [d for d in RELEASES_DIR.iterdir() if d.is_dir() and d.name.startswith("v")]
    dirs.sort(key=lambda d: parse_semver(d.name), reverse=True)

    if len(dirs) > max_releases:
        for old_dir in dirs[max_releases:]:
            print(f"  - Pruning old release: {old_dir.name}")
            shutil.rmtree(old_dir, ignore_errors=True)
        dirs = dirs[:max_releases]

    # 5. Build release list for versions.json
    all_releases = []
    for d in dirs:
        m_path = d / "manifest.json"
        bin_file = d / "firmware.bin"
        f_bin = d / "factory_firmware.bin"
        m_ver = d.name.lstrip("v")
        m_name = "Clicker Device Firmware"
        if m_path.exists():
            try:
                data = json.loads(m_path.read_text(encoding="utf-8"))
                m_ver = data.get("version", m_ver)
                m_name = data.get("name", m_name)
            except Exception:
                pass

        all_releases.append({
            "tag": d.name,
            "version": m_ver,
            "name": m_name,
            "manifest": f"releases/{d.name}/manifest.json",
            "bin": f"releases/{d.name}/firmware.bin",
            "factory_bin": f"releases/{d.name}/factory_firmware.bin",
            "size": bin_file.stat().st_size if bin_file.exists() else 0,
            "factory_size": f_bin.stat().st_size if f_bin.exists() else 0,
            "is_latest": (d.name == folder_name),
        })

    versions_json = {
        "latest": folder_name,
        "releases": all_releases,
    }

    # 6. Write directly to web_flasher/ and web_flasher/releases/
    print("\n[4/4] Updating web flasher root assets...")
    if WEB_FLASHER_DIR.exists():
        (WEB_FLASHER_DIR / "versions.json").write_text(json.dumps(versions_json, indent=2), encoding="utf-8")
        if RELEASES_DIR.exists():
            (RELEASES_DIR / "versions.json").write_text(json.dumps(versions_json, indent=2), encoding="utf-8")

        # Copy direct latest binaries to web_flasher root for zero-traversal local execution
        shutil.copy2(target_dir / "firmware.bin", WEB_FLASHER_DIR / "firmware.bin")
        shutil.copy2(target_dir / "factory_firmware.bin", WEB_FLASHER_DIR / "factory_firmware.bin")
        shutil.copy2(target_dir / "manifest.json", WEB_FLASHER_DIR / "manifest.json")
        if bootloader_bin.exists():
            shutil.copy2(bootloader_bin, WEB_FLASHER_DIR / "bootloader.bin")
        if partitions_bin.exists():
            shutil.copy2(partitions_bin, WEB_FLASHER_DIR / "partitions.bin")
        if boot_app0_bin.exists():
            shutil.copy2(boot_app0_bin, WEB_FLASHER_DIR / "boot_app0.bin")
        print("  + Synchronized web_flasher/ with latest active release.")

    return target_dir


def main() -> None:
    os.chdir(str(ROOT))
    current_ver = get_current_version()

    if len(sys.argv) > 1 and sys.argv[1] not in ["-i", "--interactive"]:
        new_version = re.sub(r"^v", "", sys.argv[1].strip())
    else:
        new_version = prompt_version_bump(current_ver)

    pio_cmd = find_platformio()
    success = build_firmware(pio_cmd)
    if not success:
        print("\n[ERROR] Firmware compilation failed. Release aborted.")
        sys.exit(1)

    target_dir = package_release(new_version, max_releases=5)

    print("\n" + "=" * 60)
    print(f"  [SUCCESS] Release v{new_version} created successfully!")
    print("=" * 60)
    print(f"  Release Folder: {target_dir}")
    print(f"  App Binary (Offset 0x10000): {target_dir / 'firmware.bin'}")
    print(f"  Merged Factory Binary (Offset 0x0): {target_dir / 'factory_firmware.bin'}")
    print(f"  Active Manifest: {target_dir / 'manifest.json'}")
    print("\n  Suggested Git Commands to Publish:")
    print("  ----------------------------------")
    print("  git add .")
    print(f'  git commit -m "Release v{new_version}"')
    print("  git push")
    print("=" * 60 + "\n")


if __name__ == "__main__":
    main()
