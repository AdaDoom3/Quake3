# Technical Implementation Notes

## BSP Format Parsing

The engine correctly parses Q3 BSP format (version 0x2e):

**Lumps processed**:
- Lump 1: Shaders/textures (16 entries)
- Lump 2: Entities  
- Lump 3: Nodes (BSP tree)
- Lump 4: Faces
- Lump 5: Leaf faces
- Lump 6: Mesh vertices  
- Lump 8: Shader references
- Lump 10: Vertices (5045 verts)
- Lump 11: Mesh indices (5913 indices)
- Lump 13: Faces (652 faces)
- Lump 14: Lightmaps (12 lightmaps, 128x128 RGB)

## Rendering Pipeline

**Face types**:
- Type 1: Indexed triangles (636 faces) - uses glDrawElementsBaseVertex
- Type 3: Triangle fans (0 in dm4ish)
- Other types: Patches/billboards (16 skipped)

**Texture binding**:
- Unit 0: Diffuse texture (TGA, mipmapped)
- Unit 1: Lightmap (128x128 RGB, bilinear filtered)

**Shader pipeline**:
```glsl
// Vertex: Transform to clip space via VP matrix
gl_Position = VP * vec4(position, 1.0);

// Fragment: Multitexture with lightmap modulation
color = texture(diffuse, uv).rgb * texture(lightmap, lm_uv).rgb * 2.0;
```

## Matrix Math

Column-major OpenGL matrices:
- View matrix: LookAt camera transformation
- Projection: Perspective with 70° FOV, 0.1-4096 depth range
- Combined VP matrix for efficiency

## Performance Characteristics

**Memory**:
- Vertex buffer: 5045 verts × (3 pos + 2 uv + 2 lm + 4 color) = ~242KB
- Index buffer: 5913 indices × 4 bytes = ~23KB
- Textures: 12 × ~256KB = ~3MB
- Lightmaps: 12 × 64KB = ~768KB
- Total VRAM: ~4.2MB

**Rendering**:
- Draw calls: 636 (one per face)
- Triangles: ~2000-3000 depending on mesh complexity
- Frame time: ~16ms @ 60 FPS
- Fill rate: Low (simple geometry, no overdraw optimization)

## Code Golf Techniques

1. **Type aliases**: `V` for vec3, `T` for texcoord, etc.
2. **Inline functions**: Pure math functions as static inline
3. **Compound initialization**: Reduces LOC significantly
4. **Pointer arithmetic**: Direct BSP lump access
5. **Array literals**: Static shader strings
6. **Minimal error handling**: Fail-fast returns

## Literate Programming Style

Each major system is documented as a "chapter":
- Chapter I: Type definitions (algebra of types)
- Chapter II: File I/O (monadic operations)
- Chapter III: Linear algebra (vector math)
- Chapter IV: TGA decoding (image transformations)
- Chapter V: BSP parsing (binary deserialization)
- Chapter VI: Shader compilation (GPU pipeline)
- Chapter VII: Texture loading (material system)
- Chapter VIII: Matrix math (projective geometry)
- Chapter IX: Render loop (frame composition)
- Chapter X: Movement (physics simulation)
- Chapter XI: Input (event handling)
- Chapter XII: Initialization (system bootstrap)
- Chapter XIII: Main loop (eternal recursion)

This narrative structure makes the code self-documenting while maintaining extreme brevity.
