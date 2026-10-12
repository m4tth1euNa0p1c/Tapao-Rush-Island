"""Models of the capture kit (T-337): portable prisoner cage (body, hinged door, folded crate), steel handcuffs (closed on
the captive's wrists, open in the player's hand) and a tiling chain link.

Style: the ground cages of the French fort (maquette fort-francais, factory kit fort_prison.build_cage_ground, rebuilt
in SM_Fort_Cage_Ground): squared timber frame on skids, round wrought iron bars (2.5 to 2.8 cm) about 12 cm apart, flat
iron straps across the bars, a barred door with strap hinges and a padlock. Late 18th - 19th century: hand forged iron,
pegged teak / coconut palm timber, no machined parts.
Metres, Z up, front (door) +X; every pivot is documented in the manifest (build.py).
"""
from __future__ import annotations

import math

from mathutils import Matrix, Vector

import kit as K

WOOD, IRON, STEEL = 'M_Capture_Wood', 'M_Capture_Iron', 'M_Capture_Steel'

# ------------------------------------------------------------------------------------------------- the cage
CAGE = {
    'depth': 1.20,          # X (front door face at +X)
    'width': 1.60,          # Y
    'height': 1.90,         # Z
    'post': 0.10,           # corner posts and frame beams (squared timber)
    'skid_h': 0.07,         # sled runners under the floor
    'floor_top': 0.20,      # walking surface
    'top_beam': 0.10,       # top frame beams (z height - 0.10 .. height)
    'bar_r': 0.0125,        # round wrought iron bars
    'bar_pitch': 0.12,
    'door_w': 0.72,         # clear opening (Y)
    'door_top': 1.70,       # clear opening from floor_top to door_top
    'door_post': 0.08,      # door jambs (Y) x post depth (X)
    'hinge_x': 0.615,       # hinge axis, just outside the front frame (the door closes over the jambs)
}


def _bars_between(mb, a, b, z0, z1, pitch, r, skip=None):
    """Vertical bars from a to b (2D, exclusive ends) between z0 and z1; skip = (t0, t1) along the run: no bar there"""
    a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
    run = (b - a).length
    n = max(1, int(round(run / pitch)))
    pts = []
    for i in range(1, n):
        t = run * i / n
        if skip and skip[0] - 0.02 <= t <= skip[1] + 0.02:
            continue
        p = a + (b - a) * (t / run)
        mb.cylinder((p.x, p.y, z0), (p.x, p.y, z1), r, IRON, segs=8, caps=False)
        pts.append(p)
    return pts


def _strap(mb, a, b, z, height=0.045, thick=0.012, rivets=()):
    """Flat iron strap from a to b (2D) at height z, rivet heads at the given points"""
    a, b = Vector((a[0], a[1], z)), Vector((b[0], b[1], z))
    d = b - a
    rot = Matrix.Rotation(math.atan2(d.y, d.x), 3, 'Z')
    mb.box((a + b) / 2, (d.length, thick, height), IRON, rot=rot, chamfer=0.002)
    n = Vector((-d.y, d.x, 0)).normalized()
    for p in rivets:
        for s in (-1, 1):
            c = Vector((p.x, p.y, z)) + n * s * (thick / 2 + 0.002)
            mb.cylinder(c, c + n * s * 0.004, 0.006, IRON, segs=6)


