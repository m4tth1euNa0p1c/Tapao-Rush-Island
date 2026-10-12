"""Build the pilot character "Bean - British soldier" (sprite/player-1.png) as a rigged, modular .blend.

Run (background):
  blender --background --factory-startup --python scripts/character/bean_soldier.py -- [options]
Options:
  --render     render the turnaround (front / side / back / 3/4) to art/_previews/bean_soldier_*.png
  --export     write FBX files to art/export/Characters/BeanSoldier/
  --no-save    do not write art/blender/characters/bean_soldier.blend

Units: meters, +Z up, the character faces -Y (Blender front view), feet on z = 0.
Body height without hat = P["height"] (1.05 m: a Minion next to a 1.80 m human, ~1/18 of an adult coconut palm).
Modular parts (one shared skeleton "SKEL_Bean"), as the cosmetics system expects:
  SK_Bean_Body          bean body, eyes, mittens (the merchant: visor too)  (skin: body colour)
  SK_Bean_Outfit_Redcoat coat, lapels, collar, cross belts, belt, pouch, breeches, boots
  SM_Bean_Hat_Tricorne  hat, pivot at the head contact point       (socket "hat_socket", own physics body)
Every tunable dimension lives in P; colours in PALETTE (sRGB hex).
"""
import argparse
import math
import re
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "art"
BLEND = ART / "blender" / "characters" / "bean_soldier.blend"
PREV = ART / "_previews"
EXPORT = ART / "export" / "Characters" / "BeanSoldier"

P = {
    "height": 1.05,      # top of the bean (no hat)
    "z_bottom": 0.16,    # bottom of the bean (inside the breeches)
    "rx": 0.200,         # half width
    "ry": 0.180,         # half depth
    "p_top": 2.6,        # superellipse exponent of the profile, top / bottom (higher = flatter sides)
    "p_bot": 3.4,
    "belly": 0.09, "belly_z": 0.40, "belly_h": 0.16, "belly_side": 0.30,   # gourmand: round belly at the belt
    "visor_z": 0.805, "visor_w": 0.118, "visor_h": 0.108,
    "eye_x": 0.052, "eye_z": 0.79, "eye_w": 0.021, "eye_h": 0.047,
    "coat_z": (0.17, 0.712), "belt_z": (0.335, 0.382), "collar_z": (0.665, 0.718),
    "shoulder": (0.20, 0.625), "elbow": (0.268, 0.50), "wrist": (0.292, 0.40),
    "hand": (0.304, 0.325), "hip_x": 0.085, "knee_z": 0.13,
    "hat_z": 0.94, "hat_crown_r": 0.19, "brim_r": 0.33,
    "fp_arm_length": 0.30, "fp_arm_back": 0.62,   # back: the sleeve end stays out of the FP frame
}

PALETTE = {
    "M_Bean_Skin": ("D4B48D", 0.58, 0.0),        # beige like the merchant (owner request 2026-10-06, T-192); was blue 4FA3E0
    "M_Bean_Visor": ("F2E9DA", 0.35, 0.0),
    "M_Bean_Eye": ("141418", 0.12, 0.0),
    "M_Bean_CoatRed": ("C21F2A", 0.80, 0.0),
    "M_Bean_Navy": ("1E2848", 0.75, 0.0),
    "M_Bean_Cream": ("ECE2C8", 0.80, 0.0),
    "M_Bean_Brass": ("C99A3E", 0.30, 1.0),
    "M_Bean_Leather": ("4B2D1D", 0.55, 0.0),
    "M_Bean_Boot": ("2A1B13", 0.40, 0.0),
    "M_Bean_Felt": ("1B1B1E", 0.90, 0.0),
    "M_Bean_HatTrim": ("C8AD78", 0.70, 0.0),
    "M_Bean_SleeveRed": ("9E1622", 0.80, 0.0),
    "M_Bean_Moustache": ("E0AE45", 0.70, 0.0),   # golden blond (owner request 2026-10-07), apart from the beige skin
    "M_Bean_Hair": ("EDBE4F", 0.65, 0.0),        # golden blond hair under the tricorne
}

# English handlebar moustache, the merchant's (scripts/shop/merchant_showcase_model.py) raised just under the eyes
# (owner requests 2026-10-07: "plus haut", then "baisse la moustache un peu": 1.5 cm lower): (x, z, radius) from the
# middle to the tip of one side
MOUSTACHE = [(0.004, 0.745, 0.014), (0.035, 0.731, 0.022), (0.068, 0.733, 0.019), (0.092, 0.750, 0.012),
             (0.108, 0.765, 0.002)]
HAIRLINE_Z = 0.905           # front hairline, just under the brim of the tricorne (visor top 0.859)
HAIR_FACE_HALF_ANGLE = 1.05  # the face (theta around 0) stays bare below the hairline
HAIR_BOTTOM_Z = 0.80         # lower edge of the hair on the sides and at the back

TAU = 2 * math.pi


# ---------------------------------------------------------------- helpers

def srgb(hexa):
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def material(name):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    hexa, rough, metal = PALETTE[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = srgb(hexa)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if name in ("M_Bean_CoatRed", "M_Bean_SleeveRed", "M_Bean_Navy", "M_Bean_Felt"):  # wool / felt sheen
        for key in ("Sheen Weight", "Sheen"):
            if key in bsdf.inputs:
                bsdf.inputs[key].default_value = 0.6
                break
    mat.diffuse_color = srgb(hexa)
    return mat


def collection(name, parent=None):
    coll = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if coll.name not in (parent or bpy.context.scene.collection).children:
        (parent or bpy.context.scene.collection).children.link(coll)
    return coll


def obj_from_bm(name, bm, mat, coll, smooth=True, solidify=0.0, subsurf=0):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(material(mat))
    for poly in me.polygons:
        poly.use_smooth = smooth
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    if solidify:
        mod = ob.modifiers.new("Solidify", "SOLIDIFY")
        mod.thickness = solidify
        mod.offset = -1.0
    if subsurf:
        mod = ob.modifiers.new("Subsurf", "SUBSURF")
        mod.levels = subsurf
        mod.render_levels = subsurf
    return ob


def grid_faces(bm, rows, closed_u, keep=None):
    """rows: list of vertex lists (v-major). keep(i, j) -> bool filters quads."""
    for j in range(len(rows) - 1):
        n = len(rows[j])
        for i in range(n if closed_u else n - 1):
            i2 = (i + 1) % n
            if keep and not keep(i, j):
                continue
            bm.faces.new((rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]))


