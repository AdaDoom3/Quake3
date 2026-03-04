#!/bin/bash
# Quick model rendering test — captures screenshots for visual inspection.
# Usage: ./tools/test_models.sh
# Outputs TGA screenshots to /tmp/q3_model_test/
#
# Requires: sway (headless Wayland), lavapipe (software Vulkan)

set -euo pipefail
cd "$(dirname "$0")/.."

OUT="/tmp/q3_model_test"
rm -rf "$OUT"
mkdir -p "$OUT"

ENGINE="./build/q3"
if [ ! -x "$ENGINE" ]; then
  echo "[!] Engine not built. Run ./sdk first."
  exit 1
fi

# Set up headless Wayland via sway (lavapipe needs it)
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/xdg-runtime-$$}"
mkdir -p "$XDG_RUNTIME_DIR"

# Start sway if not running
SWAY_PID=""
if [ -z "${WAYLAND_DISPLAY:-}" ]; then
  sway --config /dev/null &>/tmp/sway_test.log &
  SWAY_PID=$!
  sleep 2
  export WAYLAND_DISPLAY=wayland-0
  echo "[+] Started headless sway (PID $SWAY_PID)"
fi

cleanup() {
  [ -n "$SWAY_PID" ] && kill "$SWAY_PID" 2>/dev/null || true
}
trap cleanup EXIT

run_test() {
  local name="$1"
  shift
  local outfile="$OUT/${name}.tga"
  echo "[*] Test: $name"
  echo "    CMD: $ENGINE $* --screenshot $outfile --res 640x480 --spp 1"
  timeout 120 $ENGINE "$@" --screenshot "$outfile" --res 640x480 --spp 1 2>&1 | tail -30
  if [ -f "$outfile" ]; then
    local sz
    sz=$(stat -c%s "$outfile")
    echo "    => $outfile ($sz bytes)"
    # Convert to PNG for easy viewing
    if command -v convert &>/dev/null; then
      convert "$outfile" "$OUT/${name}.png" 2>/dev/null && echo "    => $OUT/${name}.png"
    fi
  else
    echo "    => FAILED: no screenshot produced"
  fi
  echo ""
}

echo "=== Q3 Model Rendering Tests ==="
echo ""

# Test 1: Q3 map with MD3 player model + weapon (default)
run_test "q3_default" oa_dm1.bsp

# Test 2: Q3 map with a different map
run_test "q3_dm3" oa_dm3.bsp

# Test 3: Source mode with CSPromod map (if available)
CSP_MAP=$(ls assets/maps/csp_*.bsp 2>/dev/null | head -1)
if [ -n "$CSP_MAP" ]; then
  CSP_BSP=$(basename "$CSP_MAP")
  run_test "source_csp" "$CSP_BSP" --source
fi

echo "=== Results ==="
echo "Screenshots in: $OUT/"
ls -lh "$OUT/"*.png 2>/dev/null || ls -lh "$OUT/"*.tga 2>/dev/null || echo "No screenshots produced"
