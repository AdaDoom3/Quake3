# Quake3 Raytracing Engine

A minimal C99 raytracing engine combining concepts from the Shadertoy Quake raytracing shader with code-golfing optimization techniques.

## Overview

This project integrates advanced raytracing concepts from `Raytracing-shaders.glsl` (an 8,294-line WebGL shader recreating Quake's first room) with a minimal C99 OpenGL engine. The implementation uses:

- **C99**: Clean, portable system code
- **GLSL 330**: Modern shader language with code-golfing optimizations
- **OpenGL 3.3 Core**: Hardware-accelerated rendering
- **GLFW3**: Cross-platform window management

## Features

### Raytracing Techniques
- **Signed Distance Fields (SDF)**: Efficient geometric representation
- **Ray Marching**: Real-time ray traversal (64 steps, 50 unit max distance)
- **Ambient Occlusion**: 5-step approximation for realistic shadowing
- **Procedural Textures**: 3D Perlin noise-based materials
- **Smooth Blending**: smin() function for organic geometry merging
- **Fresnel Effect**: Edge lighting for visual depth

### Code-Golfing Optimizations
1. **Compact Identifiers**: Single-letter function names (h, n, map, norm)
2. **Inline Operations**: Minimal variable declarations
3. **GLSL Packing**: Shaders embedded as string literals
4. **Stripped Whitespace**: Minimal formatting in shader code
5. **Function Chaining**: Direct composition without intermediates

### Scene Geometry
The engine renders a Quake-inspired environment:
- Floor and ceiling planes (16x16 units)
- Four surrounding walls
- Repeating pillars using fract() domain repetition
- Torch holders (capsule primitives)
- Smooth union blending between objects

## Building

### Prerequisites
```bash
sudo apt-get install libglfw3-dev libgl1-mesa-dev xvfb
```

### Compilation
```bash
make                    # Build interactive viewer
make screenshots        # Build and capture screenshots
```

### Running
```bash
./quake3rt             # Interactive mode (ESC to quit, P for screenshot)
```

## Technical Details

### Shader Architecture

**Vertex Shader** (2 lines):
- Fullscreen quad with UV mapping

**Fragment Shader** (~80 lines, code-golfed):
- Hash functions (Dave Hoskins' sine-free hash)
- Perlin noise (3D procedural)
- SDF primitives: Box, Sphere, Torus, Capsule
- Smooth minimum (Inigo Quilez)
- Scene mapping with domain repetition
- Central difference normal calculation
- Sphere-tracing ray marcher
- 5-step ambient occlusion
- Diffuse/Specular/Fresnel lighting
- Distance fog
- Gamma correction (sRGB)

### Camera System
Orbital camera with parametric motion:
```c
ro = vec3(cos(T*.3)*5., 2.+sin(T*.5), sin(T*.3)*5.)
```
- Radius: 5 units
- Height: 2 ± sine wave
- Rotation: 0.3 rad/sec

### Performance
- Resolution: 1024x768
- Ray steps: Up to 64 per pixel
- Precision: 0.001 unit threshold
- Optimizations: -O3, -march=native, -ffast-math

## Code Structure

```
Quake3/
├── src/
│   ├── main.c           # Interactive viewer
│   └── screenshot.c     # Automated screenshot tool
├── screenshots/         # Generated output (PPM format)
├── Makefile             # Build system
├── Raytracing-shaders.glsl  # Reference shader (8294 lines)
├── assets/              # Quake 3 assets (textures, models, maps)
└── reference/           # Example code (SILK codec)
```

## Implementation Notes

### SDF Primitives
Based on Inigo Quilez's work at iquilezles.org:

**Box SDF**:
```glsl
float sBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q,0.)) + min(max(q.x,max(q.y,q.z)),0.);
}
```

**Smooth Min** (k=0.3):
```glsl
float smin(float a, float b, float k) {
    float h = clamp(.5+.5*(b-a)/k, 0., 1.);
    return mix(b, a, h) - k*h*(1.-h);
}
```

### Ray Marching Loop
Sphere tracing with early termination:
```glsl
float march(vec3 o, vec3 d) {
    float t = 0.;
    for(int i=0; i<64; i++) {
        float h = map(o + d*t);
        if(h < .001 || t > 50.) break;
        t += h;
    }
    return t;
}
```

### Ambient Occlusion
Screen-space approximation:
```glsl
float ao(vec3 p, vec3 n) {
    float o=0., s=1.;
    for(int i=0; i<5; i++) {
        float h = .01 + .12*float(i)/4.;
        float d = map(p + h*n);
        o += s*(h-d);
        s *= .95;
    }
    return clamp(1.-3.*o, 0., 1.);
}
```

## Screenshots

Four timestamped views showcasing camera orbit and lighting:
- `screenshot_0.ppm`: t=0.0s (initial position)
- `screenshot_1.ppm`: t=2.0s (rotation)
- `screenshot_2.ppm`: t=5.0s (half orbit)
- `screenshot_3.ppm`: t=10.0s (full rotation + height variation)

Format: PPM (Portable Pixmap) - 1024x768 RGB

## References

### Original Shader
- **Raytracing-shaders.glsl**: Andrei Drexler's Quake recreation
- **Shadertoy**: https://www.shadertoy.com/

### Techniques
- **Inigo Quilez**: SDF functions, ray marching
  - https://iquilezles.org/articles/distfunctions
  - https://iquilezles.org/articles/smin
- **Jamie Wong**: Ray marching tutorial
  - http://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions/
- **Mercury**: hg_sdf library
  - http://mercury.sexy/hg_sdf
- **Dave Hoskins**: Hash without Sine
  - https://www.shadertoy.com/view/4djSRW

## License

This project combines:
- Original code (this implementation): Created for educational purposes
- Raytracing-shaders.glsl: © Andrei Drexler 2018
- Quake 3 Arena assets: © id Software

## Building Notes

The code uses implicit OpenGL function declarations (warnings suppressed) to minimize binary size. In production, link against GLEW or GLAD for explicit declarations.

### Suppressing Warnings
```bash
gcc -Wno-implicit-function-declaration ...
```

## Future Enhancements

Potential additions while maintaining code-golfed style:
- [ ] Texture sampling from assets/textures/
- [ ] BSP map loading from assets/maps/
- [ ] Multi-buffer passes (lightmaps)
- [ ] Temporal anti-aliasing
- [ ] Motion blur
- [ ] More SDF primitives (cylinder, hexagonal prism)
- [ ] Dynamic lighting
- [ ] Shadows via secondary rays

## Performance Optimization

Current optimizations:
- GCC flags: `-O3 -march=native -ffast-math`
- GLSL: Minimal branching, loop unrolling hints
- Early ray termination
- Reduced AO samples (5 vs 8+)
- Simple lighting model

Measured FPS: ~60fps @ 1024x768 on modern hardware
