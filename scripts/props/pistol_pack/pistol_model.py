"""Naval flintlock pistol of the bean, v2 realistic (T-248): exterior game geometry, single and double barrel.

Weapon coordinates (as the v0.1 pack): +X forward to the muzzle, +Z up, -Y lock side (right side), right grip at the
origin, bore axis at z = BORE_Z. Length 0.36 m (v0.1 contract: 0.32-0.38 m). Parts are grouped per bone:
  gun_root   stock (walnut), barrel(s), breech tang, lock plate(s), pan, frizzen spring, side plate, screws, butt cap,
             escutcheon, trigger guard, ramrod pipes, fore-end cap, belt hook (single)
  hammer     cock with its jaws, jaw screw, flint and leather, comb  (hammer2: left lock of the double pistol)
  frizzen    frizzen face, pan cover, curl                           (frizzen2: left lock of the double pistol)
  trigger    trigger blade(s)
  ramrod     wooden ramrod with brass tip and rear ferrule
  charge_visual  paper cartridge shown in the hand during Reload (hidden by scale otherwise)
The rest pose of the cock is FULL COCK (as v0.1): Fire rotates it HAMMER_FIRED degrees forward about +Y at its pivot,
the frizzen opens FRIZZEN_OPEN degrees forward about its own pivot.
"""
from __future__ import annotations

import json
import math

import bpy
from mathutils import Vector

import meshkit as K

BORE_Z = 0.071
MUZZLE_X = 0.270
SIGHT_LINE_Z = 0.0865          # rear notch -> front bead, above the bore (aim framing)
HAMMER_FIRED = 50.0            # deg, forward about +Y at HAMMER_PIVOT
FRIZZEN_OPEN = 62.0
BARREL_Y = {'single': [0.0], 'double': [-0.0125, 0.0125]}
HAMMER_PIVOT = Vector((0.032, -0.0215, 0.056))      # right lock (mirror in Y for the left lock)
FRIZZEN_PIVOT = Vector((0.0825, -0.0265, 0.064))
TRIGGER_PIVOT = Vector((0.052, 0.0, 0.030))
ROD_Z = 0.0345
ROD_X0, ROD_X1 = 0.070, 0.262   # rear end (inside the stock) and brass tip end
CARTRIDGE_LENGTH, CARTRIDGE_R = 0.046, 0.0078


def sockets(variant):
    ys = BARREL_Y[variant]
    s = {
        'socket_grip_r': (0, 0, 0),
        'socket_muzzle': (MUZZLE_X, ys[0], BORE_Z),
        'socket_load_port': (MUZZLE_X, ys[0], BORE_Z),
        'socket_hammer': (0.044, -0.027, 0.108),
        'socket_ramrod_grip': (ROD_X1 - 0.012, 0.0, ROD_Z),
    }
    if variant == 'double':
        s['socket_muzzle2'] = (MUZZLE_X, ys[1], BORE_Z)
        s['socket_hammer2'] = (0.044, 0.027, 0.108)
    return s


def bone_heads(variant):
    b = {'gun_root': (0, 0, 0), 'hammer': tuple(HAMMER_PIVOT), 'frizzen': tuple(FRIZZEN_PIVOT), 'trigger': tuple(TRIGGER_PIVOT),
         'ramrod': (ROD_X1, 0.0, ROD_Z), 'charge_visual': (MUZZLE_X, BARREL_Y[variant][0], BORE_Z)}
    if variant == 'double':
        b['hammer2'] = (HAMMER_PIVOT.x, -HAMMER_PIVOT.y, HAMMER_PIVOT.z)
        b['frizzen2'] = (FRIZZEN_PIVOT.x, -FRIZZEN_PIVOT.y, FRIZZEN_PIVOT.z)
    return b


SOCKET_PARENT = {'socket_hammer': 'hammer', 'socket_hammer2': 'hammer2', 'socket_ramrod_grip': 'ramrod'}
MESH_OF_BONE = {'gun_root': 'Body', 'hammer': 'Hammer', 'hammer2': 'Hammer2', 'frizzen': 'Frizzen', 'frizzen2': 'Frizzen2',
                'trigger': 'Trigger', 'ramrod': 'Ramrod', 'charge_visual': 'ChargeVisual'}


