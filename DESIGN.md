# Quake 3 Raytracing Engine - Design Document

## Executive Summary

A single-file Quake 3 clone using Vulkan raytracing, implementing the entire game engine in literate programming style with shader-accelerated game logic. The engine reads authentic Quake 3 assets (BSP maps, MD3 models, textures) and implements gameplay through compute shaders, with a minimal C99 driver using SDL2 and Vulkan.

---

## I. Architectural Philosophy

### Core Tenets

1. **Monolithic Driver + Shader Brain**: All game logic, physics, collision, and rendering in GLSL compute/raytracing shaders
2. **Functional Purity**: Haskell-style C99 - pure functions, minimal state, algebraic data types via unions
3. **Code Golf Efficiency**: Zero comments in code, maximum density, leveraging compiler optimization
4. **Literate Documentation**: Comprehensive external documentation compensating for terse implementation
5. **Asset Authenticity**: Full compatibility with original Quake 3 Arena assets and file formats

### Technology Stack

- **Language**: C99 (ISO/IEC 9899:1999)
- **Graphics API**: Vulkan 1.3 with VK_KHR_ray_tracing_pipeline
- **Platform Layer**: SDL2 (windowing, input, timing)
- **Shader Language**: GLSL 460 with raytracing extensions
- **Build System**: Single gcc/clang invocation with -O3 -march=native

---

## II. System Architecture

### File Structure

```
quake3rt.c           - Monolithic engine (target: <10,000 LOC golf'd)
rt_core.glsl         - Raytracing pipeline & BVH traversal
rt_game.glsl         - Game logic compute shader
rt_physics.glsl      - Movement, collision, projectiles
rt_lighting.glsl     - Lightmap sampling, dynamic lights
rt_materials.glsl    - Texture sampling, blending modes
rt_postfx.glsl       - Motion blur, CRT effect, HUD
```

### Data Flow

```
SDL Events → Input State Buffer (GPU) → Game Logic Shader → Physics Shader →
→ BVH Update → Raytracing Shader → Post-FX → Swapchain
```

### Memory Architecture

**GPU Memory Layout**:
```
┌─────────────────────────────────────┐
│ BSP Data (vertices, planes, nodes)  │ 256 MB
├─────────────────────────────────────┤
│ Bottom-Level Acceleration Structs   │ 128 MB
├─────────────────────────────────────┤
│ Top-Level Acceleration Structure    │  32 MB
├─────────────────────────────────────┤
│ Texture Atlas (4096×4096 BC1/BC3)   │ 128 MB
├─────────────────────────────────────┤
│ Game State (entities, projectiles)  │  16 MB
├─────────────────────────────────────┤
│ Input/Output Buffers                │   8 MB
└─────────────────────────────────────┘
```

---

## III. Quake 3 Asset Pipeline

### Supported Formats

#### BSP Maps (*.bsp)
- **Lumps**: Entities, Planes, Nodes, Leaves, LeafFaces, LeafBrushes, Models, Brushes, BrushSides, Vertices, Indices, Shaders, Lightmaps
- **Geometry**: Bezier patches (tessellated on load), triangle soups
- **Collision**: Brush-based CSG via leaf/node tree
- **Acceleration**: Convert BSP tree to BVH for ray traversal

#### MD3 Models (*.md3)
- **Structure**: Multi-surface, multi-frame keyframe animation
- **LOD**: Support for 3 LOD levels (use highest only)
- **Vertex Compression**: 16-bit fixed-point XYZ (1/64 scale), packed normals
- **Tags**: Skeletal attachment points for weapon/head positioning

#### Textures
- **Formats**: TGA (24/32-bit), JPEG (via stb_image)
- **Compression**: Convert to BC1 (opaque) / BC3 (alpha) on load
- **Mipmaps**: Generate full chain using box filter
- **Atlas**: Pack into 4096×4096 atlas with 2-pixel borders

#### Shaders (*.shader)
- **Parse**: Quake 3 shader scripts for material properties
- **Features**: Multi-stage blending, tcMod (scroll, scale, rotate), alphaFunc
- **Implementation**: Bake into material parameters for shader evaluation

#### Sounds (*.wav)
- **Deferred**: Initial version uses silent mode, audio later

---

## IV. Shader Implementation

### Compute Shader: Game Logic (rt_game.glsl)

**Responsibilities**:
- Player input processing (WASD, mouse, jump, shoot)
- Weapon state machines (firing, reloading, switching)
- Item pickup detection (health, armor, ammo, weapons)
- Score tracking and game rules (DM, TDM, CTF logic)
- AI bot decision trees (pathfinding via BSP portals)

**Execution Model**:
- Dispatch: 1 thread per entity (players, bots, projectiles)
- Frequency: 60 Hz logic update
- Output: Updated entity positions, velocities, states

