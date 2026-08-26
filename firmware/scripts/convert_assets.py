import os
import sys
from PIL import Image

# Determine script and directory paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Primary and fallback paths
POSSIBLE_ASSET_DIRS = [
    os.path.abspath(os.path.join(SCRIPT_DIR, "../assets/screens")),
    os.path.abspath(os.path.join(SCRIPT_DIR, "../../assets/screens")),
]

ASSET_DIR = None
for p in POSSIBLE_ASSET_DIRS:
    if os.path.exists(p):
        ASSET_DIR = p
        break

if ASSET_DIR is None:
    ASSET_DIR = POSSIBLE_ASSET_DIRS[0]
    os.makedirs(ASSET_DIR, exist_ok=True)

OUTPUT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../src/generated_assets"))
INCLUDE_OUTPUT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../include/screens/generated"))

os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(INCLUDE_OUTPUT_DIR, exist_ok=True)

print(f"Scanning for images in: {ASSET_DIR}")
found_files = 0

for file in os.listdir(ASSET_DIR):
    if file.lower().endswith((".png", ".jpg", ".jpeg", ".bmp")):
        found_files += 1
        img_path = os.path.join(ASSET_DIR, file)
        img = Image.open(img_path).convert("1")
        
        var_name = os.path.splitext(file)[0].replace("-", "_").replace(" ", "_")
        if var_name[0].isdigit():
            var_name = f"img_{var_name}"
            
        bytes_data = img.tobytes()
        c_array = ", ".join([f"0x{b:02X}" for b in bytes_data])
        
        header_content = (
            f"#pragma once\n\n"
            f"// Dimensions: {img.width}x{img.height}\n"
            f"const unsigned char {var_name}_bits[] = {{\n"
            f"  {c_array}\n"
            f"}};\n"
        )
        
        header_path = os.path.join(OUTPUT_DIR, f"{var_name}.h")
        with open(header_path, "w") as f:
            f.write(header_content)
            
        mirror_path = os.path.join(INCLUDE_OUTPUT_DIR, f"{var_name}.h")
        with open(mirror_path, "w") as f:
            f.write(header_content)
            
        print(f"Processed: {file} ({img.width}x{img.height}) -> {var_name}.h")

if found_files == 0:
    print(f"No image files (.png/.bmp) found in {ASSET_DIR}.")
else:
    print(f"[SUCCESS] Converted {found_files} image(s) to C header bitmaps.")