# ------------------------------------------------------------------------------------------------ helpers

def catmull(points, n=6, closed=True):
    """Smooth closed outline through 2D control points (Catmull-Rom)."""
    pts = [Vector(p) for p in points]
    out = []
    m = len(pts)
    rng = range(m) if closed else range(m - 1)
    for i in rng:
        p0, p1, p2, p3 = (pts[(i - 1) % m], pts[i], pts[(i + 1) % m], pts[(i + 2) % m]) if closed else \
            (pts[max(i - 1, 0)], pts[i], pts[i + 1], pts[min(i + 2, m - 1)])
        for k in range(n):
            t = k / n
            out.append(tuple(.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                                   + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t)))
    if not closed:
        out.append(tuple(pts[-1]))
    return out


def rig(collection, prefix, variant):
    data = bpy.data.armatures.new('ARM-' + prefix)
    arm = bpy.data.objects.new('ARM-' + prefix, data)
    collection.objects.link(arm)
    for o in bpy.context.selected_objects:
        o.select_set(False)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    heads = {**bone_heads(variant), **sockets(variant)}
    with bpy.context.temp_override(active_object=arm, selected_objects=[arm], selected_editable_objects=[arm]):
        bpy.ops.object.mode_set(mode='EDIT')
        for name, head in heads.items():
            b = data.edit_bones.new(name)
            b.head = head
            b.tail = Vector(head) + Vector((.03, 0, 0))
            b.use_deform = not name.startswith('socket_')
            if name != 'gun_root':
                b.parent = data.edit_bones[SOCKET_PARENT.get(name, 'gun_root')]
        bpy.ops.object.mode_set(mode='OBJECT')
    arm.show_in_front = True
    arm.display_type = 'WIRE'
    arm['asset_role'] = 'naval_flintlock_pistol_' + variant
    arm['forward_axis'] = '+X'
    return arm


# ------------------------------------------------------------------------------------------------ parts

def stock(b, variant):
    """Full walnut stock: fore-end and lock area lofted along X, curved grip to the butt."""
    wide = 0.012 if variant == 'double' else 0.0
    # fore-end under the barrel(s): top at the bore centre line, ramrod groove underneath
    fore = [((.262, 0, .052), .0135 + wide, .017, 0), ((.230, 0, .052), .0142 + wide, .018, 0),
            ((.180, 0, .051), .0150 + wide, .019, 0), ((.120, 0, .050), .0160 + wide, .021, 0),
            ((.095, 0, .050), .0172 + wide, .022, 0), ((.075, 0, .050), .0180 + wide, .0225, 0),
            ((.030, 0, .050), .0182 + wide, .0225, 0), ((.000, 0, .048), .0178 + wide, .0215, 0),
            ((-.012, 0, .044), .0165 + wide * .6, .0205, 0)]
    b.loft('ForeStock', fore, 'wood', seg=36, expo=2.9)
    # grip: strongly curved, swelling to the bulbous naval butt
    grip = [((-.004, 0, .041), .0160, .0190, 0), ((-.016, 0, .020), .0165, .0195, 0), ((-.031, 0, -.005), .0175, .0205, 0),
            ((-.047, 0, -.031), .0190, .0220, 0), ((-.062, 0, -.054), .0205, .0235, 0), ((-.073, 0, -.069), .0205, .0230, 0),
            ((-.080, 0, -.078), .0170, .0190, 0)]
    b.loft('Grip', grip, 'wood', seg=32, expo=2.2)
    # lock flats: the plate beds stand slightly proud of the stock (right: lock, left: side plate or second lock)
    for side in ((-1, 1) if variant == 'double' else (-1,)):
        b.plate('LockBed-%d' % side, catmull([(-.014, .049), (-.010, .060), (.030, .0715), (.096, .0725), (.108, .062),
                                              (.106, .044), (.070, .035), (.030, .035), (-.006, .040)], 5),
                side * (.0182 + wide) - .001 * side, side * .0025, 'wood', bevel=.0006)
    return b


