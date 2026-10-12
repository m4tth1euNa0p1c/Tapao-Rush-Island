"""Smoke flipbook of the firearms (owner 2026-10-11: "fix les visuels de fumée, pas assez réalistes, comme la vraie fumée").

The life of one puff of gunsmoke rendered as a real volume, 8 x 8 frames (64 frames of 128 px, 1024 x 1024 RGBA):
- the puff is a 3D density (128 x 128 x 80 voxels per frame): billowing lumps fused into one body, warped by a 3D noise,
  eaten at the edge by a turbulent noise (cauliflower outline, then holes and wisps as it ages);
- frame 0 a dense, compact puff; frame 63 a wide, thin, torn cloud: the lumps spread and rise, the noise expands with
  the cloud and boils (it moves through the noise with time);
- lighting: a sun above and a little in front, its light marched through the density (lit tops, shadowed core and
  underside, multiple scattering keeps thick smoke white rather than black), plus the sky above; then the view ray
  integrates the volume front to back. RGB is the light (not premultiplied), A the opacity of the volume.
The particle colour tints it in the material (grey powder smoke, white steam, sand dust). Deterministic (seeded).
Outputs art/textures/vfx/T_Smoke_Flipbook.png and the preview art/_previews/smoke_flipbook.png
(--frames 0,20,40,63 renders those frames only to art/_previews/smoke_frames.png, to tune quickly).

Usage: python scripts/vfx/smoke_flipbook.py [--frames 0,20,40,63] [--workers 10]
"""
import argparse
from multiprocessing import Pool
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/textures/vfx/T_Smoke_Flipbook.png"
PREVIEW = ROOT / "art/_previews/smoke_flipbook.png"
FRAMES_PREVIEW = ROOT / "art/_previews/smoke_frames.png"
GRID = 8
SIZE = 128
DEPTH = 80
SEED = 20261011
LUMPS = 16
# extinction of the smoke per frame width at density 1 (how opaque the young puff is)
SIGMA = 7.5
# toward the sun: up (image y is down), a little left and toward the viewer (z grows away from the viewer)
SUN = np.array([-0.35, -1.0, -0.55], dtype=np.float32)
SUN /= np.linalg.norm(SUN)
SUN_LIGHT = 1.0
SKY_LIGHT = 0.38


class ValueNoise3:
    """Smooth value noise on a periodic 3D lattice (trilinear with a quintic fade), flat-index gathers for speed"""

    def __init__(self, seed, period=64):
        rng = np.random.default_rng(seed)
        self.period = period
        self.grid = rng.random(period ** 3).astype(np.float32)

    def __call__(self, x, y, z):
        p = self.period
        xi, yi, zi = np.floor(x), np.floor(y), np.floor(z)
        xf, yf, zf = (x - xi).astype(np.float32), (y - yi).astype(np.float32), (z - zi).astype(np.float32)
        xi, yi, zi = xi.astype(np.int32) % p, yi.astype(np.int32) % p, zi.astype(np.int32) % p
        x1, y1, z1 = (xi + 1) % p, (yi + 1) % p, (zi + 1) % p
        u = xf * xf * xf * (xf * (xf * 6 - 15) + 10)
        v = yf * yf * yf * (yf * (yf * 6 - 15) + 10)
        w = zf * zf * zf * (zf * (zf * 6 - 15) + 10)
        g = self.grid
        a, b = xi * (p * p), x1 * (p * p)
        c, d = yi * p, y1 * p
        c000, c100 = g[a + c + zi], g[b + c + zi]
        c010, c110 = g[a + d + zi], g[b + d + zi]
        c001, c101 = g[a + c + z1], g[b + c + z1]
        c011, c111 = g[a + d + z1], g[b + d + z1]
        x00 = c000 + (c100 - c000) * u
        x10 = c010 + (c110 - c010) * u
        x01 = c001 + (c101 - c001) * u
        x11 = c011 + (c111 - c011) * u
        y0 = x00 + (x10 - x00) * v
        y1_ = x01 + (x11 - x01) * v
        return y0 + (y1_ - y0) * w


def fbm(noise, x, y, z, octaves=4, gain=0.5):
    total = np.zeros_like(x)
    amplitude, frequency, norm = 1.0, 1.0, 0.0
    for octave in range(octaves):
        # every octave shifted, so the lattices never line up
        total += amplitude * noise(x * frequency + 17.3 * octave, y * frequency + 5.1 * octave, z * frequency + 9.7 * octave)
        norm += amplitude
        amplitude *= gain
        frequency *= 2.03
    return total / norm


