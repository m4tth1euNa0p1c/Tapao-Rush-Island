"""Statuette « Relique des Marées » (SM_TidalRelic) : petit tiki en basalte porté à deux mains (touche F, 10 kg côté UE).

Planche : sprite/relique-marees.png (face, profil, dos, dessus, incrustation, relief).
Procédural et reproductible : formes additives (superellipsoïdes, capsules, tore du chaton) fusionnées en un seul
bloc de pierre par remaillage voxel, puis gravures (yeux en amande, bouche, doigts, spirales koru et chevrons du dos
et des côtés) par déplacement le long des normales, puis décimation au budget. Gemme turquoise et gouttes de nacre
sont des pièces séparées jointes au bloc (3 emplacements de matériau).

Unités : mètres, +Z en haut, FACE VERS +X (= +X dans UE, même convention que barque.py / shark.py),
origine au centre de la base (dessous du socle, z = 0). Hauteur 0,40 m, largeur (Y) ~0,24 m, profondeur (X) ~0,20 m.
Matériaux : M_Relic_Stone (basalte), M_Relic_Gem (turquoise), M_Relic_Nacre (nacre).
Collision : UCX_SM_TidalRelic_00 (enveloppe convexe simplifiée) dans le même FBX.

  blender --background --factory-startup --python scripts/props/tidal_relic.py -- [--export] [--render] [--no-save]
Sorties : art/blender/props/tidal_relic.blend, art/export/Props/SM_TidalRelic.fbx,
art/_previews/tidal_relic.png (trois-quarts face), art/_previews/tidal_relic_views.png (face / profil / dos).
"""
import argparse
import math
import sys
from pathlib import Path

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ART = Path(__file__).resolve().parents[2] / "art"
BLEND = ART / "blender" / "props" / "tidal_relic.blend"
FBX = ART / "export" / "Props" / "SM_TidalRelic.fbx"
PREVIEW = ART / "_previews" / "tidal_relic.png"
PREVIEW_VIEWS = ART / "_previews" / "tidal_relic_views.png"

NAME = "SM_TidalRelic"
VOXEL = 0.0026          # remesh resolution (m): fine enough for 2-3 mm reliefs before decimation
STONE_TRIS = 16500      # triangle budget of the stone block (gem + nacre add ~1.5k)
RELIEF = 0.0026         # height of the koru / chevron reliefs (m)

# linear RGBA, readable as flat colours
COLORS = {
    "Stone": (0.055, 0.055, 0.058, 1.0),
    "Gem": (0.0, 0.42, 0.50, 1.0),
    "Nacre": (0.86, 0.87, 0.84, 1.0),
}

# ---------------------------------------------------------------- primitives


def spow(v, p):
    return math.copysign(abs(v) ** p, v)


def blob(bm, c, r, p=1.0, pz=None, rot=None, segs=28, rings=18):
    """Closed superellipsoid: p = 1 ellipsoid, p < 1 rounded box (pz: exponent of the vertical profile)."""
    pz = p if pz is None else pz
    m = rot.to_3x3() if rot is not None else Matrix.Identity(3)
    c = Vector(c)
    grid = []
    for i in range(1, rings):
        th = -math.pi / 2 + math.pi * i / rings
        row = []
        for k in range(segs):
            ph = 2 * math.pi * k / segs
            ct = spow(math.cos(th), pz)
            v = Vector((r[0] * ct * spow(math.cos(ph), p), r[1] * ct * spow(math.sin(ph), p),
                        r[2] * spow(math.sin(th), pz)))
            row.append(bm.verts.new(c + m @ v))
        grid.append(row)
    for a, b in zip(grid, grid[1:]):
        for k in range(segs):
            bm.faces.new((a[k], a[(k + 1) % segs], b[(k + 1) % segs], b[k]))
    bot = bm.verts.new(c + m @ Vector((0, 0, -r[2])))
    top = bm.verts.new(c + m @ Vector((0, 0, r[2])))
    for k in range(segs):
        bm.faces.new((grid[0][(k + 1) % segs], grid[0][k], bot))
        bm.faces.new((grid[-1][k], grid[-1][(k + 1) % segs], top))