### Compute Shader: Physics (rt_physics.glsl)

**Responsibilities**:
- Quake 3 movement physics (acceleration, friction, air control)
- Collision detection via BSP leaf testing
- Projectile trajectories (rockets, grenades, plasma)
- Trigger volumes (teleporters, jump pads)

**Algorithms**:
- **Movement**: pm_accelerate/pm_friction from bg_pmove.c
- **Collision**: Swept sphere vs. BSP brushes (trace_t)
- **Clipping**: Iterative plane projection (max 5 iterations)

**Data Structures**:
```glsl
struct Entity {
    vec3 pos, vel, mins, maxs;
    quat orientation;
    uint state, weapon, health;
    float frameTime, nextThink;
};

struct Projectile {
    vec3 pos, vel;
    uint type, owner;
    float damage, splash, timeout;
};
```

### Ray Tracing Shader (rt_core.glsl)

**Pipeline Stages**:

1. **Ray Generation**: Primary rays from player camera
2. **Closest Hit**:
   - Sample lightmap + dynamic lights
   - Apply material shader (blending, animation)
   - Fetch albedo/normal from atlas
3. **Any Hit**: Alpha testing for grates/fences
4. **Miss**: Skybox sampling (6-sided cubemap)

**BVH Construction**:
- Bottom-level: Per-BSP-leaf triangle meshes
- Top-level: Instances for players, items, projectiles
- Update: Refit TLAS every frame (entities move)

**Lighting Model**:
```glsl
vec3 computeLighting(vec3 P, vec3 N, uint lightmapIdx) {
    vec3 direct = texelFetch(lightmaps, lightmapIdx).rgb;
    vec3 dynamic = vec3(0);
    for (int i = 0; i < numDynLights; ++i) {
        Light L = dynLights[i];
        vec3 toLight = L.pos - P;
        float d = length(toLight);
        if (d < L.radius) {
            float atten = 1.0 - d / L.radius;
            dynamic += L.color * atten * max(0, dot(N, normalize(toLight)));
        }
    }
    return direct + dynamic;
}
```

---

## V. C99 Driver Architecture

### Haskell-Style Patterns

```c
typedef struct { VkInstance i; VkDevice d; } VkCtx;
typedef struct { VkBuffer b; VkDeviceMemory m; } VkBuf;
typedef union { struct {int x,y;}; int v[2]; } iv2;
typedef union { float x,y,z,w; float v[4]; vec3; } vec4;

// Pure function pipeline
VkCtx vkInit(const char**exts, uint32_t n) { /*...*/ }
VkBuf vkAlloc(VkCtx ctx, VkDeviceSize sz, VkBufferUsageFlags u) { /*...*/ }
void vkUpload(VkBuf dst, const void*src, size_t n) { /*...*/ }

// Composition
#define pipe(f,g) ({ __typeof(g(0)) (*_g)(...)=g; f(_g); })
```

### Main Loop

```c
typedef struct {
    VkCtx vk; SDL_Window*w; VkSwapchainKHR sc;
    VkBuf bsp,tex,ents,input; VkAccelerationStructureKHR tlas,*blas;
    VkPipeline rtPipe,gamePipe,physPipe; uint64_t frame;
} State;

void tick(State*s) {
    pollInput(s);
    vkDispatch(s->gamePipe,  s->ents.b, s->input.b);
    vkDispatch(s->physPipe,  s->ents.b, s->bsp.b);
    vkUpdateTLAS(s->tlas, s->ents.b);
    uint32_t img=vkAcquire(s->sc);
    vkTraceRays(s->rtPipe, s->tlas, s->tex.b, img);
    vkPresent(s->sc, img);
    s->frame++;
}
```

### Asset Loading (Code-Golfed)

```c
typedef struct{int ofs,len;}Lump;
typedef struct{char sig[4];int ver;Lump l[17];}BSP;
void*L(BSP*b,int i,FILE*f){fseek(f,b->l[i].ofs,0);void*p=malloc(b->l[i].len);
fread(p,1,b->l[i].len,f);return p;}

BSP*loadBSP(const char*path){FILE*f=fopen(path,"rb");BSP*b=malloc(sizeof(BSP));
fread(b,sizeof(BSP),1,f);float*verts=L(b,10,f);int*idx=L(b,11,f);/*...*/return b;}
```

---

## VI. Testing Strategy

### Phase 1: Foundation (Week 1)
**Goal**: Render static BSP geometry

**Test Map**: `maps/q3dm1.bsp` (The Edge - small, iconic)

**Validation**:
- Screenshot comparison with original Q3
- Correct BSP traversal (no geometry clipping)
- Lightmap sampling accuracy

**Success Criteria**:
- BVH builds without crash
- Primary rays hit geometry
- Lightmaps display correctly

### Phase 2: Materials (Week 1)
**Goal**: Texture sampling and shader effects

