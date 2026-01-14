# Scene Gallery and Technical Breakdown

This document describes each test scene in detail, including the mathematical and rendering techniques used.

---

## Scene 0: Basic Primitives Test

### Purpose
Validate fundamental SDF primitives and basic ray marching functionality.

### Geometry

**Box** (Cube):
```glsl
float sBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.)) + min(max(q.x, max(q.y, q.z)), 0.);
}
```
- Position: (-2, 0, 0)
- Size: 1×1×1 unit cube
- Distance field: Exact signed distance

**Sphere**:
```glsl
float sSph(vec3 p, float r) {
    return length(p) - r;
}
```
- Position: (2, 0, 0)
- Radius: 1 unit
- Distance field: Mathematically perfect

**Torus**:
```glsl
float sTor(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}
```
- Position: (0, 0, 0)
- Major radius: 1.5 units
- Minor radius: 0.3 units

### Camera Motion
```glsl
vec3 ro = vec3(cos(T*0.5)*6., sin(T*0.3)*2., sin(T*0.5)*6.);
```
- Orbit radius: 6 units
- Orbit speed: 0.5 rad/sec horizontal
- Height oscillation: ±2 units at 0.3 rad/sec
- Focal point: Origin (0, 0, 0)

### Lighting
- **Model**: Simple diffuse only
- **Light Direction**: (0.5, 1.0, 0.3) normalized
- **Base Color**: (0.7, 0.6, 0.5) - warm beige
- **Sky Color**: (0.05, 0.1, 0.15) - dark blue-grey

### Ray Marching
- **Max Steps**: 96
- **Precision**: 0.001 units
- **Max Distance**: 50 units
- **Expected Steps**: 15-25 average

### Performance Characteristics
- **FPS**: 449 average
- **Complexity**: O(1) - constant geometry
- **Bottleneck**: None (simple scene)

### Screenshots
1. **t=0.0s**: Frontal view, all three primitives visible
2. **t=1.5s**: 45° rotation, torus hole visible
3. **t=3.0s**: 90° rotation, side view
4. **t=5.0s**: 135° rotation, back side approach
5. **t=7.5s**: Nearly full rotation, height variation

---

## Scene 1: Complex Room (Original)

### Purpose
Full-featured Quake-inspired environment with advanced lighting and infinite repetition.

### Geometry

**Room Bounds**:
```glsl
float d = sBox(p - vec3(0, -2, 0), vec3(8, 1, 8));  // Floor
d = min(d, sBox(p - vec3(0, 6, 0), vec3(8, 1, 8))); // Ceiling
d = min(d, sBox(p - vec3(-8, 2, 0), vec3(1, 5, 8))); // Left wall
d = min(d, sBox(p - vec3(8, 2, 0), vec3(1, 5, 8)));  // Right wall
d = min(d, sBox(p - vec3(0, 2, 8), vec3(8, 5, 1)));  // Back wall
```
- Dimensions: 16×8×16 units (W×H×D)
- Floor height: -2 units
- Ceiling height: +6 units

**Infinite Pillars** (Domain Repetition):
```glsl
vec3 q = p;
q.xz = fract(q.xz + 0.5) - 0.5;  // Fold space into 1×1 cells
float pillar = sBox(q - vec3(0, 0, 0), vec3(0.3, 4, 0.3));
```
- Cell size: 1×1 units on XZ plane
- Pillar size: 0.6×8×0.6 units
- Count: Infinite (via domain folding)
- Technique: Spatial modulo operation

**Torch Holders** (Capsule SDF):
```glsl
float torch = sCap(q - vec3(0, 1, 0), vec3(0, 0, 0), vec3(0, 0.8, 0), 0.1);
```
- Also repeated infinitely
- Height: 0.8 units above pillar base
- Radius: 0.1 units

**Smooth Blending**:
```glsl
d = smin(d, pillar, 0.3);  // k = 0.3
```
- Blend radius: 0.3 units
- Creates organic "melting" effect between pillars and room

### Lighting Model

**Components**:
1. **Diffuse** (Lambertian):
   ```glsl
   float dif = clamp(dot(nor, lig), 0., 1.);
   ```

2. **Specular** (Blinn-Phong, n=16):
   ```glsl
   vec3 hal = normalize(lig - rd);
   float spe = pow(clamp(dot(nor, hal), 0., 1.), 16.);
   ```