# ---------------------------------------------------------------- bean surface

def profile(z):
    """Radius factor of the bean at height z (superellipse, flatter on the sides than a capsule)."""
    zb, zt = P["z_bottom"], P["height"]
    zc, h = (zb + zt) / 2, (zt - zb) / 2
    t = max(-1.0, min(1.0, (z - zc) / h))
    p = P["p_top"] if t > 0 else P["p_bot"]
    return max(0.0, 1 - abs(t) ** p) ** (1 / p)


def belly(z):
    """Forward bulge of the round, well-fed belly at height z (m)."""
    return P["belly"] * math.exp(-((z - P["belly_z"]) / P["belly_h"]) ** 2)


def radii(theta, z):
    """Half width and half depth of the bean at height z, the belly bulging toward the front (theta = 0)."""
    f = profile(z)
    front = max(0.0, math.cos(theta)) ** 1.5
    b = belly(z)
    return P["rx"] * f + b * P["belly_side"], P["ry"] * f + b * front


def surf(theta, z, off=0.0):
    """Point on the bean surface (theta = 0 faces -Y, positive towards +X), pushed out by off."""
    rx, ry = radii(theta, z)
    return Vector(((rx + off) * math.sin(theta), -(ry + off) * math.cos(theta), z))


def surf_xz(x, z, off=0.0, back=False):
    """Surface point found from its x on the front (or back) of the bean."""
    rx = radii(math.pi if back else 0.0, z)[0]
    theta = math.asin(max(-1.0, min(1.0, x / max(rx, 1e-4))))
    return surf(math.pi - theta if back else theta, z, off)


def shell(name, z0, z1, off, mat, coll, keep=None, nu=96, dz=0.008, solidify=0.006, flare=None):
    """Band of the bean surface between z0 and z1, pushed out by off (flare(z) adds to off)."""
    bm = bmesh.new()
    nz = max(2, int(round((z1 - z0) / dz)))
    rows = []
    for j in range(nz + 1):
        z = z0 + (z1 - z0) * j / nz
        o = off + (flare(z) if flare else 0.0)
        rows.append([bm.verts.new(surf(TAU * i / nu, z, o)) for i in range(nu)])
    k = None
    if keep:
        def k(i, j):
            th = TAU * (i + 0.5) / nu
            z = z0 + (z1 - z0) * (j + 0.5) / nz
            return keep(math.atan2(math.sin(th), math.cos(th)), z)
    grid_faces(bm, rows, True, k)
    bm.normal_update()
    return obj_from_bm(name, bm, mat, coll, solidify=solidify)


def ellipse_patch(name, cx, cz, w, h, off, mat, coll, dome=0.0, back=False, solidify=0.0, nr=10, na=48):
    """Elliptic patch lying on the bean (visor, eyes); dome bulges its centre outwards."""
    bm = bmesh.new()
    rows = []
    for j in range(nr + 1):
        u = j / nr
        if j == 0:
            rows.append([bm.verts.new(surf_xz(cx, cz, off + dome, back))] * na)
            continue
        ring = []
        for i in range(na):
            a = TAU * i / na
            ring.append(bm.verts.new(surf_xz(cx + w * u * math.cos(a), cz + h * u * math.sin(a),
                                             off + dome * (1 - u * u), back)))
        rows.append(ring)
    for i in range(na):  # centre fan
        i2 = (i + 1) % na
        bm.faces.new((rows[0][0], rows[1][i], rows[1][i2])) if not back else \
            bm.faces.new((rows[0][0], rows[1][i2], rows[1][i]))
    for j in range(1, nr):
        for i in range(na):
            i2 = (i + 1) % na
            quad = (rows[j][i], rows[j + 1][i], rows[j + 1][i2], rows[j][i2])
            bm.faces.new(quad if not back else quad[::-1])
    bm.normal_update()
    return obj_from_bm(name, bm, mat, coll, solidify=solidify)


def ribbon(name, pts_tz, width, off, mat, coll, back=False, n=48, solidify=0.004):
    """Strap lying on the bean, following a polyline in (theta, z); off may be a function of z."""
    off_at = off if callable(off) else (lambda z: off)
    r_avg = (P["rx"] + P["ry"]) / 2
    samples = []
    for k in range(n + 1):
        s = k / n * (len(pts_tz) - 1)
        a = min(int(s), len(pts_tz) - 2)
        t = s - a
        th = pts_tz[a][0] * (1 - t) + pts_tz[a + 1][0] * t
        z = pts_tz[a][1] * (1 - t) + pts_tz[a + 1][1] * t
        samples.append((th, z))
    bm = bmesh.new()
    rows = []
    for k, (th, z) in enumerate(samples):
        a0, z0 = samples[max(0, k - 1)]
        a1, z1 = samples[min(n, k + 1)]
        d = Vector(((a1 - a0) * r_avg, z1 - z0)).normalized()
        perp = Vector((-d.y, d.x)) * (width / 2)
        row = []
        for sgn in (-1, 1):
            t2 = th + sgn * perp.x / r_avg
            z2 = z + sgn * perp.y
            row.append(bm.verts.new(surf(math.pi - t2 if back else t2, z2, off_at(z2))))
        rows.append(row)
    for k in range(n):
        quad = (rows[k][0], rows[k][1], rows[k + 1][1], rows[k + 1][0])
        bm.faces.new(quad if not back else quad[::-1])
    bm.normal_update()
    return obj_from_bm(name, bm, mat, coll, solidify=solidify)


