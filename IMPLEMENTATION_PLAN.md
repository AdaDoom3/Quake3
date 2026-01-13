# Quake 3 Raytracing Engine - Implementation Plan

## Overview

This plan breaks down development into testable blocks with screenshot validation at each milestone. Each phase produces working, demonstrable code before proceeding.

---

## Phase 0: Environment Setup & Proof of Concept
**Duration**: 1 day
**Goal**: Validate toolchain and display test pattern

### Tasks

#### 0.1: Development Environment
- [x] Install Vulkan SDK (verify: `vulkaninfo`)
- [ ] Install SDL2 development headers (`sudo apt-get install libsdl2-dev`)
- [ ] Install GLSL validator (`glslangValidator --version`)
- [ ] Install build essentials (`gcc`, `make`)

#### 0.2: Minimal Vulkan Triangle
**File**: `test_triangle.c` (will be deleted after validation)

```c
// Minimal test: Create window, init Vulkan, render triangle
// Success: 800x600 window with red triangle on black background
```

**Test**:
```bash
gcc test_triangle.c -o test_triangle -lSDL2 -lvulkan -lm
./test_triangle --screenshot triangle.tga
# Manual verification: triangle.tga shows colored triangle
```

**Success Criteria**:
- ✓ Window opens
- ✓ Vulkan device initialized
- ✓ Triangle renders
- ✓ Screenshot saves

#### 0.3: Minimal Compute Shader Test
**File**: `test_compute.c` + `test.comp.glsl`

```glsl
// test.comp.glsl: Write checkerboard to buffer
#version 460
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0, rgba8) uniform image2D img;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    vec4 c = mod(p.x + p.y, 2) == 0 ? vec4(1,0,0,1) : vec4(0,0,1,1);
    imageStore(img, p, c);
}
```

**Test**:
```bash
glslangValidator -V test.comp.glsl -o test.comp.spv
gcc test_compute.c -o test_compute -lSDL2 -lvulkan
./test_compute --screenshot checker.tga
# Verify: Red/blue checkerboard
```

**Success Criteria**:
- ✓ Compute shader dispatches
- ✓ Image written to GPU buffer
- ✓ Screenshot shows checkerboard

---

## Phase 1: BSP Geometry Loading & Display
**Duration**: 3-4 days
**Goal**: Load and display static Q3 map geometry (no textures yet)

### Milestone 1.1: BSP Parser (Day 1)
**File**: `quake3rt.c` (beginning of main file)

#### Code Structure
```c
// BSP format structures
typedef struct{int ofs,len;}Lump;
typedef struct{char sig[4];int ver;Lump l[17];}BSPHdr;
typedef struct{float pos[3];float uv[2][2];float n[3];uint8_t c[4];}BSPVert;
typedef struct{vec3 n;float d;}BSPPlane;

// Loader function
typedef struct{
    BSPVert*verts; uint32_t nVerts;
    uint32_t*idx; uint32_t nIdx;
    float*lightmaps; uint32_t nLM;
    char*entStr;
}BSPData;

BSPData loadBSP(const char*path) { /*...*/ }
```

#### Test Data Extraction
First, we need to extract a test map from the assets:

```bash
# Find and copy test map
find assets/maps -name "*.bsp" | head -1
# Likely result: No BSP files yet, need to obtain

# Option 1: Download OpenArena demo
wget http://openarena.ws/request.php?4 -O openarena-demo.zip
unzip openarena-demo.zip
cp openarena/baseoa/pak0.pk3 assets/
unzip assets/pak0.pk3 'maps/q3dm1.bsp' -d assets/

# Option 2: If user has Q3, extract from pak0.pk3
# (User needs to provide this)
```

#### Unit Test
```c
void test_bsp_load() {
    BSPData d = loadBSP("assets/maps/q3dm1.bsp");
    assert(d.nVerts > 1000); // q3dm1 has ~15k verts
    assert(d.nIdx > 3000);
    printf("✓ Loaded %d verts, %d indices\n", d.nVerts, d.nIdx);
    // Verify first vertex is within map bounds
    assert(fabs(d.verts[0].pos[0]) < 10000);
}
```

