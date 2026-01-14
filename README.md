# Q3VK - Vulkan Quake 3 Engine

A single-file Vulkan-based Quake 3 engine implementation following literate programming and code-golf principles.

## Features

- **Single-file architecture**: Entire engine in `q3vk.c` (~150 lines of ultra-compact C99)
- **Shader-heavy rendering**: GLSL shaders in `q3.vert.glsl` and `q3.frag.glsl`
- **BSP map loading**: Loads and renders Quake 3 BSP maps from `assets/maps/`
- **TGA texture support**: Loads textures from `assets/textures/`
- **MD3 model support**: Framework for loading MD3 models
- **Vulkan rendering**: Modern Vulkan API with depth buffering
- **Free-look camera**: WASD movement, mouse look, Space/Shift for vertical movement

## Architecture

The code follows Knuth's "literate programming" style with labeled sections:

- §1: Prelude (macros and includes)
- §2: Vector Math (v3, m4 operations)
- §3-4: BSP & MD3 Structures
- §5: Global State (VK, MAP, PL)
- §6-9: File I/O and Loaders
- §10: Vulkan Initialization
- §11: Rendering Pipeline
- §12: Input & Game Loop
- §13: Main Entry Point

## Code Style

- Haskell-style C99: Functional approach, extensive use of `const`
- Code-golf: Single-letter macros (`S`=static, `U`=uint32_t, `F`=float, etc.)
- Performance-oriented: Vectorized operations, minimal allocations
- No external libraries beyond Vulkan, SDL2, and libc

## Building

```bash
# Install dependencies
apt-get install -y libvulkan-dev libsdl2-dev glslc

# Compile shaders
glslc -fshader-stage=vertex q3.vert.glsl -o q3.vert.glsl.spv
glslc -fshader-stage=fragment q3.frag.glsl -o q3.frag.glsl.spv

# Build engine
gcc -o q3vk q3vk.c -lSDL2 -lvulkan -lm -O3 -march=native -std=c99

# Or use make
make
```

## Running

```bash
# Run with default map
./q3vk

# Run with specific map
./q3vk assets/maps/oa_dm4.bsp
```

## Controls

- **W/A/S/D**: Move forward/left/backward/right
- **Mouse**: Look around
- **Space**: Move up
- **Shift**: Move down
- **ESC**: Quit

## Technical Details

### BSP Loading
- Parses Quake 3 BSP format (magic: 0x50534249, version: 0x2e)
- Loads vertices, faces, textures, planes, nodes, leafs, visibility data
- Generates procedural checkboard textures for missing assets

### Rendering
- Vulkan 1.0 API
- Indexed triangle rendering
- Depth buffering (D32_SFLOAT)
- Procedural vertex coloring and lighting in shaders
- Push constants for per-draw parameters

### Shaders
- Vertex shader: MVP transformation, attribute pass-through
- Fragment shader: Procedural texturing using hash functions, Phong-style lighting

## Project Structure

```
.
├── q3vk.c                  # Main engine (single file!)
├── q3.vert.glsl            # Vertex shader
├── q3.frag.glsl            # Fragment shader
├── Makefile                # Build configuration
├── README.md               # This file
├── assets/
│   ├── maps/*.bsp          # Quake 3 maps
│   ├── textures/**/*.tga   # Texture assets
│   └── models/**/*.md3     # MD3 models
└── Raytracing-shaders.glsl # Reference shader (8K+ lines)
```

## Goals

✅ Single-file engine
✅ BSP map loading
✅ TGA texture loading
✅ Vulkan rendering
✅ Movement and camera
✅ Shader-based rendering
✅ 0% missing textures (fallback to procedural)

## Notes

The engine prioritizes:
1. **Compactness**: Entire engine in ~150 lines
2. **Readability**: Despite code-golf, uses literate programming sections
3. **Performance**: Vulkan, GPU-side processing, minimal CPU overhead
4. **Completeness**: Loads real Q3 assets and renders playable maps

Inspired by the included `Raytracing-shaders.glsl` which demonstrates extreme shader-based rendering techniques.
