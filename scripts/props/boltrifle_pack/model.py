"""Bolt-action sniper rifle of the bean (T-334): geometry, bones and sockets (key positions in spec.py), built with the
musket kit (scripts/props/musket_pack/kit.py: lofts, lathes, straps) and the T-334 materials (pa_materials.py).

Parts by bake group and bone: parts[group][bone] = [objects]
  Wood   (2048)  stock with its cheek rest and semi pistol grip, handguard                          bone root
  Metal  (2048)  receiver (ring, rear bridge with the charger guide, rails), tang, magazine box,     root
                 floorplate and trigger guard, barrel with crown and bore, sights, bands and nose
                 cap, butt plate, swivels, sling and buckle, offset scope (brass tube, eyepiece and
                 objective bells, turrets, bands), side mount (plate, arms, rings)
                 bolt "in the white" (body, lugs, extractor, handle and knob), sleeve, cocking piece  bolt
                 trigger                                                                              trigger
  Ammo   (1024)  the steel strip of the stripper clip                                                 stripper_clip
                 five rounds (drawn brass cases, primers, jacketed bullets)                           clip_rounds
  Lens   (none)  the two lenses of the scope (M_BoltRifle_Lens, not baked)                             root
Weapon space (metres): +X muzzle, +Z up, -Y right side (bolt handle), origin = the right hand grip.
"""
from __future__ import annotations

import math

import bmesh
import bpy
from mathutils import Matrix, Vector

import kit as K
import pa_materials as PM
import spec as S

V = K.V
WALNUT, WALNUT_D = 'MAT-PA-Walnut', 'MAT-PA-WalnutDark'
BLUED, DARK, BRIGHT = 'MAT-PA-Blued', 'MAT-PA-DarkSteel', 'MAT-PA-BrightSteel'
BRASS, BRASS_B = 'MAT-PA-Brass', 'MAT-PA-BrassBright'
CASE, FIRED, COPPER, LEATHER, CARD = 'MAT-PA-CaseBrass', 'MAT-PA-CaseFired', 'MAT-PA-Copper', 'MAT-PA-Leather', 'MAT-PA-Cardboard'
LENS = 'M_BoltRifle_Lens'
BZ = S.BORE_Z
SY, SZ = S.SCOPE_Y, S.SCOPE_Z

# stock sections: (x, half width, z top, z bottom, exponent top, exponent bottom)
STOCK = [
    (-0.345, 0.0205, 0.044, -0.100, 2.6, 2.6), (-0.338, 0.0228, 0.047, -0.104, 2.6, 2.6),
    (-0.300, 0.0236, 0.049, -0.097, 2.5, 2.4), (-0.240, 0.0232, 0.050, -0.084, 2.5, 2.3),
    (-0.180, 0.0224, 0.050, -0.073, 2.5, 2.3), (-0.130, 0.0210, 0.046, -0.066, 2.4, 2.3),
    (-0.095, 0.0192, 0.034, -0.063, 2.3, 2.2), (-0.068, 0.0172, 0.025, -0.064, 2.2, 2.2),
    (-0.042, 0.0166, 0.023, -0.062, 2.2, 2.2), (-0.018, 0.0170, 0.025, -0.050, 2.3, 2.3),
    (0.008, 0.0182, 0.031, -0.036, 2.6, 2.6), (0.040, 0.0190, 0.040, -0.033, 3.2, 3.0),
    (0.175, 0.0190, 0.041, -0.033, 3.2, 3.0), (0.200, 0.0186, 0.041, -0.020, 3.2, 2.8),
    (0.245, 0.0180, 0.038, -0.006, 3.0, 2.5), (0.300, 0.0170, 0.035, 0.004, 3.0, 2.4),
    (0.420, 0.0160, 0.034, 0.010, 3.0, 2.4), (0.540, 0.0148, 0.033, 0.015, 3.0, 2.4),
    (0.566, 0.0138, 0.032, 0.018, 2.8, 2.4),
]
BARREL = [(0.238, 0.0130), (0.250, 0.0128), (0.262, 0.0124), (0.300, 0.0116), (0.420, 0.0105), (0.560, 0.0096), (0.700, 0.0089),
          (0.744, 0.0087)]


