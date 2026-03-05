#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
#   Weapon Screenshot Capture Script
# ═══════════════════════════════════════════════════════════════════════════════
#
# Captures the following screenshots using headless Wayland + lavapipe:
#
#   1. Q3 scene (oa_dm1) — weapon close to camera, shifted left
#   2. M4 in Aztec (csp_aztec) — left hand (cl_righthand=0)
#   3. M4 in Aztec (csp_aztec) — right hand (cl_righthand=1)
#
# Prerequisites: ./sdk (build first), sway, grim, lavapipe
# Usage: ./tools/capture_weapon_screenshots.sh

set -euo pipefail
cd "$(dirname "$0")/.."

ENGINE="./build/q3"
OUT="screenshots"
M4_MDL="assets/models/weapons/v_rif_m4a1.mdl"

if [ ! -x "$ENGINE" ]; then
  echo "[!] Engine not built. Run ./sdk first."
  exit 1
fi

# Headless Wayland via sway + lavapipe (software Vulkan with RT)
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/xdg-runtime-$$}"
export LP_NUM_THREADS=16
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
mkdir -p "$XDG_RUNTIME_DIR"

# Start headless sway if needed
SWAY_PID=""
if [ -z "${WAYLAND_DISPLAY:-}" ]; then
  sway --config /dev/null &>/tmp/sway_weapon_screenshots.log &
  SWAY_PID=$!
  sleep 2
  export WAYLAND_DISPLAY=wayland-0
  echo "[+] Started headless sway (PID $SWAY_PID)"
fi

cleanup() {
  [ -n "$SWAY_PID" ] && kill "$SWAY_PID" 2>/dev/null || true
}
trap cleanup EXIT

run_screenshot() {
  local name="$1"
  shift
  local tga_out="/tmp/q3_weapon_${name}.tga"
  local png_out="${OUT}/${name}.png"
  echo ""
  echo "═══ Screenshot: $name ═══"
  echo "  CMD: $ENGINE $* --screenshot $tga_out --res 1280x720 --spp 1"
  timeout 180 $ENGINE "$@" --screenshot "$tga_out" --res 1280x720 --spp 1 2>&1 | tail -15
  if [ -f "$tga_out" ]; then
    local sz
    sz=$(stat -c%s "$tga_out")
    echo "  => TGA: $tga_out ($sz bytes)"
    if command -v convert &>/dev/null; then
      convert "$tga_out" "$png_out" 2>/dev/null && echo "  => PNG: $png_out"
    else
      echo "  [!] ImageMagick not found, keeping TGA"
      cp "$tga_out" "${OUT}/${name}.tga"
    fi
    rm -f "$tga_out"
  else
    echo "  => FAILED: no screenshot produced"
  fi
}

echo "=== Weapon Screenshot Capture ==="
echo ""

# ── 1. Q3 scene — weapon close to camera, shifted left ──────────────────────
# Uses the "screenshot" preset: vm_fov=54, scale=0.65, offset=(3,-3,-1), left-hand
run_screenshot "q3_weapon_closeup_left" \
  oa_dm1.bsp \
  --vm-preset screenshot

# ── 2. M4 in Aztec — left hand (on the left) ────────────────────────────────
# Source mode, Aztec map, M4 weapon, left-hand viewmodel (HL2 cl_righthand=0)
run_screenshot "m4_aztec_lefthand" \
  csp_aztec.bsp --source \
  --weapon "$M4_MDL" \
  --vm-preset aztec \
  --cl-righthand 0

# ── 3. M4 in Aztec — right hand ─────────────────────────────────────────────
# Source mode, Aztec map, M4 weapon, right-hand viewmodel (HL2 cl_righthand=1)
run_screenshot "m4_aztec_righthand" \
  csp_aztec.bsp --source \
  --weapon "$M4_MDL" \
  --vm-preset aztec \
  --cl-righthand 1

echo ""
echo "=== Results ==="
echo "Screenshots in: $OUT/"
ls -lh "$OUT"/q3_weapon_*.png "$OUT"/m4_aztec_*.png 2>/dev/null || echo "Check output above for errors"
echo ""
echo "Done."
