"""Drum sub-machine gun "Mitraillette" of the bean (T-334): geometry, bones and sockets (key positions in spec.py),
built with the musket kit (scripts/props/musket_pack/kit.py) and the T-334 materials (pa_materials.py).

Parts by bake group and bone: parts[group][bone] = [objects]
  Wood   (2048)  straight walnut stock, pistol grip with finger grooves, vertical fore-grip            root
  Metal  (2048)  receiver (rounded top, cocking slot, ejection port, trigger frame, selector levers),  root
                 barrel boss, finned barrel, slotted compensator, sights, magazine well, butt plate,
                 stock ferrule, fore-grip band, sling swivels
                 cocking knob (brass) and its stem, the bolt seen in the ejection port                 bolt
                 trigger                                                                               trigger
                 magazine latch (brass tip)                                                            mag_latch
  Drum   (1024)  50 round drum: shell with ribs, feed tower, brass winding key and boss, rivets        drum
Weapon space (metres): +X muzzle, +Z up, -Y right side (ejection port), origin = the right hand on the pistol grip.
"""
from __future__ import annotations

import math

from mathutils import Matrix, Vector

import kit as K
import pa_materials as PM
import spec as S

V = K.V
WALNUT, WALNUT_D = 'MAT-PA-Walnut', 'MAT-PA-WalnutDark'
BLUED, DARK, BRIGHT = 'MAT-PA-Blued', 'MAT-PA-DarkSteel', 'MAT-PA-BrightSteel'
BRASS, BRASS_B = 'MAT-PA-Brass', 'MAT-PA-BrassBright'
FIRED = 'MAT-PA-CaseFired'
BZ = S.BORE_Z
RX0, RX1 = S.RECEIVER
RZ0, RZ1 = S.RECEIVER_Z
HW = S.RECEIVER_HW

STOCK = [(-0.040, 0.0140, 0.084, 0.040, 2.4, 2.4), (-0.060, 0.0142, 0.084, 0.034, 2.4, 2.4), (-0.120, 0.0160, 0.081, 0.012, 2.5, 2.4),
         (-0.200, 0.0180, 0.078, -0.010, 2.5, 2.4), (-0.280, 0.0195, 0.075, -0.028, 2.6, 2.5), (-0.312, 0.0198, 0.074, -0.034, 2.6, 2.5),
         (-0.318, 0.0188, 0.072, -0.032, 2.6, 2.6)]


def materials():
    PM.ensure_all()


def fins_profile(x0, x1, pitch, r_root, r_fin):
    prof = [(x0, 0.0), (x0, r_root)]
    x = x0 + 0.004
    while x + pitch <= x1:
        prof += [(x, r_root), (x + 0.0006, r_fin), (x + 0.0024, r_fin), (x + 0.0030, r_root)]
        x += pitch
    prof += [(x1, r_root), (x1, 0.0)]
    return prof


