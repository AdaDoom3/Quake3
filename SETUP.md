# Development Environment Setup

## Required Dependencies

### For Software Raytracer (Testing Only)
```bash
# Already installed:
gcc -lm
```

### For Full Vulkan Raytracing Engine
```bash
# Ubuntu/Debian:
sudo apt-get update
sudo apt-get install -y \
    libsdl2-dev \
    libvulkan-dev \
    vulkan-tools \
    vulkan-validationlayers \
    glslang-tools \
    mesa-vulkan-drivers  # or nvidia-vulkan-icd for NVIDIA

# Verify installation:
vulkaninfo | head -20
glslangValidator --version
pkg-config --modversion sdl2
```

### Hardware Requirements
- **GPU**: RTX 2060 / RX 6600 or better (ray tracing support)
- **VRAM**: 4 GB minimum
- **Driver**: Latest Vulkan 1.3 compatible driver

## Current Status

✅ BSP loader working (tested with aggressor.bsp: 5706 vertices, 3758 triangles)
✅ TGA loader working (tested with 256×32 texture)
✅ Test assets available (50+ maps in assets/maps/)

## Build Commands

```bash
# Test BSP loading
gcc -DTEST_BSP quake3rt.c -o test_bsp -lm
./test_bsp assets/maps/aggressor.bsp

# Test TGA loading
gcc -DTEST_TGA quake3rt.c -o test_tga -lm
./test_tga assets/textures/gothic_light/border7_ceil39.tga

# Software raytracer (no GPU needed, for testing)
gcc -DSOFT_RT quake3rt.c -o q3rt_soft -lm -lpthread
./q3rt_soft --map aggressor --screenshot test.tga

# Full Vulkan version (requires setup above)
gcc -std=c99 -O3 -march=native quake3rt.c -o q3rt -lSDL2 -lvulkan -lm
./q3rt --map aggressor --screenshot test.tga
```

## Next Steps

1. Build software raytracer for screenshot validation
2. Install Vulkan dependencies
3. Port to Vulkan raytracing pipeline
4. Add game logic and physics
