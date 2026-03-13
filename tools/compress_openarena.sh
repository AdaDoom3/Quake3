#!/bin/bash
# OpenArena Asset Compression Script (Official Assets)
# Selects optimal quarter of maps (14 of 58) with maximum texture overlap,
# converts textures to KTX2 ASTC 12x12 half-res, sounds to OGG q3,
# packages into .7z ultra. Target: <30MB.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OA_DIR="$ROOT_DIR/openarena_src"
OUTPUT_DIR="$ROOT_DIR/oa_compressed"
TEMP_DIR="/tmp/oa_compress_$$"
NPROC=$(nproc)

# Selected maps (14 of 58) - chosen by greedy algorithm to minimize
# the total unique texture set. These maps share the most textures.
SELECTED_MAPS=(
    cbctf1 czest1tourney dm4ish fan hydronex
    oa_bases3cl oa_bases3plus3 oa_ctf2 oa_ctf2old oa_pvomit
    ps9ctf pul1ctf q3dm6ish suspended
)

echo "============================================="
echo "  OpenArena Official Asset Compressor"
echo "  Maps:     ${#SELECTED_MAPS[@]} of 58 (optimal quarter)"
echo "  Textures: KTX2 ASTC 12x12, half-res"
echo "  Sounds:   OGG Vorbis q3 (~112kbps stereo)"
echo "  Archive:  .7z ultra"
echo "  Threads:  $NPROC"
echo "============================================="
echo ""

# Verify source exists
if [ ! -d "$OA_DIR" ]; then
    echo "ERROR: OpenArena source not found at $OA_DIR"
    echo "Run: apt-get install openarena-data && extract pk3s"
    exit 1
fi

ORIG_SIZE=$(du -sb "$OA_DIR" --exclude='.git' | cut -f1)
echo "Original extracted assets: $(du -sh "$OA_DIR" --exclude='.git' | cut -f1)"
echo ""

# Clean up and prepare
rm -rf "$OUTPUT_DIR" "$TEMP_DIR"
mkdir -p "$OUTPUT_DIR" "$TEMP_DIR"

# ============================================
# Step 1: Determine needed files
# ============================================
echo "[1/5] Resolving texture dependencies from BSP + shaders..."

export OA_DIR TEMP_DIR OUTPUT_DIR
cd "$OA_DIR"

# Extract texture refs from selected map BSPs
> "$TEMP_DIR/needed_refs.txt"
for mapname in "${SELECTED_MAPS[@]}"; do
    bsp="maps/${mapname}.bsp"
    if [ -f "$bsp" ]; then
        strings "$bsp" | grep -E '^(textures|models|gfx|env|sprites|menu|icons|powerups|sfx)/' | sort -u >> "$TEMP_DIR/needed_refs.txt"
    fi
done
sort -u -o "$TEMP_DIR/needed_refs.txt" "$TEMP_DIR/needed_refs.txt"
echo "  Unique texture/shader refs from BSPs: $(wc -l < "$TEMP_DIR/needed_refs.txt")"

# Resolve refs to actual image files (direct or via shaders)
python3 << 'PYEOF'
import re, os, glob

OA = os.environ['OA_DIR']
TEMP = os.environ['TEMP_DIR']

# Parse shaders
shaders = {}
for sf in glob.glob(os.path.join(OA, "scripts", "*.shader")):
    with open(sf, errors='replace') as f:
        lines = f.read().split('\n')
    current_shader = None
    brace_depth = 0
    images = []
    for line in lines:
        s = line.strip()
        if brace_depth == 0 and s and not s.startswith('//') and '{' not in s:
            current_shader = s.split()[0] if s.split() else None
            images = []
        elif '{' in s:
            brace_depth += s.count('{')
        elif '}' in s:
            brace_depth -= s.count('}')
            if brace_depth <= 0:
                if current_shader and images:
                    shaders[current_shader] = images
                current_shader = None
                brace_depth = 0
                images = []
        elif brace_depth > 0 and current_shader:
            m = re.match(r'\s*(?:map|clampMap|animMap\s+\S+)\s+(\S+)', s)
            if m and not m.group(1).startswith('$'):
                images.append(m.group(1))

with open(os.path.join(TEMP, "needed_refs.txt")) as f:
    refs = set(l.strip() for l in f if l.strip())

needed = set()
for ref in refs:
    found = False
    for ext in ['', '.tga', '.jpg', '.png', '.TGA']:
        if os.path.isfile(os.path.join(OA, ref + ext)):
            needed.add(ref + ext)
            found = True
            break
    if not found and ref in shaders:
        for img in shaders[ref]:
            for ext in ['', '.tga', '.jpg', '.png', '.TGA']:
                if os.path.isfile(os.path.join(OA, img + ext)):
                    needed.add(img + ext)
                    break