def tube(name, pts, radii, mat, coll, segs=20, caps=True, squash=1.0):
    """Tube through pts with per-point radius; squash < 1 flattens it front-to-back."""
    bm = bmesh.new()
    rows = []
    for k, p in enumerate(pts):
        d = (pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]).normalized()
        side = d.cross(Vector((0, 1, 0)))
        if side.length < 1e-4:
            side = d.cross(Vector((1, 0, 0)))
        side.normalize()
        up = side.cross(d).normalized()
        ring = []
        for i in range(segs):
            a = TAU * i / segs
            ring.append(bm.verts.new(p + radii[k] * (math.cos(a) * side + squash * math.sin(a) * up)))
        rows.append(ring)
    grid_faces(bm, rows, True)
    if caps:
        for ring, rev in ((rows[0], True), (rows[-1], False)):
            c = bm.verts.new(sum((v.co for v in ring), Vector()) / len(ring))
            for i in range(segs):
                tri = (c, ring[i], ring[(i + 1) % segs])
                bm.faces.new(tri[::-1] if rev else tri)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, mat, coll, subsurf=1)


def blob(name, center, radii, mat, coll, cut_below=None, seg=32, ring=16):
    """Ellipsoid; cut_below removes the part under that world z (flat soles)."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=ring, radius=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * radii[0], v.co.y * radii[1], v.co.z * radii[2])) + Vector(center)
    if cut_below is not None:
        for v in bm.verts:
            v.co.z = max(v.co.z, cut_below)
    bm.normal_update()
    return obj_from_bm(name, bm, mat, coll)


def box(name, center, size, mat, coll, bevel=0.004):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(center)
    ob = obj_from_bm(name, bm, mat, coll, smooth=False)
    if bevel:
        mod = ob.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
    return ob


def button(name, pos, normal, mat, coll, r=0.011):
    ob = blob(name, (0, 0, 0), (r, r, r * 0.45), mat, coll, seg=16, ring=8)
    ob.location = pos
    ob.rotation_mode = "QUATERNION"
    ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(normal.normalized())
    return ob


def outward(theta, z):
    return Vector((math.sin(theta) / P["rx"], -math.cos(theta) / P["ry"], 0)).normalized()


# ---------------------------------------------------------------- parts

def build_hair(coll):
    """Blond hair of the player bean (owner request 2026-10-07): a cap over the top, the sides and the back of the head,
    open on the face below the hairline, a wavy lower edge of locks, and an 18th century queue tied with a dark
    ribbon at the back. Mostly under the tricorne; it shows when the hat falls."""
    parts = []

    def keep(th, z):
        return not (abs(th) < HAIR_FACE_HALF_ANGLE and z < HAIRLINE_Z)
    parts.append(shell("Bean_Hair_Cap", HAIR_BOTTOM_Z, P["height"] - 0.004, 0.009, "M_Bean_Hair", coll, keep=keep,
                       dz=0.008, solidify=0.008, flare=lambda z: 0.008 * max(0.0, (0.88 - z) / 0.08)))
    n = 14
    for k in range(n):                    # locks along the lower edge, sides and back
        th = HAIR_FACE_HALF_ANGLE + (TAU - 2 * HAIR_FACE_HALF_ANGLE) * (k + 0.5) / n
        parts.append(blob(f"Bean_Hair_Lock_{k:02d}", surf(th, HAIR_BOTTOM_Z + 0.008, 0.014), (0.028, 0.026, 0.034),
                          "M_Bean_Hair", coll, seg=12, ring=8))
    queue = [surf(math.pi, 0.905, 0.02), surf(math.pi, 0.85, 0.034), surf(math.pi, 0.79, 0.044),
             surf(math.pi, 0.735, 0.05)]
    parts.append(tube("Bean_Hair_Queue", queue, [0.03, 0.026, 0.019, 0.006], "M_Bean_Hair", coll, segs=12))
    knot = surf(math.pi, 0.875, 0.04)
    for s in (-1, 1):                     # ribbon bow on the queue
        parts.append(blob(f"Bean_Hair_Ribbon_{'l' if s > 0 else 'r'}", knot + Vector((s * 0.035, 0.0, 0.0)),
                          (0.032, 0.008, 0.02), "M_Bean_Navy", coll, seg=12, ring=8))
    parts.append(blob("Bean_Hair_Ribbon_Knot", knot, (0.014, 0.012, 0.014), "M_Bean_Navy", coll, seg=12, ring=8))
    return parts


def build_body(coll, moustache=False, hair=False, visor=True):
    """visor: the white face plate around the eyes; the player bean has none (owner request 2026-10-07, "enlève le
    rond blanc"): its eyes and moustache sit on the beige skin."""
    parts = []
    # bean: revolve the profile
    bm = bmesh.new()
    nu, nz = 64, 72
    zb, zt = P["z_bottom"], P["height"]
    rows = [[bm.verts.new((0, 0, zb))] * nu]
    for j in range(1, nz):
        t = j / nz
        z = zb + (zt - zb) * (0.5 - 0.5 * math.cos(math.pi * t))  # denser near the poles
        rows.append([bm.verts.new(surf(TAU * i / nu, z)) for i in range(nu)])
    rows.append([bm.verts.new((0, 0, zt))] * nu)
    for j in range(len(rows) - 1):
        for i in range(nu):
            i2 = (i + 1) % nu
            a, b, c, d = rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]
            verts = [v for k, v in enumerate((a, b, c, d)) if v not in (a, b, c, d)[:k]]
            if len(verts) >= 3:
                bm.faces.new(verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append(obj_from_bm("Bean_Body", bm, "M_Bean_Skin", coll))
    if visor:
        parts.append(ellipse_patch("Bean_Visor", 0, P["visor_z"], P["visor_w"], P["visor_h"], 0.002, "M_Bean_Visor",
                                   coll, dome=0.004, solidify=0.004))
    face = 0.006 if visor else 0.0        # height of the face plate under the eyes and the moustache
    for s in (-1, 1):
        parts.append(ellipse_patch(f"Bean_Eye_{'l' if s > 0 else 'r'}", s * P["eye_x"], P["eye_z"], P["eye_w"],
                                   P["eye_h"], 0.001 + face, "M_Bean_Eye", coll, dome=0.006, nr=6, na=32))
        hx, hz = P["hand"]
        wx, wz = P["wrist"]
        parts.append(blob(f"Bean_Hand_{'l' if s > 0 else 'r'}", (s * hx, -0.012, hz), (0.050, 0.046, 0.064),
                          "M_Bean_Skin", coll))
        parts.append(blob(f"Bean_Thumb_{'l' if s > 0 else 'r'}", (s * (hx - 0.030), -0.040, hz + 0.022),
                          (0.022, 0.022, 0.030), "M_Bean_Skin", coll, seg=16, ring=10))
        if moustache:     # the player bean (T-192); the merchant builds its own
            pts = [surf_xz(s * x, z, 0.004 + face + r * 0.45) for x, z, r in MOUSTACHE]   # above the face
            parts.append(tube(f"Bean_Moustache_{'l' if s > 0 else 'r'}", pts, [r for _, _, r in MOUSTACHE],
                              "M_Bean_Moustache", coll, segs=12))
    if hair:
        parts += build_hair(coll)
    return parts


def rigid_on_head(objs):
    """The moustache and the hair follow the head only (as the merchant's moustache): one vertex group, weight 1"""
    for ob in objs:
        if ob.name.startswith(("Bean_Moustache", "Bean_Hair")):
            ob.vertex_groups.clear()
            ob.vertex_groups.new(name="head").add(list(range(len(ob.data.vertices))), 1.0, "REPLACE")


def build_outfit(coat_coll):
    parts = []
    z0, z1 = P["coat_z"]
    bz0, bz1 = P["belt_z"]

    def coat_keep(th, z):
        if abs(th) < 0.17 and z > bz0 - 0.01:            # cream waistcoat shows between the lapels
            return False
        if abs(th) < 0.30 and z < bz0:                   # coat open under the belt
            return False
        if abs(abs(th) - math.pi) < 0.045 and z < bz0:   # back vent between the tails
            return False
        return True

    flare = lambda z: max(0.0, bz0 - z) * 0.28 + 0.004 * max(0.0, z - 0.6) / 0.1
    parts.append(shell("Coat", z0, z1, 0.022, "M_Bean_CoatRed", coat_coll, keep=coat_keep, flare=flare,
                       solidify=0.010))
    parts.append(shell("Waistcoat", z0, 0.69, 0.012, "M_Bean_Cream", coat_coll,
                       keep=lambda th, z: abs(th) < 0.45 or (abs(th) > math.pi - 0.2 and z < bz0),
                       flare=lambda z: max(0.0, bz0 - z) * 0.12, solidify=0.004))
    # turnbacks: cream lining on the coat front edges under the belt
    for s in (-1, 1):
        parts.append(ribbon(f"Turnback_{'l' if s > 0 else 'r'}", [(s * 0.33, bz0 - 0.005), (s * 0.36, z0 + 0.012)],
                            0.034, lambda z: 0.022 + flare(z) + 0.004, "M_Bean_Cream", coat_coll, n=16))
        parts.append(ribbon(f"Lapel_{'l' if s > 0 else 'r'}", [(s * 0.215, bz1 + 0.004), (s * 0.235, 0.665)], 0.062,
                            0.030, "M_Bean_Navy", coat_coll, n=20))
        for k, z in enumerate((0.47, 0.52, 0.57, 0.62)):
            th = s * 0.225
            pos = surf(th, z, 0.037)
            parts.append(button(f"Btn_Lapel_{'l' if s > 0 else 'r'}{k}", pos, outward(th, z), "M_Bean_Brass",
                                coat_coll))
        for k, z in enumerate((0.33, 0.27)):           # tail buttons in the back
            th = math.pi - s * 0.12
            pos = surf(th, z, 0.022 + flare(z) + 0.012)
            parts.append(button(f"Btn_Tail_{'l' if s > 0 else 'r'}{k}", pos, outward(th, z), "M_Bean_Brass",
                                coat_coll))
    cz0, cz1 = P["collar_z"]
    parts.append(shell("Collar", cz0, cz1, 0.034, "M_Bean_Navy", coat_coll, solidify=0.012))
    # cross belts (X on the chest and on the back)
    for back in (False, True):
        for s in (-1, 1):
            tag = f"{'B' if back else 'F'}{'l' if s > 0 else 'r'}"
            parts.append(ribbon(f"CrossBelt_{tag}", [(s * 0.95, 0.70), (-s * 0.62, bz1 - 0.005)], 0.040, 0.040,
                                "M_Bean_Cream", coat_coll, back=back, solidify=0.005))
    parts.append(shell("Belt", bz0, bz1, 0.048, "M_Bean_Leather", coat_coll, solidify=0.008))
    # the belt's outer face is at offset 0.048: the buckle sinks halfway into the leather instead of floating
    bc = surf(0, (bz0 + bz1) / 2, 0.050)
    parts.append(box("Buckle", bc, (0.056, 0.012, 0.050), "M_Bean_Brass", coat_coll))
    parts.append(box("Buckle_Inner", bc + Vector((0, -0.003, 0)), (0.034, 0.010, 0.028), "M_Bean_Leather",
                     coat_coll, bevel=0.002))
    # cartridge pouch on the back right hip
    th = math.pi - 0.55
    pc = surf(th, 0.315, 0.085)
    pouch = box("Pouch", (0, 0, 0), (0.10, 0.05, 0.085), "M_Bean_Leather", coat_coll, bevel=0.008)
    pouch.location = pc
    pouch.rotation_euler = (0, 0, -th)
    flap = box("Pouch_Flap", (0, -0.0, 0.02), (0.104, 0.054, 0.05), "M_Bean_Leather", coat_coll, bevel=0.006)
    flap.parent = pouch
    parts += [pouch, flap, button("Btn_Pouch", pc + outward(th, 0.3) * 0.028 + Vector((0, 0, 0.005)),
                                  outward(th, 0.3), "M_Bean_Brass", coat_coll, r=0.010)]
    # arms: sleeves, navy cuffs with two buttons
    for s in (-1, 1):
        side = "l" if s > 0 else "r"
        sh = Vector((s * P["shoulder"][0], 0.0, P["shoulder"][1]))
        el = Vector((s * P["elbow"][0], 0.0, P["elbow"][1]))
        wr = Vector((s * P["wrist"][0], -0.01, P["wrist"][1]))
        parts.append(tube(f"Sleeve_{side}", [sh - Vector((s * 0.05, 0, 0)), sh, el, wr], [0.078, 0.078, 0.068, 0.063],
                          "M_Bean_SleeveRed", coat_coll))
        parts += build_epaulette(side, s, coat_coll)
        d = (wr - el).normalized()
        c0 = wr - d * 0.065
        parts.append(tube(f"Cuff_{side}", [c0, wr + d * 0.005], [0.072, 0.072], "M_Bean_Navy", coat_coll))
        for k in range(2):
            p = c0 + d * (0.02 + 0.03 * k) + Vector((s * 0.073, 0, 0))
            parts.append(button(f"Btn_Cuff_{side}{k}", p, Vector((s, 0, 0)), "M_Bean_Brass", coat_coll, r=0.009))
        # legs: breeches and boots
        hx = s * P["hip_x"]
        parts.append(tube(f"Breeches_{side}", [Vector((hx, 0, 0.27)), Vector((hx, 0, 0.18)),
                                               Vector((hx, -0.005, 0.11))], [0.085, 0.082, 0.072], "M_Bean_Cream",
                          coat_coll))
        parts.append(tube(f"BootShaft_{side}", [Vector((hx, 0, 0.03)), Vector((hx, 0, 0.12)),
                                                Vector((hx, 0, 0.145))], [0.068, 0.072, 0.082], "M_Bean_Boot",
                          coat_coll))
        parts.append(blob(f"BootToe_{side}", (hx, -0.055, 0.042), (0.068, 0.105, 0.045), "M_Bean_Boot", coat_coll,
                          cut_below=0.0))
        parts.append(box(f"BootBuckle_{side}", (hx, -0.115, 0.07), (0.035, 0.012, 0.022), "M_Bean_Brass",
                         coat_coll, bevel=0.002))
    return parts


def build_hat(coll):
    parts = []
    hz, cr, R0 = P["hat_z"], P["hat_crown_r"], P["brim_r"]
    front = -math.pi / 2  # brim corner pointing to -Y

    def brim_r(phi):
        return R0 * (1 + 0.10 * math.cos(3 * (phi - front)))

    def lift(phi):  # sides turned up, the three corners lower
        return 0.30 + 0.70 * 0.5 * (1 - math.cos(3 * (phi - front)))

    def brim_pt(r_frac, phi):
        r = cr * 0.92 + (brim_r(phi) - cr * 0.92) * r_frac
        z = hz + 0.005 + (r_frac ** 1.6) * 0.15 * lift(phi)
        rr = r - (r_frac ** 2) * 0.05 * lift(phi)  # turned-up flaps lean in
        return Vector((rr * math.cos(phi), rr * math.sin(phi), z))

    bm = bmesh.new()
    na, nr = 120, 14
    rows = [[bm.verts.new(brim_pt(j / nr, TAU * i / na)) for i in range(na)] for j in range(nr + 1)]
    grid_faces(bm, rows, True)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    parts.append(obj_from_bm("Hat_Brim", bm, "M_Bean_Felt", coll, solidify=0.010))
    # crown: squashed dome covering the top of the head
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * cr, v.co.y * cr * 0.95, max(v.co.z, 0.0) * cr * 0.85 + hz))
    bm.normal_update()
    parts.append(obj_from_bm("Hat_Crown", bm, "M_Bean_Felt", coll))
    # gold trim along the brim edge
    edge = [brim_pt(1.0, TAU * i / na) for i in range(na + 1)]
    trim = tube("Hat_Trim", edge, [0.0085] * len(edge), "M_Bean_HatTrim", coll, segs=10, caps=False)
    trim.modifiers.clear()
    parts.append(trim)
    # cockade on the left turned-up side
    phi = front + math.pi / 3
    pos = brim_pt(0.80, phi)
    nrm = Vector((math.cos(phi), math.sin(phi), 0.35)).normalized()
    ck = blob("Hat_Cockade", (0, 0, 0), (0.042, 0.042, 0.010), "M_Bean_Navy", coll, seg=24, ring=8)
    ck.location = pos + nrm * 0.012
    ck.rotation_mode = "QUATERNION"
    ck.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(nrm)
    parts += [ck, button("Hat_Cockade_Button", pos + nrm * 0.024, nrm, "M_Bean_Brass", coll, r=0.014)]
    return parts


