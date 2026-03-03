# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses [nob.h](https://github.com/tsoding/nob.h) (vendored as `nobuild.h`) — a single-header C build tool. The build script is `sdk.c`.

**Bootstrap (only needed once):**
```sh
cc -o sdk sdk.c
```

**Build:**
```sh
./sdk
```
`sdk` auto-rebuilds itself when `sdk.c` is modified (via `NOB_GO_REBUILD_URSELF`).

**Run:**
```sh
./build/q3
```

**Run with software Vulkan (lavapipe, 16 threads):**
```sh
LP_NUM_THREADS=16 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ./build/q3
```
Always set `LP_NUM_THREADS=16` when using lavapipe — the default single-threaded mode is far too slow for the ray tracing pipeline.

## Architecture

- `sdk.c` — Build script. Extracts inline GLSL shaders from `q3.c`, compiles them to SPIR-V, then compiles the C source.
- `nobuild.h` — The nob build library (v3.2.2). Treat as a vendored dependency; do not modify.
- `q3.c` — Main entry point. Monolithic game engine with inline GLSL shader blocks.
- `assets/` — Game assets: textures, models, maps, sounds, menus, sprites, etc.
- `reference/` — Reference materials.

## nobuild.h Conventions

- `Nob_Cmd` is a dynamic array of strings representing a shell command.
- `nob_cmd_append(&cmd, ...)` appends arguments; `nob_cmd_run(&cmd)` runs and resets the array.
- Functions return `bool`: `true` = success, `false` = failure (the function logs the error itself).
- On POSIX: use `cc`. On MSVC: use `cl`.

## Reference / Comparison

**OpenArena** (the open-source Quake 3 engine) can be launched for visual reference:
```sh
openarena +set g_gametype 0 +set bot_enable 0 +set fraglimit 50 +map oa_dm1
```

**Important** a good test setup is the map `oa_dm1`. We've made a `spawn.cfg` file that helps set the player in the right spawn. The following command will launch the map with the player in a consistent location every time. This way you have
consistent tests.

```sh
openarena +set g_gametype 0 +set bot_enable 0 +set fraglimit 50 +devmap oa_dm1 +set sv_cheats 1 +wait +wait +wait +exec spawn.cfg
```

**Screenshots** (Wayland — X11 doesn't work on this system):
```sh
grim /tmp/screenshot.png          # full screen
grim -g "X,Y WxH" /tmp/crop.png  # region
```
Use `grim` instead of `maim` (which is X11-only).

**Important** Do the tests sequentially. 
1. Launch our game with a timeout that will kill it after a time.
2. take a few timed screenshots.
3. then kill the game process.
4. Launch open arena with the command referenced above. 
5. take a few timed screenshots.
6. then kill the game process.