# Add common UI/HUD/menu files
for d in ['gfx', 'icons', 'sprites', 'menu']:
    dp = os.path.join(OA, d)
    if os.path.isdir(dp):
        for root, dirs, files in os.walk(dp):
            for fn in files:
                if fn.lower().endswith(('.tga', '.jpg', '.png')):
                    needed.add(os.path.relpath(os.path.join(root, fn), OA))

with open(os.path.join(TEMP, "needed_images.txt"), "w") as f:
    for img in sorted(needed):
        f.write(img + "\n")

print(f"  Resolved to {len(needed)} image files")
PYEOF

# ============================================
# Step 2: Copy selected maps + non-media files
# ============================================
echo ""
echo "[2/5] Copying selected maps and support files..."

# Copy selected BSP + AAS files
mkdir -p "$OUTPUT_DIR/maps"
for mapname in "${SELECTED_MAPS[@]}"; do
    for ext in .bsp; do
        src="maps/${mapname}${ext}"
        [ -f "$src" ] && cp "$src" "$OUTPUT_DIR/maps/"
    done
done
echo "  Maps: $(ls "$OUTPUT_DIR/maps/"*.bsp 2>/dev/null | wc -l) BSP files ($(du -sh "$OUTPUT_DIR/maps" | cut -f1))"

