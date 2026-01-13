# Quake 3 Raytracing Clone - Design Document
## Project Vision

A single-file raytracing Quake 3 engine that reads and plays authentic Q3A assets, maximizing shader-side computation with code-golf'd C++ driver code, documented in Knuth's literate programming style.

## Design Philosophy

### Code Structure
- **Single File**: All C++ code in one file (`q3rt.cpp`)
- **Shader Centric**: Maximum logic in GLSL (raytracing, collision, entity updates)
- **Code Golf**: Minimal characters, no comments in implementation
- **Literate Documentation**: Separate comprehensive documentation explaining algorithms
- **Haskell Style**: Pure functions, immutability where possible, composition

### Technical Approach
- **Raytracing**: GPU raytracing via compute shaders, no rasterization
- **Asset Faithful**: Parse real Q3A BSP, MD3, TGA, shader files
- **Incremental**: Build and test in blocks with screenshots

## Architecture

### File Structure
```
q3rt.cpp              - Single file driver (1000-2000 lines, code-golfed)
rt_main.glsl          - Main raytracing compute shader
rt_bsp.glsl           - BSP traversal and collision
rt_entity.glsl        - Entity rendering and animation
rt_physics.glsl       - Game physics simulation
rt_common.glsl        - Shared utilities and data structures
```

### Data Flow
```
BSP/Assets (disk) → Parser → GPU Buffers → Compute Shader → Raytraced Frame
                                    ↓
                              Game State Buffer
                                    ↓
                            Physics/Logic Shader
```

## Quake 3 Asset Formats

### BSP Format (maps/*.bsp)
**Header**: 17 lumps containing offsets/lengths
- `LUMP_ENTITIES`: Entity definitions (text)
- `LUMP_SHADERS`: Material definitions
- `LUMP_PLANES`: Plane equations for BSP
- `LUMP_NODES`: BSP tree nodes
- `LUMP_LEAFS`: BSP tree leaves
- `LUMP_LEAFSURFACES`: Surface indices per leaf
- `LUMP_DRAWVERTS`: Vertex data (position, UV, normal, color)
- `LUMP_DRAWINDEXES`: Triangle indices
- `LUMP_SURFACES`: Surface metadata (type, vertex range, lightmap info)
- `LUMP_LIGHTMAPS`: Precomputed lighting (128x128 RGB)
- `LUMP_LIGHTGRID`: Dynamic lighting grid
- `LUMP_VISIBILITY`: PVS (Potentially Visible Set) data
- `LUMP_BRUSHES/BRUSHSIDES`: Collision geometry

**Key Structures**:
```c
struct dheader_t { int ident, version; lump_t lumps[17]; }
struct dsurface_t { int shader,fog,type,firstVert,numVerts,firstIdx,numIdx,lightmapNum,lightmapX,Y,W,H; vec3 lmOrigin,lmVecs[3]; int patchW,H; }
struct drawVert_t { vec3 xyz; vec2 st,lightmap; vec3 normal; u8 color[4]; }
struct dnode_t { int plane,children[2],mins[3],maxs[3]; }
struct dleaf_t { int cluster,area,mins[3],maxs[3],firstSurf,numSurf,firstBrush,numBrush; }
```

### MD3 Format (models/*.md3)
- Hierarchical skeletal animation
- Multiple LOD levels
- Triangular mesh per frame
- Tag system for attachment points

### TGA Format (textures/*.tga)
- Uncompressed 24/32-bit RGB/RGBA
- Simple header-based format
- Already converted from JPEG

### Shader Scripts (scripts/*.shader)
- Material definitions
- Animation, blending, deformation
- Text-based parsing

## GPU Memory Layout

### Storage Buffers
1. **BSP Geometry Buffer** (SSBO 0)
   - Vertices (xyz, st, lightmap, normal)
   - Indices
   - Surface metadata

2. **BSP Acceleration Buffer** (SSBO 1)
   - Nodes (plane, children, bounds)
   - Leaves (cluster, bounds, surface list)
   - Planes (normal, dist)

