"""Pirate grenado of the bean (T-253): hollow cast-iron sphere (8 cm, ~0.9 kg), wooden fuse plug with an iron collar
and a hemp fuse; a slow match (carried in the left hand) and the grenade bag (shown in Discard) on their own bones.

Weapon local frame: +X = the fingers of the right hand (forward), +Z up (the fuse), the right mitten centre at the
origin (socket_grip_r); the ball sits on the palm side (+Z) of the mitten. Same conventions as the pistol and the
musket packs (gun_root forward +X, up +Z) so AFirearmWeapon frames it the same way.

Bones: gun_root > body > fuse ; gun_root > match ; gun_root > bag.
Sockets (non deforming bones): socket_grip_r (gun_root), socket_body (body: ball centre, the projectile spawn),
socket_muzzle (fuse: the fuse tip, "muzzle" of the firearm logic: spark FX at Light), socket_match (match: the
ember), socket_bag (bag: its mouth, the pickup spawn at Release).
"""
import math

from mathutils import Vector

import common as C

R_BALL = 0.040
BALL = Vector((0.0, 0.0, 0.055))                    # ball centre, above the mitten centre
PLUG_TOP = Vector((0.0, 0.0, BALL.z + 0.053))
FUSE_TIP = PLUG_TOP + Vector((0.013, 0.0, 0.046))
MATCH_REST = Vector((0.0, 0.11, -0.02))             # bind pose: the match stands left of the hand
MATCH_LEN = 0.13
MATCH_TIP_REST = MATCH_REST + Vector((0.0, 0.0, MATCH_LEN))
BAG_REST = Vector((0.0, 0.0, -0.12))                # bottom of the bag (the held point); it hangs toward -Z at rest
BAG_LEN = 0.11
BAG_MOUTH_REST = BAG_REST + Vector((0.0, 0.0, -BAG_LEN))
MASS_KG = 0.9                                       # hollow shell (wall ~8 mm): not the solid-sphere volume


def materials():
    return dict(
        iron=C.mat_cast_iron('MAT-Grenade-Iron', seed=0.0),
        collar=C.mat_cast_iron('MAT-Grenade-Collar', base=(0.055, 0.050, 0.046), rust_amount=0.35, pit_scale=240.0, seed=3.1),
        wood=C.mat_wood('MAT-Grenade-Plug', light=(0.46, 0.30, 0.14), dark=(0.22, 0.12, 0.05), scale=40.0, axis=(0, 0, 1), seed=1.0),
        fuse=C.mat_cord('MAT-Grenade-Fuse', color=(0.55, 0.42, 0.22), dark=(0.26, 0.18, 0.08), twist=22.0),
        match=C.mat_cord('MAT-Grenade-Match', color=(0.30, 0.22, 0.11), dark=(0.12, 0.08, 0.04), twist=20.0,
                         charred_at=MATCH_TIP_REST, charred_len=0.03, ember=True),
        leather=C.mat_leather('MAT-Grenade-Bag', color=(0.24, 0.12, 0.06), dark=(0.10, 0.05, 0.025), seed=2.0),
        string=C.mat_cord('MAT-Grenade-BagString', color=(0.50, 0.40, 0.24), dark=(0.25, 0.18, 0.09), twist=24.0),
    )


def build_grenade(coll, only_grenade=False, offset=None, suffix=''):
    """Parts per bone (body, fuse, match, bag), sockets and bones of the grenade skeleton. only_grenade: the ball,
    plug and fuse alone (ammunition pickups), moved by offset (object location), names suffixed."""
    M = materials()
    parts = {'body': [], 'fuse': [], 'match': [], 'bag': []}
    V = C.V
    if only_grenade:
        res = _grenade_parts(coll, M, parts, V, suffix)
        for objs in parts.values():
            for ob in objs:
                ob.location = offset or V(0, 0, 0)
        return res
    _grenade_parts(coll, M, parts, V, suffix)
    _carry_parts(coll, M, parts, V)
    return _rig(parts)


def _grenade_parts(coll, M, parts, V, suffix=''):
    # --- body: cast sphere with its mould seam, the plug collar and the wooden plug
    body = parts['body']
    body.append(C.sphere('Grenade_Ball' + suffix, BALL, (R_BALL, R_BALL, R_BALL), M['iron'], coll, seg=56, ring=28))
    body.append(C.torus('Grenade_Seam' + suffix, BALL, R_BALL - 0.0002, 0.0012, M['collar'], coll, axis='Y', segs=56, minor=6))
    body.append(C.torus('Grenade_Collar' + suffix, (0.0, 0.0, BALL.z + 0.0385), 0.0125, 0.0032, M['collar'], coll, axis='Z', segs=28, minor=8))
    body.append(C.lathe('Grenade_Plug' + suffix, [(0.028, 0.0090), (0.037, 0.0097), (0.044, 0.0104), (0.049, 0.0101), (0.0528, 0.0084), (0.0535, 0.0045)],
                        (0.0, 0.0, BALL.z), M['wood'], coll, segs=24, cap_end=True))
    # --- fuse: hemp cord out of the plug, bending forward, frayed end
    fuse = parts['fuse']
    fuse.append(C.tube('Grenade_Fuse' + suffix, [PLUG_TOP - V(0, 0, 0.006), PLUG_TOP + V(0.001, 0, 0.014), PLUG_TOP + V(0.006, 0, 0.030), FUSE_TIP],
                       [0.0036, 0.0036, 0.0035, 0.0032], M['fuse'], coll, segs=12))
    fuse.append(C.sphere('Grenade_FuseEnd' + suffix, FUSE_TIP, (0.0040, 0.0040, 0.0048), M['fuse'], coll, seg=12, ring=6))
    return parts


