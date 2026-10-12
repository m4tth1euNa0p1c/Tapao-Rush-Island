"""Heavy hand-cranked machine gun "Mle 1918" of the bean (T-272, owner request 2026-10-10): a fictional gun after the
Vickers / Maxim of 1918 with the side crank of a Gatling. Real scale (1.11 m, ~17 kg with water and a full box).

Weapon coordinates (metres): +X to the muzzle, +Z up, -Y the right side (crank side), bore axis on Z = 0, origin on
the bore axis at the middle of the receiver. Every part is rigid on one bone:
  gun_root        receiver (blued steel, riveted side frames, ejection port underneath), rear plate, wooden spade
                  grips and firing lever, crank housing, rear sight (folding leaf, raised), data plate, box bracket,
                  empty belt bag, sling
  jacket          fluted water jacket in browned brass with its bands, filler cap, drain plug, steam fitting and the
                  condensing hose, front sight, carrying handle, wooden fore-grip, gland nut, barrel, conical flash hider
  crank           the side crank: hub, arm, wooden handle (the right hand's grip, socket_grip_r at its rest angle)
  feed_cover      top cover hinged at its front end (opened during the reload)
  cocking_handle  knob of the cocking slide on the left side (pulled back by the left hand: Charge)
  steam_cap       hinged steam valve flap on the jacket top front (pops at SteamVent)
  ammo_box        wooden ammunition box bound with iron, hooked under the left side of the receiver
  tripod          folded tripod legs under the jacket front (optional: HideBoneByName("tripod") in Unreal)
  belt_01..17     the loaded belt (canvas pocket, brass spacer and a cartridge each), from the extraction point inside
                  the receiver (01) back down into the box (16)
  belt_out_01..10 the empty belt after the extraction point, out of the right side into the canvas belt bag
The belt is a chain of bones (not a UV scroll): the clips place every pocket on the belt path each frame (see
animation.py: conveyor of one pitch per shot, four pitches per FireLoop turn, wrapped at the loop end, so the
interpolated frames never slide it backwards).
"""
from __future__ import annotations

import math

import bmesh
import bpy
from mathutils import Matrix, Vector

import kit as K
import mg_materials as MM

V = K.V
TAU = 2 * math.pi

# ------------------------------------------------------------------------------------------- key dimensions
REC_X0, REC_X1 = -0.205, 0.150          # receiver body (rear face of the body, front face)
REC_HW = 0.046                          # half width
REC_Z0, REC_Z1 = -0.064, 0.050          # bottom, top of the body (the covers sit on top)
TOP_Z = 0.066                           # top of the covers
TRUNNION_X0, TRUNNION_X1 = 0.128, 0.176
JACKET_X0, JACKET_X1, JACKET_R = 0.170, 0.700, 0.052
GLAND_X = 0.716
MUZZLE_X = 0.906
CONE_X0 = 0.776
# crank: axle on the right side plate, handle radius, rest angle of the handle (deg in the XZ plane from +X toward
# +Z; the crank turns forward over the top: the angle decreases)
CRANK_C = Vector((-0.095, 0.0, 0.018))
CRANK_R = 0.090
CRANK_REST_DEG = 40.0                   # rest: top front, the right hand rides above the receiver (seen in FP)
CRANK_ARM_Y = (-0.074, -0.082)          # plane of the crank arm (y range)
CRANK_HANDLE_Y = (-0.086, -0.158)       # wooden handle along -Y
# feed cover: hinge pin (transverse) at its front
COVER_X0, COVER_X1 = -0.064, 0.130
COVER_HINGE = Vector((0.128, 0.0, 0.058))
# cocking handle (left side, top rear): knob centre at rest, travel back along -X
COCK_KNOB = Vector((-0.100, REC_HW + 0.026, 0.030))
COCK_TRAVEL = 0.066
# feed: slots in both side plates (x range, z range); belt centre line x; extraction point inside
FEED_X = (0.018, 0.114)
FEED_Z = (0.020, 0.046)
BELT_X = 0.050                          # centre of the canvas strip along X
CART_SHIFT = 0.011                      # cartridge offset along X in a pocket (case body centred on the strip)
PITCH = 0.0175                          # belt pitch (one pocket)
# ammunition box (hooked under the left half of the receiver)
BOX_MIN = Vector((-0.078, 0.012, -0.206))
BOX_MAX = Vector((0.124, 0.106, -0.076))
BOX_SLOT = (0.016, 0.114, 0.062, 0.082)  # x0, x1, y0, y1 of the belt slot in the lid
# ejection port (bottom of the receiver): centre and the exit direction of the spent cases
EJECT = Vector((0.098, -0.010, REC_Z0 - 0.002))
EJECT_DIR = Vector((0.10, -0.20, -0.97)).normalized()
# flat canvas pouch for the empty belt under the right exit (clear of the crank hand: its outer face at y > -0.068)
BAG_Y, BAG_HALF_T = -0.058, 0.0095
BAG_TOP, BAG_BOTTOM = -0.004, -0.122
# fore-grip (left hand) and steam valve
FOREGRIP_X = 0.292
FOREGRIP_DIR = Vector((0.06, 0.70, -0.71)).normalized()     # canted 45 deg to the left: the FP left hand shows
FOREGRIP = Vector((FOREGRIP_X, 0.0, 0.0)) + FOREGRIP_DIR * 0.112
CARRY_GRIP = Vector((0.260, 0.0, 0.0805))      # the third person left hand (carrying handle on the jacket)
STEAM = Vector((0.640, 0.0, 0.060))
STEAM_HINGE = Vector((0.6215, 0.0, 0.0605))
# cartridge (7.7 mm rimmed, length 0.080; case 0.056): profile along X from the case head
CART_HEAD = -0.034
CASE_PROFILE = [(-0.0340, 0.0000), (-0.0340, 0.0060), (-0.0337, 0.0068), (-0.0322, 0.0068), (-0.0318, 0.0059),
                (-0.0300, 0.0058), (0.0080, 0.0055), (0.0125, 0.0045), (0.0150, 0.0043), (0.0220, 0.0042)]
BULLET_PROFILE = [(0.0205, 0.0040), (0.0300, 0.0040), (0.0370, 0.0034), (0.0420, 0.0021), (0.0458, 0.0006), (0.0462, 0.0)]


def crank_handle_point(angle_deg, y=None):
    """Centre of the crank handle (weapon space) for a handle angle (deg)."""
    a = math.radians(angle_deg)
    yy = 0.5 * (CRANK_HANDLE_Y[0] + CRANK_HANDLE_Y[1]) if y is None else y
    return Vector((CRANK_C.x + CRANK_R * math.cos(a), yy, CRANK_C.z + CRANK_R * math.sin(a)))


GRIP_R = crank_handle_point(CRANK_REST_DEG)
GRIP_L = FOREGRIP.copy()


# ------------------------------------------------------------------------------------------------ helpers

def B():
    return K.bean()


def use(ob, name):
    ob.data.materials.clear()
    ob.data.materials.append(bpy.data.materials[name])
    return ob


def revolve(name, profile, mat, coll, axis='X', center=(0.0, 0.0, 0.0), segs=24, radial=None, smooth=True, cap=True):
    """Surface of revolution: profile = [(t, r)] along the axis (r = 0: a pole); radial(theta, t) -> radius factor
    (flutes, knurls). axis 'X', 'Y' (t along -Y when the axis is '-Y') or 'Z'."""
    bm = bmesh.new()
    rows = []
    sign = -1.0 if axis.startswith('-') else 1.0
    ax = axis[-1]
    c = Vector(center)

    def point(t, r, th):
        u, v = r * math.cos(th), r * math.sin(th)
        if ax == 'X':
            return c + Vector((sign * t, u, v))
        if ax == 'Y':
            return c + Vector((v, sign * t, u))
        return c + Vector((u, v, sign * t))

    for t, r in profile:
        if r <= 1e-7:
            rows.append(bm.verts.new(point(t, 0.0, 0.0)))
            continue
        ring = []
        for i in range(segs):
            th = TAU * i / segs
            f = radial(th, t) if radial else 1.0
            ring.append(bm.verts.new(point(t, r * f, th)))
        rows.append(ring)
    for a, b in zip(rows, rows[1:]):
        if isinstance(a, list) and isinstance(b, list):
            B().grid_faces(bm, [a, b], True)
        elif isinstance(a, list):
            for i in range(segs):
                bm.faces.new((a[i], a[(i + 1) % segs], b))
        elif isinstance(b, list):
            for i in range(segs):
                bm.faces.new((a, b[(i + 1) % segs], b[i]))
    if cap:
        for row, rev in ((rows[0], True), (rows[-1], False)):
            if isinstance(row, list):
                try:
                    f = bm.faces.new(row[::-1] if rev else row)
                except ValueError:
                    pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B().obj_from_bm(name, bm, mat, coll, smooth=smooth)


