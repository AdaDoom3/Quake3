#!/usr/bin/env python3
"""
Script to convert all JPEG images to TGA format and update references in asset files.
"""

import os
import subprocess
import sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
import re

# File extensions to search for references
REFERENCE_FILE_EXTENSIONS = ['.shader', '.skin', '.cfg', '.script', '.arena', '.bot']

def find_all_jpegs(base_path):
    """Find all JPEG files in the given path."""
    jpeg_files = []
    for root, dirs, files in os.walk(base_path):
        for file in files:
            if file.lower().endswith(('.jpg', '.jpeg')):
                jpeg_files.append(os.path.join(root, file))
    return jpeg_files

def convert_jpeg_to_tga(jpeg_path):
    """Convert a single JPEG file to TGA format using ImageMagick."""
    tga_path = os.path.splitext(jpeg_path)[0] + '.tga'

    try:
        # Use ImageMagick's convert command
        result = subprocess.run(
            ['convert', jpeg_path, tga_path],
            capture_output=True,
            text=True,
            timeout=30
        )

        if result.returncode == 0:
            return {'success': True, 'jpeg': jpeg_path, 'tga': tga_path}
        else:
            return {'success': False, 'jpeg': jpeg_path, 'error': result.stderr}
    except Exception as e:
        return {'success': False, 'jpeg': jpeg_path, 'error': str(e)}

def find_files_to_update(base_path):
    """Find all files that might contain image references."""
    files_to_check = []
    for root, dirs, files in os.walk(base_path):
        # Skip binary files and directories
        if '.git' in root:
            continue

        for file in files:
            # Check file extension
            ext = os.path.splitext(file)[1].lower()
            if ext in REFERENCE_FILE_EXTENSIONS:
                files_to_check.append(os.path.join(root, file))
    return files_to_check

def update_references_in_file(file_path, jpeg_to_tga_map):
    """Update JPEG references to TGA in a single file."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        return {'file': file_path, 'updated': False, 'error': str(e)}

    original_content = content
    replacements = 0

    # For each JPEG that was converted, replace references
    for jpeg_path, tga_path in jpeg_to_tga_map.items():
        # Get the relative path from assets directory
        jpeg_rel = jpeg_path.replace('/home/user/Quake3/assets/', '')
        tga_rel = tga_path.replace('/home/user/Quake3/assets/', '')

        # Also try without the 'assets/' prefix (some references might use textures/ or models/ directly)
        jpeg_short = jpeg_rel.replace('assets/', '', 1)
        tga_short = tga_rel.replace('assets/', '', 1)

        # Replace .jpg and .jpeg extensions (case insensitive)
        patterns = [
            (re.escape(jpeg_rel), tga_rel),
            (re.escape(jpeg_short), tga_short),
            # Also match without path, just filename
            (re.escape(os.path.basename(jpeg_path)), os.path.basename(tga_path)),
        ]

        for pattern, replacement in patterns:
            # Case insensitive replacement
            new_content = re.sub(pattern, replacement, content, flags=re.IGNORECASE)
            if new_content != content:
                replacements += len(re.findall(pattern, content, flags=re.IGNORECASE))
                content = new_content

    # If content changed, write it back
    if content != original_content:
        try:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return {'file': file_path, 'updated': True, 'replacements': replacements}
        except Exception as e:
            return {'file': file_path, 'updated': False, 'error': str(e)}

    return {'file': file_path, 'updated': False, 'replacements': 0}

def main():
    base_path = '/home/user/Quake3/assets'

    print("="*80)
    print("JPEG to TGA Conversion Script")
    print("="*80)

    # Step 1: Find all JPEG files
    print("\n[1/5] Finding JPEG files...")
    jpeg_files = find_all_jpegs(base_path)
    print(f"Found {len(jpeg_files)} JPEG files")

    if not jpeg_files:
        print("No JPEG files found. Exiting.")
        return 0

    # Step 2: Convert JPEG files to TGA
    print(f"\n[2/5] Converting {len(jpeg_files)} JPEG files to TGA format...")
    jpeg_to_tga_map = {}
    failed_conversions = []

    with ThreadPoolExecutor(max_workers=8) as executor:
        futures = {executor.submit(convert_jpeg_to_tga, jpeg): jpeg for jpeg in jpeg_files}

        completed = 0
        for future in as_completed(futures):
            result = future.result()
            completed += 1

            if completed % 100 == 0 or completed == len(jpeg_files):
                print(f"  Progress: {completed}/{len(jpeg_files)}")

            if result['success']:
                jpeg_to_tga_map[result['jpeg']] = result['tga']
            else:
                failed_conversions.append(result)

    print(f"Successfully converted {len(jpeg_to_tga_map)} files")
    if failed_conversions:
        print(f"Failed to convert {len(failed_conversions)} files")
        for failure in failed_conversions[:5]:  # Show first 5 failures
            print(f"  - {failure['jpeg']}: {failure.get('error', 'Unknown error')}")

    # Step 3: Find files that might contain references
    print("\n[3/5] Finding files with potential image references...")
    files_to_update = find_files_to_update(base_path)
    print(f"Found {len(files_to_update)} files to check")

    # Step 4: Update references in files
    print(f"\n[4/5] Updating references in files...")
    updated_files = []
    failed_updates = []

    for i, file_path in enumerate(files_to_update):
        if (i + 1) % 50 == 0 or (i + 1) == len(files_to_update):
            print(f"  Progress: {i+1}/{len(files_to_update)}")

        result = update_references_in_file(file_path, jpeg_to_tga_map)

        if result.get('updated'):
            updated_files.append(result)
        elif 'error' in result:
            failed_updates.append(result)

    print(f"\n[5/5] Summary:")
    print(f"  - Converted {len(jpeg_to_tga_map)} JPEG files to TGA")
    print(f"  - Updated {len(updated_files)} files with new references")

    total_replacements = sum(r['replacements'] for r in updated_files)
    print(f"  - Made {total_replacements} reference replacements")

    if updated_files:
        print(f"\nFiles with updated references (showing first 10):")
        for result in updated_files[:10]:
            rel_path = result['file'].replace('/home/user/Quake3/', '')
            print(f"  - {rel_path} ({result['replacements']} replacements)")

    if failed_updates:
        print(f"\nFailed to update {len(failed_updates)} files:")
        for failure in failed_updates[:5]:
            print(f"  - {failure['file']}: {failure.get('error', 'Unknown error')}")

    # Create a mapping file for reference
    print("\n[INFO] Creating JPEG to TGA mapping file...")
    with open('/home/user/Quake3/jpeg_to_tga_mapping.txt', 'w') as f:
        f.write("JPEG to TGA Conversion Mapping\n")
        f.write("="*80 + "\n\n")
        for jpeg, tga in sorted(jpeg_to_tga_map.items()):
            jpeg_rel = jpeg.replace('/home/user/Quake3/', '')
            tga_rel = tga.replace('/home/user/Quake3/', '')
            f.write(f"{jpeg_rel} -> {tga_rel}\n")

    print("\n" + "="*80)
    print("Conversion complete!")
    print("="*80)

    return 0

if __name__ == '__main__':
    sys.exit(main())
