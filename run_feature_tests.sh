#!/bin/bash
# Auto-generated feature test script
# Tests specific engine features with screenshots

XVFB="xvfb-run -a -s '-screen 0 1920x1080x24'"
ENGINE="./q3_integrated"
MAP="assets/maps/dm4ish.bsp"

echo "=== Test 1: spawn_point ==="
echo "Verify player spawns at correct location with proper orientation"
$XVFB $ENGINE $MAP --test 1 --frames 60-60
if [ -f test_1_*.ppm ]; then
  for img in test_1_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 1 complete"
else
  echo "✗ Test 1 failed - no screenshots"
fi
echo

echo "=== Test 2: forward_movement ==="
echo "Test WASD physics - move forward 500 units"
$XVFB $ENGINE $MAP --test 2 --frames 0-180
if [ -f test_2_*.ppm ]; then
  for img in test_2_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 2 complete"
else
  echo "✗ Test 2 failed - no screenshots"
fi
echo

echo "=== Test 3: camera_rotation ==="
echo "Test mouse look - 360 degree rotation"
$XVFB $ENGINE $MAP --test 3 --frames 0-240
if [ -f test_3_*.ppm ]; then
  for img in test_3_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 3 complete"
else
  echo "✗ Test 3 failed - no screenshots"
fi
echo

echo "=== Test 4: animation_ik ==="
echo "Test IK system - move IK target in circle"
$XVFB $ENGINE $MAP --test 4 --frames 0-300
if [ -f test_4_*.ppm ]; then
  for img in test_4_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 4 complete"
else
  echo "✗ Test 4 failed - no screenshots"
fi
echo

echo "=== Test 5: spring_dynamics ==="
echo "Test spring bones - oscillation and damping"
$XVFB $ENGINE $MAP --test 5 --frames 0-200
if [ -f test_5_*.ppm ]; then
  for img in test_5_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 5 complete"
else
  echo "✗ Test 5 failed - no screenshots"
fi
echo

echo "=== Test 6: weapon_large_scale ==="
echo "Test weapon rendering - increased scale to 0.1"
$XVFB $ENGINE $MAP --test 6 --frames 60-60
if [ -f test_6_*.ppm ]; then
  for img in test_6_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 6 complete"
else
  echo "✗ Test 6 failed - no screenshots"
fi
echo

echo "=== Test 7: texture_variety ==="
echo "Walk through map showing different textures"
$XVFB $ENGINE $MAP --test 7 --frames 0-360
if [ -f test_7_*.ppm ]; then
  for img in test_7_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 7 complete"
else
  echo "✗ Test 7 failed - no screenshots"
fi
echo

echo "=== Test 8: lightmap_quality ==="
echo "Static camera showing lightmap detail"
$XVFB $ENGINE $MAP --test 8 --frames 120-120
if [ -f test_8_*.ppm ]; then
  for img in test_8_*.ppm; do
    convert "$img" "${img%.ppm}.png"
  done
  echo "✓ Test 8 complete"
else
  echo "✗ Test 8 failed - no screenshots"
fi
echo

echo "=== Test Summary ==="
ls -lh test_*.png 2>/dev/null | wc -l
echo "tests completed with screenshots"