def sweep(name, pts, sides, half_w, half_t, mat, coll, segs=12, expo=4.0, caps=True, subsurf=0):
    """Rounded-rectangle section swept along pts; sides[k] = the width direction at point k (unit, normal to the
    path), the thickness direction is tangent x side. half_w / half_t: floats or per-point lists."""
    bm = bmesh.new()
    rows = []
    n = len(pts)
    for k, p in enumerate(pts):
        d = (pts[min(k + 1, n - 1)] - pts[max(k - 1, 0)]).normalized()
        s = Vector(sides[k])
        s = (s - d * s.dot(d)).normalized()
        t = d.cross(s).normalized()
        hw = half_w[k] if isinstance(half_w, (list, tuple)) else half_w
        ht = half_t[k] if isinstance(half_t, (list, tuple)) else half_t
        ring = []
        for i in range(segs):
            a = TAU * i / segs
            ca, sa = math.cos(a), math.sin(a)
            u = hw * math.copysign(abs(ca) ** (2 / expo), ca)
            v = ht * math.copysign(abs(sa) ** (2 / expo), sa)
            ring.append(bm.verts.new(Vector(p) + s * u + t * v))
        rows.append(ring)
    B().grid_faces(bm, rows, True)
    if caps:
        bm.faces.new(rows[0][::-1])
        bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B().obj_from_bm(name, bm, mat, coll, subsurf=subsurf)


def box(name, center, size, mat, coll, bevel=0.002, rot=None):
    ob = B().box(name, center, size, mat, coll, bevel=bevel)
    if rot is not None:
        ob.data.transform(Matrix.Translation(Vector(center)) @ rot.to_4x4() @ Matrix.Translation(-Vector(center)))
    return ob


def rivet(name, center, normal, coll, r=0.0034, mat='MAT-MG-Blued', seg=10):
    """Domed rivet head lying on a surface (centre on the surface, normal outwards)."""
    n = Vector(normal).normalized()
    ob = B().blob(name, (0, 0, 0), (r, r, r * 0.55), mat, coll, seg=seg, ring=5)
    q = Vector((0, 0, 1)).rotation_difference(n)
    ob.data.transform(Matrix.Translation(Vector(center)) @ q.to_matrix().to_4x4())
    return ob


def bolt_head(name, center, normal, coll, r=0.0045, h=0.003, mat='MAT-MG-Blued'):
    """Hexagonal bolt head on a surface."""
    n = Vector(normal).normalized()
    ob = revolve(name, [(0.0, 0.0), (0.0, r), (h * 0.85, r), (h, r * 0.8), (h, 0.0)], mat, coll, axis='Z', segs=6, smooth=False)
    q = Vector((0, 0, 1)).rotation_difference(n)
    ob.data.transform(Matrix.Translation(Vector(center)) @ q.to_matrix().to_4x4())
    return ob


def bool_cut(target, cutter, material):
    """Boolean difference; the new faces take the cutter's material (dark recess). The cutter is hidden and
    returned in res['cutters'] (removed after the join)."""
    mod = target.modifiers.new('Cut-' + cutter.name, 'BOOLEAN')
    mod.operation = 'DIFFERENCE'
    mod.object = cutter
    try:
        mod.solver = 'EXACT'
        mod.material_mode = 'TRANSFER'
    except (AttributeError, TypeError):
        pass
    cutter.data.materials.clear()
    cutter.data.materials.append(bpy.data.materials[material])
    if material not in [m.name for m in target.data.materials if m]:
        target.data.materials.append(bpy.data.materials[material])
    cutter.hide_render = True
    cutter.hide_viewport = True
    cutter.display_type = 'WIRE'
    return cutter


# ------------------------------------------------------------------------------------------- cartridge, belt

def cartridge(coll, prefix, segs=14, fired=False):
    """A 7.7 mm rimmed cartridge along +X centred on its case head at CART_HEAD (case brass, gilding metal bullet,
    copper primer). fired: no bullet, the mouth slightly flared, the primer struck."""
    brass = 'MAT-MG-CaseBrass' if fired else 'MAT-MG-BrassBright'
    parts = [revolve(prefix + '_Case', CASE_PROFILE, brass, coll, axis='X', segs=segs)]
    parts.append(revolve(prefix + '_Primer', [(-0.0346, 0.0), (-0.0346, 0.0022), (-0.0339, 0.0024), (-0.0335, 0.0)],
                         'MAT-MG-Copper', coll, axis='X', segs=10))
    if fired:
        parts.append(revolve(prefix + '_Mouth', [(0.0200, 0.0041), (0.0226, 0.0045), (0.0226, 0.0037), (0.0150, 0.0035), (0.0150, 0.0)],
                             brass, coll, axis='X', segs=segs))
    else:
        parts.append(revolve(prefix + '_Bullet', BULLET_PROFILE, 'MAT-MG-Copper', coll, axis='X', segs=segs))
    return parts


def belt_segment(coll, prefix, loaded=True, tip=False):
    """One pocket of the canvas belt in its own frame: X = cartridge axis, Y = along the belt (direction of feed),
    Z = out of the belt plane. Origin = centre of the pocket on the canvas mid plane; the cartridge middle at
    x = CART_SHIFT from the strip centre (the strip spans BELT_X +- 0.0205 in weapon X)."""
    parts = []
    hw = 0.0205                         # half width of the canvas strip (along X)
    # canvas: a closed section in the (Y, Z) plane swept along X: two layers stitched flat between the pockets,
    # the pocket bulging around the case (empty: flattened)
    bulge = 0.0071 if loaded else 0.0034
    half = PITCH * 0.5 + 0.0007          # overlaps the next pocket a little: no gap on the bends
    prof = []
    for i in range(17):
        y = -half + 2 * half * i / 16
        z = max(0.0011, bulge * math.sqrt(max(0.0, 1.0 - (y / 0.0076) ** 2)))
        prof.append((y, z))
    section = [(y, z) for y, z in prof] + [(y, -z) for y, z in reversed(prof)]
    bm = bmesh.new()
    rows = []
    xs = [-hw, -hw + 0.0015, hw - 0.0015, hw]
    scale = [0.86, 1.0, 1.0, 0.86]
    for x, s in zip(xs, scale):
        rows.append([bm.verts.new((x, y, z * s if abs(z) > 0.0012 else z)) for y, z in section])
    B().grid_faces(bm, rows, True)
    bm.faces.new(rows[0][::-1])
    bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append(B().obj_from_bm(prefix + '_Canvas', bm, 'MAT-MG-Canvas', coll, smooth=True))
    # hem cords along both edges of the strip
    for k, x in enumerate((-hw - 0.0004, hw + 0.0004)):
        parts.append(revolve(f'{prefix}_Hem{k}', [(-half, 0.0016), (half, 0.0016)], 'MAT-MG-Canvas', coll, axis='Y',
                             center=(x, 0.0, 0.0), segs=8, cap=False))
    # brass spacer through the canvas between this pocket and the next
    parts.append(box(prefix + '_Spacer', (0.0, half - 0.0010, 0.0), (2 * hw + 0.004, 0.0016, 0.0034), 'MAT-MG-BrassBright', coll, bevel=0.0004))
    if loaded:
        for ob in cartridge(coll, prefix, segs=12):
            ob.data.transform(Matrix.Translation((CART_SHIFT, 0.0, 0.0)))
            parts.append(ob)
    if tip:
        # brass leader tip at the free end of the empty belt
        parts.append(box(prefix + '_Tip', (0.0, half + 0.006, 0.0), (2 * hw + 0.002, 0.012, 0.0022), 'MAT-MG-BrassBright', coll, bevel=0.0008))
    return parts


# ------------------------------------------------------------------------------------------------- the belt path

def bezier(p0, p1, p2, p3, n=24):
    out = []
    for i in range(n + 1):
        t = i / n
        a = (1 - t) ** 3
        b = 3 * (1 - t) ** 2 * t
        c = 3 * (1 - t) * t * t
        d = t ** 3
        out.append(p0 * a + p1 * b + p2 * c + p3 * d)
    return out