def capsule(bm, a, b, r0, r1=None, p=0.9):
    """Rounded limb between a and b (superellipsoid stretched along the segment)."""
    r1 = r0 if r1 is None else r1
    a, b = Vector(a), Vector(b)
    d = b - a
    rot = d.to_track_quat("Z", "Y").to_matrix().to_4x4()
    rr = (r0 + r1) / 2
    blob(bm, (a + b) / 2, (rr, rr, d.length / 2 + rr * 0.8), p=p, pz=0.8, rot=rot, segs=20, rings=14)


def torus(bm, c, ry, rz, minor, segs=40, tube=10):
    """Oval ring facing +X (the gem's bezel)."""
    c = Vector(c)
    rings = []
    for i in range(segs):
        a = 2 * math.pi * i / segs
        ring = []
        for j in range(tube):
            b = 2 * math.pi * j / tube
            rad = 1 + minor * math.cos(b) / max(ry, rz)
            ring.append(bm.verts.new(c + Vector((minor * math.sin(b), ry * rad * math.cos(a), rz * rad * math.sin(a)))))
        rings.append(ring)
    for i in range(segs):
        r0, r1 = rings[i], rings[(i + 1) % segs]
        for j in range(tube):
            bm.faces.new((r0[j], r0[(j + 1) % tube], r1[(j + 1) % tube], r1[j]))


# ---------------------------------------------------------------- stone block (additive shapes)

GEM_C = (0.093, 0.0, 0.168)     # centre of the turquoise inlay (on the belly)


def build_stone_source():
    bm = bmesh.new()
    # socle: plinth + two foot blocks
    blob(bm, (0.004, 0, 0.012), (0.098, 0.117, 0.013), p=0.25, pz=0.4)
    for s in (1, -1):
        blob(bm, (0.022, s * 0.057, 0.030), (0.082, 0.055, 0.020), p=0.35, pz=0.5)
    # flexed legs: thighs forward, knees, shins down to the feet
    for s in (1, -1):
        blob(bm, (0.025, s * 0.056, 0.080), (0.072, 0.054, 0.042), p=0.65)
        blob(bm, (0.060, s * 0.056, 0.055), (0.040, 0.048, 0.040), p=0.7)
    # back column: the whole back is one flat slab (profile view), rounded at the top
    blob(bm, (-0.045, 0, 0.170), (0.048, 0.106, 0.170), p=0.3, pz=0.35)
    # torso + belly
    blob(bm, (0.000, 0, 0.170), (0.080, 0.094, 0.070), p=0.6)
    blob(bm, (0.040, 0, 0.165), (0.052, 0.072, 0.058), p=0.75)
    # arms: shoulders, upper arms down the sides, forearms folded toward the inlay, hands
    for s in (1, -1):
        blob(bm, (-0.005, s * 0.090, 0.205), (0.040, 0.026, 0.030), p=0.8)
        capsule(bm, (-0.005, s * 0.094, 0.200), (0.005, s * 0.095, 0.140), 0.025)
        capsule(bm, (0.010, s * 0.092, 0.135), (0.066, s * 0.064, 0.150), 0.023)
        blob(bm, (0.074, s * 0.054, 0.152), (0.022, 0.024, 0.026), p=0.7)
    # head: massive rounded block, headband, crown with three crenels
    blob(bm, (0.002, 0, 0.300), (0.084, 0.104, 0.078), p=0.35, pz=0.3)
    blob(bm, (0.008, 0, 0.338), (0.080, 0.104, 0.009), p=0.3, pz=0.6)
    for y, w in ((-0.069, 0.034), (0.0, 0.020), (0.069, 0.034)):
        blob(bm, (0.000, y, 0.374), (0.074, w, 0.026), p=0.25, pz=0.3)
    # face: brow ridge, nose, cheeks, pouting lips, chin
    blob(bm, (0.074, 0, 0.320), (0.022, 0.090, 0.012), p=0.7)
    capsule(bm, (0.080, 0, 0.314), (0.097, 0, 0.272), 0.012, p=0.7)
    blob(bm, (0.092, 0, 0.266), (0.021, 0.032, 0.015), p=0.8)
    for s in (1, -1):
        blob(bm, (0.066, s * 0.052, 0.268), (0.024, 0.034, 0.030), p=1.0)
    blob(bm, (0.087, 0, 0.245), (0.014, 0.036, 0.008), p=0.9)
    blob(bm, (0.087, 0, 0.231), (0.016, 0.030, 0.010), p=0.9)
    blob(bm, (0.070, 0, 0.222), (0.020, 0.050, 0.015), p=0.9)
    # bezel around the gem
    torus(bm, (GEM_C[0] - 0.002, 0, GEM_C[2]), 0.026, 0.034, 0.006)
    blob(bm, (GEM_C[0] - 0.012, 0, GEM_C[2]), (0.012, 0.029, 0.037), p=0.9)
    mesh = bpy.data.meshes.new(NAME)
    bm.to_mesh(mesh)
    bm.free()
    ob = bpy.data.objects.new(NAME, mesh)
    bpy.context.scene.collection.objects.link(ob)
    return ob


