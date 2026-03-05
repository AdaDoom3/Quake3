# Master Plan: Device Scaling for Software Vulkan (lavapipe) and Beyond

## The Problem

Lavapipe (Mesa's software Vulkan implementation) runs the entire Vulkan ray tracing pipeline
on the CPU. Every "device-local" allocation is really host RAM. Every vkQueueSubmit is a
synchronous CPU compute. This creates fundamentally different pressure profiles compared to
discrete GPUs:

| Resource            | Discrete GPU (e.g. RTX 3080) | Lavapipe (software)         |
|---------------------|------------------------------|-----------------------------|
| VRAM                | 8-24 GB dedicated            | Shared with system RAM      |
| Memory allocations  | Fast, GPU-side               | malloc() under the hood     |
| Texture bandwidth   | 600+ GB/s                    | DDR4 ~50 GB/s              |
| RT core throughput  | Hardware BVH traversal       | CPU BVH traversal (slow)    |
| Max practical res   | 4K @ 60fps+                  | 720p @ 10-25 fps            |
| Texture budget      | 4-16 GB                      | ~500 MB practical ceiling   |

### Current Issues (Fixed and Outstanding)

**Fixed:**
- Segfault loading 1024x1024 VTFs: added TEXTURE_MAX_DIM=512 box-filter downscale in VTF_Load
- Missing bounds check on VTF mip0 payload vs file size
- No error handling when GPU_Heap_Alloc or Buffer_Allocate fails during texture upload

**Outstanding:**
- FIGURE_POOL_MAX=64 allocates 3 descriptor arrays x 64 SSBOs each = 192 descriptors for figures
  that are mostly empty (only 2-3 figures used in practice)
- Each figure BLAS + scratch + vertex/index/texid buffers = ~6 Vulkan allocations per figure slot
- TLAS instance buffer sized for 65 instances (1 world + 64 figures) even when only 3 are active
- No adaptive quality: render resolution, SPP, denoise passes, texture quality all hardcoded
- Descriptor pool sized for worst case (1536 textures + 192 figure SSBOs) regardless of device

---

## Architecture: Device Capability Tiers

Rather than scattered `#if LAVAPIPE` checks, define a tiered capability system that adapts
the engine to the detected device at init time. The tier is selected automatically from
Vulkan device properties, or can be overridden via CLI (`--tier 0|1|2|3`).

### Tier Detection (at Vulkan_Pick_Physical_Device time)

```c
typedef enum {
  DEVICE_TIER_SOFTWARE = 0,  // Lavapipe, SwiftShader — CPU-only, no real VRAM
  DEVICE_TIER_INTEGRATED,    // Intel/AMD iGPU — shared memory, limited bandwidth
  DEVICE_TIER_DISCRETE_LOW,  // GTX 1060, RX 580 — 4-6 GB VRAM, basic RT via compute
  DEVICE_TIER_DISCRETE_HIGH  // RTX 3070+, RX 7800+ — 8+ GB VRAM, hardware RT
} Device_Tier;
```

Detection heuristic:
```
if (deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)           → TIER_SOFTWARE
else if (deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) → TIER_INTEGRATED
else if (device_local_heap < 6 GB)                       → TIER_DISCRETE_LOW
else                                                     → TIER_DISCRETE_HIGH
```

### Per-Tier Limits Table

```c
typedef struct {
  Device_Tier Tier;
  const char *Name;
  uint  Max_Texture_Dim;      // Textures downscaled above this (0 = uncapped)
  uint  Max_Textures;         // Descriptor pool texture slot cap
  uint  Max_Figure_Slots;     // FIGURE_POOL_MAX equivalent
  uint  Max_TLAS_Instances;   // 1 + Max_Figure_Slots
  uint  Render_Scale_Percent; // Internal render resolution as % of window
  uint  Default_SPP;          // Samples per pixel for ray tracing
  uint  Default_Denoise;      // Denoise passes (more passes = compensates for low SPP)
  uint  Default_Checkerboard; // 1 = half-res checkerboard tracing (halves RT cost)
  uint  Heap_Slab_MB;         // GPU_Heap default slab size in MB
  uint  Max_Heap_Slabs;       // Maximum number of heap slabs to create
  uint64_t Texture_Budget_MB; // Total texture memory budget (soft limit)
} Device_Tier_Settings;

const Device_Tier_Settings TIER_PRESETS[] = {
  [DEVICE_TIER_SOFTWARE] = {
    .Tier = DEVICE_TIER_SOFTWARE, .Name = "Software (lavapipe/SwiftShader)",
    .Max_Texture_Dim      = 512,
    .Max_Textures         = 512,
    .Max_Figure_Slots     = 8,     // Only 2-3 needed; 8 gives headroom for props
    .Max_TLAS_Instances   = 9,
    .Render_Scale_Percent = 75,
    .Default_SPP          = 1,
    .Default_Denoise      = 3,
    .Default_Checkerboard = 1,
    .Heap_Slab_MB         = 64,
    .Max_Heap_Slabs       = 8,
    .Texture_Budget_MB    = 256,
  },
  [DEVICE_TIER_INTEGRATED] = {
    .Tier = DEVICE_TIER_INTEGRATED, .Name = "Integrated GPU",
    .Max_Texture_Dim      = 1024,
    .Max_Textures         = 1024,
    .Max_Figure_Slots     = 16,
    .Max_TLAS_Instances   = 17,
    .Render_Scale_Percent = 100,
    .Default_SPP          = 1,
    .Default_Denoise      = 2,
    .Default_Checkerboard = 0,
    .Heap_Slab_MB         = 128,
    .Max_Heap_Slabs       = 16,
    .Texture_Budget_MB    = 512,
  },
  [DEVICE_TIER_DISCRETE_LOW] = {
    .Tier = DEVICE_TIER_DISCRETE_LOW, .Name = "Discrete GPU (low VRAM)",
    .Max_Texture_Dim      = 2048,
    .Max_Textures         = 1536,
    .Max_Figure_Slots     = 32,
    .Max_TLAS_Instances   = 33,
    .Render_Scale_Percent = 100,
    .Default_SPP          = 1,
    .Default_Denoise      = 1,
    .Default_Checkerboard = 0,
    .Heap_Slab_MB         = 256,
    .Max_Heap_Slabs       = 24,
    .Texture_Budget_MB    = 2048,
  },
  [DEVICE_TIER_DISCRETE_HIGH] = {
    .Tier = DEVICE_TIER_DISCRETE_HIGH, .Name = "Discrete GPU (high VRAM)",
    .Max_Texture_Dim      = 0,     // Uncapped
    .Max_Textures         = 1536,
    .Max_Figure_Slots     = 64,
    .Max_TLAS_Instances   = 65,
    .Render_Scale_Percent = 100,
    .Default_SPP          = 2,
    .Default_Denoise      = 1,
    .Default_Checkerboard = 0,
    .Heap_Slab_MB         = 256,
    .Max_Heap_Slabs       = 32,
    .Texture_Budget_MB    = 8192,
  },
};
```

---

## Implementation Phases

### Phase 1: Device Tier Detection + Global Settings Struct (~100 lines)

**Goal:** Replace scattered `#define` constants with a runtime-selected tier.

1. Add `Device_Tier` enum and `Device_Tier_Settings` struct to §1 (Settings)
2. Add `Device_Tier_Settings Active_Tier;` global (alongside `Active_World`, `Active_Quality`)
3. In `Vulkan_Pick_Physical_Device`: detect tier from `VkPhysicalDeviceProperties.deviceType`
   and heap sizes, print `[tier] detected: Software (lavapipe/SwiftShader)`
4. Add `--tier N` CLI override
5. Replace `TEXTURE_MAX_DIM` compile-time constant with `Active_Tier.Max_Texture_Dim` runtime value
6. Replace `FIGURE_POOL_MAX` with `Active_Tier.Max_Figure_Slots` where it sizes arrays
7. Replace `DESCRIPTOR_TEXTURE_SLOTS` with `min(Active_Tier.Max_Textures, maxPerStageDescriptorSampledImages)`

**Key constraint:** FIGURE_POOL_MAX is used for static array sizing in Figure_Pool. Either:
- (a) Make Figure_Pool heap-allocated with tier-dependent size, or
- (b) Keep FIGURE_POOL_MAX=64 as the compile-time maximum, but use `Active_Tier.Max_Figure_Slots`
  as the runtime cap. Descriptor arrays and TLAS instances only allocate up to the runtime cap.
  Option (b) is simpler and avoids touching every array reference.

### Phase 2: Texture Memory Budget (~80 lines)

**Goal:** Track cumulative texture memory and stop loading high-res textures when budget is exceeded.

1. Add `uint64_t Texture_Memory_Used;` global (bytes)
2. In `Texture_Upload_With_Format`: accumulate `Width * Height * 4` into `Texture_Memory_Used`
3. Before uploading: if `Texture_Memory_Used + new_size > Active_Tier.Texture_Budget_MB << 20`:
   - If texture is > 1x1: downscale by 2x until it fits, or skip entirely and use 1x1 fallback
   - Log `[tex] budget exceeded (%u MB), downscaling %s from %ux%u to %ux%u`
4. In VTF_Load: use `Active_Tier.Max_Texture_Dim` instead of compile-time `TEXTURE_MAX_DIM`

### Phase 3: Dynamic Figure Pool + TLAS Sizing (~60 lines)

**Goal:** Size descriptor arrays and TLAS instance buffer based on tier, not worst-case 64.

1. `Figure_Pool_Init`: only mark `Active_Tier.Max_Figure_Slots` entries as available
2. `Top_Level_Initialize`: size the instance buffer for `Active_Tier.Max_TLAS_Instances`
3. Descriptor pool creation: allocate `3 * Active_Tier.Max_Figure_Slots` for figure SSBOs
4. `Top_Level_Rebuild`: cap iteration at `Active_Tier.Max_Figure_Slots`

This reduces the lavapipe case from 192 figure SSBOs + 65 TLAS instances to 24 + 9.

### Phase 4: Adaptive Render Quality (~50 lines)

**Goal:** Auto-tune render parameters based on tier and frame timing.

1. Apply tier defaults at init: `Active_Render_Scale`, `Override_SPP`, `Active_Denoise_Passes`,
   `Active_Checkerboard` all read from `Active_Tier`
2. Optional frame-time-based adaptation (future):
   - If avg frame time > 100ms: reduce render scale by 10%
   - If avg frame time < 33ms: increase render scale by 10% (up to 100%)
   - This is the "dynamic resolution" pattern from console engines

### Phase 5: Graceful Degradation on Allocation Failure (~40 lines)

**Goal:** Never crash on OOM — degrade visually instead.

1. `Texture_Upload_With_Format` already returns early on heap/staging failure (done in this commit)
2. Add: when returning early, create a 1x1 magenta fallback texture for the slot
   (currently sets VK_NULL_HANDLE — need to ensure descriptor writes handle this)
3. `Buffer_Allocate`: on heap failure, log and return null buffer (already done)
4. `Figure_BLAS_Initialize`: if buffer allocation fails, skip BLAS creation, mark figure as
   "no BLAS" — `Top_Level_Rebuild` skips figures without valid BLAS handles

### Phase 6: Heap Slab Sizing (~20 lines)

**Goal:** Don't try to allocate 256 MB slabs on a device with limited memory.

1. In `GPU_Heap_Init`: use `Active_Tier.Heap_Slab_MB << 20` as default slab size
2. Cap `Heap.Slab_Count` at `Active_Tier.Max_Heap_Slabs`
3. In `Heap_Slab_Create`: if slab count would exceed max, return -1 (triggers graceful failure)

---

## Lavapipe-Specific Optimizations

### BLAS Rebuild Frequency

Currently every active figure's BLAS is rebuilt every frame. On lavapipe, a BLAS rebuild
for an 8000-triangle weapon model takes ~5ms (CPU BVH construction). With 3 active figures
that's 15ms/frame just for BLAS.

**Optimization:** Only rebuild BLAS when the figure's vertex data actually changed (dirty flag).
Static entities (enemy standing still) skip the rebuild entirely.

```c
// In Figure_Instance:
int BLAS_Dirty;  // Set when Transformed_Vertices are modified (skinning, animation)

// In the frame loop:
if (Fig->BLAS_Dirty) {
  Figure_BLAS_Rebuild (Fig);
  Fig->BLAS_Dirty = 0;
}
```

### TLAS Rebuild Frequency

TLAS is rebuilt every frame even when no transforms changed. On lavapipe TLAS rebuild
for 3 instances is cheap (<1ms), but the pattern of submit-wait-submit-wait for BLAS
then TLAS serializes the CPU pipeline.

**Optimization:** Batch all BLAS rebuilds into one command buffer, then do TLAS build in the
same submission, eliminating per-figure vkQueueWaitIdle round-trips.

### Texture Streaming (Future)

Instead of loading all textures at init (blocking for 10+ seconds on lavapipe with 60+ VTFs):
1. Load 1x1 placeholder textures immediately
2. Background thread loads and decodes VTFs
3. Main thread replaces placeholders with real textures when ready
4. Priority: visible textures first (frustum-based), weapon textures highest priority

---

## Summary Table: What Changes Per Tier

| Feature                   | Software | Integrated | Discrete Low | Discrete High |
|---------------------------|----------|------------|--------------|---------------|
| Texture max dim           | 512      | 1024       | 2048         | Uncapped      |
| Texture budget            | 256 MB   | 512 MB     | 2 GB         | 8 GB          |
| Figure pool slots         | 8        | 16         | 32           | 64            |
| Render scale              | 75%      | 100%       | 100%         | 100%          |
| SPP                       | 1        | 1          | 1            | 2             |
| Denoise passes            | 3        | 2          | 1            | 1             |
| Checkerboard              | Yes      | No         | No           | No            |
| Heap slab size            | 64 MB    | 128 MB     | 256 MB       | 256 MB        |
| BLAS dirty-flag skip      | Yes      | Yes        | Optional     | No            |
| Texture streaming         | Priority | Optional   | Optional     | No            |

---

## Execution Order

1. **Phase 1** — Tier detection (foundation, no behavior change for high-end GPUs)
2. **Phase 2** — Texture budget (prevents OOM on any tier)
3. **Phase 3** — Dynamic figure pool sizing (reduces descriptor waste)
4. **Phase 5** — Graceful degradation (safety net)
5. **Phase 6** — Heap slab sizing (memory management)
6. **Phase 4** — Adaptive quality (polish)

Phases 1-3 are essential. Phases 4-6 are refinements. The BLAS dirty-flag optimization
can be done independently at any point.

**Total: ~350 lines of new code + ~50 lines of existing code changes.**