def _carry_parts(coll, M, parts, V):
    # --- slow match: dark cord with a charred, glowing end
    match = parts['match']
    match.append(C.tube('Grenade_Match', [MATCH_REST, MATCH_REST + V(0.003, 0, 0.045), MATCH_REST + V(-0.002, 0, 0.09), MATCH_TIP_REST],
                        [0.0046, 0.0046, 0.0044, 0.0038], M['match'], coll, segs=12))
    match.append(C.sphere('Grenade_MatchEmber', MATCH_TIP_REST, (0.0044, 0.0044, 0.0050), M['match'], coll, seg=12, ring=6))
    # --- bag: small leather pouch held by its bottom, open mouth toward -Z, drawstring at the neck
    bag = parts['bag']
    bag.append(C.lathe('Grenade_Bag', [(0.0, 0.010), (-0.005, 0.026), (-0.018, 0.040), (-0.045, 0.047), (-0.072, 0.042), (-0.090, 0.031),
                                       (-0.100, 0.027), (-0.106, 0.031), (-0.110, 0.030), (-0.108, 0.026), (-0.092, 0.022), (-0.086, 0.006)],
                       BAG_REST, M['leather'], coll, segs=28, cap_start=True, cap_end=True))
    bag.append(C.torus('Grenade_BagString', BAG_REST + V(0, 0, -0.096), 0.0285, 0.0022, M['string'], coll, axis='Z', segs=28, minor=6))
    for k, (dx, dy) in enumerate(((0.024, 0.012), (0.018, -0.020))):
        bag.append(C.tube('Grenade_BagStringEnd%d' % k, [BAG_REST + V(dx, dy, -0.096), BAG_REST + V(dx * 1.35, dy * 1.35, -0.118),
                                                           BAG_REST + V(dx * 1.5, dy * 1.5, -0.135)], [0.0022, 0.0020, 0.0016], M['string'], coll, segs=8))
    return parts


def _rig(parts):
    V = C.V
    sockets = {
        'grip_r': ('gun_root', V(0, 0, 0)),
        'body': ('body', BALL.copy()),
        'muzzle': ('fuse', FUSE_TIP.copy()),
        'fuse': ('fuse', FUSE_TIP.copy()),       # alias of socket_muzzle: the registry of import_pirate_weapons.py names it socket_fuse
        'match': ('match', MATCH_TIP_REST.copy()),
        'bag': ('bag', BAG_MOUTH_REST.copy()),
    }
    bones = {
        'gun_root': (V(0, 0, 0), V(0.05, 0, 0), None),
        'body': (BALL.copy(), BALL + V(0.04, 0, 0), 'gun_root'),
        'fuse': (PLUG_TOP.copy(), FUSE_TIP.copy(), 'body'),
        'match': (MATCH_REST.copy(), MATCH_TIP_REST.copy(), 'gun_root'),
        'bag': (BAG_REST.copy(), BAG_MOUTH_REST.copy(), 'gun_root'),
    }
    return {'parts': parts, 'sockets': sockets, 'bones': bones, 'materials': materials()}


def build_skeletal_grenade(coll):
    """The grenade as a skeletal mesh: armature + one mesh weighted per part (rigid)."""
    res = build_grenade(coll)
    bone_names = list(res['bones'])
    arm = C.make_skeleton('SKEL-Grenade-Template', res['bones'], res['sockets'], coll)
    mesh = C.join_parts('SK_Bean_PirateGrenade', res['parts'], coll, bone_names=bone_names)
    C.pack_uvs(mesh)
    mesh.parent = arm
    mod = mesh.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    res.update(object=mesh, armature=arm, bone_names=bone_names)
    res['physics'] = {'mass_kg': MASS_KG, 'com': [round(v, 4) for v in BALL], 'note': 'hollow cast shell, wall ~8 mm'}
    return res


def pickup_points():
    """Points of the convex collision of the grenade on the ground (ball + plug), in weapon space."""
    pts = C.icosphere_points(BALL, R_BALL + 0.001, 1)
    pts += [Vector((0.0105 * math.cos(C.TAU * i / 8), 0.0105 * math.sin(C.TAU * i / 8), PLUG_TOP.z)) for i in range(8)]
    return pts