def build_cage(coll, mats):
    """SM_Capture_Cage (without its door): returns (object, ucx objects, sockets dict, info)"""
    C = CAGE
    hx, hy, H, p = C['depth'] / 2, C['width'] / 2, C['height'], C['post']
    mb = K.MeshBuilder('SM_Capture_Cage', seed=337)
    sk = C['skid_h']
    # skids (sled runners) along Y, ends cut at 45 deg (two boxes)
    for x in (-hx + 0.16, hx - 0.16):
        mb.box((x, 0, sk / 2), (0.09, C['width'] + 0.06, sk), WOOD, chamfer=0.008, grain=1)
        for s in (-1, 1):
            mb.box((x, s * (hy + 0.05), sk * 0.65), (0.088, 0.07, sk * 0.7), WOOD, chamfer=0.01, grain=1,
                   rot=Matrix.Rotation(math.radians(-35 * s), 3, 'X'))
    zf0, zf1 = sk, C['floor_top'] - 0.03          # bottom frame beams
    zc = (zf0 + zf1) / 2
    for x in (-hx + p / 2, hx - p / 2):
        mb.box((x, 0, zc), (p, C['width'], zf1 - zf0), WOOD, chamfer=0.006, grain=1)
    for y in (-hy + p / 2, hy - p / 2):
        mb.box((0, y, zc), (C['depth'] - 2 * p, p, zf1 - zf0), WOOD, chamfer=0.006, grain=0)
    for x in (-0.22, 0.22):                       # joists under the planks
        mb.box((x, 0, zc), (0.08, C['width'] - 2 * p, zf1 - zf0), WOOD, chamfer=0.004, grain=1)
    # floor planks along X, small gaps
    n_planks = 10
    pw = (C['width'] - 0.01) / n_planks
    for i in range(n_planks):
        y = -hy + 0.005 + pw * (i + 0.5)
        mb.box((0, y, C['floor_top'] - 0.015), (C['depth'] - 0.012, pw - 0.008, 0.03), WOOD, chamfer=0.004, grain=0)
    # corner posts
    for x in (-hx + p / 2, hx - p / 2):
        for y in (-hy + p / 2, hy - p / 2):
            mb.box((x, y, (zf0 + H) / 2), (p, p, H - zf0), WOOD, chamfer=0.008, grain=2)
    # top frame
    zt0 = H - C['top_beam']
    ztc = (zt0 + H) / 2
    for x in (-hx + p / 2, hx - p / 2):
        mb.box((x, 0, ztc), (p, C['width'], C['top_beam']), WOOD, chamfer=0.006, grain=1)
    for y in (-hy + p / 2, hy - p / 2):
        mb.box((0, y, ztc), (C['depth'] - 2 * p, p, C['top_beam']), WOOD, chamfer=0.006, grain=0)
    for y in (-0.27, 0.27):                       # roof cross beams
        mb.box((0, y, H - 0.04), (C['depth'] - 2 * p, 0.08, 0.08), WOOD, chamfer=0.005, grain=0)
    # door jambs and lintel on the front
    dp = C['door_post']
    dw = C['door_w']
    jy = dw / 2 + dp / 2
    for s in (-1, 1):
        mb.box((hx - p / 2, s * jy, (C['floor_top'] + zt0) / 2), (p, dp, zt0 - C['floor_top']), WOOD, chamfer=0.006, grain=2)
    lin_z0 = C['door_top']
    mb.box((hx - p / 2, 0, (lin_z0 + zt0) / 2), (p, dw, zt0 - lin_z0), WOOD, chamfer=0.005, grain=1)
    # bars: back, sides, front sides of the door, roof
    zb0, zb1 = C['floor_top'] - 0.01, zt0 + 0.005
    bx = hx - p / 2
    by = hy - p / 2
    r = C['bar_r']
    back = _bars_between(mb, (-bx, -by), (-bx, by), zb0, zb1, C['bar_pitch'], r)
    left = _bars_between(mb, (-bx, by), (bx, by), zb0, zb1, C['bar_pitch'], r)
    right = _bars_between(mb, (-bx, -by), (bx, -by), zb0, zb1, C['bar_pitch'], r)
    front_l = _bars_between(mb, (bx, jy + dp / 2), (bx, by), zb0, zb1, C['bar_pitch'] * 0.9, r)
    front_r = _bars_between(mb, (bx, -by), (bx, -jy - dp / 2), zb0, zb1, C['bar_pitch'] * 0.9, r)
    # roof bars along X between the long top beams
    n_roof = 12
    for i in range(1, n_roof):
        y = -by + 2 * by * i / n_roof
        if abs(abs(y) - 0.27) < 0.05:
            continue
        mb.cylinder((-bx, y, H - 0.05), (bx, y, H - 0.05), r * 0.9, IRON, segs=8, caps=False)
    # flat straps across the bars (two rows), riveted at every bar
    for z in (0.78, 1.32):
        _strap(mb, (-bx, -by + p / 2), (-bx, by - p / 2), z, rivets=back)
        _strap(mb, (-bx + p / 2, by), (bx - p / 2, by), z, rivets=left)
        _strap(mb, (-bx + p / 2, -by), (bx - p / 2, -by), z, rivets=right)
        _strap(mb, (bx, jy + dp / 2), (bx, by - p / 2), z, rivets=front_l)
        _strap(mb, (bx, -by + p / 2), (bx, -jy - dp / 2), z, rivets=front_r)
    # iron corner brackets (L plates) on the top and bottom frame corners
    for x in (-hx, hx):
        for y in (-hy, hy):
            for z in (zc, ztc):
                sx, sy = (1 if x < 0 else -1), (1 if y < 0 else -1)
                mb.box((x - 0.003 * sx, y + sy * 0.09, z), (0.006, 0.16, 0.07), IRON, chamfer=0.002)
                mb.box((x + sx * 0.09, y - 0.003 * sy, z), (0.16, 0.006, 0.07), IRON, chamfer=0.002)
    # lifting eyes on the top long beams (for a sling / the boat)
    for x in (-hx + p / 2, hx - p / 2):
        for y in (-0.55, 0.55):
            mb.torus((x - (0.06 if x < 0 else -0.06), y, H - 0.05), (0, 1, 0), 0.032, 0.007, IRON, segs=14, ring_segs=6)
    # hinge pintles on the left jamb (Blender -Y side), staple of the hasp on the right jamb
    hinge_y = -dw / 2
    hx_axis = C['hinge_x']
    for zh in (0.45, 1.45):
        z = C['floor_top'] + zh - 0.2
        mb.box((hx - 0.004, hinge_y - dp / 2, z), (0.008, dp, 0.09), IRON, chamfer=0.002)
        mb.cylinder((hx_axis, hinge_y, z - 0.035), (hx_axis, hinge_y, z + 0.035), 0.009, IRON, segs=8)
        mb.box(((hx + hx_axis) / 2, hinge_y - 0.012, z), (hx_axis - hx + 0.012, 0.024, 0.05), IRON, chamfer=0.002)
    lock_z = C['floor_top'] + 0.85
    mb.box((hx - 0.004, jy, lock_z), (0.008, dp * 0.9, 0.10), IRON, chamfer=0.002)
    mb.torus((hx + 0.012, dw / 2 + 0.012, lock_z), (0, 0, 1), 0.016, 0.004, IRON, segs=12, ring_segs=6, arc=(-90, 90))
    ob = mb.build(coll, mats)
    ucx = []
    nm = 'UCX_SM_Capture_Cage'
    boxes = [((0, 0, C['floor_top'] / 2), (C['depth'], C['width'], C['floor_top'])),            # floor
             ((-hx + p / 2, 0, (C['floor_top'] + H) / 2), (p, C['width'], H - C['floor_top'])),   # back
             ((0, hy - p / 2, (C['floor_top'] + H) / 2), (C['depth'] - 2 * p, p, H - C['floor_top'])),
             ((0, -hy + p / 2, (C['floor_top'] + H) / 2), (C['depth'] - 2 * p, p, H - C['floor_top'])),
             ((0, 0, H - C['top_beam'] / 2), (C['depth'] - 2 * p, C['width'] - 2 * p, C['top_beam'])),  # roof
             ((hx - p / 2, (dw / 2 + hy) / 2, (C['floor_top'] + H) / 2), (p, hy - dw / 2, H - C['floor_top'])),
             ((hx - p / 2, -(dw / 2 + hy) / 2, (C['floor_top'] + H) / 2), (p, hy - dw / 2, H - C['floor_top'])),
             ((hx - p / 2, 0, (C['door_top'] + H) / 2), (p, dw, H - C['door_top']))]                 # lintel
    for i, (c, s) in enumerate(boxes):
        ucx.append(K.ucx_box(f'{nm}_{i:02d}', c, s, coll))
    for u in ucx:
        u.parent = ob
    hinge = Vector((hx_axis, hinge_y, C['floor_top']))
    sockets = {
        'prisoner': ((0.0, 0.0, C['floor_top']), (0, 0, 0)),
        'door_hinge': (tuple(hinge), (0, 0, 0)),
        'door_handle': ((hx_axis + 0.035, hinge_y + dw + 0.016 - 0.08, C['floor_top'] + 0.736), (0, 0, 0)),
        'lock': ((hx + 0.03, dw / 2 + 0.012, lock_z), (0, 0, 0)),
        'entry': ((hx + 0.45, 0.0, 0.0), (0, 0, math.pi)),
        'captor': ((hx + 0.95, 0.15, 0.0), (0, 0, math.pi)),
    }
    info = {'clear_inside_m': [round(C['depth'] - 2 * p, 3), round(C['width'] - 2 * p, 3), round(zt0 - C['floor_top'], 3)],
            'door_opening_m': [dw, round(C['door_top'] - C['floor_top'], 3)], 'floor_top_m': C['floor_top'],
            'bar_pitch_m': C['bar_pitch'], 'bar_diameter_m': 2 * r}
    return ob, ucx, sockets, info