def build_epaulette(side, s, coll):
    """Navy shoulder board with a gold fringe: marks where the arm starts (redcoat epaulette)."""
    sx, sz = P["shoulder"]
    c = Vector((s * (sx + 0.012), 0.0, sz + 0.058))
    pad = blob(f"Epaulette_{side}", (0, 0, 0), (0.088, 0.072, 0.024), "M_Bean_Navy", coll, seg=32, ring=12)
    pad.location = c
    pad.rotation_euler = (0.0, s * math.radians(30), 0.0)
    parts = [pad]
    # fringe: short gold strands hanging from the outer edge
    for k in range(9):
        a = math.radians(-70 + 140 * k / 8)
        root = c + Vector((s * (0.06 + 0.012 * math.cos(a)), 0.066 * math.sin(a), -0.035))
        strand = tube(f"EpauletteFringe_{side}{k}", [root, root + Vector((s * 0.018, 0, -0.06))], [0.0085, 0.0075],
                      "M_Bean_Brass", coll, segs=8)
        strand.modifiers.clear()
        parts.append(strand)
    return parts


def box_uvs(objs, size=0.20):
    """Box-projected UVs at real scale (1 UV unit = size metres) so the fabric detail has the same grain
    everywhere; the procedural meshes have no UVs of their own."""
    for ob in objs:
        if ob.type != "MESH":
            continue
        me = ob.data
        bm = bmesh.new()
        bm.from_mesh(me)
        uv = bm.loops.layers.uv.verify()
        mw = ob.matrix_world
        for f in bm.faces:
            n = (mw.to_3x3() @ f.normal)
            ax = max(range(3), key=lambda i: abs(n[i]))
            for loop in f.loops:
                p = mw @ loop.vert.co
                u, v = ((p.y, p.z), (p.x, p.z), (p.x, p.y))[ax]
                loop[uv].uv = (u / size, v / size)
        bm.to_mesh(me)
        bm.free()


