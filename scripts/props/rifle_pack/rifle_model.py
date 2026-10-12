"""Baker-type flintlock rifle of the bean (T-250, owner request 2026-10-09): browned barrel with an octagonal breech,
seven-groove bore, folding rear sight and front blade, sword bayonet bar, walnut full stock with a raised cheek piece,
brass furniture with the scroll trigger guard and the hinged PATCH BOX in the butt, 1.15 m. Same bone names, pivots and
socket names as the musket pack so APirateMusketWeapon plays its clips with another DataAsset; two extra bones:
"patchbox_lid" (hinged brass lid, opened during the reload) and "patch" (the greased patch wrapped around a ball, taken
from the box and set on the muzzle). The ammunition item is SM_Ammo_RifleCartridge (paper cartridge: powder and a
patched ball).

Weapon local: +X to the muzzle, +Z up, -Y is the lock side. Metres. Materials: scripts/props/longarm_common/materials.py.
"""
import math

import bpy
from mathutils import Vector

import kit as K
import materials as MATS

V = K.V
WALNUT, DARK = 'MAT-LA-Walnut', 'MAT-LA-WalnutDark'
STEEL, GREY, BRASS, BRIGHT = 'MAT-LA-BrownSteel', 'MAT-LA-GreySteel', 'MAT-LA-Brass', 'MAT-LA-BrassBright'
LEATHER, LINEN, PAPER, LEAD = 'MAT-LA-Leather', 'MAT-LA-Linen', 'MAT-LA-Paper', 'MAT-LA-Lead'

CARTRIDGE_REST = Vector((-0.215, 0.0, -0.030))
CARTRIDGE_LENGTH = 0.054
CARTRIDGE_RADIUS = 0.0072
# the patched ball at rest: inside the butt, next to the box
PATCH_REST = Vector((-0.200, 0.0, -0.036))
# patch box: centre of the lid, hinge at its rear end (vertical pin), on the right side of the butt
PATCHBOX = Vector((-0.262, -0.0275, -0.038))
PATCHBOX_HINGE = Vector((-0.316, -0.0278, -0.038))


def params(p=None):
    d = {'butt_x': -0.35, 'muzzle_x': 0.80, 'breech_x': 0.03, 'barrel_z': 0.045, 'barrel_r': 0.0168, 'muzzle_r': 0.0135,
         'bore_r': 0.0078, 'butt_hw': 0.027, 'butt_hh': 0.060, 'grip_l_x': 0.30, 'guard_bottom_z': -0.080,
         'ramrod_r': 0.0052, 'fore_end_x': 0.735, 'sling': True}
    d.update(p or {})
    return d


def flint(name, size, coll, at):
    ob = K.flint_chip(name, size, coll)
    ob.data.materials[0] = bpy.data.materials['MAT-LA-Flint']
    ob.location = at
    return ob


def octagon(name, x0, x1, radius, bz, mat, coll):
    """Octagonal breech section with a flat on top (kit.cylinder_x turned by 22.5 deg)."""
    ob = K.cylinder_x(name, x0, x1, radius, (0.0, 0.0), mat, coll, segs=8)
    ob.location = (0, 0, bz)
    ob.rotation_euler = (math.radians(22.5), 0, 0)
    return ob


def build_cartridge(coll):
    B = K.bean()
    c, half, r = CARTRIDGE_REST, CARTRIDGE_LENGTH * 0.5, CARTRIDGE_RADIUS
    parts = [B.tube('Rifle_Cartridge', [c + V(-half, 0, 0), c + V(half * 0.4, 0, 0), c + V(half, 0, 0)], [r * 0.8, r, r * 0.95], PAPER,
                    coll, segs=14)]
    parts.append(B.blob('Rifle_CartridgeBall', c + V(half * 0.72, 0, 0), (r * 1.05, r * 1.08, r * 1.08), PAPER, coll, seg=16, ring=10))
    parts.append(B.tube('Rifle_CartridgeTwist', [c + V(-half - 0.009, 0, 0), c + V(-half + 0.002, 0, 0)], [r * 0.25, r * 0.55], PAPER,
                        coll, segs=10))
    parts.append(B.tube('Rifle_CartridgeTie', [c + V(-half + 0.004, 0, 0), c + V(-half + 0.0065, 0, 0)], [r * 0.86, r * 0.86], LINEN,
                        coll, segs=14))
    return parts


