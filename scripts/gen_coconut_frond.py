"""Procedural coconut palm (Cocos nucifera) frond texture cards.

Generates top-down frond cards (rachis vertical, base at bottom, tip at top) for
alpha-tested foliage cards in Blender / UE.

Usage: python scripts/gen_coconut_frond.py [--width 1024] [--seed 7]
Output: art/textures/palm/coconut_frond/frond_<variant>_<map>.png
  basecolor (RGBA, alpha = opacity), normal_gl, normal_dx, roughness, translucency
"""
import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parent.parent / "art" / "textures" / "palm" / "coconut_frond"
SS = 2  # supersampling factor

# Per-variant look. Colors are sRGB 0-255.
VARIANTS = {
    "green": dict(base=(58, 92, 30), tip=(104, 128, 42), rachis=(150, 140, 70), var=0.10,
                  torn=0.12, missing=0.01, curl=0.0, rough=0.42, sss=1.0, brown_tips=0.15),
    "old": dict(base=(122, 128, 40), tip=(176, 150, 56), rachis=(176, 150, 80), var=0.14,
                torn=0.25, missing=0.04, curl=0.15, rough=0.55, sss=0.8, brown_tips=0.45),
    "dry": dict(base=(120, 92, 58), tip=(156, 128, 88), rachis=(130, 104, 72), var=0.18,
                torn=0.45, missing=0.12, curl=0.55, rough=0.85, sss=0.25, brown_tips=0.0),
}