# ---------------------------------------------------------------- carvings (displacement along normals)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def ridge(dist, half):
    """1 on a line of half-width `half`, soft 30 % shoulder."""
    return 1.0 - smooth(half, half * 1.6, np.abs(dist))


def koru(u, v, cu, cv, rmax, pitch, half):
    """Archimedean spiral ridge (koru) around (cu, cv), with a central bud."""
    du, dv = u - cu, v - cv
    r = np.hypot(du, dv)
    th = np.arctan2(dv, du)
    ph = r / pitch - th / (2 * np.pi)
    d = (ph - np.floor(ph) - 0.5) * pitch
    line = ridge(d, half) * (1 - smooth(rmax - pitch * 0.3, rmax, r))
    bud = 1 - smooth(pitch * 0.35, pitch * 0.55, r)
    return np.maximum(line, bud)


def chevrons(u, v, period, slope, half):
    """Stacked V chevrons pointing down (back of the statue)."""
    s = v - slope * np.abs(u)
    d = (s / period - np.floor(s / period) - 0.5) * period
    return ridge(d, half)


def almond(u, v, cu, cv, a, b):
    """Signed-ish almond (vesica) field: < 1 inside, 1 on the outline."""
    x = (u - cu) / a
    y = (v - cv) / b
    return np.abs(y) / np.maximum(1e-4, 1 - x * x) ** 0.85 + 0 * x + np.where(np.abs(x) >= 1, 9.0, 0.0)