def smooth_interp(x, keys):
    """Value at x of the (x, value) keys, smoothstep between two keys, held outside."""
    if x <= keys[0][0]:
        return keys[0][1]
    for (a, va), (b, vb) in zip(keys, keys[1:]):
        if x <= b:
            t = (x - a) / max(b - a, 1e-9)
            return va + (vb - va) * t * t * (3 - 2 * t)
    return keys[-1][1]


def lathe_x(name, profile, mat, coll, axis_z, segs=40, cap_start=False, cap_end=False):
    """Surface of revolution around the first person arm axis (+X, centre (x, 0, axis_z(x))): profile = [(x, r)] from
    the shoulder side to the hand side, it may fold back (a turned-back cuff: outer wall, lip, inner wall). No
    subdivision: the radii are the real ones, so buttons and mitten sit on them (a Subsurf on few rings shrank the old
    cuff into a pill and left its buttons and the mitten floating, owner report 2026-10-07). Rings counter-clockwise
    around +X give outward normals on the forward walls and inward ones on the folded-back walls, as seen."""
    bm = bmesh.new()
    rows = []
    for x, r in profile:
        cz = axis_z(x)
        rows.append([bm.verts.new((x, r * math.cos(TAU * i / segs), cz + r * math.sin(TAU * i / segs)))
                     for i in range(segs)])
    for a, b in zip(rows, rows[1:]):
        for i in range(segs):
            j = (i + 1) % segs
            bm.faces.new((a[i], a[j], b[j], b[i]))
    # the first cap faces the shoulder (-X), the last one the hand (+X), also at the bottom of a folded cuff
    for ring, cap, rev in ((rows[0], cap_start, True), (rows[-1], cap_end, False)):
        if cap:
            c = bm.verts.new(sum((v.co for v in ring), Vector()) / segs)
            for i in range(segs):
                tri = (c, ring[i], ring[(i + 1) % segs])
                bm.faces.new(tri[::-1] if rev else tri)
    bm.normal_update()
    return obj_from_bm(name, bm, mat, coll)