def barrel_r(x):
    return K.interp(x, BARREL)


def carve(x, y, z, top):
    """Receiver and barrel channels in the top of the stock, the tang inletting behind the receiver."""
    if not top:
        return z
    if 0.030 <= x <= 0.245:
        r = S.RECEIVER_R + 0.0008
        if abs(y) < r:
            z = min(z, BZ - math.sqrt(r * r - y * y))
    elif x > 0.245:
        r = barrel_r(min(x, 0.744)) + 0.0006
        if abs(y) < r:
            z = min(z, BZ - math.sqrt(r * r - y * y))
    elif -0.024 < x < 0.030 and abs(y) < 0.0072:
        z = min(z, 0.0215)
    return z


def materials():
    PM.ensure_all()
    lens = PM.lens(LENS)
    return lens


def ring_x(name, x, y, z, radius, minor, mat, coll, segs=32):
    return K.ring(name, (x, y, z), radius, minor, mat, coll, axis='X', segs=segs, minor_segs=8)


def round_parts(prefix, x0, z, coll, y=0.0, case_mat=CASE, bullet=True):
    """One rimless round along +X, base at x0, axis at (y, z): drawn case, primer, jacketed bullet (bean scale ~0.75)."""
    parts = []
    case = [(x0, 0.0), (x0, 0.0052), (x0 + 0.0010, 0.0057), (x0 + 0.0016, 0.0050), (x0 + 0.0030, 0.0050), (x0 + 0.0036, 0.0055),
            (x0 + 0.0340, 0.0052), (x0 + 0.0390, 0.0036), (x0 + 0.0455, 0.0034)]
    if bullet:
        case += [(x0 + 0.0455, 0.0)]
    else:
        case += [(x0 + 0.0455, 0.0031), (x0 + 0.0440, 0.0030), (x0 + 0.0060, 0.0030), (x0 + 0.0060, 0.0)]
    parts.append(K.lathe(prefix + 'Case', case, case_mat, coll, center_yz=(y, z), segs=18))
    parts.append(K.disc(prefix + 'Primer', (x0 - 0.0001, y, z), 0.0021, 0.0006, (-1, 0, 0), BRASS_B if bullet else FIRED, coll, segs=12))
    if bullet:
        b = [(x0 + 0.0440, 0.0), (x0 + 0.0440, 0.0031), (x0 + 0.0510, 0.0031), (x0 + 0.0570, 0.0024), (x0 + 0.0612, 0.0010), (x0 + 0.0620, 0.0)]
        parts.append(K.lathe(prefix + 'Bullet', b, COPPER, coll, center_yz=(y, z), segs=16))
    return parts


def build_clip(coll, prefix='Clip_'):
    """The stripper clip seated in the charger guide (rest geometry of the bones stripper_clip / clip_rounds)."""
    B = K.bean()
    strip, rounds = [], []
    z0, p = S.CLIP_Z0, S.CLIP_PITCH
    lo, hi = z0 - 0.0075, z0 + 4 * p + 0.0075
    zc = (lo + hi) * 0.5
    strip.append(B.box(prefix + 'Spine', (S.CLIP_X - 0.0002, 0.0, zc), (0.0012, 0.0136, hi - lo), DARK, coll, bevel=0.0003))
    for s in (-1, 1):
        strip.append(B.box(prefix + f'Lip{s:+d}', (S.CLIP_X + 0.0012, s * 0.0064, zc), (0.0030, 0.0008, hi - lo), DARK, coll, bevel=0.0002))
    strip.append(B.box(prefix + 'Spring', (S.CLIP_X + 0.0006, 0.0, zc), (0.0006, 0.0060, (hi - lo) * 0.86), BRIGHT, coll, bevel=0.0))
    for i in range(5):
        rounds += round_parts(f'{prefix}Round{i}_', S.CLIP_X + 0.0008, z0 + i * p, coll)
    return strip, rounds