def build_gun(coll):
    B = K.bean()
    materials()
    parts = {g: {} for g in ('Wood', 'Metal', 'Drum')}

    def add(group, bone, ob):
        parts[group].setdefault(bone, []).append(ob)
        return ob

    W = lambda ob: add('Wood', 'root', ob)          # noqa: E731
    M = lambda ob: add('Metal', 'root', ob)         # noqa: E731

    # --------------------------------------------------------------------------------------------- wood
    W(K.section_loft('SMG_Stock', STOCK, WALNUT, coll, segs=64, subsurf=1))
    grip = [V(0.002, 0, RZ0 - 0.001), V(-0.004, 0, 0.004), V(-0.012, 0, -0.026), V(-0.019, 0, -0.046)]
    W(B.tube('SMG_PistolGrip', grip, [0.0150, 0.0155, 0.0152, 0.0142], WALNUT, coll, segs=20, squash=0.80))
    for i, z in enumerate((0.010, -0.010, -0.028)):
        x = -0.004 - (0.010 - z) * 0.27
        W(B.blob(f'SMG_GripFinger{i}', (x + 0.0125, 0.0, z), (0.0050, 0.0105, 0.0058), WALNUT, coll, seg=16, ring=8))
    fore = [V(0.258, 0, BZ - 0.009), V(0.261, 0, 0.022), V(0.266, 0, -0.016), V(0.270, 0, -0.044)]
    W(B.tube('SMG_ForeGrip', fore, [0.0128, 0.0140, 0.0145, 0.0132], WALNUT, coll, segs=20, squash=0.84))
    for i, z in enumerate((0.012, -0.004, -0.020, -0.034)):
        x = 0.261 + (0.022 - z) * 0.125
        W(B.blob(f'SMG_ForeFinger{i}', (x - 0.0120, 0.0, z), (0.0045, 0.0108, 0.0052), WALNUT_D, coll, seg=16, ring=8))

    # ----------------------------------------------------------------------------------------- receiver
    rec = [(RX0, HW, RZ1, RZ0, 2.0, 7.0), (RX0 + 0.004, HW, RZ1, RZ0, 2.0, 7.0), (RX1 - 0.004, HW, RZ1, RZ0, 2.0, 7.0), (RX1, HW - 0.001, RZ1 - 0.002, RZ0 + 0.002, 2.0, 7.0)]
    M(K.section_loft('SMG_Receiver', rec, BLUED, coll, segs=48, subsurf=0))
    M(B.box('SMG_CockingSlot', (0.093, 0.0, RZ1 + 0.0002), (0.098, 0.0056, 0.0028), DARK, coll, bevel=0.0))
    M(B.box('SMG_Port', (0.125, -HW - 0.0002, 0.067), (0.052, 0.0012, 0.020), DARK, coll, bevel=0.0))
    M(B.box('SMG_PortLip', (0.125, -HW - 0.0006, 0.0575), (0.056, 0.0012, 0.0022), BLUED, coll, bevel=0.0004))
    M(B.box('SMG_TriggerFrame', (0.012, 0.0, RZ0 - 0.0050), (0.110, 0.0230, 0.0100), BLUED, coll, bevel=0.0018))
    guard = [V(0.010, 0, RZ0 - 0.010), V(0.012, 0, 0.012), V(0.020, 0, 0.002), V(0.040, 0, -0.002), V(0.058, 0, 0.004),
             V(0.064, 0, 0.016), V(0.064, 0, RZ0 - 0.010)]
    M(K.strap('SMG_TriggerGuard', guard, 0.0045, 0.0016, BLUED, coll, segs=12))
    for i, x in enumerate((0.002, 0.036)):
        M(B.box(f'SMG_Selector{i}', (x, HW + 0.0012, 0.047), (0.004, 0.0024, 0.018), BLUED, coll, bevel=0.0008))
        M(K.disc(f'SMG_SelectorCap{i}', (x, HW + 0.0026, 0.054), 0.0036, 0.0016, (0, 1, 0), BRASS_B, coll, segs=14))
    M(K.disc('SMG_SelectorPivotL', (0.020, HW + 0.0002, 0.040), 0.0030, 0.0010, (0, 1, 0), BRIGHT, coll, segs=12))
    M(B.box('SMG_MagWell', (0.118, 0.0, RZ0 - 0.0040), (0.046, 0.0200, 0.0090), BLUED, coll, bevel=0.0012))
    M(B.box('SMG_RearSightBase', (S.SIGHT_REAR[0], 0.0, RZ1 + 0.0016), (0.016, 0.0120, 0.0036), BLUED, coll, bevel=0.0010))
    for s in (-1, 1):
        M(B.box(f'SMG_RearSightEar{s:+d}', (S.SIGHT_REAR[0], s * 0.0034, S.SIGHT_REAR[2] - 0.0020), (0.0060, 0.0030, 0.0050), BLUED, coll,
                bevel=0.0006))
    # stock ferrule and butt plate
    M(K.section_loft('SMG_Ferrule', [(-0.046, 0.0150, 0.0855, 0.0385, 2.4, 2.4), (-0.040, 0.0150, 0.0855, 0.0385, 2.4, 2.4)], BLUED, coll,
                     segs=40, subsurf=0))
    M(K.section_loft('SMG_ButtPlate', [(-0.324, 0.0194, 0.0730, -0.0330, 2.6, 2.6), (-0.318, 0.0194, 0.0730, -0.0330, 2.6, 2.6)], BLUED, coll,
                     segs=40, subsurf=1))
    for i, z in enumerate((0.055, -0.016)):
        M(K.disc(f'SMG_ButtScrew{i}', (-0.3242, 0.0, z), 0.0028, 0.0010, (-1, 0, 0), BRIGHT, coll, segs=12))
    M(K.ring('SMG_SwivelRear', (-0.230, 0.0, -0.010), 0.0068, 0.0014, BLUED, coll, axis='Y', segs=16, minor_segs=6))

    # ------------------------------------------------------------------------------------ barrel group
    M(K.lathe('SMG_Boss', [(0.200, 0.0), (0.200, 0.0150), (0.212, 0.0145), (0.218, 0.0112), (0.218, 0.0)], BLUED, coll, center_yz=(0.0, BZ), segs=28))
    M(K.lathe('SMG_Fins', fins_profile(0.218, 0.360, 0.0072, 0.0108, 0.0165), BLUED, coll, center_yz=(0.0, BZ), segs=28))
    M(K.lathe('SMG_Barrel', [(0.355, 0.0), (0.355, 0.0095), (0.432, 0.0092), (0.432, 0.0)], BLUED, coll, center_yz=(0.0, BZ), segs=20))
    comp = [(0.430, 0.0), (0.430, 0.0128), (0.434, 0.0136), (0.478, 0.0136), (S.MUZZLE_X, 0.0128), (S.MUZZLE_X, 0.0060), (S.MUZZLE_X - 0.001, 0.0054),
            (0.460, 0.0054), (0.460, 0.0)]
    M(K.lathe('SMG_Compensator', comp, BLUED, coll, center_yz=(0.0, BZ), segs=28))
    for i in range(4):
        M(B.box(f'SMG_CompSlot{i}', (0.442 + i * 0.0085, 0.0, BZ + 0.0134), (0.0040, 0.0150, 0.0012), DARK, coll, bevel=0.0))
    M(B.box('SMG_FrontSight', (S.SIGHT_FRONT[0], 0.0, (S.SIGHT_FRONT[2] + BZ + 0.012) * 0.5), (0.0036, 0.0022, S.SIGHT_FRONT[2] - BZ - 0.012),
            BLUED, coll, bevel=0.0004))
    M(B.blob('SMG_FrontBead', (S.SIGHT_FRONT[0], 0.0, S.SIGHT_FRONT[2]), (0.0020, 0.0020, 0.0020), BRASS_B, coll, seg=12, ring=6))
    M(K.ring('SMG_ForeBand', (0.258, 0.0, BZ), 0.0112, 0.0024, BLUED, coll, axis='X', segs=24, minor_segs=8))
    M(B.box('SMG_ForeBandLug', (0.258, 0.0, BZ - 0.0120), (0.012, 0.0100, 0.0060), BLUED, coll, bevel=0.0010))
    M(K.disc('SMG_ForeGripCap', (0.270, 0.0, -0.0462), 0.0118, 0.0018, (0.12, 0, -1), BLUED, coll, segs=18))
    M(K.disc('SMG_PistolGripCap', (-0.019, 0.0, -0.0478), 0.0120, 0.0018, (-0.28, 0, -1), BLUED, coll, segs=18))

    # --------------------------------------------------------------------------------------------- bolt
    Bo = lambda ob: add('Metal', 'bolt', ob)        # noqa: E731
    k = Vector(S.KNOB)
    Bo(B.box('SMG_KnobStem', (k.x, 0.0, RZ1 - 0.002), (0.0090, 0.0042, 0.012), BLUED, coll, bevel=0.0006))
    Bo(B.blob('SMG_Knob', k, (0.0068, 0.0068, 0.0062), BRASS, coll, seg=18, ring=10))
    Bo(B.box('SMG_BoltBody', (0.070, 0.0, BZ + 0.001), (0.080, 0.0200, 0.0200), BRIGHT, coll, bevel=0.0012))
    # ---------------------------------------------------------------------------------------- trigger
    add('Metal', 'trigger', B.tube('SMG_Trigger', [V(*S.TRIGGER_PIVOT), V(0.031, 0, 0.022), V(0.029, 0, 0.012), V(0.025, 0, 0.006)],
                                   [0.0030, 0.0028, 0.0026, 0.0022], BLUED, coll, segs=10, squash=0.6))
    # ------------------------------------------------------------------------------------------ latch
    lp = Vector(S.LATCH_PIVOT)
    add('Metal', 'mag_latch', B.box('SMG_Latch', (lp.x + 0.007, 0.0, lp.z - 0.0035), (0.016, 0.0100, 0.0040), BLUED, coll, bevel=0.0008))
    add('Metal', 'mag_latch', B.blob('SMG_LatchTip', (lp.x + 0.0150, 0.0, lp.z - 0.0045), (0.0030, 0.0060, 0.0028), BRASS_B, coll, seg=12, ring=6))

    # ------------------------------------------------------------------------------------------- drum
    D = lambda ob: add('Drum', 'drum', ob)          # noqa: E731
    c = Vector(S.DRUM_C)
    r, hw = S.DRUM_R, S.DRUM_HW
    T = Matrix.Translation(c) @ Matrix.Rotation(math.radians(90), 4, 'Z')        # lathe X axis -> weapon Y (drum axis)
    shell = [(-hw, 0.0), (-hw, r - 0.008), (-hw + 0.0008, r - 0.002), (-hw + 0.003, r), (hw - 0.003, r), (hw - 0.0008, r - 0.002), (hw, r - 0.008),
             (hw, 0.0)]
    D(K.lathe('Drum_Shell', shell, BLUED, coll, segs=48, transform=T))
    for s in (-1, 1):
        D(K.ring(f'Drum_Rib{s:+d}', (c.x, c.y + s * hw, c.z), r - 0.016, 0.0016, BLUED, coll, axis='Y', segs=40, minor_segs=6))
        D(K.ring(f'Drum_RibOuter{s:+d}', (c.x, c.y + s * (hw - 0.0005), c.z), r - 0.006, 0.0012, BLUED, coll, axis='Y', segs=48, minor_segs=6))
        D(K.disc(f'Drum_Boss{s:+d}', (c.x, c.y + s * hw, c.z), 0.0115, 0.0030, (0, s, 0), BRASS if s > 0 else BLUED, coll, segs=24))
        for i in range(8):
            a = 2 * math.pi * i / 8
            D(K.disc(f'Drum_Rivet{s:+d}{i}', (c.x + (r - 0.0105) * math.cos(a), c.y + s * hw, c.z + (r - 0.0105) * math.sin(a)), 0.0016, 0.0010,
                     (0, s, 0), BRIGHT, coll, segs=8))
    # the winding key (brass butterfly) on the left face
    D(B.box('Drum_Key', (c.x, c.y + hw + 0.0050, c.z), (0.026, 0.0040, 0.0070), BRASS_B, coll, bevel=0.0018))
    D(B.tube('Drum_KeyStem', [V(c.x, c.y + hw, c.z), V(c.x, c.y + hw + 0.0048, c.z)], [0.0035, 0.0035], BRASS, coll, segs=12))
    D(B.box('Drum_FeedTower', (c.x, 0.0, c.z + r + 0.0035), (0.034, 0.0170, 0.0110), BLUED, coll, bevel=0.0012))
    D(B.box('Drum_FeedLips', (c.x, 0.0, RZ0 - 0.0010), (0.030, 0.0120, 0.0040), DARK, coll, bevel=0.0006))

    bones = {n: (Vector(h), Vector(t), p) for n, (h, t, p) in S.BONES.items()}
    sockets = {n: (v[0], Vector(v[1])) for n, v in S.SOCKETS.items()}
    return {'parts': parts, 'bones': bones, 'sockets': sockets,
            'params': {'bore_z': BZ, 'muzzle_x': S.MUZZLE_X, 'drum_centre_m': list(S.DRUM_C), 'drum_radius_m': S.DRUM_R,
                       'bolt_travel_m': S.BOLT_TRAVEL, 'knob_rest_m': list(S.KNOB)},
            'sights': {'rear_m': list(S.SIGHT_REAR), 'front_m': list(S.SIGHT_FRONT)}}


def build_casing(coll):
    """A fired pistol case lying along +X (mouth +X), base at the origin (bean scale ~0.75 of a .45)."""
    case = [(0.0, 0.0), (0.0, 0.0046), (0.0008, 0.0048), (0.0013, 0.0042), (0.0022, 0.0042), (0.0026, 0.0046), (0.0172, 0.0045),
            (0.0172, 0.0040), (0.0165, 0.0039), (0.0040, 0.0039), (0.0040, 0.0)]
    parts = [K.lathe('Casing_SMG', case, FIRED, coll, segs=16)]
    parts.append(K.disc('Casing_SMGPrimer', (-0.0001, 0.0, 0.0), 0.0018, 0.0006, (-1, 0, 0), FIRED, coll, segs=10))
    return parts