def butt_cap(b):
    """Brass cap over the bulb of the grip, two long ears up the sides, a rope border and a pommel stud."""
    cap = [((-.0665, 0, -.0605), .0215, .0245, 0), ((-.074, 0, -.0705), .0215, .0242, 0), ((-.0805, 0, -.0785), .0180, .0200, 0),
           ((-.0865, 0, -.0855), .0105, .0115, 0)]
    b.loft('ButtCap', cap, 'brass', seg=32, expo=2.2)
    b.ellipsoid('PommelStud', (-.0885, 0, -.0875), (.0055, .0070, .0070), 'brass', seg=16, ring=8)
    # rope border where the cap meets the wood
    ring = []
    for j in range(33):
        a = j * K.TAU / 32
        cs, sn = math.cos(a), math.sin(a)
        y = .0225 * math.copysign(abs(cs) ** (2 / 2.2), cs)
        z = .0255 * math.copysign(abs(sn) ** (2 / 2.2), sn)
        t = Vector((-.0665, 0, -.0605))
        n = Vector((.52, 0, .85)).normalized()   # section normal
        bv = n.cross(Vector((0, 1, 0))).normalized()
        ring.append(t + Vector((0, 1, 0)) * y + bv * z)
    b.tube('CapBorder', ring, .0014, 'brass', seg=8, caps=False)
    for side in (-1, 1):
        ear = [(-.064, side * .0205, -.056), (-.053, side * .0200, -.040), (-.042, side * .0192, -.024), (-.033, side * .0185, -.010)]
        b.tube('CapEar-%d' % side, ear, [.0032, .0030, .0026, .0018], 'brass', seg=8)
        b.screw('CapEarScrew-%d' % side, (-.049, side * .0225, -.034), .0028, (0, side, 0), 'bright', slot_angle=.6)


def barrels(b, variant):
    for i, y in enumerate(BARREL_Y[variant]):
        sfx = '' if i == 0 else '2'
        breech_r = .0135 if variant == 'single' else .0118
        bore_r = .0075 if variant == 'single' else .0068
        # octagonal breech, round swamped barrel, slight muzzle flare
        b.rings('Breech' + sfx, [(.034, breech_r, breech_r), (.078, breech_r, breech_r)], (0, y, BORE_Z), 'steel', seg=8, flat=True)
        b.rings('BreechStep' + sfx, [(.076, breech_r * .98, breech_r * .98), (.084, breech_r * .92, breech_r * .92)], (0, y, BORE_Z),
                'steel', seg=28, caps=False)
        b.rings('Barrel' + sfx, [(.083, breech_r * .92, breech_r * .92), (.14, breech_r * .86, breech_r * .86),
                                 (.22, breech_r * .80, breech_r * .80), (.252, breech_r * .80, breech_r * .80),
                                 (.263, breech_r * .88, breech_r * .88), (MUZZLE_X, breech_r * .90, breech_r * .90)],
                (0, y, BORE_Z), 'steel', seg=28, caps=False)
        # muzzle: crown ring, bore going in (dark), bright chamfer
        b.rings('MuzzleFace' + sfx, [(MUZZLE_X, breech_r * .90, breech_r * .90), (MUZZLE_X, bore_r * 1.15, bore_r * 1.15)],
                (0, y, BORE_Z), 'bright', seg=28, caps=False)
        b.rings('Bore' + sfx, [(MUZZLE_X, bore_r * 1.15, bore_r * 1.15), (MUZZLE_X - .003, bore_r, bore_r),
                               (MUZZLE_X - .030, bore_r, bore_r), (MUZZLE_X - .031, .0005, .0005)], (0, y, BORE_Z),
                'recess', seg=20, caps=False)
    # breech plug tang along the wrist, tang screw; sights on the (right) barrel's line
    y0 = BARREL_Y[variant][0]
    b.plate_xy('Tang', [(.036, -.0055), (.036, .0055), (.004, .0042), (-.010, .0030), (-.018, 0), (-.010, -.0030), (.004, -.0042)],
               .0795, .0035, 'steel', bevel=.0006)
    b.screw('TangScrew', (.006, 0, .0835), .0032, (0, 0, 1), 'bright', slot_angle=1.2)
    b.box('RearSight', (.040, y0, .0855), (.010, .011, .004), 'steel', bevel=.0008)
    b.box('RearSightNotch', (.040, y0, .0878), (.011, .0028, .0025), 'recess', bevel=0)
    b.ellipsoid('FrontSight', (.256, y0, .0838), (.0030, .0024, .0032), 'brass', seg=12, ring=6)
    if variant == 'double':
        b.box('TopRib', (.16, 0, BORE_Z + .007), (.21, .006, .009), 'steel', bevel=.0015)
        b.box('BottomRib', (.16, 0, BORE_Z - .009), (.21, .006, .006), 'steel', bevel=.0012)


