#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
#   Test Environment Setup for Q3 Raytracing Engine
# ═══════════════════════════════════════════════════════════════════════════════
#
# Installs dependencies needed for headless visual testing with lavapipe
# (software Vulkan) on a system without a GPU.
#
# Requirements: Ubuntu/Debian with apt-get and root access.
# Usage:  sudo ./test_setup.sh

set -euo pipefail

echo "=== Installing virtual test dependencies ==="

# Software Vulkan renderer (lavapipe — supports ray tracing!)
apt-get install -y mesa-vulkan-drivers

# Vulkan validation layers (optional but useful for debugging)
apt-get install -y vulkan-validationlayers

# Virtual framebuffer (fake X server for headless rendering)
apt-get install -y xvfb

# Screenshot capture via ImageMagick's `import` command
apt-get install -y imagemagick

# Input simulation for automated movement/camera tests
apt-get install -y xdotool

# SDL2 and Vulkan development libraries (for building the engine)
apt-get install -y libsdl2-dev libvulkan-dev

# GLSL-to-SPIR-V compiler (for building embedded shaders)
apt-get install -y glslang-tools

echo ""
echo "=== Verifying installation ==="
echo -n "  Vulkan:     " && vulkaninfo --summary 2>&1 | grep "deviceType" || echo "MISSING"
echo -n "  Xvfb:       " && which Xvfb || echo "MISSING"
echo -n "  import:     " && which import || echo "MISSING"
echo -n "  xdotool:    " && which xdotool || echo "MISSING"
echo -n "  glslang:    " && which glslangValidator || echo "MISSING"
echo ""
echo "=== Setup complete ==="
echo "Run ./test_engine.sh to execute the visual test suite."