def build_rifle(coll):
    """parts[group][bone], bones, sockets, params (all positions from spec.py)."""
    B = K.bean()
    materials()
    parts = {g: {} for g in ('Wood', 'Metal', 'Ammo', 'Lens')}

    def add(group, bone, ob):
        parts[group].setdefault(bone, []).append(ob)
        return ob

    W = lambda ob: add('Wood', 'root', ob)          # noqa: E731
    M = lambda ob: add('Metal', 'root', ob)         # noqa: E731

    # ------------------------------------------------------------------------------------------- wood
    W(K.section_loft('Rifle_Stock', STOCK, WALNUT, coll, segs=72, subsurf=1, shape=carve))
    cheek = [(-0.262, 0.0060, 0.044, 0.006, 2.2, 2.2), (-0.240, 0.0115, 0.047, 0.004, 2.4, 2.4), (-0.160, 0.0118, 0.047, 0.004, 2.4, 2.4),
             (-0.120, 0.0080, 0.042, 0.010, 2.2, 2.2), (-0.108, 0.0030, 0.036, 0.016, 2.0, 2.0)]
    ob = K.section_loft('Rifle_CheekRest', cheek, WALNUT_D, coll, segs=32, subsurf=1)
    ob.location.y = 0.0200
    W(ob)
    hand = [(0.282, 0.0122, BZ + 0.0150, BZ + 0.0035, 2.4, 6.0), (0.300, 0.0128, BZ + 0.0158, BZ + 0.0035, 2.4, 6.0),
            (0.455, 0.0118, BZ + 0.0148, BZ + 0.0035, 2.4, 6.0), (0.472, 0.0112, BZ + 0.0138, BZ + 0.0035, 2.4, 6.0)]
    W(K.section_loft('Rifle_Handguard', hand, WALNUT, coll, segs=40, subsurf=1))

    # ------------------------------------------------------------------------------------- receiver
    ring = [(0.155, 0.0), (0.155, 0.0148), (0.158, 0.0165), (0.236, 0.0165), (0.240, 0.0150), (0.240, 0.0)]
    M(K.lathe('Rifle_ReceiverRing', ring, BLUED, coll, center_yz=(0.0, BZ), segs=32))
    bridge = [(0.034, 0.0), (0.034, 0.0148), (0.037, 0.0160), (0.080, 0.0160), (0.083, 0.0150), (0.083, 0.0)]
    M(K.lathe('Rifle_RearBridge', bridge, BLUED, coll, center_yz=(0.0, BZ), segs=32))
    for s in (-1, 1):
        # charger guide: two jaws on the front of the bridge, the slot of the clip between them
        M(B.box(f'Rifle_ChargerGuide{s:+d}', (0.0815, s * 0.0084, BZ + 0.0160), (0.0050, 0.0030, 0.0070), BLUED, coll, bevel=0.0006))
    M(B.box('Rifle_RailL', (0.119, 0.0135, BZ - 0.0060), (0.072, 0.0060, 0.0260), BLUED, coll, bevel=0.0010))
    M(B.box('Rifle_RailR', (0.119, -0.0135, BZ - 0.0115), (0.072, 0.0060, 0.0150), BLUED, coll, bevel=0.0010))
    M(B.box('Rifle_MagBox', (0.120, 0.0, 0.0), (0.068, 0.0190, 0.0480), DARK, coll, bevel=0.0))
    M(K.section_loft('Rifle_Tang', [(-0.024, 0.0030, 0.0235, 0.0170, 2.0, 2.0), (-0.010, 0.0066, 0.0245, 0.0170, 2.0, 2.0),
                                    (0.034, 0.0072, 0.0260, 0.0170, 2.0, 2.0)], BLUED, coll, segs=20, subsurf=0))
    M(K.disc('Rifle_TangScrew', (-0.004, 0.0, 0.0248), 0.0028, 0.0010, (0, 0, 1), BRIGHT, coll, segs=12))
    M(B.box('Rifle_BoltStop', (0.046, 0.0168, BZ + 0.0060), (0.022, 0.0030, 0.0060), BLUED, coll, bevel=0.0008))
    M(B.box('Rifle_RecoilLug', (0.200, 0.0, BZ - 0.0190), (0.012, 0.0140, 0.0060), BLUED, coll, bevel=0.0006))
    # floorplate and trigger guard: one blued piece under the stock
    M(B.box('Rifle_Floorplate', (0.118, 0.0, -0.0345), (0.100, 0.0210, 0.0034), BLUED, coll, bevel=0.0012))
    M(K.disc('Rifle_FloorplateCatch', (0.073, 0.0, -0.0362), 0.0030, 0.0012, (0, 0, -1), BRIGHT, coll, segs=12))
    guard = [V(-0.034, 0, -0.0565), V(-0.016, 0, -0.0520), V(-0.006, 0, -0.0470), V(0.000, 0, -0.0540), V(0.014, 0, -0.0590),
             V(0.034, 0, -0.0605), V(0.052, 0, -0.0570), V(0.062, 0, -0.0480), V(0.066, 0, -0.0380), V(0.072, 0, -0.0350)]
    M(K.strap('Rifle_TriggerGuard', guard, [0.0030, 0.0050, 0.0060, 0.0060, 0.0062, 0.0062, 0.0060, 0.0058, 0.0055, 0.0050], 0.0016,
              BLUED, coll, segs=12))
    M(K.disc('Rifle_GuardScrew', (-0.026, 0.0, -0.0575), 0.0026, 0.0010, (0, -0.2, -1), BRIGHT, coll, segs=12))

    # ----------------------------------------------------------------------------------------- barrel
    prof = [(0.238, 0.0)] + BARREL + [(0.746, 0.0082), (0.746, 0.0046), (0.745, 0.0040), (0.716, 0.0040), (0.716, 0.0)]
    M(K.lathe('Rifle_Barrel', prof, BLUED, coll, center_yz=(0.0, BZ), segs=28))
    fs = S.IRON_FRONT
    M(B.box('Rifle_FrontSightBase', (fs[0] - 0.002, 0.0, BZ + 0.0098), (0.014, 0.0080, 0.0050), BLUED, coll, bevel=0.0012))
    M(B.box('Rifle_FrontSight', (fs[0], 0.0, fs[2] - 0.0040), (0.0040, 0.0016, 0.0080), BLUED, coll, bevel=0.0003))
    rs = S.IRON_REAR
    M(B.box('Rifle_RearSightBase', (rs[0], 0.0, BZ + 0.0132), (0.022, 0.0120, 0.0040), BLUED, coll, bevel=0.0010))
    M(B.box('Rifle_RearSightLeaf', (rs[0] + 0.002, 0.0, rs[2] - 0.0020), (0.0060, 0.0120, 0.0040), BLUED, coll, bevel=0.0004))
    M(B.box('Rifle_RearSightNotch', (rs[0] + 0.002, 0.0, rs[2] - 0.0002), (0.0062, 0.0018, 0.0010), DARK, coll, bevel=0.0))
    band = [(0.457, 0.0176, BZ + 0.0170, 0.0060, 3.0, 2.6), (0.467, 0.0176, BZ + 0.0170, 0.0060, 3.0, 2.6)]
    M(K.section_loft('Rifle_Band', band, BLUED, coll, segs=40, subsurf=0))
    nose = [(0.554, 0.0152, BZ + 0.0110, 0.0150, 3.0, 2.6), (0.570, 0.0146, BZ + 0.0108, 0.0170, 3.0, 2.6)]
    M(K.section_loft('Rifle_NoseCap', nose, BLUED, coll, segs=40, subsurf=0))
    M(K.ring('Rifle_SwivelFront', (0.462, 0.0, 0.0010), 0.0072, 0.0014, BLUED, coll, axis='Y', segs=18, minor_segs=6))
    M(K.ring('Rifle_SwivelRear', (-0.220, 0.0, -0.0860), 0.0072, 0.0014, BLUED, coll, axis='Y', segs=18, minor_segs=6))
    M(B.box('Rifle_SwivelRearBase', (-0.220, 0.0, -0.0790), (0.012, 0.0060, 0.0040), BLUED, coll, bevel=0.0008))
    # butt plate and screws
    butt = [(-0.3510, 0.0206, 0.0445, -0.1005, 2.6, 2.6), (-0.3450, 0.0206, 0.0445, -0.1005, 2.6, 2.6)]
    M(K.section_loft('Rifle_ButtPlate', butt, BLUED, coll, segs=48, subsurf=1))
    for i, z in enumerate((0.022, -0.078)):
        M(K.disc(f'Rifle_ButtScrew{i}', (-0.3512, 0.0, z), 0.0030, 0.0010, (-1, 0, 0), BRIGHT, coll, segs=12))
    # sling and its buckle
    sling = [V(0.462, 0, -0.0060), V(0.400, 0, -0.0300), V(0.280, 0, -0.0580), V(0.140, 0, -0.0720), V(0.000, 0, -0.0820),
             V(-0.120, 0, -0.0900), V(-0.212, 0, -0.0940)]
    M(K.strap('Rifle_Sling', sling, 0.0095, 0.0018, LEATHER, coll, segs=12))
    M(K.ring('Rifle_SlingBuckle', (0.200, 0.0, -0.0660), 0.0110, 0.0016, BRASS_B, coll, axis='Y', segs=16, minor_segs=6))

    # ------------------------------------------------------------------------------------- the scope
    tube = [(0.0235, 0.0), (0.0235, 0.0120), (0.0205, 0.0128), (0.0205, 0.0152), (0.0245, 0.0157), (0.0590, 0.0157), (0.0630, 0.0150),
            (0.0690, 0.0115), (0.0730, 0.0112), (0.2680, 0.0112), (0.2760, 0.0120), (0.2900, 0.0162), (0.3220, 0.0172), (0.3300, 0.0172),
            (0.3300, 0.0160), (0.3260, 0.0152), (0.3260, 0.0)]
    M(K.lathe('Scope_Tube', tube, BRASS, coll, center_yz=(SY, SZ), segs=36))
    for x, r in ((0.0245, 0.0159), (0.0730, 0.0114), (0.2680, 0.0114), (0.3290, 0.0174)):
        M(ring_x(f'Scope_Band{int(x * 1000)}', x, SY, SZ, r, 0.0010, BRASS_B, coll))
    M(ring_x('Scope_FocusRing', 0.0420, SY, SZ, 0.0158, 0.0012, BRASS_B, coll))
    # turrets: elevation on top, windage on the left (outer) side, on a saddle
    M(B.box('Scope_Saddle', (0.170, SY, SZ), (0.030, 0.0250, 0.0250), BRASS, coll, bevel=0.0050))
    M(B.tube('Scope_Elevation', [V(0.170, SY, SZ + 0.0100), V(0.170, SY, SZ + 0.0205)], [0.0078, 0.0078], BRASS, coll, segs=18))
    M(K.disc('Scope_ElevationCap', (0.170, SY, SZ + 0.0207), 0.0080, 0.0016, (0, 0, 1), BRASS_B, coll, segs=20))
    M(B.tube('Scope_Windage', [V(0.170, SY + 0.0100, SZ), V(0.170, SY + 0.0195, SZ)], [0.0068, 0.0068], BRASS, coll, segs=18))
    M(K.disc('Scope_WindageCap', (0.170, SY + 0.0197, SZ), 0.0070, 0.0016, (0, 1, 0), BRASS_B, coll, segs=20))
    # side mount: plate on the left of the receiver, two arms, two rings with clamp screws
    M(B.box('Mount_Plate', (0.132, 0.0182, BZ + 0.0040), (0.130, 0.0040, 0.0200), BLUED, coll, bevel=0.0010))
    for i, x in enumerate((0.098, 0.228)):
        M(B.box(f'Mount_Arm{i}', (x, 0.0222, (BZ + 0.0130 + SZ - 0.0110) * 0.5), (0.010, 0.0070, SZ - 0.0110 - BZ - 0.0110), BLUED, coll,
                bevel=0.0010))
        M(ring_x(f'Mount_Ring{i}', x, SY, SZ, 0.0124, 0.0022, BLUED, coll))
        M(K.disc(f'Mount_RingScrew{i}', (x, SY, SZ + 0.0146), 0.0024, 0.0012, (0, 0, 1), BRIGHT, coll, segs=12))
        M(K.disc(f'Mount_PlateScrew{i}', (x, 0.0203, BZ + 0.0040), 0.0024, 0.0010, (0, 1, 0), BRIGHT, coll, segs=12))
    # lenses: their own material, not baked
    add('Lens', 'root', K.disc('Scope_LensEye', (0.0233, SY, SZ), 0.0119, 0.0005, (-1, 0, 0), LENS, coll, segs=32))
    add('Lens', 'root', K.disc('Scope_LensObjective', (0.3262, SY, SZ), 0.0151, 0.0005, (1, 0, 0), LENS, coll, segs=32))

    # ------------------------------------------------------------------------------------------- bolt
    Bo = lambda ob: add('Metal', 'bolt', ob)        # noqa: E731
    Bo(K.cylinder_x('Bolt_Body', S.BOLT_REAR_X + 0.008, S.BOLT_FACE_X, S.BOLT_R, (0.0, BZ), BRIGHT, coll, segs=20))
    for s in (-1, 1):
        Bo(B.box(f'Bolt_Lug{s:+d}', (S.BOLT_FACE_X - 0.006, s * 0.0092, BZ), (0.0080, 0.0040, 0.0060), BRIGHT, coll, bevel=0.0006))
    Bo(B.box('Bolt_Extractor', (S.BOLT_FACE_X - 0.030, -0.0088, BZ + 0.0010), (0.050, 0.0016, 0.0040), BRIGHT, coll, bevel=0.0004))
    sleeve = [(0.012, 0.0), (0.012, 0.0080), (0.016, 0.0096), (0.042, 0.0096), (0.042, 0.0)]
    Bo(K.lathe('Bolt_Sleeve', sleeve, BLUED, coll, center_yz=(0.0, BZ), segs=20))
    cocking = [(0.002, 0.0), (0.002, 0.0045), (0.006, 0.0060), (0.014, 0.0060), (0.014, 0.0)]
    Bo(K.lathe('Bolt_CockingPiece', cocking, BLUED, coll, center_yz=(0.0, BZ), segs=16))
    Bo(B.box('Bolt_Safety', (0.024, -0.0040, BZ + 0.0118), (0.0080, 0.0026, 0.0080), BRIGHT, coll, bevel=0.0008))
    k = Vector(S.KNOB)
    Bo(B.tube('Bolt_Handle', [V(S.BOLT_HANDLE_X, -0.0060, BZ), V(S.BOLT_HANDLE_X + 0.001, -0.0220, BZ - 0.0020),
                              V(k.x, -0.0400, BZ - 0.0060), V(k.x, k.y + 0.0060, k.z + 0.0010)], [0.0042, 0.0036, 0.0033, 0.0032],
              BRIGHT, coll, segs=14))
    Bo(B.blob('Bolt_Knob', k, (0.0085, 0.0085, 0.0085), BRIGHT, coll, seg=20, ring=12))

    # ----------------------------------------------------------------------------------------- trigger
    add('Metal', 'trigger', B.tube('Rifle_Trigger', [V(0.024, 0, -0.026), V(0.0245, 0, -0.036), V(0.022, 0, -0.045), V(0.017, 0, -0.050)],
                                   [0.0032, 0.0030, 0.0027, 0.0022], BLUED, coll, segs=10, squash=0.6))

    # ------------------------------------------------------------------------------------------ ammo
    strip, rounds = build_clip(coll)
    for ob in strip:
        add('Ammo', 'stripper_clip', ob)
    for ob in rounds:
        add('Ammo', 'clip_rounds', ob)

    bones = {n: (Vector(h), Vector(t), p) for n, (h, t, p) in S.BONES.items()}
    sockets = {n: (v[0], Vector(v[1])) for n, v in S.SOCKETS.items()}
    return {'parts': parts, 'bones': bones, 'sockets': sockets,
            'params': {'bore_z': BZ, 'muzzle_x': S.MUZZLE_X, 'scope_axis_m': [0.0, SY, SZ], 'eye_relief_m': S.EYE_RELIEF,
                       'bolt_travel_m': S.BOLT_TRAVEL, 'bolt_lift_deg': S.BOLT_LIFT_DEG, 'clip_pitch_m': S.CLIP_PITCH,
                       'push_depth_m': S.PUSH_DEPTH},
            'scope': {'axis_y_m': SY, 'axis_z_m': SZ, 'eyepiece_x_m': S.SCOPE_X0, 'objective_x_m': S.SCOPE_X1, 'eye_relief_m': S.EYE_RELIEF,
                      'eye_point_m': list(S.SCOPE_EYE), 'eyepiece_radius_m': 0.0120, 'objective_radius_m': 0.0152,
                      'offset': 'left of the bore (+Y): the stripper clip loads from the top, the bolt handle lifts on the right',
                      'lens_material': LENS},
            'iron_sights': {'rear_m': list(S.IRON_REAR), 'front_m': list(S.IRON_FRONT)}}


