# Refactoring Plan: World Assets & Forward Declarations in q3.c

## Goals

1. **Finish the World system** — complete WORLD_UNREAL, unify asset loading around pak-driven archives with CPU inflate, and introduce an articulated figure concept that subsumes both entity models and weapon models.
2. **Add proper forward declarations** for every function in the specification section.
3. **Add a memory arena allocator** — a simple bump allocator for per-frame and per-load scratch memory, replacing scattered malloc/free pairs.
4. Keep the existing Ada-like naming / labelled-parameter / code-golfed-Haskell-C99 style.

---

## Part A — Memory Arena (§5. Memory)

Add to the specification section a small arena allocator (bump pointer with reset). Two arenas:

| Arena            | Purpose                                              |
|------------------|------------------------------------------------------|
| `Frame_Arena`    | Scratch memory reset every frame (temp vertices etc) |
| `Load_Arena`     | Scratch memory reset after each asset load phase     |

### New types and functions (specification)

```c
typedef struct {
  uint8_t *Base;     // Start of the backing allocation
  uint64_t Used;     // Current high-water byte offset
  uint64_t Capacity; // Total size in bytes
} Arena;

Arena  Arena_Create  (uint64_t Capacity);
void  *Arena_Alloc   (Arena *A, uint64_t Size, uint64_t Align);
void   Arena_Reset   (Arena *A);
void   Arena_Destroy (Arena *A);
```

Implementation: ~20 lines of code-golfed C99 in the §5 implementation block.

---

## Part B — Pak-Driven Asset System (§14. Assets → §6. Assets)

Replace the hard-coded asset path lists with an archive-first loader. The engine reads `.pak` (Quake), `.pk3` (Quake 3 / ZIP), `.vpk` (Source), or `.upk` (Unreal) into memory and inflates entries on the CPU. Loose files on disk are a fallback.

### New types (specification)

```c
// Archive format tag
typedef enum {
  PACK_PK3,   // Quake 3 — ZIP with .pk3 extension
  PACK_PAK,   // Quake 1/2 — flat header + entries
  PACK_VPK,   // Source — Valve Pak
  PACK_UPK,   // Unreal Tournament — Unreal Package (headers only, bulk data separate)
  PACK_COUNT
} Pack_Format;

// One entry inside a loaded archive
typedef struct {
  char     Name[256];   // Virtual path inside the pack (e.g. "models/weapons/rocket.md3")
  uint64_t Offset;      // Byte offset into the archive's raw data
  uint64_t Packed_Size; // Compressed size in bytes (== Size when uncompressed)
  uint64_t Size;        // Uncompressed size in bytes
  int      Compressed;  // Non-zero if deflate-compressed
} Pack_Entry;

// A loaded archive: header parsed, raw data kept in memory
typedef struct {
  Pack_Format  Format;
  char         Path[512];       // Filesystem path to the archive file
  uint8_t     *Data;            // Entire file mapped / read into memory
  uint64_t     Data_Size;       // Size of the mapped data
  Pack_Entry  *Entries;         // Directory of entries (heap-allocated)
  uint         Entry_Count;     // Number of entries in the directory
} Pack_File;

// The virtual filesystem: a stack of pack files searched newest-first, with a loose-file root as fallback
#define PACK_MAX 32
typedef struct {
  Pack_File Packs[PACK_MAX]; // Loaded archives (searched [Count-1] down to [0])
  uint      Pack_Count;
  char      Loose_Root[512]; // Fallback directory for loose files (e.g. "assets/")
  Arena     Scratch;         // Scratch arena for inflate buffers
} Asset_Store;
```

### New functions (specification)

```c
// Mount an archive file (reads entire file into memory, parses directory)
int          Asset_Store_Mount   (Asset_Store *Store, const char *Archive_Path);

// Locate an entry by virtual path (searches packs newest-first, then loose root)
// Returns a pointer to the inflated data (caller must free or use arena) and sets *Out_Size.
uint8_t     *Asset_Load          (Asset_Store *Store, const char *Virtual_Path, uint64_t *Out_Size);

// Check whether a virtual path exists without loading
int          Asset_Exists        (const Asset_Store *Store, const char *Virtual_Path);

// Unmount all packs and free backing memory
void         Asset_Store_Destroy (Asset_Store *Store);

// Low-level inflate (zlib deflate stream → output buffer). Returns bytes written.
uint64_t     Inflate_Buffer      (const uint8_t *In, uint64_t In_Size,
                                  uint8_t *Out, uint64_t Out_Capacity);
```

### Implementation notes

