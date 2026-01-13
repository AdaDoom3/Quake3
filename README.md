# Quake 3 Raytracing Engine

Single-file raytracing Quake 3 engine with GLSL compute shaders. Renders authentic Q3 BSP maps with proper lighting, curved surfaces, and sky rendering.

## Features Implemented

### Core Rendering
- ✅ **BSP Loading**: IBSP v46 format with 17 lumps
- ✅ **Surface Types**: Planar, Triangle Soup, Bezier Patches
- ✅ **Bezier Patches**: Quadratic curve tessellation with smooth normals
- ✅ **Lightmaps**: 128x128 RGB precomputed lighting
- ✅ **Sky Detection**: SURF_SKY flag detection
- ✅ **Normal Interpolation**: Smooth shading across surfaces

### Geometry Processing
- **Planar Surfaces**: Direct triangle rendering (1:1 from BSP)
- **Triangle Soup**: Arbitrary triangle meshes
- **Bezier Patches**: Tessellated to L=3 subdivision
  - Proper grid assembly across patch boundaries
  - Normals computed from geometry derivatives
  - Handles all patch dimensions (3x3, 3x9, 5x3, etc.)

### Visual Quality
- **Lightmap Sampling**: Interpolated UVs, bounds-checked lookup
- **Directional Lighting**: Sun direction (1,1,2) with 50% contribution
- **Sky Rendering**: Proper sky blue (135,206,235) for SURF_SKY surfaces
- **Smooth Shading**: Normal interpolation on curved surfaces

## Current Statistics

**Test Maps:**
- **delta**: 20,352 verts, 16,802 tris, 44 lightmaps
- **czest3ctf**: 41,984 verts, 27,947 tris (424 patches)
- **aggressor**: 5,706 verts, 3,310 tris (planar only)

**Validation Results:** 5/5 tests passed (see VALIDATION_REPORT.md)

## Building

```bash
# Test BSP loading
gcc -O3 -DTEST_BSP -o test_bsp quake3rt.c -lm

# Test TGA loading
gcc -O3 -DTEST_TGA -o test_tga quake3rt.c -lm

# Software raytracer (validation/testing)
gcc -O3 -DSOFT_RT -fopenmp -o q3rt_soft quake3rt.c -lm

# Vulkan raytracer (WIP)
gcc -O3 -DVK_RT -o q3rt_vk quake3rt.c -lm -lvulkan -lSDL2
```

## Usage

```bash
# Render from default position
./q3rt_soft assets/maps/delta.bsp output.tga

# Render from custom position/direction
./q3rt_soft assets/maps/delta.bsp output.tga 2000 700 600 1 0 0
#                                            x    y   z   dx dy dz

# Parse spawn points
gcc -o parse_spawn parse_spawn.c -lm
./parse_spawn assets/maps/delta.bsp

# Verify render quality
gcc -o verify_render verify_render.c -lm
./verify_render output.tga
```

## Architecture

**Single File Design:**
- `quake3rt.c`: 380 LOC main engine
  - Math library (Haskell-style unions)
  - BSP loader with patch tessellation
  - TGA loader/saver
  - Software raytracer (validation)
  - Vulkan raytracer (WIP)

**Shader Files:**
- `rt.comp`: Ray query compute shader (38 LOC)
- `rt.rgen`: Ray generation shader (35 LOC)
- `game.comp`: Player physics (47 LOC)

**Test Infrastructure:**
- `test.sh`: Unit tests
- `benchmark.sh`: Performance tests
- `validate.sh`: Quality validation
- `VALIDATION_REPORT.md`: Comprehensive test results

## Visual Quality Progression

1. **Base Geometry**: Planar surfaces only, flat shading
2. **+ Patches**: Bezier curve tessellation, smooth normals
3. **+ Lightmaps**: Precomputed shadows and lighting
4. **+ Sky**: Proper sky detection and rendering

See `screenshots/` for rendered examples from each stage.

## Missing Features

- [ ] Texture sampling (base textures from .tga files)
- [ ] Vertex color modulation
- [ ] Shader effects (transparency, blending)
- [ ] Skybox textures
- [ ] MD3 model loading
- [ ] Interactive controls (WASD + mouse)
- [ ] Real-time GPU rendering

## Performance

**Software Raytracer (CPU):**
- Single-threaded: 0.1-0.3 FPS (validation only)
- OpenMP parallel: 0.3-0.8 FPS (2.6x speedup)

**Target (GPU Vulkan RT):**
- Expected: 60+ FPS with VK_KHR_ray_query
- Hardware: RTX 2060+ or equivalent

## References

- Original Q3 source: `reference/` directory
- BSP format: IBSP version 46
- Surface types: tr_bsp.c, tr_curve.c
- Lightmaps: 128x128 RGB per surface

## License

Code: Single-file engine implementation
Assets: From original Quake 3 Arena (not included in repo)
