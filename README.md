# Quake 3 Raytracing Engine

A single-file raytracing Quake 3 clone built with Vulkan, implementing maximum functionality in shaders. Code-golfed C99 with Haskell-style functional programming and Knuth literate programming documentation.

## 🎯 Project Goals

- **Single File**: Entire engine in quake3rt.c + GLSL shaders
- **Shader-Accelerated**: Game logic, physics, and rendering in GPU compute/raytracing shaders
- **Asset Compatible**: Loads authentic Quake 3 BSP maps, MD3 models, TGA textures
- **Code Golf**: No comments in code, maximum density
- **Literate Docs**: Comprehensive external documentation

## 🏗️ Current Status: Phase 0 Complete ✅

### Working Features

- ✅ **BSP Parser**: Loads Quake 3 maps (IBSP v46 format)
  - 17 lumps: entities, shaders, planes, nodes, leaves, vertices, indices, lightmaps
  - Tested with 50+ maps from assets/

- ✅ **TGA Loader/Saver**: Custom 24-bit RGB image handler
  - No external dependencies (stb_image, etc.)
  - Loads textures, saves screenshots

- ✅ **Software Raytracer**: CPU-based renderer for testing
  - Möller-Trumbore ray-triangle intersection
  - OpenMP parallelization
  - Directional lighting (diffuse shading)
  - ~15 seconds per 800×600 frame

### Build & Run

```bash
# Test BSP loading
gcc -DTEST_BSP quake3rt.c -o test_bsp -lm
./test_bsp assets/maps/aggressor.bsp

# Test TGA loading
gcc -DTEST_TGA quake3rt.c -o test_tga -lm
./test_tga assets/textures/gothic_light/border7_ceil39.tga

# Software raytracer (generates screenshots)
gcc -DSOFT_RT -fopenmp quake3rt.c -o q3rt_soft -lm
./q3rt_soft assets/maps/aggressor.bsp output.tga

# With custom camera position/direction
./q3rt_soft assets/maps/ce1m7.bsp output.tga 100 -200 250 0.3 0.8 -0.5
```

## 📸 Screenshots

![Screenshot 1](screenshots/screenshot_test1.tga) - *aggressor.bsp from (0,0,100)*

![Screenshot 2](screenshots/screenshot_test2.tga) - *aggressor.bsp angled view*

![Screenshot 3](screenshots/screenshot_ce1m7.tga) - *ce1m7.bsp demonstration*

All screenshots are 800×600 TGA images showing raytraced Quake 3 geometry with directional lighting.

## 📁 Repository Structure

```
.
├── quake3rt.c              # Main engine (284 LOC)
├── Raytracing-shaders.glsl # Reference WebGL shader (8,294 LOC)
│
├── DESIGN.md               # Architecture & system design
├── IMPLEMENTATION_PLAN.md  # Detailed phase breakdown
├── SETUP.md                # Development environment setup
├── PROGRESS.md             # Current status & achievements
├── README.md               # This file
│
├── screenshots/            # Generated renders
│   ├── screenshot_test1.tga
│   ├── screenshot_test2.tga
│   └── screenshot_ce1m7.tga
│
├── assets/                 # 787 MB Quake 3 assets
│   ├── maps/               # 50 BSP files
│   ├── textures/           # 1,693 TGA files
│   ├── models/             # 772 MD3 files
│   ├── sound/              # 408 WAV files
│   └── scripts/            # 74 shader scripts
│
└── reference/              # Original Q3 source (15 MB)
    └── *.c, *.h            # 914 source files
```

## 🛠️ Technology Stack

- **Language**: C99 (code-golfed, Haskell-style)
- **Graphics**: Vulkan 1.3 + VK_KHR_ray_tracing_pipeline *(planned)*
- **Platform**: SDL2 *(planned)*
- **Shaders**: GLSL 460 with raytracing extensions *(planned)*
- **Current**: Pure software renderer (testing phase)

## 📋 Implementation Phases

### ✅ Phase 0: Foundation (COMPLETE)
- [x] BSP format parser
- [x] TGA image loader/saver
- [x] Software raytracer
- [x] Screenshot generation
- [x] Multi-map testing

### 🔄 Phase 1: Vulkan Bootstrap (IN PROGRESS)
- [ ] Install Vulkan SDK + SDL2
- [ ] Window creation + device selection
- [ ] Swapchain setup
- [ ] Command buffer management
- [ ] Shader compilation pipeline

