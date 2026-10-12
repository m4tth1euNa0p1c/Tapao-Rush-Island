"""Generates the blood masks of the tiki creatures (T-153) in art/textures/vfx/: made by us, procedural, no external
source. Reproducible (fixed seeds). numpy + Pillow.

Outputs (8-bit grayscale PNG, linear masks, import without sRGB, compression Masks / Grayscale):
- T_Blood_Splat_A/B/C.png (512): ground splats, 1 = blood. A = round drop with satellites, B = splash elongated along
  +X (a hit coming from the left), C = scattered droplets.
- T_Blood_Pool.png (512): distance field of a pool, 1 at its centre falling to 0 at its noisy rim. The decal material
  keeps the texels above (1 - growth): the pool spreads from the centre as growth goes 0 -> 1.
- T_Blood_Droplet.png (128): soft drop for the Niagara sprites (centre 1, smooth rim).
- T_Blood_Noise.png (256): tiling value noise to break the flat colour (darker clots, thinner edges).

Usage: python scripts/vfx/gen_blood_textures.py
"""
from pathlib import Path

import numpy as np
from PIL import Image

OUT = Path(__file__).resolve().parents[2] / "art" / "textures" / "vfx"


def value_noise(size, cells, rng, tiling=True):
	"""Smooth value noise in [0, 1], size x size, cells x cells lattice (tiles when tiling)"""
	grid = rng.random((cells + 1, cells + 1))
	if tiling:
		grid[-1, :] = grid[0, :]
		grid[:, -1] = grid[:, 0]
	coords = np.linspace(0, cells, size, endpoint=not tiling)
	i = np.floor(coords).astype(int).clip(0, cells - 1)
	f = coords - i
	f = f * f * (3 - 2 * f)
	y0, x0 = np.meshgrid(i, i, indexing="ij")
	fy, fx = np.meshgrid(f, f, indexing="ij")
	a, b = grid[y0, x0], grid[y0, x0 + 1]
	c, d = grid[y0 + 1, x0], grid[y0 + 1, x0 + 1]
	return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(size, rng, octaves=5, base=4, tiling=True):
	"""Fractal noise in [0, 1]"""
	total, amp, norm = np.zeros((size, size)), 1.0, 0.0
	for octave in range(octaves):
		total += value_noise(size, base * 2 ** octave, rng, tiling) * amp
		norm += amp
		amp *= 0.5
	return total / norm


def grid(size):
	"""Coordinates in [-1, 1]"""
	axis = np.linspace(-1, 1, size)
	y, x = np.meshgrid(axis, axis, indexing="ij")
	return x, y


def blob(x, y, cx, cy, rx, ry, angle=0.0):
	"""Signed field of an ellipse: > 0 inside, 1 at its centre"""
	ca, sa = np.cos(angle), np.sin(angle)
	dx, dy = x - cx, y - cy
	u, v = (dx * ca + dy * sa) / rx, (-dx * sa + dy * ca) / ry
	return 1.0 - np.sqrt(u * u + v * v)


def to_mask(field, softness=0.04):
	"""Field to an antialiased 0..1 mask"""
	return np.clip(field / softness + 0.5, 0, 1)


def save(name, array):
	OUT.mkdir(parents=True, exist_ok=True)
	Image.fromarray((np.clip(array, 0, 1) * 255).astype(np.uint8), "L").save(OUT / f"{name}.png")
	print("wrote", OUT / f"{name}.png")


def splat(size, seed, kind):
	rng = np.random.default_rng(seed)
	x, y = grid(size)
	edge = (fbm(size, rng, tiling=False) - 0.5) * 0.35
	field = np.full((size, size), -1.0)
	if kind == "A":
		field = np.maximum(field, blob(x, y, 0, 0, 0.42, 0.38) + edge)
		satellites = 14
	elif kind == "B":
		field = np.maximum(field, blob(x, y, -0.2, 0, 0.5, 0.26, 0.05) + edge)
		# the splash thrown along +X: a tail and droplets getting smaller
		for k in range(9):
			t = 0.15 + k * 0.09
			field = np.maximum(field, blob(x, y, t, rng.normal(0, 0.06), 0.06 - k * 0.004, 0.04 - k * 0.003, 0) + edge * 0.3)
		satellites = 10
	else:
		satellites = 30
		field = np.maximum(field, blob(x, y, 0, 0, 0.18, 0.16) + edge)
	for _ in range(satellites):
		angle = rng.uniform(0, 2 * np.pi) if kind != "B" else rng.normal(0, 0.5)
		dist = rng.uniform(0.45, 0.9) if kind != "C" else rng.uniform(0.1, 0.9)
		r = rng.uniform(0.012, 0.045)
		field = np.maximum(field, blob(x, y, np.cos(angle) * dist, np.sin(angle) * dist, r, r * rng.uniform(0.7, 1.0)) + edge * 0.15)
	mask = to_mask(field)
	# thicker in the middle, thin at the rim: the material reads it for opacity and darkness
	thickness = np.clip(field * 3.0, 0, 1) * 0.35 + 0.65
	return mask * thickness * (1.0 - 0.25 * (x * x + y * y) ** 0.5).clip(0.5, 1)


def pool(size, seed):
	rng = np.random.default_rng(seed)
	x, y = grid(size)
	r = np.sqrt(x * x + y * y)
	warp = (fbm(size, rng, base=3, tiling=False) - 0.5) * 0.45
	# 1 at the centre, 0 at the rim (r ~ 0.85), the rim lobed by the noise
	field = 1.0 - (r + warp * r) / 0.85
	return np.clip(field, 0, 1) ** 0.8


def droplet(size):
	x, y = grid(size)
	r = np.sqrt(x * x + y * y)
	return np.clip(1.0 - r, 0, 1) ** 0.6 * to_mask(0.92 - r, 0.08)


def main():
	save("T_Blood_Splat_A", splat(512, 153001, "A"))
	save("T_Blood_Splat_B", splat(512, 153002, "B"))
	save("T_Blood_Splat_C", splat(512, 153003, "C"))
	save("T_Blood_Pool", pool(512, 153004))
	save("T_Blood_Droplet", droplet(128))
	save("T_Blood_Noise", fbm(256, np.random.default_rng(153005), octaves=5, base=4, tiling=True))


if __name__ == "__main__":
	main()
