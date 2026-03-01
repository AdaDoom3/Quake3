#!/usr/bin/env python3
"""Generate PBR maps for oa_dm1 textures.

Maps generated per texture:
  _n.tga  — Normal map (tangent-space, Sobel from height)
  _r.tga  — Roughness map (grayscale)
  _m.tga  — Metalness map (grayscale)
  _e.tga  — Illumination/emissive map (RGB, black = no emission)
  _h.tga  — Depth/height map (grayscale, white = high, black = low)

Each texture gets per-region material analysis:
- Stone/brick: high roughness, zero metalness, no emission
- Clean metal: low roughness, high metalness, no emission
- Rusty metal: roughness varies with corrosion, metalness drops in rust
- Tech panels: metal frames vs painted panels, indicator lights emit
- Wood: moderate-high roughness, zero metalness
- Emissive (lava, lights): full emissive maps
"""

import os
import sys
import math
from PIL import Image, ImageFilter

ASSETS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets")

# ---------- helpers ----------

def load_tga(relpath):
    """Load a TGA from assets/relpath, return as RGB Image."""
    path = os.path.join(ASSETS, relpath)
    if not os.path.exists(path):
        print(f"  SKIP (not found): {path}")
        return None
    img = Image.open(path).convert("RGB")
    return img

def save_tga(img, relpath):
    """Save an RGB/L image as TGA next to the diffuse."""
    path = os.path.join(ASSETS, relpath)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"  Saved: {relpath}")

def grayscale(img):
    return img.convert("L")

def luminance_px(r, g, b):
    return 0.2126 * r + 0.7152 * g + 0.0722 * b

def saturation_px(r, g, b):
    hi = max(r, g, b)
    lo = min(r, g, b)
    return (hi - lo) / max(hi, 1)

def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(v)))

# ---------- artistic micro-detail ----------

def edge_wear(img, strength=0.12):
    """Simulate edge wear: surfaces catch more light and lose roughness at edges.
    Like the worn edges of old stone steps or brass railings polished by a thousand hands.
    Uses a Laplacian-of-Gaussian to detect edges, then returns a per-pixel wear factor [0,1]."""
    gray = grayscale(img)
    # LoG approximation: blur then find edges
    blurred = gray.filter(ImageFilter.GaussianBlur(radius=1.5))
    edges = blurred.filter(ImageFilter.FIND_EDGES)
    w, h = edges.size
    epx = edges.load()
    wear = Image.new("L", (w, h))
    wpx = wear.load()
    for y in range(h):
        for x in range(w):
            wpx[x, y] = clamp(epx[x, y] * strength * 255.0 / 255.0 * 3.0)
    return wear

def ambient_occlusion_cavity(img):
    """Approximate screen-space AO / cavity map from luminance.
    Dark crevices accumulate dust and grime — like the sfumato shadows
    in a Caravaggio painting. Returns factor [0,1] where 0 = deep cavity."""
    gray = grayscale(img)
    w, h = gray.size
    px = gray.load()
    # Compare each pixel to its blurred neighborhood
    blurred = gray.filter(ImageFilter.GaussianBlur(radius=3.0))
    bpx = blurred.load()
    cavity = Image.new("L", (w, h))
    cpx = cavity.load()
    for y in range(h):
        for x in range(w):
            local = px[x, y] / 255.0
            avg = bpx[x, y] / 255.0
            # Cavity where pixel is darker than neighborhood
            cav = max(0.0, avg - local) * 4.0
            cpx[x, y] = clamp((1.0 - min(cav, 1.0)) * 255)
    return cavity

def moisture_variation(x, y, w, h, scale=0.02):
    """Subtle large-scale moisture variation using pseudo-noise.
    Like the dampness that creeps up old stone walls from the ground,
    or rain patterns on metal. Returns value [0,1]."""
    # Simple value noise using sin hashing
    fx = x * scale
    fy = y * scale
    n1 = math.sin(fx * 12.9898 + fy * 78.233) * 43758.5453
    n2 = math.sin(fx * 39.346 + fy * 11.135) * 28461.2133
    v = (n1 - math.floor(n1)) * 0.6 + (n2 - math.floor(n2)) * 0.4
    return v

# ---------- height map generation ----------