def build_casing(coll):
    """A fired case lying along +X (mouth +X), base at the origin."""
    return round_parts('Casing_', 0.0, 0.0, coll, case_mat=FIRED, bullet=False)


def build_clip_box(coll):
    """Cardboard box of clips: an open tray (two clips seen on top), the lid folded back. Box space: X along the rounds,
    Y across, Z up, bottom at z = 0."""
    B = K.bean()
    parts = []
    L, Wd, H, t = 0.074, 0.130, 0.030, 0.0016
    parts.append(B.box('ClipBox_Bottom', (0.0, 0.0, t * 0.5), (L, Wd, t), CARD, coll, bevel=0.0004))
    for s in (-1, 1):
        parts.append(B.box(f'ClipBox_SideX{s:+d}', (s * (L - t) * 0.5, 0.0, H * 0.5), (t, Wd, H), CARD, coll, bevel=0.0004))
        parts.append(B.box(f'ClipBox_SideY{s:+d}', (0.0, s * (Wd - t) * 0.5, H * 0.5), (L, t, H), CARD, coll, bevel=0.0004))
    lid = B.box('ClipBox_Lid', (0.0, 0.0, 0.0), (L + 0.002, Wd + 0.002, t), CARD, coll, bevel=0.0004)
    lid.data.transform(Matrix.Translation(((L + 0.002) * 0.5, 0.0, 0.0)))          # spans x 0..L from its hinge
    lid.matrix_world = Matrix.Translation((-L * 0.5, 0.0, H)) @ Matrix.Rotation(math.radians(-112), 4, 'Y')   # open, up and back
    parts.append(lid)
    return parts
