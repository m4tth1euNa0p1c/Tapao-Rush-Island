"""Build the expedition boat "SM_Barque" (cahier §5, §6.4, §6.5) as a game-ready, textured static mesh.

Run (background):
  blender --background --factory-startup --python scripts/props/barque.py -- [options]
Options:
  --bake       UV-unwrap and bake the texture atlas T_Barque_D / _ORM / _N (art/export/Boat/Textures/)
  --render     preview renders to art/_previews/barque_*.png (with bean-sized figures and cargo, render only)
  --export     write art/export/Boat/SM_Barque.fbx (+ UCX collisions, SOCKET_ empties) and SM_Barque_Oar.fbx,
               and register both in art/export/ue_manifest.json
  --no-save    do not write art/blender/props/barque.blend

Design (brief: docs/art/briefs/barque.md):
  a stylised ship's boat of a galleon (clinker strakes, raked transom with the Crown's emblem), sized for
  four 1.05 m beans and twelve cargo slots (cahier §5: 12 slots / 150 kg; the limits themselves are data,
  the mesh only offers the places).
  - 4 seats: two thwarts with red cushions (aft and forward), sockets SOCKET_Seat_0..3;
  - 12 cargo slots: 3 x 4 grid painted on the floorboards, one tie-down ring each, sockets SOCKET_Slot_00..11;
  - lantern hook on the stem head (night, L5), Crown pennant, tiller, two oarlocks (rowing is an extension:
    the oar is a separate mesh SM_Barque_Oar, pivot at the oarlock);
  - one material slot (MI_Barque) and one baked atlas -> one draw call; ~UCX convex pieces so beans can
    stand inside the hull (floor top = P["floor_z"]).

Units: meters, +Z up, bow toward +X (Unreal +X), port side = +Y (UE -Y), origin on the centerline at the
DESIGNED WATERLINE (z = 0): place the actor at the water level of UIslandWaterSubsystem, not on the ground.
Every tunable dimension lives in P; colours in PALETTE (sRGB hex).
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "art"
BLEND = ART / "blender" / "props" / "barque.blend"
PREV = ART / "_previews"
EXPORT = ART / "export" / "Boat"
TEX = EXPORT / "Textures"
UE_MESHES = "/Game/MonkeIsland/Boat/Meshes"

P = {
    "length": 5.6,          # stem to transom
    "beam": 2.2,            # max outer width
    "draft": 0.30,          # keel shell bottom under the waterline
    "freeboard": 0.50,      # gunwale above the waterline, amidships
    "sheer_bow": 0.30,      # extra sheer rise at the bow / stern (stylised)
    "sheer_stern": 0.16,
    "keel_rise_bow": 0.55,  # forefoot
    "keel_rise_stern": 0.15,
    "transom_ratio": 0.62,  # transom half beam / max half beam
    "transom_rake": 0.18,   # transom leans aft by this much per meter of height
    "fullness": 0.7,        # section exponent (lower = fuller bilge, more floor room)
    "strakes": 6,
    "lip": 0.04,           # clinker step
    "shell": 0.035,         # planking thickness
    "floor_z": -0.10,       # top of the floorboards (where beans stand and cargo sits)
    "seat_z": 0.14,         # top of the thwarts (cushion on top)
    "cargo_x0": -1.10,      # aft edge of the cargo grid
    "slot_pitch": 0.50,
    "slot_rows": 4,         # along X
    "slot_cols": 3,         # across Y
    "slot_mark": 0.44,      # painted square
    "thwart_aft_x": -1.45,
    "thwart_fwd_x": 1.25,
    "thwart_w": 0.28,
    "seat_y": 0.42,
    "foredeck_x": 2.05,
    "ribs": (-2.35, -1.85, -0.85, -0.10, 0.65, 1.70),
    "oarlock_x": 0.15,
    "oar_length": 2.4,
    "oar_pivot": 0.78,      # from the handle end
    "stem_head": 0.28,      # stem above the bow sheer
    "flag_height": 1.05,
    "nx": 64,               # hull stations
    "tex": 2048,
}

# name: kind + colours (sRGB hex). Kinds: hull, wood, paint, metal, fabric, rope
PALETTE = {
    "Hull":       dict(kind="hull", light="6B4A31", dark="3E2A1C", tar="1A1512", stripe="D9CBA6", axis="X"),
    "HullPaint":  dict(kind="paint", paint="1E2848", light="6B4A31", dark="3E2A1C", axis="X"),
    "InnerWood":  dict(kind="wood", light="8A6440", dark="5A3E27", axis="X"),
    "FloorWood":  dict(kind="wood", light="9C7650", dark="664A2F", axis="X"),
    "SeatWood":   dict(kind="wood", light="9C7650", dark="664A2F", axis="Y"),
    "RailRed":    dict(kind="paint", paint="A3202A", light="7A5636", dark="4A3220", axis="X"),
    "SlotPaint":  dict(kind="paint", paint="E8DCC0", light="9C7650", dark="664A2F", axis="X"),
    "Gold":       dict(kind="metal", base="C99A3E", rough=0.32, metal=1.0, stain="5C4520"),
    "Iron":       dict(kind="metal", base="34302D", rough=0.55, metal=0.8, stain="7A3B1E"),
    "Rope":       dict(kind="rope", light="C2A574", dark="7E6440"),
    "Cushion":    dict(kind="fabric", base="C21F2A"),
    "Pennant":    dict(kind="fabric", base="C21F2A"),
    "OarWood":    dict(kind="wood", light="B08A5C", dark="7A5A38", axis="X"),
}

PLANK_TONES = (0.25, 0.75, 0.5, 0.25, 0.5, 0.75, 0.5)
TAU = 2 * math.pi
L2 = P["length"] / 2
CHANNELS = {}   # material name -> {"color", "rough", "metal", "bsdf", "out"} sockets / nodes, used by the bake


# ---------------------------------------------------------------- materials

def srgb(hexa):
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def sock(sockets, key):
    for s in sockets:
        if s.identifier == key:
            return s
    return sockets[key]


class Graph:
    def __init__(self, nt):
        self.nt = nt
        self.x = -1600

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        n.location = (self.x, len(self.nt.nodes) * -40)
        self.x += 60
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.nt.links.new(a, b)

    def mix(self, fac, a, b, blend="MIX"):
        n = self.node("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        for key, v in (("Factor_Float", fac), ("A_Color", a), ("B_Color", b)):
            s = sock(n.inputs, key)
            if isinstance(v, (int, float)) or (isinstance(v, (list, tuple))):
                s.default_value = v
            else:
                self.link(v, s)
        return sock(n.outputs, "Result_Color")

    def fmix(self, fac, a, b):
        n = self.node("ShaderNodeMix", data_type="FLOAT")
        for key, v in (("Factor_Float", fac), ("A_Float", a), ("B_Float", b)):
            s = sock(n.inputs, key)
            if isinstance(v, (int, float)):
                s.default_value = v
            else:
                self.link(v, s)
        return sock(n.outputs, "Result_Float")

    def math(self, op, a, b=0.0, c=0.0):
        n = self.node("ShaderNodeMath", operation=op)
        for i, v in enumerate((a, b, c)):
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                self.link(v, n.inputs[i])
        return n.outputs[0]

    def noise(self, vec, scale, detail=4.0, rough=0.55, distortion=0.0):
        n = self.node("ShaderNodeTexNoise")
        self.link(vec, n.inputs["Vector"])
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distortion
        return n.outputs["Fac"]

    def ramp(self, fac, stops):
        n = self.node("ShaderNodeValToRGB")
        self.link(fac, n.inputs["Fac"])
        els = n.color_ramp.elements
        while len(els) < len(stops):
            els.new(0.5)
        for e, (pos, col) in zip(els, stops):
            e.position = pos
            e.color = col if len(col) == 4 else list(col) + [1.0]
        return n.outputs["Color"]

    def mapping(self, vec, scale):
        n = self.node("ShaderNodeMapping")
        self.link(vec, n.inputs["Vector"])
        n.inputs["Scale"].default_value = scale
        return n.outputs["Vector"]

    def wood(self, obj, spec):
        stretch = (1.0, 24.0, 24.0) if spec["axis"] == "X" else (24.0, 1.0, 24.0)
        stretch = tuple(s * 1.6 for s in stretch)
        grain = self.noise(self.mapping(obj, stretch), 2.5, detail=5.0, rough=0.5, distortion=0.15)
        rings = self.noise(self.mapping(obj, tuple(s * 0.25 for s in stretch)), 2.2, detail=2.0)
        g = self.math("MULTIPLY_ADD", grain, 0.75, self.math("MULTIPLY", rings, 0.35))
        col = self.ramp(g, [(0.42, srgb(spec["dark"])), (0.62, srgb(spec["light"]))])
        planks = self.noise(obj, 0.9, detail=1.0)          # board-to-board tone
        tone = self.ramp(planks, [(0.3, (0.78, 0.78, 0.78)), (0.7, (1.08, 1.05, 1.0))])
        col = self.mix(1.0, col, tone, "MULTIPLY")
        attr = self.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name="plank")
        strake = self.ramp(attr.outputs["Fac"], [(0.0, (1, 1, 1)), (0.03, (0.18, 0.16, 0.15)),
                                                 (0.2, (0.72, 0.72, 0.72)), (0.45, (0.95, 0.95, 0.95)),
                                                 (0.7, (1.22, 1.18, 1.12))])
        strake.node.color_ramp.interpolation = "CONSTANT"
        col = self.mix(1.0, col, strake, "MULTIPLY")
        rough = self.math("MULTIPLY_ADD", grain, -0.25, 0.86)
        return col, rough, g


def make_material(name):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    spec = PALETTE[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    G = Graph(nt)
    out = G.node("ShaderNodeOutputMaterial")
    bsdf = G.node("ShaderNodeBsdfPrincipled")
    G.link(bsdf.outputs[0], out.inputs["Surface"])
    obj = G.node("ShaderNodeTexCoord").outputs["Object"]
    kind = spec["kind"]
    metal = 0.0
    if kind in ("wood", "hull", "paint"):
        col, rough, height = G.wood(obj, spec)
        if kind == "hull":  # tarred bottom and a cream boot-top stripe at the waterline
            z = G.node("ShaderNodeSeparateXYZ")
            G.link(obj, z.inputs[0])
            wl = G.math("MULTIPLY_ADD", G.noise(obj, 3.0, 2.0), 0.03, -0.015)
            dz = G.math("SUBTRACT", z.outputs["Z"], wl)
            stripe = G.math("MULTIPLY", G.math("GREATER_THAN", dz, -0.035), G.math("LESS_THAN", dz, 0.035))
            tar = G.math("LESS_THAN", dz, -0.035)
            col = G.mix(stripe, col, srgb(spec["stripe"]))
            col = G.mix(tar, col, srgb(spec["tar"]))
            rough = G.fmix(tar, rough, 0.45)
        if kind == "paint":  # chipped paint over the wood
            wear = G.ramp(G.noise(obj, 7.0, detail=9.0, rough=0.7), [(0.64, (0, 0, 0)), (0.69, (1, 1, 1))])
            wear = G.math("MULTIPLY", wear, 1.0)
            shade = G.ramp(G.noise(obj, 1.5, 2.0), [(0.3, (0.82, 0.82, 0.82)), (0.7, (1.0, 1.0, 1.0))])
            paint = G.mix(1.0, srgb(spec["paint"]), shade, "MULTIPLY")
            col = G.mix(wear, paint, col)
            rough = G.fmix(wear, 0.48, rough)
            height = G.math("MULTIPLY_ADD", G.math("SUBTRACT", 1.0, wear), 0.6, G.math("MULTIPLY", height, 0.15))
    elif kind == "metal":
        stain = G.ramp(G.noise(obj, 9.0, detail=6.0), [(0.56, (0, 0, 0)), (0.64, (1, 1, 1))])
        stain = G.math("MULTIPLY", stain, 1.0)
        col = G.mix(stain, srgb(spec["base"]), srgb(spec["stain"]))
        rough = G.fmix(stain, spec["rough"], 0.9)
        metal = G.fmix(stain, spec["metal"], 0.0)
        height = G.math("MULTIPLY", stain, 0.4)
    elif kind == "fabric":
        weave = G.noise(obj, 90.0, detail=2.0)
        col = G.mix(1.0, srgb(spec["base"]),
                    G.ramp(G.noise(obj, 2.5, 3.0), [(0.3, (0.75, 0.75, 0.75)), (0.7, (1.0, 1.0, 1.0))]), "MULTIPLY")
        rough = 0.88
        height = G.math("MULTIPLY", weave, 0.5)
    elif kind == "rope":
        wave = G.node("ShaderNodeTexWave", wave_type="BANDS", bands_direction="DIAGONAL")
        G.link(obj, wave.inputs["Vector"])
        wave.inputs["Scale"].default_value = 45.0
        wave.inputs["Distortion"].default_value = 2.0
        col = G.ramp(wave.outputs["Fac"], [(0.2, srgb(spec["dark"])), (0.8, srgb(spec["light"]))])
        rough = 0.92
        height = wave.outputs["Fac"]
    else:
        raise ValueError(kind)
    # grime in the crevices (ambient occlusion baked into the colour: stylised, "inquiétant")
    ao = G.node("ShaderNodeAmbientOcclusion", samples=16)
    ao.inputs["Distance"].default_value = 0.25
    dirt = G.ramp(ao.outputs["AO"], [(0.0, (0.35, 0.33, 0.30)), (0.85, (1.0, 1.0, 1.0))])
    col = G.mix(1.0, col, dirt, "MULTIPLY")

    def as_socket(v):
        if isinstance(v, (int, float)):
            n = G.node("ShaderNodeValue")
            n.outputs[0].default_value = v
            return n.outputs[0]
        return v

    rough, metal = as_socket(rough), as_socket(metal)
    G.link(col, bsdf.inputs["Base Color"])
    G.link(rough, bsdf.inputs["Roughness"])
    G.link(metal, bsdf.inputs["Metallic"])
    bump = G.node("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    bump.inputs["Distance"].default_value = 0.003
    G.link(height, bump.inputs["Height"])
    G.link(bump.outputs["Normal"], bsdf.inputs["Normal"])
    CHANNELS[name] = dict(color=col, rough=rough, metal=metal, bsdf=bsdf, out=out)
    rgb = srgb(spec.get("paint") or spec.get("base") or spec.get("light"))
    mat.diffuse_color = rgb
    return mat


# ---------------------------------------------------------------- hull geometry

def half_beam(x):
    s = x / L2
    b = P["beam"] / 2
    if s >= 0:
        return max(0.035, b * max(0.0, 1 - s ** 2.2) ** 0.6)
    return b * (1 - (-s) ** 2.5 * (1 - P["transom_ratio"]))


def keel_z(x):
    s = x / L2
    z = -P["draft"]
    if s > 0.55:
        z += P["keel_rise_bow"] * ((s - 0.55) / 0.45) ** 2
    elif s < -0.85:
        z += P["keel_rise_stern"] * ((-s - 0.85) / 0.15) ** 2
    return z


def sheer_z(x):
    s = x / L2
    return P["freeboard"] + (P["sheer_bow"] * s ** 2.5 if s > 0 else P["sheer_stern"] * (-s) ** 2.5)


def rake_w(x):
    return max(0.0, (-x / L2 - 0.9) / 0.1)


def raw_section(x, t):
    a = min(max(t, 0.0), 1.0) * math.pi / 2
    zk = keel_z(x)
    return half_beam(x) * math.sin(a) ** P["fullness"], zk + (sheer_z(x) - zk) * (1 - math.cos(a))


def sec_pt(x, t, side=1, off=0.0):
    """Hull surface point at station x, girth parameter t (0 keel, 1 gunwale), offset along the outward normal."""
    y, z = raw_section(x, t)
    if off:
        e = 1e-3
        y2, z2 = raw_section(x, t + e) if t < 1 - e else raw_section(x, t - e)
        ty, tz = (y2 - y, z2 - z) if t < 1 - e else (y - y2, z - z2)
        n = math.hypot(ty, tz) or 1.0
        y += tz / n * off
        z += -ty / n * off
    xx = x - P["transom_rake"] * (z - keel_z(x)) * rake_w(x)
    return Vector((xx, side * y, z))


def t_at_z(x, z):
    lo, hi = 0.0, 1.0
    if z <= raw_section(x, 0)[1]:
        return 0.0
    if z >= raw_section(x, 1)[1]:
        return 1.0
    for _ in range(40):
        mid = (lo + hi) / 2
        if raw_section(x, mid)[1] < z:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2


def inner_hw(x, z, pad=0.0):
    """Inner half width of the hull at station x and height z."""
    return max(0.0, sec_pt(x, t_at_z(x, z), 1, -P["shell"]).y - pad)


def stations():
    n = P["nx"]
    return [-L2 + P["length"] * i / (n - 1) for i in range(n)]


# ---------------------------------------------------------------- mesh helpers

def collection(name, parent=None):
    coll = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if coll.name not in (parent or bpy.context.scene.collection).children:
        (parent or bpy.context.scene.collection).children.link(coll)
    return coll


def obj_from_bm(name, bm, mats, coll, smooth=False, sharp_angle=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in ([mats] if isinstance(mats, str) else mats):
        me.materials.append(make_material(m))
    for poly in me.polygons:
        poly.use_smooth = smooth
    if smooth and sharp_angle:
        me.set_sharp_from_angle(angle=math.radians(sharp_angle))
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob


def loft(name, sections, mat, coll, cap=True, smooth=False):
    """Quads between consecutive closed profiles (each a list of Vectors of the same length)."""
    bm = bmesh.new()
    rows = [[bm.verts.new(p) for p in sec] for sec in sections]
    n = len(sections[0])
    for j in range(len(rows) - 1):
        for i in range(n):
            i2 = (i + 1) % n
            bm.faces.new((rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]))
    if cap:
        bm.faces.new(list(reversed(rows[0])))
        bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, mat, coll, smooth=smooth, sharp_angle=40)


def box(name, center, size, mat, coll, bevel=0.008, segments=1, matrix=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(center)
        if matrix:
            v.co = matrix @ v.co
    ob = obj_from_bm(name, bm, mat, coll)
    if bevel:
        mod = ob.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = segments
        mod.limit_method = "ANGLE"
    return ob


def cylinder(name, p0, p1, r, mat, coll, segs=10, r1=None):
    p0, p1 = Vector(p0), Vector(p1)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r if r1 is None else r1,
                          depth=(p1 - p0).length)
    rot = Vector((0, 0, 1)).rotation_difference((p1 - p0).normalized()).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((p0 + p1) / 2) @ rot, verts=bm.verts)
    return obj_from_bm(name, bm, mat, coll, smooth=True, sharp_angle=50)


def torus(name, center, R, r, mat, coll, normal=(0, 0, 1), seg=12, ring=6):
    bm = bmesh.new()
    rows = []
    for i in range(seg):
        a = TAU * i / seg
        rows.append([bm.verts.new(((R + r * math.cos(b)) * math.cos(a), (R + r * math.cos(b)) * math.sin(a),
                                   r * math.sin(b))) for b in (TAU * k / ring for k in range(ring))])
    for i in range(seg):
        for k in range(ring):
            bm.faces.new((rows[i][k], rows[(i + 1) % seg][k], rows[(i + 1) % seg][(k + 1) % ring],
                          rows[i][(k + 1) % ring]))
    rot = Vector((0, 0, 1)).rotation_difference(Vector(normal).normalized()).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(center) @ rot, verts=bm.verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, mat, coll, smooth=True)


def sweep_xz(name, path, half_w, up, down, mat, coll):
    """Rectangular beam along a path in the XZ plane (keel, stem, tiller). up/down: extent on each side."""
    secs = []
    for i, p in enumerate(path):
        a = path[max(i - 1, 0)]
        b = path[min(i + 1, len(path) - 1)]
        tx, tz = b[0] - a[0], b[1] - a[1]
        n = math.hypot(tx, tz) or 1.0
        nx, nz = -tz / n, tx / n
        u = Vector((p[0] + nx * up, 0, p[1] + nz * up))
        d = Vector((p[0] - nx * down, 0, p[1] - nz * down))
        secs.append([d + Vector((0, -half_w, 0)), d + Vector((0, half_w, 0)),
                     u + Vector((0, half_w, 0)), u + Vector((0, -half_w, 0))])
    return loft(name, secs, mat, coll)


# ---------------------------------------------------------------- parts

def strake_rows():
    N, lip = P["strakes"], P["lip"]
    half = []
    for k in range(N):
        t0, t1 = k / N, (k + 1) / N
        lk = lip if k else 0.0
        half += [(t0, lk, k), ((t0 + t1) / 2, lk * 0.45, k), (t1, 0.0, k)]
    return [(-1, *r) for r in reversed(half)] + [(1, *r) for r in half[1:]]


def build_hull(coll):
    """Clinker shell (outer + inner via Solidify) and the raked transom."""
    rows = strake_rows()
    N = P["strakes"]
    bm = bmesh.new()
    plank = bm.faces.layers.float.new("plank")  # per-strake tone + dark lap seams, read by the wood shader
    grid = []
    for x in stations():
        lip_k = min(1.0, half_beam(x) / 0.35)
        grid.append([bm.verts.new(sec_pt(x, t, side, off * lip_k)) for side, t, off, _ in rows])
    for j in range(len(grid) - 1):
        for i in range(len(rows) - 1):
            f = bm.faces.new((grid[j][i], grid[j + 1][i], grid[j + 1][i + 1], grid[j][i + 1]))
            f.material_index = 1 if min(rows[i][3], rows[i + 1][3]) == N - 1 else 0
            ka, kb = rows[i][3], rows[i + 1][3]
            f[plank] = 0.05 if ka != kb else PLANK_TONES[(ka * 7 + j // 22) % len(PLANK_TONES)]
    # outward normals: the keel face amidships must look down
    bm.normal_update()
    mid = [f for f in bm.faces if abs(f.calc_center_median().y) < 0.2 and abs(f.calc_center_median().x) < 0.3]
    if mid and mid[0].normal.z > 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    hull = obj_from_bm("Hull", bm, ["Hull", "HullPaint", "InnerWood", "HullPaint"], coll, smooth=True,
                       sharp_angle=35)
    mod = hull.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = P["shell"]
    mod.offset = -1.0
    mod.material_offset = 2
    mod.use_even_offset = True

    # transom: the last station outline, closed at the top, thickness toward the bow
    x = -L2
    outline = [sec_pt(x, t, side) for side, t, off, _ in rows if off == 0.0 or t == 0.0]
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in outline]
    face = bm.faces.new(vs)
    face.normal_update()
    if face.normal.x > 0:
        face.normal_flip()
    tr = obj_from_bm("Transom", bm, ["HullPaint", "InnerWood"], coll)
    mod = tr.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = 0.05
    mod.offset = -1.0
    mod.material_offset = 1
    return [hull, tr]


def rail_section(x, side, z_top, z_bot, out, inn):
    po = sec_pt(x, 1.0, side)
    pi = sec_pt(x, 1.0, side, -P["shell"])
    zs = sheer_z(x)
    yo = po.y + side * out
    yi = pi.y - side * inn
    return [Vector((po.x, yo, zs + z_bot)), Vector((po.x, yo, zs + z_top)),
            Vector((po.x, yi, zs + z_top)), Vector((po.x, yi, zs + z_bot))]


def build_trim(coll):
    """Red cap rails, gold moulding, keel + stem, sternpost, ribs, risers."""
    parts = []
    xs = stations()
    for side, tag in ((1, "L"), (-1, "R")):
        secs = [rail_section(x, side, 0.045, -0.05, 0.025, 0.03) for x in xs]
        if side < 0:
            secs = [list(reversed(s)) for s in secs]
        parts.append(loft(f"Rail_{tag}", secs, "RailRed", coll))
        # gold moulding on the top strake
        t = 1 - 0.5 / P["strakes"]
        gs = []
        for x in xs[1:-3]:
            a = sec_pt(x, t, side, 0.004)
            b = sec_pt(x, t, side, 0.016)
            gs.append([a + Vector((0, 0, -0.012)), b + Vector((0, 0, -0.012)),
                       b + Vector((0, 0, 0.012)), a + Vector((0, 0, 0.012))])
        if side < 0:
            gs = [list(reversed(s)) for s in gs]
        parts.append(loft(f"Moulding_{tag}", gs, "Gold", coll))
        # riser under the thwarts
        rs = []
        for x in np.linspace(-2.3, 1.9, 24):
            z0, z1 = P["seat_z"] - 0.12, P["seat_z"] - 0.05
            y0 = inner_hw(x, z1) * side
            rs.append([Vector((x, y0, z0)), Vector((x, y0, z1)),
                       Vector((x, y0 - side * 0.04, z1)), Vector((x, y0 - side * 0.04, z0))])
        if side < 0:
            rs = [list(reversed(s)) for s in rs]
        parts.append(loft(f"Riser_{tag}", rs, "InnerWood", coll))
    # stern cap rail across the transom
    xt = sec_pt(-L2, 1.0, 1).x
    yo = sec_pt(-L2, 1.0, 1).y + 0.025
    zs = sheer_z(-L2)
    parts.append(box("Rail_Transom", (xt + 0.02, 0, zs + 0.0), (0.09, 2 * yo, 0.095), "RailRed", coll))
    # keel and stem: one beam from the transom to the stem head
    path = [(x, keel_z(x)) for x in xs[1:]]
    zb = keel_z(L2)
    zh = sheer_z(L2) + P["stem_head"]
    for u in np.linspace(0.0, 1.0, 14)[1:]:
        path.append((L2 + 0.03 * u + 0.09 * u ** 3, zb + (zh - zb) * u))
    parts.append(sweep_xz("KeelStem", path, 0.045, 0.02, 0.07, "Hull", coll))
    # sternpost on the transom
    zk = keel_z(-L2)
    sp = [(sec_pt(-L2, 0.0, 1).x - P["transom_rake"] * (z - zk) * 0 - 0.0, z) for z in np.linspace(zk, zs, 8)]
    sp = [(-L2 - P["transom_rake"] * (z - zk), z) for _, z in sp]
    parts.append(sweep_xz("Sternpost", sp, 0.04, 0.0, 0.04, "Hull", coll))
    # ribs (frames) on the inner shell, above the floor
    for k, x in enumerate(P["ribs"]):
        t0 = t_at_z(x, P["floor_z"] - 0.02)
        ts = np.linspace(t0, 0.97, 9)
        for side, tag in ((1, "L"), (-1, "R")):
            secs = []
            for t in ts:
                p = sec_pt(x, t, side, -P["shell"] + 0.002)
                q = sec_pt(x, t, side, -P["shell"] - 0.03)
                secs.append([p + Vector((-0.025, 0, 0)), p + Vector((0.025, 0, 0)),
                             q + Vector((0.025, 0, 0)), q + Vector((-0.025, 0, 0))])
            parts.append(loft(f"Rib_{k}_{tag}", secs, "InnerWood", coll))
    return parts


def build_floor(coll):
    """Floorboards, thwarts with cushions, foredeck, painted cargo grid with tie-down rings."""
    parts = []
    fz, pw, gap = P["floor_z"], 0.14, 0.012
    n = 12
    xs = np.arange(-L2 + 0.07, L2, 0.02)
    for i in range(n):
        y = (i - (n - 1) / 2) * (pw + gap)
        ok = [x for x in xs if inner_hw(x, fz) - 0.005 >= abs(y) + pw / 2 and x < P["foredeck_x"] + 0.1]
        if len(ok) < 15:
            continue
        x0, x1 = ok[0], ok[-1]
        parts.append(box(f"Floor_{i:02d}", ((x0 + x1) / 2, y, fz - 0.015), (x1 - x0, pw, 0.03), "FloorWood", coll,
                         bevel=0.006))
    # thwarts + cushions (4 seats)
    for k, x in enumerate((P["thwart_aft_x"], P["thwart_fwd_x"])):
        w = P["thwart_w"]
        hw = min(inner_hw(x - w / 2, P["seat_z"]), inner_hw(x + w / 2, P["seat_z"])) - 0.005
        parts.append(box(f"Thwart_{k}", (x, 0, P["seat_z"] - 0.025), (w, 2 * hw, 0.05), "SeatWood", coll))
        for side in (1, -1):
            parts.append(box(f"Cushion_{k}_{side}", (x, side * P["seat_y"], P["seat_z"] + 0.027),
                             (w - 0.04, 0.38, 0.055), "Cushion", coll, bevel=0.02, segments=3))
    # foredeck
    bm = bmesh.new()
    rows = []
    for x in [x for x in stations() if x >= P["foredeck_x"]] + [L2]:
        zd = sheer_z(x) - 0.10
        hw = max(inner_hw(x, zd), 0.01)
        rows.append([bm.verts.new((x, -hw, zd)), bm.verts.new((x, hw, zd))])
    for j in range(len(rows) - 1):
        bm.faces.new((rows[j][0], rows[j + 1][0], rows[j + 1][1], rows[j][1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
    deck = obj_from_bm("Foredeck", bm, "FloorWood", coll)
    mod = deck.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = 0.03
    mod.offset = -1.0
    parts.append(deck)
    # cargo grid: painted squares + one tie-down ring per slot
    pitch, mark, sw = P["slot_pitch"], P["slot_mark"], 0.024
    for r in range(P["slot_rows"]):
        for c in range(P["slot_cols"]):
            cx = P["cargo_x0"] + pitch * (r + 0.5)
            cy = (c - (P["slot_cols"] - 1) / 2) * pitch
            for dx, dy, sx, sy in ((0, mark / 2, mark, sw), (0, -mark / 2, mark, sw),
                                   (mark / 2, 0, sw, mark), (-mark / 2, 0, sw, mark)):
                parts.append(box(f"Slot_{r}{c}_{dx:+.2f}{dy:+.2f}", (cx + dx, cy + dy, fz + 0.002),
                                 (sx, sy, 0.004), "SlotPaint", coll, bevel=0))
            parts.append(cylinder(f"RingPlate_{r}{c}", (cx, cy, fz), (cx, cy, fz + 0.006), 0.03, "Iron", coll,
                                  segs=8))
            parts.append(torus(f"Ring_{r}{c}", (cx, cy, fz + 0.012), 0.032, 0.007, "Iron", coll, seg=10, ring=5))
    return parts


def build_stern(coll):
    """Rudder, pintles, tiller, Crown emblem on the transom, flagstaff + pennant."""
    parts = []
    zk = keel_z(-L2)

    def xt(z):  # outer face of the raked transom (sternpost face)
        return -L2 - P["transom_rake"] * (z - zk) - 0.04

    blade = [(xt(0.62), 0.70), (xt(-0.24), -0.24), (xt(-0.24) - 0.36, -0.20), (xt(0.25) - 0.22, 0.25),
             (xt(0.55) - 0.10, 0.55)]
    bm = bmesh.new()
    face = bm.faces.new([bm.verts.new((x, 0, z)) for x, z in blade])
    face.normal_update()
    if face.normal.y < 0:
        face.normal_flip()
    bmesh.ops.translate(bm, vec=(0, -0.025, 0), verts=bm.verts)
    rud = obj_from_bm("Rudder", bm, "Hull", coll)
    mod = rud.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = 0.05
    mod.offset = 1.0
    bev = rud.modifiers.new("Bevel", "BEVEL")
    bev.width = 0.01
    parts.append(rud)
    for z in (-0.15, 0.35):
        parts.append(box(f"Pintle_{z:+.2f}", (xt(z) - 0.005, 0, z), (0.06, 0.065, 0.04), "Iron", coll, bevel=0.004))
    head = (xt(0.70) - 0.01, 0.70)
    parts.append(sweep_xz("Tiller", [head, (-2.45, 0.76), (-1.95, 0.80)], 0.028, 0.025, 0.025, "OarWood", coll))
    # Crown emblem: band + five points + pearls, tilted like the transom
    zc = 0.28
    x0 = -L2 - P["transom_rake"] * (zc - zk) - 0.004
    rot = Matrix.Rotation(math.atan(P["transom_rake"]), 4, "Y")
    M = Matrix.Translation((x0, 0, zc)) @ rot
    parts.append(box("Crown_Band", (-0.012, 0, 0), (0.024, 0.36, 0.07), "Gold", coll, bevel=0.006, matrix=M))
    for k, yy in enumerate((-0.16, -0.08, 0.0, 0.08, 0.16)):
        h = 0.15 if k % 2 == 0 else 0.10
        bm = bmesh.new()
        base = [bm.verts.new(M @ Vector((-0.004, yy + s * 0.035, 0.03))) for s in (-1, 1)]
        back = [bm.verts.new(M @ Vector((-0.022, yy + s * 0.035, 0.03))) for s in (-1, 1)]
        tip = [bm.verts.new(M @ Vector((-0.004, yy, 0.03 + h))), bm.verts.new(M @ Vector((-0.022, yy, 0.03 + h)))]
        bm.faces.new((base[0], base[1], tip[0]))
        bm.faces.new((back[1], back[0], tip[1]))
        bm.faces.new((base[0], tip[0], tip[1], back[0]))
        bm.faces.new((base[1], back[1], tip[1], tip[0]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        parts.append(obj_from_bm(f"Crown_Point_{k}", bm, "Gold", coll))
        pearl = M @ Vector((-0.013, yy, 0.03 + h + 0.012))
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=0.02)
        bmesh.ops.translate(bm, vec=pearl, verts=bm.verts)
        parts.append(obj_from_bm(f"Crown_Pearl_{k}", bm, "Gold", coll, smooth=True))
    # flagstaff on the port quarter + Crown pennant
    fx = -2.55
    fz0 = sheer_z(fx) - 0.30
    fy = inner_hw(fx, fz0) - 0.035
    top = sheer_z(fx) + P["flag_height"]
    parts.append(cylinder("Flagstaff", (fx, fy, fz0), (fx, fy, top), 0.022, "OarWood", coll, segs=8))
    parts.append(box("Flagstaff_Bracket", (fx, fy + 0.03, sheer_z(fx) - 0.02), (0.06, 0.08, 0.05), "Iron", coll,
                     bevel=0.004))
    parts.append(torus("Flagstaff_Knob", (fx, fy, top + 0.01), 0.02, 0.014, "Gold", coll, seg=8, ring=5))
    bm = bmesh.new()
    nu, nv, length, h0 = 10, 3, 0.75, 0.30
    grid = []
    for i in range(nu + 1):
        u = i / nu
        h = h0 * (1 - 0.8 * u)
        wave = 0.05 * math.sin(u * TAU * 1.1) * u
        col_v = []
        for j in range(nv + 1):
            v = j / nv
            notch = 0.12 * u ** 3 * (1 - abs(2 * v - 1))  # swallowtail
            col_v.append(bm.verts.new((fx - 0.02 - u * length + notch * length * 0.6, fy + wave,
                                       top - 0.04 - v * h - (h0 - h) / 2)))
        grid.append(col_v)
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]))
    pen = obj_from_bm("Pennant", bm, "Pennant", coll, smooth=True)
    mod = pen.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = 0.008
    parts.append(pen)
    return parts, (fx, fy, top + 0.03), (head[0] + 0.5, 0, 0.78)


def build_bow(coll):
    """Lantern hook on the stem head, mooring ring, rope coil, oarlocks."""
    parts = []
    zh = sheer_z(L2) + P["stem_head"]
    hx = L2 + 0.12
    parts.append(box("LanternArm", (hx + 0.06, 0, zh - 0.06), (0.16, 0.03, 0.03), "Iron", coll, bevel=0.004))
    parts.append(torus("LanternHook", (hx + 0.13, 0, zh - 0.10), 0.03, 0.007, "Iron", coll, normal=(0, 1, 0),
                       seg=10, ring=5))
    lantern = (hx + 0.13, 0, zh - 0.13)
    moor = (L2 + 0.075, 0, 0.42)
    parts.append(torus("MooringRing", moor, 0.05, 0.011, "Iron", coll, normal=(0, 1, 0), seg=12, ring=5))
    zd = sheer_z(2.4) - 0.10 + 0.015
    for k in range(3):
        parts.append(torus(f"RopeCoil_{k}", (2.38 + 0.01 * k, 0.0, zd + 0.018 + k * 0.03), 0.13 - 0.012 * k,
                           0.02, "Rope", coll, seg=16, ring=6))
    oarlocks = {}
    x = P["oarlock_x"]
    for side, tag in ((1, "L"), (-1, "R")):
        sec = rail_section(x, side, 0.045, -0.05, 0.025, 0.03)
        yc = (sec[1].y + sec[2].y) / 2
        zt = sheer_z(x) + 0.045
        for dx in (-0.06, 0.06):
            parts.append(cylinder(f"Thole_{tag}{dx:+.2f}", (x + dx, yc, zt - 0.01), (x + dx, yc, zt + 0.11), 0.014,
                                  "Iron", coll, segs=8))
        oarlocks[tag] = (x, yc, zt + 0.04)
    return parts, lantern, moor, oarlocks


def build_oar(coll):
    """Separate mesh, along +X, pivot at the oarlock contact (rowing animation is an extension)."""
    Lo, piv = P["oar_length"], P["oar_pivot"]
    parts = [cylinder("Oar_Shaft", (-piv + 0.12, 0, 0), (Lo - piv - 0.55, 0, 0), 0.026, "OarWood", coll, segs=10),
             cylinder("Oar_Grip", (-piv, 0, 0), (-piv + 0.13, 0, 0), 0.022, "OarWood", coll, segs=10),
             cylinder("Oar_Leather", (-0.06, 0, 0), (0.12, 0, 0), 0.031, "Rope", coll, segs=10)]
    bm = bmesh.new()
    x0, x1 = Lo - piv - 0.6, Lo - piv
    pts = [(x0, 0.026), (x0 + 0.18, 0.07), (x1 - 0.06, 0.085), (x1, 0.07)]
    top = [bm.verts.new((x, w, 0.012)) for x, w in pts] + [bm.verts.new((x, -w, 0.012)) for x, w in reversed(pts)]
    bm.faces.new(top)
    bmesh.ops.translate(bm, vec=(0, 0, -0.024), verts=bm.verts)
    blade = obj_from_bm("Oar_Blade", bm, "OarWood", coll)
    mod = blade.modifiers.new("Solidify", "SOLIDIFY")
    mod.thickness = 0.024
    mod.offset = 1.0
    parts.append(blade)
    parts.append(box("Oar_Tip", (x1 - 0.07, 0, 0), (0.14, 0.172, 0.03), "RailRed", coll, bevel=0.006))
    return parts


# ---------------------------------------------------------------- collisions and sockets

def hull_obj(name, pts):
    pts = np.unique(np.round(np.asarray([tuple(p) for p in pts], np.float64), 4), axis=0)
    bm = bmesh.new()
    for p in pts:
        bm.verts.new(p)
    res = bmesh.ops.convex_hull(bm, input=bm.verts, use_existing_faces=False)
    junk = list({g for g in res["geom_interior"] + res["geom_unused"] if isinstance(g, bmesh.types.BMVert)})
    bmesh.ops.delete(bm, geom=junk, context="VERTS")
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return bpy.data.objects.new(name, me)


def build_collisions(coll):
    """Convex pieces: floor blocks, side walls (with rails), solid bow, transom, thwarts, rudder."""
    fz, sh = P["floor_z"], P["shell"]
    chunks = [-L2, -1.9, -1.0, -0.1, 0.8, 1.6, P["foredeck_x"]]
    groups = []
    for x0, x1 in zip(chunks[:-1], chunks[1:]):
        xs = np.linspace(x0, x1, 5)
        bottom = []
        for x in xs:
            tf = t_at_z(x, fz)
            for side in (1, -1):
                bottom += [sec_pt(x, t, side, P["lip"]) for t in np.linspace(0, tf, 6)]
            bottom.append(Vector((sec_pt(x, 0, 1).x, 0, keel_z(x) - 0.07)))
        groups.append(bottom)
        for side in (1, -1):
            wall = []
            for x in xs:
                tf = t_at_z(x, fz)
                for t in np.linspace(tf, 1.0, 6):
                    wall += [sec_pt(x, t, side, P["lip"]), sec_pt(x, t, side, -sh)]
                wall += rail_section(x, side, 0.045, -0.05, 0.025, 0.03)
            groups.append(wall)
    bow = []
    for x in np.linspace(P["foredeck_x"], L2, 6):
        for side in (1, -1):
            bow += [sec_pt(x, t, side, P["lip"]) for t in np.linspace(0, 1, 6)]
            bow += rail_section(x, side, 0.045, -0.05, 0.025, 0.03)
    groups.append(bow)
    tr = [sec_pt(-L2, t, s) for s in (1, -1) for t in np.linspace(0, 1, 8)]
    groups.append(tr + [p + Vector((0.05, 0, 0)) for p in tr])
    for x in (P["thwart_aft_x"], P["thwart_fwd_x"]):
        w = P["thwart_w"]
        hw = min(inner_hw(x - w / 2, P["seat_z"]), inner_hw(x + w / 2, P["seat_z"]))
        groups.append([Vector((x + sx * w / 2, sy * hw, P["seat_z"] + sz * 0.025 - 0.025))
                       for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])
    zk = keel_z(-L2)
    xt = lambda z: -L2 - P["transom_rake"] * (z - zk) - 0.04  # noqa: E731
    blade = [(xt(0.62), 0.70), (xt(-0.24), -0.24), (xt(-0.24) - 0.36, -0.20), (xt(0.25) - 0.22, 0.25)]
    groups.append([Vector((x, y, z)) for x, z in blade for y in (-0.03, 0.03)])
    objs = []
    for k, g in enumerate(groups):
        ob = hull_obj(f"UCX_SM_Barque_{k:02d}", g)
        coll.objects.link(ob)
        ob.display_type = "WIRE"
        ob.hide_render = True
        objs.append(ob)
    return objs


def make_sockets(hull, coll, lantern, flag, tiller, moor, oarlocks):
    socks = {}
    seats = [(P["thwart_aft_x"], 1), (P["thwart_aft_x"], -1), (P["thwart_fwd_x"], 1), (P["thwart_fwd_x"], -1)]
    for i, (x, side) in enumerate(seats):
        socks[f"Seat_{i}"] = (x, side * P["seat_y"], P["seat_z"] + 0.055)
    k = 0
    for r in range(P["slot_rows"]):
        for c in range(P["slot_cols"]):
            socks[f"Slot_{k:02d}"] = (P["cargo_x0"] + P["slot_pitch"] * (r + 0.5),
                                      (c - (P["slot_cols"] - 1) / 2) * P["slot_pitch"], P["floor_z"])
            k += 1
    socks["Lantern"] = lantern
    socks["Flag"] = flag
    socks["Tiller"] = tiller
    socks["Mooring"] = moor
    socks["Oarlock_L"] = oarlocks["L"]
    socks["Oarlock_R"] = oarlocks["R"]
    for side, tag in ((1, "L"), (-1, "R")):
        socks[f"Board_{tag}"] = (-0.2, side * (half_beam(-0.2) + 0.25), 0.0)
    socks["Marker"] = (0.0, 0.0, 2.2)
    objs = []
    for name, loc in socks.items():
        e = bpy.data.objects.new(f"SOCKET_{name}", None)
        e.empty_display_type = "ARROWS"
        e.empty_display_size = 0.12
        e.location = loc
        e.parent = hull
        coll.objects.link(e)
        objs.append(e)
    return objs


# ---------------------------------------------------------------- assemble, bake, export

def join(parts, name):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in parts:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.convert(target="MESH")
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = ob.data.name = name
    return ob


def unwrap(objs):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.002, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.uv.pack_islands(rotate=True, margin=0.004)
    bpy.ops.object.mode_set(mode="OBJECT")


def bake(objs):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    S = P["tex"]
    mats = {m for ob in objs for m in ob.data.materials if m}
    imgs = {}
    for key, cs in (("D", "sRGB"), ("R", "Non-Color"), ("M", "Non-Color"), ("AO", "Non-Color"), ("N", "Non-Color")):
        img = bpy.data.images.new(f"T_Barque_bake_{key}", S, S, alpha=False, float_buffer=key == "N")
        img.colorspace_settings.name = cs
        imgs[key] = img
    tex_nodes = {}
    for m in mats:
        n = m.node_tree.nodes.new("ShaderNodeTexImage")
        n.location = (600, 400)
        m.node_tree.nodes.active = n
        tex_nodes[m.name] = n
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]

    def run(key, btype, samples, channel=None):
        for m in mats:
            nt = m.node_tree
            tex_nodes[m.name].image = imgs[key]
            ch = CHANNELS[m.name]
            if channel:
                em = nt.nodes.get("BAKE_EMIT") or nt.nodes.new("ShaderNodeEmission")
                em.name = "BAKE_EMIT"
                nt.links.new(ch[channel], em.inputs["Color"])
                nt.links.new(em.outputs[0], ch["out"].inputs["Surface"])
            else:
                nt.links.new(ch["bsdf"].outputs[0], ch["out"].inputs["Surface"])
        scene.cycles.samples = samples
        kw = dict(type=btype, margin=8, use_clear=True)
        if btype == "NORMAL":
            kw["normal_space"] = "TANGENT"
        bpy.ops.object.bake(**kw)
        print(f"[barque] baked {key}")

    run("D", "EMIT", 16, "color")
    run("R", "EMIT", 1, "rough")
    run("M", "EMIT", 1, "metal")
    run("AO", "AO", 32)
    run("N", "NORMAL", 4)
    for m in mats:  # restore the shader for previews
        run_restore = CHANNELS[m.name]
        m.node_tree.links.new(run_restore["bsdf"].outputs[0], run_restore["out"].inputs["Surface"])

    TEX.mkdir(parents=True, exist_ok=True)

    def px(img):
        a = np.empty(S * S * 4, np.float32)
        img.pixels.foreach_get(a)
        return a.reshape(S, S, 4)

    def save(arr, name, cs):
        img = bpy.data.images.new(name, S, S, alpha=False)
        img.colorspace_settings.name = cs
        img.pixels.foreach_set(np.clip(arr, 0, 1).ravel())
        img.filepath_raw = str(TEX / f"{name}.png")
        img.file_format = "PNG"
        img.save()
        return img

    d = px(imgs["D"])
    d[..., 3] = 1.0
    orm = np.ones((S, S, 4), np.float32)
    orm[..., 0] = px(imgs["AO"])[..., 0]
    orm[..., 1] = px(imgs["R"])[..., 0]
    orm[..., 2] = px(imgs["M"])[..., 0]
    nrm = px(imgs["N"])
    nrm[..., 1] = 1.0 - nrm[..., 1]  # OpenGL -> DirectX (Unreal)
    nrm[..., 3] = 1.0
    out = {"D": save(d, "T_Barque_D", "sRGB"), "ORM": save(orm, "T_Barque_ORM", "Non-Color"),
           "N": save(nrm, "T_Barque_N", "Non-Color")}
    print("[barque] textures:", TEX)
    return out


def baked_material(tex):
    """Single slot MI_Barque using the atlas (what Unreal gets: one material instance, one draw call)."""
    mat = bpy.data.materials.new("MI_Barque")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    d = nt.nodes.new("ShaderNodeTexImage")
    d.image = tex["D"]
    orm = nt.nodes.new("ShaderNodeTexImage")
    orm.image = tex["ORM"]
    nrm = nt.nodes.new("ShaderNodeTexImage")
    nrm.image = tex["N"]
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(d.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(orm.outputs["Color"], sep.inputs[0])
    nt.links.new(sep.outputs[1], bsdf.inputs["Roughness"])
    nt.links.new(sep.outputs[2], bsdf.inputs["Metallic"])
    # preview only: re-flip green for Blender (OpenGL) normal maps
    flip = nt.nodes.new("ShaderNodeSeparateColor")
    comb = nt.nodes.new("ShaderNodeCombineColor")
    inv = nt.nodes.new("ShaderNodeMath")
    inv.operation = "SUBTRACT"
    inv.inputs[0].default_value = 1.0
    nt.links.new(nrm.outputs["Color"], flip.inputs[0])
    nt.links.new(flip.outputs[0], comb.inputs[0])
    nt.links.new(flip.outputs[1], inv.inputs[1])
    nt.links.new(inv.outputs[0], comb.inputs[1])
    nt.links.new(flip.outputs[2], comb.inputs[2])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(comb.outputs[0], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def assign_single(ob, mat):
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    for p in ob.data.polygons:
        p.material_index = 0


def export_fbx(objs, path):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.hide_set(False)
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH", "EMPTY"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             add_leaf_bones=False, bake_anim=False, use_tspace=True)
    print("[barque] export:", path)


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def register_manifest(entries):
    path = ART / "export" / "ue_manifest.json"
    manifest = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    manifest.update(entries)
    path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False), encoding="utf-8")


# ---------------------------------------------------------------- previews

def blob(name, center, radii, mat_rgb, coll):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * radii[0], v.co.y * radii[1], v.co.z * radii[2])) + Vector(center)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    m = bpy.data.materials.new(f"REF_{name}")
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = srgb(mat_rgb)
    bsdf.inputs["Roughness"].default_value = 0.5
    me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob


def preview_props(coll):
    """Bean-sized figures (1.05 m) and stowed cargo, to judge scale and the slot grid (render only)."""
    blob("REF_Bean_Seated_0", (P["thwart_aft_x"], P["seat_y"], P["seat_z"] + 0.06 + 0.34), (0.2, 0.18, 0.36),
         "4FA3E0", coll)
    blob("REF_Bean_Seated_3", (P["thwart_fwd_x"], -P["seat_y"], P["seat_z"] + 0.06 + 0.34), (0.2, 0.18, 0.36),
         "C21F2A", coll)
    blob("REF_Bean_Standing", (0.65, 0.0, P["floor_z"] + 0.525), (0.2, 0.18, 0.525), "4FA3E0", coll)
    crate = make_material("SeatWood")
    for k, (r, c) in enumerate(((0, 0), (0, 1), (1, 0), (2, 2), (0, 2))):
        cx = P["cargo_x0"] + P["slot_pitch"] * (r + 0.5)
        cy = (c - 1) * P["slot_pitch"]
        ob = box(f"REF_Crate_{k}", (cx, cy, P["floor_z"] + 0.19), (0.40, 0.40, 0.38), "SeatWood", coll, bevel=0.015)
        ob.data.materials[0] = crate
    for k, (r, c) in enumerate(((1, 2), (3, 0))):
        cx = P["cargo_x0"] + P["slot_pitch"] * (r + 0.5)
        cy = (c - 1) * P["slot_pitch"]
        cylinder(f"REF_Log_{k}", (cx - 0.21, cy, P["floor_z"] + 0.1), (cx + 0.21, cy, P["floor_z"] + 0.1), 0.1,
                 "InnerWood", coll, segs=12)


def setup_render(scene):
    scene.render.resolution_x, scene.render.resolution_y = 1400, 900
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT", "BLENDER_WORKBENCH"):
        try:
            scene.render.engine = eng
            break
        except TypeError:
            continue
    world = bpy.data.worlds.new("W_Studio")
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs[0].default_value = (0.62, 0.70, 0.78, 1)
    bg.inputs[1].default_value = 0.8
    scene.world = world
    for name, rot, energy in (("Key", (0.8, 0.2, -0.7), 4.0), ("Fill", (1.2, 0.0, 2.4), 1.0),
                              ("Rim", (1.3, 0.0, 3.8), 2.2)):
        ld = bpy.data.lights.new(name, "SUN")
        ld.energy = energy
        ld.angle = math.radians(6)
        lo = bpy.data.objects.new(name, ld)
        lo.rotation_euler = rot
        scene.collection.objects.link(lo)
    target = bpy.data.objects.new("CAM_Target", None)
    target.location = (0, 0, 0.25)
    scene.collection.objects.link(target)
    cam = bpy.data.objects.new("CAM_Barque", bpy.data.cameras.new("CAM_Barque"))
    scene.collection.objects.link(cam)
    con = cam.constraints.new("TRACK_TO")
    con.target = target
    con.track_axis = "TRACK_NEGATIVE_Z"
    con.up_axis = "UP_Y"
    scene.camera = cam
    return cam


def render_views(scene, cam, water):
    PREV.mkdir(parents=True, exist_ok=True)
    views = [("trois_quarts", (6.2, 5.0, 3.6), "PERSP", None, False),
             ("dessus", (0.0, 0.0, 9.0), "ORTHO", 6.4, False),
             ("profil", (0.0, 9.0, 0.3), "ORTHO", 6.6, False),
             ("poupe", (-6.0, 3.2, 1.6), "PERSP", None, False),
             ("flottaison", (5.5, -5.5, 1.4), "PERSP", None, True),
             ("detail", (2.2, 3.4, 0.9), "PERSP", None, False)]
    out = []
    for tag, loc, kind, ortho, wet in views:
        cam.location = loc
        cam.data.type = kind
        cam.data.lens = 40
        if ortho:
            cam.data.ortho_scale = ortho
        water.hide_render = not wet
        path = PREV / f"barque_{tag}.png"
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        out.append(str(path))
    print("[barque] renders:", ", ".join(out))


def water_plane(coll):
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=12)
    me = bpy.data.meshes.new("REF_Water")
    bm.to_mesh(me)
    bm.free()
    m = bpy.data.materials.new("REF_Water")
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = srgb("1F5C66")
    bsdf.inputs["Roughness"].default_value = 0.15
    me.materials.append(m)
    ob = bpy.data.objects.new("REF_Water", me)
    coll.objects.link(ob)
    return ob


# ---------------------------------------------------------------- main

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--bake", action="store_true")
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    a = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Barque"
    scene.unit_settings.system = "METRIC"
    root = collection("Barque")
    c_parts = collection("Parts", root)
    c_oar = collection("OarParts", root)
    c_col = collection("Collision", root)
    c_ref = collection("Reference", root)

    parts = build_hull(c_parts)
    parts += build_trim(c_parts)
    parts += build_floor(c_parts)
    stern, flag, tiller = build_stern(c_parts)
    parts += stern
    bow, lantern, moor, oarlocks = build_bow(c_parts)
    parts += bow
    hull = join(parts, "SM_Barque")
    oar = join(build_oar(c_oar), "SM_Barque_Oar")
    oar.location = (0, 4.0, 0)  # out of the hull's ambient occlusion during the bake
    bpy.ops.object.select_all(action="DESELECT")
    oar.select_set(True)
    bpy.context.view_layer.objects.active = oar
    bpy.ops.object.transform_apply(location=True)
    ucx = build_collisions(c_col)
    socks = make_sockets(hull, c_col, lantern, flag, tiller, moor, oarlocks)

    unwrap([hull, oar])
    tex = bake([hull, oar]) if a.bake else None
    if tex:
        mat = baked_material(tex)
        assign_single(hull, mat)
        assign_single(oar, mat)
    for v in oar.data.vertices:  # back to its pivot (oarlock contact) at the origin
        v.co.y -= 4.0

    dims = hull.dimensions
    print(f"[barque] SM_Barque {dims.x:.2f} x {dims.y:.2f} x {dims.z:.2f} m, {tri_count(hull)} tris, "
          f"{len(hull.data.materials)} material slot(s), {len(ucx)} UCX, {len(socks)} sockets; "
          f"SM_Barque_Oar {tri_count(oar)} tris")

    if a.export:
        if not tex:
            print("[barque] WARNING: exporting without --bake: material slots are the procedural ones")
        export_fbx([hull] + ucx + socks, EXPORT / "SM_Barque.fbx")
        oar.location = (0, 0, 0)
        export_fbx([oar], EXPORT / "SM_Barque_Oar.fbx")
        register_manifest({
            "SM_Barque": {"folder": UE_MESHES, "fbx": str(EXPORT / "SM_Barque.fbx").replace("\\", "/"),
                          "slots": ["MI_Barque"], "foliage": False, "ucx": len(ucx), "group": "boat",
                          "size_m": [round(dims.x, 2), round(dims.y, 2), round(dims.z, 2)],
                          "verts": len(hull.data.vertices), "sockets": [s.name for s in socks],
                          "textures": ["T_Barque_D", "T_Barque_ORM", "T_Barque_N"],
                          "pivot": "centerline, designed waterline (z = 0)"},
            "SM_Barque_Oar": {"folder": UE_MESHES, "fbx": str(EXPORT / "SM_Barque_Oar.fbx").replace("\\", "/"),
                              "slots": ["MI_Barque"], "foliage": False, "ucx": 0, "group": "boat",
                              "size_m": [round(v, 2) for v in oar.dimensions], "verts": len(oar.data.vertices),
                              "pivot": "oarlock contact"},
        })

    if a.render:
        preview_props(c_ref)
        for tag, side in (("L", 1), ("R", -1)):  # oars shipped in the oarlocks (pivot = SOCKET_Oarlock_*)
            o = bpy.data.objects.new(f"REF_Oar_{tag}", oar.data)
            o.location = oarlocks[tag]
            o.rotation_euler = (0.0, math.radians(14), side * math.radians(80))
            c_ref.objects.link(o)
        oar.hide_render = True
        water = water_plane(c_ref)
        cam = setup_render(scene)
        render_views(scene, cam, water)
    if not a.no_save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        print("[barque] saved", BLEND)


if __name__ == "__main__":
    main()