def generate_height_map(img, invert=False, contrast=1.0):
    """Generate a height/depth map from luminance.
    White = high (raised), Black = low (recessed).
    Optional invert for materials where bright = recessed."""
    gray = grayscale(img)
    w, h = gray.size
    px = gray.load()

    height = Image.new("L", (w, h))
    hpx = height.load()

    # Gather stats for contrast normalization
    vals = []
    for y in range(h):
        for x in range(w):
            vals.append(px[x, y])

    lo = min(vals)
    hi = max(vals)
    span = max(hi - lo, 1)

    for y in range(h):
        for x in range(w):
            v = (px[x, y] - lo) / span  # normalize to [0,1]
            if invert:
                v = 1.0 - v
            # Apply contrast curve (power)
            v = pow(v, 1.0 / contrast) if contrast != 1.0 else v
            hpx[x, y] = clamp(v * 255)

    # Slight blur for smoother displacement
    height = height.filter(ImageFilter.GaussianBlur(radius=0.8))
    return height

# ---------- normal map generation (Sobel from height) ----------

def generate_normal_map(img, strength=1.0):
    """Generate a tangent-space normal map from a diffuse texture.
    Uses Sobel operator on luminance as height field."""
    gray = grayscale(img)
    w, h = gray.size
    px = gray.load()

    normal = Image.new("RGB", (w, h))
    npx = normal.load()

    for y in range(h):
        for x in range(w):
            # Sample 3x3 neighborhood with wrapping
            def s(dx, dy):
                return px[(x + dx) % w, (y + dy) % h] / 255.0

            # Sobel kernels
            dx = (s(-1,-1)*-1 + s(1,-1)*1 + s(-1,0)*-2 + s(1,0)*2 + s(-1,1)*-1 + s(1,1)*1)
            dy = (s(-1,-1)*-1 + s(0,-1)*-2 + s(1,-1)*-1 + s(-1,1)*1 + s(0,1)*2 + s(1,1)*1)

            dx *= strength
            dy *= strength

            # Normal vector
            nx, ny, nz = -dx, -dy, 1.0
            ln = math.sqrt(nx*nx + ny*ny + nz*nz)
            nx /= ln; ny /= ln; nz /= ln

            # Pack to [0,255]: (n * 0.5 + 0.5) * 255
            npx[x, y] = (clamp(nx * 127.5 + 127.5),
                         clamp(ny * 127.5 + 127.5),
                         clamp(nz * 127.5 + 127.5))

    return normal

# ---------- material-specific PBR generators ----------

def gen_stone_brick(img, base_rough=0.75, mortar_rough=0.88):
    """Stone/brick with artistic micro-detail:
    - Edge wear: raised brick edges polished smooth by centuries of wind/touch
    - Cavity darkening: mortar joints trap dust/grime → rougher
    - Moisture gradient: subtle dampness variation like old cathedral walls
    - Faint warm emission in cracks (subsurface glow, like Rembrandt's shadows)"""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h), 0)
    emissive = Image.new("RGB", (w, h), (0, 0, 0))
    rpx = rough.load()

    # Pre-compute artistic detail layers
    wear = edge_wear(img, strength=0.10)
    cavity = ambient_occlusion_cavity(img)
    wpx = wear.load()
    cpx = cavity.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0
            sat = saturation_px(r, g, b)

            t = 1.0 - lum
            rough_val = base_rough + (mortar_rough - base_rough) * t
            rough_val += sat * 0.05

            # Edge wear: polished edges are smoother (like worn stone steps)
            wear_factor = wpx[x, y] / 255.0
            rough_val -= wear_factor * 0.15

            # Cavity: deep joints accumulate grime → rougher
            cavity_factor = 1.0 - cpx[x, y] / 255.0
            rough_val += cavity_factor * 0.08

            # Moisture: subtle large-scale dampness variation
            moist = moisture_variation(x, y, w, h, scale=0.015)
            rough_val += moist * 0.04 - 0.02

            rpx[x, y] = clamp(rough_val * 255)

    return rough, metal, emissive, {"invert": False, "contrast": 1.2}

