#!/bin/bash
set -e

echo "=== Quake 3 RT Build Script ==="

if ! command -v glslangValidator &>/dev/null; then
echo "Installing glslang..."
sudo apt-get install -y glslang-tools
fi

if ! pkg-config --exists sdl2 vulkan; then
echo "Installing SDL2 and Vulkan..."
sudo apt-get install -y libsdl2-dev libvulkan-dev vulkan-tools mesa-vulkan-drivers
fi

echo "Compiling shaders..."
glslangValidator -V rt.comp -o rt.comp.spv --target-env vulkan1.3
glslangValidator -V game.comp -o game.comp.spv --target-env vulkan1.3
echo "✓ Shaders compiled"

echo "Building software RT..."
gcc -DSOFT_RT -fopenmp quake3rt.c -o q3rt_soft -lm -O2
echo "✓ Software RT ready"

echo "Building Vulkan RT..."
gcc -DVK_RT quake3rt.c -o q3rt -lSDL2 -lvulkan -lm -O2
echo "✓ Vulkan RT ready"

echo ""
echo "=== Build Complete ==="
echo "Software: ./q3rt_soft assets/maps/aggressor.bsp output.tga"
echo "Vulkan:   ./q3rt assets/maps/aggressor.bsp"
