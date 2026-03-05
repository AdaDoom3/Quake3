#!/usr/bin/env python3
"""Generate missing Aztec map textures as VTF files.

For each missing material, generates a plausible tileable texture that matches
the general color and character of the original de_aztec / CSPromod textures.
The generated VTFs are RGBA8888 (format 0) which the engine's VTF loader handles.

Textures that are pure tool materials (NODRAW, SKIP, HINT, TRIGGER, INVISIBLE)
are skipped — the engine already handles those with transparent/invisible fallbacks.
"""

import struct, os, sys, hashlib, math, random

# ── VTF Writer ───────────────────────────────────────────────────────────────

VTF_MAGIC      = 0x00465456  # "VTF\0"
VTF_HEADER_SZ  = 80

def write_vtf(path, pixels, width, height):
    """Write a minimal VTF 7.1 file (RGBA8888, no mipmaps, single frame)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fmt = 0  # IMAGE_FORMAT_RGBA8888
    # VTF header: signature, version major, version minor, header size,
    # width, height, flags, frames, first_frame, padding,
    # reflectivity (3 floats), padding, bump_scale, image_format
    hdr = struct.pack("<I II I HH I H H 4x fff 4x f I",
        VTF_MAGIC,         # signature
        7, 1,              # version 7.1
        VTF_HEADER_SZ,     # header size
        width, height,     # dimensions (as uint16)
        0,                 # flags
        1,                 # frames
        0,                 # first frame
        # 4 bytes padding
        0.5, 0.5, 0.5,    # reflectivity
        # 4 bytes padding
        1.0,               # bump scale
        fmt)               # image format
    # Pad header to 80 bytes
    hdr = hdr.ljust(VTF_HEADER_SZ, b'\x00')
    with open(path, "wb") as f:
        f.write(hdr)
        f.write(pixels)
    print(f"  [gen] {path} ({width}x{height})")


# ── Noise Helpers ────────────────────────────────────────────────────────────

def noise_2d(x, y, seed=0):
    """Simple value noise from hash."""
    h = hashlib.md5(struct.pack("<iif", x, y, seed)).digest()
    return (h[0] + h[1] * 256) / 65535.0

def smooth_noise(x, y, scale, seed=0):
    """Bilinear-interpolated value noise."""
    fx, fy = x / scale, y / scale
    ix, iy = int(math.floor(fx)), int(math.floor(fy))
    dx, dy = fx - ix, fy - iy
    n00 = noise_2d(ix,   iy,   seed)
    n10 = noise_2d(ix+1, iy,   seed)
    n01 = noise_2d(ix,   iy+1, seed)
    n11 = noise_2d(ix+1, iy+1, seed)
    nx0 = n00 * (1-dx) + n10 * dx
    nx1 = n01 * (1-dx) + n11 * dx
    return nx0 * (1-dy) + nx1 * dy

def fbm(x, y, octaves=4, seed=0):
    """Fractal Brownian motion — layered noise for natural textures."""
    v, amp, freq = 0.0, 0.5, 32.0
    for i in range(octaves):
        v += amp * smooth_noise(x, y, freq, seed + i * 137)
        amp *= 0.5
        freq *= 0.5
    return max(0.0, min(1.0, v))


# ── Texture Generators ───────────────────────────────────────────────────────

def gen_grass(w, h, seed=42, dark=False):
    """Aztec jungle grass — green with variation."""
    px = bytearray(w * h * 4)
    base_r, base_g, base_b = (50, 90, 35) if not dark else (35, 65, 25)
    for y in range(h):
        for x in range(w):
            n = fbm(x, y, 5, seed)
            n2 = fbm(x * 2.3, y * 1.7, 3, seed + 99)
            r = int(base_r + n * 40 - 15 + n2 * 15)
            g = int(base_g + n * 60 - 20 + n2 * 20)
            b = int(base_b + n * 20 - 8)
            i = (y * w + x) * 4
            px[i:i+4] = bytes([max(0,min(255,r)), max(0,min(255,g)), max(0,min(255,b)), 255])
    return bytes(px)

def gen_stone(w, h, seed=7, base_color=(160, 155, 140)):
    """Aztec carved stone — warm sandstone with subtle grain."""
    px = bytearray(w * h * 4)
    br, bg, bb = base_color
    for y in range(h):
        for x in range(w):
            n = fbm(x, y, 5, seed)
            n2 = fbm(x + 500, y + 500, 3, seed + 31)
            # Mortar line simulation (grid pattern at ~64px)
            mx = abs(math.sin(x * math.pi / 64)) ** 8
            my = abs(math.sin(y * math.pi / 48)) ** 8
            mortar = max(mx, my) * 0.15
            v = n * 0.5 + n2 * 0.3 + 0.2 - mortar
            r = int(br * (0.7 + v * 0.6))
            g = int(bg * (0.7 + v * 0.6))
            b = int(bb * (0.7 + v * 0.55))
            i = (y * w + x) * 4
            px[i:i+4] = bytes([max(0,min(255,r)), max(0,min(255,g)), max(0,min(255,b)), 255])
    return bytes(px)

def gen_ivy(w, h, seed=13):
    """Ivy/vine texture — dark green leaves on transparent-ish background."""
    px = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            n = fbm(x, y, 5, seed)
            leaf = fbm(x * 3, y * 3, 4, seed + 77)
            # Leaves are where leaf noise > 0.45
            if leaf > 0.45:
                intensity = (leaf - 0.45) / 0.55
                r = int(30 + n * 40 + intensity * 20)
                g = int(60 + n * 70 + intensity * 40)
                b = int(20 + n * 25)
                a = int(min(255, 128 + intensity * 200))
            else:
                r, g, b, a = 20, 30, 15, int(leaf / 0.45 * 60)
            i = (y * w + x) * 4
            px[i:i+4] = bytes([max(0,min(255,r)), max(0,min(255,g)), max(0,min(255,b)), max(0,min(255,a))])
    return bytes(px)

def gen_water(w, h, seed=23):
    """Water surface — blue-green with caustic-like patterns."""
    px = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            n1 = fbm(x, y, 4, seed)
            n2 = fbm(x * 1.5 + 200, y * 1.5 + 200, 3, seed + 50)
            caustic = abs(math.sin(n1 * 12) * math.cos(n2 * 10)) ** 0.5
            r = int(20 + caustic * 40 + n1 * 20)
            g = int(60 + caustic * 50 + n1 * 30)
            b = int(100 + caustic * 80 + n1 * 40)
            a = 200  # semi-transparent
            i = (y * w + x) * 4
            px[i:i+4] = bytes([max(0,min(255,r)), max(0,min(255,g)), max(0,min(255,b)), a])
    return bytes(px)

def gen_solid(w, h, r, g, b, a=255):
    """Solid color with very slight noise for non-flatness."""
    px = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            n = smooth_noise(x, y, 16, 42)
            delta = int((n - 0.5) * 8)
            i = (y * w + x) * 4
            px[i:i+4] = bytes([max(0,min(255,r+delta)), max(0,min(255,g+delta)), max(0,min(255,b+delta)), a])
    return bytes(px)


# ── Generate All Missing Textures ────────────────────────────────────────────

def main():
    base = "/tmp/cspromod_textures/cspromod_b105/cspromod/materials"

    textures = [
        # (path_relative_to_materials, generator, width, height)

        # Grass variant B — slightly darker jungle grass
        ("cspromod/aztec/azt_grass1b.vtf", lambda: gen_grass(256, 256, seed=73, dark=True), 256, 256),

        # Column 1B variant — stone column, slightly different shade
        ("cspromod/aztec/azt_column1b.vtf", lambda: gen_stone(256, 256, seed=19, base_color=(150, 140, 125)), 256, 256),

        # Ivy from de_cbble — vine/leaf overlay
        ("de_cbble/ivy_cbble01a.vtf", lambda: gen_ivy(256, 256, seed=13), 256, 256),
        ("de_cbble/ivy_cbble01full.vtf", lambda: gen_ivy(512, 512, seed=17), 512, 512),

        # WVT patches — displacement blend layers, use grass variants
        ("maps/csp_aztec/cspromod/aztec/azt_grass1_wvt_patch.vtf",
         lambda: gen_grass(128, 128, seed=55), 128, 128),
        ("maps/csp_aztec/cspromod/aztec/azt_grass1b_wvt_patch.vtf",
         lambda: gen_grass(128, 128, seed=66, dark=True), 128, 128),

        # Water textures — blue-green caustics
        ("maps/csp_aztec/liquids/water_pretty1_-288_-1088_80.vtf",
         lambda: gen_water(256, 256, seed=23), 256, 256),
        ("maps/csp_aztec/liquids/water_pretty1_1152_1104_-224.vtf",
         lambda: gen_water(256, 256, seed=29), 256, 256),
        ("maps/csp_aztec/liquids/water_pretty1_2720_1728_-64.vtf",
         lambda: gen_water(256, 256, seed=31), 256, 256),
        ("maps/csp_aztec/liquids/water_pretty1_-128_144_-224.vtf",
         lambda: gen_water(256, 256, seed=37), 256, 256),

        # Dev water beneath — dark blue
        ("dev/dev_waterbeneath2.vtf",
         lambda: gen_solid(64, 64, 15, 30, 60, 180), 64, 64),

        # Tool textures — transparent/invisible (1x1 is fine)
        ("tools/toolsnodraw.vtf",    lambda: gen_solid(4, 4, 0, 0, 0, 0), 4, 4),
        ("tools/toolsskybox.vtf",    lambda: gen_solid(4, 4, 135, 180, 220), 4, 4),
        ("tools/toolsskip.vtf",      lambda: gen_solid(4, 4, 0, 0, 0, 0), 4, 4),
        ("tools/toolshint.vtf",      lambda: gen_solid(4, 4, 0, 0, 0, 0), 4, 4),
        ("tools/toolsinvisible.vtf", lambda: gen_solid(4, 4, 0, 0, 0, 0), 4, 4),
        ("tools/toolstrigger.vtf",   lambda: gen_solid(4, 4, 0, 0, 0, 0), 4, 4),
    ]

    print(f"Generating {len(textures)} missing textures...")
    for rel_path, gen_fn, w, h in textures:
        full_path = os.path.join(base, rel_path)
        if os.path.exists(full_path):
            print(f"  [skip] {rel_path} (already exists)")
            continue
        pixels = gen_fn()
        write_vtf(full_path, pixels, w, h)
    print("Done.")

if __name__ == "__main__":
    main()
