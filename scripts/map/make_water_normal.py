"""Genere une normal map d'eau tuilable (convention DirectX, attendue par Unreal) a partir d'un spectre de houle.

Usage: python scripts/map/make_water_normal.py [--size 2048] [--out art/textures/water/T_Water_N.png]
Heightfield = FFT inverse d'un spectre de type Phillips (vent constant) -> tuilable par construction.
"""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=2048)
    parser.add_argument("--out", default=str(ROOT / "art" / "textures" / "water" / "T_Water_N.png"))
    parser.add_argument("--seed", type=int, default=3)
    args = parser.parse_args()

    n = args.size
    rng = np.random.default_rng(args.seed)
    k = np.fft.fftfreq(n) * n                      # frequences entieres -> periodique sur la tuile
    kx, ky = np.meshgrid(k, k)
    kk = np.hypot(kx, ky)
    kk[0, 0] = 1.0
    wind = np.array([0.8, 0.6])                     # direction dominante (alizes)
    align = ((kx * wind[0] + ky * wind[1]) / kk) ** 2
    peak = n / 40.0                                 # longueur d onde dominante ~ 1/40 de la tuile
    phillips = np.exp(-1.0 / (kk / peak) ** 2) / kk ** 4 * (0.25 + 0.75 * align)
    phillips *= np.exp(-(kk / (n / 12.0)) ** 2)     # coupe les hautes frequences (eau de lagon calme)
    phillips[0, 0] = 0.0
    spectrum = np.sqrt(phillips) * (rng.standard_normal((n, n)) + 1j * rng.standard_normal((n, n)))
    height = np.real(np.fft.ifft2(spectrum))
    height /= np.abs(height).max()

    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    strength = 6.0
    nrm = np.stack([-dx * strength, -dy * strength, np.ones_like(height)], -1)  # Y vers le bas = DirectX
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(((nrm * 0.5 + 0.5) * 255).astype(np.uint8)).save(out)
    print(out, "slope max", float(np.abs(nrm[..., :2]).max()))


if __name__ == "__main__":
    main()
