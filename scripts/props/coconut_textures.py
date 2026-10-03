"""Textures of the fallen coconuts SM_Coconut_Brown / _Green / _Yellow (T-048), painted in the UV space of the
existing meshes (scripts/atoll/palms.py nut_template: lat-long sphere, u around the long axis, v from the stalk
end (v = 0, calyx) to the pointed free end (v = 1)), so the meshes already in Unreal only need new material
instances.

Usage: python scripts/props/coconut_textures.py [--res 1024] [--preview]
Output: art/export/Props/Coconut/Textures/T_Coconut_<Var>_D.png (sRGB), _N.png (DirectX green down), _ORM.png
        art/_previews/coconut_textures.png with --preview
Detail: fresh nuts get a smooth waxy skin with fine longitudinal streaks, three soft ridges and a calyx of bracts;
the dry nut gets coconut fibres from the CC0 scan ambientCG Bark014 (art/textures/characters/Bark014, fetched by
scripts/character/fetch_character_assets.py), gradient-mapped to husk browns.
"""
import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art" / "export" / "Props" / "Coconut" / "Textures"
BARK = ROOT / "art" / "textures" / "characters" / "Bark014" / "Bark014_1K-JPG_Color.jpg"
BARK_H = ROOT / "art" / "textures" / "characters" / "Bark014" / "Bark014_1K-JPG_Displacement.jpg"

# sRGB colours per variant: base, light, dark, tip, calyx
VARIANTS = {
    "Green": dict(base="4F7F22", light="7DA83A", dark="2E5214", tip="9C9A3A", calyx="6A6A2C", rough=0.42, dry=False),
    "Yellow": dict(base="C79A26", light="E8BE48", dark="8A6414", tip="B0742A", calyx="7A6A2C", rough=0.45, dry=False),
    "Brown": dict(base="6E4A2A", light="A27446", dark="3A2414", tip="2E1C10", calyx="3A2616", rough=0.88, dry=True),
}


def hex_lin(h):
    c = np.array([int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)])
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def value_noise(shape, cells, seed, periodic_u=True):
    """Smooth noise on a (H, W) grid with `cells` lattice cells (periodic along u = columns)."""
    rng = np.random.default_rng(seed)
    cu, cv = cells
    g = rng.random((cv + 2, cu + 1))
    if periodic_u:
        g[:, -1] = g[:, 0]
    H, W = shape
    v = np.linspace(0, cv, H, endpoint=False)[:, None]
    u = np.linspace(0, cu, W, endpoint=False)[None, :]
    i0, j0 = np.floor(v).astype(int), np.floor(u).astype(int)
    fv, fu = v - i0, u - j0
    fv, fu = fv * fv * (3 - 2 * fv), fu * fu * (3 - 2 * fu)
    a = g[i0, j0]
    b = g[i0, j0 + 1]
    c = g[i0 + 1, j0]
    d = g[i0 + 1, j0 + 1]
    return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv


def fbm(shape, cells, seed, octaves=4):
    out = np.zeros(shape)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        out += amp * value_noise(shape, (cells[0] * 2 ** o, cells[1] * 2 ** o), seed + o)
        tot += amp
        amp *= 0.5
    return out / tot


def sample_tiled(img, H, W, tiles_u, tiles_v):
    """Sample a square scan as a tiling texture over the UV square (u repeats tiles_u times)."""
    a = np.asarray(img, np.float64) / 255.0
    ih, iw = a.shape[:2]
    ys = (np.arange(H)[:, None] * tiles_v * ih / H).astype(int) % ih
    xs = (np.arange(W)[None, :] * tiles_u * iw / W).astype(int) % iw
    return a[ys, xs]


