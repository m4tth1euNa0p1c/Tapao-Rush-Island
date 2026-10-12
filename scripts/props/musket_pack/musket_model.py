"""Flintlock musket of the bean, v2 "realistic" (T-246, owner request 2026-10-09: "rendre les modèles des armes plus
réalistes") - a Brown Bess / 1777 style piece at the pack's 0.92 m, with the SAME bone names, pivots, sockets and
rest geometry lines as the T-212 model (ROD_HEAD, MUZZLE, PAN, HAMMER_SPUR, FRIZZEN_TOE, CARTRIDGE_REST of
animation.py): every clip, the game's sockets and the FP framing stay valid.

Geometry (weapon local: +X to the muzzle, +Z up, -Y the lock side):
  - full walnut stock lofted from asymmetric sections: comb and toe lines, thin wrist, lock flats, fore-end with the
    barrel bedded in a half-round channel and the ramrod in a groove under it (section_loft + carving);
  - round tapered barrel turned from one profile, breech swell and plug line, open muzzle with a 3 cm deep bore, tang
    screwed into the wrist, bayonet lug doubling as front sight, rear sight block with a notch (the sight line is
    SIGHT_Z above the bore axis, both sights at the same height: the aim framing puts it on the camera axis);
  - complete lock: shaped lock plate, pan with its bowl and bridle, frizzen spring, swan-neck cock with jaws, jaw
    screw, cock screw, spur, flint in its leather; frizzen face, pan cover and curled toe;
  - brass furniture: butt plate and comb tang, trigger guard bow with finial and long rear tang, trigger plate,
    serpentine side plate with the lock screws, wrist escutcheon, nose cap, three ramrod pipes (tail pipe with tang);
  - iron sling swivels and a taut leather sling with its buckle, stock pins;
  - steel ramrod with a trumpet head, on its own bone; paper cartridge on the bone "cartridge".
Materials are temporary procedural looks (looks.py) baked to T_Musket_<Wood|Iron|Brass|Leather|Paper>_*.png.
"""
import math

import bpy
from mathutils import Matrix, Vector

import kit as K
import looks as L

# materials: atlas -> final material (resolution); temporary materials carry the looks
ATLAS = {'Musket_Wood': ('M_Musket_Wood', 2048), 'Musket_Iron': ('M_Musket_Iron', 2048), 'Musket_Brass': ('M_Musket_Brass', 2048),
         'Musket_Leather': ('M_Musket_Leather', 1024), 'Musket_Paper': ('M_Musket_Paper', 512)}
WOOD, IRON, DARK, BRASS, LEATHER, PAPER, LINEN, FLINT = ('TMP-Musket-Wood', 'TMP-Musket-Iron', 'TMP-Musket-DarkIron',
                                                        'TMP-Musket-Brass', 'TMP-Musket-Leather', 'TMP-Musket-Paper',
                                                        'TMP-Musket-Linen', 'TMP-Musket-Flint')

# rest place of the cartridge: inside the butt, never seen (centre, length along X, radius) - contract of the clips
CARTRIDGE_REST = Vector((-0.195, 0.0, -0.030))
CARTRIDGE_LENGTH = 0.052
CARTRIDGE_RADIUS = 0.0072
# sights: front (bayonet lug top) and rear notch bottom at the same height above the bore axis
SIGHT_Z = 0.0665
SIGHT_REAR_X = 0.080
BORE_Z = 0.045

