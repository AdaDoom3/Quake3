#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
#   Iterative Visual Test Suite for Q3 Raytracing Engine
# ═══════════════════════════════════════════════════════════════════════════════
#
# Runs the engine under Xvfb + lavapipe (software Vulkan with ray tracing)
# and captures screenshots via ImageMagick's `import` for human analysis.
#
# Tests:
#   T1 - Startup:    Does the engine launch and stay alive?
#   T2 - First Frame: Is the raytraced image non-black?
#   T3 - Stability:  5 rapid screenshots — no crashes, frames consistent?
#   T4 - Movement:   WASD + mouse look — does the view change?
#   T5 - Final:      Clean shutdown after all tests.
#
# Prerequisites:  ./test_setup.sh   (installs Xvfb, mesa-vulkan, xdotool, etc.)
# Build first:    ./nob
# Usage:          ./test_engine.sh [timeout_seconds]

set -euo pipefail

TIMEOUT="${1:-20}"
DISPLAY_NUM=":99"
SCREENSHOT_DIR="/tmp/q3_test_screenshots"
ENGINE_BIN="./build/quake3"
ENGINE_LOG="/tmp/q3_engine.log"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
log()  { echo -e "${GREEN}[TEST]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail() { echo -e "${RED}[FAIL]${NC} $*"; }

# ── Setup ────────────────────────────────────────────────────────────────────

log "Creating screenshot directory: $SCREENSHOT_DIR"
rm -rf "$SCREENSHOT_DIR"
mkdir -p "$SCREENSHOT_DIR"

# Ensure Xvfb is running on the test display
if ! pgrep -x Xvfb > /dev/null; then
  log "Starting Xvfb on $DISPLAY_NUM"
  Xvfb $DISPLAY_NUM -screen 0 1280x720x24 2>/dev/null &
  sleep 1
fi

export DISPLAY=$DISPLAY_NUM
export XDG_RUNTIME_DIR="/tmp/xdg-runtime-$$"
mkdir -p "$XDG_RUNTIME_DIR"

# Quick check that the display is reachable
if ! import -window root -display $DISPLAY_NUM /dev/null 2>/dev/null; then
  fail "Cannot capture from $DISPLAY_NUM — is Xvfb running?"
  exit 1
fi
log "Xvfb responding on $DISPLAY_NUM"

# ── T1: Engine Launch ────────────────────────────────────────────────────────

log "═══ T1: Engine Launch ═══"

cd "$(dirname "$0")"
$ENGINE_BIN > "$ENGINE_LOG" 2>&1 &
ENGINE_PID=$!
log "Engine PID: $ENGINE_PID"

sleep 3

if ! kill -0 $ENGINE_PID 2>/dev/null; then
  fail "T1 FAIL: Engine crashed during startup!"
  echo "--- Engine Log ---"
  cat "$ENGINE_LOG"
  echo "--- End Log ---"
  exit 1
fi
log "T1 PASS: Engine alive after 3s"

# ── T2: First Frame ──────────────────────────────────────────────────────────

log "═══ T2: First Frame ═══"

import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t2_first_frame.png" 2>/dev/null || true

if [ -f "$SCREENSHOT_DIR/t2_first_frame.png" ]; then
  SIZE=$(stat -c%s "$SCREENSHOT_DIR/t2_first_frame.png" 2>/dev/null || echo "0")
  if [ "$SIZE" -gt 5000 ]; then
    log "T2 PASS: First frame captured (${SIZE} bytes — has content)"
  else
    warn "T2: First frame suspiciously small (${SIZE} bytes)"
  fi
else
  warn "T2: Could not capture first frame"
fi

# ── T3: Temporal Stability (rapid burst) ─────────────────────────────────────

log "═══ T3: Temporal Stability ═══"

for i in $(seq 1 5); do
  sleep 0.5
  import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t3_frame_${i}.png" 2>/dev/null || true
  log "T3: Captured frame $i"
done

if ! kill -0 $ENGINE_PID 2>/dev/null; then
  fail "T3 FAIL: Engine crashed during frame burst!"
  cat "$ENGINE_LOG"
  exit 1
fi
log "T3 PASS: Stable through 5-frame burst"

# ── T4: Movement Test ────────────────────────────────────────────────────────

log "═══ T4: Movement Test ═══"

if command -v xdotool &> /dev/null; then
  # Forward walk (W key) — 4 screenshots over 1s
  log "T4: Forward (W)..."
  xdotool keydown w
  for i in $(seq 1 4); do
    sleep 0.25
    import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t4_fwd_${i}.png" 2>/dev/null || true
  done
  xdotool keyup w
  log "T4: Captured 4 forward frames"

  # Strafe left (A key)
  log "T4: Strafe left (A)..."
  xdotool keydown a
  sleep 0.5
  import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t4_strafe.png" 2>/dev/null || true
  xdotool keyup a

  # Jump (Space)
  log "T4: Jump (Space)..."
  xdotool key space
  sleep 0.15
  import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t4_jump_up.png" 2>/dev/null || true
  sleep 0.35
  import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t4_jump_land.png" 2>/dev/null || true

  # Mouse look — right, left, up
  log "T4: Mouse look..."
  xdotool mousemove_relative -- 200 0
  sleep 0.4
  import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t4_look_right.png" 2>/dev/null || true

  xdotool mousemove_relative -- -400 0
  sleep 0.4
  import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t4_look_left.png" 2>/dev/null || true

  xdotool mousemove_relative -- 200 -150
  sleep 0.4
  import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t4_look_up.png" 2>/dev/null || true

  log "T4 PASS: Movement simulation complete"
else
  warn "T4 SKIP: xdotool not installed (run test_setup.sh)"
fi

# ── T5: Final State + Cleanup ────────────────────────────────────────────────

log "═══ T5: Final State ═══"

sleep 1
import -window root -display $DISPLAY_NUM "$SCREENSHOT_DIR/t5_final.png" 2>/dev/null || true

if kill -0 $ENGINE_PID 2>/dev/null; then
  kill $ENGINE_PID 2>/dev/null || true
  sleep 1
  kill -9 $ENGINE_PID 2>/dev/null || true
  log "T5 PASS: Engine shut down"
else
  warn "T5: Engine already exited"
fi

# ── Results ──────────────────────────────────────────────────────────────────

log "═══ Results ═══"
echo ""
echo "Engine log (first 30 lines):"
head -30 "$ENGINE_LOG"
echo ""
echo "Screenshots:"
ls -1 "$SCREENSHOT_DIR/"*.png 2>/dev/null | while read -r f; do
  SIZE=$(stat -c%s "$f")
  printf "  %-40s %6d bytes\n" "$(basename "$f")" "$SIZE"
done
TOTAL=$(ls "$SCREENSHOT_DIR/"*.png 2>/dev/null | wc -l)
log "Total: $TOTAL screenshots in $SCREENSHOT_DIR"

rm -rf "$XDG_RUNTIME_DIR"
log "Done."
