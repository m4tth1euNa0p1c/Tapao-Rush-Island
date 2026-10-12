"""Ammunition props of the musket pack v2 (T-246): static meshes, metres, pivot at the bottom centre, one baked atlas
each (looks.py), a convex UCX_<Name>_00 collision written by export_pack.py.

  SM_Ammo_MusketCartridge  paper cartridge lying on its side (the same cartridge the reload holds on the bone "cartridge")
  SM_Ammo_CartridgeBox     leather cartridge box, flap open, wooden block with 20 cartridges standing in their holes
  SM_Ammo_PowderHorn       cow horn with a wooden base plug, brass ring, brass spout and wooden stopper, leather strap
  SM_Ammo_LeadBall         0.75" lead ball (1.7 cm) with its casting sprue and seam
"""
import math

import bpy
from mathutils import Matrix, Vector

import kit as K
import looks as L
import musket_model as M

AMMO = {
    'SM_Ammo_MusketCartridge': ('Ammo_Cartridge', 512),
    'SM_Ammo_CartridgeBox': ('Ammo_CartridgeBox', 1024),
    'SM_Ammo_PowderHorn': ('Ammo_PowderHorn', 1024),
    'SM_Ammo_LeadBall': ('Ammo_LeadBall', 512),
}


def mats(atlas, *looks, **params):
    """Temporary look materials of one atlas; returns {look: material name}."""
    out = {}
    for look in looks:
        name = 'TMP-%s-%s' % (atlas, look)
        L.temp_material(name, look, atlas, **params.get(look, {}))
        out[look] = name
    return out


def pivot_bottom_centre(ob, keep_xy=False):
    """Moves the mesh so that the bounding box's bottom centre (or its bottom, keep_xy) is the object origin."""
    me = ob.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    shift = Vector((0.0 if keep_xy else -(min(xs) + max(xs)) / 2, 0.0 if keep_xy else -(min(ys) + max(ys)) / 2, -min(zs)))
    me.transform(Matrix.Translation(shift))
    ob['size_m'] = [round(max(xs) - min(xs), 4), round(max(ys) - min(ys), 4), round(max(zs) - min(zs), 4)]
    return ob


def finish(name, parts, coll, keep_xy=False, transform=None):
    ob = K.join_parts(name, {'root': parts}, coll)
    if transform is not None:
        ob.data.transform(transform)
    pivot_bottom_centre(ob, keep_xy)
    ob['ammo'] = True
    return ob


def build_cartridge(coll):
    atlas, res = AMMO['SM_Ammo_MusketCartridge']
    L.register_atlas(atlas, 'M_Ammo_MusketCartridge', res)
    m = mats(atlas, 'paper', 'linen')
    parts = M.build_cartridge(coll, paper=m['paper'], linen=m['linen'], centre=Vector((0, 0, M.CARTRIDGE_RADIUS * 1.1)), prefix='Ammo_')
    return finish('SM_Ammo_MusketCartridge', parts, coll)


def build_lead_ball(coll):
    B = K.bean()
    atlas, res = AMMO['SM_Ammo_LeadBall']
    L.register_atlas(atlas, 'M_Ammo_LeadBall', res)
    m = mats(atlas, 'lead')
    r = 0.0085
    parts = [B.blob('Ammo_Ball', (0, 0, r), (r, r, r), m['lead'], coll, seg=32, ring=20)]
    parts.append(B.blob('Ammo_BallSprue', (0, 0, 2 * r - 0.0004), (0.0020, 0.0020, 0.0012), m['lead'], coll, seg=12, ring=6))
    parts.append(K.ring('Ammo_BallSeam', (0, 0, r), r - 0.0001, 0.00035, m['lead'], coll, axis='Z', segs=40, minor_segs=5))
    return finish('SM_Ammo_LeadBall', parts, coll)