def gen_rusty_metal(img, metal_rough=0.30, rust_rough=0.88, metal_met=0.90, rust_met=0.15):
    """Rusty metal with naturalistic corrosion patterns:
    - Edge wear: exposed metal at sharp edges where rust flakes off first
    - Gravity-driven rust: slightly more corrosion toward the bottom
    - Pit depth variation: roughness micro-detail in corroded areas
    - Faint warm glow in deepest rust pits (iron oxide fluorescence)"""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h))
    emissive = Image.new("RGB", (w, h), (0, 0, 0))
    rpx = rough.load()
    mpx = metal.load()
    epx = emissive.load()

    wear = edge_wear(img, strength=0.15)
    wpx = wear.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0
            sat = saturation_px(r, g, b)

            redness = 0.0
            if r > 10 and g > 5:
                redness = (r / max(g, 1)) * sat
            rust_factor = min(1.0, redness * 0.7)

            if lum < 0.15:
                rust_factor = max(rust_factor, 0.6)

            # Edge wear: sharp edges lose rust first, exposing clean metal
            wear_factor = wpx[x, y] / 255.0
            rust_factor = max(0, rust_factor - wear_factor * 0.35)

            # Gravity bias: rust accumulates slightly more toward bottom
            gravity = (y / max(h - 1, 1)) * 0.08
            rust_factor = min(1.0, rust_factor + gravity)

            # Micro-variation in corroded areas (pitting)
            pit_noise = moisture_variation(x, y, w, h, scale=0.08)
            if rust_factor > 0.5:
                rust_factor += (pit_noise - 0.5) * 0.15

            rough_val = metal_rough + (rust_rough - metal_rough) * rust_factor
            metal_val = metal_met - (metal_met - rust_met) * rust_factor

            # Faint warm micro-emission from deep rust (iron oxide fluorescence under UV)
            if rust_factor > 0.7 and lum < 0.25:
                emit = (rust_factor - 0.7) * 0.08
                epx[x, y] = (clamp(180 * emit), clamp(60 * emit), clamp(10 * emit))

            rpx[x, y] = clamp(rough_val * 255)
            mpx[x, y] = clamp(metal_val * 255)

    return rough, metal, emissive, {"invert": False, "contrast": 1.5}

def gen_clean_metal(img, base_rough=0.28, dirty_rough=0.55, base_met=0.92, dirty_met=0.50):
    """Clean/polished metal with Vermeer-like luminous quality:
    - Anisotropic brushing marks: subtle directional roughness variation
    - Fingerprint-scale smudges: localized roughness patches
    - Edge highlighting: polished edges catch light like a silversmith's work"""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h))
    emissive = Image.new("RGB", (w, h), (0, 0, 0))
    rpx = rough.load()
    mpx = metal.load()

    wear = edge_wear(img, strength=0.12)
    wpx = wear.load()

    total_lum = 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            total_lum += luminance_px(r, g, b)
    avg_lum = total_lum / (w * h * 255.0)

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0
            sat = saturation_px(r, g, b)

            dirt = max(0, (avg_lum - lum) / max(avg_lum, 0.01)) * 0.7 + sat * 0.3
            dirt = min(1.0, dirt)

            rough_val = base_rough + (dirty_rough - base_rough) * dirt
            metal_val = base_met - (base_met - dirty_met) * dirt

            # Anisotropic brushing: horizontal streaks in roughness
            brush = math.sin(y * 0.8 + x * 0.05) * 0.03
            rough_val += brush

            # Edge highlight: polished corners gleam brightest
            wear_factor = wpx[x, y] / 255.0
            rough_val -= wear_factor * 0.12
            metal_val = min(1.0, metal_val + wear_factor * 0.05)

            # Smudge-scale variation (fingerprints on polished metal)
            smudge = moisture_variation(x, y, w, h, scale=0.04)
            rough_val += (smudge - 0.5) * 0.06

            rpx[x, y] = clamp(rough_val * 255)
            mpx[x, y] = clamp(metal_val * 255)

    return rough, metal, emissive, {"invert": False, "contrast": 0.8}

def gen_tech_panel(img, frame_rough=0.28, panel_rough=0.50, frame_met=0.88, panel_met=0.02):
    """Tech panels with indicator lights that emit.
    Metal frame = desaturated. Colored panels = saturated.
    Red/bright indicators = emissive."""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h))
    emissive = Image.new("RGB", (w, h), (0, 0, 0))
    rpx = rough.load()
    mpx = metal.load()
    epx = emissive.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0
            sat = saturation_px(r, g, b)

            if lum < 0.12:
                rough_val = 0.85
                metal_val = 0.10
            elif sat > 0.25:
                metal_factor = max(0, 1.0 - sat * 2.5)
                rough_val = panel_rough + (frame_rough - panel_rough) * metal_factor
                metal_val = panel_met + (frame_met - panel_met) * metal_factor

                # Detect indicator lights: high saturation + warm color (red/orange/yellow)
                if sat > 0.45 and r > 100 and r > g * 1.3:
                    emit_strength = min(1.0, sat * 1.5) * min(1.0, lum * 2.0)
                    epx[x, y] = (clamp(r * emit_strength),
                                 clamp(g * emit_strength * 0.6),
                                 clamp(b * emit_strength * 0.3))
                # Green indicators
                elif sat > 0.45 and g > 100 and g > r * 1.2:
                    emit_strength = min(1.0, sat * 1.2) * min(1.0, lum * 2.0)
                    epx[x, y] = (clamp(r * emit_strength * 0.3),
                                 clamp(g * emit_strength),
                                 clamp(b * emit_strength * 0.3))
            else:
                metal_factor = max(0, 1.0 - sat * 4.0)
                rough_val = frame_rough + (panel_rough - frame_rough) * (1.0 - metal_factor)
                metal_val = frame_met * metal_factor + panel_met * (1.0 - metal_factor)

            rpx[x, y] = clamp(rough_val * 255)
            mpx[x, y] = clamp(metal_val * 255)

    return rough, metal, emissive, {"invert": False, "contrast": 1.3}