def lock(b, side, suffix, bone_h, bone_f, variant):
    """One flintlock on side -1 (right) or +1 (left): plate, pan, frizzen spring, screws; cock and frizzen on
    their own bones."""
    wide = 0.012 if variant == 'double' else 0.0
    y = side * (.0182 + wide + .0015)                    # lock bed outer face
    ty = side * .0035                                     # plate thickness, outwards
    outline = [(-.012, .050), (-.008, .0585), (.030, .0695), (.095, .0705), (.104, .062), (.102, .046), (.070, .038),
               (.030, .037), (-.004, .042)]
    if side > 0:
        outline = [(x, z) for x, z in reversed(outline)]
    b.plate('LockPlate' + suffix, catmull(outline, 5), y, ty, 'steel', bevel=.0009)
    yo = y + ty                                           # outer plate face
    n = (0, side, 0)
    # pan with its bowl, bridle, touch hole; frizzen spring (V) under it with its screw; sear and tail screws
    b.box('Pan' + suffix, (.070, yo + side * .0045, .0705), (.015, .010, .0075), 'steel', bevel=.0012)
    b.box('PanBowl' + suffix, (.070, yo + side * .0045, .0745), (.009, .0055, .002), 'recess', bevel=0)
    b.box('PanBridle' + suffix, (.0795, yo + side * .003, .0665), (.007, .005, .006), 'bright', bevel=.0008)
    b.tube('FrizzenSpring' + suffix, [(.094, yo + side * .0022, .0445), (.078, yo + side * .0022, .0475), (.064, yo + side * .0022, .0535)],
           [.0021, .0024, .0016], 'bright', seg=8)
    b.tube('FrizzenSpringLeaf' + suffix, [(.092, yo + side * .0022, .0455), (.076, yo + side * .0022, .0515), (.068, yo + side * .0022, .0565)],
           [.0013, .0016, .0012], 'bright', seg=8)
    b.screw('SpringScrew' + suffix, (.096, yo + side * .001, .0445), .0027, n, 'bright', slot_angle=.3)
    b.screw('FrizzenScrew' + suffix, (FRIZZEN_PIVOT.x, yo + side * .0015, FRIZZEN_PIVOT.z), .0032, n, 'bright', slot_angle=1.9)
    b.screw('SearScrew' + suffix, (-.001, yo + side * .001, .0475), .0026, n, 'bright', slot_angle=2.4)
    b.screw('TailScrew' + suffix, (.016, yo + side * .001, .0415), .0022, n, 'bright', slot_angle=.9)
    # the cock boss sits on the plate, the cock body in front of it
    pivot = Vector((HAMMER_PIVOT.x, yo, HAMMER_PIVOT.z))
    b.disc('CockBoss' + suffix, pivot + Vector((0, side * .0015, 0)), .0085, .003, n, 'steel', seg=20, dome=.3)
    cy = yo + side * .0055                                 # cock body plane
    neck = [pivot + Vector((0, side * .0055, 0)), (.023, cy, .069), (.019, cy, .084), (.023, cy, .098), (.034, cy, .1068)]
    b.tube('CockNeck' + suffix, neck, [.0078, .0066, .0058, .0054, .0050], 'steel', bone_h, seg=12, up=(0, 1, 0), squash=.55)
    b.tube('CockComb' + suffix, [(.028, cy, .105), (.019, cy, .1125), (.012, cy, .121)], [.0042, .0036, .0026], 'steel', bone_h, seg=8,
           up=(0, 1, 0), squash=.6)
    b.box('LowerJaw' + suffix, (.044, cy, .1065), (.018, .011, .0035), 'bright', bone_h, bevel=.0008)
    b.box('FlintLeather' + suffix, (.046, cy, .1115), (.015, .011, .0065), 'leather', bone_h, bevel=.0012)
    b.chip('Flint' + suffix, (.051, cy, .1115), (.014, .010, .0070), 'flint', bone_h, seed=5)
    b.box('TopJaw' + suffix, (.044, cy, .1165), (.018, .011, .0035), 'bright', bone_h, bevel=.0008)
    b.tube('JawScrew' + suffix, [(.040, cy, .104), (.040, cy, .128)], .0022, 'bright', bone_h, seg=10)
    b.tube('JawScrewLoop' + suffix, [(.040, cy, .128), (.036, cy, .133), (.040, cy, .1375), (.044, cy, .133), (.040, cy, .128)],
           .0014, 'bright', bone_h, seg=8)
    b.screw('TumblerScrew' + suffix, (HAMMER_PIVOT.x, cy + side * .0045, HAMMER_PIVOT.z), .0036, n, 'bright', bone_h, slot_angle=.8)
    # frizzen: face standing over the pan, pan cover, curled top, its foot on the pivot
    fy = yo + side * .0045
    b.box('FrizzenFace' + suffix, (.0815, fy, .0825), (.0050, .0115, .030), 'bright', bone_f, bevel=.0012)
    b.box('PanCover' + suffix, (.0730, fy, .0775), (.019, .0120, .0030), 'steel', bone_f, bevel=.0008)
    b.tube('FrizzenCurl' + suffix, [(.0815, fy, .0965), (.0780, fy, .1000), (.0735, fy, .1005)], [.0032, .0028, .0022], 'bright', bone_f, seg=8)
    b.box('FrizzenFoot' + suffix, (.0825, fy, .0665), (.010, .0105, .0065), 'steel', bone_f, bevel=.0012)