def build_door(coll, mats):
    """SM_Capture_Cage_Door: pivot on the hinge axis at the bottom of the door; the door runs along +Y (Blender) from the
    pivot, in the plane x = 0 of the pivot (in front of the jambs when closed)"""
    C = CAGE
    w = C['door_w'] + 0.02
    h = C['door_top'] - C['floor_top'] - 0.025
    x0 = 0.0
    mb = K.MeshBuilder('SM_Capture_Cage_Door', seed=338)
    y0, y1 = 0.012, w - 0.004
    z0, z1 = 0.012, h
    fb = 0.045                                   # flat bar of the frame
    t = 0.012
    mb.box((x0, (y0 + y1) / 2, z0 + fb / 2), (t, y1 - y0, fb), IRON, chamfer=0.002)
    mb.box((x0, (y0 + y1) / 2, z1 - fb / 2), (t, y1 - y0, fb), IRON, chamfer=0.002)
    mb.box((x0, y0 + fb / 2, (z0 + z1) / 2), (t, fb, z1 - z0), IRON, chamfer=0.002)
    mb.box((x0, y1 - fb / 2, (z0 + z1) / 2), (t, fb, z1 - z0), IRON, chamfer=0.002)
    for zz in (0.50, 0.98):
        mb.box((x0, (y0 + y1) / 2, zz), (t, y1 - y0, fb * 0.9), IRON, chamfer=0.002)
    # diagonal brace (hinge bottom -> latch top)
    a, b = Vector((x0, y0 + fb, z0 + fb)), Vector((x0, y1 - fb, 0.98 - fb / 2))
    d = b - a
    rot = Matrix.Rotation(math.atan2(d.z, d.y), 3, 'X')
    mb.box((a + b) / 2, (t * 0.9, d.length, fb * 0.75), IRON, rot=rot, chamfer=0.002)
    n = 6
    for i in range(1, n):
        y = y0 + (y1 - y0) * i / n
        mb.cylinder((x0 + 0.004, y, z0 + fb), (x0 + 0.004, y, z1 - fb), 0.011, IRON, segs=8, caps=False)
    # strap hinges: knuckle on the axis, strap along the door
    for zh in (0.45, 1.45):
        z = zh - 0.2
        mb.cylinder((0, 0, z + 0.036), (0, 0, z + 0.1), 0.017, IRON, segs=10)
        mb.cylinder((0, 0, z - 0.1), (0, 0, z - 0.036), 0.017, IRON, segs=10)
        mb.box((x0 + 0.004, 0.17, z + 0.068), (0.014, 0.32, 0.05), IRON, chamfer=0.003)
        mb.box((x0 + 0.004, 0.17, z - 0.068), (0.014, 0.32, 0.05), IRON, chamfer=0.003)
    # pull ring and hasp on the free edge, padlock hanging from the hasp
    ring_c = Vector((x0 + 0.035, y1 - 0.08, 0.80))
    mb.box((x0 + 0.009, y1 - 0.08, 0.80), (0.006, 0.05, 0.05), IRON, chamfer=0.002)
    mb.torus(ring_c + Vector((0, 0, -0.03)), (0, 1, 0), 0.034, 0.006, IRON, segs=16, ring_segs=6)
    hasp_z = 0.85
    mb.box((x0 + 0.012, y1 + 0.005, hasp_z), (0.008, 0.07, 0.03), IRON, chamfer=0.002)
    lock_c = Vector((x0 + 0.045, y1 + 0.03, hasp_z - 0.075))
    mb.box(lock_c, (0.03, 0.06, 0.07), IRON, chamfer=0.006)
    mb.torus(lock_c + Vector((0, 0, 0.042)), (1, 0, 0), 0.02, 0.005, IRON, segs=12, ring_segs=6, arc=(0, 180))
    mb.cylinder(lock_c + Vector((0.015, 0.0, -0.01)), lock_c + Vector((0.019, 0.0, -0.01)), 0.006, IRON, segs=8)
    ob = mb.build(coll, mats)
    ucx = [K.ucx_box('UCX_SM_Capture_Cage_Door_00', (x0, (y0 + y1) / 2, (z0 + z1) / 2), (0.04, y1 - y0, z1 - z0), coll)]
    for u in ucx:
        u.parent = ob
    sockets = {'handle': (tuple(ring_c + Vector((0.0, 0.0, -0.064))), (0, 0, 0)),
               'padlock': (tuple(lock_c), (0, 0, 0))}
    info = {'size_m': [0.06, round(w, 3), round(h, 3)], 'open_axis': 'Z of the pivot',
            'open_blender_deg': -100.0, 'open_unreal_yaw_deg': 100.0}
    return ob, ucx, sockets, info


