#!/bin/bash
set -e
echo "Building Quake3RT variants..."
echo ""
echo "1. Software raytracer (validation)"
gcc -O3 -DSOFT_RT -fopenmp -o q3rt_soft quake3rt.c -lm
echo "   ✓ q3rt_soft"
echo ""
echo "2. Vulkan GPU raytracer"
glslangValidator -V rt.comp -o rt.comp.spv 2>&1 | head -1
gcc -O3 -DVK_RT -o q3rt_vk quake3rt.c -lm -lvulkan -lSDL2
echo "   ✓ q3rt_vk + rt.comp.spv"
echo ""
echo "3. Test utilities"
gcc -O3 -DTEST_BSP -o test_bsp quake3rt.c -lm
gcc -O3 -DTEST_TGA -o test_tga quake3rt.c -lm
echo "   ✓ test_bsp, test_tga"
echo ""
echo "All builds complete!"
ls -lh q3rt_soft q3rt_vk test_bsp test_tga rt.comp.spv 2>/dev/null