**Tests**:
- Animated textures (lava, water)
- Blending modes (additive lights)
- Alpha testing (grates)

**Assets**:
- `textures/base_wall/*`
- `textures/liquids/*`
- `scripts/*.shader`

### Phase 3: Player Movement (Week 2)
**Goal**: First-person camera control

**Tests**:
- WASD movement
- Mouse look
- Collision with walls
- Gravity and jumping

**Validation**:
- Record position trace, compare with original Q3 physics
- Verify bounding box collision

### Phase 4: Weapons & Projectiles (Week 2)
**Goal**: Functional combat

**Tests**:
- Rocket launcher (splash damage, trail)
- Railgun (instant hit, beam effect)
- Plasma gun (projectile arcs)

**Models**:
- `models/weapons2/*`
- `models/weaphits/*`

### Phase 5: NPCs & Items (Week 3)
**Goal**: Populated environment

**Tests**:
- Item respawn (armor, health)
- Bot pathfinding
- Animated player models

**Assets**:
- `models/players/sarge/*`
- `models/powerups/*`

### Phase 6: Polish (Week 3)
**Goal**: Game feel

**Features**:
- HUD (health, armor, ammo)
- Motion blur
- Dynamic lights (muzzle flash, explosions)

---

## VII. Build & Development Workflow

### Build Command

```bash
#!/bin/bash
gcc -std=c99 -O3 -march=native -flto -ffast-math \
    quake3rt.c -o q3rt \
    -lSDL2 -lvulkan -lm \
    -DVK_NO_PROTOTYPES \
    -DNDEBUG
```

### Shader Compilation

```bash
glslangValidator --target-env vulkan1.3 \
                 -V rt_core.glsl -o rt_core.spv
# Repeat for all shaders
```

### Test Automation

```bash
#!/bin/bash
./q3rt --map q3dm1 --screenshot test1.tga --quit-after 1s
compare test1.tga reference/q3dm1_ref.tga -metric AE diff.png
if [ $? -gt 100 ]; then echo "FAIL: Too many different pixels"; fi
```

### Screenshot Validation

```bash
# Automated testing positions
./q3rt --map q3dm1 --setpos "0 0 64" --setang "0 90 0" \
       --screenshot pos1.tga --quit-after 1
```

---

## VIII. Asset Acquisition for Testing

### Free/Legal Options

1. **OpenArena** (GPL)
   - Full open-source Q3 replacement
   - Download: http://www.openarena.ws/
   - Assets: `~/.openarena/baseoa/`

2. **Quake 3 Demo** (id Software)
   - Official demo version
   - Maps: q3dm1 (The Edge)
   - Download archived from id Software FTP

3. **Retail Quake 3 Arena**
   - If user owns copy
   - pak0.pk3 contains core assets
   - Extract with zip utility

### Asset Organization

```
assets/
├── maps/
│   └── q3dm1.bsp          # Test map
├── textures/              # Unpacked from BSP
├── models/
│   ├── players/sarge/     # Test player model
│   └── weapons2/          # Weapon models
└── scripts/
    └── *.shader           # Material definitions
```

---

## IX. Performance Targets

### Hardware Requirements

- **GPU**: RTX 2060 or RX 6600 (minimal RT cores)
- **VRAM**: 4 GB
- **CPU**: Any modern quad-core (minimal driver overhead)

### Framerate Goals

- **1080p**: 144 FPS (competitive)
- **1440p**: 90 FPS
- **4K**: 60 FPS

### Optimizations

1. **Shader Occupancy**: Minimize register pressure (< 32 VGPRs per thread)
2. **BVH Quality**: Balanced SAH build (< 5ms)
3. **Async Compute**: Physics on async queue during RT
4. **Temporal Reuse**: Reprojection for 4-8x effective sample count

---

## X. Implementation Phases

### Phase 1: Vulkan Bootstrap (3 days)
- [x] SDL2 window creation
- [ ] Vulkan instance, device selection
- [ ] Swapchain setup
- [ ] Command buffer management
- [ ] Shader loading utility

### Phase 2: BSP Loader (4 days)
- [ ] Parse BSP header and lumps
- [ ] Load vertices, indices
- [ ] Parse entities (spawn points)
- [ ] Lightmap extraction
- [ ] Build BLAS per leaf

### Phase 3: Texture System (3 days)
- [ ] TGA/JPEG loader (stb_image)
- [ ] BC1/BC3 compression
- [ ] Atlas packing
- [ ] Shader script parser
- [ ] Material parameter encoding

### Phase 4: Raytracing Pipeline (5 days)
- [ ] RT pipeline creation
- [ ] Shader binding table
- [ ] TLAS/BLAS management
- [ ] Primary ray generation
- [ ] Hit shader (lighting + materials)

