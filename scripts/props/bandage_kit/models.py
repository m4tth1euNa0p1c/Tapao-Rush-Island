"""The bandage models of T-340 (lot L5): unbleached linen, simple readable shapes in the bean's style.

  SM_Bandage_Roll         the roll held in the right mitten: a linen cylinder (axis = +X, 5.5 cm wide, 6 cm across) with its
                          core hole, layer lines on the sides and a short loose end hanging down (-Z); pivot = roll centre =
                          socket grip; socket tail at the end of the loose end
  SM_Bandage_Roll_Pickup  the same roll lying on the ground on its side, the loose end flat in front (+X); pivot = centre of
                          the underside (Z = 0); one UCX box
  SM_Bandage_Wrapped      three overlapping turns of linen (helix) around the bean's left forearm, fitted by ray casts on the
                          sleeve cuff of bean_soldier.blend (Cuff_l + Sleeve_l, rest pose) + 2 mm; forearm axis = +X; pivot
                          = band centre on the forearm axis; the tucked end on top; no collision (attached)

Units: metres, Z up, front of the object = +X (kit.py exports). Every face carries its material and UVs in metres / TILE.
"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

import kit as K

MAT = 'M_Bandage_Linen'
ROLL = {'radius': 0.030, 'width': 0.055, 'core': 0.008, 'tail_len': 0.06, 'tail_th': 0.0022, 'segs': 28}
WRAP = {'turns': 3, 'width': 0.032, 'advance': 0.011, 'layer': 0.0024, 'gap': 0.004, 'segs_per_turn': 40,
        'centre_t': 0.55,               # band centre along lowerarm_l (0 = elbow head, 1 = wrist tail)
        'thickness': 0.0022}


# ------------------------------------------------------------------------------------------------- roll
def _roll_body(mb, centre=Vector(), axis_rot=None):
    """Linen cylinder along X with a core hole; the sides get three rings of layer lines (small steps)"""
    R = axis_rot if axis_rot is not None else Matrix.Identity(3)
    r, w, core, segs = ROLL['radius'], ROLL['width'], ROLL['core'], ROLL['segs']
    tile = K.TILE[MAT]
    hw = w / 2
    # outer surface, slightly barrelled (soft fabric)
    rings = []
    profile = [(-hw, r * 0.965), (-hw * 0.6, r * 0.995), (0.0, r), (hw * 0.6, r * 0.995), (hw, r * 0.965)]
    for x, rr in profile:
        ring = []
        for i in range(segs):
            a = 2 * math.pi * i / segs
            ring.append(mb.vert(centre + R @ Vector((x, math.cos(a) * rr, math.sin(a) * rr))))
        rings.append(ring)
    circ = 2 * math.pi * r / tile
    for k in range(len(rings) - 1):
        for i in range(segs):
            j = (i + 1) % segs
            u0, u1 = i / segs * circ, (i + 1) / segs * circ
            v0, v1 = (profile[k][0] + hw) / tile, (profile[k + 1][0] + hw) / tile
            mb.face([rings[k][i], rings[k + 1][i], rings[k + 1][j], rings[k][j]], MAT,
                    [(u0, v0), (u0, v1), (u1, v1), (u1, v0)], smooth=True)
    # sides: concentric steps from the outer rim to the core (the wound layers), then the core tube
    radii = [r * 0.965, r * 0.80, r * 0.62, r * 0.45, core * 1.25, core]
    for side, x_end, first in ((-1, -hw, rings[0]), (1, hw, rings[-1])):
        prev = first
        for k, rr in enumerate(radii[1:], 1):
            x = x_end - side * (0.0012 * (k % 2))          # a small step every other ring: layer lines
            ring = [mb.vert(centre + R @ Vector((x, math.cos(2 * math.pi * i / segs) * rr, math.sin(2 * math.pi * i / segs) * rr)))
                    for i in range(segs)]
            for i in range(segs):
                j = (i + 1) % segs
                ids = [prev[i], prev[j], ring[j], ring[i]] if side > 0 else [prev[i], ring[i], ring[j], prev[j]]
                pts = [mb.verts[q] - centre for q in ids]
                uvs = [((R.transposed() @ p).y / tile, (R.transposed() @ p).z / tile) for p in pts]
                mb.face(ids, MAT, uvs, smooth=False)
            prev = ring
        # core tube to the middle (closed by a disc at x = 0)
        mid = [mb.vert(centre + R @ Vector((0.0, math.cos(2 * math.pi * i / segs) * core, math.sin(2 * math.pi * i / segs) * core)))
               for i in range(segs)]
        for i in range(segs):
            j = (i + 1) % segs
            ids = [prev[i], mid[i], mid[j], prev[j]] if side > 0 else [prev[i], prev[j], mid[j], mid[i]]
            mb.face(ids, MAT, [(i / segs, 0), (i / segs, 0.1), ((i + 1) / segs, 0.1), ((i + 1) / segs, 0)], smooth=True)
        disc = mid[::-1] if side > 0 else mid
        mb.face(disc, MAT, [(0.5 + 0.5 * math.cos(2 * math.pi * i / segs), 0.5 + 0.5 * math.sin(2 * math.pi * i / segs))
                            for i in range(segs)], smooth=False)


def _strip(mb, pts, normals, width, thickness, taper_end=0.0):
    """Flat band of linen along pts (normals = the band's face direction), width across, both faces and edges"""
    tile = K.TILE[MAT]
    n = len(pts)
    acc = [0.0]
    for i in range(1, n):
        acc.append(acc[-1] + (pts[i] - pts[i - 1]).length)
    rows = []
    for i in range(n):
        t = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
        nn = (normals[i] - t * normals[i].dot(t)).normalized()
        side = t.cross(nn).normalized()
        w = width * (1.0 - taper_end * max(0.0, (i - (n - 4)) / 3.0))
        hw, ht = w / 2, thickness / 2
        rows.append([mb.vert(pts[i] + side * sx * hw + nn * sz * ht) for sx, sz in ((-1, 1), (1, 1), (1, -1), (-1, -1))])
    for i in range(n - 1):
        a, b = rows[i], rows[i + 1]
        v0, v1 = acc[i] / tile, acc[i + 1] / tile
        for k in range(4):
            kk = (k + 1) % 4
            u0, u1 = (0.0, width / tile) if k % 2 == 0 else (0.0, thickness / tile)
            mb.face([a[k], b[k], b[kk], a[kk]], MAT, [(u0, v0), (u0, v1), (u1, v1), (u1, v0)], smooth=(k % 2 == 0))
    for ring, flip in ((rows[0], False), (rows[-1], True)):
        ids = ring[::-1] if flip else ring
        mb.face(ids, MAT, [(0, 0), (width / tile, 0), (width / tile, 0.02), (0, 0.02)])


def build_roll(coll, mats):
    """SM_Bandage_Roll: pivot at the roll centre; the loose end leaves the roll at its front (+X side... +Y) and hangs (-Z)"""
    mb = K.MeshBuilder('SM_Bandage_Roll', seed=3)
    _roll_body(mb)
    r, w = ROLL['radius'], ROLL['width']
    # loose end: leaves the outer surface tangentially at the front bottom (angle -60 deg), hangs and curls slightly
    pts, nors = [], []
    a0 = math.radians(-35)
    for k in range(7):                                     # on the roll surface, from a0 down to -90 deg
        a = a0 - math.radians(55) * k / 6
        rr = r + ROLL['tail_th'] * 0.6
        pts.append(Vector((0.0, math.cos(a) * rr, math.sin(a) * rr)))
        nors.append(Vector((0.0, math.cos(a), math.sin(a))))
    start = pts[-1]
    L = ROLL['tail_len']
    for k in range(1, 9):                                  # hanging, a gentle curl forward at the end
        s = k / 8
        pts.append(start + Vector((0.0, -0.010 * s * s + 0.004 * s, -L * s)))
        nors.append(Vector((0.0, 1.0, 0.25 * s)).normalized())
    _strip(mb, pts, nors, w * 0.92, ROLL['tail_th'], taper_end=0.0)
    ob = mb.build(coll, mats)
    sockets = {'grip': ((0.0, 0.0, 0.0), (0, 0, 0)), 'tail': (tuple(round(v, 4) for v in pts[-1]), (0, 0, 0))}
    info = {'roll_diameter_m': 2 * r, 'roll_width_m': w, 'tail_length_m': round(L + 0.03, 3),
            'look': 'lin écru, rouleau serré (lignes de couches sur les flancs, trou de mandrin), bout libre qui pend'}
    return ob, sockets, info


def build_pickup(coll, mats):
    """SM_Bandage_Roll_Pickup: the roll on its side (axis along Y), the loose end unrolled flat in front (+X)"""
    mb = K.MeshBuilder('SM_Bandage_Roll_Pickup', seed=5)
    r, w = ROLL['radius'], ROLL['width']
    rot = Matrix.Rotation(math.radians(90), 3, 'Z')        # roll axis X -> Y
    centre = Vector((0.0, 0.0, r * 0.985))                 # resting on its barrelled middle
    _roll_body(mb, centre, rot)
    pts, nors = [], []
    for k in range(9):                                     # from the front bottom of the roll, flat on the ground
        a = math.radians(-90 + 70 * (1 - k / 8)) if k < 8 else math.radians(-90)
        if k < 6:
            a = math.radians(-20 - 70 * k / 5)
            p = centre + Vector((math.cos(a) * (r + 0.0013), 0.0, math.sin(a) * (r + 0.0013)))
            nn = Vector((math.cos(a), 0.0, math.sin(a)))
        else:
            p = Vector((0.004 + 0.035 * (k - 5), 0.0, ROLL['tail_th'] / 2 + 0.0004))
            nn = Vector((0.0, 0.0, 1.0))
        pts.append(p)
        nors.append(nn)
    for k in range(1, 4):
        pts.append(Vector((0.11 + 0.03 * k, 0.004 * math.sin(k), ROLL['tail_th'] / 2 + 0.0004)))
        nors.append(Vector((0.0, 0.0, 1.0)))
    _strip(mb, pts, nors, w * 0.92, ROLL['tail_th'])
    lift = -min(v.z for v in mb.verts)                     # resting exactly on Z = 0
    mb.transform(Matrix.Translation((0.0, 0.0, lift)))
    centre = centre + Vector((0.0, 0.0, lift))
    ob = mb.build(coll, mats)
    st = K.mesh_stats(ob)
    lo, hi = Vector(st['min_m']), Vector(st['max_m'])
    ucx = [K.ucx_box('UCX_SM_Bandage_Roll_Pickup_00', ((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (hi.z + 0.0) / 2),
                     (hi.x - lo.x + 0.004, hi.y - lo.y + 0.004, hi.z + 0.002), coll)]
    sockets = {'grip': (tuple(round(v, 4) for v in centre), (0, 0, 0))}
    info = {'look': 'le rouleau couché sur le flanc, le bout déroulé à plat devant (+X)', 'rest_height_m': round(hi.z, 4)}
    return ob, ucx, sockets, info


# ------------------------------------------------------------------------------------------------- wrapped band
def forearm_frame(rig):
    """Rest frame of the band on lowerarm_l: X along the bone (elbow -> wrist), Z toward the back of the hand side (up at
    rest: the bone's own Z projected), origin on the bone at WRAP['centre_t']"""
    b = rig.bones['lowerarm_l']
    h, t = b.head_local, b.tail_local
    x = (t - h).normalized()
    up = Vector((0, -1, 0))                                # thumb side (front) as the band's +Z (the tucked end on it)
    z = (up - x * up.dot(x)).normalized()
    y = z.cross(x)
    M = Matrix((x, y, z)).transposed().to_4x4()
    M.translation = h.lerp(t, WRAP['centre_t'])
    return M


def sleeve_tree(names):
    vs, ps = [], []
    for n in names:
        o = bpy.data.objects.get(n)
        if o is None:
            continue
        me = o.data
        base = len(vs)
        vs += [o.matrix_world @ v.co for v in me.vertices]
        ps += [[base + i for i in p.vertices] for p in me.polygons]
    return BVHTree.FromPolygons(vs, ps)


def surface_radius(tree, Mframe, x, angle, rmax=0.2):
    """Distance from the frame axis to the outermost sleeve surface at (x along the axis, angle about it)"""
    o = Mframe @ Vector((x, 0.0, 0.0))
    d = (Mframe.to_3x3() @ Vector((0.0, math.cos(angle), math.sin(angle)))).normalized()
    start = o + d * rmax
    hit = tree.ray_cast(start, -d, rmax)
    if hit[0] is None:
        return None
    return rmax - hit[3]


def build_wrapped(coll, mats, tree, Mframe, name='SM_Bandage_Wrapped'):
    """Three turns (helix) fitted on the sleeve: radius(angle, x) = sleeve surface + gap + layer * turn"""
    p = WRAP
    n = p['turns'] * p['segs_per_turn']
    total = p['width'] + p['advance'] * (p['turns'] - 1)
    radii_used = []
    pts, nors = [], []
    surf_cache = {}

    def surf(x, a):
        key = (round(x, 4), round(a % (2 * math.pi), 3))
        if key not in surf_cache:
            vals = [surface_radius(tree, Mframe, x + dx, a) for dx in (-p['width'] / 2, -p['width'] / 4, 0.0, p['width'] / 4, p['width'] / 2)]
            vals = [v for v in vals if v is not None]
            surf_cache[key] = max(vals) if vals else 0.07
        return surf_cache[key]
    for i in range(n + 1):
        s = i / n
        a = 2 * math.pi * p['turns'] * s + math.radians(90)          # starts on top (+Z), turns toward +Y
        x = -total / 2 + p['width'] / 2 + p['advance'] * p['turns'] * s * (p['turns'] - 1) / p['turns']
        rr = surf(x, a) + p['gap'] + p['layer'] * p['turns'] * s + p['thickness'] / 2
        radii_used.append(rr)
        pts.append(Vector((x, math.cos(a) * rr, math.sin(a) * rr)))
        nors.append(Vector((0.0, math.cos(a), math.sin(a))))
    mb = K.MeshBuilder(name, seed=7)
    _strip(mb, pts, nors, p['width'], p['thickness'], taper_end=0.35)
    # the tucked end: a small folded flap over the last turn, on top
    a = 2 * math.pi * p['turns'] + math.radians(90)
    xe = pts[-1].x
    re_ = radii_used[-1] + p['thickness']
    flap = [Vector((xe - 0.004, math.cos(a + d) * re_, math.sin(a + d) * re_)) for d in (math.radians(-6), math.radians(-14), math.radians(-22))]
    flap_n = [Vector((0.0, math.cos(a + d), math.sin(a + d))) for d in (math.radians(-6), math.radians(-14), math.radians(-22))]
    _strip(mb, flap, flap_n, p['width'] * 0.55, p['thickness'])
    ob = mb.build(coll, mats)
    info = {'turns': p['turns'], 'band_width_m': p['width'], 'covered_length_m': round(total, 4),
            'inner_radius_m': [round(min(radii_used) - p['thickness'] / 2, 4), round(max(radii_used) - p['thickness'] / 2, 4)],
            'fitted_on': 'Cuff_l + Sleeve_l de bean_soldier.blend (pose de repos), écart 4 mm, 2,4 mm de plus par tour',
            'centre_along_lowerarm_l': p['centre_t']}
    return ob, info, surf


def fit_scale_fp(tree_fp, centre_x, centre_z, inner_radius_fn, x_extent, samples=36):
    """Radial scale of SM_Bandage_Wrapped so that it clears the first person cuff (FPArm_Cuff / Sleeve, arm along +X)"""
    M = Matrix.Translation((centre_x, 0.0, centre_z))
    worst = 0.0
    for i in range(samples):
        a = 2 * math.pi * i / samples
        for dx in (-x_extent / 2, 0.0, x_extent / 2):
            fp = surface_radius(tree_fp, M, dx, a)
            if fp is None:
                continue
            worst = max(worst, (fp + WRAP['gap']) / max(inner_radius_fn(dx, a), 1e-4))
    return worst
