#!/bin/bash
echo "=== Q3RT Validation Report ==="
echo ""
echo "Code Statistics:"
echo "  quake3rt.c: $(wc -l < quake3rt.c) lines"
echo "  Shaders: $(wc -l < rt.comp) (rt.comp), $(wc -l < game.comp) (game.comp)"
echo "  Total: $(($(wc -l < quake3rt.c) + $(wc -l < rt.comp) + $(wc -l < game.comp))) LOC"
echo ""
echo "Asset Coverage:"
echo "  Maps: $(ls assets/maps/*.bsp 2>/dev/null | wc -l)"
echo "  Textures: $(find assets/textures -name '*.tga' 2>/dev/null | wc -l)"
echo "  Models: $(find assets/models -name '*.md3' 2>/dev/null | wc -l)"
echo ""
echo "Test Results:"
echo "  Screenshots: $(ls screenshots/*.tga 2>/dev/null | wc -l)"
for img in screenshots/*.tga; do
    [ -f "$img" ] || continue
    sz=$(du -h "$img" | cut -f1)
    dim=$(file "$img" | grep -o '[0-9]\+ x [0-9]\+')
    echo "    $(basename $img): $sz, $dim"
done
echo ""
echo "Build Artifacts:"
for bin in q3rt_soft test_bsp test_tga; do
    if [ -f "$bin" ]; then
        sz=$(du -h "$bin" | cut -f1)
        echo "  ✓ $bin ($sz)"
    else
        echo "  ✗ $bin (missing)"
    fi
done
echo ""
echo "Performance:"
./benchmark.sh 2>/dev/null | tail -5
echo ""
echo "Map Complexity:"
for map in assets/maps/{aggressor,ce1m7,ctf_inyard}.bsp; do
    name=$(basename $map .bsp)
    data=$(./test_bsp $map 2>/dev/null | grep -E "Vertices|triangles")
    echo "  $name: $(echo $data | tr '\n' ' ')"
done