def billows(noise, x, y, z, octaves=5):
    """Folded fBm: rounded bulges with creases between them, the cauliflower look of smoke"""
    total = np.zeros_like(x)
    amplitude, frequency, norm = 1.0, 1.0, 0.0
    for octave in range(octaves):
        n = noise(x * frequency + 3.7 * octave, y * frequency + 11.9 * octave, z * frequency + 7.3 * octave)
        total += amplitude * (1.0 - np.abs(2.0 * n - 1.0))
        norm += amplitude
        amplitude *= 0.52
        frequency *= 2.01
    return total / norm


def lump_layout(seed):
    """Fixed billows of the puff, in units of its radius: centre, size, rise speed"""
    rng = np.random.default_rng(seed)
    directions = rng.normal(size=(LUMPS, 3))
    directions /= np.linalg.norm(directions, axis=1, keepdims=True)
    distances = np.concatenate([[0.0], rng.uniform(0.45, 0.95, LUMPS - 1)])
    centres = directions * distances[:, None]
    # flatter in depth than across: a puff seen from the side keeps its outline
    centres[:, 2] *= 0.8
    sizes = np.concatenate([[0.55], rng.uniform(0.24, 0.42, LUMPS - 1)])
    rises = rng.uniform(0.0, 0.35, LUMPS)
    return centres.astype(np.float32), sizes.astype(np.float32), rises.astype(np.float32)


def density_volume(t, noise_a, noise_b, layout):
    """Density of the puff at life t in [0, 1], array (DEPTH, SIZE, SIZE) = (z, y, x), frame units in [-0.5, 0.5]"""
    xs = (np.arange(SIZE, dtype=np.float32) + 0.5) / SIZE - 0.5
    zs = (np.arange(DEPTH, dtype=np.float32) + 0.5) / DEPTH - 0.5
    z, y, x = np.meshgrid(zs, xs, xs, indexing="ij")
    # the puff grows fast, then slows (drag)
    grow = 1.0 - (1.0 - t) ** 2.2
    radius = 0.25 + 0.16 * grow
    # noise space: in units of the puff's radius (the billows are carried by the expansion), moving with time (boiling)
    qx, qy, qz = x / radius, y / radius, z / radius
    boil = 1.1 * t
    warp = 0.32 + 0.12 * t
    wx = fbm(noise_b, qx * 1.3 + 3.1, qy * 1.3 + 7.4, qz * 1.3 + 1.3 + boil, octaves=3) - 0.5
    wy = fbm(noise_b, qx * 1.3 + 11.3, qy * 1.3 + 1.9 + boil, qz * 1.3 + 5.0, octaves=3) - 0.5
    wz = fbm(noise_b, qx * 1.3 + 21.7 + boil, qy * 1.3 + 13.1, qz * 1.3 + 2.2, octaves=3) - 0.5
    px, py, pz = qx + warp * wx, qy + warp * wy, qz + warp * wz
    # the lumps: they spread out a little and the light ones rise (image y is down)
    centres, sizes, rises = layout
    spread = 1.0 + 0.25 * t
    field = np.zeros_like(x)
    for (cx, cy, cz), size, rise in zip(centres, sizes, rises):
        cy = cy * spread - rise * t
        cx, cz = cx * spread, cz * spread
        s = size * (1.0 + 0.15 * t)
        field += np.exp(-((px - cx) ** 2 + (py - cy) ** 2 + (pz - cz) ** 2) / (s * s))
    body = 1.0 - np.exp(-1.25 * field)
    bulge = billows(noise_a, px * 1.45 + 20.0, py * 1.45 + 20.0 + 0.5 * boil, pz * 1.45 + 20.0 + boil)
    detail = fbm(noise_a, px * 4.2 + 40.0, py * 4.2 + 40.0 + boil, pz * 4.2 + 40.0, octaves=3)
    fine = billows(noise_b, px * 4.6 + 60.0, py * 4.6 + 60.0 + boil, pz * 4.6 + 60.0 + 0.7 * boil, octaves=3)
    # cauliflower outline early; later the edge frays into wisps and holes open
    shape = (body + (0.75 - 0.2 * t) * (bulge - 0.55) + 0.28 * (fine - 0.55)
             - (0.08 + 0.5 * t) * (1.0 - detail) - 0.42 - 0.08 * t)
    soft = 0.1 + 0.3 * t
    density = np.clip(shape / soft, 0.0, 1.0)
    density = density * density * (3.0 - 2.0 * density)
    # thinner as it spreads: the same smoke over a larger volume
    density *= 1.0 - 0.85 * t ** 0.75
    # never touch the borders of the frame or of the volume
    edge = np.minimum(np.minimum(0.5 - np.abs(x), 0.5 - np.abs(y)), (0.5 - np.abs(z)) * 1.6)
    density *= np.clip(edge / 0.05, 0.0, 1.0)
    return density.astype(np.float32)