### Phase 5: First Render (2 days)
- [ ] Load q3dm1.bsp
- [ ] Build acceleration structures
- [ ] Dispatch rays
- [ ] **Screenshot validation**

### Phase 6: Player Controller (4 days)
- [ ] Input processing
- [ ] Camera matrices
- [ ] Movement compute shader
- [ ] Collision detection
- [ ] **WASD + mouse test**

### Phase 7: Weapons (5 days)
- [ ] MD3 loader
- [ ] Weapon models rendering
- [ ] Projectile simulation
- [ ] Hit detection
- [ ] **Fire rocket, verify trajectory**

### Phase 8: Game Logic (6 days)
- [ ] Item spawning
- [ ] Pickup detection
- [ ] Health/armor/ammo
- [ ] Respawn system
- [ ] **Full gameplay loop**

### Phase 9: NPCs (5 days)
- [ ] Bot AI skeleton
- [ ] Pathfinding (BSP areas)
- [ ] Weapon selection
- [ ] Animated models
- [ ] **1v1 deathmatch test**

### Phase 10: Polish (5 days)
- [ ] HUD rendering
- [ ] Post-processing
- [ ] Sound integration
- [ ] Menu system
- [ ] **Playable demo**

**Total Estimated Time**: 42 days (6 weeks)

---

## XI. Known Challenges & Mitigations

### Challenge 1: Bezier Patch Tessellation
**Issue**: BSP contains curved surfaces as control points
**Solution**: Pre-tessellate on load, store as triangle soup in BLAS

### Challenge 2: Shader Script Complexity
**Issue**: Q3 shaders have Turing-complete features (nested stages)
**Solution**: Subset support - common cases only (90% coverage)

### Challenge 3: BSP to BVH Conversion
**Issue**: BSP optimized for PVS, not ray traversal
**Solution**: Build separate BVH per leaf, use BSP for broad-phase

### Challenge 4: Code Size Limit
**Issue**: 10K LOC is aggressive for full engine
**Solution**: Heavy use of macros, table-driven logic, math lib reuse

---

## XII. Literate Programming Documentation

### External Documentation Structure

```
docs/
├── 00_architecture.tex    # LaTeX literate source
├── 01_vulkan_init.tex
├── 02_bsp_loader.tex
├── 03_raytracing.tex
├── 04_physics.tex
├── 05_game_logic.tex
└── quake3rt.pdf           # Compiled TeX → PDF
```

### TeX Style

```latex
\section{BSP Loading}
The Quake 3 BSP format stores map data in 17 lumps:

\begin{verbatim}
typedef struct{int ofs,len;}Lump;
typedef struct{char sig[4];int ver;Lump l[17];}BSP;
\end{verbatim}

Lump 10 contains vertices, lump 11 indices. We load:
```

---

## XIII. Success Metrics

### Minimum Viable Product (MVP)
- [ ] Loads q3dm1.bsp
- [ ] Renders geometry with textures
- [ ] Player can walk around
- [ ] Fire rocket launcher
- [ ] 60 FPS @ 1080p on RTX 2060

### Full Release
- [ ] 10+ maps supported
- [ ] All weapons functional
- [ ] Bot AI competitive
- [ ] Multiplayer networking (stretch)
- [ ] Tool to convert any Q3 map

---

## XIV. Risk Assessment

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Vulkan RT driver bugs | High | Medium | Fallback compute path |
| BSP parsing errors | High | Low | Validate against ioquake3 |
| Performance below 60 FPS | Medium | Medium | Reduce resolution, effects |
| Code size exceeds 10K LOC | Low | High | Acceptable overage to 15K |
| Asset copyright issues | High | Low | Use OpenArena assets |

---

## XV. Future Enhancements (Post-MVP)

1. **Path Tracing Mode**: Full global illumination (offline rendering)
2. **Map Editor Integration**: Live reload during mapping
3. **Mod Support**: Load custom game logic shaders
4. **VR Mode**: Stereo rendering, motion controllers
5. **Network Play**: Lockstep deterministic simulation
6. **Mobile Port**: Vulkan + Android NDK

---

## XVI. Conclusion

This design balances the contradictory requirements of "code golf" density and "literate programming" clarity by separating code (terse, optimized) from documentation (comprehensive, pedagogical). The single-file architecture forces excellent API design, while shader-based game logic enables massively parallel execution.

The phased implementation with screenshot validation ensures incremental progress toward a playable demo, with each milestone independently verifiable.

**Next Steps**:
1. Review and approve design
2. Set up development environment (Vulkan SDK, SDL2)
3. Begin Phase 1 implementation
4. Daily screenshot comparisons with reference

---

**Document Version**: 1.0
**Last Updated**: 2026-01-13
**Author**: Claude (Anthropic)
**License**: MIT (code), CC-BY-4.0 (documentation)
