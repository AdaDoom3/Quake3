#!/bin/bash
# ──────────────────────────────────────────────────────────────────────────────
#   generate_aztec_textures.sh — Download CC0 textures from ambientCG and
#   create stand-in TGA files for the CSPromod Aztec map (csp_aztec.bsp).
#
#   All source textures are CC0 (Creative Commons Zero) from ambientcg.com.
#   Requires: curl, unzip, ImageMagick (convert)
#
#   Usage:  bash generate_aztec_textures.sh
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SZ=512
DL=/tmp/aztec_textures
ASSET=assets

echo "[texgen] downloading CC0 textures from ambientcg.com..."
mkdir -p "$DL"

# Download texture packs (CC0 license)
download () { # $1=name $2=url
  local zip="$DL/${1}.zip"
  if [ ! -f "$zip" ]; then
    echo "  ↓ $1"
    curl -sL -o "$zip" "$2"
  fi
  if [ ! -d "$DL/$1" ]; then
    mkdir -p "$DL/$1"
    unzip -o "$zip" -d "$DL/$1" > /dev/null 2>&1
  fi
}

download bricks100    "https://ambientcg.com/get?file=Bricks100_1K-JPG.zip"     # Cut stone wall
download bricks061    "https://ambientcg.com/get?file=Bricks061_1K-JPG.zip"     # Grey masonry
download pavingstones "https://ambientcg.com/get?file=PavingStones150_1K-JPG.zip" # Stone floor
download grass001     "https://ambientcg.com/get?file=Grass001_1K-JPG.zip"       # Grass
download ground054    "https://ambientcg.com/get?file=Ground054_1K-JPG.zip"      # Sand
download ground103    "https://ambientcg.com/get?file=Ground103_1K-JPG.zip"      # Dirt
download planks030    "https://ambientcg.com/get?file=Planks030A_1K-JPG.zip"     # Wood planks

echo "[texgen] generating TGA textures at ${SZ}x${SZ}..."

# Helper: convert source JPG to TGA with optional modulate
gen () { # $1=source_jpg $2=output_tga $3=modulate(optional)
  local dir
  dir=$(dirname "$2")
  mkdir -p "$dir"
  if [ -n "${3:-}" ]; then
    convert "$1" -resize ${SZ}x${SZ}! -modulate "$3" TGA:"$2"
  else
    convert "$1" -resize ${SZ}x${SZ}! TGA:"$2"
  fi
}

# Source texture paths
STONE="$DL/bricks100/Bricks100_1K-JPG_Color.jpg"
STONE_N="$DL/bricks100/Bricks100_1K-JPG_NormalGL.jpg"
STONE_R="$DL/bricks100/Bricks100_1K-JPG_Roughness.jpg"
TRIM="$DL/bricks061/Bricks061_1K-JPG_Color.jpg"
TRIM_N="$DL/bricks061/Bricks061_1K-JPG_NormalGL.jpg"
FLOOR="$DL/pavingstones/PavingStones150_1K-JPG_Color.jpg"
FLOOR_N="$DL/pavingstones/PavingStones150_1K-JPG_NormalGL.jpg"
FLOOR_R="$DL/pavingstones/PavingStones150_1K-JPG_Roughness.jpg"
GRASS="$DL/grass001/Grass001_1K-JPG_Color.jpg"
GRASS_N="$DL/grass001/Grass001_1K-JPG_NormalGL.jpg"
GRASS_R="$DL/grass001/Grass001_1K-JPG_Roughness.jpg"
SAND="$DL/ground054/Ground054_1K-JPG_Color.jpg"
SAND_N="$DL/ground054/Ground054_1K-JPG_NormalGL.jpg"
SAND_R="$DL/ground054/Ground054_1K-JPG_Roughness.jpg"
DIRT="$DL/ground103/Ground103_1K-JPG_Color.jpg"
DIRT_N="$DL/ground103/Ground103_1K-JPG_NormalGL.jpg"
DIRT_R="$DL/ground103/Ground103_1K-JPG_Roughness.jpg"
WOOD="$DL/planks030/Planks030A_1K-JPG_Color.jpg"
WOOD_N="$DL/planks030/Planks030A_1K-JPG_NormalGL.jpg"

