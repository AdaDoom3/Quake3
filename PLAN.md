# Master Plan: Engine Architecture Overhaul

## Overview

Transform the monolithic single-threaded q3.c into a professional multi-threaded engine with
a CVar system, dedicated thread architecture, and clean init/render/game separation.

---

## Phase 1: CVar System (Foundation — everything else depends on this)

### 1A. CVar Infrastructure

Add a CVar registry to q3.c (not a separate file — keep monolithic style). A CVar is a named,
typed, thread-safe variable that can be read/written from any thread and serialized to config.

```c
// CVar types
typedef enum { CVAR_INT, CVAR_FLOAT, CVAR_STRING, CVAR_ENUM } CVar_Type;
typedef enum { CVAR_ARCHIVE = 1, CVAR_READONLY = 2, CVAR_CHEAT = 4, CVAR_LATCH = 8 } CVar_Flags;

typedef struct {
  const char *Name;         // "r_width", "r_height", "r_quality", etc.
  const char *Help;         // "Window width in pixels"
  CVar_Type   Type;
  CVar_Flags  Flags;        // ARCHIVE = save to config, READONLY = engine-set only, LATCH = apply on restart
  union { int I; float F; char S[256]; } Value;
  union { int I; float F; char S[256]; } Default;
  union { int I; float F; } Min, Max;  // For numeric CVars
  int         Modified;     // Dirty flag — set on write, cleared by consumer
  pthread_mutex_t Lock;     // Thread-safe access (Ada protected type equivalent)
} CVar;
```

Registry: flat array of CVars with string-keyed lookup (hash table or linear scan — we'll have
< 100 CVars, so linear is fine and simpler).

```c
#define MAX_CVARS 128
CVar  CVar_Registry[MAX_CVARS];
int   CVar_Count;

// API
CVar *CVar_Register (const char *Name, const char *Help, CVar_Type Type, CVar_Flags Flags, ...);
CVar *CVar_Find     (const char *Name);
int   CVar_Get_Int  (CVar *V);
float CVar_Get_Float(CVar *V);
void  CVar_Set_Int  (CVar *V, int Val);
void  CVar_Set_Float(CVar *V, float Val);
void  CVar_Set_Str  (CVar *V, const char *Val);
void  CVar_Save     (const char *Path);  // Write all ARCHIVE CVars to config file
void  CVar_Load     (const char *Path);  // Read config file and apply values
```

### 1B. Convert Globals to CVars

Replace these globals/defines with CVar registrations at init time:

**Video CVars (r_ prefix):**
- `r_width` (int, ARCHIVE) — replaces `Width` global (line 485)
- `r_height` (int, ARCHIVE) — replaces `Height` global (line 486)
- `r_quality` (int/enum, ARCHIVE) — replaces `Active_Quality` (line 382)
- `r_render_scale` (float, ARCHIVE) — replaces `Active_Render_Scale` (line 487)
- `r_spp` (int, ARCHIVE) — replaces `Override_SPP` (line 2524)
- `r_denoise_passes` (int, ARCHIVE) — replaces `Active_Denoise_Passes` (line 488)
- `r_checkerboard` (int, ARCHIVE) — replaces `Active_Checkerboard` (line 489)
- `r_postprocess` (int, ARCHIVE) — replaces `Skip_Postprocess` (line 407), inverted
- `r_parallax` (int, ARCHIVE) — replaces `No_Parallax` flag, inverted
- `r_pbr` (int, ARCHIVE) — replaces `No_PBR` / `PBR_Stride` logic
- `r_validation` (int, 0) — replaces `Use_Validation` (line 408)
- `r_fullscreen` (int, ARCHIVE) — replaces `Current_Window_Mode` (line 386)
- `r_exposure` (float, ARCHIVE) — replaces `Active_Exposure` (line 542)
- `r_fov` (float, ARCHIVE) — vertical FOV override

**World CVars (w_ prefix):**
- `w_preset` (int/enum, ARCHIVE) — replaces `Active_World` / `Active_Movement` (lines 378-379)
- `w_gravity` (float) — replaces `GRAVITY` define
- `w_max_speed` (float) — replaces `MAXIMUM_SPEED` define
- `w_jump_velocity` (float) — replaces `JUMP_VELOCITY` define
- `w_friction` (float) — replaces `GROUND_FRICTION` define
- `w_accelerate` (float) — replaces `GROUND_ACCELERATE` define
- `w_air_accelerate` (float) — replaces `AIR_ACCELERATE` define