# stock sections: (x, half width, z top, z bottom, exponent top, exponent bottom)
STOCK = [
    (-0.300, 0.0230, 0.0200, -0.0900, 2.6, 2.6), (-0.288, 0.0252, 0.0218, -0.0915, 2.6, 2.6),
    (-0.255, 0.0255, 0.0225, -0.0880, 2.5, 2.4), (-0.210, 0.0248, 0.0240, -0.0760, 2.5, 2.3),
    (-0.165, 0.0238, 0.0255, -0.0610, 2.5, 2.3), (-0.135, 0.0222, 0.0235, -0.0500, 2.4, 2.3),
    (-0.105, 0.0200, 0.0180, -0.0400, 2.3, 2.3), (-0.075, 0.0195, 0.0150, -0.0330, 2.3, 2.3),
    (-0.050, 0.0200, 0.0160, -0.0280, 2.3, 2.3), (-0.030, 0.0210, 0.0215, -0.0245, 2.5, 2.4),
    (-0.012, 0.0215, 0.0340, -0.0215, 3.2, 2.4), (0.015, 0.0215, 0.0430, -0.0195, 4.0, 2.4),
    (0.060, 0.0215, 0.0430, -0.0150, 4.0, 2.4), (0.110, 0.0210, 0.0430, -0.0060, 4.0, 2.4),
    (0.160, 0.0200, 0.0430, -0.0010, 4.0, 2.4), (0.230, 0.0192, 0.0430, 0.0005, 4.0, 2.4),
    (0.310, 0.0184, 0.0430, 0.0018, 4.0, 2.4), (0.390, 0.0176, 0.0430, 0.0030, 4.0, 2.4),
    (0.460, 0.0168, 0.0430, 0.0042, 4.0, 2.4), (0.505, 0.0160, 0.0425, 0.0050, 4.0, 2.4),
    (0.517, 0.0150, 0.0400, 0.0055, 3.0, 2.4),
]
BARREL_R = [(0.024, 0.0200), (0.030, 0.0206), (0.045, 0.0206), (0.052, 0.0198), (0.105, 0.0195), (0.20, 0.0185),
            (0.35, 0.0172), (0.50, 0.0160), (0.600, 0.0150), (0.616, 0.0156)]
ROD_Z = [(0.10, 0.0), (0.608, 0.008)]


def stock_top(x):
    return K.interp(x, [(s[0], s[2]) for s in STOCK])


def stock_bottom(x):
    return K.interp(x, [(s[0], s[3]) for s in STOCK])


def stock_hw(x):
    return K.interp(x, [(s[0], s[1]) for s in STOCK])


def barrel_r(x):
    return K.interp(x, BARREL_R)


def rod_z(x):
    return K.interp(x, ROD_Z)


TANG = [(0.026, 0.0130, 0.0640, 0.0360, 2.0, 2.0), (0.016, 0.0125, 0.0560, 0.0360, 2.0, 2.0),
        (0.004, 0.0118, 0.0480, 0.0340, 2.0, 2.0), (-0.010, 0.0105, 0.0400, 0.0300, 2.0, 2.0),
        (-0.024, 0.0088, 0.0300, 0.0210, 2.0, 2.0), (-0.045, 0.0055, 0.0200, 0.0120, 2.0, 2.0),
        (-0.058, 0.0014, 0.0170, 0.0125, 2.0, 2.0)]


def tang_top(x):
    return K.interp(x, [(s[0], s[2]) for s in TANG])


def carve(x, y, z, top):
    """Barrel channel in the top of the stock, ramrod groove under the fore-end."""
    if top and x >= 0.022:
        r = barrel_r(max(x, 0.024)) + 0.0006
        if abs(y) < r:
            z = min(z, BORE_Z - math.sqrt(r * r - y * y))
    if not top and x >= 0.122:
        g = 0.0060
        if abs(y) < g:
            z = max(z, rod_z(x) + math.sqrt(g * g - y * y))
    return z


def materials():
    for aid, (final, res) in ATLAS.items():
        L.register_atlas(aid, final, res)
    L.temp_material(WOOD, 'wood', 'Musket_Wood', handling=0.3)
    L.temp_material(IRON, 'iron', 'Musket_Iron')
    L.temp_material(DARK, 'darkiron', 'Musket_Iron')
    L.temp_material(FLINT, 'flint', 'Musket_Iron')
    L.temp_material(BRASS, 'brass', 'Musket_Brass')
    L.temp_material(LEATHER, 'leather', 'Musket_Leather')
    L.temp_material(PAPER, 'paper', 'Musket_Paper')
    L.temp_material(LINEN, 'linen', 'Musket_Paper')