AZT="$ASSET/CSPROMOD/AZTEC"

# ── Stone walls (warm cut stone blocks, varied tints) ──
gen "$STONE" "$AZT/AZT_STONE3.tga"
gen "$STONE" "$AZT/AZT_STONEWALL3.tga"
gen "$STONE" "$AZT/AZT_STONEWALL3C.tga"      "85,95"
gen "$STONE" "$AZT/AZT_STONEWALL2.tga"        "100,115"
gen "$STONE" "$AZT/AZT_STONEWALL2C.tga"       "90,75"
gen "$STONE" "$AZT/AZT_STONE3B.tga"           "110,90"
gen "$STONE" "$AZT/AZT_COLUMN1.tga"           "95,80"
gen "$STONE" "$AZT/AZT_COLUMN1B.tga"          "92,85"
gen "$STONE" "$AZT/AZT_COLUMN2.tga"           "88,90"

# ── Stone floors ──
gen "$FLOOR" "$AZT/AZT_STONEFLOOR2.tga"
gen "$FLOOR" "$AZT/AZT_STONEFLOOR1.tga"       "105,95"
gen "$FLOOR" "$AZT/AZT_STONESTEPS.tga"
gen "$FLOOR" "$AZT/AZT_STONESTEPSB.tga"        "90,100"

# ── Stone trim / carvings (grey masonry with contrast) ──
gen "$TRIM" "$AZT/AZT_STONETRIM1.tga"
gen "$TRIM" "$AZT/AZT_STONETRIM2.tga"
gen "$TRIM" "$AZT/AZT_STONETRIM2C.tga"         "108,80"
gen "$TRIM" "$AZT/AZT_CARVING1.tga"
gen "$TRIM" "$AZT/AZT_CARVING1B.tga"           "95,85"
gen "$TRIM" "$AZT/AZT_CARVING2.tga"            "100,90"

# ── Grass ──
gen "$GRASS" "$AZT/AZT_GRASS1.tga"
gen "$GRASS" "$AZT/AZT_GRASS1B.tga"            "95,105"

# ── Sand / Dirt ──
gen "$SAND" "$AZT/AZT_SAND.tga"
gen "$DIRT" "$AZT/AZT_DIRT.tga"

# ── Wood / Doors ──
gen "$WOOD" "$AZT/AZT_WOOD1.tga"
gen "$WOOD" "$AZT/AZT_WOOD2.tga"
gen "$WOOD" "$AZT/AZT_DOOR.tga"
gen "$WOOD" "$AZT/AZT_DOORB.tga"               "80,110"

# ── Crates (slightly tinted wood) ──
for name in AZT_CRATE1 AZT_CRATE1B AZT_CRATE2 AZT_CRATE2B AZT_CRATE3 AZT_CRATE3B \
            AZT_CRATE4 AZT_CRATE4B AZT_CRATE5 AZT_CRATE5B AZT_CRATE5C; do
  gen "$WOOD" "$AZT/${name}.tga" "95,110"
done

# ── Normal maps ──
for name in AZT_STONE3 AZT_STONEWALL3 AZT_STONEWALL3C AZT_STONEWALL2 AZT_STONEWALL2C \
            AZT_STONE3B AZT_COLUMN1 AZT_COLUMN1B AZT_COLUMN2; do
  gen "$STONE_N" "$AZT/${name}_n.tga"
done
for name in AZT_STONEFLOOR1 AZT_STONEFLOOR2 AZT_STONESTEPS AZT_STONESTEPSB; do
  gen "$FLOOR_N" "$AZT/${name}_n.tga"
done
for name in AZT_STONETRIM1 AZT_STONETRIM2 AZT_STONETRIM2C AZT_CARVING1 AZT_CARVING1B AZT_CARVING2; do
  gen "$TRIM_N" "$AZT/${name}_n.tga"
