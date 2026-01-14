# Quake III Arena - Code Golf Edition

A single-file, code-golfed implementation of the Quake 3 engine written in literate programming style.

## Features

- ✅ **Single file**: 563 lines of C99 code (23KB source)
- ✅ **Complete BSP parser**: Loads authentic Quake 3 .bsp map files
- ✅ **Multi-texture rendering**: TGA texture loading with mipmaps
- ✅ **Lightmap support**: Real Q3 lightmaps for proper lighting/shadows
- ✅ **OpenGL 3.3 shaders**: Modern shader-based pipeline
- ✅ **FPS controls**: WASD movement + mouse look
- ✅ **Zero errors**: 0% sky/missing textures

## Architecture

Written in functional C99 with Haskell-style design principles:
- Pure functions where possible
- Monadic I/O separation
- Type algebra with expressive structures
- Maximum GPU utilization through shaders
- Literate programming documentation

## Build

```bash
gcc -o q3 q3.c -lSDL2 -lGL -lGLEW -lm -O3 -std=c99
```

**Binary size**: 31KB optimized

## Run

```bash
./q3 assets/maps/dm4ish.bsp
```

**Controls**:
- `W/A/S/D` - Movement
- `Mouse` - Look around
- `ESC` - Quit

## Performance

- 60 FPS rendering
- 636 polygon faces (dm4ish map)
- 12+ textures with mipmaps
- Real-time lightmap blending

## Technical Details

**Rendering Pipeline**:
1. BSP file parsing (all 17 lumps)
2. TGA texture decoding (24/32-bit)
3. Vertex/fragment shader compilation
4. Multi-texture + lightmap rendering
5. glDrawElementsBaseVertex for indexed geometry
6. Column-major matrix transforms

**Dependencies**: SDL2, OpenGL 3.3, GLEW

**Code Philosophy**:
> "In the beginning, Carmack created the vertices and the pixels..."

This implementation demonstrates that game engine architecture can be both minimal (code golf) and maximally expressive (literate programming). Every line serves a purpose, documented through chapter-based prose.

## Screenshots

See `final_shot1.png` and `final_shot2.png` for rendered output showing:
- Textured walls and floors
- Lightmap lighting/shadows
- Multiple material types
- Proper 3D perspective

## License

Educational/research implementation. Original Quake 3 Arena © id Software.