def paint(var, res, seed):
    P = VARIANTS[var]
    H = W = res
    v = (np.arange(H)[:, None] + 0.5) / H          # 0 at the stalk end, 1 at the pointed end
    u = (np.arange(W)[None, :] + 0.5) / W
    base, light, dark = hex_lin(P["base"]), hex_lin(P["light"]), hex_lin(P["dark"])
    # three soft ridges along the nut (trigonal section), a little wobble
    wob = (fbm((H, W), (4, 3), seed + 5, 2) - 0.5) * 0.06
    ridge = 0.5 + 0.5 * np.cos(3 * 2 * math.pi * (u + wob))
    # fine longitudinal streaks: noise stretched along v
    streak = fbm((H, W), (90, 3), seed + 11, 3)
    blotch = fbm((H, W), (6, 4), seed + 21, 4)
    height = 0.35 * ridge + 0.2 * streak
    if P["dry"]:
        img = Image.open(BARK).convert("L")
        fib = sample_tiled(img, H, W, 3, 2)
        img_h = Image.open(BARK_H).convert("L")
        fib_h = sample_tiled(img_h, H, W, 3, 2)
        t = np.clip((fib - 0.15) / 0.55, 0, 1)[..., None]
        col = dark * (1 - t) + light * t
        col = col * (0.85 + 0.3 * blotch[..., None])
        height = 0.45 * fib_h + 0.25 * ridge + 0.1 * streak
        rough = np.full((H, W), P["rough"]) - 0.1 * fib
    else:
        t = np.clip(0.25 + 0.5 * streak + 0.3 * (blotch - 0.5), 0, 1)[..., None]
        col = base * (1 - t) + light * t
        col = col * (0.9 + 0.12 * ridge[..., None])
        col = col * (1 - 0.25 * np.clip((blotch[..., None] - 0.62) * 4, 0, 1)) + dark * 0.25 * np.clip(
            (blotch[..., None] - 0.62) * 4, 0, 1)
        rough = np.full((H, W), P["rough"]) + 0.08 * (streak - 0.5)
    # pointed free end: slightly darker, dry little tip
    tip = np.clip((v - 0.86) / 0.14, 0, 1)[..., None] ** 2
    col = col * (1 - 0.65 * tip) + hex_lin(P["tip"]) * 0.65 * tip
    # calyx at the stalk end: star of overlapping bracts
    ang = 2 * math.pi * u
    bract = np.maximum.reduce([np.clip(1 - np.abs(((ang + k * 2 * math.pi / 6) % (2 * math.pi / 6)) - math.pi / 6)
                                       / (math.pi / 6), 0, 1) for k in range(1)])
    reach = 0.10 + 0.05 * bract
    cal = np.clip((reach - v) / 0.03, 0, 1)[..., None]
    col = col * (1 - cal) + hex_lin(P["calyx"]) * (0.8 + 0.3 * blotch[..., None]) * cal
    height = height * (1 - cal[..., 0]) + (0.75 + 0.15 * bract) * cal[..., 0]
    rough = rough * (1 - cal[..., 0]) + 0.8 * cal[..., 0]
    # stalk scar
    scar = np.clip((0.025 - v) / 0.02, 0, 1)
    col = col * (1 - scar[..., None]) + hex_lin("2A1C10") * scar[..., None]
    # cavity occlusion (between the ridges, at the calyx edge)
    ao = 1.0 - 0.18 * (1 - ridge) - 0.25 * cal[..., 0] * (1 - cal[..., 0]) * 4
    return col, np.clip(height, 0, 1), np.clip(rough, 0.05, 1), np.clip(ao, 0, 1)


def normal_from_height(h, strength_u, strength_v):
    """DirectX normal map (green down) from a height field; u wraps (seam), v clamps (poles)."""
    du = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength_u
    dv = np.zeros_like(h)
    dv[1:-1] = (h[2:] - h[:-2]) * strength_v
    n = np.stack([-du, -dv, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    rgb = n * 0.5 + 0.5
    rgb[..., 1] = 1.0 - rgb[..., 1]  # OpenGL -> DirectX (Unreal), like scripts/props/barque.py
    return rgb


def save(arr, path):
    Image.fromarray((np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)).transpose(Image.FLIP_TOP_BOTTOM).save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--res", type=int, default=1024)
    ap.add_argument("--preview", action="store_true")
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    tiles = []
    for k, var in enumerate(VARIANTS):
        col, h, r, ao = paint(var, a.res, 100 + 17 * k)
        # u spans the circumference (~0.66 m), v the length (~0.36 m): stronger slope along v per pixel
        nrm = normal_from_height(h, strength_u=6.0, strength_v=3.3)
        orm = np.stack([ao, r, np.zeros_like(r)], -1)
        d = lin_srgb(col)
        save(d, OUT / f"T_Coconut_{var}_D.png")
        save(nrm, OUT / f"T_Coconut_{var}_N.png")
        save(orm, OUT / f"T_Coconut_{var}_ORM.png")
        tiles.append(np.concatenate([d, nrm], axis=0))
        print(f"[coconut] {var}: D / N / ORM {a.res}px -> {OUT}")
    if a.preview:
        prev = np.concatenate(tiles, axis=1)
        p = ROOT / "art" / "_previews" / "coconut_textures.png"
        Image.fromarray((np.clip(prev, 0, 1) * 255).astype(np.uint8)).resize((prev.shape[1] // 2, prev.shape[0] // 2)).save(p)
        print("[coconut] preview", p)


if __name__ == "__main__":
    main()