def side_plate_and_hook(b):
    """Left side of the single pistol: serpentine brass side plate with the two side nails, iron belt hook."""
    y = .0182 + .0015
    outline = catmull([(.002, .046), (.008, .058), (.030, .0565), (.052, .060), (.078, .056), (.086, .050), (.078, .044),
                       (.052, .047), (.030, .043), (.010, .041)], 5)
    b.plate('SidePlate', outline, y, .0025, 'brass', bevel=.0006)
    for i, x in enumerate((.014, .070)):
        b.screw('SideNail%d' % i, (x, y + .0025, .051), .0036, (0, 1, 0), 'bright', slot_angle=.4 + i)
    hook = [(.010, .0225, .0515), (-.004, .0205, .044), (-.020, .0200, .022), (-.036, .0205, -.004), (-.052, .0218, -.030),
            (-.064, .0230, -.050), (-.070, .0228, -.062), (-.072, .0210, -.070)]
    b.tube('BeltHook', hook, [.0030, .0026, .0024, .0024, .0024, .0024, .0022, .0016], 'iron', seg=8, up=(0, 1, 0), squash=.55)


def escutcheon(b):
    outline = []
    for j in range(24):
        a = j * K.TAU / 24
        outline.append((-.0115 + .011 * math.cos(a), .0045 * math.sin(a)))
    # oval thumb plate on top of the wrist, slightly tilted with the wrist
    pts = [(x, z) for x, z in outline]
    n = len(pts)
    faces = [tuple(range(n)), tuple(reversed(range(n, 2 * n)))] + [(( (i + 1) % n), i, i + n, ((i + 1) % n) + n) for i in range(n)]
    base = Vector((-.012, 0, .0605))
    up = Vector((.35, 0, .94)).normalized()
    right = Vector((0, 1, 0))
    fwd = right.cross(up)
    pos = [tuple(base + fwd * (x + .0115) + right * z) for x, z in pts] + [tuple(base + fwd * (x + .0115) + right * z + up * .0016) for x, z in pts]
    b.mesh('Escutcheon', pos, faces, 'brass', smooth=False)
    b.ellipsoid('EscutcheonCrown', tuple(base + up * .0024 + fwd * .002), (.0040, .0028, .0012), 'brass', seg=12, ring=6)