def build_powder_horn(coll):
    B = K.bean()
    V = K.V
    atlas, res = AMMO['SM_Ammo_PowderHorn']
    L.register_atlas(atlas, 'M_Ammo_PowderHorn', res)
    m = mats(atlas, 'horn', 'wood', 'brass', 'leather', horn=dict(x0=0.01, x1=0.15), wood=dict(base='3A2213', dark='1E1109', light='5A3A22'))
    length = 0.155

    def axis(t):
        return Vector((length * t, 0.0, 0.040 * t * t))

    def radius(t):
        return 0.026 * (1 - t) ** 0.8 + 0.0046

    ts = [i / 16 for i in range(17)]
    body = B.tube('Ammo_Horn', [axis(t) for t in ts], [radius(t) for t in ts], m['horn'], coll, segs=28)
    parts = [body]
    # base: wooden plug set in the horn, a brass ring around the rim and a brass tack at the centre
    parts.append(B.blob('Ammo_HornPlug', (-0.0035, 0, 0), (0.0065, radius(0) - 0.0012, radius(0) - 0.0012), m['wood'], coll, seg=28, ring=12))
    parts.append(K.ring('Ammo_HornRing', (0.0045, 0, 0), radius(0.03) + 0.0005, 0.0022, m['brass'], coll, axis='X', segs=28, minor_segs=7))
    parts.append(K.disc('Ammo_HornTack', (-0.0098, 0, 0), 0.0032, 0.0014, (-1, 0, 0), m['brass'], coll, segs=14))
    # spout: brass tip and a turned wooden stopper, along the horn's end tangent
    tip = axis(1.0)
    d = (axis(1.0) - axis(0.96)).normalized()
    T = Matrix.Translation(tip) @ Vector((1, 0, 0)).rotation_difference(d).to_matrix().to_4x4()
    spout = [(-0.006, 0.0), (-0.006, 0.0060), (0.004, 0.0058), (0.010, 0.0052), (0.013, 0.0060), (0.016, 0.0056), (0.016, 0.0036), (0.000, 0.0036), (0.000, 0.0)]
    parts.append(K.lathe('Ammo_HornSpout', spout, m['brass'], coll, segs=16, transform=T))
    stopper = [(0.004, 0.0), (0.004, 0.0030), (0.017, 0.0032), (0.020, 0.0050), (0.026, 0.0055), (0.029, 0.0040), (0.029, 0.0)]
    parts.append(K.lathe('Ammo_HornStopper', stopper, m['wood'], coll, segs=16, transform=T))
    # strap: two leather loops around the horn and a slack strap between them along its back
    for t in (0.10, 0.82):
        c = axis(t)
        dt = (axis(t + 0.02) - axis(t - 0.02)).normalized()
        rot = Vector((1, 0, 0)).rotation_difference(dt).to_matrix().to_4x4()
        loop = K.ring('Ammo_HornLoop%d' % int(t * 100), (0, 0, 0), radius(t) + 0.0012, 0.0020, m['leather'], coll, axis='X', segs=24, minor_segs=6)
        loop.matrix_world = Matrix.Translation(c) @ rot
        parts.append(loop)
    s0, s1 = axis(0.10), axis(0.82)
    up0 = Vector((0, 0, radius(0.10) + 0.003))
    up1 = Vector((0, 0, radius(0.82) + 0.003))
    strap = [s0 + up0, s0 + up0 + Vector((0.02, 0, 0.012)), (s0 + s1) * 0.5 + Vector((0, 0, 0.034)), s1 + up1 + Vector((-0.015, 0, 0.012)), s1 + up1]
    parts.append(K.strap('Ammo_HornStrap', strap, 0.0075, 0.0014, m['leather'], coll, segs=10))
    # laid on its side: the curve in the ground plane, the strap loops on top
    return finish('SM_Ammo_PowderHorn', parts, coll, transform=Matrix.Rotation(math.radians(90), 4, 'X'))