**Audio CVars (a_ prefix):**
- `a_volume` (float, ARCHIVE) — master volume
- `a_enabled` (int, ARCHIVE) — audio on/off

**Input CVars (in_ prefix):**
- `in_sensitivity` (float, ARCHIVE) — mouse sensitivity
- `in_invert_y` (int, ARCHIVE) — invert mouse Y

**Shader Tuning CVars (rt_ prefix) — the 26 SHADER_TUNING defines:**
- `rt_vndf_alpha_floor`, `rt_specular_d_bias`, `rt_refl_clamp_lo`, etc.
- These are ARCHIVE | LATCH (need shader recompile to take effect)

### 1C. Config File Load/Save

Simple key-value format:

```
r_width 1280
r_height 720
r_quality 2
r_render_scale 0.75
w_gravity 800
```

Load at startup before Vulkan init. Save on clean exit and on settings change.

---

## Phase 2: Thread Architecture

### 2A. Thread Model

Four threads, each with a regulated sleep timer:

```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│  Main Thread  │  │ Render Thread │  │  Game Thread  │  │ Input Thread  │
│              │  │              │  │              │  │              │
│ - SDL window │  │ - Vulkan cmds │  │ - Physics     │  │ - SDL events  │
│ - Init/shutdown│ │ - TLAS rebuild│  │ - Animations  │  │ - Mouse/KB    │
│ - Orchestrate │  │ - Submit/pres │  │ - AI/entities │  │ - 500Hz poll  │
│              │  │ - 60-144Hz   │  │ - 60Hz fixed  │  │              │
└──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘
        │                │                 │                 │
        └────────────────┴─────────────────┴─────────────────┘
                    Communicate via CVars + shared ring buffers
```

**Main Thread** — Startup, shutdown, orchestration. Spawns others then waits for quit.

**Input Thread** (500 Hz target):
- Owns SDL_PollEvent exclusively (SDL requirement: events on the thread that created the window)
- NOTE: SDL requires event polling on the main thread on some platforms. If so, input stays on main.
- Writes raw input to a lock-free ring buffer or atomic input struct
- Sleep: `usleep(2000)` → 500 Hz

**Game Thread** (60 Hz fixed timestep):
- Reads input snapshot from ring buffer
- Runs `Physics_Dispatch()` (GPU compute — just records and submits, returns immediately)
- Updates entity animations, weapon viewmodel
- Writes "game state snapshot" (camera, entity transforms, etc.) for render
- Sleep: computes delta from target 16.67ms, sleeps remainder

**Render Thread** (unlocked, targets vsync or CVar-capped):
- Reads latest game state snapshot
- Rebuilds TLAS from entity transforms
- Uploads camera uniform
- Records command buffer (ray trace + denoise + post-process + blit)
- `vkQueueSubmit` + `vkQueuePresentKHR`
- `vkWaitForFences` to pace with GPU
- Sleep: none (GPU-bound) or `usleep` to cap framerate via CVar

### 2B. Synchronization

- **Input → Game**: Lock-free SPSC ring buffer (input thread produces, game thread consumes).
  Or simpler: atomic snapshot struct (input thread writes, game thread reads — single-producer-single-consumer with seq lock).

- **Game → Render**: Double-buffered game state snapshot. Game writes to back buffer, then
  atomically swaps pointer. Render reads from front buffer. `pthread_mutex` protects the swap.

- **CVars**: Each CVar has its own `pthread_mutex_t`. This is the Ada protected type pattern.
  Any thread can Get/Set any CVar safely. For hot-path reads, cache the value per-frame.

- **Quit signal**: Atomic `int Quit` flag, set by input thread on SDL_QUIT, read by all threads.

### 2C. Thread Sleep/Timing

Each thread uses:
```c
void Thread_Sleep_Until (uint64_t Target_Time) {
  uint64_t Now = SDL_GetPerformanceCounter();
  if (Now < Target_Time) {
    uint64_t Remaining_Us = (Target_Time - Now) * 1000000 / SDL_GetPerformanceFrequency();
    if (Remaining_Us > 1000) usleep((unsigned)(Remaining_Us - 500)); // Sleep most of it
    while (SDL_GetPerformanceCounter() < Target_Time) {} // Spin the last ~500us for precision
  }
}
```

This prevents CPU hogging while maintaining timing accuracy.

---

## Phase 3: Render Thread Extraction

### 3A. Extract Rendering from main()

Move the entire Raytracing_Frame() call chain into the render thread:

1. Create `Render_Thread_State` struct holding all Vulkan state the render thread needs
2. Render thread owns: command buffer recording, queue submission, presentation, fence wait
3. Main thread no longer calls `vkWaitForFences` — that's the render thread's pacing mechanism

### 3B. Game State Snapshot

Define a snapshot struct that the game thread produces and render thread consumes:

```c
typedef struct {
  Camera   Cam;
  float    TLAS_Transforms[FIGURE_POOL_MAX][3][4]; // Per-entity transform matrices
  uint8_t  TLAS_Active[FIGURE_POOL_MAX];           // Which entities are active
  int      Budget_Byte;                            // Quality budget for this frame
  float    Delta_Time;                             // For TAA reprojection
  uint64_t Frame;                                  // Monotonic frame counter
  // Post-process constants
  float    Sun_Screen[4];
  float    Exposure;
  int      Denoise_Passes;
  int      Active_SPP;
  int      Checkerboard;
} Game_Snapshot;

// Double-buffered with atomic swap
Game_Snapshot  Snapshot_Buffers[2];
_Atomic int    Snapshot_Read_Index;  // Render reads this
pthread_mutex_t Snapshot_Swap_Lock;
```

### 3C. Swapchain Recreation

Swapchain recreation (resize/fullscreen) requires synchronization between threads:
- Input thread detects resize event, sets `Swapchain_Dirty` CVar
- Render thread checks the flag each frame, recreates swapchain while game thread pauses
- Use a barrier or condition variable to coordinate the pause

---

## Phase 4: Asset Loading Pipeline

### 4A. Async Asset Loading

Move texture loading off the main thread:
- Main thread creates placeholders (1×1 magenta textures)
- Background loader thread reads TGA/VTF files and fills staging buffers
- Render thread picks up staged textures and does the GPU upload + layout transitions

### 4B. Asset Pack System Improvements

The existing `Pack_Format` enum and asset store can be extended:
- Load packs asynchronously during init
- Progressive loading: render with placeholder textures while real ones stream in

---

## Phase 5: Polish and Integration

### 5A. Console System

Add a simple in-engine console (developer tool):
- Toggle with ` (backtick) key
- Submit CVar commands: `r_quality 4`, `r_width 1920`, `w_gravity 400`
- Tab completion from CVar registry
- Command history
- This maps directly to the Ada `Submit()` / `Command` / `CVar` pattern from the reference

### 5B. Quality Presets via CVars

Replace the hardcoded `QUALITY_PRESETS` array. When `r_quality` CVar changes, it sets:
- `r_width`, `r_height`, `r_render_scale`, `r_spp`, `r_denoise_passes`, `r_checkerboard`, `r_parallax`
- This is a one-to-many CVar cascade (changing quality preset updates dependent CVars)

### 5C. Live CVar Hot-Reload

CVars with the `LATCH` flag queue changes for next restart/reload. CVars without LATCH
take effect immediately. The render thread checks `Modified` flags each frame and
responds to changes (e.g., recreate swapchain on `r_width`/`r_height` change).

---

## Implementation Order

```
Phase 1A: CVar struct + registry + Get/Set API              (~200 lines)
Phase 1B: Convert 30+ globals to CVars                       (~150 lines of changes)
Phase 1C: Config file load/save                              (~100 lines)
Phase 2A: Thread creation + sleep timers                     (~100 lines)
Phase 2B: Input ring buffer + game snapshot double buffer    (~150 lines)
Phase 2C: Thread timing infrastructure                       (~50 lines)
Phase 3A: Extract render loop to pthread                     (~200 lines of refactoring)
Phase 3B: Game snapshot population                           (~100 lines)
Phase 3C: Swapchain coordination                             (~50 lines)
Phase 4A: Async texture loading                              (~200 lines)
Phase 5A: In-game console                                    (~300 lines)
Phase 5B: Quality preset CVar cascade                        (~50 lines)
```

**Total: ~1650 lines of new code + significant refactoring of existing code.**

## Key Principles

1. **CVars are the Ada protected type** — mutex-guarded, named, typed, serializable
2. **Threads communicate via CVars and snapshot buffers** — no ad-hoc shared state
3. **Each thread has a sleep timer** — no busy-waiting, no CPU hogging
4. **Render thread owns Vulkan submission** — game thread never touches command buffers
5. **Keep monolithic q3.c** — all code in one file, section-commented, consistent style
6. **SDL threading constraint**: SDL_PollEvent must run on the thread that created the window
   (which is main on macOS, flexible on Linux). Design for main=input on all platforms.