def feed_path():
    """The fed belt path F (weapon space, points in the plane x = BELT_X), from deep in the box to deep in the
    belt bag, densely sampled; and the arc length marks (s) of the remarkable points."""
    x = BELT_X
    slot_y = 0.5 * (BOX_SLOT[2] + BOX_SLOT[3])
    deep_box = Vector((x, slot_y, BOX_MAX.z - 0.095))
    box_slot = Vector((x, slot_y, BOX_MAX.z + 0.002))
    feed_in = Vector((x, REC_HW + 0.004, 0.5 * (FEED_Z[0] + FEED_Z[1])))
    extract = Vector((x, -0.004, 0.5 * (FEED_Z[0] + FEED_Z[1])))
    exit_ = Vector((x, -REC_HW - 0.004, 0.5 * (FEED_Z[0] + FEED_Z[1])))
    bag_mouth = Vector((x, BAG_Y, BAG_TOP))
    deep_bag = Vector((x, BAG_Y, BAG_BOTTOM + 0.006))
    pts = [deep_box + (box_slot - deep_box) * (i / 12) for i in range(12)]
    # out of the slot: up and outward, round the outside of the receiver's left flank, into the feed slot
    pts += bezier(box_slot, box_slot + Vector((0, 0.040, 0.030)), feed_in + Vector((0, 0.060, -0.010)), feed_in, 40)[:-1]
    pts += [feed_in + (extract - feed_in) * (i / 10) for i in range(10)]
    pts += [extract + (exit_ - extract) * (i / 6) for i in range(6)]
    pts += bezier(exit_, exit_ + Vector((0, -0.010, 0.0)), bag_mouth + Vector((0, 0.0, 0.020)), bag_mouth, 16)[:-1]
    pts += [bag_mouth + (deep_bag - bag_mouth) * (i / 16) for i in range(17)]
    s = [0.0]
    for a, b in zip(pts, pts[1:]):
        s.append(s[-1] + (b - a).length)

    def mark(p):
        return min(range(len(pts)), key=lambda i: (pts[i] - p).length)

    marks = {k: s[mark(p)] for k, p in (('box_slot', box_slot), ('feed_in', feed_in), ('extract', extract), ('exit', exit_),
                                        ('bag_mouth', bag_mouth))}
    marks['end'] = s[-1]
    return pts, s, marks


class Path:
    """A polyline with arc length: point / tangent at s, beyond the ends extrapolated along the end tangents."""

    def __init__(self, pts):
        self.pts = [Vector(p) for p in pts]
        self.s = [0.0]
        for a, b in zip(self.pts, self.pts[1:]):
            self.s.append(self.s[-1] + max((b - a).length, 1e-9))
        self.length = self.s[-1]

    def at(self, s):
        pts, ss = self.pts, self.s
        if s <= 0.0:
            t = (pts[1] - pts[0]).normalized()
            return pts[0] + t * s, t
        if s >= self.length:
            t = (pts[-1] - pts[-2]).normalized()
            return pts[-1] + t * (s - self.length), t
        lo, hi = 0, len(ss) - 1
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if ss[mid] <= s:
                lo = mid
            else:
                hi = mid
        w = (s - ss[lo]) / max(ss[hi] - ss[lo], 1e-9)
        t = (pts[hi] - pts[lo]).normalized()
        # smooth tangent: blend with the neighbours
        t0 = (pts[lo] - pts[max(lo - 1, 0)]).normalized() if lo > 0 else t
        t1 = (pts[min(hi + 1, len(pts) - 1)] - pts[hi]).normalized() if hi < len(pts) - 1 else t
        tan = (t0 * (1 - w) + t * 1.0 + t1 * w).normalized()
        return pts[lo].lerp(pts[hi], w), tan


def pocket_frame(point, tangent, axis=Vector((1.0, 0.0, 0.0))):
    """Matrix of a belt pocket: X = cartridge axis (weapon X), Y = path tangent, Z = X x Y; translation = point."""
    x = Vector(axis).normalized()
    y = Vector(tangent)
    y = (y - x * y.dot(x)).normalized()
    z = x.cross(y).normalized()
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(point)
    return m


N_IN, N_OUT = 17, 10
OUT_SPARE = 4                            # empty pockets behind the extraction point used only by the conveyor


def belt_rest_positions():
    """{bone: arc length on F} of the fed belt at offset 0 (in_k behind the extraction point, out_j around it)."""
    pts, s, marks = feed_path()
    sx = marks['extract']
    pos = {}
    for k in range(1, N_IN + 1):
        pos[f'belt_{k:02d}'] = sx - (k - 0.5) * PITCH
    for j in range(1, N_OUT + 1):
        pos[f'belt_out_{j:02d}'] = sx + (j - OUT_SPARE - 0.5) * PITCH
    return pos


# ------------------------------------------------------------------------------------------------- the gun

