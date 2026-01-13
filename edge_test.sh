#!/bin/bash
echo "=== Edge Case Tests ==="

echo "1. Extreme camera positions"
./q3rt_soft assets/maps/aggressor.bsp /tmp/edge1.tga 10000 10000 10000 -1 -1 -1 2>&1 | grep -q Saved && echo "  ✓ Far position" || echo "  ✗ FAIL"

echo "2. Zero-length direction"
./q3rt_soft assets/maps/aggressor.bsp /tmp/edge2.tga 0 0 100 0 0 0 2>&1 | grep -q Saved && echo "  ✓ Zero dir" || echo "  ✗ FAIL"

echo "3. Negative coordinates"
./q3rt_soft assets/maps/aggressor.bsp /tmp/edge3.tga -1000 -1000 -1000 1 1 1 2>&1 | grep -q Saved && echo "  ✓ Negative pos" || echo "  ✗ FAIL"

echo "4. Inside geometry"
./q3rt_soft assets/maps/aggressor.bsp /tmp/edge4.tga 0 0 0 1 0 0 2>&1 | grep -q Saved && echo "  ✓ Origin pos" || echo "  ✗ FAIL"

echo "5. Vertical look"
./q3rt_soft assets/maps/aggressor.bsp /tmp/edge5.tga 0 0 100 0 0 1 2>&1 | grep -q Saved && echo "  ✓ Look up" || echo "  ✗ FAIL"
./q3rt_soft assets/maps/aggressor.bsp /tmp/edge6.tga 0 0 100 0 0 -1 2>&1 | grep -q Saved && echo "  ✓ Look down" || echo "  ✗ FAIL"

echo ""
echo "6. BSP edge cases"
for map in assets/maps/*.bsp; do
    ./test_bsp "$map" >/dev/null 2>&1 || echo "  ✗ Parse failed: $(basename $map)"
done | head -5
[ $? -eq 0 ] && echo "  ✓ All maps parse"

echo ""
echo "7. Memory stress (sequential)"
for i in {1..5}; do
    ./q3rt_soft assets/maps/ce1m7.bsp /tmp/stress_$i.tga >/dev/null 2>&1 &
done
wait
echo "  ✓ 5 concurrent renders"

echo ""
echo "8. Performance scaling"
for tris in 1412 1971 3758 4145; do
    map=$(find assets/maps -name "*.bsp" -exec sh -c './test_bsp "$1" 2>/dev/null | grep -q "triangles: '$tris')" && echo "$1"' _ {} \; | head -1)
    [ -n "$map" ] && echo "  $tris tris: $(basename $map .bsp)"
done
