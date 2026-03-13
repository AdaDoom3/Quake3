#!/bin/bash
# OpenArena Asset Compression Script
# Converts textures to KTX2 ASTC, sounds to OGG, packages into .7z
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
ASSETS_DIR="$ROOT_DIR/assets"
OUTPUT_DIR="$ROOT_DIR/assets_compressed"
TEMP_DIR="/tmp/oa_compress_$$"
NPROC=$(nproc)

echo "============================================="
echo "  OpenArena Asset Compressor"
echo "  Textures: TGA/PNG -> KTX2 (ASTC 6x6)"
echo "  Sounds:   WAV -> OGG (q1 ~80kbps)"
echo "  Archive:  .7z ultra"
echo "  Threads:  $NPROC"
echo "============================================="

# Measure original size
ORIG_SIZE=$(du -sb "$ASSETS_DIR" | cut -f1)
echo ""
echo "Original assets size: $(du -sh "$ASSETS_DIR" | cut -f1)"
echo ""

# Clean up and prepare
rm -rf "$OUTPUT_DIR" "$TEMP_DIR"
mkdir -p "$OUTPUT_DIR" "$TEMP_DIR"

# ============================================
# Step 1: Copy directory structure + non-texture/non-audio files
# ============================================
echo "[1/4] Copying directory structure and passthrough files..."
cd "$ASSETS_DIR"
find . -type d -exec mkdir -p "$OUTPUT_DIR/{}" \;

# Copy everything that is NOT a texture or sound file
# (shaders, configs, models, maps, skins, etc.)
find . -type f \
    ! -iname "*.tga" \
    ! -iname "*.png" \
    ! -iname "*.wav" \
    ! -iname "*.zip" \
    ! -name "*.zip.*" \
    ! -iname "*.pspimage" \
    ! -iname "*.psp" \
    -exec cp --parents {} "$OUTPUT_DIR/" \;

echo "  Done. Copied non-media files."

# ============================================
# Step 2: Convert textures TGA/PNG -> KTX2 ASTC
# ============================================
echo ""
echo "[2/4] Converting textures to KTX2 ASTC (6x6 block, medium quality)..."

TEXTURE_COUNT=0
TEXTURE_FAIL=0
TEXTURE_OK=0

# toktx needs PNG input, so we convert TGA->PNG first then use toktx
convert_texture() {
    local src="$1"
    local rel="${src#./}"
    local base="${rel%.*}"
    local outfile="$OUTPUT_DIR/${base}.ktx2"
    local tmpfile="$TEMP_DIR/$(echo "$rel" | tr '/' '_').png"

    mkdir -p "$(dirname "$outfile")"

    # Convert to PNG (toktx needs PNG/JPG input)
    if ! convert "$src" -strip "$tmpfile" 2>/dev/null; then
        echo "  WARN: Failed to convert $src to PNG, copying as-is"
        cp "$src" "$OUTPUT_DIR/$rel"
        return 1
    fi

    # Get dimensions - ASTC needs multiples of block size, toktx handles this
    # Use 6x6 ASTC blocks for good compression ratio
    if toktx --encode astc \
        --astc_blk_d 6x6 \
        --astc_quality medium \
        --t2 \
        --assign_oetf srgb \
        --threads 1 \
        "$outfile" "$tmpfile" 2>/dev/null; then
        rm -f "$tmpfile"
        return 0
    else
        # Fallback: try without srgb assignment
        if toktx --encode astc \
            --astc_blk_d 6x6 \
            --astc_quality medium \
            --t2 \
            --threads 1 \
            "$outfile" "$tmpfile" 2>/dev/null; then
            rm -f "$tmpfile"
            return 0
        else
            echo "  WARN: toktx failed for $src, copying original"
            cp "$src" "$OUTPUT_DIR/$rel"
            rm -f "$tmpfile"
            return 1
        fi
    fi
}
export -f convert_texture
export OUTPUT_DIR TEMP_DIR

# Find all textures
TEXTURE_LIST=$(find . \( -iname "*.tga" -o -iname "*.png" \) -type f | sort)
TEXTURE_COUNT=$(echo "$TEXTURE_LIST" | wc -l)
echo "  Found $TEXTURE_COUNT texture files"