**Test Command**:
```bash
gcc -DUNIT_TEST quake3rt.c -o test_bsp -lm
./test_bsp
# Expected output:
# ✓ Loaded 15234 verts, 45678 indices
```

**Success Criteria**:
- ✓ BSP file opens
- ✓ Header parsed (magic "IBSP", version 46)
- ✓ All 17 lumps read
- ✓ Vertex count matches expected (~15k for q3dm1)

### Milestone 1.2: Vulkan Vertex Buffer Upload (Day 2)
**Goal**: Transfer BSP geometry to GPU

```c
typedef struct{VkBuffer b;VkDeviceMemory m;VkDeviceSize sz;}Buf;

Buf uploadGeometry(VkDevice dev, BSPData*bsp) {
    Buf b = allocBuffer(dev, sizeof(BSPVert) * bsp->nVerts,
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    void*map; vkMapMemory(dev, b.m, 0, b.sz, 0, &map);
    memcpy(map, bsp->verts, b.sz);
    vkUnmapMemory(dev, b.m);
    return b;
}
```

**Test**: Print buffer device address
```bash
./quake3rt --test-upload
# Expected:
# ✓ Vertex buffer: 0x7f3a2e000000 (1.2 MB)
# ✓ Index buffer: 0x7f3a2e200000 (0.3 MB)
```

### Milestone 1.3: Wireframe Render (Day 3)
**Files**: `quake3rt.c` + `simple.vert.glsl` + `simple.frag.glsl`

**Vertex Shader** (`simple.vert.glsl`):
```glsl
#version 460
layout(location=0)in vec3 pos;
layout(push_constant)uniform PC{mat4 mvp;};
void main(){gl_Position=mvp*vec4(pos,1);}
```

**Fragment Shader** (`simple.frag.glsl`):
```glsl
#version 460
layout(location=0)out vec4 col;
void main(){col=vec4(1,1,1,1);}
```

**Rendering Code**:
```c
void renderFrame(State*s) {
    mat4 view = lookAt(s->camPos, s->camPos + s->camDir, V3(0,0,1));
    mat4 proj = perspective(90.0f, 16.0f/9.0f, 1.0f, 10000.0f);
    mat4 mvp = proj * view;

    vkCmdPushConstants(s->cmd, s->pipeLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(mat4), &mvp);
    vkCmdBindVertexBuffers(s->cmd, 0, 1, &s->geomBuf.b, &(VkDeviceSize){0});
    vkCmdBindIndexBuffer(s->cmd, s->idxBuf.b, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(s->cmd, s->bsp.nIdx, 1, 0, 0, 0);
}
```

**Test**:
```bash
./quake3rt --map q3dm1 --wireframe --pos "0,0,100" --screenshot wire1.tga
```

**Expected Screenshot**: White wireframe geometry on black background, recognizable as Q3DM1 architecture

**Success Criteria**:
- ✓ Geometry visible
- ✓ Camera movement with WASD
- ✓ No crashes for 30 seconds
- ✓ Framerate > 100 FPS (simple wireframe)

### Milestone 1.4: Flat Shaded (Day 4)
**Goal**: Display geometry with lighting direction only (no textures)

**Modified Fragment Shader**:
```glsl
#version 460
layout(location=0)in vec3 norm;
layout(location=0)out vec4 col;
void main(){
    vec3 L=normalize(vec3(1,1,2));
    float d=max(0,dot(norm,L))*0.8+0.2;
    col=vec4(vec3(d),1);
}
```

**Screenshot Test**:
```bash
./quake3rt --map q3dm1 --shaded --screenshot shaded1.tga
```

**Expected**: Gray-shaded geometry with directional lighting

**Validation**:
- Run reference Q3 in `r_showtris 1` mode
- Compare silhouettes match

