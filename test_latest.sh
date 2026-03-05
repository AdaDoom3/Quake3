#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
#   test_latest.sh — Comprehensive test + benchmark script
# ═══════════════════════════════════════════════════════════════════════════════
#
# Runs the engine through all major configurations and captures screenshots.
# Designed for headless CI on lavapipe (software Vulkan) behind a proxy.
#
# Usage: ./test_latest.sh
# Prereqs: ./sdk (build first), sway, grim, ImageMagick (convert), p7zip-full
#
# Proxy note: if behind a corporate proxy, set these before apt-get:
#   export http_proxy=http://proxy:port https_proxy=http://proxy:port
#   apt-get -o Acquire::http::Proxy="$http_proxy" install -y <pkg>

set -euo pipefail
cd "$(dirname "$0")"

ENGINE="./build/q3"
OUT="test_results"
BENCH_FRAMES=10
RES="1280x720"
M4_MDL="assets/models/weapons/v_rif_m4a1.mdl"

# ── Preflight ──────────────────────────────────────────────────────────────────

if [ ! -x "$ENGINE" ]; then
  echo "[!] Engine not built. Run: cc -o sdk sdk.c && ./sdk"
  exit 1
fi
mkdir -p "$OUT"

# ── Headless Wayland + lavapipe setup ──────────────────────────────────────────
# Lavapipe = software Vulkan (Mesa llvmpipe RT). Always set LP_NUM_THREADS=16.
# Sway runs headless via WLR_BACKENDS=headless for CI/screenshot capture.

export LP_NUM_THREADS=16
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
export SDL_VIDEODRIVER=wayland

# Find or start headless sway
SWAY_PID=""
if [ -z "${WAYLAND_DISPLAY:-}" ]; then
  export WLR_BACKENDS=headless
  export WLR_RENDERER=pixman
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/xdg-runtime-$$}"
  mkdir -p "$XDG_RUNTIME_DIR"
  sway --config /dev/null &>/tmp/sway_test.log &
  SWAY_PID=$!
  sleep 2
  # Find the wayland socket sway created
  WAYLAND_DISPLAY=$(ls "$XDG_RUNTIME_DIR"/wayland-* 2>/dev/null | grep -v '\.lock' | head -1 | xargs basename)
  export WAYLAND_DISPLAY
  echo "[+] Started headless sway (PID $SWAY_PID), display=$WAYLAND_DISPLAY"
else
  echo "[+] Using existing Wayland display: $WAYLAND_DISPLAY"
  # Ensure XDG_RUNTIME_DIR is set (sway needs it)
  if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
    for D in /tmp/xdg-runtime-*; do
      if [ -S "$D/$WAYLAND_DISPLAY" ]; then
        export XDG_RUNTIME_DIR="$D"
        break
      fi
    done
  fi
fi