- PK3/ZIP: parse the End-of-Central-Directory → Central Directory → build `Pack_Entry[]`. Inflate via miniz / zlib (already have `reference/unzip.c` to borrow from).
- PAK (Quake 1/2): 12-byte header (`PACK`, offset, size), then 64-byte entries (56-char name + offset + size). No compression — just memcpy.
- VPK (Source): `_dir.vpk` header → entry tree. Bulk data in `_000.vpk` .. `_NNN.vpk`. Parse tree, open bulk files on demand.
- UPK (Unreal): parse the package header + name/export/import tables. Extract raw bulk data by export offset. (Partial support — enough to load meshes and textures.)
- Loose files: `fopen(Loose_Root + Virtual_Path)`.

### Changes to main()

Replace:
```c
const char *Map_Name = DEFAULT_MAP;
```
with:
```c
Asset_Store Assets = {.Loose_Root = ASSET_ROOT};
// --pak <path> mounts an archive before loading begins
```

Add `--pak PATH` CLI flag (repeatable) that calls `Asset_Store_Mount`. All downstream loaders (`Scene_Load_From_BSP`, `Weapon_Model_Load`, texture loaders, audio loaders) receive an `Asset_Store *` and call `Asset_Load` instead of raw `fopen`.

---

## Part C — Finish the World System (§1. Settings)

### C.1 Complete WORLD_UNREAL preset

Fill in the missing `WORLD_PRESETS[WORLD_UNREAL]` entry:

```c
[WORLD_UNREAL] = {WORLD_UNREAL, "Unreal Tournament", 1.f, 78.f, 17.f, 68.f, 48.f, 39.f, 16.f, 90.f, 400.f, 950.f, 2}
```

(UT99 player is ~78 units tall, eye at 68, crouch 48, gravity 950, max run speed 400.)

### C.2 Add asset pack path to World_Settings

Extend `World_Settings` to carry a default asset pack path and default map:

```c
typedef struct {
  World_Type  Type;
  const char *Name;
  float       Unit_Scale, Player_Height, Player_Width;
  float       Eye_Height, Crouch_Eye_Height, Crouch_Height;
  float       Step_Size, FOV, Max_Speed, Gravity;
  int         Up_Axis;
  const char *Default_Pack;  // e.g. "assets/pak0.pk3"
  const char *Default_Map;   // e.g. "oa_dm1.bsp"
} World_Settings;
```

### C.3 Parse `--world unreal` in CLI

Add the unreal branch to the `--world` parser alongside q3 and source.

---

## Part D — Articulated Figures (§7. Models → unified model)

Currently weapon loading (`Weapon_Model_Load`, `Source_Weapon_Model_Load`) and entity loading (`Entity_Load`, `MDL_Load`) are parallel but disconnected pipelines. Unify them under a single **Articulated_Figure** concept:

### New type (specification)

```c
// An articulated figure is a hierarchy of named parts, each with geometry, a skeleton, and
// attachment tags. A weapon is a figure with parts {body, barrel, hand}. A player is a figure
// with parts {head, upper, lower}. A Source MDL is a figure with parts derived from bodygroups.
// This replaces both Weapon_Model and the ad-hoc Entity frame arrays.

#define FIGURE_MAX_PARTS      8
#define FIGURE_MAX_TAGS       16
#define FIGURE_MAX_ANIMS      32
#define FIGURE_MAX_FRAMES     256
#define FIGURE_MAX_BONES      128

// Named attachment point (tag_barrel, tag_weapon, tag_head, etc.)
typedef struct {
  char  Name[64];
  float Transform[12]; // Origin[3] + Axis[9]  (per-frame for animated tags)
} Figure_Tag;

// One geometric part of the figure (e.g. "barrel", "upper_body", "head")
typedef struct {
  char     Name[64];
  Vertex  *Vertices;     uint Vertex_Count;
  uint    *Indices;      uint Index_Count;
  uint    *Texture_Ids;  uint Triangle_Count;
  char     Texture_Names[WEAPON_MAX_TEXTURES][64];
  uint     Surface_Count;
  int      Parent_Tag;   // Index into the figure's tag array (-1 = root)
} Figure_Part;

// Animation clip: a named sequence of keyframes
typedef struct {
  char  Name[64];
  int   First_Frame;  // Index into the figure's frame vertex arrays
  int   Frame_Count;
  float FPS;
  int   Looping;
} Figure_Animation;

// The complete articulated figure
typedef struct {
  Figure_Part       Parts[FIGURE_MAX_PARTS];
  uint              Part_Count;
  Figure_Tag        Tags[FIGURE_MAX_TAGS];
  uint              Tag_Count;
  Figure_Animation  Animations[FIGURE_MAX_ANIMS];
  uint              Animation_Count;

  // Merged geometry (all parts flattened for GPU upload)
  Vertex *Vertices;     uint Vertex_Count;
  uint   *Indices;      uint Index_Count;
  uint   *Texture_Ids;  uint Triangle_Count;

  // Per-frame vertex snapshots (for MD3-style vertex animation)
  Vertex *Frame_Vertices[FIGURE_MAX_FRAMES];
  uint    Total_Frame_Count;

  // Skeletal data (for Source MDL bone-driven animation)
  int         Bone_Count;
  int         Bone_Parents [FIGURE_MAX_BONES];
  float       Bind_Pose    [FIGURE_MAX_BONES][3][4];
  float       Inv_Bind     [FIGURE_MAX_BONES][3][4];
  Bone_Matrix Pose         [FIGURE_MAX_BONES];
  uint8_t    *Bone_Ids;
  uint8_t    *Bone_Weights;

  int         Is_Source;   // 1 = Source MDL skeletal, 0 = MD3 vertex animation
} Articulated_Figure;
```

