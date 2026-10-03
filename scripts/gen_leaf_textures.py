"""Procedural leaf textures for atoll plants, in "strip space".

The 3D leaves are opaque geometry strips (no alpha cut-out, Nanite-friendly): the strip's
UV is u = across (0 left margin, 0.5 midrib, 1 right margin), v = along (0 base, 1 tip).
The leaf outline comes from the geometry; these textures only carry color, veins and
surface detail. Color variation (age, dryness) is added per vertex in the material.

Usage: python scripts/gen_leaf_textures.py
Output: art/textures/plants/<species>_{basecolor,normal_gl,normal_dx,roughness}.png
"""
import math
from pathlib import Path

import numpy as np
from PIL import Image

OUT = Path(__file__).resolve().parent.parent / "art" / "textures" / "plants"

# Colors are linear-ish sRGB 0..1. aspect = physical length / max width.
SPECIES = {
    # Cocos nucifera leaflet: long, narrow, glossy, parallel veins, lighter midrib.
    "coconut_leaflet": dict(size=(128, 1024), aspect=22, base=(0.27, 0.42, 0.10), tip=(0.36, 0.47, 0.12),
                            margin=(0.42, 0.47, 0.16), midrib=(0.62, 0.60, 0.26), midrib_w=0.09,
                            veins="parallel", n_par=9, vein_depth=0.35, rough=0.42, blotch=0.07,
                            flecks=0.25),
    # Coconut seedling first leaves (eophylls): pleated (plicate), bright green.
    "coconut_eophyll": dict(size=(256, 1024), aspect=8, base=(0.26, 0.45, 0.10), tip=(0.32, 0.50, 0.12),
                            margin=(0.34, 0.48, 0.12), midrib=(0.45, 0.55, 0.20), midrib_w=0.04,
                            veins="plicate", n_par=12, vein_depth=1.0, rough=0.40, blotch=0.05),
    # Pandanus tectorius: long strap, V-channel, spiny margins and midrib.
    "pandanus": dict(size=(128, 1024), aspect=22, base=(0.20, 0.36, 0.10), tip=(0.33, 0.42, 0.13),
                     margin=(0.34, 0.40, 0.14), midrib=(0.30, 0.42, 0.16), midrib_w=0.05,
                     veins="parallel", n_par=13, vein_depth=0.25, rough=0.48, blotch=0.06,
                     spines=True, flecks=0.35),
    # Scaevola taccada (naupaka): succulent, spatulate, glossy yellow-green, pale midrib.
    "naupaka": dict(size=(512, 512), aspect=2.6, base=(0.38, 0.55, 0.13), tip=(0.42, 0.58, 0.16),
                    margin=(0.48, 0.60, 0.20), midrib=(0.72, 0.76, 0.48), midrib_w=0.035,
                    veins="pinnate", n_sec=7, vein_angle=55, vein_depth=0.25, rough=0.33, blotch=0.05),
    # Heliotropium foertherianum (tahinu): silvery, velvety, obovate.
    "tahinu": dict(size=(512, 512), aspect=2.8, base=(0.50, 0.57, 0.46), tip=(0.54, 0.60, 0.50),
                   margin=(0.60, 0.64, 0.56), midrib=(0.70, 0.73, 0.64), midrib_w=0.03,
                   veins="pinnate", n_sec=9, vein_angle=58, vein_depth=0.45, rough=0.82, blotch=0.04,
                   hair=0.10),
    # Pemphis acidula (miki-miki): tiny thick grey-green leaves.
    "mikimiki": dict(size=(256, 256), aspect=2.4, base=(0.31, 0.43, 0.22), tip=(0.35, 0.46, 0.25),
                     margin=(0.39, 0.47, 0.28), midrib=(0.42, 0.50, 0.32), midrib_w=0.04,
                     veins="none", vein_depth=0.0, rough=0.62, blotch=0.06, hair=0.05),
    # Phymatosorus grossus fern lobe: glossy, fine netted veins.
    "fern_lobe": dict(size=(256, 512), aspect=7, base=(0.13, 0.34, 0.07), tip=(0.17, 0.38, 0.08),
                      margin=(0.20, 0.38, 0.10), midrib=(0.30, 0.42, 0.14), midrib_w=0.05,
                      veins="pinnate", n_sec=16, vein_angle=62, vein_depth=0.3, rough=0.38, blotch=0.05),
    # Ipomoea pes-caprae: thick, glossy, round bilobed leaf.
    "ipomoea": dict(size=(512, 512), aspect=1.0, base=(0.18, 0.40, 0.10), tip=(0.21, 0.43, 0.11),
                    margin=(0.26, 0.44, 0.13), midrib=(0.50, 0.58, 0.32), midrib_w=0.03,
                    veins="pinnate", n_sec=6, vein_angle=48, vein_depth=0.35, rough=0.32, blotch=0.04),
    # Coastal grasses / sedges (Lepturus, Fimbristylis): green base, straw tips.
    "grass": dict(size=(64, 512), aspect=40, base=(0.30, 0.44, 0.14), tip=(0.62, 0.58, 0.32),
                  margin=(0.40, 0.48, 0.18), midrib=(0.44, 0.52, 0.22), midrib_w=0.10,
                  veins="parallel", n_par=5, vein_depth=0.3, rough=0.55, blotch=0.08, tip_ramp=0.55),
}