**Success Criteria**:
- ✓ Normals correct (lighting makes sense)
- ✓ No black/inverted faces
- ✓ Architecture recognizable as Q3DM1

---

## Phase 2: Texture System
**Duration**: 3 days
**Goal**: Display textured geometry with lightmaps

### Milestone 2.1: TGA Loader (Day 5)
**Task**: Load single texture file

```c
typedef struct{uint8_t*px;int w,h,bpp;}Img;

Img loadTGA(const char*path) {
    FILE*f=fopen(path,"rb");
    uint8_t hdr[18]; fread(hdr,1,18,f);
    int w=hdr[12]|(hdr[13]<<8);
    int h=hdr[14]|(hdr[15]<<8);
    int bpp=hdr[16];
    Img img={malloc(w*h*(bpp/8)),w,h,bpp};
    fread(img.px,1,w*h*(bpp/8),f);
    fclose(f);
    return img;
}
```

**Unit Test**:
```bash
# Find a texture in assets
find assets/textures -name "*.tga" | head -1
# Example: assets/textures/base_wall/concrete01.tga

gcc -DTEST_TGA quake3rt.c -o test_tga
./test_tga assets/textures/base_wall/concrete01.tga
# Expected output:
# ✓ Loaded concrete01.tga: 128×128 (24-bit)
# ✓ First pixel: RGB(142, 138, 130)
```

### Milestone 2.2: Texture Atlas (Day 6)
**Goal**: Pack all textures into single 4096×4096 atlas

```c
typedef struct{int x,y,w,h;}Rect;
typedef struct{Img atlas; Rect*coords; int n;}Atlas;

Atlas packTextures(Img*textures, int count) {
    // Simple scanline packing
    Atlas a = {.atlas = {calloc(4096*4096*4,1), 4096, 4096, 32},
               .coords = malloc(sizeof(Rect)*count)};
    int x=0,y=0,rowH=0;
    for(int i=0;i<count;i++){
        if(x+textures[i].w>4096){x=0;y+=rowH;rowH=0;}
        a.coords[i]=(Rect){x,y,textures[i].w,textures[i].h};
        // Copy texture into atlas at (x,y)
        blit(&a.atlas, x, y, &textures[i]);
        x+=textures[i].w; rowH=max(rowH,textures[i].h);
    }
    return a;
}
```

**Test**:
```bash
./quake3rt --pack-textures assets/textures --output atlas.tga
# Verify: atlas.tga is 4096×4096, contains all textures
```

**Manual Check**: Open `atlas.tga` in image viewer, verify textures visible

### Milestone 2.3: Textured Render (Day 7)
**Goal**: Apply textures to geometry

**Fragment Shader Update**:
```glsl
#version 460
layout(location=0)in vec2 uv;
layout(binding=0)uniform sampler2D atlas;
layout(location=0)out vec4 col;
void main(){
    col=texture(atlas,uv);
}
```

**Screenshot Test**:
```bash
./quake3rt --map q3dm1 --textured --screenshot tex1.tga
```

**Expected**: Fully textured map (may look too bright without lightmaps)

**Comparison**:
- Take screenshot from same position in original Q3
- Overlay in image editor, check texture alignment

**Success Criteria**:
- ✓ Textures applied correctly
- ✓ No UV distortion
- ✓ Texture coordinates match original

---

## Phase 3: Raytracing Pipeline
**Duration**: 4 days
**Goal**: Replace rasterization with ray tracing

### Milestone 3.1: Acceleration Structure Build (Day 8)
**Task**: Convert BSP geometry to bottom-level AS

```c
VkAccelerationStructureKHR buildBLAS(VkDevice dev, Buf geom, uint32_t triCount) {
    VkAccelerationStructureGeometryKHR geo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry.triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = {.deviceAddress = getBufferAddress(dev, geom.b)},
            .vertexStride = sizeof(BSPVert),
            .maxVertex = triCount * 3,
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = {.deviceAddress = /*...*/}
        }
    };
    // Build AS...
}
```