3. **Texture Atlas** (Texture 2D Array)
   - All TGA textures packed
   - Mipmapped

4. **Lightmap Atlas** (Texture 2D)
   - All lightmaps packed into large texture

5. **Entity State Buffer** (SSBO 2)
   - Position, rotation, velocity
   - Animation frame
   - Type, health, etc.

6. **Game State Buffer** (SSBO 3)
   - Player state
   - Input state
   - Score, time, etc.

## Raytracing Algorithm

### Primary Rays
```
For each pixel:
  1. Generate ray from camera
  2. Traverse BSP tree
  3. Find nearest intersection
  4. Fetch surface material/lightmap
  5. Compute lighting
  6. Optional: cast shadow rays
  7. Write to framebuffer
```

### BSP Traversal
```
fn traverse_bsp(ray, origin):
  node = root_node
  while node is not leaf:
    plane = planes[node.plane]
    dist = dot(ray.origin, plane.normal) - plane.dist
    side = dist > 0 ? 0 : 1
    node = nodes[node.children[side]]

  for surf_idx in leaf.surfaces:
    test_triangle_intersection(ray, surf_idx)

  return closest_hit
```

### Triangle Intersection (Möller-Trumbore)
```glsl
bool intersect_triangle(ray r, vec3 v0, vec3 v1, vec3 v2, out float t, out vec2 uv) {
  vec3 e1=v1-v0,e2=v2-v0,h=cross(r.dir,e2);float a=dot(e1,h);
  if(abs(a)<1e-6)return false;float f=1./a;vec3 s=r.origin-v0;
  float u=f*dot(s,h);if(u<0.||u>1.)return false;vec3 q=cross(s,e1);
  float v=f*dot(r.dir,q);if(v<0.||u+v>1.)return false;
  t=f*dot(e2,q);uv=vec2(u,v);return t>1e-6;
}
```

## Implementation Phases

### Phase 1: Minimal Viable Renderer (MVP)
**Goal**: Display a single BSP polygon with solid color

**Driver Code**:
- SDL2 window + OpenGL 4.5 context
- BSP header parser
- Load LUMP_DRAWVERTS, LUMP_DRAWINDEXES, LUMP_SURFACES
- Upload to GPU buffers
- Dispatch compute shader

**Shader Code**:
- Basic ray generation
- Single triangle intersection
- Flat color output

**Test**: Load `aggressor.bsp`, render first surface
**Screenshot**: Solid colored polygon visible

### Phase 2: Full BSP Geometry
**Goal**: Render complete level geometry (no textures)

**Additions**:
- BSP node/leaf traversal
- All surfaces rendering
- Debug visualization (wireframe, normals)

**Test**: Load `aggressor.bsp`, show full level
**Screenshot**: Wireframe view of complete level

### Phase 3: Texture Mapping
**Goal**: Apply diffuse textures

**Additions**:
- TGA loader
- Texture atlas packing
- UV coordinate interpolation
- Bilinear filtering

**Test**: Render textured level
**Screenshot**: Fully textured environment

### Phase 4: Lightmaps
**Goal**: Baked lighting

**Additions**:
- Lightmap loading from BSP
- Lightmap atlas
- Dual UV coordinates
- Lightmap * texture multiplication

**Test**: Full lighting fidelity
**Screenshot**: Properly lit scene

### Phase 5: Camera & Collision
**Goal**: Player movement with collision

**Additions**:
- Camera controller (WASD + mouse)
- Physics simulation in shader
- Brush-based collision
- Gravity, jumping

**Test**: Walk through level
**Screenshot**: First-person view from various angles

### Phase 6: Entities
**Goal**: Items, weapons, enemies

**Additions**:
- Entity parsing from BSP entities lump
- MD3 loader
- Entity raytracing
- Basic AI (move toward player)

**Test**: Spawn items and enemies
**Screenshot**: Visible entities in scene

### Phase 7: Game Logic
**Goal**: Playable game

**Additions**:
- Weapon firing
- Damage system
- Pickups (health, armor, ammo)
- HUD rendering

**Test**: Complete gameplay loop
**Screenshot**: HUD showing health/ammo while shooting

