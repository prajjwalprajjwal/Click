Import("env")
import subprocess
import shutil
import sys
import os

PROJECT_DIR  = env.subst("$PROJECT_DIR")
BUILD_DIR    = env.subst("$BUILD_DIR")
FLASHER_DIR  = os.path.join(PROJECT_DIR, "web_flasher")

def run_asset_pipeline():
    """Runs convert_assets.py immediately so headers & cpp timestamps are updated before SCons dependency calculation."""
    script = os.path.join(PROJECT_DIR, "firmware", "scripts", "convert_assets.py")
    print("\n[pre_build] Running asset conversion pipeline...")
    result = subprocess.run([sys.executable, script], capture_output=False)
    if result.returncode != 0:
        print("[pre_build] Asset conversion failed — check errors above.")

# Execute immediately on SCons startup
run_asset_pipeline()

def sync_flasher_bins(source, target, env):
    """Copy freshly built binaries into web_flasher/ so web flasher is always up to date."""
    bins = ["firmware.bin", "bootloader.bin", "partitions.bin"]
    for b in bins:
        src = os.path.join(BUILD_DIR, b)
        dst = os.path.join(FLASHER_DIR, b)
        if os.path.exists(src):
            shutil.copy2(src, dst)
    print("\n[post_build] web_flasher/ binaries synced.")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sync_flasher_bins)