def build_patch(coll):
    """Greased linen patch folded under a lead ball (the +X end is the ball: it goes into the muzzle first)."""
    B = K.bean()
    c = PATCH_REST
    parts = [B.blob('Rifle_PatchBall', c + V(0.004, 0, 0), (.0078, .0078, .0078), LEAD, coll, seg=16, ring=10),
             B.blob('Rifle_Patch', c + V(-0.004, 0, 0), (.0075, .0135, .0135), LINEN, coll, seg=18, ring=10),
             B.tube('Rifle_PatchTail', [c + V(-0.008, 0.004, 0), c + V(-0.018, 0.009, -0.003)], [.004, .0022], LINEN, coll, segs=10)]
    return parts


def build_rifle(coll, p=None):
    MATS.ensure_all()
    B = K.bean()
    p = params(p)
    bz, bx0, mx = p['barrel_z'], p['breech_x'], p['muzzle_x']
    parts = {'root': [], 'hammer': [], 'frizzen': [], 'trigger': [], 'ramrod': [], 'cartridge': [], 'patchbox_lid': [], 'patch': []}
    R = parts['root']
    bh, bw, gz = p['butt_hh'], p['butt_hw'], p['guard_bottom_z']

    # full walnut stock, deep butt, slim fore-end to the nose cap
    stations = [
        (p['butt_x'], bw, bh, -.036), (p['butt_x'] + .018, bw * 1.03, bh * 1.02, -.034),
        (-.30, bw * .99, bh * .93, -.030), (-.25, bw * .96, bh * .84, -.025),
        (-.19, bw * .88, bh * .72, -.018), (-.13, .023, .028, -.011),
        (-.07, .0205, .024, -.004), (-.02, .020, .023, .003),
        (.05, .021, .024, .013), (.14, .0205, .021, .019),
        (.30, .0195, .019, .023), (.50, .018, .0165, .025),
        (.66, .0165, .0145, .026), (.72, .015, .013, .027), (p['fore_end_x'], .0135, .012, .027),
    ]
    R.append(K.loft('Rifle_Stock', stations, WALNUT, coll, segs=40, expo=2.9, subsurf=1))
    R.append(B.box('Rifle_Cheekpiece', (-.215, .0235, .012), (.130, .009, .038), DARK, coll, bevel=.010))
    R.append(B.box('Rifle_ButtPlate', (p['butt_x'] - .004, 0, -.036), (.009, bw * 2 + .007, bh * 2 + .008), BRASS, coll, bevel=.0035))
    R.append(B.box('Rifle_ButtTang', (p['butt_x'] + .030, 0, -.036 + bh * 1.0), (.070, .012, .004), BRASS, coll, bevel=.0015))
    for i, z in enumerate((-.036 + bh * .55, -.036 - bh * .55)):
        R.append(K.disc(f'Rifle_ButtScrew{i}', (p['butt_x'] - .0088, 0, z), .0035, .0014, (-1, 0, 0), STEEL, coll, segs=14))

    # patch box: recess and the stack of greased patches on the root; the brass lid and its hinge pin on patchbox_lid
    R.append(B.box('Rifle_PatchBoxRecess', (PATCHBOX.x, -.0238, PATCHBOX.z), (.100, .007, .040), DARK, coll, bevel=0))
    R.append(B.box('Rifle_PatchStack', (PATCHBOX.x, -.0232, PATCHBOX.z), (.070, .004, .030), LINEN, coll, bevel=.001))
    lid = parts['patchbox_lid']
    lid.append(B.box('Rifle_PatchBoxLid', (PATCHBOX.x, PATCHBOX.y, PATCHBOX.z), (.108, .0025, .046), BRASS, coll, bevel=.004))
    lid.append(B.tube('Rifle_PatchBoxHinge', [PATCHBOX_HINGE + V(0, 0, -.024), PATCHBOX_HINGE + V(0, 0, .024)], [.0028, .0028], BRASS, coll, segs=10))
    lid.append(K.disc('Rifle_PatchBoxCatch', (PATCHBOX.x + .046, PATCHBOX.y - .0012, PATCHBOX.z), .0035, .0014, (0, -1, 0), BRIGHT, coll, segs=12))

    # barrel: octagonal breech, round browned barrel, crown, bore, breech line, tang
    R.append(octagon('Rifle_Breech', bx0, .145, .0188, bz, STEEL, coll))
    R.append(B.tube('Rifle_Barrel', [V(.13, 0, bz), V(.30, 0, bz), V(.55, 0, bz), V(mx - .02, 0, bz), V(mx, 0, bz)],
                    [p['barrel_r'], .0158, .0145, .0137, p['muzzle_r']], STEEL, coll, segs=16))
    R.append(B.tube('Rifle_MuzzleCrown', [V(mx - .016, 0, bz), V(mx - .004, 0, bz)], [.0142, .0142], GREY, coll, segs=16))
    R.append(B.blob('Rifle_Bore', (mx + .001, 0, bz), (.003, p['bore_r'], p['bore_r']), 'MAT-LA-Bore', coll, seg=24, ring=12))
    R.append(B.box('Rifle_Tang', (bx0 - .009, 0, bz - .010), (.065, .028, .013), STEEL, coll, bevel=.005))
    R.append(K.disc('Rifle_TangScrew', (bx0 - .025, 0, bz - .0034), .0034, .0014, (0, 0, 1), GREY, coll, segs=14))
    # sights: block and folding leaf with its notch at the breech, brass blade at the muzzle; sword bayonet bar
    R.append(B.box('Rifle_RearSightBlock', (.165, 0, bz + .0168 + .003), (.013, .012, .006), STEEL, coll, bevel=.0012))
    R.append(B.box('Rifle_RearSightLeaf', (.165, 0, bz + .0168 + .006 + .0035), (.0022, .012, .007), GREY, coll, bevel=.0005))
    R.append(B.box('Rifle_RearSightNotch', (.165, 0, bz + .0168 + .006 + .0065), (.0026, .0026, .0022), 'MAT-LA-Bore', coll, bevel=0))
    R.append(B.blob('Rifle_FrontSight', (mx - .035, 0, p['muzzle_r'] + bz + .0045), (.0065, .0018, .0050), BRASS, coll, seg=12, ring=8))
    R.append(B.box('Rifle_SwordBar', (mx - .07, -.0215, .040), (.085, .004, .009), STEEL, coll, bevel=.001))
    R.append(B.box('Rifle_SwordLug', (mx - .105, -.019, .040), (.012, .006, .006), STEEL, coll, bevel=.001))

    # lock (browned): plate, tail, pan, frizzen spring and screw, bridle, tumbler screw; brass side plate with the screws
    R.append(B.box('Rifle_LockPlate', (.047, -.024, .033), (.110, .006, .043), STEEL, coll, bevel=.004))
    R.append(B.box('Rifle_LockTail', (-.014, -.0235, .030), (.030, .005, .022), STEEL, coll, bevel=.003))
    R.append(B.box('Rifle_Pan', (.070, -.029, .047), (.027, .012, .009), 'MAT-LA-Bore', coll, bevel=.0025))
    R.append(B.box('Rifle_FrizzenSpring', (.090, -.0285, .028), (.032, .004, .006), GREY, coll, bevel=.0012))
    R.append(K.disc('Rifle_FrizzenSpringScrew', (.103, -.0275, .028), .0032, .0016, (0, -1, 0), BRASS, coll, segs=14))
    R.append(B.box('Rifle_PanBridle', (.084, -.0305, .045), (.010, .003, .006), GREY, coll, bevel=.001))
    R.append(K.disc('Rifle_TumblerScrew', (.022, -.0272, .019), .0040, .0016, (0, -1, 0), GREY, coll, segs=16))
    R.append(K.disc('Rifle_LockScrewR0', (.000, -.0262, .036), .0032, .0016, (0, -1, 0), GREY, coll, segs=14))
    R.append(B.tube('Rifle_SidePlate', [V(-.030, .0205, .022), V(-.005, .0215, .030), V(.035, .0225, .024), V(.070, .0222, .018),
                                        V(.090, .0216, .021)], [.0040, .0050, .0055, .0045, .0030], BRASS, coll, segs=10, squash=.35))
    for i, x in enumerate((.010, .062)):
        R.append(K.disc(f'Rifle_LockScrew{i}', (x, .0262, .025), .0042, .0018, (0, 1, 0), GREY, coll, segs=16))
    R.append(K.disc('Rifle_Escutcheon', (-.040, 0, .0262), .0085, .0016, (0, 0, 1), BRASS, coll, segs=24))
    R.append(B.box('Rifle_TriggerPlate', (.004, 0, -.0225), (.060, .010, .003), BRASS, coll, bevel=.0012))

    # furniture: nose cap, barrel keys, tail pipe and three ramrod pipes, swivels and sling, stock pins
    R.append(K.ring('Rifle_NoseCap', (p['fore_end_x'], 0, .030), .0150, .0030, BRASS, coll, axis='X', segs=28, minor_segs=8))
    for i, x in enumerate((.30, .50, .66)):
        R.append(B.box(f'Rifle_BarrelKey{i}', (x, 0, .030), (.006, .044, .003), GREY, coll, bevel=.001))
    R.append(B.tube('Rifle_TailPipe', [V(.160, 0, .0025), V(.215, 0, .0040)], [.0072, .0068], BRASS, coll, segs=14))
    for x in (.26, .42, .60):
        R.append(K.ring(f'Rifle_RamrodPipe{int(x * 100)}', (x, 0, .006), .0068, .0018, BRASS, coll, axis='X', segs=16, minor_segs=6))
    if p.get('sling', True):
        R.append(K.ring('Rifle_SwivelF', (.50, 0, -.006), .008, .0018, BRASS, coll, axis='Y', segs=20, minor_segs=7))
        R.append(K.ring('Rifle_SwivelB', (-.15, 0, -.070), .008, .0018, BRASS, coll, axis='Y', segs=20, minor_segs=7))
        R.append(B.tube('Rifle_Sling', [V(.50, 0, -.014), V(.36, 0, -.105), V(.15, 0, -.145), V(-.02, 0, -.145), V(-.15, 0, -.083)],
                        [.011, .013, .014, .013, .011], LEATHER, coll, segs=12, squash=.26))
    for x in (-.30, -.17, -.045, .10):
        R.append(B.blob(f'Rifle_StockPin{x:+.3f}', (x, -.023, .005), (.0035, .002, .0035), BRASS, coll, seg=10, ring=6))

    # flintlock: hammer, frizzen, trigger, scroll guard; ramrod
    hp = parts['hammer']
    hp.append(B.tube('Rifle_Hammer', [V(.022, -.032, .031), V(.010, -.032, .049), V(.022, -.032, .069), V(.041, -.032, .078)],
                     [.0075, .0065, .006, .0058], GREY, coll, segs=12))
    hp.append(B.blob('Rifle_HammerPivot', (.022, -.034, .031), (.010, .004, .010), BRASS, coll, seg=16, ring=8))
    hp.append(B.box('Rifle_JawTop', (.048, -.032, .086), (.020, .013, .005), GREY, coll, bevel=.0015))
    hp.append(B.box('Rifle_JawLow', (.048, -.032, .067), (.020, .013, .005), GREY, coll, bevel=.0015))
    hp.append(B.tube('Rifle_JawScrew', [V(.048, -.033, .064), V(.048, -.033, .097)], [.003, .003], BRASS, coll, segs=10))
    hp.append(B.blob('Rifle_JawScrewHead', (.048, -.033, .098), (.006, .006, .003), BRASS, coll, seg=12, ring=6))
    hp.append(B.box('Rifle_FlintLeather', (.054, -.031, .0765), (.016, .014, .0075), LEATHER, coll, bevel=.001))
    hp.append(flint('Rifle_Flint', (.020, .014, .0085), coll, (.054, -.031, .077)))
    fz = parts['frizzen']
    fz.append(B.box('Rifle_Frizzen', (.076, -.029, .066), (.007, .014, .038), GREY, coll, bevel=.002))
    fz.append(B.box('Rifle_FrizzenFoot', (.068, -.030, .048), (.019, .014, .005), STEEL, coll, bevel=.0015))
    fz.append(B.box('Rifle_FrizzenToe', (.0805, -.029, .086), (.010, .012, .003), GREY, coll, bevel=.001))
    R.append(B.tube('Rifle_Guard', [V(.076, 0, -.011), V(.067, 0, -.029), V(.045, 0, gz + .013), V(-.004, 0, gz), V(-.050, 0, gz + .017),
                                    V(-.064, 0, -.030), V(-.082, 0, -.034), V(-.106, 0, -.047), V(-.123, 0, -.063), V(-.131, 0, -.061)],
                    [.0055] * 10, BRASS, coll, segs=12))
    parts['trigger'].append(B.tube('Rifle_Trigger', [V(.014, 0, -.017), V(.012, 0, -.038), V(.002, 0, -.052)], [.0045, .004, .0035], GREY,
                                   coll, segs=10))
    rr_end = mx - .012
    parts['ramrod'].append(B.tube('Rifle_Ramrod', [V(.10, 0, .000), V(rr_end, 0, .008)], [p['ramrod_r']] * 2, GREY, coll, segs=10))
    parts['ramrod'].append(B.blob('Rifle_RamrodTip', (rr_end + .002, 0, .008), (.009, .0065, .0065), BRASS, coll, seg=12, ring=8))
    parts['ramrod'].append(B.blob('Rifle_RamrodButton', (.098, 0, .000), (.004, .0055, .0055), GREY, coll, seg=12, ring=8))
    parts['cartridge'] += build_cartridge(coll)
    parts['patch'] += build_patch(coll)

    sockets = {
        'grip_r': ('root', V(0, 0, -.002)), 'grip_l': ('root', V(p['grip_l_x'], 0, .012)),
        'barrel': ('root', V(.55, 0, bz)), 'muzzle': ('root', V(mx + .006, 0, bz)),
        'pan': ('root', V(.068, -.027, .050)), 'butt': ('root', V(p['butt_x'] - .008, 0, -.036)),
        'sling': ('root', V(.08, 0, -.135)), 'ramrod_tip': ('ramrod', V(mx - .008, 0, .008)),
        'ramrod_end': ('ramrod', V(.115, 0, .0003)), 'flint': ('hammer', V(.054, -.030, .074)),
        'sight_rear': ('root', V(.165, 0, bz + .0168 + .006 + .0055)), 'sight_front': ('root', V(mx - .035, 0, p['muzzle_r'] + bz + .0095)),
        'patchbox': ('root', PATCHBOX.copy()),
    }
    bones = {
        'root': (V(0, 0, 0), V(.10, 0, 0), None),
        'hammer': (V(.022, -.030, .030), V(.022, -.030, .070), 'root'),
        'frizzen': (V(.076, -.027, .046), V(.076, -.027, .086), 'root'),
        'trigger': (V(.014, 0, -.016), V(.006, 0, -.050), 'root'),
        'ramrod': (V(.10, 0, .000), V(rr_end, 0, .008), 'root'),
        'cartridge': (CARTRIDGE_REST - V(CARTRIDGE_LENGTH * 0.5, 0, 0), CARTRIDGE_REST + V(CARTRIDGE_LENGTH * 0.5, 0, 0), 'root'),
        'patchbox_lid': (PATCHBOX_HINGE + V(0, 0, -.022), PATCHBOX_HINGE + V(0, 0, .022), 'root'),
        'patch': (PATCH_REST - V(.012, 0, 0), PATCH_REST + V(.012, 0, 0), 'root'),
    }
    return {'parts': parts, 'sockets': sockets, 'bones': bones, 'params': p,
            'bounds': {'min': [p['butt_x'] - .01, -.035, -.150], 'max': [mx + .005, .035, bz + .0168 + .0135]}}