## Code Style Examples

### Driver Code (Haskell Style, Code Golf'd)
```cpp
// Literate: Load BSP lump into GPU buffer
// Implementation:
auto L=[](auto&f,int i,auto&b){lump_t l;f.seekg(8+i*8);f.read((char*)&l,8);
std::vector<char>d(l.len);f.seekg(l.ofs);f.read(&d[0],l.len);glBufferData(
GL_SHADER_STORAGE_BUFFER,l.len,&d[0],GL_STATIC_DRAW);return l.len;};
```

### Shader Code (Commented Separately)
```glsl
// Literate: BSP traversal - descend tree based on ray position relative to splitting planes
// Implementation:
int T(vec3 o){int n=0;while(n>=0){vec4 p=P[N[n].p];n=N[n].c[dot(o,p.xyz)>p.w?0:1];}
return~n;}
```

## Testing Strategy

### Automated Tests
1. BSP parser correctness (compare vertex counts with reference)
2. Ray-triangle intersection (unit tests)
3. BSP traversal (verify leaf finding)

### Visual Tests (Screenshots)
Each phase produces a screenshot demonstrating progress:
- Phase 1: Single polygon
- Phase 2: Full wireframe
- Phase 3: Textured geometry
- Phase 4: With lighting
- Phase 5: From player perspective
- Phase 6: With entities
- Phase 7: Gameplay

### Performance Targets
- 1920x1080 @ 60 FPS on RTX 3060
- Primary rays only: 120 FPS
- With shadows: 60 FPS
- Full path tracing: 30 FPS

## Asset Acquisition

### Using Repository Assets
- Maps: `/home/user/Quake3/assets/maps/*.bsp`
- Textures: `/home/user/Quake3/assets/textures/**/*.tga`
- Models: `/home/user/Quake3/assets/models/**/*.md3`
- Sounds: `/home/user/Quake3/assets/sound/**/*.wav`

### Test Assets
Start with smallest map for rapid iteration:
- `aggressor.bsp` (839KB)
- Essential textures only
- No models initially

## Build System

### Compilation
```bash
g++ -std=c++20 -O3 -march=native q3rt.cpp -lSDL2 -lGL -o q3rt
```

### Shader Compilation
- Runtime compilation via `glCompileShader`
- Error checking with detailed logs
- Shader includes via string concatenation

## Development Workflow

1. **Design** → Document literate description
2. **Implement** → Code golf implementation
3. **Test** → Verify with screenshot
4. **Iterate** → Fix bugs, optimize

Each phase produces:
- Literate documentation (markdown)
- Implementation (code)
- Test output (screenshot)
- Performance metrics (FPS, memory)

## Open Questions

1. **Curve Tessellation**: Use compute shader or pre-tessellate?
   - Decision: Pre-tessellate during load (simpler)

2. **Shadow Quality**: Ray-traced or shadowmaps?
   - Decision: Ray-traced (fits raytracer design)

3. **Transparency**: Sorted or order-independent?
   - Decision: Sorted for now (Q3A has limited transparency)

4. **Animation**: CPU or GPU?
   - Decision: GPU (skeletal math in shader)

## References

- Quake 3 Source: `/home/user/Quake3/reference/`
- BSP Format: `reference/qfiles.h`
- BSP Loading: `reference/cm_load.c`, `reference/tr_bsp.c`
- Collision: `reference/cm_trace.c`
- MD3: `reference/tr_model.c`
- Raytracing Shader: `/home/user/Quake3/Raytracing-shaders.glsl`

## Success Criteria

1. ✓ Loads real Q3A assets
2. ✓ Single file + shaders
3. ✓ Functional raytracing
4. ✓ Playable game loop
5. ✓ Code-golfed style
6. ✓ Literate documentation
7. ✓ Performance oriented
8. ✓ Tested incrementally with screenshots

---

*Generated: 2026-01-13*
*Target: 1 file C++20 driver + 5 GLSL shaders*
*Estimated Implementation: 2000-3000 lines C++, 3000-5000 lines GLSL*