**Test**:
```bash
./quake3rt --build-as --map q3dm1
# Expected output:
# ✓ Built BLAS: 15234 triangles
# ✓ AS size: 8.4 MB
# ✓ Build time: 12ms
```

### Milestone 3.2: Ray Tracing Shaders (Day 9-10)
**Files**: `rt.rgen`, `rt.rchit`, `rt.rmiss`

**Ray Generation** (`rt.rgen`):
```glsl
#version 460
#extension GL_EXT_ray_tracing : require
layout(binding=0,set=0)uniform accelerationStructureEXT tlas;
layout(binding=1,set=0,rgba8)uniform image2D img;
layout(push_constant)uniform PC{mat4 invVP;vec3 camPos;};

void main(){
    vec2 px=vec2(gl_LaunchIDEXT.xy);
    vec2 uv=px/vec2(gl_LaunchSizeEXT.xy)*2-1;
    vec4 target=invVP*vec4(uv,1,1);
    vec3 dir=normalize(target.xyz/target.w-camPos);

    rayQueryEXT rq;
    rayQueryInitializeEXT(rq,tlas,gl_RayFlagsOpaqueEXT,0xFF,
                          camPos,0.01,dir,10000);
    while(rayQueryProceedEXT(rq));

    vec3 col=vec3(0);
    if(rayQueryGetIntersectionTypeEXT(rq,true)==gl_RayQueryCommittedIntersectionTriangleEXT){
        vec2 bary=rayQueryGetIntersectionBarycentricsEXT(rq,true);
        col=vec3(bary,1-bary.x-bary.y); // Debug: show barycentric coords
    }
    imageStore(img,ivec2(gl_LaunchIDEXT.xy),vec4(col,1));
}
```

**Test**:
```bash
./quake3rt --rt --map q3dm1 --screenshot rt_bary.tga
```

**Expected**: Colorful triangles (RGB = barycentric coordinates)

### Milestone 3.3: Texture Sampling in RT (Day 11)
**Goal**: Sample textures in closest-hit shader

**Updated Ray Gen**:
```glsl
// Fetch vertex data
int triIdx = rayQueryGetIntersectionPrimitiveIndexEXT(rq,true);
BSPVert v0 = verts[indices[triIdx*3+0]];
BSPVert v1 = verts[indices[triIdx*3+1]];
BSPVert v2 = verts[indices[triIdx*3+2]];

// Interpolate UV
vec2 uv = v0.uv * (1-bary.x-bary.y) + v1.uv * bary.x + v2.uv * bary.y;
col = texture(atlas, uv).rgb;
```

**Screenshot Test**:
```bash
./quake3rt --rt --map q3dm1 --screenshot rt_tex.tga
```

**Expected**: Textured scene via raytracing

**Comparison**: Should match Phase 2 textured render

---

## Phase 4: Lighting
**Duration**: 2 days
**Goal**: Correct lighting (lightmaps + dynamic)

### Milestone 4.1: Lightmap Extraction (Day 12)
**Task**: Load lightmap lump from BSP

```c
// BSP lump 14 = lightmaps (128×128 RGB, multiple)
uint8_t*loadLightmaps(BSPHdr*hdr, FILE*f, int*count) {
    fseek(f, hdr->l[14].ofs, SEEK_SET);
    int sz = hdr->l[14].len;
    *count = sz / (128*128*3);
    uint8_t*lm = malloc(sz);
    fread(lm, 1, sz, f);
    return lm;
}
```

**Upload to GPU**:
```c
// Create 2D array texture
VkImage lmTex = createImage(dev, 128, 128, *count, VK_FORMAT_R8G8B8_UNORM,
                            VK_IMAGE_VIEW_TYPE_2D_ARRAY);
```