def lerp(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def leaflet_poly(p0, d, length, width, droop, side, steps=24):
    """Outline of a linear-lanceolate leaflet along a bent centerline."""
    pts_l, pts_r, center = [], [], []
    ang0 = math.atan2(d[1], d[0])
    for i in range(steps + 1):
        t = i / steps
        ang = ang0 - side * droop * t * t  # bends toward the frond tip (up)
        if i == 0:
            x, y = p0
        else:
            x = center[-1][0] + math.cos(ang) * length / steps
            y = center[-1][1] + math.sin(ang) * length / steps
        center.append((x, y))
        # narrow base, widest ~30%, acuminate tip
        w = width * (min(1.0, t / 0.12) ** 0.6) * (1 - t) ** 0.85
        nx, ny = -math.sin(ang), math.cos(ang)
        pts_l.append((x + nx * w / 2, y + ny * w / 2))
        pts_r.append((x - nx * w / 2, y - ny * w / 2))
    return pts_l + pts_r[::-1], center


def build(variant, W, rng):
    v = VARIANTS[variant]
    H = W * 4
    w, h = W * SS, H * SS
    color = Image.new("RGB", (w, h), (0, 0, 0))
    alpha = Image.new("L", (w, h), 0)
    height = Image.new("L", (w, h), 0)
    sss = Image.new("L", (w, h), 0)
    dc, da, dh, ds = (ImageDraw.Draw(i) for i in (color, alpha, height, sss))

    # Rachis: slight S-bend, thick at base, thin at tip.
    def rachis(s):  # s=0 base .. 1 tip
        x = w / 2 + w * 0.02 * math.sin(s * math.pi * 1.3)
        y = h * (0.985 - 0.97 * s)
        return x, y

    def rachis_w(s):
        return w * (0.030 * (1 - s) + 0.004)

    n_per_side = 95
    leaflets = []
    for side in (-1, 1):
        for i in range(n_per_side):
            s = 0.10 + 0.88 * (i + rng.uniform(-0.3, 0.3)) / n_per_side
            if rng.random() < v["missing"]:
                continue
            # length peaks at mid-frond, short at base and tip
            prof = math.sin(math.pi * min(1.0, 0.12 + 0.95 * s)) ** 0.75
            length = w * 0.47 * prof * rng.uniform(0.88, 1.06) * (1 - 0.35 * v["curl"])
            if rng.random() < v["torn"]:
                length *= rng.uniform(0.45, 0.85)
            if length < w * 0.03:
                continue
            width = w * 0.026 * (0.6 + 0.4 * prof) * rng.uniform(0.85, 1.1) * (1 - 0.5 * v["curl"])
            # leaflets point outward and toward the tip (~50° from rachis)
            a = math.radians(rng.uniform(40, 58)) + 0.25 * (s - 0.5)
            d = (side * math.sin(a), -math.cos(a))
            droop = rng.uniform(0.15, 0.45) * (1 + v["curl"])
            leaflets.append((s, side, length, width, d, droop))

    # draw far side first so near leaflets overlap
    leaflets.sort(key=lambda L: rng.random())
    for s, side, length, width, d, droop in leaflets:
        p0 = rachis(s)
        p0 = (p0[0] + side * rachis_w(s) * 0.3, p0[1])
        poly, center = leaflet_poly(p0, d, length, width, droop, -side)
        # base→tip color gradient drawn as segments
        tint = rng.uniform(-v["var"], v["var"])
        base = lerp(v["base"], (255, 255, 200) if tint > 0 else (20, 30, 10), abs(tint))
        tipc = lerp(v["tip"], (255, 255, 200) if tint > 0 else (20, 30, 10), abs(tint))
        n = len(center) - 1
        L, R = poly[: n + 1], poly[n + 1:][::-1]
        brown = v["brown_tips"] * rng.random()
        for k in range(n):
            t = k / n
            c = lerp(base, tipc, t)
            if brown and t > 1 - brown:
                c = lerp(c, (128, 96, 60), (t - (1 - brown)) / brown)
            quad = [L[k], L[k + 1], R[k + 1], R[k]]
            dc.polygon(quad, fill=c)
            da.polygon(quad, fill=255)
            ds.polygon(quad, fill=int(255 * v["sss"] * (0.75 + 0.25 * t)))
        # height: folded leaflet, ridge on the midrib (reduplicate fold)
        dh.polygon(poly, fill=90)
        mid = [(x, y) for x, y in center]
        dh.line(mid, fill=200, width=max(2, int(width * 0.35)))
        dh.line(mid, fill=255, width=max(1, int(width * 0.12)))
        # midrib + parallel veins, slightly lighter/darker
        dc.line(mid[: int(n * 0.9)], fill=lerp(base, (230, 220, 150), 0.35), width=max(1, SS))
        if v["curl"] > 0.3:  # dry leaflets: dark split along the fold
            dc.line(mid[int(n * 0.2):], fill=lerp(base, (40, 30, 20), 0.5), width=max(1, SS))

    # rachis on top
    steps = 200
    for k in range(steps):
        s0, s1 = k / steps, (k + 1) / steps
        if s0 > 0.985:
            break
        p0, p1 = rachis(s0), rachis(s1)
        rw = rachis_w(s0)
        c = lerp(v["rachis"], v["base"], s0 * 0.6)
        quad = [(p0[0] - rw / 2, p0[1]), (p0[0] + rw / 2, p0[1]),
                (p1[0] + rachis_w(s1) / 2, p1[1]), (p1[0] - rachis_w(s1) / 2, p1[1])]
        dc.polygon(quad, fill=c)
        da.polygon(quad, fill=255)
        ds.polygon(quad, fill=int(40 * v["sss"]))
        dh.line([p0, p1], fill=160, width=int(rw))
        dh.line([p0, p1], fill=255, width=max(1, int(rw * 0.4)))

    # Downsample (antialiasing) to final size.
    color = color.resize((W, H), Image.LANCZOS)
    alpha = alpha.resize((W, H), Image.LANCZOS)
    height = height.resize((W, H), Image.LANCZOS).filter(ImageFilter.GaussianBlur(1.2))
    sss = sss.resize((W, H), Image.LANCZOS)

    # Fine longitudinal fiber noise on color + roughness.
    a = np.asarray(alpha, np.float32) / 255
    col = np.asarray(color, np.float32)
    noise = rng.normal(0, 1, (H // 2, W)).astype(np.float32)
    noise = np.asarray(Image.fromarray(((noise * 30) + 128).clip(0, 255).astype(np.uint8))
                       .resize((W, H), Image.BILINEAR)
                       .filter(ImageFilter.GaussianBlur(0.8)), np.float32) / 255 - 0.5
    col *= (1 + 0.18 * noise)[..., None]
    # Bleed color into transparent pixels so mipmaps don't fringe dark.
    bled = Image.fromarray(col.clip(0, 255).astype(np.uint8))
    for r in (2, 6, 16):
        blur = np.asarray(bled.filter(ImageFilter.GaussianBlur(r)), np.float32)
        wb = np.asarray(alpha.filter(ImageFilter.GaussianBlur(r)), np.float32)[..., None] / 255
        fill = np.where(wb > 1e-3, blur / np.maximum(wb, 1e-3), col)
        col = np.where(a[..., None] > 0.5, col, np.where(wb > 1e-3, fill, col))
        bled = Image.fromarray(col.clip(0, 255).astype(np.uint8))
    rgba = np.dstack([col.clip(0, 255), a * 255]).astype(np.uint8)

    # Normal from height.
    hgt = np.asarray(height, np.float32) / 255 * 3.0
    gy, gx = np.gradient(hgt)
    nz = np.ones_like(hgt)
    nrm = np.dstack([-gx, gy, nz])  # OpenGL: +Y up in image space
    nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    gl = ((nrm * 0.5 + 0.5) * 255).astype(np.uint8)
    dx = gl.copy()
    dx[..., 1] = 255 - dx[..., 1]

    rough = np.full((H, W), v["rough"], np.float32) + 0.08 * noise - 0.1 * (hgt / 3.0) * (v["rough"] < 0.6)
    rough = (rough.clip(0, 1) * 255).astype(np.uint8)

    OUT.mkdir(parents=True, exist_ok=True)
    stem = OUT / f"frond_{variant}"
    Image.fromarray(rgba, "RGBA").save(f"{stem}_basecolor.png")
    Image.fromarray(gl, "RGB").save(f"{stem}_normal_gl.png")
    Image.fromarray(dx, "RGB").save(f"{stem}_normal_dx.png")
    Image.fromarray(rough, "L").save(f"{stem}_roughness.png")
    sss.save(f"{stem}_translucency.png")
    print(f"  ok  {stem.name} ({W}x{H})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=1024)
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    for i, name in enumerate(VARIANTS):
        build(name, args.width, np.random.default_rng(args.seed + i))


if __name__ == "__main__":
    main()