def build_cartridge_box(coll):
    B = K.bean()
    V = K.V
    atlas, res = AMMO['SM_Ammo_CartridgeBox']
    L.register_atlas(atlas, 'M_Ammo_CartridgeBox', res)
    m = mats(atlas, 'leather', 'wood', 'brass', 'paper', 'linen', 'darkiron', leather=dict(base='2A1A10', worn='5C4028'))
    lx, ly, lz = 0.100, 0.072, 0.072          # outer body
    wall = 0.004
    parts = [B.box('Ammo_BoxBody', (0, 0, lz / 2), (lx, ly, lz), m['leather'], coll, bevel=0.0035)]
    # the open cavity: a dark wooden block fills it, its top 8 mm under the rim, with the holes of the block
    bx, by, bz = lx - 2 * wall, ly - 2 * wall, 0.050
    top = lz - 0.008
    parts.append(B.box('Ammo_BoxBlock', (0, 0, top - bz / 2), (bx, by, bz), m['wood'], coll, bevel=0.0015))
    parts.append(B.box('Ammo_BoxRim', (0, 0, lz - 0.004), (lx + 0.002, ly + 0.002, 0.008), m['leather'], coll, bevel=0.002))
    # flap: hinged on the back top edge, open and folded back
    flap_len = ly + 0.036
    hinge = Vector((0, ly / 2 + 0.001, lz))
    flap = B.box('Ammo_BoxFlap', (0, -flap_len / 2, 0.0022), (lx + 0.004, flap_len, 0.0044), m['leather'], coll, bevel=0.002)
    badge = K.disc('Ammo_BoxBadge', (0, -0.040, 0.0044), 0.013, 0.0016, (0, 0, 1), m['brass'], coll, segs=24)
    crown = B.blob('Ammo_BoxBadgeCrown', (0, -0.040, 0.0062), (0.0075, 0.0055, 0.0020), m['brass'], coll, seg=12, ring=6)
    strap = B.box('Ammo_BoxFlapStrap', (0, -flap_len + 0.012, 0.0030), (0.016, 0.020, 0.0040), m['leather'], coll, bevel=0.0012)
    buckle = K.ring('Ammo_BoxFlapBuckle', (0, -flap_len + 0.006, 0.0030), 0.0075, 0.0014, m['brass'], coll, axis='Z', segs=16, minor_segs=5)
    open_rot = Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(-150), 4, 'X') @ Matrix.Translation(-hinge)
    for ob in (flap, badge, crown, strap, buckle):
        ob.matrix_world = open_rot @ Matrix.Translation(hinge)
        parts.append(ob)
    # belt loops on the back
    for x in (-0.030, 0.030):
        parts.append(B.box('Ammo_BoxLoop%+d' % int(x * 1000), (x, ly / 2 + 0.004, lz * 0.55), (0.020, 0.006, 0.040), m['leather'], coll, bevel=0.0018))
    # 20 cartridges standing in the block: twisted end up, a dark ring marks every hole
    r = M.CARTRIDGE_RADIUS * 0.97
    for i in range(5):
        for j in range(4):
            x = (i - 2) * 0.0176
            y = (j - 1.5) * 0.0155
            base = Vector((x, y, top - 0.040))
            parts.append(K.ring('Ammo_BoxHole%d%d' % (i, j), (x, y, top + 0.0002), r + 0.0006, 0.0008, m['darkiron'], coll, axis='Z', segs=16, minor_segs=4))
            parts.append(B.tube('Ammo_BoxCartridge%d%d' % (i, j), [base, base + V(0, 0, 0.030), base + V(0, 0, 0.052), base + V(0, 0, 0.058)],
                                [r, r, r * 0.95, r * 0.7], m['paper'], coll, segs=12))
            parts.append(B.tube('Ammo_BoxTwist%d%d' % (i, j), [base + V(0, 0, 0.056), base + V(0, 0, 0.062), base + V(0, 0, 0.066)],
                                [r * 0.6, r * 0.45, r * 0.3], m['paper'], coll, segs=8))
            parts.append(B.tube('Ammo_BoxTie%d%d' % (i, j), [base + V(0, 0, 0.0485), base + V(0, 0, 0.0510)], [r * 1.05, r * 1.05], m['linen'], coll, segs=12))
    return finish('SM_Ammo_CartridgeBox', parts, coll, keep_xy=True)


def build_all(coll):
    return {ob.name: ob for ob in (build_cartridge(coll), build_cartridge_box(coll), build_powder_horn(coll), build_lead_ball(coll))}