def build_folded(coll, mats):
    """SM_Capture_Cage_Folded: the cage folded flat into a strapped bundle carried by its top handle at the side
    (the bean's mitten is 33 cm above the ground: 26 cm of crate under the grip). Pivot at the bottom centre; long axis
    +X (the bean's forward when carried), thickness along Y."""
    L, T, Hh = 0.46, 0.15, 0.205
    mb = K.MeshBuilder('SM_Capture_Cage_Folded', seed=339)
    # outer panels (timber frames with bar stubs), three folded panels between them
    for y, th in ((-T / 2 + 0.018, 0.036), (T / 2 - 0.018, 0.036)):
        for z in (0.02, Hh - 0.02):
            mb.box((0, y, z), (L, th, 0.04), WOOD, chamfer=0.004, grain=0)
        for x in (-L / 2 + 0.02, L / 2 - 0.02):
            mb.box((x, y, Hh / 2), (0.04, th, Hh - 0.08), WOOD, chamfer=0.004, grain=2)
        n = 6
        for i in range(1, n):
            x = -L / 2 + L * i / n
            mb.cylinder((x, y, 0.04), (x, y, Hh - 0.04), 0.008, IRON, segs=6, caps=False)
    for k, y in enumerate((-0.026, 0.0, 0.026)):
        inset = 0.012 * (k % 2)
        mb.box((0, y, 0.03 + inset), (L - 0.03, 0.022, 0.035), WOOD, chamfer=0.003, grain=0)
        mb.box((0, y, Hh - 0.03 - inset), (L - 0.03, 0.022, 0.035), WOOD, chamfer=0.003, grain=0)
        mb.box((-L / 2 + 0.03, y, Hh / 2), (0.03, 0.022, Hh - 0.1), WOOD, chamfer=0.003, grain=2)
        mb.box((L / 2 - 0.03, y, Hh / 2), (0.03, 0.022, Hh - 0.1), WOOD, chamfer=0.003, grain=2)
        for i in range(1, 5):
            x = -L / 2 + L * i / 5
            mb.cylinder((x, y, 0.06), (x, y, Hh - 0.06), 0.006, IRON, segs=6, caps=False)
    # two iron straps around the bundle (around X), hinge knuckles on one long edge
    for x in (-0.14, 0.14):
        pts = [(x, -T / 2 - 0.004, 0.004), (x, T / 2 + 0.004, 0.004), (x, T / 2 + 0.004, Hh - 0.004), (x, -T / 2 - 0.004, Hh - 0.004)]
        prof = [(-0.003, -0.016), (0.003, -0.016), (0.003, 0.016), (-0.003, 0.016)]
        mb.tube(pts, 0.0, IRON, closed=True, profile=[(z, y) for y, z in prof])
    for x in (-0.18, 0.0, 0.18):
        mb.cylinder((x - 0.04, -T / 2 - 0.006, Hh - 0.01), (x + 0.04, -T / 2 - 0.006, Hh - 0.01), 0.008, IRON, segs=8)
    # iron corner caps
    for x in (-L / 2, L / 2):
        for z in (0.0, Hh):
            sz = 1 if z == 0 else -1
            sx = 1 if x < 0 else -1
            mb.box((x - sx * 0.002, 0, z + sz * 0.025), (0.008, T + 0.006, 0.05), IRON, chamfer=0.002)
            mb.box((x + sx * 0.025, 0, z - sz * 0.002), (0.05, T + 0.006, 0.008), IRON, chamfer=0.002)
    # carrying handle: iron bail with a turned wooden grip
    gz = Hh + 0.045
    bail = [(-0.075, 0, Hh - 0.005), (-0.075, 0, gz - 0.012), (-0.06, 0, gz), (0.06, 0, gz), (0.075, 0, gz - 0.012),
            (0.075, 0, Hh - 0.005)]
    mb.tube(bail, 0.006, IRON, segs=8)
    for x in (-0.075, 0.075):
        mb.box((x, 0, Hh + 0.004), (0.03, 0.04, 0.008), IRON, chamfer=0.002)
    mb.cylinder((-0.052, 0, gz), (0.052, 0, gz), 0.016, WOOD, segs=12, tile=0.3)
    ob = mb.build(coll, mats)
    ucx = [K.ucx_box('UCX_SM_Capture_Cage_Folded_00', (0, 0, Hh / 2), (L, T, Hh), coll)]
    for u in ucx:
        u.parent = ob
    sockets = {'grip': ((0.0, 0.0, gz), (0, 0, 0)),
               'hand_l': ((0.0, T / 2 + 0.01, Hh * 0.55), (0, 0, 0)),
               'hand_r': ((0.0, -T / 2 - 0.01, Hh * 0.55), (0, 0, 0))}
    info = {'size_m': [L, T, round(gz + 0.016, 3)], 'grip_height_m': round(gz, 3)}
    return ob, ucx, sockets, info