def surface_box(name, x, angle, axis_z, radius, size, mat, coll):
    """Thin box lying on the arm's round surface at x and at angle around +X (0 = +Y, pi/2 = top), its size along
    (X, around, out); its inner face sinks 0.5 mm under the surface so it never floats."""
    ob = box(name, (0, 0, 0), size, mat, coll, bevel=min(size) * 0.35)
    normal = Vector((0.0, math.cos(angle), math.sin(angle)))
    ob.location = Vector((x, 0.0, axis_z(x))) + normal * (radius + size[2] * 0.5 - 0.0005)
    ob.rotation_euler = (angle - math.pi / 2, 0.0, 0.0)
    return ob


def build_fp_arm(coll):
    """Right arm for the first person view (static mesh): pivot at the shoulder, pointing along +X (UE forward),
    mitten thumb up. The left arm is the same mesh mirrored in Unreal (and by the weapon packs).

    The hand centre stays at (L + 0.06, 0, -0.015): the musket pack (scripts/props/musket_pack) puts that point on the
    weapon grips. Every part overlaps the next one (sleeve in the cuff, wrist and mitten in the cuff opening) so nothing
    floats whatever the angle; buttons are half sunk in the cuff."""
    parts = []
    L = P["fp_arm_length"]
    # the sleeve runs on far behind the shoulder pivot so its end always stays behind the camera, out of view:
    # in first person the arm then seems to come from the body instead of floating cut off
    back = P["fp_arm_back"]
    hand = Vector((L + 0.06, 0, -0.015))

    def axis_z(x):
        return smooth_interp(x, [(-back, -0.06), (-back * 0.5, -0.025), (0.0, 0.0), (L * 0.55, -0.01), (L, -0.015)])

    xs = [-back, -back * 0.85, -back * 0.6, -back * 0.35, -back * 0.12, 0.0, 0.08, 0.15, 0.20, L - 0.045, L - 0.03]
    sleeve = [(x, smooth_interp(x, [(-back * 0.5, 0.074), (0.0, 0.072), (L * 0.55, 0.067), (L - 0.03, 0.064)]))
              for x in xs]
    parts.append(lathe_x("FPArm_Sleeve", sleeve, "M_Bean_SleeveRed", coll, axis_z, cap_start=True, cap_end=True))

    # turned-back navy cuff: tucked on the sleeve, slight flare, rounded lip, inner wall and a closed bottom
    x0, x1 = L - 0.075, L + 0.010
    cuff = [(x0, 0.0625), (x0 + 0.002, 0.0700), (x0 + 0.006, 0.0752), (x0 + 0.013, 0.0768), (L - 0.030, 0.0776),
            (x1 - 0.007, 0.0786), (x1 - 0.0025, 0.0772), (x1, 0.0738), (x1 + 0.0008, 0.0660), (x1, 0.0560),
            (x1 - 0.003, 0.0512), (x1 - 0.008, 0.0500), (L - 0.025, 0.0496)]
    parts.append(lathe_x("FPArm_Cuff", cuff, "M_Bean_Navy", coll, axis_z, cap_end=True))

    def cuff_radius(x):
        """Radius of the cuff's outer wall at x, as meshed (linear between rings)."""
        wall = cuff[2:6]
        for (a, ra), (b, rb) in zip(wall, wall[1:]):
            if a <= x <= b:
                return ra + (rb - ra) * (x - a) / (b - a)
        return wall[0][1] if x < wall[0][0] else wall[-1][1]

    # three brass buttons on the top of the cuff, each on a cream lace bar (British regimental cuff), half sunk
    for k, x in enumerate((L - 0.054, L - 0.031, L - 0.008)):
        r = cuff_radius(x)
        parts.append(surface_box(f"FPArm_Lace{k}", x, math.pi / 2, axis_z, r, (0.0085, 0.030, 0.0022), "M_Bean_Cream",
                                 coll))
        top = Vector((x, 0.0, axis_z(x) + r + 0.0022))
        parts.append(button(f"FPArm_Btn{k}", top, Vector((0, 0, 1)), "M_Bean_Brass", coll, r=0.0078))

    # skin wrist from inside the cuff into the mitten, then the mitten and its thumb (dense, no facets up close)
    parts.append(lathe_x("FPArm_Wrist", [(L - 0.035, 0.040), (L - 0.010, 0.042), (L + 0.020, 0.043), (L + 0.040, 0.041)],
                         "M_Bean_Skin", coll, lambda x: hand.z, segs=32, cap_start=True, cap_end=True))
    parts.append(blob("FPArm_Hand", hand, (0.062, 0.050, 0.046), "M_Bean_Skin", coll, seg=48, ring=24))
    parts.append(blob("FPArm_Thumb", hand + Vector((-0.010, 0.030, 0.031)), (0.030, 0.022, 0.022), "M_Bean_Skin", coll,
                      seg=32, ring=16))
    return parts