def build_ammo(coll):
    """SM_Ammo_RifleCartridge: a paper cartridge lying on its side (powder tube, tied neck, the patched ball bulging at
    one end), pivot at the bottom centre."""
    MATS.ensure_all()
    B = K.bean()
    r = .0078
    z = r * 1.1
    parts = [B.tube('RifleCartridge_Paper', [V(-.030, 0, z), V(-.012, 0, z), V(.008, 0, z), V(.020, 0, z)], [r * .78, r, r * .95, r * .9], PAPER,
                    coll, segs=16)]
    parts.append(B.tube('RifleCartridge_Twist', [V(-.039, 0, z), V(-.029, 0, z)], [r * .25, r * .55], PAPER, coll, segs=10))
    parts.append(B.blob('RifleCartridge_Ball', (.021, 0, z), (r * 1.12, r * 1.14, r * 1.14), PAPER, coll, seg=18, ring=12))
    parts.append(B.tube('RifleCartridge_Tie', [V(.0105, 0, z), V(.0135, 0, z)], [r * .9, r * .9], LINEN, coll, segs=14))
    parts.append(B.tube('RifleCartridge_Tie2', [V(-.027, 0, z), V(-.024, 0, z)], [r * .84, r * .84], LINEN, coll, segs=14))
    return [{'name': 'SM_Ammo_RifleCartridge', 'parts': parts, 'bake': 1024, 'size_m': [.070, .018, .018]}]