# --------------------------------------------------------------------------------------------- handcuffs, chain
CUFF = {
    'inner_r': 0.058,      # around the top of the bean's mitten, just under the sleeve cuff (radius 0.072)
    'band_w': 0.022,       # along the forearm
    'band_t': 0.008,       # radial thickness
    'along': 0.031,        # bracelet centre from the hand bone head, along the bone (m)
}
LINK = {'inner_l': 0.032, 'inner_w': 0.016, 'wire_r': 0.005}


def link_points(center, axis, side, inner_l, inner_w, wire_r, n=16):
    """Oval chain link (stadium centreline) centred at center, long axis `axis`, lying in the plane of axis and side"""
    a = Vector(axis).normalized()
    s = Vector(side).normalized()
    half_straight = (inner_l - inner_w) / 2
    rr = inner_w / 2 + wire_r
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x = math.cos(t) * rr
        y = math.sin(t) * rr
        x += half_straight if x > 0 else -half_straight
        pts.append(Vector(center) + a * x + s * y)
    return pts


def chain_link_model(mb, center, axis, side, scale=1.0, segs=6):
    L = LINK
    pts = link_points(center, axis, side, L['inner_l'] * scale, L['inner_w'] * scale, L['wire_r'] * scale, n=18)
    mb.tube(pts, L['wire_r'] * scale, STEEL, segs=segs, closed=True)


