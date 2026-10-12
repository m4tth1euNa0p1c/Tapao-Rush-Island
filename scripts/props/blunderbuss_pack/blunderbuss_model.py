"""Naval flintlock blunderbuss of the bean (T-250, owner request 2026-10-09): short brass barrel with a bell mouth,
walnut full stock, flintlock, short ramrod, 0.75 m. Same bone names, pivots and socket names as the musket pack
(scripts/props/musket_pack/musket_model.py) so APirateMusketWeapon plays its clips with another DataAsset.

Weapon local: +X to the muzzle, +Z up, -Y is the lock side. Metres. Materials: scripts/props/longarm_common/materials.py
(procedural, baked by bake.py). The "cartridge" bone carries a SHOT CARTRIDGE: a paper powder tube ending in a linen
bag of pellets (the reload bites the paper end, primes, pours the powder, then tips the bag into the bell and rams the
paper as wadding); the ammunition item is SM_Ammo_ShotBag.
"""
import math

import bpy
from mathutils import Vector

import kit as K
import materials as MATS

V = K.V
WALNUT, DARK = 'MAT-LA-Walnut', 'MAT-LA-WalnutDark'
STEEL, BRASS, BRIGHT = 'MAT-LA-GreySteel', 'MAT-LA-Brass', 'MAT-LA-BrassBright'
LEATHER, LINEN, PAPER, LEAD = 'MAT-LA-Leather', 'MAT-LA-Linen', 'MAT-LA-Paper', 'MAT-LA-Lead'

# rest place of the shot cartridge: inside the butt (centre, length along X, paper radius)
CARTRIDGE_REST = Vector((-0.170, 0.0, -0.028))
CARTRIDGE_LENGTH = 0.060
CARTRIDGE_RADIUS = 0.0075


def params(p=None):
    d = {'butt_x': -0.27, 'muzzle_x': 0.48, 'breech_x': 0.03, 'barrel_z': 0.045, 'bell_r': 0.030, 'bore_r': 0.012,
         'butt_hw': 0.025, 'butt_hh': 0.054, 'grip_l_x': 0.20, 'guard_bottom_z': -0.076, 'ramrod_r': 0.0048,
         'fore_end_x': 0.335}
    d.update(p or {})
    return d


def flint(name, size, coll, at):
    ob = K.flint_chip(name, size, coll)
    ob.data.materials[0] = bpy.data.materials['MAT-LA-Flint']
    ob.location = at
    return ob


def build_cartridge(coll):
    """Shot cartridge along +X centred on CARTRIDGE_REST: twisted paper tail at -X (powder), the linen bag of pellets
    at +X with its tie, two pellets showing at the mouth of the bag."""
    B = K.bean()
    c, half, r = CARTRIDGE_REST, CARTRIDGE_LENGTH * 0.5, CARTRIDGE_RADIUS
    parts = [B.tube('Blunderbuss_CartridgePaper', [c + V(-half, 0, 0), c + V(-half * 0.2, 0, 0), c + V(half * 0.15, 0, 0)],
                    [r * 0.8, r, r * 0.9], PAPER, coll, segs=14)]
    parts.append(B.tube('Blunderbuss_CartridgeTwist', [c + V(-half - 0.009, 0, 0), c + V(-half + 0.002, 0, 0)],
                        [r * 0.25, r * 0.55], PAPER, coll, segs=10))
    parts.append(B.blob('Blunderbuss_ShotBag', c + V(half * 0.55, 0, 0), (half * 0.50, r * 1.75, r * 1.75), LINEN, coll, seg=18, ring=12))
    parts.append(B.tube('Blunderbuss_ShotBagTie', [c + V(half * 0.12, 0, 0), c + V(half * 0.18, 0, 0)], [r * 1.05, r * 1.05], LEATHER,
                        coll, segs=14))
    for i, (y, z) in enumerate(((0.004, 0.003), (-0.004, -0.002), (0.0, 0.006))):
        parts.append(B.blob(f'Blunderbuss_Pellet{i}', c + V(half * 1.02, y, z), (0.0032, 0.0032, 0.0032), LEAD, coll, seg=10, ring=6))
    return parts