def build_cartridge(coll, paper=PAPER, linen=LINEN, centre=CARTRIDGE_REST, prefix='Musket_'):
    """Paper cartridge along +X centred on centre: twisted tail at -X, the ball bulging at +X, a thread tie."""
    B = K.bean()
    c, half, r = Vector(centre), CARTRIDGE_LENGTH * 0.5, CARTRIDGE_RADIUS
    parts = [B.tube(prefix + 'Cartridge', [c + Vector((-half, 0, 0)), c + Vector((-half * 0.2, 0, 0)), c + Vector((half * 0.45, 0, 0)),
                                           c + Vector((half, 0, 0))], [r * 0.82, r * 0.98, r, r * 0.9], paper, coll, segs=16)]
    parts.append(B.blob(prefix + 'CartridgeBall', c + Vector((half * 0.70, 0, 0)), (r * 1.08, r * 1.10, r * 1.10), paper, coll,
                        seg=18, ring=10))
    parts.append(B.tube(prefix + 'CartridgeTwist', [c + Vector((-half - 0.010, 0, 0)), c + Vector((-half - 0.004, 0, 0)),
                                                    c + Vector((-half + 0.002, 0, 0))], [r * 0.22, r * 0.40, r * 0.62], paper, coll, segs=10))
    parts.append(B.tube(prefix + 'CartridgeTie', [c + Vector((half * 0.36, 0, 0)), c + Vector((half * 0.36 + 0.0025, 0, 0))],
                        [r * 1.06, r * 1.06], linen, coll, segs=16))
    return parts