### Milestone 4.2: Lightmap Sampling (Day 13)
**Shader**:
```glsl
layout(binding=2)uniform sampler2DArray lightmaps;
// ...
int lmIdx = v0.lmIdx; // Need to add this to BSPVert
vec2 lmUV = v0.lmUV * (1-bary.x-bary.y) + v1.lmUV * bary.x + v2.lmUV * bary.y;
vec3 lightmap = texture(lightmaps, vec3(lmUV, lmIdx)).rgb;
col = albedo * lightmap;
```

**Screenshot**:
```bash
./quake3rt --rt --lighting --screenshot rt_lit.tga
```

**Expected**: Correctly lit scene matching original Q3

**Validation**:
- Compare with original Q3 screenshot
- Brightness should match (adjust gamma if needed)

---

## Phase 5: Player Movement
**Duration**: 3 days
**Goal**: Interactive first-person camera

### Milestone 5.1: Input Handling (Day 14)
**Code**:
```c
typedef struct{uint8_t w,a,s,d,space,shift;int16_t mx,my;}Input;

Input pollInput(SDL_Window*win) {
    Input inp={0};
    SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_KEYDOWN){
            switch(ev.key.keysym.sym){
                case SDLK_w: inp.w=1; break;
                case SDLK_a: inp.a=1; break;
                // ...
            }
        }
    }
    int mx,my;
    SDL_GetRelativeMouseState(&mx,&my);
    inp.mx=mx; inp.my=my;
    return inp;
}
```

**Test**:
```bash
./quake3rt --test-input
# Expected: Print input state each frame
# W pressed: w=1, a=0, s=0, d=0, mx=-5, my=3
```

### Milestone 5.2: Camera Update (Day 15)
**Physics** (simplified, CPU-side first):
```c
void updateCamera(State*s, Input inp, float dt) {
    // Rotation
    s->camYaw += inp.mx * 0.002f;
    s->camPitch -= inp.my * 0.002f;
    s->camPitch = clamp(s->camPitch, -M_PI/2, M_PI/2);

    vec3 fwd = {cos(s->camYaw)*cos(s->camPitch),
                sin(s->camYaw)*cos(s->camPitch),
                sin(s->camPitch)};
    vec3 right = cross(fwd, (vec3){0,0,1});

    // Movement
    vec3 vel = {0};
    if(inp.w) vel += fwd;
    if(inp.s) vel -= fwd;
    if(inp.a) vel -= right;
    if(inp.d) vel += right;

    s->camPos += normalize(vel) * 320.0f * dt; // Q3 move speed
}
```

**Test**:
```bash
./quake3rt --rt --map q3dm1
# Manually test: WASD movement, mouse look
# Should feel responsive
```

### Milestone 5.3: Collision Detection (Day 16)
**Goal**: Don't walk through walls

**Simplified Collision**:
```c
bool traceLine(BSPData*bsp, vec3 start, vec3 end) {
    // Iterate all triangles (slow, but works)
    for(int i=0;i<bsp->nIdx;i+=3){
        vec3 v0 = bsp->verts[bsp->idx[i+0]].pos;
        vec3 v1 = bsp->verts[bsp->idx[i+1]].pos;
        vec3 v2 = bsp->verts[bsp->idx[i+2]].pos;
        if(rayTriIntersect(start, end-start, v0, v1, v2))
            return true;
    }
    return false;
}

void updateCameraWithCollision(State*s, Input inp, float dt) {
    vec3 newPos = s->camPos + vel * dt;
    if(!traceLine(&s->bsp, s->camPos, newPos))
        s->camPos = newPos;
}
```

**Test**:
```bash
./quake3rt --rt --map q3dm1
# Try to walk through wall - should stop
# Jump off edge - should fall (gravity)
```

**Success Criteria**:
- ✓ Cannot walk through walls
- ✓ Gravity works
- ✓ Can jump (if space pressed)

---

## Phase 6: Weapons & Projectiles
**Duration**: 4 days
**Goal**: Functional rocket launcher