def build_blunderbuss(coll, p=None):
    """parts per bone (root, hammer, frizzen, trigger, ramrod, cartridge), sockets, bones, params."""
    MATS.ensure_all()
    B = K.bean()
    p = params(p)
    bz, bx0, mx = p['barrel_z'], p['breech_x'], p['muzzle_x']
    parts = {'root': [], 'hammer': [], 'frizzen': [], 'trigger': [], 'ramrod': [], 'cartridge': []}
    R = parts['root']
    bh, bw, gz = p['butt_hh'], p['butt_hw'], p['guard_bottom_z']

    # stock: short walnut full stock, pronounced cheek, the fore-end stops before the bell
    stations = [
        (p['butt_x'], bw, bh, -.033), (p['butt_x'] + .016, bw * 1.03, bh * 1.02, -.031),
        (-.22, bw * .98, bh * .90, -.027), (-.18, bw * .95, bh * .80, -.022),
        (-.13, bw * .86, bh * .68, -.015), (-.085, .020, .024, -.008),
        (-.03, .019, .022, .002), (.04, .020, .023, .013),
        (.11, .0195, .020, .019), (.22, .018, .017, .023),
        (.31, .0165, .014, .026), (p['fore_end_x'], .0145, .012, .027),
    ]
    R.append(K.loft('Blunderbuss_Stock', stations, WALNUT, coll, segs=40, expo=2.9, subsurf=1))
    R.append(B.box('Blunderbuss_Cheekpiece', (-.175, .019, .008), (.100, .008, .030), DARK, coll, bevel=.008))
    R.append(B.box('Blunderbuss_ButtPlate', (p['butt_x'] - .004, 0, -.033), (.009, bw * 2 + .007, bh * 2 + .008), BRASS, coll, bevel=.0035))
    R.append(B.box('Blunderbuss_ButtTang', (p['butt_x'] + .016, 0, -.033 + bh * 1.0), (.040, .012, .004), BRASS, coll, bevel=.0015))
    for i, z in enumerate((-.033 + bh * .55, -.033 - bh * .55)):
        R.append(K.disc(f'Blunderbuss_ButtScrew{i}', (p['butt_x'] - .0088, 0, z), .0035, .0014, (-1, 0, 0), STEEL, coll, segs=14))

    # brass barrel swelling into the bell, hollow mouth (folded lathe: outer wall, lip, inner wall, bottom of the bore)
    br = p['bell_r']
    profile = [(bx0, .0195), (.10, .0180), (.20, .0165), (.30, .0158), (.36, .0168), (.40, .0210), (.44, .0265), (mx - .005, br + .0005),
               (mx + .002, br + .001), (mx + .004, br - .0015), (mx - .002, br - .0045), (.44, .0215), (.40, .0135), (.385, p['bore_r']),
               (.37, p['bore_r'])]
    R.append(B.lathe_x('Blunderbuss_Barrel', profile, BRASS, coll, axis_z=lambda x: bz, segs=36, cap_start=True, cap_end=True))
    R.append(K.ring('Blunderbuss_BreechLine', (bx0 + .066, 0, bz), .0186, .0010, 'MAT-LA-Bore', coll, axis='X', segs=24, minor_segs=6))
    R.append(B.box('Blunderbuss_Tang', (bx0 - .009, 0, bz - .010), (.060, .026, .012), STEEL, coll, bevel=.005))
    R.append(K.disc('Blunderbuss_TangScrew', (bx0 - .024, 0, bz - .0034), .0034, .0014, (0, 0, 1), STEEL, coll, segs=14))
    R.append(K.ring('Blunderbuss_BellBand', (.405, 0, bz), .0222, .0022, BRIGHT, coll, axis='X', segs=32, minor_segs=8))

    # lock (grey steel): plate, tail, pan, frizzen spring and screw, bridle, tumbler screw; brass side plate opposite
    R.append(B.box('Blunderbuss_LockPlate', (.040, -.0235, .032), (.092, .006, .040), STEEL, coll, bevel=.004))
    R.append(B.box('Blunderbuss_LockTail', (-.012, -.0235, .029), (.026, .005, .020), STEEL, coll, bevel=.003))
    R.append(B.box('Blunderbuss_Pan', (.062, -.029, .047), (.024, .012, .009), 'MAT-LA-Bore', coll, bevel=.0025))
    R.append(B.box('Blunderbuss_FrizzenSpring', (.082, -.0285, .027), (.028, .004, .006), STEEL, coll, bevel=.0012))
    R.append(K.disc('Blunderbuss_FrizzenSpringScrew', (.093, -.0275, .027), .0030, .0016, (0, -1, 0), BRASS, coll, segs=14))
    R.append(B.box('Blunderbuss_PanBridle', (.076, -.0305, .044), (.010, .003, .006), STEEL, coll, bevel=.001))
    R.append(K.disc('Blunderbuss_TumblerScrew', (.020, -.0272, .019), .0040, .0016, (0, -1, 0), STEEL, coll, segs=16))
    R.append(K.disc('Blunderbuss_LockScrewR0', (-.002, -.0262, .034), .0032, .0016, (0, -1, 0), STEEL, coll, segs=14))
    R.append(B.tube('Blunderbuss_SidePlate', [V(-.026, .0205, .021), V(-.004, .0215, .029), V(.030, .0225, .023), V(.060, .0222, .017),
                                              V(.078, .0216, .020)], [.0040, .0050, .0055, .0045, .0030], BRASS, coll, segs=10, squash=.35))
    for i, x in enumerate((.008, .055)):
        R.append(K.disc(f'Blunderbuss_LockScrew{i}', (x, .0262, .024), .0042, .0018, (0, 1, 0), STEEL, coll, segs=16))
    R.append(K.disc('Blunderbuss_Escutcheon', (-.036, 0, .0258), .0080, .0016, (0, 0, 1), BRASS, coll, segs=24))
    R.append(B.box('Blunderbuss_TriggerPlate', (.004, 0, -.0222), (.052, .010, .003), BRASS, coll, bevel=.0012))

    # fore-end furniture: nose cap, barrel key, tail pipe and ramrod pipes, stock pins
    R.append(K.ring('Blunderbuss_NoseCap', (p['fore_end_x'], 0, .030), .0160, .0030, BRASS, coll, axis='X', segs=28, minor_segs=8))
    R.append(B.box('Blunderbuss_BarrelKey', (.26, 0, .030), (.006, .044, .003), STEEL, coll, bevel=.001))
    R.append(B.tube('Blunderbuss_TailPipe', [V(.120, 0, .0022), V(.165, 0, .0036)], [.0066, .0062], BRASS, coll, segs=14))
    for x in (.21, .30):
        R.append(K.ring(f'Blunderbuss_RamrodPipe{int(x * 100)}', (x, 0, .0048), .0062, .0016, BRASS, coll, axis='X', segs=16, minor_segs=6))
    for x in (-.22, -.12, -.04, .09):
        R.append(B.blob(f'Blunderbuss_StockPin{x:+.3f}', (x, -.021, .004), (.0035, .002, .0035), BRASS, coll, seg=10, ring=6))

    # flintlock: hammer with its jaws, screw and flint; frizzen; trigger and brass guard; stowed ramrod
    hp = parts['hammer']
    hp.append(B.tube('Blunderbuss_Hammer', [V(.020, -.032, .031), V(.009, -.032, .048), V(.020, -.032, .067), V(.038, -.032, .076)],
                     [.0072, .0062, .0058, .0056], STEEL, coll, segs=12))
    hp.append(B.blob('Blunderbuss_HammerPivot', (.020, -.034, .031), (.010, .004, .010), BRASS, coll, seg=16, ring=8))
    hp.append(B.box('Blunderbuss_JawTop', (.045, -.032, .084), (.019, .013, .005), STEEL, coll, bevel=.0015))
    hp.append(B.box('Blunderbuss_JawLow', (.045, -.032, .066), (.019, .013, .005), STEEL, coll, bevel=.0015))
    hp.append(B.tube('Blunderbuss_JawScrew', [V(.045, -.033, .063), V(.045, -.033, .095)], [.003, .003], BRASS, coll, segs=10))
    hp.append(B.blob('Blunderbuss_JawScrewHead', (.045, -.033, .096), (.006, .006, .003), BRASS, coll, seg=12, ring=6))
    hp.append(B.box('Blunderbuss_FlintLeather', (.051, -.031, .0750), (.015, .014, .0075), LEATHER, coll, bevel=.001))
    hp.append(flint('Blunderbuss_Flint', (.019, .014, .008), coll, (.051, -.031, .0755)))
    fz = parts['frizzen']
    fz.append(B.box('Blunderbuss_Frizzen', (.070, -.029, .065), (.007, .014, .037), STEEL, coll, bevel=.002))
    fz.append(B.box('Blunderbuss_FrizzenFoot', (.063, -.030, .047), (.018, .014, .005), STEEL, coll, bevel=.0015))
    fz.append(B.box('Blunderbuss_FrizzenToe', (.0745, -.029, .084), (.010, .012, .003), STEEL, coll, bevel=.001))
    R.append(B.tube('Blunderbuss_Guard', [V(-.056, 0, -.020), V(-.048, 0, gz + .017), V(-.004, 0, gz), V(.043, 0, gz + .013),
                                          V(.064, 0, -.028), V(.072, 0, -.011)], [.0052] * 6, BRASS, coll, segs=12))
    parts['trigger'].append(B.tube('Blunderbuss_Trigger', [V(.014, 0, -.017), V(.012, 0, -.037), V(.003, 0, -.050)],
                                   [.0045, .004, .0035], STEEL, coll, segs=10))
    rr_end = mx - .014
    parts['ramrod'].append(B.tube('Blunderbuss_Ramrod', [V(.10, 0, .000), V(rr_end, 0, .0065)], [p['ramrod_r']] * 2, STEEL, coll, segs=10))
    parts['ramrod'].append(B.blob('Blunderbuss_RamrodTip', (rr_end + .002, 0, .0065), (.008, .006, .006), BRASS, coll, seg=12, ring=8))
    parts['ramrod'].append(B.blob('Blunderbuss_RamrodButton', (.098, 0, .000), (.004, .0052, .0052), STEEL, coll, seg=12, ring=8))
    parts['cartridge'] += build_cartridge(coll)

    sockets = {
        'grip_r': ('root', V(0, 0, -.002)), 'grip_l': ('root', V(p['grip_l_x'], 0, .012)),
        'barrel': ('root', V(.30, 0, bz)), 'muzzle': ('root', V(mx + .006, 0, bz)),
        'pan': ('root', V(.062, -.027, .050)), 'butt': ('root', V(p['butt_x'] - .008, 0, -.033)),
        'sling': ('root', V(.06, 0, -.11)), 'ramrod_tip': ('ramrod', V(mx - .010, 0, .0065)),
        'ramrod_end': ('ramrod', V(.115, 0, .0003)), 'flint': ('hammer', V(.051, -.030, .073)),
        'sight_rear': ('root', V(.045, 0, bz + .0195)), 'sight_front': ('root', V(mx - .005, 0, bz + br + .0005)),
    }
    bones = {
        'root': (V(0, 0, 0), V(.10, 0, 0), None),
        'hammer': (V(.020, -.030, .030), V(.020, -.030, .070), 'root'),
        'frizzen': (V(.070, -.027, .046), V(.070, -.027, .086), 'root'),
        'trigger': (V(.014, 0, -.016), V(.006, 0, -.050), 'root'),
        'ramrod': (V(.10, 0, .000), V(rr_end, 0, .0065), 'root'),
        'cartridge': (CARTRIDGE_REST - V(CARTRIDGE_LENGTH * 0.5, 0, 0), CARTRIDGE_REST + V(CARTRIDGE_LENGTH * 0.5, 0, 0), 'root'),
    }
    return {'parts': parts, 'sockets': sockets, 'bones': bones, 'params': p,
            'bounds': {'min': [p['butt_x'] - .01, -.034, -.082], 'max': [mx + .005, .034, bz + br + .002]}}


