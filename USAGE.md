# Quake 3 Raytracing Engine - Usage Guide

## Quick Start

```bash
# Build everything
./build_all.sh

# Software raytracer (CPU, validation)
./q3rt_soft assets/maps/delta.bsp output.tga
./q3rt_soft assets/maps/delta.bsp output.tga 2000 700 600 1 0 0  # Custom camera

# GPU raytracer (Vulkan, interactive)
./q3rt_vk assets/maps/delta.bsp
```

## Controls (GPU Version)

- **WASD**: Move forward/back/left/right
- **Mouse**: Look around (captured)
- **Space**: Move up
- **Left Shift**: Move down
- **ESC**: Exit

## Features Implemented

### Visual Quality
- ✅ Lightmap sampling (128x128 RGB per surface)
- ✅ Sky detection (SURF_SKY flag)
- ✅ Smooth normals on curved surfaces
- ✅ Bezier patch tessellation (L=3)
- ✅ Directional lighting (sun direction)

### Architecture
- ✅ Single-file engine (quake3rt.c - 740 LOC)
- ✅ GLSL compute shader (rt.comp - 60 LOC)
- ✅ Haskell-style C99 (union-based vectors)
- ✅ Vulkan raytracing (VK_KHR_ray_query)
- ✅ Acceleration structures (BLAS + TLAS)

### Performance
- **Software (CPU)**: 0.3-0.8 FPS (validation only)
- **GPU (Vulkan RT)**: 60+ FPS target (RTX 2060+)

## Build Targets

```bash
# Software raytracer with OpenMP
gcc -O3 -DSOFT_RT -fopenmp -o q3rt_soft quake3rt.c -lm

# Vulkan GPU raytracer
glslangValidator -V rt.comp -o rt.comp.spv
gcc -O3 -DVK_RT -o q3rt_vk quake3rt.c -lm -lvulkan -lSDL2

# Test utilities
gcc -O3 -DTEST_BSP -o test_bsp quake3rt.c -lm
gcc -O3 -DTEST_TGA -o test_tga quake3rt.c -lm
```

## Map Testing

Tested on 8 Q3 maps with complete visual validation:

- **delta**: 20,352 verts, 16,802 tris, 44 lightmaps
- **czest3ctf**: 41,984 verts, 27,947 tris, 424 patches
- **aggressor**: 5,706 verts, 3,310 tris (planar-heavy)

All maps render correctly with lightmaps, sky, and curved surfaces.

## Technical Details

### GPU Data Upload
1. Vertex buffer (position, UV, normal)
2. Index buffer (triangle list)
3. Surface mapping (trisurf)
4. Surface data (shader index, lightmap index)
5. Shader flags (SURF_SKY detection)
6. Lightmap texture array (128x128 RGBA8)

### Shader Bindings
- Binding 0: Acceleration structure (TLAS)
- Binding 1: Output image (storage image)
- Binding 2: Vertex buffer (SSBO)
- Binding 3: Index buffer (SSBO)
- Binding 4: Triangle→Surface mapping (SSBO)
- Binding 5: Surface data (SSBO)
- Binding 6: Shader flags (SSBO)
- Binding 7: Lightmaps (sampler2DArray)

### Push Constants
- mat4 invVP: Inverse view-projection matrix
- vec3 camPos: Camera position
- uint nlm: Number of lightmaps

## Next Steps

- [ ] Base texture sampling (TGA files)
- [ ] Vertex color modulation
- [ ] Transparency/blending effects
- [ ] MD3 model loading (players, items)
- [ ] Collision detection
- [ ] Physics integration