### Milestone 6.1: MD3 Loader (Day 17)
**Code**:
```c
typedef struct{
    vec3*verts; // Per-frame vertex positions
    uint32_t*tris;
    vec2*uvs;
    int nFrames,nVerts,nTris;
}MD3;

MD3 loadMD3(const char*path) {
    FILE*f=fopen(path,"rb");
    // Read header, surfaces, frames...
}
```

**Test**:
```bash
./quake3rt --load-md3 assets/models/weapons2/rocketl/rocketl.md3
# Expected:
# ✓ Loaded rocketl.md3: 8 frames, 245 verts, 156 tris
```

### Milestone 6.2: Weapon Rendering (Day 18)
**Goal**: Show rocket launcher in view

**Code**:
```c
void renderWeapon(State*s) {
    // Position weapon in front of camera
    mat4 weaponMat = translate(s->camPos + s->camFwd * 20 + s->camRight * 10);
    // Add to TLAS as instance
    addInstance(&s->tlas, s->weaponBLAS, weaponMat);
}
```

**Screenshot**:
```bash
./quake3rt --rt --weapon rocketl --screenshot weapon.tga
```

**Expected**: Rocket launcher visible in bottom-right of screen

### Milestone 6.3: Projectile Simulation (Day 19-20)
**Compute Shader** (`projectiles.comp`):
```glsl
#version 460
layout(local_size_x=64)in;

struct Projectile{vec3 pos,vel;uint type,owner;float timeout;};
layout(binding=0)buffer Projectiles{Projectile proj[];};
layout(push_constant)uniform PC{float dt;};

void main(){
    uint i=gl_GlobalInvocationID.x;
    if(proj[i].timeout<=0)return;

    // Update position
    proj[i].pos += proj[i].vel * dt;
    proj[i].vel.z -= 800 * dt; // Gravity
    proj[i].timeout -= dt;

    // TODO: Collision detection
}
```

**Firing**:
```c
void fireRocket(State*s) {
    Projectile p = {
        .pos = s->camPos,
        .vel = s->camFwd * 900.0f, // Q3 rocket speed
        .type = PROJ_ROCKET,
        .timeout = 15.0f
    };
    addProjectile(s, &p);
}
```

**Test**:
```bash
./quake3rt --rt --map q3dm1
# Press mouse button, rocket should fly forward
# Should arc downward due to gravity
```

**Screenshot Sequence**:
```bash
# Fire rocket, take screenshots at 0.1s intervals
./quake3rt --rt --map q3dm1 --auto-fire --screenshot-sequence rocket_%03d.tga
# Creates: rocket_000.tga, rocket_001.tga, ..., rocket_010.tga
# Manual verification: Rocket trajectory looks correct
```

---

## Phase 7: Game Logic
**Duration**: 3 days
**Goal**: Items, pickups, health/armor

### Milestone 7.1: Entity Parsing (Day 21)
**Code**:
```c
typedef struct{vec3 pos;int type,flags;}Entity;

Entity*parseEntities(char*entStr, int*count) {
    // Parse entity string from BSP
    // Example: { "classname" "weapon_rocketlauncher" "origin" "0 0 64" }
    // ...
}
```

**Test**:
```bash
./quake3rt --parse-entities assets/maps/q3dm1.bsp
# Expected output:
# Entity 0: worldspawn
# Entity 1: info_player_deathmatch (0, 0, 64)
# Entity 2: weapon_rocketlauncher (128, 64, 32)
# ...
# Total: 87 entities
```

### Milestone 7.2: Item Rendering (Day 22)
**Goal**: Show items in world

**Code**:
```c
void renderItems(State*s) {
    for(int i=0;i<s->nEnts;i++){
        Entity*e=&s->ents[i];
        if(e->type==ENT_WEAPON_RL){
            // Load rocket launcher pickup model
            MD3*model=getModel("models/weapons2/rocketl/rocketl.md3");
            addInstance(&s->tlas, model->blas, translate(e->pos));
        }
    }
}
```

