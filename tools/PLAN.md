# Production Quality GPU Physics Body Implementation Plan

## Problem Analysis

The file `quake3.c` (5307 lines) has accumulated **two complete implementations** of every function:

1. **First Body (lines 840-3502):** Verbose, well-commented, production-quality style with descriptive variable names. This is the target style. However it is:
   - Missing `Convert_BSP_Vertex` and `Bezier_Evaluate` implementations
   - Missing `Shader_Binding_Table_Create` implementation
   - Has a Collision_Map (CM) system woven into `Scene_Load_From_BSP` (GPU does everything, so this is dead code)
   - Does NOT contain the physics section at all

2. **Second Body (lines 3503-5307):** Compact, terse style with short variable names. Contains:
   - **Complete physics section** (types, quickhull, pipeline, dispatch with readback) - the code we need
   - **Complete main() function** - the entry point we need
   - Duplicates of every other function (collision, BSP, textures, accel structs, pipeline, render)
   - Also has Collision_Map code that needs removal

Additionally, the **Specification section (lines 1-839)** has:
   - Forward declarations that reference `Collision_Map` (nonexistent type)
   - Duplicate type definitions (`Gpu_Player`, `Gpu_Input`, `Convex_Hull`, `Gpu_Hull`, `QH_Face`, `QH_Edge`)
   - Duplicate `#define` constants (`HULL_MAX_*`, `SHADER_PATH_*`)
   - Duplicate global variable declarations
   - A broken `VK_CHECK` macro (missing closing brace)
   - Signature mismatches (`Scene_Load_From_BSP` has 2 params in spec vs 3 in body)

## Strategy: Keep First Body Style, Cherry-Pick Second Body Content

We keep the verbose, well-commented first body as our canonical style and integrate the missing pieces from the second body (rewritten in that style).

---

## Phase 1: Fix Specification Section (lines 1-839)

1. **Fix VK_CHECK macro** - Add missing `}` after `exit(1)` to properly close the if-block
2. **Remove duplicate type definitions** - The spec already defines `Gpu_Player`, `Gpu_Input`, `Convex_Hull`, `Gpu_Hull`, `QH_Face`, `QH_Edge`, `Collider_Shape` etc. The body redeclares them; the body copies must go
3. **Remove duplicate `#define` constants** - `HULL_MAX_*`, `SHADER_PATH_*` are defined in both spec and body
4. **Remove duplicate global declarations** - Physics pipeline globals, `Hull_Storage_Buffer` declared in both context and body
5. **Fix `Scene_Load_From_BSP` signature** - Remove `Collision_Map *Out_Collision` parameter (GPU handles everything)
6. **Remove `QH_Dist` forward declaration** - It's `static` in implementation, shouldn't be in the spec
7. **Remove Collision_Map from the specification completely** - No CM types needed, no CM parameter on BSP loader

## Phase 2: Add Missing Function Implementations to First Body

1. **Add `Convert_BSP_Vertex`** - Simple coordinate swizzle function, placed before `BSP_Tessellate_Patch`
2. **Add `Bezier_Evaluate`** - Quadratic Bezier curve evaluation, placed before `BSP_Tessellate_Patch`
3. **Add `Shader_Binding_Table_Create`** - Rewrite from second body in verbose style, placed after `Raytracing_Pipeline_Create`

## Phase 3: Remove Collision_Map from First Body

1. **Remove CM code block** from `Scene_Load_From_BSP` (lines ~2253-2330 approx)
2. **Update function signature** to match the spec (2 params instead of 3)

## Phase 4: Delete Entire Second Body (lines 3503-5307)

Remove all duplicate compact code including:
- Duplicate physics types, hulls, pipeline, dispatch
- Duplicate BSP loading, textures, accel structs, pipeline, render
- Duplicate main function

## Phase 5: Append Clean Physics Section (verbose style)

Rewrite the complete physics section from the second body in the first body's verbose, well-commented style:

1. **Quickhull** (`QH_Dist`, `Quickhull`, `Hull_From_Vertices`, `Hull_Upload`) - already well-written in lines 3580-3796
2. **Physics Pipeline** (`Physics_Pipeline_Create`, `Physics_Resources_Create`) - 5 descriptor bindings including hull
3. **Physics Dispatch** (`Physics_Dispatch`) - Complete with memory barrier and GPU readback

## Phase 6: Append Clean Main Function (verbose style, no CM)

Write main() in the first body's verbose style:
- SDL/Vulkan initialization
- BSP scene loading (no Collision_Map)
- `Weapon.Model = Weapon_Model_Load()` (correct API)
- Acceleration structure construction
- Pipeline + descriptor creation
- Physics pipeline setup
- Game loop with GPU physics dispatch
- Cleanup (no CM freeing)

## Phase 7: Final Fixups

1. Fix `LAYERS` -> `VALIDATION_LAYERS` in `Vulkan_Create_Instance`
2. Ensure `DEFAULT_MAP` constant is used consistently (not `DEFAULT_MAP_PATH`)
3. Remove any remaining references to undefined Collision_Map types
4. Verify section numbering is coherent

---

## Style Congruence Rules (First Body Style)

- **Descriptive variable names**: `Image_Index` not `Img`, `Build_Info` not `Bi`
- **Labeled parameters**: `/*device =>*/`, `/*pCreateInfo =>*/` in Vulkan calls
- **Full section headers**: `// ════...════` with `//   Function_Name` sub-headers
- **Inline comments** explaining non-obvious logic
- **Consistent spacing**: spaces around operators, after commas
- **Named constants**: `SHAPE_CAPSULE` not magic numbers

## Files Modified

- `quake3.c` - All changes in this single file (monolithic architecture)

## Expected Result

A single, coherent, production-quality `quake3.c` with:
- Clean spec/body separation
- No duplicate definitions or implementations
- Complete GPU physics with convex hull support
- No dead Collision_Map code
- Style-congruent verbose formatting throughout