### Unified loading functions (specification)

```c
// Load an articulated figure from any supported format. Dispatches based on file extension:
//   .md3 → Q3 multi-part assembly (head + upper + lower, or body + barrel + hand)
//   .mdl → Source engine skeletal model
//   .psk → Unreal skeletal mesh (future)
Articulated_Figure Figure_Load (Asset_Store *Store, const char *Path, vec3 Origin, float Yaw);

// Convenience: load a weapon figure (sets up viewmodel transforms, scales, etc.)
Articulated_Figure Figure_Load_Weapon (Asset_Store *Store, const char *Path);
```

### Migration

- `Weapon_Model` fields migrate into `Articulated_Figure` (Tag_Barrel, Tag_Weapon animation frames → `Figure_Tag` arrays + `Figure_Animation` clips)
- `Weapon_Instance.Model` becomes `Weapon_Instance.Figure` (type `Articulated_Figure`)
- `Entity` frame arrays and bone data migrate into `Articulated_Figure`
- `Weapon_Model_Load()` becomes a thin wrapper: `Figure_Load_Weapon(Store, "models/weapons2/machinegun/machinegun.md3")`
- `Entity_Load()` becomes: `Figure_Load(Store, "models/players/sarge/lower.md3", Origin, Yaw)`
- `Source_Weapon_Model_Load(Path)` becomes: `Figure_Load_Weapon(Store, Path)`
- `MDL_Load(S, Path, Origin, Yaw)` becomes: `Figure_Load(Store, Path, Origin, Yaw)`

The old functions become deprecated wrappers that delegate to the new ones, then are removed.

---

## Part E — Complete Forward Declarations (specification section)

The following functions are defined in the implementation but **missing** from the specification:

| Function                        | Defined at | Section to declare in |
|---------------------------------|------------|----------------------|
| `Identity`                      | ~3349      | §4. Math             |
| `Mat34_Mul`                     | ~3356      | §4. Math             |
| `Mat4_Mul`                      | ~3420      | §4. Math             |
| `Tag_Compose`                   | ~4111      | §7. Models           |
| `VTF_Bpp`                       | ~4148      | §6. Materials        |
| `DXT1_Decode_Block`             | ~4167      | §6. Materials        |
| `DXT5_Decode_Alpha`             | ~4187      | §6. Materials        |
| `Entity_Assemble_Frame`         | ~4555      | §7. Models           |
| `Entity_Load`                   | ~4678      | §8. Scene            |
| `Source_Weapon_Model_Load`      | ~5162      | §7. Models           |
| `Classify_Entity`               | ~6639      | §8. Scene            |
| `Entity_Bottom_Level_Initialize`| ~7900      | §9. Accel Structs    |
| `Entity_Bottom_Level_Rebuild`   | ~8015      | §9. Accel Structs    |
| `Postprocess_Pipeline_Create`   | ~8264      | §11. Pipeline        |
| `Quickhull_Dist`                | ~9125      | §10. Physics         |
| `Audio_Generate_Modal_Impact`   | ~9743      | §13. Audio           |
| `Audio_Load_WAV_Or_Modal`       | ~9979      | §13. Audio (has typo `ffloat` on line 1872) |
| `Skinning_Pipeline_Create`      | ~11899     | §11. Pipeline        |

Each gets a one-line forward declaration with a brief doc comment, matching the existing style.

Also fix the `ffloat` typo on line 1872.

---

## Execution Order

1. **E** — Forward declarations (safest, no behavior change)
2. **A** — Arena allocator (new code, no existing code changes)
3. **B** — Asset_Store pak system (new code + rewire loaders)
4. **C** — Complete WORLD_UNREAL (small additions to settings + CLI)
5. **D** — Articulated_Figure unification (largest refactor, builds on B)

---

## Style Notes

- Full Ada-like names: `Asset_Store_Mount`, `Articulated_Figure`, `Figure_Load_Weapon`
- Labelled parameters in Vulkan calls: `/*device =>*/`
- Code-golfed Haskell-like C99: single-expression function bodies where possible, compound literals, ternary chains, comma operator for sequencing
- No emojis, no markdown in code comments
- `not`, `and`, `or` via `<iso646.h>`