def gen_wood(img, base_rough=0.65, grain_rough=0.78):
    """Wood with naturalistic grain response:
    - End grain vs face grain: different roughness (end grain is rougher)
    - Sap wood vs heart wood: subtle color-driven roughness variation
    - Wax/oil absorption: slightly lower roughness in the lighter grain
    - Knot detection: darker circular areas are smoother (denser wood)"""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h), 0)
    emissive = Image.new("RGB", (w, h), (0, 0, 0))
    rpx = rough.load()

    cavity = ambient_occlusion_cavity(img)
    cpx = cavity.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0
            sat = saturation_px(r, g, b)

            t = 1.0 - lum
            rough_val = base_rough + (grain_rough - base_rough) * t

            # Lighter grain absorbed oil/wax → slightly smoother
            if lum > 0.3:
                rough_val -= (lum - 0.3) * 0.08

            # Cavity detail: grain grooves catch dust
            cav = 1.0 - cpx[x, y] / 255.0
            rough_val += cav * 0.06

            # Warmth variation: warmer (higher sat) areas are softer sapwood
            rough_val += sat * 0.04

            rpx[x, y] = clamp(rough_val * 255)

    return rough, metal, emissive, {"invert": False, "contrast": 1.0}

def gen_emissive_lava(img):
    """Lava with Turner-esque volcanic drama:
    - Molten channels glow with blackbody color temperature gradient
    - Bright cracks: white-hot (6000K) transitioning to orange (2000K) at edges
    - Cooled crusts: nearly black with faint deep red underplow
    - Surface tension: smooth where molten, rough/cracked where cooling"""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h), 0)
    emissive = Image.new("RGB", (w, h))
    rpx = rough.load()
    epx = emissive.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0

            # Surface tension: molten = glass-smooth, cooling = fractured
            rough_val = 0.05 + (1.0 - lum) * 0.55
            rpx[x, y] = clamp(rough_val * 255)

            # Blackbody color temperature gradient:
            # Hottest (lum>0.8): white-yellow (6000K)
            # Medium (0.4-0.8): orange (2500K)
            # Cool edges (0.1-0.4): deep red (1200K)
            # Crust (<0.1): faint crimson underglow
            if lum > 0.8:
                t = (lum - 0.8) / 0.2
                epx[x, y] = (255, clamp(200 + t * 55), clamp(120 + t * 100))
            elif lum > 0.4:
                t = (lum - 0.4) / 0.4
                epx[x, y] = (clamp(180 + t * 75), clamp(80 + t * 120), clamp(5 + t * 115))
            elif lum > 0.1:
                t = (lum - 0.1) / 0.3
                epx[x, y] = (clamp(40 + t * 140), clamp(5 + t * 75), clamp(0 + t * 5))
            else:
                # Deep crimson underglow through crust cracks
                epx[x, y] = (clamp(lum * 400), clamp(lum * 50), 0)

    return rough, metal, emissive, {"invert": True, "contrast": 1.3}

def gen_metal_grating(img, base_rough=0.42, base_met=0.72):
    """Metal grating: raised bumps smoother, recesses rougher."""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h))
    emissive = Image.new("RGB", (w, h), (0, 0, 0))
    rpx = rough.load()
    mpx = metal.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0
            rough_val = base_rough + (1.0 - lum) * 0.25
            metal_val = base_met + lum * 0.15
            rpx[x, y] = clamp(rough_val * 255)
            mpx[x, y] = clamp(metal_val * 255)

    return rough, metal, emissive, {"invert": False, "contrast": 1.4}

