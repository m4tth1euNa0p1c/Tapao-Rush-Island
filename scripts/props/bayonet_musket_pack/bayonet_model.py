"""Socket bayonet of the bayonet musket (T-334): parts on the bone "bayonet" of the musket skeleton, in the musket's
weapon space (metres, +X muzzle, +Z up, -Y lock side), built with the musket kit (scripts/props/musket_pack/kit.py) and
its baked looks (looks.py): the socket in the musket's browned iron, the blade in a brighter polished steel.

  - socket (douille): a tube slipped over the muzzle, behind the lug that doubles as the front sight, with its rear
    bridge ring and the slot of the lug;
  - neck (coude): cranked from the lower left of the socket down to the blade;
  - blade: triangular section (a flat face toward the barrel, a spine below), tapering to the point, running forward
    UNDER the muzzle and 2 cm to the left of the bore (the ramrod still slides along its channel, the right hand works
    the muzzle during the reload).
"""
from __future__ import annotations

import math

import bmesh
import bpy
from mathutils import Vector

import kit as K
import looks as L
import spec as S

BLADE = 'TMP-Bayonet-Blade'


def materials(atlas_iron):
    """The blade look (the socket uses the musket's TMP-Musket-Iron)."""
    L.temp_material(BLADE, 'iron', atlas_iron, base='5E656C', bare='C3C9CC', rust='5C341B', wear=0.95, rust_amount=0.12, rough=0.24,
                    wear_scale=9.0)


def blade(name, coll, mat):
    """Triangular blade along +X from BLADE_ROOT, BLADE_LEN long: flat face up (toward the barrel), spine down."""
    B = K.bean()
    bm = bmesh.new()
    x0, y0, z0 = S.BLADE_ROOT
    w, h = S.BLADE_W, S.BLADE_H
    rows = []
    n = 18
    for i in range(n + 1):
        t = i / n
        # a slow taper, quicker over the last fifth to a sharp point
        k = (1.0 - t) ** 0.8 if t < 0.8 else (1.0 - 0.8) ** 0.8 * (1.0 - (t - 0.8) / 0.2) ** 1.3
        k = max(k, 0.0)
        x = x0 + S.BLADE_LEN * t
        # a hollow along the flat face: the top face is two facets meeting a little below the edges (fuller)
        tri = [(-w * 0.5 * k, h * 0.34 * k), (0.0, h * 0.22 * k), (w * 0.5 * k, h * 0.34 * k), (0.0, -h * 0.66 * k)]
        if i == n:
            rows.append(bm.verts.new((x + 0.004, y0, z0)))
        else:
            rows.append([bm.verts.new((x, y0 + y, z0 + z)) for y, z in tri])
    for a, b in zip(rows, rows[1:]):
        if isinstance(b, list):
            m = len(a)
            for j in range(m):
                bm.faces.new((a[j], a[(j + 1) % m], b[(j + 1) % m], b[j]))
        else:
            m = len(a)
            for j in range(m):
                bm.faces.new((a[j], a[(j + 1) % m], b))
    bm.faces.new(rows[0][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B.obj_from_bm(name, bm, mat, coll, smooth=False)


def build_bayonet(coll, iron):
    """The parts of the bone "bayonet" (list of objects)."""
    B = K.bean()
    V = K.V
    bz = S.BORE_Z
    r_in, r_out = S.SOCKET_R
    x0, x1 = S.SOCKET_X
    parts = []
    # the socket: an annulus turned about the bore axis, open at both ends
    profile = [(x0, r_in), (x0, r_out), (x1, r_out), (x1, r_in), (x0, r_in)]
    parts.append(K.lathe('Bayonet_Socket', profile, iron, coll, center_yz=(0.0, bz), segs=32))
    parts.append(K.ring('Bayonet_Bridge', (x0 + 0.0016, 0.0, bz), r_out + 0.0002, 0.0014, iron, coll, axis='X', segs=32, minor_segs=8))
    parts.append(K.ring('Bayonet_FrontRing', (x1 - 0.0012, 0.0, bz), r_out - 0.0002, 0.0009, iron, coll, axis='X', segs=32, minor_segs=6))
    # the lug's slot along the top (the zig-zag slot of the socket bayonets, straight part)
    parts.append(B.box('Bayonet_Slot', (0.5985 + 0.008, 0.0, bz + r_out + 0.0003), (0.020, 0.0042, 0.0012), 'TMP-Musket-DarkIron', coll, bevel=0))
    # the neck: from the lower left of the socket down and forward to the blade root
    a = math.atan2(S.BLADE_ROOT[2] - bz, S.BLADE_ROOT[1])
    s0 = V(0.611, r_out * math.cos(a) * 0.92, bz + r_out * math.sin(a) * 0.92)
    root = V(*S.BLADE_ROOT)
    pts = [s0, s0.lerp(root, 0.35) + V(0.004, 0.001, 0.0), s0.lerp(root, 0.72) + V(0.003, 0.001, 0.0), root + V(0.004, 0.0, 0.0)]
    parts.append(B.tube('Bayonet_Neck', pts, [0.0050, 0.0044, 0.0040, 0.0036], iron, coll, segs=14, squash=0.82))
    parts.append(B.blob('Bayonet_Shoulder', root + V(0.002, 0.0, 0.0005), (0.0062, 0.0090, 0.0050), BLADE, coll, seg=16, ring=8))
    parts.append(blade('Bayonet_Blade', coll, BLADE))
    return parts