def trigger_guard(b, variant):
    path = [(.112, 0, .029), (.098, 0, .023), (.084, 0, .008), (.066, 0, -.004), (.046, 0, -.006), (.030, 0, .002),
            (.020, 0, .013), (.010, 0, .020), (-.002, 0, .020), (-.012, 0, .008), (-.024, 0, -.012), (-.034, 0, -.028)]
    if variant == 'double':
        path = [(.118, 0, .029), (.104, 0, .022), (.090, 0, .005), (.070, 0, -.007), (.040, 0, -.008), (.026, 0, .000),
                (.018, 0, .012), (.010, 0, .020), (-.002, 0, .020), (-.012, 0, .008), (-.024, 0, -.012), (-.034, 0, -.028)]
    b.tube('TriggerGuard', path, [.0024, .0030, .0032, .0032, .0032, .0032, .0030, .0028, .0028, .0026, .0024, .0018], 'brass', seg=10)
    b.ellipsoid('GuardFinial', (.118 if variant == 'double' else .112, 0, .0295), (.012, .0055, .0030), 'brass', seg=12, ring=6)
    b.screw('GuardScrew', (.106 if variant == 'double' else .100, 0, .0245), .0025, (0, 0, -1), 'bright', slot_angle=.5)
    b.box('TriggerPlate', (.044, 0, .0275), (.062 if variant == 'double' else .050, .009, .0025), 'brass', bevel=.0006)


def triggers(b, variant):
    xs = [TRIGGER_PIVOT.x] if variant == 'single' else [TRIGGER_PIVOT.x + .010, TRIGGER_PIVOT.x - .008]
    for i, x in enumerate(xs):
        b.tube('Trigger%d' % i, [(x, 0, .031), (x - .002, 0, .021), (x - .005, 0, .011), (x - .010, 0, .004)],
               [.0030, .0028, .0025, .0018], 'bright', 'trigger', seg=10)


def ramrod(b, variant):
    wide = 0.012 if variant == 'double' else 0.0
    b.rings('RamrodShaft', [(ROD_X0, .0033, .0033), (ROD_X1 - .024, .0033, .0033)], (0, 0, ROD_Z), 'wood', 'ramrod', seg=14)
    b.rings('RamrodTip', [(ROD_X1 - .025, .0036, .0036), (ROD_X1 - .006, .0045, .0045), (ROD_X1, .0040, .0040)], (0, 0, ROD_Z),
            'brass', 'ramrod', seg=16)
    b.rings('RamrodFerrule', [(ROD_X0 + .004, .0036, .0036), (ROD_X0 + .014, .0036, .0036)], (0, 0, ROD_Z), 'brass', 'ramrod', seg=12)
    # pipes (thimbles) on the stock, the entry pipe at the rear of the groove, fore-end cap, barrel wedge
    for i, x in enumerate((.125, .215)):
        b.rings('RamrodPipe%d' % i, [(x - .007, .0056, .0056), (x - .005, .0062, .0062), (x + .005, .0062, .0062), (x + .007, .0056, .0056)],
                (0, 0, ROD_Z), 'brass', seg=16, caps=False)
        b.box('PipeMount%d' % i, (x, 0, ROD_Z + .006), (.012, .009, .008), 'brass', bevel=.001)
    b.rings('EntryPipe', [(.070, .0058, .0058), (.084, .0060, .0060), (.090, .0050, .0050)], (0, 0, ROD_Z), 'brass', seg=16, caps=False)
    cap = [((.258, 0, .052), .0150 + wide, .0188, 0), ((.266, 0, .052), .0150 + wide, .0188, 0)]
    b.loft('ForeEndCap', cap, 'brass', seg=32, expo=2.9)
    b.box('BarrelWedge', (.170, 0, .041), (.007, .036 + 2 * wide, .0028), 'bright', bevel=.0006)
    for side in (-1, 1):
        b.disc('WedgeEsc-%d' % side, (.170, side * (.0165 + wide), .041), .0048, .0012, (0, side, 0), 'brass', seg=14, dome=.2)


