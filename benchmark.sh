#!/bin/bash
echo "=== Q3RT Benchmark ==="

bench() {
    local map=$1
    local name=$(basename $map .bsp)
    echo -n "$name: "
    start=$(date +%s%N)
    timeout 30 ./q3rt_soft $map /tmp/bench_$name.tga 0 0 100 1 0 0 >/dev/null 2>&1
    end=$(date +%s%N)
    ms=$(( (end - start) / 1000000 ))
    tris=$(./test_bsp $map 2>/dev/null | grep triangles | awk '{print $4}')
    fps=$(awk "BEGIN {printf \"%.2f\", 1000.0/$ms}")
    echo "${ms}ms ($tris tris, ${fps} FPS)"
}

for map in assets/maps/{aggressor,ce1m7,delta,dm4ish}.bsp; do
    bench $map
done

echo ""
echo "Parallel batch (4 maps):"
start=$(date +%s%N)
for map in assets/maps/{fan,dm6ish,cbctf1,hydronex}.bsp; do
    ./q3rt_soft $map /tmp/batch_$(basename $map .bsp).tga >/dev/null 2>&1 &
done
wait
end=$(date +%s%N)
ms=$(( (end - start) / 1000000 ))
echo "Total: ${ms}ms (parallel speedup: $(awk "BEGIN {printf \"%.1f\", 4.0 * 15000 / $ms}")x)"