3. **Fresnel** (Schlick approximation, power=2):
   ```glsl
   float fre = pow(clamp(1. + dot(nor, rd), 0., 1.), 2.);
   ```

4. **Ambient Occlusion** (5-step approximation):
   ```glsl
   float ao(vec3 p, vec3 n) {
       float o = 0., s = 1.;
       for(int i = 0; i < 5; i++) {
           float h = 0.01 + 0.12 * float(i) / 4.;
           float d = map(p + h * n);
           o += s * (h - d);
           s *= 0.95;
       }
       return clamp(1. - 3. * o, 0., 1.);
   }
   ```
   - Sample range: 0.01 to 0.13 units
   - Sample decay: 0.95 per step
   - Cost: 5 additional ray marches per pixel

### Texturing

**3D Perlin Noise**:
```glsl
float tex = n(pos * 4.) * 0.5 + 0.5;
col = vec3(0.6, 0.5, 0.4) * tex;
```
- Frequency: 4.0 (scales world space)
- Range: [0, 1] (remapped from [-1, 1])
- Base color: Brownish sandstone aesthetic
- Applied to: All surfaces

### Post-Processing

**Distance Fog**:
```glsl
col = mix(col, vec3(0.1, 0.15, 0.2), 1. - exp(-0.01 * t * t));
```
- Exponential falloff
- Rate: 0.01 (quadratic in distance)
- Fog color: Matches sky (blue-grey)

**Gamma Correction**:
```glsl
col = pow(col, vec3(0.4545));  // 1/2.2
```
- Converts linear to sRGB
- Gamma: 2.2 (standard)

### Camera Motion
```glsl
vec3 ro = vec3(cos(T*0.3)*5., 2.+sin(T*0.5), sin(T*0.3)*5.);
vec3 ta = vec3(0, 2, 0);
```
- Orbit radius: 5 units
- Orbit speed: 0.3 rad/sec
- Height: 2 ± sin(0.5*T) units
- Look-at: (0, 2, 0) - eye level

### Ray Marching
- **Max Steps**: 64
- **Expected Steps**: 25-35 (complex geometry)
- **Precision**: 0.001 units
- **AO Cost**: 5× additional map() calls per hit

### Performance Characteristics
- **FPS**: 446 average
- **Complexity**: O(1) per pixel (domain repetition is O(1))
- **Bottleneck**: AO calculation (5 samples/pixel)
- **Optimization**: Early ray termination crucial

### Technical Notes
- Domain repetition creates infinite detail at constant cost
- Smooth union adds visual interest without topology complexity
- AO provides depth cues critical for spatial understanding
- Noise adds material variation without texture mapping

---

## Scene 2: Stress Test - Smooth Blending

### Purpose
Maximum computational load to test performance under worst-case conditions.

### Geometry

**Animated Spheres** (8 total):
```glsl
for(int i = 0; i < 8; i++) {
    float a = float(i) * 3.14159 * 0.25;  // 45° spacing
    vec3 q = p - vec3(cos(a+T)*3., sin(T+float(i)), sin(a+T)*3.);
    d = smin(d, sSph(q, 0.5), 0.5);
}
```
- Arrangement: Circular orbit, radius 3 units
- Size: 0.5 unit radius each
- Animation: Synchronized rotation + individual height oscillation
- Blend: k=0.5 (heavy blending)

**Center Box**:
```glsl
d = smin(d, sBox(p, vec3(2)), 0.3);
```
- Size: 4×4×4 units (half-bound = 2)
- Position: Origin
- Blend: k=0.3 with sphere cluster

### Computational Complexity

**Per-Pixel Cost**:
```
Ray march steps: 128 (max)
SDF evaluations per step: 1 map() call
  └─ map() contains:
     - 8 sphere evaluations
     - 8 smin operations (exponential cost)
     - 1 box evaluation
     - 1 final smin

Total ops/pixel (worst case): 128 * (8 + 8 + 1 + 1) = 2,304 operations
```

**Why It's Slow**:
1. **Smooth Min Cost**: Each smin() involves:
   - 1 clamp()
   - 1 mix()
   - Multiple arithmetic operations
   - ~10× more expensive than regular min()

2. **Loop Overhead**: 8 iterations in map()

3. **Ray Step Count**: 128 (2× normal)

4. **Animation**: Dynamic geometry prevents caching

### Lighting
```glsl
col = mix(vec3(0.8, 0.3, 0.2), vec3(0.2, 0.3, 0.8), sin(pos.y*2.)*0.5+0.5) * dif;
```
- Gradient: Red-to-blue based on Y position
- Frequency: 2.0 cycles per unit
- Lighting: Simple diffuse only (no AO to save cost)