def value_noise(shape, cells, rng, octaves=4, persistence=0.5):
    """Tileable fBm value noise in [0,1] over an image grid. cells=(cx, cy) base frequency."""
    h, w = shape
    total = np.zeros(shape, np.float32)
    amp, norm = 1.0, 0.0
    cx, cy = cells
    for _ in range(octaves):
        grid = rng.random((cy, cx)).astype(np.float32)
        ys = np.linspace(0, cy, h, endpoint=False)
        xs = np.linspace(0, cx, w, endpoint=False)
        y0 = np.floor(ys).astype(int)
        x0 = np.floor(xs).astype(int)
        fy = ys - y0
        fx = xs - x0
        fy = fy * fy * (3 - 2 * fy)
        fx = fx * fx * (3 - 2 * fx)
        y1 = (y0 + 1) % cy
        x1 = (x0 + 1) % cx
        a = grid[np.ix_(y0 % cy, x0 % cx)]
        b = grid[np.ix_(y0 % cy, x1)]
        c = grid[np.ix_(y1, x0 % cx)]
        d = grid[np.ix_(y1, x1)]
        top = a + (b - a) * fx[None, :]
        bot = c + (d - c) * fx[None, :]
        total += amp * (top + (bot - top) * fy[:, None])
        norm += amp
        amp *= persistence
        cx, cy = cx * 2, cy * 2
    return total / norm


def lerp(a, b, t):
    a = np.asarray(a, np.float32)
    b = np.asarray(b, np.float32)
    return a + (b - a) * t[..., None]


