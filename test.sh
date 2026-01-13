#!/bin/bash
set -e

echo "=== Q3RT Test Suite ==="

echo "Test 1: BSP parser"
./test_bsp assets/maps/aggressor.bsp | grep -q "5706 verts" && echo "✓ BSP load" || echo "✗ FAIL"

echo "Test 2: TGA loader"
./test_tga assets/textures/gothic_light/border7_ceil39.tga | grep -q "256×32" && echo "✓ TGA load" || echo "✗ FAIL"

echo "Test 3: Software RT renders"
./q3rt_soft assets/maps/aggressor.bsp /tmp/test_render.tga >/dev/null 2>&1 && \
test -f /tmp/test_render.tga && file /tmp/test_render.tga | grep -q "Targa" && \
echo "✓ Render" || echo "✗ FAIL"

echo "Test 4: Multiple maps"
for map in aggressor ce1m7 cbctf1 delta; do
    ./test_bsp assets/maps/$map.bsp >/dev/null 2>&1 && echo "  ✓ $map" || echo "  ✗ $map"
done

echo "Test 5: Camera positions"
./q3rt_soft assets/maps/aggressor.bsp /tmp/pos1.tga 100 200 300 1 0 0 2>&1 | grep -q "100.0,200.0,300.0" && \
echo "✓ Camera" || echo "✗ FAIL"

echo "Test 6: Stress test (large map)"
timeout 60 ./q3rt_soft assets/maps/ctf_inyard.bsp /tmp/stress.tga >/dev/null 2>&1 && \
echo "✓ Large map" || echo "✗ Timeout/crash"

echo ""
echo "=== Summary ==="
echo "Screenshots: $(ls screenshots/*.tga 2>/dev/null | wc -l) generated"
echo "Maps tested: $(ls assets/maps/*.bsp 2>/dev/null | wc -l) available"