**Screenshot**:
```bash
./quake3rt --rt --map q3dm1 --items --screenshot items.tga
```

**Expected**: Weapon/item models visible at spawn points

### Milestone 7.3: Pickup System (Day 23)
**Compute Shader**:
```glsl
// Check distance between player and items
for(int i=0;i<numItems;i++){
    if(length(player.pos - items[i].pos) < 32.0){
        // Pickup item
        if(items[i].type == ITEM_ROCKET_AMMO)
            player.ammo[WP_RL] += 10;
        items[i].active = false;
        items[i].respawnTime = currentTime + 30.0;
    }
}
```

**Test**:
```bash
./quake3rt --rt --map q3dm1
# Walk over rocket launcher
# Expected: Ammo count increases, item disappears
# Wait 30 seconds - item respawns
```

---

## Phase 8: Polish & Optimization
**Duration**: 3 days
**Goal**: Playable demo quality

### Milestone 8.1: HUD (Day 24)
**Goal**: Display health, armor, ammo

**Rasterized Overlay**:
```c
// After raytracing, render HUD with traditional rasterization
void renderHUD(State*s) {
    char buf[64];
    sprintf(buf, "Health: %d  Armor: %d  Rockets: %d",
            s->player.health, s->player.armor, s->player.ammo[WP_RL]);
    drawText(s, buf, 10, 10, (vec3){1,1,1});
}
```

**Screenshot**:
```bash
./quake3rt --rt --map q3dm1 --hud --screenshot hud.tga
```

**Expected**: Text overlay showing stats

### Milestone 8.2: Performance Optimization (Day 25)
**Tasks**:
- Profile with `nsight-sys`
- Optimize BVH build (use SAH)
- Reduce shader register usage
- Enable async compute for physics

**Test**:
```bash
nsight-sys profile ./quake3rt --rt --map q3dm1 --benchmark 60s
# Analyze results, identify bottlenecks
# Target: 144 FPS @ 1080p on RTX 2060
```

### Milestone 8.3: Post-Processing (Day 26)
**Effects**:
- Temporal anti-aliasing
- Motion blur
- Tonemapping

**Compute Shader** (`postfx.comp`):
```glsl
vec4 history = texelFetch(prevFrame, reproject(uv, velocity), 0);
vec4 current = texelFetch(currentFrame, uv, 0);
vec4 result = mix(current, history, 0.9); // TAA
result.rgb = tonemap(result.rgb); // ACES
imageStore(output, px, result);
```

**Screenshot**:
```bash
./quake3rt --rt --map q3dm1 --postfx --screenshot final.tga
```

**Expected**: Polished image quality

---

## Testing & Validation Strategy

### Automated Testing
```bash
#!/bin/bash
# test_all.sh - Run all validation tests

TESTS=(
    "q3dm1:0,0,100:0,90,0:phase1_wire.tga"
    "q3dm1:256,128,64:45,0,0:phase2_tex.tga"
    "q3dm1:512,256,128:90,45,0:phase3_rt.tga"
)

for TEST in "${TESTS[@]}"; do
    IFS=':' read -r MAP POS ANG OUT <<< "$TEST"
    ./quake3rt --map "$MAP" --pos "$POS" --ang "$ANG" --screenshot "$OUT"
    compare "$OUT" "reference/$OUT" -metric AE diff_"$OUT"
    if [ $? -gt 50 ]; then
        echo "FAIL: $OUT differs from reference"
        exit 1
    fi
done
echo "✓ All tests passed"
```

### Performance Benchmarks
```bash
# bench.sh
./quake3rt --map q3dm1 --benchmark 10s --json > bench.json
python3 plot_fps.py bench.json  # Generate FPS graph
```

### Screenshot Comparisons
**Reference Images**: Taken from original Quake 3 at specific positions
**Tool**: ImageMagick `compare` command
**Threshold**: < 5% pixel difference (accounting for raytracing differences)

---

## Asset Acquisition Plan