# Copy shaders, configs, QVMs, skins, and other support files
for d in scripts vm; do
    if [ -d "$d" ]; then
        mkdir -p "$OUTPUT_DIR/$d"
        cp -r "$d"/* "$OUTPUT_DIR/$d/" 2>/dev/null || true
    fi
done

# Copy model files - weapons, items, effects (essential for gameplay)
for d in models/weapons models/weapons2 models/weaphits models/powerups \
         models/ammo models/flags models/gibs models/misc models/dpoints; do
    if [ -d "$d" ]; then
        find "$d" -type f \( -name "*.md3" -o -name "*.skin" -o -name "*.ase" -o -name "*.cfg" \) \
            -exec cp --parents {} "$OUTPUT_DIR/" \; 2>/dev/null || true
    fi
done

# Copy all player models
if [ -d "models/players" ]; then
    find "models/players" -type f \( -name "*.md3" -o -name "*.skin" -o -name "*.cfg" \) \
        -exec cp --parents {} "$OUTPUT_DIR/" \; 2>/dev/null || true
fi

# Copy only referenced mapobjects (flares, fan)
for obj in models/mapobjects/flares models/mapobjects/fan; do
    if [ -d "$obj" ]; then
        find "$obj" -type f -name "*.md3" -exec cp --parents {} "$OUTPUT_DIR/" \; 2>/dev/null || true
    fi
done

# Copy levelshots for selected maps
mkdir -p "$OUTPUT_DIR/levelshots"
for mapname in "${SELECTED_MAPS[@]}"; do
    for ext in .tga .jpg .png; do
        src="levelshots/${mapname}${ext}"
        [ -f "$src" ] && cp "$src" "$OUTPUT_DIR/levelshots/"
    done
done

echo "  Support files copied."

# ============================================
# Step 3: Convert textures -> KTX2 ASTC 12x12 half-res
# ============================================
echo ""
echo "[3/5] Converting textures to KTX2 ASTC (12x12, half-res, medium quality)..."

convert_texture() {
    local src="$1"
    local base="${src%.*}"
    local outfile="$OUTPUT_DIR/${base}.ktx2"
    local tmpfile="$TEMP_DIR/$(echo "$src" | tr '/' '_').png"

    mkdir -p "$(dirname "$outfile")"

    # Convert to PNG at half resolution
    if ! convert "$OA_DIR/$src" -strip -resize 50% "$tmpfile" 2>/dev/null; then
        # Try without resize for very small images
        if ! convert "$OA_DIR/$src" -strip "$tmpfile" 2>/dev/null; then
            cp "$OA_DIR/$src" "$OUTPUT_DIR/$src"
            return 1
        fi
    fi

    # ASTC 12x12 (0.89 bpp) for maximum compression
    if toktx --encode astc \
        --astc_blk_d 12x12 \
        --astc_quality medium \
        --t2 \
        --assign_oetf srgb \
        --threads 1 \
        "$outfile" "$tmpfile" 2>/dev/null; then
        rm -f "$tmpfile"
        return 0
    else
        if toktx --encode astc \
            --astc_blk_d 12x12 \
            --astc_quality medium \
            --t2 \
            --threads 1 \
            "$outfile" "$tmpfile" 2>/dev/null; then
            rm -f "$tmpfile"
            return 0
        else
            cp "$OA_DIR/$src" "$OUTPUT_DIR/$src"
            rm -f "$tmpfile"
            return 1
        fi
    fi
}
export -f convert_texture

TEXTURE_COUNT=$(wc -l < "$TEMP_DIR/needed_images.txt")
echo "  Processing $TEXTURE_COUNT texture files..."

cat "$TEMP_DIR/needed_images.txt" | xargs -P "$NPROC" -I {} bash -c 'convert_texture "$@"' _ {}

TEXTURE_OK=$(find "$OUTPUT_DIR" -name "*.ktx2" | wc -l)
echo "  Converted: $TEXTURE_OK / $TEXTURE_COUNT to KTX2 ASTC"

# ============================================
# Step 4: Convert sounds WAV -> OGG
# ============================================
echo ""
echo "[4/5] Converting sounds to OGG Vorbis (q3 ~112kbps stereo)..."

convert_sound() {
    local src="$1"
    local rel="${src#./}"
    local base="${rel%.*}"
    local outfile="$OUTPUT_DIR/${base}.ogg"

    mkdir -p "$(dirname "$outfile")"

    if ffmpeg -y -i "$OA_DIR/$src" -c:a libvorbis -q:a 3 -ac 2 "$outfile" 2>/dev/null; then
        return 0
    else
        cp "$OA_DIR/$src" "$OUTPUT_DIR/$rel"
        return 1
    fi
}
export -f convert_sound

SOUND_LIST=$(find . -iname "*.wav" -type f | sed 's|^\./||' | sort)
SOUND_COUNT=$(echo "$SOUND_LIST" | wc -l)
echo "  Processing $SOUND_COUNT sound files..."

echo "$SOUND_LIST" | xargs -P "$NPROC" -I {} bash -c 'convert_sound "$@"' _ {}

SOUND_OK=$(find "$OUTPUT_DIR" -name "*.ogg" | wc -l)
echo "  Converted: $SOUND_OK / $SOUND_COUNT to OGG"

# ============================================
# Step 5: Package into .7z ultra
# ============================================
echo ""
echo "[5/5] Packaging into .7z with ultra compression..."

ARCHIVE="$ROOT_DIR/openarena_compressed.7z"
rm -f "$ARCHIVE"

cd "$OUTPUT_DIR"
7z a -t7z -mx=9 -mfb=273 -ms=on -md=64m -mmt="$NPROC" "$ARCHIVE" . 2>&1 | tail -5

cd "$ROOT_DIR"

# ============================================
# Results
# ============================================
COMPRESSED_DIR_SIZE=$(du -sb "$OUTPUT_DIR" | cut -f1)
ARCHIVE_SIZE=$(stat -c%s "$ARCHIVE")

echo ""
echo "============================================="
echo "  COMPRESSION RESULTS"
echo "============================================="
echo ""
echo "  Source (full OA extracted):  $(du -sh "$OA_DIR" --exclude='.git' | cut -f1)"
echo "  Selected subset (raw):      $(du -sh "$OUTPUT_DIR" | cut -f1)"
echo "  .7z archive:                $(numfmt --to=iec-i --suffix=B $ARCHIVE_SIZE)"
echo ""
echo "  Maps:      ${#SELECTED_MAPS[@]} of 58 (optimized quarter)"
echo "  Textures:  $TEXTURE_OK converted to KTX2 ASTC 12x12 half-res"
echo "  Sounds:    $SOUND_OK converted to OGG q3"
echo ""

# Per-directory breakdown
echo "  === Directory sizes ==="
for d in maps textures models sound menu gfx env sprites icons levelshots scripts vm; do
    if [ -d "$OUTPUT_DIR/$d" ]; then
        printf "    %-15s %s\n" "$d" "$(du -sh "$OUTPUT_DIR/$d" | cut -f1)"
    fi
done

echo ""
echo "  Full OA -> .7z ratio:  $(echo "scale=1; $ORIG_SIZE / $ARCHIVE_SIZE" | bc)x smaller"
echo "  Savings:               $(echo "scale=1; 100 - ($ARCHIVE_SIZE * 100 / $ORIG_SIZE)" | bc)%"
echo ""
echo "  Output: $ARCHIVE"
echo "============================================="

rm -rf "$TEMP_DIR"