def carve(ob):
    me = ob.data
    n = len(me.vertices)
    co = np.empty(n * 3)
    no = np.empty(n * 3)
    me.vertices.foreach_get("co", co)
    me.attributes["normal"].data.foreach_get("vector", no) if "normal" in me.attributes else \
        me.vertices.foreach_get("normal", no)
    co = co.reshape(-1, 3)
    no = no.reshape(-1, 3)
    x, y, z = co[:, 0], co[:, 1], co[:, 2]
    nx, ny = no[:, 0], no[:, 1]
    disp = np.zeros(n)

    # --- face: almond eyes (sunken lid with a raised rim and a domed eyeball), mouth slit, nostrils
    front = smooth(0.25, 0.55, nx) * (z > 0.20) * (z < 0.34)
    for s in (1, -1):
        f = almond(y, z, s * 0.046, 0.296, 0.042, 0.020)
        lid = 1 - smooth(0.88, 1.0, f)                 # inside the almond: deep socket
        rim = smooth(0.98, 1.08, f) * (1 - smooth(1.3, 1.6, f))
        ball = 1 - smooth(0.45, 0.72, f)               # flat eyeball, ringed by the lid groove
        disp += front * (-0.0095 * lid + 0.0028 * rim + 0.0040 * ball)
    # pouting mouth: slit with down-turned corners
    mouth = (1 - smooth(0.0012, 0.0024, np.abs(z - (0.2385 - 5.0 * y * y)))) * (1 - smooth(0.028, 0.036, np.abs(y)))
    disp += front * -0.004 * mouth
    for s in (1, -1):
        nos = 1 - smooth(0.004, 0.007, np.hypot(y - s * 0.017, z - 0.259))
        disp += front * -0.004 * nos

    # --- fingers: three horizontal grooves on each hand (front)
    for s in (1, -1):
        near = 1 - smooth(0.022, 0.030, np.hypot(np.hypot(x - 0.074, y - s * 0.054), z - 0.152))
        g = ((z - 0.152) / 0.0105) % 1.0
        disp += smooth(0.2, 0.5, nx) * near * -0.0022 * (np.abs(g - 0.5) > 0.38)

    # --- back: chevrons on the trunk, horizontal bands + twin koru under the crown, koru pair low down
    back = smooth(0.45, 0.75, -nx)
    rel = np.zeros(n)
    trunk = (z > 0.105) * (z < 0.255) * (np.abs(y) < 0.088)
    rel = np.maximum(rel, chevrons(y, z, 0.022, 0.55, 0.0032) * trunk)
    bands = (z > 0.272) * (z < 0.332) * (np.abs(y) < 0.092)
    bd = ((z - 0.272) / 0.020) % 1.0
    band_line = ridge((bd - 0.5) * 0.020, 0.0028) * bands * (np.abs(y) > 0.040)
    rel = np.maximum(rel, band_line)
    for s in (1, -1):
        rel = np.maximum(rel, koru(y, z, s * 0.020, 0.302, 0.026, 0.0085, 0.0024))
        rel = np.maximum(rel, koru(y, z, s * 0.050, 0.070, 0.030, 0.0085, 0.0024) * (z < 0.10))
    disp += back * RELIEF * rel

    # --- sides: koru on the temples, koru on the shoulders, chevrons along the haunches
    side = smooth(0.5, 0.8, np.abs(ny))
    rel = np.zeros(n)
    rel = np.maximum(rel, koru(x, z, -0.012, 0.292, 0.040, 0.0095, 0.0026) * (z > 0.24))
    rel = np.maximum(rel, koru(x, z, -0.010, 0.168, 0.036, 0.0090, 0.0025) * (z > 0.12) * (z < 0.215))
    haunch = (z > 0.050) * (z < 0.115) * (x < 0.05)
    rel = np.maximum(rel, chevrons(z - 0.075, x, 0.020, 0.6, 0.0030) * haunch)
    disp += side * RELIEF * rel

    co += no * disp[:, None]
    me.vertices.foreach_set("co", co.ravel())
    me.update()


# ---------------------------------------------------------------- inlays


def build_inlays():
    bm = bmesh.new()
    blob(bm, (GEM_C[0] + 0.001, 0, GEM_C[2]), (0.012, 0.0205, 0.0285), p=1.0, segs=32, rings=18)
    gem = bm_object(bm, "Gem")
    bm = bmesh.new()
    for s in (1, -1):
        # teardrop: point upward, slightly tilted outward
        blob(bm, (GEM_C[0] - 0.006, s * 0.0385, GEM_C[2] + 0.006), (0.0065, 0.0082, 0.0105), p=1.0,
             segs=18, rings=12)
    for v in bm.verts:  # narrow the top into a rounded drop
        h = v.co.z - (GEM_C[2] + 0.006)
        if h > 0:
            k = 1 - 0.45 * min(1.0, h / 0.0105) ** 1.5
            cy = math.copysign(0.0385, v.co.y)
            v.co.y = cy + (v.co.y - cy) * k
            v.co.x = (GEM_C[0] - 0.006) + (v.co.x - (GEM_C[0] - 0.006)) * k
    nacre = bm_object(bm, "Nacre")
    return gem, nacre


def bm_object(bm, name):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


# ---------------------------------------------------------------- materials