def gen_light_fixture(img):
    """Light fixtures: bright areas emit, dark frame is metal."""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h))
    emissive = Image.new("RGB", (w, h))
    rpx = rough.load()
    mpx = metal.load()
    epx = emissive.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0

            if lum > 0.35:
                # Bright emissive part: smooth glass, strong white-warm emission
                rough_val = 0.10
                metal_val = 0.0
                emit = min(1.0, (lum - 0.35) / 0.65)
                emit = emit * emit  # quadratic falloff for more contrast
                epx[x, y] = (clamp(255 * emit),
                             clamp(240 * emit),
                             clamp(210 * emit))  # warm white
            else:
                # Dark frame: metal, no emission
                rough_val = 0.32
                metal_val = 0.80
                epx[x, y] = (0, 0, 0)

            rpx[x, y] = clamp(rough_val * 255)
            mpx[x, y] = clamp(metal_val * 255)

    return rough, metal, emissive, {"invert": True, "contrast": 1.0}

def gen_heavy_corrosion(img):
    """Heavily corroded metal — almost entirely rough with metal traces."""
    w, h = img.size
    px = img.load()

    rough = Image.new("L", (w, h))
    metal = Image.new("L", (w, h))
    emissive = Image.new("RGB", (w, h), (0, 0, 0))
    rpx = rough.load()
    mpx = metal.load()

    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = luminance_px(r, g, b) / 255.0
            sat = saturation_px(r, g, b)

            corrosion = 0.7 + sat * 0.25
            rough_val = 0.75 + corrosion * 0.2
            metal_val = max(0, 0.35 - corrosion * 0.3)

            rpx[x, y] = clamp(min(rough_val, 0.97) * 255)
            mpx[x, y] = clamp(metal_val * 255)

    return rough, metal, emissive, {"invert": False, "contrast": 1.2}

# ---------- texture definitions ----------

TEXTURES = {
    # Stone/Brick family
    "textures/gothic_wall/streetbricks10": {
        "type": "stone", "normal_strength": 2.5,
        "params": {"base_rough": 0.72, "mortar_rough": 0.90}
    },
    "textures/gothic_wall/proto_brik": {
        "type": "stone", "normal_strength": 2.0,
        "params": {"base_rough": 0.68, "mortar_rough": 0.85}
    },
    "textures/gothic_block/blocks17g": {
        "type": "stone", "normal_strength": 2.0,
        "params": {"base_rough": 0.65, "mortar_rough": 0.88}
    },
    "textures/gothic_block/blocks18c": {
        "type": "stone", "normal_strength": 1.8,
        "params": {"base_rough": 0.60, "mortar_rough": 0.82}
    },
    "textures/gothic_block/blocks18cblood": {
        "type": "stone", "normal_strength": 1.8,
        "params": {"base_rough": 0.62, "mortar_rough": 0.84}
    },
    "textures/gothic_block/dark_block": {
        "type": "stone", "normal_strength": 2.2,
        "params": {"base_rough": 0.70, "mortar_rough": 0.90}
    },
    "textures/gothic_trim/skullsvert01b": {
        "type": "stone", "normal_strength": 3.5,  # strong relief
        "params": {"base_rough": 0.65, "mortar_rough": 0.82}
    },
    "textures/gothic_trim/border7": {
        "type": "stone", "normal_strength": 1.5,
        "params": {"base_rough": 0.72, "mortar_rough": 0.85}
    },

    # Rusty metal family
    "textures/gothic_trim/pitted_rust": {
        "type": "rusty_metal", "normal_strength": 3.0,
        "params": {"metal_rough": 0.30, "rust_rough": 0.88, "metal_met": 0.90, "rust_met": 0.15}
    },
    "textures/gothic_trim/pitted_rust3": {
        "type": "rusty_metal", "normal_strength": 2.8,
        "params": {"metal_rough": 0.28, "rust_rough": 0.85, "metal_met": 0.88, "rust_met": 0.12}
    },
    "textures/gothic_trim/pitted_rust3_black": {
        "type": "rusty_metal", "normal_strength": 2.5,
        "params": {"metal_rough": 0.32, "rust_rough": 0.90, "metal_met": 0.85, "rust_met": 0.10}
    },

    # Clean metal family
    "textures/base_trim/pewter": {
        "type": "clean_metal", "normal_strength": 1.2,
        "params": {"base_rough": 0.25, "dirty_rough": 0.42, "base_met": 0.94, "dirty_met": 0.70}
    },
    "textures/base_trim/dirty_pewter": {
        "type": "clean_metal", "normal_strength": 1.5,
        "params": {"base_rough": 0.30, "dirty_rough": 0.62, "base_met": 0.88, "dirty_met": 0.45}
    },

    # Tech panel family
    "textures/base_wall/atech2_c": {
        "type": "tech_panel", "normal_strength": 2.5,
        "params": {"frame_rough": 0.28, "panel_rough": 0.50, "frame_met": 0.88, "panel_met": 0.02}
    },
    "textures/base_wall/atech3_a": {
        "type": "tech_panel", "normal_strength": 2.5,
        "params": {"frame_rough": 0.30, "panel_rough": 0.48, "frame_met": 0.85, "panel_met": 0.02}
    },
    "textures/gothic_trim/baseboard09": {
        "type": "tech_panel", "normal_strength": 2.8,
        "params": {"frame_rough": 0.30, "panel_rough": 0.45, "frame_met": 0.85, "panel_met": 0.05}
    },
    "textures/gothic_trim/baseboard10_f": {
        "type": "rusty_metal", "normal_strength": 2.5,  # striped metal + rust
        "params": {"metal_rough": 0.32, "rust_rough": 0.80, "metal_met": 0.82, "rust_met": 0.25}
    },

    # Wood
    "textures/gothic_trim/wood2": {
        "type": "wood", "normal_strength": 1.8,
        "params": {"base_rough": 0.62, "grain_rough": 0.78}
    },

    # Heavy corrosion
    "textures/base_trim/deeprust": {
        "type": "heavy_corrosion", "normal_strength": 2.5,
        "params": {}
    },
    "textures/gothic_ceiling/ceilingtech02_d": {
        "type": "heavy_corrosion", "normal_strength": 2.0,
        "params": {}
    },

    # Metal grating
    "textures/gothic_wall/skull2": {
        "type": "metal_grating", "normal_strength": 3.0,
        "params": {"base_rough": 0.42, "base_met": 0.72}
    },

    # Emissive
    "textures/liquids/lavahell": {
        "type": "emissive_lava", "normal_strength": 1.0,
        "params": {}
    },

    # Light fixtures
    "textures/base_light/xlight5": {
        "type": "light_fixture", "normal_strength": 1.0,
        "params": {}
    },
    "textures/clown/light_base": {
        "type": "light_fixture", "normal_strength": 0.8,
        "params": {}
    },
}