# Process in parallel using xargs
echo "$TEXTURE_LIST" | xargs -P "$NPROC" -I {} bash -c 'convert_texture "$@"' _ {}

TEXTURE_OK=$(find "$OUTPUT_DIR" -name "*.ktx2" | wc -l)
echo "  Converted: $TEXTURE_OK / $TEXTURE_COUNT textures to KTX2 ASTC"

# ============================================
# Step 3: Convert sounds WAV -> OGG
# ============================================
echo ""
echo "[3/4] Converting sounds to OGG Vorbis (quality 1 ~80kbps)..."

SOUND_COUNT=0
SOUND_OK=0

convert_sound() {
    local src="$1"
    local rel="${src#./}"
    local base="${rel%.*}"
    local outfile="$OUTPUT_DIR/${base}.ogg"

    mkdir -p "$(dirname "$outfile")"

    # Use ffmpeg with OGG Vorbis at quality level 1 (roughly 80kbps)
    # This is the lowest "still acceptable" quality
    if ffmpeg -y -i "$src" -c:a libvorbis -q:a 1 -ac 1 "$outfile" 2>/dev/null; then
        return 0
    else
        echo "  WARN: Failed to convert $src, copying original"
        cp "$src" "$OUTPUT_DIR/$rel"
        return 1
    fi
}
export -f convert_sound

SOUND_LIST=$(find . -iname "*.wav" -type f | sort)
SOUND_COUNT=$(echo "$SOUND_LIST" | wc -l)
echo "  Found $SOUND_COUNT sound files"

echo "$SOUND_LIST" | xargs -P "$NPROC" -I {} bash -c 'convert_sound "$@"' _ {}

SOUND_OK=$(find "$OUTPUT_DIR" -name "*.ogg" | wc -l)
echo "  Converted: $SOUND_OK / $SOUND_COUNT sounds to OGG"

# ============================================
# Step 4: Package into .7z ultra
# ============================================
echo ""
echo "[4/4] Packaging into .7z with ultra compression..."

ARCHIVE="$ROOT_DIR/openarena_assets_compressed.7z"
rm -f "$ARCHIVE"

cd "$OUTPUT_DIR"
7z a -t7z -mx=9 -mfb=273 -ms=on -md=64m -mmt="$NPROC" "$ARCHIVE" . 2>&1 | tail -5

cd "$ROOT_DIR"

# ============================================
# Results
# ============================================
echo ""
echo "============================================="
echo "  COMPRESSION RESULTS"
echo "============================================="

COMPRESSED_DIR_SIZE=$(du -sb "$OUTPUT_DIR" | cut -f1)
ARCHIVE_SIZE=$(stat -c%s "$ARCHIVE")

echo ""
echo "  Original assets:         $(du -sh "$ASSETS_DIR" | cut -f1) ($ORIG_SIZE bytes)"
echo "  Compressed directory:    $(du -sh "$OUTPUT_DIR" | cut -f1) ($COMPRESSED_DIR_SIZE bytes)"
echo "  .7z archive:             $(numfmt --to=iec-i --suffix=B $ARCHIVE_SIZE) ($ARCHIVE_SIZE bytes)"
echo ""
echo "  Dir compression ratio:   $(echo "scale=1; $ORIG_SIZE * 100 / $COMPRESSED_DIR_SIZE" | bc)% -> $(echo "scale=1; 100 - ($COMPRESSED_DIR_SIZE * 100 / $ORIG_SIZE)" | bc)% saved"
echo "  .7z compression ratio:   $(echo "scale=1; $ORIG_SIZE / $ARCHIVE_SIZE" | bc)x smaller"
echo "  .7z savings:             $(echo "scale=1; 100 - ($ARCHIVE_SIZE * 100 / $ORIG_SIZE)" | bc)% saved"
echo ""
echo "  Textures converted:      $TEXTURE_OK / $TEXTURE_COUNT"
echo "  Sounds converted:        $SOUND_OK / $SOUND_COUNT"
echo ""
echo "  Output: $ARCHIVE"
echo "============================================="

# Clean up temp dir
rm -rf "$TEMP_DIR"