def material(name, rgba, rough, extra=None):
    mat = bpy.data.materials.new(f"M_Relic_{name}")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(nd for nd in nt.nodes if nd.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = rgba
    bsdf.inputs["Roughness"].default_value = rough
    mat.diffuse_color = rgba
    if extra:
        extra(nt, bsdf)
    return mat


def stone_detail(nt, bsdf):
    """Basalt porosity for the previews: fine noise -> colour speckle + bump pits (UE rebuilds its own material)."""
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 420.0
    noise.inputs["Detail"].default_value = 4.0
    pits = nt.nodes.new("ShaderNodeTexVoronoi")
    pits.inputs["Scale"].default_value = 140.0
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[1].position = 0.12
    nt.links.new(pits.outputs["Distance"], ramp.inputs["Fac"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value = (0.020, 0.020, 0.022, 1)
    mix.inputs["B"].default_value = (0.075, 0.073, 0.070, 1)
    nt.links.new(noise.outputs["Fac"], mix.inputs["Factor"])
    nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.9
    bump.inputs["Distance"].default_value = 0.002
    nt.links.new(ramp.outputs["Color"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])


def gem_detail(nt, bsdf):
    if "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = 0.6
        bsdf.inputs["Coat Roughness"].default_value = 0.05


def nacre_detail(nt, bsdf):
    if "Thin Film Thickness" in bsdf.inputs:
        bsdf.inputs["Thin Film Thickness"].default_value = 420.0
    if "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = 0.5


# ---------------------------------------------------------------- assembly


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def apply_modifier(ob, mod):
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=mod.name)


def build():
    stone = build_stone_source()
    rm = stone.modifiers.new("Remesh", "REMESH")
    rm.mode = "VOXEL"
    rm.voxel_size = VOXEL
    rm.use_smooth_shade = True
    apply_modifier(stone, rm)
    sm = stone.modifiers.new("Smooth", "SMOOTH")   # soften the voxel steps before carving
    sm.factor, sm.iterations = 0.5, 2
    apply_modifier(stone, sm)
    carve(stone)
    dense = tri_count(stone)
    dc = stone.modifiers.new("Decimate", "DECIMATE")
    dc.decimate_type = "COLLAPSE"
    dc.ratio = min(1.0, STONE_TRIS / max(1, dense))
    apply_modifier(stone, dc)
    print(f"[relic] stone: {dense} -> {tri_count(stone)} tris")

    gem, nacre = build_inlays()
    mats = [material("Stone", COLORS["Stone"], 0.88, stone_detail),
            material("Gem", COLORS["Gem"], 0.18, gem_detail),
            material("Nacre", COLORS["Nacre"], 0.22, nacre_detail)]
    for ob, idx in ((stone, 0), (gem, 1), (nacre, 2)):
        for m in mats:
            ob.data.materials.append(m)
        for p in ob.data.polygons:
            p.material_index = idx
            p.use_smooth = True
    bpy.ops.object.select_all(action="DESELECT")
    for ob in (stone, gem, nacre):
        ob.select_set(True)
    bpy.context.view_layer.objects.active = stone
    bpy.ops.object.join()

    # origin at the centre of the base: centre X/Y of the footprint, bottom at z = 0, flat underside
    me = stone.data
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    lo, hi = co.min(0), co.max(0)
    co -= np.array([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]])
    co[:, 2] = np.where(co[:, 2] < 0.0015, 0.0, co[:, 2])
    me.vertices.foreach_set("co", co.ravel())
    me.update()

    # UVs (box-ish smart projection, for UE triplanar / later bakes)
    bpy.context.view_layer.objects.active = stone
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.003)
    bpy.ops.object.mode_set(mode="OBJECT")
    return stone


def build_collision(stone):
    """One simple convex hull around the body: extreme points of 7 horizontal slices in 8 directions."""
    co = np.empty(len(stone.data.vertices) * 3)
    stone.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    top = co[:, 2].max()
    edges = np.linspace(0.0, top, 8)
    dirs = [(math.cos(a), math.sin(a)) for a in np.linspace(0, 2 * math.pi, 8, endpoint=False)]
    bm = bmesh.new()
    for z0, z1 in zip(edges, edges[1:]):
        sl = co[(co[:, 2] >= z0 - 1e-4) & (co[:, 2] <= z1 + 1e-4)]
        for dx, dy in dirs:
            p = sl[np.argmax(sl[:, 0] * dx + sl[:, 1] * dy)]
            for z in (z0, z1):
                bm.verts.new((p[0] * 1.01, p[1] * 1.01, z))
    res = bmesh.ops.convex_hull(bm, input=bm.verts, use_existing_faces=False)
    bmesh.ops.delete(bm, geom=list(set(res["geom_interior"]) | set(res["geom_unused"])), context="VERTS")
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.004)
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(4), verts=bm.verts, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    me = bpy.data.meshes.new(f"UCX_{NAME}_00")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(f"UCX_{NAME}_00", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.display_type = "WIRE"
    ob.hide_render = True
    return ob


def export(objs):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    FBX.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(FBX), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_tspace=True,
                             add_leaf_bones=False, bake_anim=False)
    print("[relic] export:", FBX)