def light_volume(density):
    """Light scattered toward the viewer at every voxel: sun through the smoke (with multiple scattering) and sky"""
    voxel = 1.0 / SIZE
    step = 1.5 * voxel
    optical = np.zeros_like(density)
    shift = SUN * step
    for k in range(1, 34):
        dx, dy = int(round(shift[0] * k * SIZE)), int(round(shift[1] * k * SIZE))
        dz = int(round(shift[2] * k * DEPTH))
        # what lies toward the sun (np.roll moves the content: minus the offset reads it)
        optical += np.roll(density, (-dz, -dy, -dx), axis=(0, 1, 2))
    optical *= step * SIGMA
    sun = 0.7 * np.exp(-optical) + 0.22 * np.exp(-0.3 * optical) + 0.08 * np.exp(-0.08 * optical)
    # sky: from above, through the smoke over the voxel, and blocked by the smoke around it
    above = (np.cumsum(density, axis=1) - density) * voxel * SIGMA
    around = gaussian_filter(density, sigma=(4.0, 6.0, 6.0)) * 0.35 * SIGMA
    sky = np.exp(-0.45 * above) * 0.6 + np.exp(-around) * 0.4
    return SUN_LIGHT * sun + SKY_LIGHT * sky


def render(density):
    """Front-to-back integration along z: light (not premultiplied) and opacity of the frame"""
    scattered = light_volume(density)
    alpha = 1.0 - np.exp(-density * SIGMA / DEPTH)
    through = np.cumprod(1.0 - alpha, axis=0)
    before = np.concatenate([np.ones_like(through[:1]), through[:-1]], axis=0)
    colour = (before * alpha * scattered).sum(axis=0)
    opacity = 1.0 - through[-1]
    light = colour / np.maximum(opacity, 1e-4) / (SUN_LIGHT + SKY_LIGHT)
    # where the smoke is almost nothing, its light is the light of the thin edge: lit
    light = np.where(opacity > 0.01, light, 0.9)
    return np.clip(light, 0.0, 1.0), np.clip(opacity, 0.0, 1.0)


def render_frame(index):
    t = index / (GRID * GRID - 1)
    noise_a, noise_b = ValueNoise3(SEED), ValueNoise3(SEED + 1)
    density = density_volume(t, noise_a, noise_b, lump_layout(SEED + 7))
    light, opacity = render(density)
    return index, light.astype(np.float32), opacity.astype(np.float32)


def over(light, opacity, background):
    a = opacity[..., None]
    rgb = np.repeat(light[..., None], 3, axis=2)
    return rgb * a + np.array(background, dtype=np.float32) * (1.0 - a)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", default="")
    parser.add_argument("--workers", type=int, default=10)
    args = parser.parse_args()
    indices = [int(i) for i in args.frames.split(",")] if args.frames else list(range(GRID * GRID))
    with Pool(min(args.workers, len(indices))) as pool:
        results = sorted(pool.map(render_frame, indices))
    if args.frames:
        # tuning strip: each frame over the sky and over a dark wall, twice the size
        tiles = []
        for _, light, opacity in results:
            pair = np.concatenate([over(light, opacity, (0.36, 0.5, 0.68)), over(light, opacity, (0.12, 0.1, 0.09))], axis=0)
            tiles.append(pair)
        strip = np.concatenate(tiles, axis=1)
        image = Image.fromarray((np.clip(strip, 0.0, 1.0) * 255).astype(np.uint8), "RGB")
        image.resize((image.width * 2, image.height * 2), Image.LANCZOS).save(FRAMES_PREVIEW)
        print("frames", indices, "->", FRAMES_PREVIEW)
        return
    sheet = np.zeros((GRID * SIZE, GRID * SIZE, 4), dtype=np.float32)
    for index, light, opacity in results:
        row, col = divmod(index, GRID)
        tile = sheet[row * SIZE:(row + 1) * SIZE, col * SIZE:(col + 1) * SIZE]
        tile[..., 0] = tile[..., 1] = tile[..., 2] = light
        tile[..., 3] = opacity
    OUT.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray((np.clip(sheet, 0.0, 1.0) * 255 + 0.5).astype(np.uint8), "RGBA").save(OUT)
    preview = over(sheet[..., 0], sheet[..., 3], (0.36, 0.5, 0.68))
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray((np.clip(preview, 0.0, 1.0) * 255).astype(np.uint8), "RGB").resize((768, 768), Image.LANCZOS).save(PREVIEW)
    print(f"flipbook {OUT} ({GRID}x{GRID} frames of {SIZE} px), preview {PREVIEW}")


if __name__ == "__main__":
    main()