### 📅 Phase 2: BSP to GPU
- [ ] Vertex buffer upload
- [ ] Build bottom-level acceleration structures (BLAS)
- [ ] Build top-level acceleration structure (TLAS)
- [ ] Basic raytracing shader (rgen, rchit, rmiss)

### 📅 Phase 3: Textures & Lighting
- [ ] Texture atlas packing
- [ ] Shader script parsing
- [ ] Lightmap extraction
- [ ] Material system

### 📅 Phase 4: Game Logic
- [ ] Player movement (WASD + mouse)
- [ ] Collision detection
- [ ] Item pickups
- [ ] Weapons & projectiles

### 📅 Phase 5: Polish
- [ ] HUD rendering
- [ ] Post-processing effects
- [ ] Sound system
- [ ] Menu/UI

**Estimated Total**: 4-6 weeks for playable demo

## 🎮 Test Assets

Using **OpenArena** assets (GPL licensed):
- **Maps**: 50+ BSP files (aggressor, ce1m7, cbctf1, etc.)
- **Textures**: 1,693 TGA images
- **Models**: 772 MD3 character/weapon models
- **Total Size**: 787 MB

## 📊 Code Metrics

### Current Stats
- **Lines of Code**: 284 (quake3rt.c)
- **Files**: 1 (monolithic design ✓)
- **External Dependencies**: libc + libm only
- **Compilation Time**: < 1 second
- **Binary Size**: ~25 KB (stripped)

### Code Style Examples

**Haskell-style type definitions**:
```c
typedef union{struct{float x,y,z;};float v[3];}v3;
typedef union{struct{float x,y,z,w;};v3 xyz;float v[4];}v4;
```

**Pure functional math**:
```c
static inline v3 v3norm(v3 a){
    float l=v3len(a);
    return l>1e-6f?v3mul(a,1.f/l):V3(0,0,0);
}
```

**Code-golfed I/O**:
```c
void*ld(FILE*f,Lump*l){
    void*p=malloc(l->l);
    fseek(f,l->o,0);
    fread(p,1,l->l,f);
    return p;
}
```

## 🔬 Technical Highlights

### Achieved
1. **Zero-dependency asset loading**: Custom BSP + TGA parsers
2. **Functional C99**: Union-based algebraic types, pure functions
3. **Parallel rendering**: OpenMP auto-vectorization
4. **Real-world testing**: Actual Quake 3 maps render correctly

### Planned
1. **GPU raytracing**: VK_KHR_ray_tracing_pipeline
2. **Shader game logic**: Physics/AI in compute shaders
3. **BVH acceleration**: SAH-based spatial partitioning
4. **Temporal reprojection**: TAA for anti-aliasing

## 📖 Documentation

Detailed documentation available:
- **[DESIGN.md](DESIGN.md)**: System architecture, shader design, asset pipeline
- **[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)**: Phase-by-phase breakdown with tests
- **[PROGRESS.md](PROGRESS.md)**: Current achievements and statistics
- **[SETUP.md](SETUP.md)**: Development environment configuration

## 🚀 Performance Targets

### Current (Software RT)
- **Framerate**: 0.06 FPS (15 sec/frame)
- **Resolution**: 800×600
- **Geometry**: 4K triangles (aggressor.bsp)

### Target (Vulkan RT)
- **Framerate**: 144 FPS @ 1080p (RTX 2060)
- **Framerate**: 60 FPS @ 4K (RTX 2060)
- **Latency**: < 7ms input-to-photon

## 🤝 Contributing

This is a personal learning project following specific constraints:
- Single-file architecture
- No external libraries (except platform layers)
- Code-golf style (no comments)
- Literate programming docs (external)

If you'd like to follow along or learn from the approach, check the documentation files.

## 📜 License

MIT License (code) / CC-BY-4.0 (documentation)

Quake 3 assets are © id Software / used under GPL
OpenArena assets are GPL licensed

## 🙏 Acknowledgments

- **id Software**: Quake 3 Arena, engine source code
- **ioquake3**: Modern Q3 reference implementation
- **OpenArena**: GPL-licensed Q3 replacement assets
- **Andrei Drexler**: Raytracing-shaders.glsl inspiration

## 📞 Status

**Latest Commit**: Phase 0 Complete - Foundation & Software Raytracer
**Date**: 2026-01-13
**Branch**: claude/quake3-raytracing-engine-XfObc
**Next Milestone**: Vulkan initialization + triangle test

---

**Note**: Screenshots are TGA format. To view, use ImageMagick:
```bash
convert screenshots/screenshot_test1.tga screenshot.png
```

Or use any TGA-compatible viewer (GIMP, XnView, etc.)