def cartridge(b, bone='charge_visual', center=(MUZZLE_X, 0.0, BORE_Z), prefix=''):
    """Paper cartridge along +X centred on center: twisted tail at -X, the ball bulging at +X."""
    c = Vector(center)
    h, r = CARTRIDGE_LENGTH * .5, CARTRIDGE_R
    b.rings(prefix + 'Paper', [(-h, r * .78, r * .78), (-h + .006, r * .96, r * .96), (h * .35, r, r), (h * .62, r * .96, r * .96),
                               (h - .002, r * .70, r * .70), (h, r * .30, r * .30)], c, 'paper', bone, seg=16)
    b.ellipsoid(prefix + 'Ball', (c.x + h * .70, c.y, c.z), (r * 1.04, r * 1.05, r * 1.05), 'paper', bone, seg=16, ring=8)
    b.rings(prefix + 'Twist', [(-h - .010, r * .22, r * .22), (-h - .003, r * .50, r * .50), (-h + .001, r * .78, r * .78)], c, 'paper', bone, seg=10)
    b.rings(prefix + 'Tie', [(-h + .009, r * .98, r * .98), (-h + .0115, r * 1.02, r * 1.02), (-h + .014, r * .98, r * .98)], c, 'cord', bone, seg=14)


# ------------------------------------------------------------------------------------------------ assembly

def create_weapon(collection, materials, prefix='PiratePistol', variant='single'):
    """Builds the weapon in `collection`: armature + one mesh per bone, sharing one UV atlas (packed by
    finalize_atlas). Returns the same dictionary shape as the v0.1 generator."""
    arm = rig(collection, prefix, variant)
    b = K.Builder(collection, prefix, materials)
    stock(b, variant)
    butt_cap(b)
    barrels(b, variant)
    lock(b, -1, '', 'hammer', 'frizzen', variant)
    if variant == 'double':
        lock(b, 1, '2', 'hammer2', 'frizzen2', variant)
    else:
        side_plate_and_hook(b)
    escutcheon(b)
    trigger_guard(b, variant)
    triggers(b, variant)
    ramrod(b, variant)
    cartridge(b, 'charge_visual', (MUZZLE_X, BARREL_Y[variant][0], BORE_Z), 'Charge')
    return {'armature': arm, 'builder': b, 'variant': variant, 'prefix': prefix, 'collection': collection,
            'socket_positions': sockets(variant), 'bone_heads': bone_heads(variant)}


def finalize(weapon, atlas_size, exclude_bones=()):
    """Packs the UV atlas over every part, merges the parts per bone, skins them rigidly. Fills the contract."""
    b = weapon['builder']
    arm = weapon['armature']
    density = K.pack_islands(b.meshes, b.islands, atlas_size)
    grouped = {}
    for ob in b.meshes:
        grouped.setdefault(b.bindings[ob.name], []).append(ob)
    meshes, charge = [], []
    for bone, objects in grouped.items():
        ob = K.combine(objects, weapon['collection'], 'GEO-' + weapon['prefix'] + '-' + MESH_OF_BONE[bone])
        ob['deform_bone'] = bone
        K.skin(ob, arm, bone)
        (charge if bone == 'charge_visual' else meshes).append(ob)
    arm.pose.bones['charge_visual'].scale = (.0001,) * 3
    bpy.context.view_layer.update()
    bb = K.bounds(meshes)
    wt, ct = K.triangles(meshes), K.triangles(charge)
    contract = {'forward_axis': '+X', 'up_axis': '+Z', 'primary_hand': 'right', 'variant': weapon['variant'],
                'socket_positions': {k: list(v) for k, v in weapon['socket_positions'].items()},
                'bone_heads': {k: list(v) for k, v in weapon['bone_heads'].items()},
                'bones': [bone.name for bone in arm.data.bones], 'bounds': bb,
                'weapon_triangle_count': wt, 'charge_triangle_count': ct, 'triangle_count': wt + ct,
                'bounds_method': 'Evaluated mesh vertices after rigid armature modifier (charge hidden)',
                'modern_shell_ejection': False, 'pump': False, 'animation_only_bones': ['charge_visual'],
                'hammer_fired_deg': HAMMER_FIRED, 'frizzen_open_deg': FRIZZEN_OPEN, 'bore_z_m': BORE_Z,
                'sight_line_z_m': SIGHT_LINE_Z, 'muzzle_x_m': MUZZLE_X, 'uv_texel_density_px_per_m': round(density, 1),
                'source': 'scripts/props/pistol_pack/pistol_model.py (T-248, local)'}
    arm['geometry_contract'] = json.dumps(contract)
    weapon.update(meshes=meshes + charge, weapon_meshes=meshes, charge_meshes=charge, bounds=bb, contract=contract,
                  socket_bones={n: arm.data.bones[n] for n in weapon['socket_positions']})
    return weapon