# ---------------------------------------------------------------- skeleton and weights

def bone_table():
    sx, sz = P["shoulder"]
    ex, ez = P["elbow"]
    wx, wz = P["wrist"]
    hx, hz = P["hand"]
    ix, kz = P["hip_x"], P["knee_z"]
    B = [("root", (0, 0, 0), (0, 0, 0.10), None),
         ("pelvis", (0, 0, 0.22), (0, 0, 0.36), "root"),
         ("spine_01", (0, 0, 0.36), (0, 0, 0.52), "pelvis"),
         ("spine_02", (0, 0, 0.52), (0, 0, 0.70), "spine_01"),
         ("head", (0, 0, 0.70), (0, 0, P["height"]), "spine_02"),
         ("hat", (0, 0, P["hat_z"]), (0, 0, P["hat_z"] + 0.16), "head")]
    for s, side in ((1, "l"), (-1, "r")):
        B += [(f"upperarm_{side}", (s * sx, 0, sz), (s * ex, 0, ez), "spine_02"),
              (f"lowerarm_{side}", (s * ex, 0, ez), (s * wx, -0.01, wz), f"upperarm_{side}"),
              (f"hand_{side}", (s * wx, -0.01, wz), (s * hx, -0.012, hz - 0.05), f"lowerarm_{side}"),
              (f"thigh_{side}", (s * ix, 0, 0.25), (s * ix, -0.005, kz), "pelvis"),
              (f"calf_{side}", (s * ix, -0.005, kz), (s * ix, 0, 0.045), f"thigh_{side}"),
              (f"foot_{side}", (s * ix, 0, 0.045), (s * ix, -0.13, 0.03), f"calf_{side}"),
              (f"coattail_{side}", (s * 0.07, 0.19, P["belt_z"][0]), (s * 0.08, 0.23, P["coat_z"][0]), "pelvis")]
    return B


def build_armature(coll):
    arm_data = bpy.data.armatures.new("SKEL_Bean")
    arm = bpy.data.objects.new("SKEL_Bean", arm_data)
    coll.objects.link(arm)
    arm.show_in_front = True
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, head, tail, parent in bone_table():
        eb = arm_data.edit_bones.new(name)
        eb.head, eb.tail = head, tail
        if parent:
            eb.parent = arm_data.edit_bones[parent]
            eb.use_connect = False
        eb.use_deform = name != "root"
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (p - (a + ab * t)).length