def build_musket(coll, p=None):
    """parts per bone (root, hammer, frizzen, trigger, ramrod, cartridge), sockets, bones, params."""
    B = K.bean()
    materials()
    p = K.musket_params(p)
    bz, bx0, mx = p['barrel_z'], p['breech_x'], p['muzzle_x']
    parts = {'root': [], 'hammer': [], 'frizzen': [], 'trigger': [], 'ramrod': [], 'cartridge': []}
    R = parts['root']
    V = K.V

    # ---------------------------------------------------------------------------------------------- stock (walnut)
    R.append(K.section_loft('Musket_Stock', STOCK, WOOD, coll, segs=72, subsurf=1, shape=carve))

    # ------------------------------------------------------------------------------------------------ barrel (iron)
    bore_r = p['bore_r']
    profile = [(0.024, 0.0)] + BARREL_R + [(0.620, 0.0156), (0.620, bore_r + 0.0006), (0.619, bore_r), (0.590, bore_r), (0.590, 0.0)]
    R.append(K.lathe('Musket_Barrel', profile, IRON, coll, center_yz=(0.0, bz), segs=28))
    # tang: the breech plug's domed strip down the top of the wrist, screwed
    R.append(K.section_loft('Musket_Tang', TANG, IRON, coll, segs=20, subsurf=1))
    R.append(K.disc('Musket_TangScrew', (-0.032, 0, tang_top(-0.032) - 0.0002), 0.0034, 0.0014, (-0.45, 0, 1), IRON, coll, segs=14))
    R.append(B.box('Musket_TangScrewSlot', (-0.032, 0, tang_top(-0.032) + 0.0010), (0.0052, 0.0009, 0.0008), DARK, coll, bevel=0))
    # sights: bayonet lug (front sight) and rear sight block with its notch ears, both at SIGHT_Z
    R.append(B.box('Musket_BayonetLug', (mx - 0.040, 0, SIGHT_Z - 0.0075), (0.012, 0.006, 0.015), IRON, coll, bevel=0.0012))
    R.append(B.box('Musket_RearSight', (SIGHT_REAR_X, 0, SIGHT_Z - 0.0035), (0.011, 0.0100, 0.0070), IRON, coll, bevel=0.0012))
    for s in (-1, 1):
        R.append(B.box('Musket_RearSightEar%+d' % s, (SIGHT_REAR_X, s * 0.0032, SIGHT_Z + 0.0015), (0.011, 0.0036, 0.0030), IRON, coll,
                       bevel=0.0008))
    R.append(B.box('Musket_RearSightNotch', (SIGHT_REAR_X, 0, SIGHT_Z + 0.0004), (0.0112, 0.0016, 0.0008), DARK, coll, bevel=0))

    # -------------------------------------------------------------------------------------------- lock (right side)
    outline = [(-0.030, 0.0300), (-0.022, 0.0372), (-0.008, 0.0430), (0.012, 0.0472), (0.040, 0.0500), (0.070, 0.0520),
               (0.096, 0.0510), (0.112, 0.0455), (0.117, 0.0360), (0.112, 0.0250), (0.098, 0.0168), (0.072, 0.0128),
               (0.040, 0.0118), (0.012, 0.0128), (-0.010, 0.0180), (-0.024, 0.0245)]
    R.append(K.plate('Musket_LockPlate', outline, -0.0212, -0.0262, IRON, coll, bevel=0.0012))
    R.append(B.box('Musket_Pan', (0.069, -0.0295, 0.0465), (0.024, 0.012, 0.010), IRON, coll, bevel=0.0025))
    R.append(B.blob('Musket_PanBowl', (0.067, -0.0295, 0.0515), (0.0065, 0.0042, 0.0025), DARK, coll, seg=16, ring=8))
    R.append(B.box('Musket_PanBridle', (0.084, -0.031, 0.0455), (0.010, 0.004, 0.006), IRON, coll, bevel=0.001))
    R.append(B.tube('Musket_FrizzenSpring', [V(0.072, -0.0285, 0.0255), V(0.088, -0.0285, 0.0240), V(0.104, -0.0285, 0.0300),
                                             V(0.1125, -0.0285, 0.0400)], [0.0030, 0.0034, 0.0032, 0.0022], IRON, coll, segs=10, squash=0.3))
    R.append(K.disc('Musket_FrizzenSpringScrew', (0.106, -0.0272, 0.0280), 0.0030, 0.0014, (0, -1, 0), IRON, coll, segs=14))
    # serpentine brass side plate on the left, with the two lock screw heads
    R.append(B.tube('Musket_SidePlate', [V(-0.034, 0.0218, 0.0240), V(-0.010, 0.0220, 0.0315), V(0.030, 0.0224, 0.0265),
                                         V(0.066, 0.0224, 0.0205), V(0.092, 0.0220, 0.0245)], [0.0035, 0.0055, 0.0062, 0.0050, 0.0030],
                    BRASS, coll, segs=10, squash=0.32))
    for i, (x, z) in enumerate(((0.004, 0.0290), (0.064, 0.0215))):
        R.append(K.disc('Musket_LockScrew%d' % i, (x, 0.0244, z), 0.0042, 0.0018, (0, 1, 0), IRON, coll, segs=16))
        R.append(B.box('Musket_LockScrewSlot%d' % i, (x, 0.0258, z), (0.0062, 0.0008, 0.0010), DARK, coll, bevel=0))

    # ------------------------------------------------------------------------------------------- brass furniture
    butt = [(-0.3085, 0.0240, 0.0210, -0.0912, 2.6, 2.6), (-0.2995, 0.0240, 0.0212, -0.0912, 2.6, 2.6)]
    R.append(K.section_loft('Musket_ButtPlate', butt, BRASS, coll, segs=48, subsurf=1))
    btang = [(-0.300, 0.0100, stock_top(-0.300) + 0.0028, stock_top(-0.300) - 0.004, 2.0, 2.0),
             (-0.270, 0.0090, stock_top(-0.270) + 0.0026, stock_top(-0.270) - 0.004, 2.0, 2.0),
             (-0.240, 0.0065, stock_top(-0.240) + 0.0022, stock_top(-0.240) - 0.004, 2.0, 2.0),
             (-0.222, 0.0025, stock_top(-0.222) + 0.0014, stock_top(-0.222) - 0.003, 2.0, 2.0)]
    R.append(K.section_loft('Musket_ButtTang', btang, BRASS, coll, segs=20, subsurf=1))
    for i, z in enumerate((-0.006, -0.068)):
        R.append(K.disc('Musket_ButtScrew%d' % i, (-0.3090, 0, z), 0.0035, 0.0014, (-1, 0, 0), IRON, coll, segs=14))
    R.append(K.disc('Musket_ButtTangScrew', (-0.262, 0, stock_top(-0.262) + 0.0028), 0.0030, 0.0012, (0, 0, 1), IRON, coll, segs=14))
    # trigger guard: finial, bow, rear tang along the wrist
    guard = [V(0.100, 0, stock_bottom(0.100) - 0.0012), V(0.084, 0, stock_bottom(0.084) - 0.0014), V(0.074, 0, -0.0250),
             V(0.060, 0, -0.0500), V(0.040, 0, -0.0700), V(0.015, 0, -0.0800), V(-0.012, 0, -0.0800), V(-0.038, 0, -0.0680),
             V(-0.052, 0, -0.0460), V(-0.062, 0, stock_bottom(-0.062) - 0.0014), V(-0.088, 0, stock_bottom(-0.088) - 0.0014),
             V(-0.118, 0, stock_bottom(-0.118) - 0.0012)]
    widths = [0.0025, 0.0050, 0.0065, 0.0068, 0.0068, 0.0068, 0.0068, 0.0068, 0.0065, 0.0060, 0.0045, 0.0020]
    R.append(K.strap('Musket_TriggerGuard', guard, widths, 0.0018, BRASS, coll, segs=12))
    tplate = [(-0.022, 0.0055, stock_bottom(-0.022) + 0.002, stock_bottom(-0.022) - 0.0016, 6.0, 6.0),
              (0.010, 0.0058, stock_bottom(0.010) + 0.002, stock_bottom(0.010) - 0.0016, 6.0, 6.0),
              (0.044, 0.0052, stock_bottom(0.044) + 0.002, stock_bottom(0.044) - 0.0014, 6.0, 6.0)]
    R.append(K.section_loft('Musket_TriggerPlate', tplate, BRASS, coll, segs=16, subsurf=0))
    R.append(K.disc('Musket_Escutcheon', (-0.072, 0, stock_top(-0.072) + 0.0012), 0.0085, 0.0014, (-0.08, 0, 1), BRASS, coll, segs=24))
    nose = [(0.5085, stock_hw(0.5085) + 0.0012, stock_top(0.5085) + 0.0012, stock_bottom(0.5085) - 0.0012, 4.0, 2.4),
            (0.5210, stock_hw(0.5210) + 0.0012, stock_top(0.5210) + 0.0012, stock_bottom(0.5210) - 0.0012, 3.0, 2.4)]
    R.append(K.section_loft('Musket_NoseCap', nose, BRASS, coll, segs=48, subsurf=1, shape=carve))
    for x in (0.330, 0.495):
        R.append(K.ring('Musket_RamrodPipe%d' % int(x * 100), (x, 0, rod_z(x)), 0.0068, 0.0016, BRASS, coll, axis='X', segs=20, minor_segs=6))
    R.append(B.tube('Musket_TailPipe', [V(0.150, 0, rod_z(0.150)), V(0.205, 0, rod_z(0.205))], [0.0074, 0.0068], BRASS, coll, segs=14))
    R.append(B.box('Musket_TailPipeTang', (0.134, 0, stock_bottom(0.134) - 0.0008), (0.034, 0.0060, 0.0018), BRASS, coll, bevel=0.0006))
    R.append(K.disc('Musket_TailPipePin', (0.128, 0, stock_bottom(0.128) - 0.0018), 0.0016, 0.0008, (0, 0, -1), IRON, coll, segs=10))

    # ------------------------------------------------------------------------------------ sling, swivels, pins
    R.append(K.ring('Musket_SwivelF', (0.330, 0, rod_z(0.330) - 0.0068 - 0.0090), 0.0085, 0.0016, IRON, coll, axis='Y', segs=20, minor_segs=6))
    R.append(K.ring('Musket_SwivelB', (0.084, 0, stock_bottom(0.084) - 0.0036 - 0.0090), 0.0085, 0.0016, IRON, coll, axis='Y', segs=20,
                    minor_segs=6))
    if p.get('sling', True):
        sling = [V(0.330, 0, -0.0205), V(0.290, 0, -0.0420), V(0.225, 0, -0.0580), V(0.160, 0, -0.0580), V(0.105, 0, -0.0490),
                 V(0.084, 0, -0.0360)]
        R.append(K.strap('Musket_Sling', sling, 0.0115, 0.0022, LEATHER, coll, segs=12))
        R.append(K.ring('Musket_SlingBuckle', (0.165, 0, -0.0580), 0.0125, 0.0018, BRASS, coll, axis='Y', segs=16, minor_segs=6))
        R.append(B.box('Musket_SlingBuckleBar', (0.165, 0, -0.0580), (0.0030, 0.0250, 0.0030), BRASS, coll, bevel=0.0008))
    for x in (0.135, 0.255, 0.455):
        for s in (-1, 1):
            R.append(K.disc('Musket_Pin%d%+d' % (int(x * 1000), s), (x, s * stock_hw(x), 0.0180), 0.0020, 0.0008, (0, s, 0), IRON, coll, segs=10))

    # ----------------------------------------------------------------------------- flintlock: cock (bone hammer)
    hp = parts['hammer']
    hp.append(B.tube('Musket_Cock', [V(0.022, -0.0325, 0.030), V(0.0135, -0.0325, 0.0440), V(0.0150, -0.0325, 0.0580),
                                     V(0.0255, -0.0325, 0.0690), V(0.0400, -0.0325, 0.0745), V(0.0475, -0.0325, 0.0720)],
                     [0.0100, 0.0070, 0.0060, 0.0060, 0.0065, 0.0060], IRON, coll, segs=12, squash=0.55))
    hp.append(B.blob('Musket_CockBoss', (0.022, -0.0335, 0.030), (0.0105, 0.0045, 0.0105), IRON, coll, seg=16, ring=8))
    hp.append(K.disc('Musket_CockScrew', (0.022, -0.0382, 0.030), 0.0042, 0.0016, (0, -1, 0), IRON, coll, segs=16))
    hp.append(B.box('Musket_CockScrewSlot', (0.022, -0.0395, 0.030), (0.0062, 0.0010, 0.0010), DARK, coll, bevel=0))
    hp.append(B.blob('Musket_CockSpur', (0.028, -0.0325, 0.0800), (0.0060, 0.0048, 0.0050), IRON, coll, seg=12, ring=8))
    hp.append(B.box('Musket_JawLow', (0.0485, -0.0320, 0.0665), (0.021, 0.0125, 0.0050), IRON, coll, bevel=0.0015))
    hp.append(B.box('Musket_JawTop', (0.0485, -0.0320, 0.0860), (0.021, 0.0125, 0.0050), IRON, coll, bevel=0.0015))
    hp.append(B.tube('Musket_JawScrew', [V(0.0485, -0.0325, 0.0640), V(0.0485, -0.0325, 0.0975)], [0.0028, 0.0028], IRON, coll, segs=10))
    hp.append(B.blob('Musket_JawScrewHead', (0.0485, -0.0325, 0.0985), (0.0062, 0.0062, 0.0028), IRON, coll, seg=14, ring=6))
    hp.append(B.box('Musket_JawScrewSlot', (0.0485, -0.0325, 0.1010), (0.0090, 0.0012, 0.0010), DARK, coll, bevel=0))
    hp.append(B.box('Musket_FlintLeather', (0.0505, -0.0315, 0.0763), (0.014, 0.0135, 0.0070), LEATHER, coll, bevel=0.001))
    fl = K.flint_chip('Musket_Flint', (0.026, 0.015, 0.0095), coll)
    fl.location = (0.057, -0.031, 0.0765)
    fl.data.materials[0] = bpy.data.materials[FLINT]
    hp.append(fl)

    # ---------------------------------------------------------------------------- frizzen (bone frizzen)
    fz = parts['frizzen']
    fz.append(B.box('Musket_Frizzen', (0.0785, -0.0295, 0.0665), (0.0065, 0.014, 0.039), IRON, coll, bevel=0.0018))
    fz.append(B.box('Musket_FrizzenFoot', (0.0675, -0.0295, 0.0490), (0.022, 0.0145, 0.0048), IRON, coll, bevel=0.0012))
    fz.append(B.box('Musket_FrizzenToe', (0.0825, -0.0295, 0.0855), (0.011, 0.0125, 0.0036), IRON, coll, bevel=0.001))
    fz.append(B.blob('Musket_FrizzenToeCurl', (0.0875, -0.0295, 0.0848), (0.0035, 0.0060, 0.0030), IRON, coll, seg=12, ring=6))
    fz.append(K.disc('Musket_FrizzenScrew', (0.076, -0.0375, 0.046), 0.0036, 0.0014, (0, -1, 0), IRON, coll, segs=14))

    # ------------------------------------------------------------------------------------- trigger (bone trigger)
    parts['trigger'].append(B.tube('Musket_Trigger', [V(0.014, 0, -0.016), V(0.0135, 0, -0.028), V(0.010, 0, -0.040), V(0.0025, 0, -0.051)],
                                   [0.0042, 0.0038, 0.0033, 0.0028], IRON, coll, segs=10, squash=0.6))

    # ---------------------------------------------------------------------------------------- ramrod (bone ramrod)
    rr_end = mx - 0.012
    head = Vector((0.10, 0, 0.0))
    tilt = (Vector((rr_end, 0, 0.008)) - head).normalized()
    rod = [(0.098, 0.0), (0.098, 0.0050), (0.104, 0.0050), (0.106, p['ramrod_r'] * 0.82), (rr_end - 0.030, p['ramrod_r'] * 0.82),
           (rr_end - 0.006, 0.0064), (rr_end - 0.0015, 0.0062), (rr_end, 0.0045), (rr_end, 0.0)]
    T = Matrix.Translation(head) @ Vector((1, 0, 0)).rotation_difference(tilt).to_matrix().to_4x4() @ Matrix.Translation(-head)
    parts['ramrod'].append(K.lathe('Musket_Ramrod', rod, IRON, coll, center_yz=(0.0, 0.0), segs=12, transform=T))

    # ------------------------------------------------------------------------------------ cartridge (bone cartridge)
    parts['cartridge'] += build_cartridge(coll)

    sockets = {
        'grip_r': ('root', V(0, 0, -.002)), 'grip_l': ('root', V(p['grip_l_x'], 0, .012)),
        'barrel': ('root', V(.45, 0, bz)), 'muzzle': ('root', V(mx + .006, 0, bz)),
        'pan': ('root', V(.068, -.027, .050)), 'butt': ('root', V(p['butt_x'] - .008, 0, -.035)),
        'sling': ('root', V(.08, 0, -.135)), 'ramrod_tip': ('ramrod', V(mx - .008, 0, .008)),
        'ramrod_end': ('ramrod', V(.115, 0, .0003)), 'flint': ('hammer', V(.054, -.030, .074)),
    }
    bones = {
        'root': (V(0, 0, 0), V(.10, 0, 0), None),
        'hammer': (V(.022, -.030, .030), V(.022, -.030, .070), 'root'),
        'frizzen': (V(.076, -.027, .046), V(.076, -.027, .086), 'root'),
        'trigger': (V(.014, 0, -.016), V(.006, 0, -.050), 'root'),
        'ramrod': (V(.10, 0, .000), V(mx - .012, 0, .008), 'root'),
        'cartridge': (CARTRIDGE_REST - Vector((CARTRIDGE_LENGTH * 0.5, 0, 0)),
                      CARTRIDGE_REST + Vector((CARTRIDGE_LENGTH * 0.5, 0, 0)), 'root'),
    }
    sights = {'rear_notch_m': [SIGHT_REAR_X, 0.0, SIGHT_Z], 'front_sight_m': [mx - 0.040, 0.0, SIGHT_Z],
              'bore_axis_z_m': bz, 'height_above_bore_m': round(SIGHT_Z - bz, 4)}
    return {'parts': parts, 'sockets': sockets, 'bones': bones, 'params': p, 'sights': sights}