def build_chain_link(coll, mats):
    mb = K.MeshBuilder('SM_Chain_Link', seed=340)
    chain_link_model(mb, (0, 0, 0), (1, 0, 0), (0, 1, 0), segs=8)
    ob = mb.build(coll, mats)
    L = LINK
    info = {'pitch_m': L['inner_l'], 'length_m': round(L['inner_l'] + 4 * L['wire_r'], 4),
            'width_m': round(L['inner_w'] + 4 * L['wire_r'], 4), 'wire_diameter_m': 2 * L['wire_r'],
            'tiling': 'link n centred at n * pitch along +X, rolled 90 deg about X from link n-1'}
    return ob, info


def _bracelet(mb, frame, open_deg=0.0, eye_deg=-35.0):
    """One bracelet in frame (4x4: origin = centre, Y = ring axis = along the forearm, X = toward the other cuff, Z up):
    hinged band in two halves (hinge knuckle below at -90 deg, lock case on top at +90 deg), the inner half fixed with a
    swivel eye at eye_deg (in the XZ plane, from +X toward +Z), the outer half the jaw, swung open by open_deg about the
    hinge. Returns the eye centre."""
    C = CUFF
    ri, w, t = C['inner_r'], C['band_w'], C['band_t']
    rm = ri + t / 2
    prof = [(-t / 2, -w / 2), (t / 2, -w / 2), (t / 2, w / 2), (-t / 2, w / 2)]

    def arc(a0, a1, M, n=12):
        return [M @ Vector((math.cos(math.radians(a0 + (a1 - a0) * i / n)) * rm, 0,
                            math.sin(math.radians(a0 + (a1 - a0) * i / n)) * rm)) for i in range(n + 1)]

    F = frame
    up = F.to_3x3() @ Vector((0, 1, 0))
    mb.tube(arc(-84, 84, F), 0.0, STEEL, profile=prof, up=up)                      # fixed inner half
    hinge = Vector((0, 0, -rm))
    J = F @ Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(-open_deg), 4, 'Y') @ Matrix.Translation(-hinge)
    mb.tube(arc(96, 264, J), 0.0, STEEL, profile=prof, up=up)                      # jaw (outer half)
    R3 = F.to_3x3()
    mb.cylinder(F @ Vector((0, -w / 2 - 0.002, -rm)), F @ Vector((0, w / 2 + 0.002, -rm)), t * 0.95, STEEL, segs=8)
    # lock case on top (the period's box lock with its keyhole), riveted to the fixed half
    lb = F @ Vector((0, 0, rm + 0.011))
    mb.box(lb, (0.034, w + 0.006, 0.02), STEEL, rot=R3, chamfer=0.004)
    mb.cylinder(lb + R3 @ Vector((0.004, 0, 0.0095)), lb + R3 @ Vector((0.004, 0, 0.0115)), 0.004, STEEL, segs=6)
    a = math.radians(eye_deg)
    eye = F @ Vector((math.cos(a) * (rm + 0.013), 0, math.sin(a) * (rm + 0.013)))
    mb.torus(eye, R3 @ Vector((0, 1, 0)), 0.0085, 0.003, STEEL, segs=12, ring_segs=6)
    return eye