# ------------------------------------------------------------------------------------------------ ammunition

def create_ammo(collection, materials, atlas_size):
    """SM_Ammo_PistolCartridge (lying, pivot bottom centre) and SM_Ammo_BallPouch (open pouch, lead balls):
    one small atlas for both. Returns {name: (mesh object, [collision objects])}."""
    b = K.Builder(collection, 'Ammo', materials)
    # cartridge lying on its side, lowest point on z = 0
    cartridge(b, 'cartridge', (0, 0, CARTRIDGE_R * 1.05), 'Cartridge')
    # pouch: soft bag lofted upward, rolled rim, drawstring, balls in the opening
    # loft along Z (sections perpendicular to +Z): bag, rolled rim, inner wall down to the floor the balls sit on
    bag = [((0, 0, .003), .030, .024, 0), ((0, 0, .012), .042, .034, 0), ((0, 0, .030), .046, .037, 0), ((0, 0, .048), .040, .032, 0),
           ((0, 0, .060), .030, .024, 0), ((0, 0, .068), .028, .022, 0), ((0, 0, .074), .031, .025, 0), ((0, 0, .0735), .026, .020, 0),
           ((0, 0, .066), .025, .019, 0), ((0, 0, .058), .024, .018, 0)]
    b.loft('Pouch', bag, 'leather', 'pouch', seg=28, expo=2.0, frame=((0, 1, 0), (-1, 0, 0)))
    ring = []
    for j in range(29):
        a = j * K.TAU / 28
        ring.append((.030 * math.cos(a) * 1.0, .024 * math.sin(a), .0665))
    b.tube('Drawstring', ring, .0022, 'cord', 'pouch', seg=8, caps=False)
    b.tube('DrawstringEnd', [(.029, -.010, .066), (.040, -.024, .052), (.044, -.034, .036)], [.0020, .0018, .0016], 'cord', 'pouch', seg=8)
    b.ellipsoid('DrawstringKnot', (.044, -.034, .033), (.004, .004, .004), 'cord', 'pouch', seg=10, ring=6)
    for i, (x, y, z) in enumerate(((.000, .000, .066), (.0135, .005, .064), (-.012, .007, .0635), (.004, -.012, .0635), (-.009, -.009, .062),
                                   (.012, -.008, .0625))):
        b.ellipsoid('Ball%d' % i, (x, y, z), (.0072, .0072, .0072), 'lead', 'pouch', seg=14, ring=8)
    K.pack_islands(b.meshes, b.islands, atlas_size)
    grouped = {}
    for ob in b.meshes:
        grouped.setdefault(b.bindings[ob.name], []).append(ob)
    out = {}
    for bone, name in (('cartridge', 'SM_Ammo_PistolCartridge'), ('pouch', 'SM_Ammo_BallPouch')):
        ob = K.combine(grouped[bone], collection, name)
        bb = K.bounds([ob], evaluated=False)
        # pivot: bottom centre
        shift = Vector(((bb['min'][0] + bb['max'][0]) * .5, (bb['min'][1] + bb['max'][1]) * .5, bb['min'][2]))
        for v in ob.data.vertices:
            v.co -= shift
        bb = K.bounds([ob], evaluated=False)
        lo, hi = Vector(bb['min']), Vector(bb['max'])
        c = (lo + hi) * .5
        s = (hi - lo)
        ucx = bpy.data.meshes.new('UCX_' + name + '_00')
        verts = [(c.x + sx * s.x * .5, c.y + sy * s.y * .5, c.z + sz * s.z * .5) for sx, sy, sz in
                 [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (1, -1, -1), (1, -1, 1), (1, 1, 1), (1, 1, -1)]]
        ucx.from_pydata(verts, [], [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (3, 7, 6, 2), (1, 2, 6, 5), (0, 4, 7, 3)])
        ucx.update()
        col = bpy.data.objects.new('UCX_' + name + '_00', ucx)
        collection.objects.link(col)
        col.display_type = 'WIRE'
        out[name] = (ob, [col], bb)
    return out