cleanup() {
  [ -n "$SWAY_PID" ] && kill "$SWAY_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "[+] Wayland: $WAYLAND_DISPLAY, XDG: $XDG_RUNTIME_DIR"
echo "[+] Vulkan:  $VK_ICD_FILENAMES"
echo ""

# ── Texture extraction (CSPromod) ──────────────────────────────────────────────
# The Aztec map textures live in a split 7z archive. Extract if not already done.

CSPROMOD_DIR="/tmp/cspromod_extract"
if [ ! -d "$CSPROMOD_DIR/cspromod_b105" ] && ls assets/textures/cspromod_b105.7z.001 &>/dev/null; then
  echo "[+] Extracting CSPromod textures..."
  mkdir -p "$CSPROMOD_DIR"
  if command -v 7z &>/dev/null; then
    7z x -y -o"$CSPROMOD_DIR" assets/textures/cspromod_b105.7z.001 2>&1 | tail -3
  else
    echo "[!] p7zip-full not installed. Install with:"
    echo "    apt-get install -y p7zip-full"
    echo "    (set http_proxy/https_proxy if behind a proxy)"
  fi
fi

# ── Helper: run benchmark and capture ──────────────────────────────────────────

run_test() {
  local NAME="$1"; shift
  local TGA="/tmp/q3_test_${NAME}.tga"
  local PNG="${OUT}/${NAME}.png"
  echo ""
  echo "═══ Test: $NAME ═══"
  echo "  CMD: $ENGINE $* --screenshot $TGA --res $RES --spp 1"
  local START=$(date +%s%N)
  timeout 180 $ENGINE "$@" --screenshot "$TGA" --res "$RES" --spp 1 2>&1 | \
    grep -E '\[benchmark\]|\[frame|\[textures\]|\[weapon\].*textures|\[heap\]|\[limits\]|\[figure\]|Avg|P50|Min|Max|EXIT' || true
  local END=$(date +%s%N)
  local ELAPSED=$(( (END - START) / 1000000 ))
  echo "  Wall time: ${ELAPSED}ms"
  if [ -f "$TGA" ]; then
    if command -v convert &>/dev/null; then
      convert "$TGA" "$PNG" 2>/dev/null && echo "  => $PNG ($(stat -c%s "$PNG" 2>/dev/null || echo "?") bytes)"
    else
      cp "$TGA" "${OUT}/${NAME}.tga"
      echo "  => ${OUT}/${NAME}.tga (no ImageMagick, keeping TGA)"
    fi
    rm -f "$TGA"
  else
    echo "  => FAILED: no screenshot produced"
  fi
}

run_benchmark() {
  local NAME="$1"; shift
  echo ""
  echo "═══ Benchmark: $NAME ($BENCH_FRAMES frames) ═══"
  echo "  CMD: $ENGINE $* --benchmark $BENCH_FRAMES --res $RES --spp 1"
  timeout 300 $ENGINE "$@" --benchmark "$BENCH_FRAMES" --res "$RES" --spp 1 2>&1 | \
    grep -E 'Avg|P50|P95|Min|Max|Total' || true
}

# ── Test suite ─────────────────────────────────────────────────────────────────

echo "╔══════════════════════════════════════════════════╗"
echo "║   Q3 Engine Test Suite                           ║"
echo "╚══════════════════════════════════════════════════╝"

# 1. Q3 map with default weapon (machinegun)
run_test "q3_dm1" oa_dm1.bsp

# 2. Aztec map with M4 weapon (Source engine mode)
run_test "aztec_m4" csp_aztec.bsp --source --weapon "$M4_MDL"

# 3. Aztec with M4 — left hand
run_test "aztec_m4_left" csp_aztec.bsp --source --weapon "$M4_MDL" --cl-righthand 0

# 4. Q3 with viewmodel screenshot preset
run_test "q3_vm_closeup" oa_dm1.bsp --vm-preset screenshot

# ── Benchmarks ─────────────────────────────────────────────────────────────────

echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║   Benchmarks                                     ║"
echo "╚══════════════════════════════════════════════════╝"

run_benchmark "q3_dm1" oa_dm1.bsp
run_benchmark "aztec_m4" csp_aztec.bsp --source --weapon "$M4_MDL"

# ── Edge case: tiny resolution (stress descriptor/TLAS path) ──────────────────
echo ""
echo "═══ Edge: Tiny resolution (320x240) ═══"
timeout 60 $ENGINE oa_dm1.bsp --screenshot /tmp/q3_tiny.tga --res 320x240 --spp 1 2>&1 | \
  grep -E 'Avg|P50|frame.*ms|render' | head -5
[ -f /tmp/q3_tiny.tga ] && echo "  => OK" || echo "  => FAILED"
rm -f /tmp/q3_tiny.tga

# ── Edge case: max SPP (stress RT workload) ───────────────────────────────────
echo ""
echo "═══ Edge: SPP=4 (heavy RT) ═══"
timeout 120 $ENGINE oa_dm1.bsp --screenshot /tmp/q3_spp4.tga --res 640x480 --spp 4 2>&1 | \
  grep -E 'Avg|P50|frame.*ms|render' | head -5
[ -f /tmp/q3_spp4.tga ] && echo "  => OK" || echo "  => FAILED"
rm -f /tmp/q3_spp4.tga

# ── Summary ────────────────────────────────────────────────────────────────────

echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║   Results                                        ║"
echo "╚══════════════════════════════════════════════════╝"
echo "Screenshots in: $OUT/"
ls -lh "$OUT"/*.png 2>/dev/null || ls -lh "$OUT"/*.tga 2>/dev/null || echo "(none)"
echo ""
echo "Done."