def build(name, p, rng):
    w, h = p["size"]
    u = (np.arange(w, dtype=np.float32) + 0.5) / w
    v = (np.arange(h, dtype=np.float32) + 0.5) / h
    U, V = np.meshgrid(u, v)  # V row 0 = base; flipped when saving so v=0 is the image bottom
    X = U * 2 - 1
    AX = np.abs(X)
    aspect = p["aspect"]
    along = V * aspect  # in units of leaf width

    # Base color: base->tip gradient, margins, blotches.
    ramp = V ** p.get("tip_ramp", 1.0)
    col = lerp(p["base"], p["tip"], ramp)
    col = lerp(col, p["margin"], np.clip((AX - 0.78) / 0.22, 0, 1) ** 2)
    blotch = value_noise((h, w), (2, max(2, int(aspect / 2))), rng, 4) - 0.5
    col *= (1 + p["blotch"] * 2 * blotch)[..., None]

    height = np.zeros((h, w), np.float32)
    # Midrib.
    mr = np.exp(-(X / p["midrib_w"]) ** 2)
    col = lerp(col, p["midrib"], mr * 0.85)
    height += 0.6 * mr

    veins = p["veins"]
    if veins == "parallel":
        k = p["n_par"]
        line = np.cos(np.pi * X * k) ** 8
        col *= (1 - 0.06 * line)[..., None]
        height -= p["vein_depth"] * line * 0.3
    elif veins == "plicate":  # pleated folds: strong height ridges, light/dark bands
        k = p["n_par"]
        fold = np.abs(((X * k * 0.5 + along * 0.02) % 1.0) - 0.5) * 2  # triangle wave
        height += p["vein_depth"] * (fold - 0.5)
        col *= (0.92 + 0.12 * fold)[..., None]
    elif veins == "pinnate":
        ang = np.radians(p["vein_angle"])
        slope = 1.0 / np.tan(ang)
        # secondary veins leave the midrib toward the tip and curve toward the margin
        phase = (along - AX * 0.5 * slope - 0.25 * AX ** 2) * p["n_sec"] / aspect
        d = np.abs((phase % 1.0) - 0.5) * 2  # 1 on the vein, 0 between
        sec = np.clip((d - 0.86) / 0.14, 0, 1) ** 1.5 * np.clip(1 - AX * 0.9, 0, 1)
        col = lerp(col, p["midrib"], sec * 0.35)
        height -= p["vein_depth"] * sec
        # tertiary net
        net = value_noise((h, w), (max(4, w // 24), max(4, h // 24)), rng, 2)
        net = np.clip((np.abs(net - 0.5) < 0.04) * 1.0, 0, 1)
        height -= 0.08 * net
        # puffed areoles between veins
        height += 0.15 * (1 - d) * np.clip(1 - AX, 0, 1)

    if p.get("spines"):  # small pale teeth along margins
        teeth = (np.cos(along * 2 * np.pi * 1.2) > 0.92) & (AX > 0.94)
        col = np.where(teeth[..., None], lerp(col, (0.75, 0.70, 0.45), np.ones_like(AX) * 0.7), col)
    if p.get("flecks"):  # brown flecks / scale damage
        f = value_noise((h, w), (w // 6 or 2, h // 6), rng, 2)
        spots = np.clip((f - 0.72) / 0.1, 0, 1) * p["flecks"]
        col = lerp(col, (0.42, 0.36, 0.20), spots)
    if p.get("hair"):  # velvety silver sheen
        fine = rng.random((h, w)).astype(np.float32)
        col *= (1 + p["hair"] * (fine - 0.5) * 2)[..., None]
        col = lerp(col, (0.78, 0.80, 0.74), np.full_like(AX, p["hair"] * 1.5))

    grain = rng.normal(0, 1, (h, w)).astype(np.float32)
    col *= (1 + 0.025 * grain)[..., None]
    height += 0.05 * grain

    # Normal (OpenGL) from height. Physical pixel size differs across/along.
    sx = 1.0 / w
    sy = aspect / h
    gy, gx = np.gradient(height)
    strength = 0.02
    nx = -gx / sx * strength
    ny = gy / sy * strength
    n = np.dstack([nx, ny, np.ones_like(nx)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)

    rough = p["rough"] + 0.06 * (value_noise((h, w), (4, 8), rng, 3) - 0.5) - 0.08 * mr
    rough = np.clip(rough, 0.05, 1)

    def save(arr, suffix, mode):
        arr = np.flipud(arr)  # v=0 (leaf base) at the bottom of the image
        Image.fromarray((np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8), mode).save(OUT / f"{name}_{suffix}.png")

    save(col, "basecolor", "RGB")
    gl = n * 0.5 + 0.5
    save(gl, "normal_gl", "RGB")
    dx = gl.copy()
    dx[..., 1] = 1 - dx[..., 1]
    save(dx, "normal_dx", "RGB")
    save(rough, "roughness", "L")
    print(f"  ok  {name} {w}x{h}")


def twig_card(name, rng, size=512, leaf_col=((0.27, 0.38, 0.19), (0.38, 0.48, 0.26)), n_side=6,
              leaf_len=(30, 44), leaf_w=(12, 17), dry=0.06):
    """Alpha card: a small leafy twig (opposite pairs of small leaves), stem at the bottom center."""
    from PIL import ImageDraw

    ss = 2
    S = size * ss
    col = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    hgt = Image.new("L", (S, S), 0)
    dc, dh = ImageDraw.Draw(col), ImageDraw.Draw(hgt)
    stem = (92, 78, 62, 255)

    def twig(p0, ang, length, width):
        pts = [p0]
        a = ang
        for _ in range(12):
            a += rng.normal(0, 0.05)
            x, y = pts[-1]
            pts.append((x + math.cos(a) * length / 12, y - math.sin(a) * length / 12))
        dc.line(pts, fill=stem, width=int(width))
        return pts

    main = twig((S / 2, S * 0.99), math.pi / 2 + rng.normal(0, 0.08), S * 0.9, 4 * ss)
    twigs = [main]
    for _ in range(n_side):
        i = int(rng.integers(2, 10))
        side = rng.choice([-1, 1])
        twigs.append(twig(main[i], math.pi / 2 + side * rng.uniform(0.5, 0.95), S * rng.uniform(0.25, 0.45), 2 * ss))
    leaves = []
    for pts in twigs:
        for k in range(1, len(pts)):
            x, y = pts[k]
            dx, dy = pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]
            a = math.atan2(-dy, dx)
            for sd in (-1, 1):
                if rng.random() < 0.12:
                    continue
                la = a + sd * rng.uniform(0.75, 1.15)
                L = rng.uniform(*leaf_len) * ss * (0.6 + 0.4 * k / len(pts))
                Wl = rng.uniform(*leaf_w) * ss * (L / (leaf_len[1] * ss))
                leaves.append((x, y, la, L, Wl))
    order = rng.permutation(len(leaves))
    for j in order:
        x, y, la, L, Wl = leaves[j]
        cx, cy = x + math.cos(la) * L / 2, y - math.sin(la) * L / 2
        t = np.linspace(0, 2 * math.pi, 24)
        ex, ey = np.cos(t) * L / 2, np.sin(t) * Wl / 2
        ca, sa = math.cos(-la), math.sin(-la)
        poly = list(zip(cx + ex * ca - ey * sa, cy + ex * sa + ey * ca))
        c0, c1 = np.array(leaf_col[0]), np.array(leaf_col[1])
        c = c0 + (c1 - c0) * rng.random()
        if rng.random() < dry:
            c = np.array([0.55, 0.45, 0.28]) * rng.uniform(0.8, 1.1)
        dc.polygon(poly, fill=tuple(int(v * 255) for v in c) + (255,))
        dc.line([(x, y), (cx + math.cos(la) * L * 0.4, cy - math.sin(la) * L * 0.4)],
                fill=tuple(int(min(1, v * 1.25) * 255) for v in c) + (255,), width=max(1, ss))
        dh.polygon(poly, fill=int(rng.uniform(120, 200)))
    col = col.resize((size, size), Image.LANCZOS)
    hgt = hgt.resize((size, size), Image.LANCZOS)
    rgba = np.asarray(col, np.float32) / 255
    a = rgba[..., 3]
    # bleed color into transparent texels (no dark fringes on mips)
    from PIL import ImageFilter
    rgb = rgba[..., :3].copy()
    for r in (2, 6, 14):
        im = Image.fromarray((rgb * 255).astype(np.uint8))
        bl = np.asarray(im.filter(ImageFilter.GaussianBlur(r)), np.float32) / 255
        wa = np.asarray(Image.fromarray((a * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(r)), np.float32) / 255
        rgb = np.where(a[..., None] > 0.5, rgb, np.where(wa[..., None] > 1e-3, bl / np.maximum(wa[..., None], 1e-3), rgb))
    out = np.dstack([np.clip(rgb, 0, 1), a])
    Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA").save(OUT / f"{name}_basecolor.png")
    h = np.asarray(hgt.filter(__import__("PIL.ImageFilter", fromlist=["x"]).GaussianBlur(2)), np.float32) / 255
    gy, gx = np.gradient(h)
    n = np.dstack([-gx * 6, gy * 6, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    gl = n * 0.5 + 0.5
    Image.fromarray((gl * 255).astype(np.uint8), "RGB").save(OUT / f"{name}_normal_gl.png")
    dx = gl.copy()
    dx[..., 1] = 1 - dx[..., 1]
    Image.fromarray((dx * 255).astype(np.uint8), "RGB").save(OUT / f"{name}_normal_dx.png")
    Image.fromarray(np.full((size, size), int(0.62 * 255), np.uint8), "L").save(OUT / f"{name}_roughness.png")
    print(f"  ok  {name} (alpha card) {size}x{size}")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for i, (name, p) in enumerate(SPECIES.items()):
        build(name, p, np.random.default_rng(100 + i))
    twig_card("mikimiki_twig", np.random.default_rng(900))


if __name__ == "__main__":
    main()