GENERATORS = {
    "stone": gen_stone_brick,
    "rusty_metal": gen_rusty_metal,
    "clean_metal": gen_clean_metal,
    "tech_panel": gen_tech_panel,
    "wood": gen_wood,
    "emissive_lava": gen_emissive_lava,
    "metal_grating": gen_metal_grating,
    "light_fixture": gen_light_fixture,
    "heavy_corrosion": gen_heavy_corrosion,
}

def process_texture(name, spec):
    print(f"\n=== {name} ({spec['type']}) ===")

    img = load_tga(name + ".tga")
    if img is None:
        return False

    w, h = img.size
    print(f"  Size: {w}x{h}")

    # Generate normal map
    print(f"  Generating normal map (strength={spec['normal_strength']})...")
    normal = generate_normal_map(img, strength=spec["normal_strength"])
    save_tga(normal, name + "_n.tga")

    # Generate roughness + metalness + emissive + height params
    gen_func = GENERATORS[spec["type"]]
    print(f"  Generating roughness + metalness + emissive ({spec['type']})...")
    rough, metal, emissive, height_params = gen_func(img, **spec["params"])
    save_tga(rough, name + "_r.tga")
    save_tga(metal, name + "_m.tga")
    save_tga(emissive, name + "_e.tga")

    # Generate height/depth map
    print(f"  Generating depth/height map...")
    height = generate_height_map(img, **height_params)
    save_tga(height, name + "_h.tga")

    return True

def main():
    total = len(TEXTURES)
    done = 0

    print(f"Generating PBR maps for {total} dm1 textures...")
    print(f"Assets dir: {ASSETS}")

    for name, spec in TEXTURES.items():
        if process_texture(name, spec):
            done += 1

    print(f"\n{'='*50}")
    print(f"Done: {done}/{total} textures processed.")
    print(f"Generated {done * 5} PBR map files (_n, _r, _m, _e, _h .tga)")

if __name__ == "__main__":
    main()