def _chain_between(mb, a, b, sag=0.02, scale=0.85):
    """Links from point a to point b hanging a little (catenary-like parabola)"""
    pitch = LINK['inner_l'] * scale
    d = b - a
    n = max(2, int(round(d.length / pitch)))
    pts = []
    for i in range(n + 1):
        t = i / n
        p = a + d * t + Vector((0, 0, -sag * 4 * t * (1 - t)))
        pts.append(p)
    side_up = Vector((0, 0, 1))
    for i in range(n):
        c = (pts[i] + pts[i + 1]) / 2
        axis = pts[i + 1] - pts[i]
        side = side_up.cross(axis) if i % 2 == 0 else side_up - axis.normalized() * side_up.dot(axis.normalized())
        if side.length < 1e-6:
            side = Vector((1, 0, 0))
        chain_link_model(mb, c, axis, side, scale=scale)
    return pts


def build_handcuffs(coll, mats, frame_r, frame_l, name='SM_Steel_Handcuffs', sag=0.012):
    """Two closed bracelets at the frames (4x4 in the armature space of the captive's reference pose: translation = bracelet
    centre, Y = ring axis along the forearm) and the short chain between them (eye - link - ring - link - eye). The object
    is expressed in its own frame: origin halfway between the bracelet centres, +X from the right cuff to the left one, +Z
    up as far as possible. Returns (object, sockets, info, M) with M = the object's frame in armature space."""
    cr, cl = frame_r.translation.copy(), frame_l.translation.copy()
    mid = (cr + cl) / 2
    R = K.frame_from(cl - cr, Vector((0, 0, 1)))
    M = R.to_4x4()
    M.translation = mid
    Minv = M.inverted()
    mb = K.MeshBuilder(name, seed=341)
    eyes = []
    for F, toward in ((frame_r, cl), (frame_l, cr)):
        y = (F.to_3x3() @ Vector((0, 1, 0))).normalized()
        to = toward - F.translation
        x = (to - y * to.dot(y)).normalized()
        z = x.cross(y)
        if z.dot(Vector((0, 0, 1))) < 0:          # keep the lock case up: flip the ring axis, not the side
            y = -y
            z = x.cross(y)
        B = Matrix((x, y, z)).transposed().to_4x4()
        B.translation = F.translation
        eyes.append(_bracelet(mb, Minv @ B))
    a, b = eyes
    ring_c = (a + b) / 2 + Vector((0, 0, -0.018))
    for e in (a, b):
        _chain_between(mb, e, ring_c, sag=sag * 0.3, scale=0.62)
    mb.torus(ring_c, (1, 0, 0), 0.013, 0.004, STEEL, segs=14, ring_segs=6)
    ob = mb.build(coll, mats)
    sockets = {'cuff_r': (tuple(Minv @ cr), (0, 0, 0)), 'cuff_l': (tuple(Minv @ cl), (0, 0, 0)),
               'chain_anchor': (tuple(ring_c + Vector((0, 0, -0.013))), (0, 0, 0))}
    info = {'bracelet_inner_radius_m': CUFF['inner_r'], 'bracelet_outer_radius_m': CUFF['inner_r'] + CUFF['band_t'],
            'band_width_m': CUFF['band_w'], 'centre_distance_m': round((cl - cr).length, 4)}
    return ob, sockets, info, M