### Camera Motion
```glsl
vec3 ro = vec3(cos(T*0.4)*8., 3., sin(T*0.4)*8.);
```
- Orbit radius: 8 units (distant view to see all spheres)
- Orbit speed: 0.4 rad/sec
- Height: Fixed at 3 units

### Performance Characteristics
- **FPS**: 7.7 average (intentionally low)
- **Complexity**: O(n) where n=8 spheres
- **Bottleneck**: Smooth min operations in map()
- **Frame time**: 130 ms (vs. 2 ms for Scene 0)

### Expected Behavior
This scene is **supposed to be slow**. It validates:
- Ray marcher handles complex scenes without crashing
- Performance degrades gracefully under load
- Visual quality maintained even at low FPS
- Smooth blending produces organic "blobby" appearance

---

## Scene 3: Fractal Domain Folding

### Purpose
Demonstrate advanced domain manipulation techniques for procedural complexity.

### Domain Folding Technique

**Kaleidoscopic Fold**:
```glsl
p = abs(p);  // Octahedral fold
if(p.x < p.y) p.xy = p.yx;  // Sort coordinates
if(p.x < p.z) p.xz = p.zx;
if(p.y < p.z) p.yz = p.zy;
```
- Effect: Reflects space across 6 planes
- Result: 48-fold symmetry (octahedral group)
- Cost: 3 conditionals, 3 swaps

**Iterative Folding**:
```glsl
for(int i = 0; i < 3; i++) {
    p = abs(p) - vec3(0.5);
    d = min(d, sSph(p, 0.3));
}
```
- Iterations: 3 levels
- Translation: 0.5 units per iteration
- Effect: Recursive sphere placement

### Resulting Geometry
- Base: Initial folded box
- Layer 1: 48 spheres (octahedral symmetry)
- Layer 2: 48 × 2 = 96 spheres (subdivided)
- Layer 3: 48 × 4 = 192 spheres (further subdivided)
- Total visual complexity: ~336 apparent primitives
- Actual primitives: 1 box + 3 spheres per iteration

### Visual Effect
Creates a "Menger sponge" / "Jerusalem cube" like structure:
- Self-similar at multiple scales
- High visual complexity from simple code
- Organic, crystalline appearance

### Lighting - Fresnel Only
```glsl
float fres = pow(1. - abs(dot(nor, rd)), 3.);
col = mix(vec3(0.1, 0.3, 0.5), vec3(0.9, 0.7, 0.3), fres);
```
- Base color: Dark blue
- Edge color: Warm gold
- Exponent: 3 (sharper than typical Fresnel)
- Effect: Highlights geometric edges

### Camera Motion
```glsl
vec3 ro = vec3(cos(T*0.6)*4., sin(T*0.3)*2., sin(T*0.6)*4.);
```
- Orbit radius: 4 units (close view)
- Orbit speed: 0.6 rad/sec (fast)
- Height oscillation: ±2 units
- FOV: 1.5 (narrower than default for detail)

### Ray Marching
- **Max Steps**: 96
- **Precision**: 0.001 units
- **Challenge**: Many near-misses with sphere cluster
- **Optimization**: Early termination crucial

### Performance Characteristics
- **FPS**: 387 average
- **Complexity**: Appears O(n) but actually O(1)
- **Bottleneck**: Ray convergence (many small features)
- **Optimization**: Domain folding cheaper than explicit geometry

### Mathematical Insight
This scene demonstrates that:
- Visual complexity ≠ computational complexity
- Domain manipulation can create fractal detail cheaply
- Symmetry operations (reflections) are nearly free
- Iteration count matters more than apparent geometry count

---

## Comparative Analysis

| Feature | Scene 0 | Scene 1 | Scene 2 | Scene 3 |
|---------|---------|---------|---------|---------|
| **Primitives** | 3 | 2 + ∞ | 9 | 4 (folded) |
| **Lighting** | Diffuse | Full | Diffuse | Fresnel |
| **Ray Steps** | 96 | 64 | 128 | 96 |
| **AO** | No | Yes (5) | No | No |
| **Texturing** | No | Noise | Gradient | Procedural |
| **Animation** | Camera | Camera | Geom+Cam | Camera |
| **FPS** | 449 | 446 | 7.7 | 387 |
| **Complexity** | Low | Medium | Very High | Medium-High |