def build_machinegun(coll, p=None):
    """Parts per bone, bones, sockets, cutters and the contract of the machine gun."""
    MM.ensure_all()
    parts = {n: [] for n in ('gun_root', 'jacket', 'crank', 'feed_cover', 'cocking_handle', 'steam_cap', 'ammo_box', 'tripod')}
    cutters = []
    R = parts['gun_root']
    J = parts['jacket']
    BL, BR, WN, BRS, PL = 'MAT-MG-Blued', 'MAT-MG-Brass', 'MAT-MG-Walnut', 'MAT-MG-BrassBright', 'MAT-MG-Plate'

    # ---------------------------------------------------------------- receiver
    cx = 0.5 * (REC_X0 + REC_X1)
    body = box('MG_Receiver', (cx, 0.0, 0.5 * (REC_Z0 + REC_Z1)), (REC_X1 - REC_X0, 2 * REC_HW, REC_Z1 - REC_Z0), BL, coll, bevel=0.0035)
    R.append(body)
    # feed slots through both side plates and the ejection port underneath
    for k, side in enumerate((1, -1)):
        cut = box(f'MG_FeedSlotCut{k}', (0.5 * (FEED_X[0] + FEED_X[1]), side * REC_HW, 0.5 * (FEED_Z[0] + FEED_Z[1])),
                  (FEED_X[1] - FEED_X[0], 0.030, FEED_Z[1] - FEED_Z[0]), 'MAT-MG-Bore', coll, bevel=0.0)
        cutters.append(bool_cut(body, cut, 'MAT-MG-Bore'))
    cut = box('MG_EjectCut', (EJECT.x, EJECT.y, REC_Z0), (0.044, 0.026, 0.024), 'MAT-MG-Bore', coll, bevel=0.0)
    cutters.append(bool_cut(body, cut, 'MAT-MG-Bore'))
    # dark inner walls behind the openings (the slots and the port show depth, not the far side)
    R.append(box('MG_FeedwayFloor', (0.5 * (FEED_X[0] + FEED_X[1]), 0.0, FEED_Z[0] - 0.003), (FEED_X[1] - FEED_X[0] + 0.004, 2 * REC_HW - 0.006, 0.004), 'MAT-MG-Bore', coll, bevel=0.0))
    R.append(box('MG_EjectChute', (EJECT.x, EJECT.y, REC_Z0 + 0.016), (0.040, 0.024, 0.006), 'MAT-MG-Bore', coll, bevel=0.0))
    # riveted frame strips along the top and bottom edges of both side plates, rivets on them
    for side in (1, -1):
        y = side * (REC_HW + 0.0012)
        for z, nm in ((REC_Z1 - 0.007, 'Top'), (REC_Z0 + 0.007, 'Bot')):
            R.append(box(f'MG_Frame{nm}{side:+d}', (cx + 0.002, y, z), (REC_X1 - REC_X0 - 0.010, 0.0030, 0.011), BL, coll, bevel=0.0010))
            x = REC_X0 + 0.016
            i = 0
            while x < REC_X1 - 0.010:
                skip = side < 0 and abs(x - CRANK_C.x) < 0.034 and nm == 'Top'
                if not skip:
                    R.append(rivet(f'MG_Rivet{nm}{side:+d}_{i}', (x, y + side * 0.0016, z), (0, side, 0), coll))
                x += 0.024
                i += 1
        # vertical frame strip at the rear and in front of the feed
        for xx, nm in ((REC_X0 + 0.010, 'Rear'), (FEED_X[1] + 0.009, 'Front')):
            R.append(box(f'MG_FrameV{nm}{side:+d}', (xx, y, 0.5 * (REC_Z0 + REC_Z1)), (0.010, 0.0030, REC_Z1 - REC_Z0 - 0.010), BL, coll, bevel=0.0010))
    # bottom plate: a raised plate with the ejection port frame
    R.append(box('MG_BottomPlate', (cx - 0.02, 0.0, REC_Z0 - 0.0012), (REC_X1 - REC_X0 - 0.07, 2 * REC_HW - 0.012, 0.003), BL, coll, bevel=0.0008))
    for k, (dx, dy, sx, sy) in enumerate(((0.0, 0.016, 0.050, 0.004), (0.0, -0.016, 0.050, 0.004), (0.026, 0.0, 0.004, 0.034), (-0.026, 0.0, 0.004, 0.034))):
        R.append(box(f'MG_PortFrame{k}', (EJECT.x + dx, EJECT.y + dy, REC_Z0 - 0.003), (sx, sy, 0.004), BL, coll, bevel=0.0006))
    # front trunnion block (the jacket's root)
    R.append(box('MG_Trunnion', (0.5 * (TRUNNION_X0 + TRUNNION_X1), 0.0, -0.006), (TRUNNION_X1 - TRUNNION_X0, 2 * REC_HW + 0.014, 0.122), BL, coll, bevel=0.0055))
    for side in (1, -1):
        for z in (0.040, -0.052):
            R.append(bolt_head(f'MG_TrunnionBolt{side:+d}{z:+.2f}', (0.152, side * (REC_HW + 0.007), z), (0, side, 0), coll, r=0.0042))
    # rear plate and its bolts
    R.append(box('MG_RearPlate', (REC_X0 - 0.006, 0.0, -0.006), (0.012, 2 * REC_HW + 0.010, 0.124), BL, coll, bevel=0.0030))
    for y in (0.034, -0.034):
        for z in (0.040, -0.052):
            R.append(bolt_head(f'MG_RearBolt{y:+.2f}{z:+.2f}', (REC_X0 - 0.0122, y, z), (-1, 0, 0), coll, r=0.0040))
    # rear top plate (behind the feed cover) with the folding rear sight raised
    R.append(box('MG_RearTop', (0.5 * (REC_X0 + COVER_X0) - 0.002, 0.0, REC_Z1 + 0.006), (COVER_X0 - REC_X0 - 0.004, 2 * REC_HW + 0.004, 0.012), BL, coll, bevel=0.0025))
    R.append(box('MG_SightBase', (-0.150, 0.0, REC_Z1 + 0.0135), (0.026, 0.030, 0.005), BL, coll, bevel=0.0012))
    R.append(box('MG_SightLeaf', (-0.152, 0.0, REC_Z1 + 0.032), (0.0035, 0.024, 0.034), BL, coll, bevel=0.0007))
    R.append(box('MG_SightNotch', (-0.152, 0.0, REC_Z1 + 0.046), (0.0045, 0.0040, 0.006), 'MAT-MG-Bore', coll, bevel=0.0))
    R.append(box('MG_SightSlide', (-0.152, 0.0, REC_Z1 + 0.027), (0.0070, 0.028, 0.006), BL, coll, bevel=0.0010))
    R.append(revolve('MG_SightPin', [(-0.016, 0.0018), (0.016, 0.0018)], BL, coll, axis='Y', center=(-0.150, 0.0, REC_Z1 + 0.016), segs=8))
    # spade grips (walnut) on their arms, firing lever between them
    for side in (1, -1):
        y = side * 0.040
        R.append(box(f'MG_GripArmTop{side:+d}', (-0.233, y, 0.040), (0.046, 0.012, 0.009), BL, coll, bevel=0.0025))
        R.append(box(f'MG_GripArmBot{side:+d}', (-0.233, y, -0.058), (0.046, 0.012, 0.009), BL, coll, bevel=0.0025))
        R.append(B().tube(f'MG_SpadeGrip{side:+d}', [V(-0.254, y, -0.054), V(-0.254, y, -0.034), V(-0.254, y, 0.000), V(-0.254, y, 0.022),
                                                       V(-0.254, y, 0.036)], [0.0106, 0.0128, 0.0134, 0.0126, 0.0106], WN, coll, segs=16))
        for z in (0.040, -0.058):
            R.append(revolve(f'MG_GripFerrule{side:+d}{z:+.2f}', [(-0.004, 0.0105), (0.004, 0.0105)], BL, coll, axis='Z',
                             center=(-0.254, y, z + (-0.003 if z > 0 else 0.003)), segs=14))
            R.append(bolt_head(f'MG_GripNut{side:+d}{z:+.2f}', (-0.254, y, z + (0.0045 if z > 0 else -0.0045)), (0, 0, 1 if z > 0 else -1), coll, r=0.0045))
    R.append(box('MG_FiringLever', (-0.236, 0.0, -0.006), (0.008, 0.044, 0.024), BL, coll, bevel=0.0025))
    R.append(revolve('MG_LeverPin', [(-0.026, 0.0028), (0.026, 0.0028)], BL, coll, axis='Y', center=(-0.230, 0.0, 0.006), segs=10))
    R.append(box('MG_SafetyCatch', (-0.226, 0.0, 0.022), (0.012, 0.010, 0.010), BL, coll, bevel=0.0020))
    # crank housing (right side): gear case with its bolts and the hub boss
    hc = Vector((CRANK_C.x, -REC_HW, CRANK_C.z))
    R.append(revolve('MG_CrankHousing', [(0.0, 0.0), (0.0, 0.0285), (0.003, 0.0295), (0.016, 0.0280), (0.020, 0.0255), (0.020, 0.0)],
                     BL, coll, axis='-Y', center=hc, segs=40))
    for i in range(6):
        a = TAU * i / 6 + 0.3
        R.append(bolt_head(f'MG_HousingBolt{i}', (hc.x + 0.0205 * math.cos(a), hc.y - 0.020, hc.z + 0.0205 * math.sin(a)), (0, -1, 0), coll, r=0.0030, h=0.0025))
    R.append(revolve('MG_HubBoss', [(0.019, 0.0), (0.019, 0.017), (0.026, 0.016), (0.028, 0.012), (0.028, 0.0)], BL, coll, axis='-Y', center=hc, segs=28))
    # data plate (brass) on the right side, behind the crank
    R.append(box('MG_DataPlate', (-0.178, -REC_HW - 0.0016, -0.030), (0.040, 0.0016, 0.024), PL, coll, bevel=0.0006))
    for i, (dx, dz) in enumerate(((-0.016, 0.009), (0.016, 0.009), (-0.016, -0.009), (0.016, -0.009))):
        R.append(rivet(f'MG_PlateRivet{i}', (-0.178 + dx, -REC_HW - 0.0025, -0.030 + dz), (0, -1, 0), coll, r=0.0018, mat=PL, seg=8))
    # feed trays (left: the belt's ledge and guide horns; right: the exit lip)
    fx = 0.5 * (FEED_X[0] + FEED_X[1])
    R.append(box('MG_FeedTray', (fx, REC_HW + 0.008, FEED_Z[0] - 0.002), (FEED_X[1] - FEED_X[0] + 0.006, 0.016, 0.0040), BL, coll, bevel=0.0010))
    for k, xx in enumerate((FEED_X[0] - 0.003, FEED_X[1] + 0.003)):
        R.append(box(f'MG_FeedHorn{k}', (xx, REC_HW + 0.008, 0.5 * (FEED_Z[0] + FEED_Z[1])), (0.004, 0.016, FEED_Z[1] - FEED_Z[0] + 0.006), BL, coll, bevel=0.0010))
    R.append(box('MG_ExitLip', (fx, -REC_HW - 0.006, FEED_Z[0] - 0.002), (FEED_X[1] - FEED_X[0] + 0.006, 0.012, 0.0035), BL, coll, bevel=0.0010))
    # cocking slide slot on the left side
    R.append(box('MG_CockSlot', (COCK_KNOB.x - 0.5 * COCK_TRAVEL + 0.002, REC_HW + 0.0018, COCK_KNOB.z), (COCK_TRAVEL + 0.024, 0.0020, 0.010), 'MAT-MG-Bore', coll, bevel=0.0))
    R.append(box('MG_CockRail', (COCK_KNOB.x - 0.5 * COCK_TRAVEL + 0.002, REC_HW + 0.0024, COCK_KNOB.z + 0.0085), (COCK_TRAVEL + 0.030, 0.0030, 0.004), BL, coll, bevel=0.0008))
    # box bracket: dovetail rail under the left half of the receiver, its latch
    R.append(box('MG_BoxRail', (0.5 * (BOX_MIN.x + BOX_MAX.x), 0.5 * (BOX_MIN.y + BOX_MAX.y), REC_Z0 - 0.006), (BOX_MAX.x - BOX_MIN.x - 0.030, 0.060, 0.010), BL, coll, bevel=0.0018))
    R.append(box('MG_BoxLatch', (BOX_MAX.x - 0.010, BOX_MAX.y - 0.004, REC_Z0 - 0.010), (0.010, 0.010, 0.016), BL, coll, bevel=0.0018))
    # flat canvas pouch for the empty belt under the right exit, its sheet metal mouth (dark opening) and strap
    R.append(box('MG_BeltBag', (BELT_X, BAG_Y, 0.5 * (BAG_TOP + BAG_BOTTOM)), (0.062, 2 * BAG_HALF_T, BAG_TOP - BAG_BOTTOM), 'MAT-MG-Canvas', coll, bevel=0.0045))
    R.append(box('MG_BagMouth', (BELT_X, BAG_Y, BAG_TOP + 0.001), (0.066, 2 * BAG_HALF_T + 0.003, 0.005), BL, coll, bevel=0.0012))
    R.append(box('MG_BagOpening', (BELT_X, BAG_Y, BAG_TOP + 0.0036), (0.052, 2 * BAG_HALF_T - 0.006, 0.0008), 'MAT-MG-Bore', coll, bevel=0.0))
    R.append(box('MG_BagHanger', (BELT_X, -REC_HW - 0.004, REC_Z0 + 0.020), (0.030, 0.008, 0.040), BL, coll, bevel=0.0015))
    R.append(box('MG_BagStrap', (BELT_X, BAG_Y - BAG_HALF_T - 0.0012, 0.5 * (BAG_TOP + BAG_BOTTOM)), (0.012, 0.0025, BAG_TOP - BAG_BOTTOM - 0.012), 'MAT-MG-Leather', coll, bevel=0.0008))
    # sling (leather) along the lower right flank of the jacket, buckle and two swivels
    sl = []
    for i in range(13):
        t = i / 12
        x = 0.640 - (0.640 - 0.186) * t
        ang = math.radians(-36.0)
        r = JACKET_R + 0.0065 + 0.010 * math.sin(math.pi * t)
        sl.append(V(x, r * math.cos(ang) * -1.0, r * math.sin(ang)))
    sides = [V(0, -math.sin(math.radians(36.0)), math.cos(math.radians(36.0))) for _ in sl]
    R.append(sweep('MG_Sling', sl, sides, 0.0135, 0.0022, 'MAT-MG-Leather', coll, segs=10, expo=6.0))
    R.append(box('MG_SlingBuckle', (0.420, -(JACKET_R + 0.0185) * math.cos(math.radians(-36)), (JACKET_R + 0.0185) * math.sin(math.radians(-36))),
                 (0.020, 0.004, 0.030), 'MAT-MG-Plate', coll, bevel=0.0010, rot=Matrix.Rotation(math.radians(36.0), 3, 'X')))
    for k, x in enumerate((0.640, 0.186)):
        J.append(K.ring(f'MG_Swivel{k}', (x, -(JACKET_R + 0.006) * math.cos(math.radians(-36)), (JACKET_R + 0.006) * math.sin(math.radians(-36))),
                        0.009, 0.0022, BL, coll, axis='X', segs=18, minor_segs=6))

    # ---------------------------------------------------------------- jacket (browned brass), barrel group
    bands = [(JACKET_X0, JACKET_X0 + 0.030), (0.432, 0.452), (JACKET_X1 - 0.026, JACKET_X1)]

    def flute(th, t):
        for a, b in bands:
            if a - 0.004 <= t <= b + 0.004:
                return 1.0 + 0.048 if a <= t <= b else 1.0
        return 1.0 - 0.060 * (0.5 + 0.5 * math.cos(20 * th)) ** 2

    stations = []
    x = JACKET_X0
    marks = {JACKET_X0, JACKET_X1}
    for a, b in bands:
        marks |= {a, b, a - 0.004, b + 0.004, a - 0.0045, b + 0.0045}
    while x < JACKET_X1:
        stations.append(x)
        x += 0.028
    stations = sorted(set([round(v, 5) for v in stations + list(marks) if JACKET_X0 <= v <= JACKET_X1]))
    profile = [(JACKET_X0, 0.0)] + [(v, JACKET_R) for v in stations] + [(JACKET_X1, 0.0)]
    J.append(revolve('MG_Jacket', profile, BR, coll, axis='X', segs=120, radial=flute))
    J.append(revolve('MG_JacketFront', [(JACKET_X1 - 0.002, 0.0548), (JACKET_X1 + 0.004, 0.0546), (JACKET_X1 + 0.011, 0.050), (JACKET_X1 + 0.016, 0.040),
                                         (JACKET_X1 + 0.018, 0.024), (JACKET_X1 + 0.018, 0.0)], BR, coll, axis='X', segs=48))
    for k, (a, b) in enumerate(bands):
        for xx in (a, b):
            J.append(K.ring(f'MG_BandBead{k}{xx:.3f}', (xx, 0, 0), JACKET_R * 1.048, 0.0016, BR, coll, axis='X', segs=64, minor_segs=6))
    # rear filler cap (knurled) and the drain plug
    J.append(revolve('MG_FillerBoss', [(0.042, 0.0), (0.042, 0.017), (0.059, 0.016), (0.059, 0.0)], BR, coll, axis='Z', center=(0.215, 0, 0), segs=28))
    J.append(revolve('MG_FillerCap', [(0.058, 0.0), (0.058, 0.019), (0.066, 0.019), (0.068, 0.016), (0.068, 0.0)], PL, coll, axis='Z',
                     center=(0.215, 0, 0), segs=36, radial=lambda th, t: 1.0 - 0.05 * (0.5 + 0.5 * math.cos(24 * th))))
    J.append(K.ring('MG_FillerChain', (0.230, 0.0, 0.060), 0.004, 0.0009, BRS, coll, axis='Y', segs=12, minor_segs=5))
    J.append(revolve('MG_DrainPlug', [(0.045, 0.0), (0.045, 0.012), (0.058, 0.012), (0.058, 0.0)], PL, coll, axis='-Z', center=(0.662, 0, 0), segs=6, smooth=False))
    # steam valve boss (the flap is the steam_cap bone)
    J.append(revolve('MG_SteamBoss', [(0.044, 0.0), (0.044, 0.017), (0.058, 0.016), (0.0595, 0.013), (0.0595, 0.0)], BR, coll, axis='Z',
                     center=(STEAM.x, 0, 0), segs=28))
    J.append(revolve('MG_SteamVent', [(0.0597, 0.0), (0.0597, 0.0085), (0.0598, 0.0)], 'MAT-MG-Bore', coll, axis='Z', center=(STEAM.x, 0, 0), segs=20))
    J.append(box('MG_SteamHingeBlock', (STEAM_HINGE.x - 0.002, 0.0, 0.057), (0.008, 0.016, 0.006), BR, coll, bevel=0.0012))
    # steam escape fitting (left front) and the condensing hose back along the left flank to a nozzle on the rear band
    J.append(B().tube('MG_SteamElbow', [V(0.655, 0.046, 0.012), V(0.655, 0.060, 0.012), V(0.650, 0.067, 0.004), V(0.640, 0.068, -0.006)],
                      [0.0068, 0.0068, 0.0068, 0.0068], BR, coll, segs=14))
    J.append(revolve('MG_HoseClampF', [(-0.006, 0.0098), (0.006, 0.0098)], BL, coll, axis='Z', center=(0.638, 0.068, -0.012), segs=16, cap=False))
    hose = [V(0.638, 0.068, -0.008), V(0.622, 0.069, -0.004), V(0.560, 0.067, 0.004), V(0.460, 0.066, 0.007), V(0.360, 0.066, 0.007),
            V(0.260, 0.065, 0.005), V(0.212, 0.062, -0.002), V(0.192, 0.059, -0.010)]
    J.append(B().tube('MG_Hose', hose, [0.0088] * len(hose), 'MAT-MG-Rubber', coll, segs=14))
    J.append(B().tube('MG_HoseNozzle', [V(0.194, 0.059, -0.009), V(0.180, 0.057, -0.015)], [0.0070, 0.0058], BR, coll, segs=12))
    J.append(box('MG_HoseClip', (0.445, 0.058, 0.007), (0.012, 0.012, 0.020), BL, coll, bevel=0.0020))
    # front sight with protecting ears on the front band
    J.append(box('MG_FrontSightBase', (0.686, 0.0, JACKET_R * 1.048 + 0.003), (0.020, 0.016, 0.007), BL, coll, bevel=0.0015))
    J.append(box('MG_FrontSightBlade', (0.686, 0.0, JACKET_R * 1.048 + 0.012), (0.009, 0.0026, 0.012), BL, coll, bevel=0.0005))
    for side in (1, -1):
        J.append(box(f'MG_FrontSightEar{side:+d}', (0.686, side * 0.0075, JACKET_R * 1.048 + 0.011), (0.012, 0.0022, 0.014), BL, coll, bevel=0.0006))
    # carrying handle on the rear of the jacket: clamp ring, two posts, wooden grip
    J.append(revolve('MG_HandleClamp', [(0.252, 0.0558), (0.268, 0.0558)], BL, coll, axis='X', segs=48, cap=False))
    for xx in (0.238, 0.282):
        J.append(box(f'MG_HandlePost{xx:.3f}', (xx, 0.0, 0.066), (0.008, 0.010, 0.024), BL, coll, bevel=0.0018))
    J.append(box('MG_HandleBase', (0.260, 0.0, 0.0565), (0.060, 0.012, 0.006), BL, coll, bevel=0.0015))
    J.append(B().tube('MG_CarryGrip', [V(0.226, 0, 0.080), V(0.245, 0, 0.081), V(0.275, 0, 0.081), V(0.294, 0, 0.080)], [0.0090, 0.0105, 0.0105, 0.0090], WN, coll, segs=14))
    # fore-grip (left hand): clamp ring, stem, walnut grip and its cap
    J.append(revolve('MG_ForeClamp', [(FOREGRIP.x - 0.009, 0.0560), (FOREGRIP.x + 0.009, 0.0560)], BL, coll, axis='X', segs=48, cap=False))
    fu = FOREGRIP_DIR
    fo = Vector((FOREGRIP_X, 0.0, 0.0))
    J.append(B().tube('MG_ForeStem', [fo + fu * 0.050, fo + fu * 0.062, fo + fu * 0.068], [0.0105, 0.0105, 0.0095], BL, coll, segs=14))
    J.append(B().tube('MG_ForeGrip', [fo + fu * 0.066, fo + fu * 0.080, fo + fu * 0.108, fo + fu * 0.136, fo + fu * 0.152],
                      [0.0135, 0.0160, 0.0168, 0.0160, 0.0140], WN, coll, segs=18))
    cap = revolve('MG_ForeCap', [(0.0, 0.0), (0.0, 0.0145), (0.006, 0.0140), (0.008, 0.0)], BL, coll, axis='Z', segs=18)
    cap.data.transform(Matrix.Translation(fo + fu * 0.151) @ Vector((0, 0, 1)).rotation_difference(fu).to_matrix().to_4x4())
    J.append(cap)
    # gland nut, exposed barrel, conical flash hider (the dark steel; its own heat channel)
    knurl = lambda th, t: 1.0 - 0.04 * (0.5 + 0.5 * math.cos(30 * th))  # noqa: E731
    J.append(revolve('MG_GlandNut', [(GLAND_X - 0.001, 0.0), (GLAND_X - 0.001, 0.021), (GLAND_X + 0.018, 0.021), (GLAND_X + 0.020, 0.016), (GLAND_X + 0.020, 0.0)],
                     'MAT-MG-DarkSteel', coll, axis='X', segs=40, radial=knurl))
    J.append(revolve('MG_Barrel', [(GLAND_X + 0.018, 0.0), (GLAND_X + 0.018, 0.0128), (CONE_X0 + 0.004, 0.0124), (CONE_X0 + 0.004, 0.0)],
                     'MAT-MG-DarkSteel', coll, axis='X', segs=28))
    J.append(revolve('MG_FlashCone', [(CONE_X0, 0.0), (CONE_X0, 0.0150), (CONE_X0 + 0.008, 0.0190), (CONE_X0 + 0.030, 0.0218), (0.850, 0.0285),
                                      (MUZZLE_X - 0.006, 0.0352), (MUZZLE_X, 0.0360), (MUZZLE_X + 0.001, 0.0350), (MUZZLE_X - 0.002, 0.0330),
                                      (0.850, 0.0255), (CONE_X0 + 0.030, 0.0120), (CONE_X0 + 0.022, 0.0060), (CONE_X0 + 0.020, 0.0)],
                     'MAT-MG-DarkSteel', coll, axis='X', segs=40))
    for k, xx in enumerate((CONE_X0 + 0.012, 0.842)):
        r = 0.0195 if k == 0 else 0.0275
        J.append(K.ring(f'MG_ConeRing{k}', (xx, 0, 0), r, 0.0018, 'MAT-MG-DarkSteel', coll, axis='X', segs=40, minor_segs=6))
    J.append(revolve('MG_Muzzle', [(CONE_X0 + 0.021, 0.0), (CONE_X0 + 0.021, 0.0044), (CONE_X0 + 0.0215, 0.0)], 'MAT-MG-Bore', coll, axis='X', segs=16))

    # ---------------------------------------------------------------- steam valve flap (steam_cap)
    sc = parts['steam_cap']
    sc.append(revolve('MG_SteamFlap', [(0.0, 0.0), (0.0, 0.0175), (0.0035, 0.0172), (0.0055, 0.014), (0.0060, 0.0)], PL, coll, axis='Z',
                      center=(STEAM.x, 0, 0.0598), segs=28))
    sc.append(revolve('MG_SteamHinge', [(-0.009, 0.0026), (0.009, 0.0026)], BRS, coll, axis='Y', center=STEAM_HINGE, segs=10))
    sc.append(box('MG_SteamTab', (STEAM.x + 0.019, 0.0, 0.0640), (0.008, 0.008, 0.0025), PL, coll, bevel=0.0008))
    sc.append(box('MG_SteamArm', (STEAM.x - 0.010, 0.0, 0.0625), (0.022, 0.006, 0.0025), PL, coll, bevel=0.0008))

    # ---------------------------------------------------------------- crank
    ck = parts['crank']
    cy0, cy1 = CRANK_ARM_Y
    ck.append(revolve('MG_CrankHub', [(0.028, 0.0), (0.028, 0.015), (0.0365, 0.0145), (0.0380, 0.010), (0.0380, 0.0)], BL, coll, axis='-Y',
                      center=(CRANK_C.x, -REC_HW, CRANK_C.z), segs=28))
    ck.append(box('MG_CrankNut', (CRANK_C.x, -0.0855, CRANK_C.z), (0.016, 0.006, 0.016), BL, coll, bevel=0.0018,
                  rot=Matrix.Rotation(math.radians(45.0), 3, 'Y')))
    hp = crank_handle_point(CRANK_REST_DEG)
    u = Vector((hp.x - CRANK_C.x, 0.0, hp.z - CRANK_C.z)).normalized()
    nrm = Vector((-u.z, 0.0, u.x))
    outline = []
    for i in range(13):
        a = math.pi * (0.5 + i / 12)
        outline.append(Vector((CRANK_C.x, 0, CRANK_C.z)) + (u * math.cos(a) + nrm * math.sin(a)) * 0.0125)
    for i in range(13):
        a = math.pi * (-0.5 + i / 12)
        outline.append(Vector((hp.x, 0, hp.z)) + (u * math.cos(a) + nrm * math.sin(a)) * 0.0085)
    ck.append(K.plate('MG_CrankArm', [(p.x, p.z) for p in outline], cy0, cy1, BL, coll, bevel=0.0012))
    ck.append(revolve('MG_HandlePin', [(-cy1 - 0.001, 0.0062), (-CRANK_HANDLE_Y[0] + 0.002, 0.0062)], BL, coll, axis='-Y', center=(hp.x, 0, hp.z), segs=12))
    hy0, hy1 = CRANK_HANDLE_Y
    ck.append(B().tube('MG_CrankHandle', [V(hp.x, hy0, hp.z), V(hp.x, hy0 - 0.012, hp.z), V(hp.x, 0.5 * (hy0 + hy1), hp.z), V(hp.x, hy1 + 0.010, hp.z),
                                           V(hp.x, hy1, hp.z)], [0.0118, 0.0136, 0.0142, 0.0135, 0.0115], WN, coll, segs=18))
    ck.append(revolve('MG_HandleCap', [(0.0, 0.0), (0.0, 0.0102), (0.0035, 0.0092), (0.0055, 0.0040), (0.0058, 0.0)], BL, coll, axis='-Y',
                      center=(hp.x, hy1 + 0.001, hp.z), segs=16))
    ck.append(revolve('MG_HandleFerrule', [(-hy0 - 0.0005, 0.0122), (-hy0 + 0.0045, 0.0122)], BL, coll, axis='-Y', center=(hp.x, 0, hp.z), segs=16, cap=False))

    # ---------------------------------------------------------------- feed cover (hinged at the front)
    fc = parts['feed_cover']
    cxc = 0.5 * (COVER_X0 + COVER_X1)
    fc.append(box('MG_FeedCover', (cxc, 0.0, REC_Z1 + 0.0085), (COVER_X1 - COVER_X0, 2 * REC_HW + 0.004, 0.017), BL, coll, bevel=0.0035))
    for y in (0.021, -0.021):
        fc.append(box(f'MG_CoverRib{y:+.3f}', (cxc - 0.004, y, TOP_Z + 0.0015), (COVER_X1 - COVER_X0 - 0.030, 0.007, 0.004), BL, coll, bevel=0.0012))
    for i in range(4):
        for y in (0.036, -0.036):
            fc.append(rivet(f'MG_CoverRivet{i}{y:+.3f}', (COVER_X0 + 0.024 + i * 0.048, y, TOP_Z + 0.0003), (0, 0, 1), coll, r=0.0030))
    fc.append(box('MG_CoverLatch', (COVER_X0 + 0.006, 0.0, TOP_Z + 0.003), (0.014, 0.034, 0.008), BL, coll, bevel=0.0020))
    fc.append(revolve('MG_LatchButton', [(0.0, 0.0), (0.0, 0.0050), (0.0040, 0.0046), (0.0055, 0.0)], BL, coll, axis='Z',
                      center=(COVER_X0 + 0.006, 0.0, TOP_Z + 0.007), segs=14))
    for y0, y1 in ((-0.046, -0.020), (0.020, 0.046)):
        fc.append(revolve(f'MG_HingeKnuckle{y0:+.3f}', [(y0, 0.0058), (y1, 0.0058)], BL, coll, axis='Y', center=(COVER_HINGE.x, 0, COVER_HINGE.z), segs=14))
    R.append(revolve('MG_HingeKnuckleFixed', [(-0.019, 0.0056), (0.019, 0.0056)], BL, coll, axis='Y', center=(COVER_HINGE.x, 0, COVER_HINGE.z), segs=14))

    # ---------------------------------------------------------------- cocking handle (left side)
    cc = parts['cocking_handle']
    cc.append(box('MG_CockStem', (COCK_KNOB.x, REC_HW + 0.010, COCK_KNOB.z), (0.012, 0.020, 0.008), BL, coll, bevel=0.0018))
    cc.append(revolve('MG_CockKnob', [(0.012, 0.0), (0.012, 0.0080), (0.016, 0.0112), (0.026, 0.0112), (0.030, 0.0085), (0.031, 0.0)], BL, coll,
                      axis='Y', center=(COCK_KNOB.x, REC_HW, COCK_KNOB.z), segs=24, radial=lambda th, t: 1.0 - (0.06 * (0.5 + 0.5 * math.cos(16 * th)) if 0.017 < t < 0.025 else 0.0)))

    # ---------------------------------------------------------------- tripod, folded forward under the jacket
    tp = parts['tripod']
    tp.append(revolve('MG_TripodClamp', [(0.462, 0.0560), (0.482, 0.0560)], BL, coll, axis='X', segs=48, cap=False))
    tp.append(box('MG_TripodHead', (0.472, 0.0, -0.064), (0.030, 0.050, 0.020), BL, coll, bevel=0.0040))
    for side in (1, -1):
        y = side * 0.030
        tp.append(B().tube(f'MG_TripodLeg{side:+d}', [V(0.480, y, -0.068), V(0.560, y, -0.066), V(0.640, y * 1.08, -0.062), V(0.690, y * 1.1, -0.060)],
                           [0.0092, 0.0088, 0.0082, 0.0074], BL, coll, segs=12))
        tp.append(B().tube(f'MG_TripodFoot{side:+d}', [V(0.690, y * 1.1, -0.060), V(0.708, y * 1.1, -0.060), V(0.722, y * 1.1, -0.061)],
                           [0.0070, 0.0050, 0.0012], 'MAT-MG-Iron', coll, segs=10))
        tp.append(revolve(f'MG_TripodPivot{side:+d}', [(-0.006, 0.0050), (0.006, 0.0050)], BL, coll, axis='Y', center=(0.476, y, -0.068), segs=10))
    tp.append(box('MG_TripodStrap', (0.610, 0.0, -0.064), (0.012, 0.074, 0.010), 'MAT-MG-Leather', coll, bevel=0.0020))

    # ---------------------------------------------------------------- ammunition box
    bx = parts['ammo_box']
    bx += build_box_parts(coll, BOX_MIN, BOX_MAX)

    # ---------------------------------------------------------------- belt (built at its rest pockets on F)
    belt_parts = {}
    pts, s, fmarks = feed_path()
    path = Path(pts)
    for name, sp in belt_rest_positions().items():
        point, tan = path.at(sp)
        frame = pocket_frame(point, tan)
        loaded = not name.startswith('belt_out')
        made = belt_segment(coll, 'MG_' + name, loaded=loaded, tip=(name == f'belt_out_{N_OUT:02d}'))
        for ob in made:
            ob.data.transform(frame)
        belt_parts[name] = made

    # ---------------------------------------------------------------- bones and sockets
    bones = {
        'gun_root': (V(0, 0, 0), V(0.12, 0, 0), None),
        'jacket': (V(JACKET_X0, 0, 0), V(JACKET_X0 + 0.13, 0, 0), 'gun_root'),
        'crank': (V(CRANK_C.x, CRANK_ARM_Y[0], CRANK_C.z), V(CRANK_C.x, CRANK_ARM_Y[0] - 0.045, CRANK_C.z), 'gun_root'),
        'feed_cover': (COVER_HINGE.copy(), COVER_HINGE + V(-0.10, 0, 0), 'gun_root'),
        'cocking_handle': (V(COCK_KNOB.x, REC_HW + 0.004, COCK_KNOB.z), V(COCK_KNOB.x + 0.04, REC_HW + 0.004, COCK_KNOB.z), 'gun_root'),
        'steam_cap': (STEAM_HINGE.copy(), STEAM_HINGE + V(0.04, 0, 0), 'jacket'),
        'ammo_box': ((BOX_MIN + BOX_MAX) * 0.5, (BOX_MIN + BOX_MAX) * 0.5 + V(0, 0, 0.05), 'gun_root'),
        'tripod': (V(0.472, 0, -0.064), V(0.60, 0, -0.064), 'jacket'),
    }
    for name, sp in belt_rest_positions().items():
        point, tan = path.at(sp)
        bones[name] = (point.copy(), point + tan.normalized() * 0.012, 'gun_root')
    eject_tail = EJECT + EJECT_DIR * 0.012
    sockets = {
        'grip_r': ('crank', GRIP_R.copy()),
        'grip_l': ('jacket', GRIP_L.copy()),
        'grip_l_tp': ('jacket', CARRY_GRIP.copy()),
        'muzzle': ('jacket', V(MUZZLE_X, 0, 0)),
        'eject': ('gun_root', EJECT.copy(), eject_tail),
        'eject_dir': ('gun_root', EJECT + EJECT_DIR * 0.050, EJECT + EJECT_DIR * 0.062),
        'steam': ('steam_cap', V(STEAM.x, 0, 0.066)),
        'jacket': ('jacket', V(0.43, 0, JACKET_R + 0.004)),
        'hose': ('jacket', V(0.180, 0.057, -0.016)),
        'feed': ('gun_root', V(BELT_X, REC_HW + 0.010, 0.5 * (FEED_Z[0] + FEED_Z[1]))),
        'belt_out': ('gun_root', V(BELT_X, -REC_HW - 0.010, 0.5 * (FEED_Z[0] + FEED_Z[1]))),
        'box': ('ammo_box', (BOX_MIN + BOX_MAX) * 0.5),
        'box_slot': ('ammo_box', V(BELT_X, 0.5 * (BOX_SLOT[2] + BOX_SLOT[3]), BOX_MAX.z + 0.004)),
        'cocking': ('cocking_handle', COCK_KNOB.copy()),
        'cover_latch': ('feed_cover', V(COVER_X0 + 0.006, 0, TOP_Z + 0.008)),
        'crank_handle': ('crank', crank_handle_point(CRANK_REST_DEG, CRANK_HANDLE_Y[1] - 0.004)),
    }
    all_parts = dict(parts)
    all_parts.update(belt_parts)
    groups = {
        'Body': ['gun_root', 'crank', 'feed_cover', 'cocking_handle', 'tripod'],
        'Jacket': ['jacket', 'steam_cap'],
        'Belt': list(belt_parts),
        'Box': ['ammo_box'],
    }
    # the sling, the bag, the hose... live on gun_root / jacket; split per bake group by part name
    lo = [min(v[i] for v in (BOX_MIN, V(REC_X0 - 0.06, -0.165, -0.21))) for i in range(3)]
    hi = [MUZZLE_X + 0.002, 0.12, 0.09]
    contract = {
        'forward_axis': '+X', 'up_axis': '+Z', 'right_side': '-Y', 'bore_z_m': 0.0, 'units': 'metres',
        'crank': {'axle_m': [*CRANK_C], 'axis': 'weapon Y (handle turns forward over the top: the angle about +Y grows)',
                  'radius_m': CRANK_R, 'rest_angle_deg': CRANK_REST_DEG, 'shots_per_turn': 4},
        'belt': {'pitch_m': PITCH, 'method': 'chain of bones (belt_01..16 loaded, belt_out_01..10 empty), placed on the belt path every frame',
                 'loaded_bones': N_IN, 'empty_bones': N_OUT, 'spare_empty_bones': OUT_SPARE, 'path_marks_m': fmarks},
        'feed_cover_hinge_m': [*COVER_HINGE], 'cocking_travel_m': COCK_TRAVEL, 'eject_direction_weapon': [round(v, 4) for v in EJECT_DIR],
        'eject_direction_unreal_weapon_space': [round(EJECT_DIR.x, 4), round(-EJECT_DIR.y, 4), round(EJECT_DIR.z, 4)],
        'box_bounds_m': {'min': [*BOX_MIN], 'max': [*BOX_MAX]}, 'steam_hinge_m': [*STEAM_HINGE],
    }
    return {'parts': all_parts, 'groups': groups, 'sockets': sockets, 'bones': bones, 'cutters': cutters, 'contract': contract,
            'params': {'pitch_m': PITCH, 'crank_radius_m': CRANK_R}, 'bounds': {'min': lo, 'max': hi}}


