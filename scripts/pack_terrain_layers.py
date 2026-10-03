"""Pack atoll ground layers for the multi-layer terrain material.

For each layer: <name>_ch.png  RGB = graded base color (sRGB), A = height (linear)
                <name>_n.png   OpenGL normal
Grading pulls scanned textures toward Tuamotu colors (white/grey coral, not brown).

Usage: python scripts/pack_terrain_layers.py [--size 2048]
Output: art/textures/terrain_layers/
"""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image

ART = Path(__file__).resolve().parent.parent / "art"
G = ART / "textures" / "ground"
OUT = ART / "textures" / "terrain_layers"

# name: (folder, saturation, brightness, tint rgb, contrast)
LAYERS = {
    "sand_dry": ("Ground033", 0.40, 1.10, (1.00, 0.985, 0.95), 1.00),
    "sand_wet": ("aerial_beach_01", 0.60, 1.10, (1.00, 1.00, 0.97), 1.05),
    "wrack": ("shell_floor_01", 0.55, 1.30, (1.00, 0.98, 0.93), 1.10),
    "soil": ("forrest_sand_01", 0.50, 1.08, (1.00, 0.985, 0.95), 1.05),
    "litter": ("Ground072", 0.55, 1.02, (1.00, 0.98, 0.94), 1.00),
    "rubble": ("coral_gravel", 0.22, 1.45, (1.00, 0.99, 0.96), 1.15),
    "feo": ("coral_ground_02", 0.30, 1.05, (0.96, 0.98, 0.92), 1.10),
}


def find(folder: Path, *keys: str) -> Path:
    for k in keys:
        hits = sorted(p for p in folder.iterdir() if k.lower() in p.name.lower())
        if hits:
            return hits[0]
    raise FileNotFoundError(f"{folder}: {keys}")


def load(p: Path, size: int, mode: str) -> np.ndarray:
    im = Image.open(p)
    if im.mode in ("I;16", "I;16B", "I"):  # 16-bit height maps
        a = np.asarray(im, np.float32)
        a = (a - a.min()) / max(1e-6, a.max() - a.min())
        im = Image.fromarray((a * 255).astype(np.uint8))
    im = im.convert(mode).resize((size, size), Image.LANCZOS)
    return np.asarray(im, np.float32) / 255.0


def grade(rgb: np.ndarray, sat: float, bright: float, tint, contrast: float) -> np.ndarray:
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    mean = float(lum.mean())
    rgb = lum[..., None] + (rgb - lum[..., None]) * sat
    rgb = (rgb - mean) * contrast + mean
    rgb = rgb * bright * np.array(tint, np.float32)
    return np.clip(rgb, 0, 1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=2048)
    size = ap.parse_args().size
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (folder, sat, bright, tint, contrast) in LAYERS.items():
        f = G / folder
        col = grade(load(find(f, "_diff_", "_Color"), size, "RGB"), sat, bright, tint, contrast)
        h = load(find(f, "_disp_", "_Displacement"), size, "L")
        h = (h - h.min()) / max(1e-6, h.max() - h.min())  # full 0..1 range for height blending
        ch = np.dstack([col, h])
        Image.fromarray((ch * 255 + 0.5).astype(np.uint8), "RGBA").save(OUT / f"{name}_ch.png")
        n = load(find(f, "_nor_gl_", "_NormalGL"), size, "RGB")
        Image.fromarray((n * 255 + 0.5).astype(np.uint8), "RGB").save(OUT / f"{name}_n.png")
        print(f"  ok  {name:9s} <- {folder}  mean rgb {np.round(col.reshape(-1, 3).mean(0), 2)}")


if __name__ == "__main__":
    main()