### Key Takeaways

1. **Lighting Cost**: AO (Scene 1) adds minimal overhead (~1% FPS loss)
2. **Smooth Blending**: Extremely expensive (Scene 2: 58× slower)
3. **Domain Folding**: Efficient complexity (Scene 3: 87% of Scene 0 FPS)
4. **Ray Steps**: Minimal impact (Scene 0: 96 steps @ 449 FPS)

---

## Technical Implementation Notes

### Code-Golfing in Scenes

All test scenes use extreme code-golfing:
- No whitespace except newlines
- Single-letter function names
- Inline all expressions
- Decimal shorthand (`.5` vs `0.5`)

**Example** (Scene 2 map function):
```glsl
float map(vec3 p){float d=1e10;for(int i=0;i<8;i++){float a=float(i)*3.14159*.25;
vec3 q=p-vec3(cos(a+T)*3.,sin(T+float(i)),sin(a+T)*3.);d=smin(d,sSph(q,.5),.5);}
d=smin(d,sBox(p,vec3(2)),.3);return d;}
```

**Readable equivalent** (not used):
```glsl
float map(vec3 position) {
    float distance = 1e10;

    // Create 8 animated spheres in circular pattern
    for(int i = 0; i < 8; i++) {
        float angle = float(i) * PI * 0.25;
        vec3 sphere_pos = position - vec3(
            cos(angle + time) * 3.0,
            sin(time + float(i)),
            sin(angle + time) * 3.0
        );
        distance = smooth_min(distance, sphere_sdf(sphere_pos, 0.5), 0.5);
    }

    // Add center box
    distance = smooth_min(distance, box_sdf(position, vec3(2.0)), 0.3);

    return distance;
}
```

**Savings**: ~60% character reduction

---

## Shader Fragments Reference

### Complete Scene 0 Fragment Shader
```glsl
#version 330 core
uniform vec2 R;uniform float T;uniform int S;in vec2 uv;out vec4 C;
float h(float n){return fract(sin(n)*43758.5453);}
float sBox(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}
float sSph(vec3 p,float r){return length(p)-r;}
float sTor(vec3 p,vec2 t){vec2 q=vec2(length(p.xz)-t.x,p.y);return length(q)-t.y;}
float map(vec3 p){return min(min(sBox(p-vec3(-2,0,0),vec3(1)),sSph(p-vec3(2,0,0),1.)),sTor(p-vec3(0,0,0),vec2(1.5,.3)));}
vec3 norm(vec3 p){vec2 e=vec2(.001,0);return normalize(vec3(map(p+e.xyy)-map(p-e.xyy),map(p+e.yxy)-map(p-e.yxy),map(p+e.yyx)-map(p-e.yyx)));}
float march(vec3 o,vec3 d){float t=0.;for(int i=0;i<96;i++){float h=map(o+d*t);if(h<.001||t>50.)break;t+=h;}return t;}
void main(){vec2 p=(uv-.5)*vec2(R.x/R.y,1.)*2.;vec3 ro=vec3(cos(T*.5)*6.,sin(T*.3)*2.,sin(T*.5)*6.),ta=vec3(0),
f=normalize(ta-ro),r=normalize(cross(vec3(0,1,0),f)),u=cross(f,r);vec3 rd=normalize(p.x*r+p.y*u+2.*f);
float t=march(ro,rd);vec3 col=vec3(.05,.1,.15);if(t<50.){vec3 pos=ro+rd*t,nor=norm(pos);
vec3 lig=normalize(vec3(.5,1.,.3));float dif=clamp(dot(nor,lig),0.,1.);col=vec3(.7,.6,.5)*dif;}
col=pow(col,vec3(.4545));C=vec4(col,1);}
```
**Size**: ~800 bytes

---

## Recommended Testing Workflow

1. **Quick Validation**: Run Scene 0 (fastest, simplest)
2. **Full Feature Test**: Run Scene 1 (all features)
3. **Performance Limit**: Run Scene 2 (stress test)
4. **Advanced Techniques**: Run Scene 3 (domain folding)

**Command**:
```bash
make test    # Runs all 4 scenes + benchmarks
```

**Output**:
- 20 screenshots (5 per scene)
- Performance metrics per scene
- FPS min/max/average
- Total test time: ~45 seconds

---

**Document Version**: 1.0
**Last Updated**: 2026-01-14
**Scenes Documented**: 4
**Total Screenshots**: 20