done
for name in AZT_GRASS1 AZT_GRASS1B; do
  gen "$GRASS_N" "$AZT/${name}_n.tga"
done
gen "$SAND_N" "$AZT/AZT_SAND_n.tga"
gen "$DIRT_N" "$AZT/AZT_DIRT_n.tga"
for name in AZT_WOOD1 AZT_WOOD2 AZT_DOOR AZT_DOORB AZT_CRATE1 AZT_CRATE2 AZT_CRATE3 AZT_CRATE4 AZT_CRATE5; do
  gen "$WOOD_N" "$AZT/${name}_n.tga"
done

# ── Roughness maps ──
for name in AZT_STONE3 AZT_STONEWALL3 AZT_STONEWALL2 AZT_COLUMN1; do
  gen "$STONE_R" "$AZT/${name}_r.tga"
done
for name in AZT_STONEFLOOR1 AZT_STONEFLOOR2; do
  gen "$FLOOR_R" "$AZT/${name}_r.tga"
done
gen "$GRASS_R" "$AZT/AZT_GRASS1_r.tga"
gen "$SAND_R"  "$AZT/AZT_SAND_r.tga"
gen "$DIRT_R"  "$AZT/AZT_DIRT_r.tga"

# ── Special textures ──
echo "[texgen] generating special textures..."

# Backdrop (sky gradient)
convert -size ${SZ}x${SZ} gradient:'#6899c8'-'#4a7aab' -blur 0x3 TGA:"$AZT/AZT_BACKDROP.tga"

# Misc logos
mkdir -p "$ASSET/cspromod/aztec" "$ASSET/cspromod/logos"
convert -size ${SZ}x${SZ} xc:'#8a8070' TGA:"$ASSET/cspromod/aztec/azt_mikeispro.tga"
convert -size ${SZ}x${SZ} xc:'#605850' TGA:"$ASSET/cspromod/logos/csp_logo_carving.tga"

# Tools (invisible/skip — magenta placeholder)
mkdir -p "$ASSET/TOOLS"
for name in TOOLSNODRAW TOOLSSKYBOX TOOLSSKIP TOOLSHINT TOOLSINVISIBLE TOOLSTRIGGER; do
  convert -size 4x4 xc:'#ff00ff' TGA:"$ASSET/TOOLS/${name}.tga"
done

# Underwater
mkdir -p "$ASSET/dev"
convert -size ${SZ}x${SZ} xc:'#1a3050' TGA:"$ASSET/dev/dev_waterbeneath2.tga"

# Ivy (dark saturated grass)
mkdir -p "$ASSET/DE_CBBLE"
gen "$GRASS" "$ASSET/DE_CBBLE/IVY_CBBLE01A.tga"    "80,130"
gen "$GRASS" "$ASSET/DE_CBBLE/IVY_CBBLE01FULL.tga"  "75,140"

# Water (procedural blue-green)
convert -size ${SZ}x${SZ} plasma:'#2a5060'-'#1a3848' -blur 0x8 -modulate 100,120 TGA:/tmp/water_base.tga
mkdir -p "$ASSET/maps/csp_aztec/liquids" "$ASSET/maps/csp_aztec/cspromod/aztec"
for w in "water_pretty1_-288_-1088_80" "water_pretty1_1152_1104_-224" \
         "water_pretty1_2720_1728_-64" "water_pretty1_-128_144_-224"; do
  cp /tmp/water_base.tga "$ASSET/maps/csp_aztec/liquids/${w}.tga"
done

# Grass WVT patches (blend textures)
gen "$GRASS" "$ASSET/maps/csp_aztec/cspromod/aztec/azt_grass1_wvt_patch.tga"  "90,100"
gen "$GRASS" "$ASSET/maps/csp_aztec/cspromod/aztec/azt_grass1b_wvt_patch.tga" "90,100"

echo "[texgen] done — $(find "$ASSET" -name '*.tga' -newer "$0" 2>/dev/null | wc -l) textures generated"
echo "[texgen] all textures are CC0 from ambientcg.com"