def build_handcuffs_open(coll, mats):
    """SM_Steel_Handcuffs_Open: the cuffs as the player holds them, jaws open, the two bracelets hanging from the middle
    ring held in the mitten. Pivot = socket grip (the middle ring); +X along the pair, Z up."""
    mb = K.MeshBuilder('SM_Steel_Handcuffs_Open', seed=342)
    top = Vector((0, 0, 0))
    eyes = []
    for sx, tilt in ((-1, 16), (1, -16)):
        c = Vector((sx * 0.075, 0.0, -0.125))
        x = (top - c).normalized()                     # toward the ring (the eye at 0 deg)
        y = Vector((0, 1, 0))
        y = (Matrix.Rotation(math.radians(tilt), 3, x) @ (y - x * y.dot(x))).normalized()
        z = x.cross(y)
        B = Matrix((x, y, z)).transposed().to_4x4()
        B.translation = c
        eyes.append(_bracelet(mb, B, open_deg=60.0, eye_deg=0.0))
    for e in eyes:
        _chain_between(mb, top + (e - top).normalized() * 0.012, e, sag=0.004, scale=0.62)
    mb.torus(top, (1, 0, 0), 0.013, 0.004, STEEL, segs=14, ring_segs=6)
    ob = mb.build(coll, mats)
    sockets = {'grip': ((0.0, 0.0, 0.0), (0, 0, 0)), 'chain_anchor': ((0.0, 0.0, -0.013), (0, 0, 0))}
    info = {'held_by': 'the middle ring (socket grip, pivot) in the right mitten', 'jaw_open_deg': 60.0}
    return ob, sockets, info