# ---------------------------------------------------------------- previews


def setup_render(scene):
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT", "BLENDER_WORKBENCH"):
        try:
            scene.render.engine = eng
            break
        except TypeError:
            continue
    scene.view_settings.view_transform = "AgX" if "AgX" in [
        i.identifier for i in scene.view_settings.bl_rna.properties["view_transform"].enum_items] else \
        scene.view_settings.view_transform
    world = bpy.data.worlds.new("W_Studio")
    world.use_nodes = True
    bg = next(nd for nd in world.node_tree.nodes if nd.type == "BACKGROUND")
    bg.inputs[0].default_value = (0.16, 0.22, 0.32, 1)
    bg.inputs[1].default_value = 0.9
    scene.world = world
    for name, rot, energy in (("Key", (0.9, 0.15, 0.6), 4.5), ("Fill", (1.1, 0.0, -1.0), 1.4),
                              ("Rim", (1.2, 0.0, 3.6), 3.0)):
        ld = bpy.data.lights.new(name, "SUN")
        ld.energy = energy
        ld.angle = math.radians(8)
        lo = bpy.data.objects.new(name, ld)
        lo.rotation_euler = rot
        scene.collection.objects.link(lo)
    ground = bpy.data.meshes.new("REF_Ground")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=3.0)
    bm.to_mesh(ground)
    bm.free()
    g = bpy.data.objects.new("REF_Ground", ground)
    g.data.materials.append(material("Ground", (0.09, 0.11, 0.15, 1), 0.7))
    scene.collection.objects.link(g)
    target = bpy.data.objects.new("CAM_Target", None)
    scene.collection.objects.link(target)
    cam = bpy.data.objects.new("CAM_Relic", bpy.data.cameras.new("CAM_Relic"))
    scene.collection.objects.link(cam)
    con = cam.constraints.new("TRACK_TO")
    con.target = target
    con.track_axis = "TRACK_NEGATIVE_Z"
    con.up_axis = "UP_Y"
    scene.camera = cam
    return cam, target


def render(stone):
    scene = bpy.context.scene
    cam, target = setup_render(scene)
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    # three-quarter front view (face toward +X)
    scene.render.resolution_x, scene.render.resolution_y = 1000, 1100
    cam.data.type = "PERSP"
    cam.data.lens = 85
    cam.location = (1.15, -0.85, 0.55)
    target.location = (0, 0, 0.19)
    scene.render.filepath = str(PREVIEW)
    bpy.ops.render.render(write_still=True)
    print("[relic] preview:", PREVIEW)
    # face / profile / back side by side (camera on +X looking -X: screen right = +Y)
    copies = []
    for k, (dy, rz) in enumerate(((-0.34, 0.0), (0.0, math.pi / 2), (0.34, math.pi))):
        ob = stone if k == 0 else bpy.data.objects.new(f"REF_View_{k}", stone.data)
        if k:
            scene.collection.objects.link(ob)
            copies.append(ob)
        ob.location = (0, dy, 0)
        ob.rotation_euler = (0, 0, rz)
    scene.render.resolution_x, scene.render.resolution_y = 1500, 700
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.05
    cam.location = (3.0, 0, 0.2)
    target.location = (0, 0, 0.2)
    scene.render.filepath = str(PREVIEW_VIEWS)
    bpy.ops.render.render(write_still=True)
    print("[relic] views:", PREVIEW_VIEWS)
    for ob in copies:
        bpy.data.objects.remove(ob)
    stone.location = (0, 0, 0)
    stone.rotation_euler = (0, 0, 0)
    for name in ("REF_Ground",):
        bpy.data.objects.remove(bpy.data.objects[name])


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    a = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    stone = build()
    ucx = build_collision(stone)
    d = stone.dimensions
    print(f"[relic] {NAME}: {d.x:.3f} x {d.y:.3f} x {d.z:.3f} m (X depth, Y width, Z height), "
          f"{tri_count(stone)} tris, {len(stone.data.materials)} material slots; "
          f"{ucx.name}: {len(ucx.data.vertices)} verts")
    if a.render:
        render(stone)
    if a.export:
        export([stone, ucx])
    if not a.no_save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        print("[relic] saved", BLEND)


if __name__ == "__main__":
    main()