def allowed_bones(ob_name):
    n = ob_name.lower()
    m = re.search(r"_(l|r)\d*$", n)
    side = m.group(1) if m else None
    if n.startswith("hat"):
        return ["hat"]
    if any(k in n for k in ("sleeve", "cuff", "hand", "thumb", "epaulette")) and side:
        return ["spine_02", f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"]
    if any(k in n for k in ("breeches", "boot")) and side:
        return ["pelvis", f"thigh_{side}", f"calf_{side}", f"foot_{side}"]
    if n.startswith(("coat", "btn_tail", "turnback")):
        return ["pelvis", "spine_01", "spine_02", "head", "coattail_l", "coattail_r"]
    return ["pelvis", "spine_01", "spine_02", "head"]


def weight_and_bind(objs, arm, power=4.0, keep=3):
    """Distance-to-bone weights per part (deterministic, no bone-heat failures on layered clothes)."""
    bones = {b.name: (arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones}
    for ob in objs:
        if ob.type != "MESH":
            continue
        names = allowed_bones(ob.name)
        groups = {n: ob.vertex_groups.new(name=n) for n in names}
        mw = ob.matrix_world
        for v in ob.data.vertices:
            p = mw @ v.co
            w = sorted(((1.0 / (seg_dist(p, *bones[n]) + 0.015) ** power, n) for n in names), reverse=True)[:keep]
            tot = sum(x for x, _ in w)
            for x, n in w:
                groups[n].add([v.index], x / tot, "REPLACE")
        mod = ob.modifiers.new("Armature", "ARMATURE")
        mod.object = arm
        ob.parent = arm
        ob.matrix_parent_inverse = arm.matrix_world.inverted()


# ---------------------------------------------------------------- scene, preview, export

def bake_transforms(objs):
    """Apply object transforms (rotated buttons, pouch) so every part sits in armature space."""
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.select_all(action="DESELECT")


def reference_figures(coll):
    """1.80 m human silhouette and a 1:20 palm marker, for scale review only (not exported)."""
    hum = blob("REF_Human_180cm", (0.8, 0.3, 0.90), (0.20, 0.13, 0.90), "M_Bean_Cream", coll)
    hum.display_type = "WIRE"
    hum.hide_render = True


def setup_render(scene):
    scene.render.resolution_x, scene.render.resolution_y = 900, 1100
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "BLENDER_WORKBENCH"):
        try:
            scene.render.engine = eng
            break
        except TypeError:
            continue
    world = bpy.data.worlds.new("W_Studio")
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs[0].default_value = (0.92, 0.93, 0.95, 1)
    bg.inputs[1].default_value = 0.9
    scene.world = world
    for name, rot, energy in (("Key", (0.9, 0.0, -0.6), 3.5), ("Fill", (1.1, 0.0, 2.4), 1.2),
                              ("Rim", (1.3, 0.0, 3.6), 2.0)):
        ld = bpy.data.lights.new(name, "SUN")
        ld.energy = energy
        lo = bpy.data.objects.new(name, ld)
        lo.rotation_euler = rot
        scene.collection.objects.link(lo)
    cam_d = bpy.data.cameras.new("CAM_Turn")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = 1.55
    cam = bpy.data.objects.new("CAM_Turn", cam_d)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam


def render_turnaround(scene, cam):
    PREV.mkdir(parents=True, exist_ok=True)
    out = []
    for tag, yaw in (("face", 0), ("profil", 90), ("dos", 180), ("trois_quarts", 35)):
        a = math.radians(yaw)
        d = 4.0
        cam.location = (d * math.sin(a), -d * math.cos(a), 0.62)
        cam.rotation_euler = (math.radians(90), 0, a)
        path = PREV / f"bean_soldier_{tag}.png"
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        out.append(path)
    print("[bean] renders:", ", ".join(str(p) for p in out))


def export_fbx(objs, arm, path, with_armature=True):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    types = {"MESH"}
    if with_armature:
        arm.select_set(True)
        types.add("ARMATURE")
    path.parent.mkdir(parents=True, exist_ok=True)
    arm_name = arm.name
    arm.name = "Armature"  # Unreal drops an armature node named "Armature"; any other name becomes an extra root bone
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types=types,
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
                             use_armature_deform_only=True, bake_anim=False)
    arm.name = arm_name
    print("[bean] export:", path)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    a = ap.parse_args(argv)

    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob)
    scene = bpy.context.scene
    scene.name = "BeanSoldier"
    scene.unit_settings.system = "METRIC"
    root = collection("BeanSoldier")
    c_body = collection("SK_Bean_Body", root)
    c_outfit = collection("SK_Bean_Outfit_Redcoat", root)
    c_hat = collection("SM_Bean_Hat_Tricorne", root)
    c_rig = collection("Rig", root)
    c_ref = collection("Reference", root)

    body = build_body(c_body, moustache=True, hair=True, visor=False)
    outfit = build_outfit(c_outfit)
    hat = build_hat(c_hat)
    bake_transforms(body + outfit + hat)
    box_uvs(body + outfit + hat)
    arm = build_armature(c_rig)
    weight_and_bind(body + outfit + hat, arm)  # hat: 100 % on bone "hat" (exported apart as a static mesh)
    rigid_on_head(body)
    reference_figures(c_ref)

    zmax = max((ob.matrix_world @ Vector(c)).z for ob in hat for c in ob.bound_box)
    print(f"[bean] body {P['height']:.2f} m, with hat {zmax:.2f} m, parts body={len(body)} outfit={len(outfit)} "
          f"hat={len(hat)}")

    if a.render:
        cam = setup_render(scene)
        render_turnaround(scene, cam)
    if a.export:
        export_fbx(body, arm, EXPORT / "SK_Bean_Body.fbx")
        export_fbx(outfit, arm, EXPORT / "SK_Bean_Outfit_Redcoat.fbx")
        # hat: static mesh, pivot at the head contact point (socket "hat_socket" on bone "hat")
        for ob in hat:
            ob.location.z -= P["hat_z"]
        export_fbx(hat, arm, EXPORT / "SM_Bean_Hat_Tricorne.fbx", with_armature=False)
        for ob in hat:
            ob.location.z += P["hat_z"]
        # first person arm, built apart (not part of the rigged character)
        c_fp = collection("SM_Bean_FPArm", root)
        fp = build_fp_arm(c_fp)
        bake_transforms(fp)
        box_uvs(fp)
        export_fbx(fp, arm, EXPORT / "SM_Bean_FPArm.fbx", with_armature=False)
        c_fp.hide_viewport = True
        c_fp.hide_render = True
    if not a.no_save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        print("[bean] saved", BLEND)


if __name__ == "__main__":
    main()