### Option 1: OpenArena (GPL, Free)
```bash
wget http://download.tuxfamily.org/openarena/rel/088/openarena-0.8.8.zip
unzip openarena-0.8.8.zip
cp openarena-0.8.8/baseoa/pak*.pk3 assets/
```

**Maps Available**: 45 original maps (DM, CTF, Tournament)

### Option 2: Quake 3 Demo (Freeware)
```bash
# Download from archive.org or mirror
wget https://archive.org/download/quake3demo/q3ademo.exe
wine q3ademo.exe  # Extract pak0.pk3
cp demoq3/pak0.pk3 assets/quake3-demo.pk3
```

**Maps Available**: q3dm1 (The Edge) only

### Option 3: User-Owned Retail Copy
**Instructions for user**:
```
1. Locate Quake 3 Arena installation
2. Navigate to baseq3/ folder
3. Copy pak0.pk3 to this project's assets/ folder
4. Run: ./quake3rt --map q3dm1
```

---

## Success Metrics

### Minimum Viable Demo (End of Phase 8)
- [ ] Loads and displays q3dm1.bsp with textures and lighting
- [ ] Smooth 60 FPS @ 1080p on mid-range GPU (RTX 2060)
- [ ] Player can move around map with collision
- [ ] Can fire rocket launcher and see projectiles
- [ ] Items spawn and can be picked up
- [ ] HUD displays game state

### Stretch Goals
- [ ] Multiple maps supported
- [ ] Bot AI (simple patrol/combat)
- [ ] All weapons implemented
- [ ] Gibs and particle effects
- [ ] Sound effects
- [ ] Menu system

---

## Timeline Summary

| Phase | Duration | Milestone | Screenshot |
|-------|----------|-----------|------------|
| 0 | 1 day | Triangle + compute test | `triangle.tga`, `checker.tga` |
| 1 | 4 days | BSP geometry loaded | `wire1.tga`, `shaded1.tga` |
| 2 | 3 days | Textured rendering | `tex1.tga` |
| 3 | 4 days | Raytracing pipeline | `rt_tex.tga` |
| 4 | 2 days | Lighting (lightmaps) | `rt_lit.tga` |
| 5 | 3 days | Player movement | (Interactive test) |
| 6 | 4 days | Weapons & projectiles | `weapon.tga`, `rocket_*.tga` |
| 7 | 3 days | Game logic (items) | `items.tga` |
| 8 | 3 days | Polish (HUD, postfx) | `final.tga` |

**Total**: 27 days (~4 weeks)

---

## Daily Workflow

### Morning (2 hours)
1. Review previous day's screenshots
2. Code new feature
3. Unit test in isolation

### Afternoon (3 hours)
1. Integration with main file
2. Build and run
3. Take validation screenshot
4. Compare with reference

### Evening (1 hour)
1. Commit code with screenshot
2. Update progress document
3. Plan next day's tasks

---

## Git Workflow

### Commit Strategy
```bash
# After each milestone
git add quake3rt.c *.glsl screenshots/phase*.tga
git commit -m "Phase 1.3: Wireframe render working

- Implemented Vulkan graphics pipeline
- Uploaded BSP geometry to GPU
- Added camera matrix push constants
- Screenshot: wire1.tga shows Q3DM1 geometry

Test: ./quake3rt --map q3dm1 --wireframe --screenshot wire1.tga
FPS: 340 @ 1080p (RTX 2060)"
```

### Branch Structure
- `main`: Stable, tested milestones
- `claude/quake3-raytracing-engine-XfObc`: Active development (current)
- Tags: `phase-1-complete`, `phase-2-complete`, etc.

---

## Next Steps

1. **Approve this plan** or request modifications
2. **Obtain assets**: Download OpenArena or provide Q3 pak files
3. **Setup environment**: Install Vulkan SDK, SDL2, etc.
4. **Begin Phase 0**: Verify toolchain with triangle test
5. **Daily screenshots**: Post progress with visual proof

**Ready to begin implementation?**
