"""Atoll ground props: fallen coconuts, dry fronds, coral blocks (feo), coral rubble, driftwood.

Object space: +Z up, resting on z=0 (slightly sunk). Material slots documented per builder.
"""
import math

import numpy as np

from . import palms
from .geo import UP, MeshBuilder, Noise, add_tube, align_z, icosphere, norm, rot_axis, smoothstep, spline, stamp

_ICO3 = icosphere(3)
_ICO1 = icosphere(1)


def coconut(seed, maturity):
    """Fallen nut (slot 0 = nut material)."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    co, q, uv = palms.NUT_MESH
    M = align_z(norm(np.array([1.0, rng.normal(0, 0.2), rng.uniform(-0.15, 0.15)])))
    stamp(mb, co * rng.uniform(0.95, 1.15), q, M, np.array([0, 0, 0.085]), uv, palms.nut_color(rng, maturity), 0)
    return mb


def dry_frond(seed, length=4.6):
    """Dead frond lying on the ground (slot 1 = coconut leaf material, slot 0 unused)."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    palms.frond(mb, rng, np.array([0.0, 0.0, 0.05]), 0.0, 0.02, length, 1.0, dead=False, lying=True, bend=0.0,
                n_side=55)
    return mb


def coral_block(seed, size=1.0):
    """Weathered coral boulder (slot 0 = box-projected coral rock)."""
    rng = np.random.default_rng(seed)
    nz = Noise(seed)
    co, tri = _ICO3
    p = co * 1.3 + rng.uniform(0, 50, 3)
    r = 1 + 0.55 * (nz.fbm3(p, 4) - 0.5) + 0.22 * (nz.fbm3(p * 3.1, 3) - 0.5)
    pits = np.clip(nz.fbm3(p * 6.0, 2) - 0.62, 0, None) * 1.2  # porous holes
    v = co * (r - pits)[:, None]
    v *= np.array([rng.uniform(0.85, 1.2), rng.uniform(0.6, 1.0), rng.uniform(0.45, 0.8)]) * size * 0.5
    zmin = v[:, 2].min()
    flat = zmin * 0.45
    v[:, 2] = np.where(v[:, 2] < flat, flat + (v[:, 2] - flat) * 0.25, v[:, 2])
    v[:, 2] -= v[:, 2].min() + 0.12 * size  # sunk into the ground
    v = v @ rot_axis(UP, rng.uniform(0, 6.28)).T
    t = smoothstep(0.3 * size, -0.05 * size, v[:, 2])[:, None]
    bright = rng.uniform(0.9, 1.15)
    tint = (1 - t) * np.array([1.0, 1.0, 1.0]) * bright + t * np.array([0.55, 0.58, 0.45])
    mb = MeshBuilder()
    base = mb.verts(v, box_uv(v, tri, 0.7), np.concatenate([tint, np.zeros((len(v), 1))], 1))
    mb.faces(tri + base, 0)
    return mb


def box_uv(v, tri, tile):
    """Per-vertex box projection (dominant axis of the smoothed normal), in tiles of `tile` meters."""
    fn = np.cross(v[tri[:, 1]] - v[tri[:, 0]], v[tri[:, 2]] - v[tri[:, 0]])
    vn = np.zeros_like(v)
    for k in range(3):
        np.add.at(vn, tri[:, k], fn)
    ax = np.abs(vn).argmax(1)
    uv = np.where(ax[:, None] == 0, v[:, [1, 2]], np.where(ax[:, None] == 1, v[:, [0, 2]], v[:, [0, 1]]))
    return uv / tile


def rubble(seed, kind="chunk"):
    """Coral fragment 5-25 cm (slot 0 = rubble material). 'chunk' or 'branch' (staghorn piece)."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    shade = rng.uniform(0.55, 0.78)  # linear albedo of bleached coral
    pink = rng.random() < 0.2
    col = np.array([shade, shade * (0.94 if pink else 0.98), shade * (0.92 if pink else 0.95), 0.0], np.float32)
    if kind == "chunk":
        nz = Noise(seed)
        co, tri = _ICO1
        p = co * 2.0 + rng.uniform(0, 20, 3)
        r = 1 + 1.1 * (nz.fbm3(p, 3) - 0.5)
        v = co * r[:, None] * np.array([rng.uniform(0.04, 0.09), rng.uniform(0.025, 0.06), rng.uniform(0.015, 0.035)])
        v[:, 2] -= v[:, 2].min() + 0.01
        base = mb.verts(v, box_uv(v, tri, 0.3), col)
        mb.faces(tri + base, 0)
    else:
        L = rng.uniform(0.06, 0.15)
        a = np.array([0, 0, 0.012])
        b = a + norm(np.array([1.0, rng.normal(0, 0.3), 0.05])) * L
        add_tube(mb, spline([a, (a + b) / 2 + rng.normal(0, 0.01, 3), b], 5), np.linspace(0.013, 0.008, 5), 5, 0,
                 col, 1.0, 1.0, cap=True)
        for _ in range(int(rng.integers(1, 3))):
            s = a + (b - a) * rng.uniform(0.3, 0.7)
            d = norm(np.array([rng.uniform(0.3, 1), rng.choice([-1, 1]) * rng.uniform(0.4, 1), 0.3]))
            add_tube(mb, [s, s + d * L * rng.uniform(0.3, 0.5)], np.array([0.009, 0.006]), 5, 0, col, 1.0, 1.0,
                     cap=True)
    return mb


def driftwood(seed):
    """Bleached log washed up on the beach (slot 0 = driftwood)."""
    rng = np.random.default_rng(seed)
    nz = Noise(seed)
    mb = MeshBuilder()
    L = rng.uniform(2.2, 3.8)
    pts = np.array([[x, nz.n2(x * 0.5, 0.3) * 0.25, 0.0] for x in np.linspace(-L / 2, L / 2, 6)])
    r0 = rng.uniform(0.11, 0.17)
    C = spline(pts, 24)
    rad = r0 * (1 + 0.15 * np.sin(np.linspace(0, 9, 24))) * np.linspace(1.0, 0.75, 24)
    C[:, 2] = rad * 0.75
    add_tube(mb, C, rad, 10, 0, None, 1.0, 1.0, cap=True)
    for _ in range(int(rng.integers(1, 4))):
        i = int(rng.integers(4, 20))
        d = norm(np.array([rng.uniform(-0.3, 0.3), rng.choice([-1, 1]), rng.uniform(0.0, 0.6)]))
        add_tube(mb, [C[i], C[i] + d * rng.uniform(0.25, 0.6)], np.array([rad[i] * 0.45, rad[i] * 0.2]), 6, 0,
                 None, 1.0, 1.0, cap=True)
    return mb


def import_lambis(path):
    """Import the Poly Haven Lambis shell glTF and return its mesh object (joined, at origin)."""
    import bpy

    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    obj = meshes[0]
    for o in new:
        if o is not obj and o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    obj.parent = None
    obj.location = (0, 0, 0)
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    return obj