def build_ammo(coll):
    """SM_Ammo_ShotBag: a linen bag of lead shot tied with a leather thong, standing, a few pellets spilt at its foot.
    Pivot at the bottom centre (z = 0)."""
    MATS.ensure_all()
    B = K.bean()
    parts = []
    # gathered bag: round belly, neck, flared mouth (tube along Z)
    pts = [V(0, 0, .003), V(0, 0, .010), V(0, 0, .022), V(0, 0, .034), V(0, 0, .042), V(0, 0, .048), V(0, 0, .054)]
    radii = [.012, .0185, .0205, .017, .011, .0095, .0125]
    bag = B.tube('ShotBag_Body', pts, radii, LINEN, coll, segs=22)
    parts.append(bag)
    parts.append(B.tube('ShotBag_Thong', [V(-.016, 0, .044), V(0, .016, .0445), V(.016, 0, .045), V(0, -.016, .0455), V(-.016, .002, .046)],
                        [.0014] * 5, LEATHER, coll, segs=8))
    parts.append(B.tube('ShotBag_ThongEnd', [V(-.016, .002, .046), V(-.024, .010, .036), V(-.028, .014, .024)], [.0014, .0013, .0011],
                        LEATHER, coll, segs=8))
    for i, (x, y, r) in enumerate(((.026, .004, .0032), (.021, -.012, .0032), (-.019, -.017, .003), (.014, .018, .003))):
        parts.append(B.blob(f'ShotBag_Pellet{i}', (x, y, r), (r, r, r), LEAD, coll, seg=12, ring=8))
    for i, (y, z) in enumerate(((.004, .056), (-.0035, .0565), (.0, .0585))):
        parts.append(B.blob(f'ShotBag_MouthPellet{i}', (0.002 * i - .002, y, z), (.0031, .0031, .0031), LEAD, coll, seg=10, ring=6))
    return [{'name': 'SM_Ammo_ShotBag', 'parts': parts, 'bake': 1024, 'size_m': [.058, .042, .062]}]