def build_box_parts(coll, mn, mx, closed_slot_tab=False):
    """The wooden ammunition box bound with iron: planks with real seams, two iron hoops with nails, corner irons,
    hinged lid with the belt slot (lip in iron), hasp, end handle, the dovetail shoe on top."""
    parts = []
    W, I, D = 'MAT-MG-BoxWood', 'MAT-MG-Iron', 'MAT-MG-BoxDark'
    c = (mn + mx) * 0.5
    size = mx - mn
    t = 0.0065                           # plank thickness
    parts.append(box('MGBox_Core', c, (size.x - 2 * t + 0.002, size.y - 2 * t + 0.002, size.z - 2 * t + 0.002), D, coll, bevel=0.0))
    # side planks (two per side, horizontal), end planks (vertical), bottom
    for side, y in ((1, mx.y - t * 0.5), (-1, mn.y + t * 0.5)):
        h = (size.z - 0.0015) * 0.5
        for k in range(2):
            z = mn.z + h * (k + 0.5) + 0.0005 * k
            parts.append(box(f'MGBox_Side{side:+d}{k}', (c.x, y, z), (size.x, t, h - 0.0012), W, coll, bevel=0.0012))
    for end, x in ((1, mx.x - t * 0.5), (-1, mn.x + t * 0.5)):
        w = (size.y - 2 * t) * 0.5
        for k in range(2):
            y = mn.y + t + w * (k + 0.5)
            parts.append(box(f'MGBox_End{end:+d}{k}', (x, y, c.z - 0.002), (t, w - 0.0012, size.z - 0.010), W, coll, bevel=0.0012))
    parts.append(box('MGBox_Bottom', (c.x, c.y, mn.z + t * 0.5), (size.x - 0.002, size.y - 0.002, t), W, coll, bevel=0.0012))
    # lid around the slot: front and rear boards, the outer and inner strips beside the slot
    sx0, sx1, sy0, sy1 = BOX_SLOT
    zt = mx.z - t * 0.5
    parts.append(box('MGBox_LidRear', (0.5 * (mn.x + sx0), c.y, zt), (sx0 - mn.x, size.y, t), W, coll, bevel=0.0012))
    parts.append(box('MGBox_LidFront', (0.5 * (sx1 + mx.x), c.y, zt), (mx.x - sx1, size.y, t), W, coll, bevel=0.0012))
    parts.append(box('MGBox_LidIn', (0.5 * (sx0 + sx1), 0.5 * (mn.y + sy0), zt), (sx1 - sx0, sy0 - mn.y, t), W, coll, bevel=0.0012))
    parts.append(box('MGBox_LidOut', (0.5 * (sx0 + sx1), 0.5 * (sy1 + mx.y), zt), (sx1 - sx0, mx.y - sy1, t), W, coll, bevel=0.0012))
    parts.append(box('MGBox_SlotLip', (0.5 * (sx0 + sx1), sy1 + 0.0015, mx.z + 0.0010), (sx1 - sx0 + 0.004, 0.003, 0.006), I, coll, bevel=0.0006))
    parts.append(box('MGBox_SlotLipIn', (0.5 * (sx0 + sx1), sy0 - 0.0015, mx.z + 0.0010), (sx1 - sx0 + 0.004, 0.003, 0.006), I, coll, bevel=0.0006))
    # the next two cartridges seen in the slot
    for k, dy in enumerate((-0.004, 0.006)):
        for ob in cartridge(coll, f'MGBox_SlotCart{k}', segs=10):
            ob.data.transform(Matrix.Translation((BELT_X + CART_SHIFT, 0.5 * (sy0 + sy1) + dy, mx.z - 0.010 - 0.004 * k)))
            parts.append(ob)
    # iron hoops (top, bottom, both sides) with nails
    for k, x in enumerate((mn.x + 0.032, mx.x - 0.032)):
        w = 0.013
        parts.append(box(f'MGBox_HoopS{k}+', (x, mx.y + 0.0009, c.z), (w, 0.0018, size.z + 0.002), I, coll, bevel=0.0005))
        parts.append(box(f'MGBox_HoopS{k}-', (x, mn.y - 0.0009, c.z), (w, 0.0018, size.z + 0.002), I, coll, bevel=0.0005))
        parts.append(box(f'MGBox_HoopB{k}', (x, c.y, mn.z - 0.0009), (w, size.y + 0.0036, 0.0018), I, coll, bevel=0.0005))
        for zz in (mn.z + 0.020, c.z, mx.z - 0.020):
            for yy, n in ((mx.y + 0.0018, 1), (mn.y - 0.0018, -1)):
                parts.append(rivet(f'MGBox_Nail{k}{zz:.3f}{n:+d}', (x, yy, zz), (0, n, 0), coll, r=0.0019, mat=I, seg=8))
    # corner irons
    for xs in (mn.x, mx.x):
        for ys in (mn.y, mx.y):
            for zs in (mn.z, mx.z):
                sxv = -1 if xs == mn.x else 1
                syv = -1 if ys == mn.y else 1
                szv = -1 if zs == mn.z else 1
                parts.append(box(f'MGBox_Corner{xs:+.2f}{ys:+.2f}{zs:+.2f}', (xs - sxv * 0.008, ys - syv * 0.008, zs - szv * 0.008),
                                 (0.0175, 0.0175, 0.0175), I, coll, bevel=0.0020))
    # end handle (iron bail on cleats) on the front end, hasp on the outer side of the lid
    parts.append(K.ring('MGBox_Bail', (mx.x + 0.010, c.y, c.z + 0.012), 0.020, 0.0026, I, coll, axis='X', segs=24, minor_segs=6))
    for dy in (-0.022, 0.022):
        parts.append(box(f'MGBox_Cleat{dy:+.3f}', (mx.x + 0.0025, c.y + dy, c.z + 0.012), (0.005, 0.010, 0.016), I, coll, bevel=0.0012))
    parts.append(box('MGBox_Hasp', (c.x + 0.010, mx.y + 0.0016, mx.z - 0.016), (0.016, 0.0028, 0.030), I, coll, bevel=0.0010))
    parts.append(box('MGBox_Staple', (c.x + 0.010, mx.y + 0.0040, mx.z - 0.028), (0.010, 0.006, 0.004), I, coll, bevel=0.0010))
    # dovetail shoe on top (slides onto the gun's rail)
    parts.append(box('MGBox_Shoe', (c.x, c.y, mx.z + 0.0040), (size.x - 0.050, 0.050, 0.008), 'MAT-MG-Blued', coll, bevel=0.0015))
    return parts


# ------------------------------------------------------------------------------------------------ statics

def build_casing(coll):
    """SM_MG_Casing: a fired 7.7 mm case (0.056 m), lying along +X (mouth +X), bottom at z = 0, centred in X and Y;
    struck primer, slightly flared sooty mouth."""
    parts = cartridge(coll, 'MGCasing', segs=20, fired=True)
    r = 0.0068
    centre = -0.5 * (CART_HEAD + 0.0226)
    for ob in parts:
        ob.data.transform(Matrix.Translation((centre, 0.0, r)))
    dent = B().blob('MGCasing_Strike', (CART_HEAD - 0.0004 + centre, 0.0, r), (0.0006, 0.0009, 0.0009), 'MAT-MG-Bore', coll, seg=8, ring=4)
    parts.append(dent)
    return parts


def mesh_part_names(res):
    return {n: [o.name for o in objs] for n, objs in res['parts'].items()}
