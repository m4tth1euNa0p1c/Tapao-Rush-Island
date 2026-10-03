"""Geometry and materials of the minka.

Every mesh is built in world space (meters, +Z up, facade towards -Y, origin at the front-left corner of the main
body at ground level) and gets world-scaled UVs, so the CC0 photo textures tile at their real size and the
materials stay plain Principled BSDF + image textures: they export as-is to glTF and FBX (no procedural node to
bake). Colour variants of a texture are derived once with PIL (art/textures/props/minka/_derived).
"""
import math
import random
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[3]
TEX = ROOT / "art" / "textures"
MINKA_TEX = TEX / "props" / "minka"
DERIVED = MINKA_TEX / "_derived"
TAU = 2 * math.pi


# ---------------------------------------------------------------- colours, images

def srgb(hexa, a=1.0):
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [a]


def find(folder, *keys):
    files = sorted(Path(folder).glob("*"))
    for k in keys:
        for f in files:
            if k in f.name.lower():
                return f
    return None


def derived(src, name, mul=(1.0, 1.0, 1.0), add=(0.0, 0.0, 0.0), sat=1.0):
    """Tinted copy of a colour texture (kept as a real image so glTF / FBX keep the look)."""
    from PIL import Image, ImageEnhance
    DERIVED.mkdir(parents=True, exist_ok=True)
    dst = DERIVED / f"{name}.jpg"
    if dst.exists() and dst.stat().st_mtime > Path(src).stat().st_mtime:
        return dst
    im = Image.open(src).convert("RGB")
    if sat != 1.0:
        im = ImageEnhance.Color(im).enhance(sat)
    r, g, b = im.split()
    r = r.point(lambda v: max(0, min(255, int(v * mul[0] + add[0] * 255))))
    g = g.point(lambda v: max(0, min(255, int(v * mul[1] + add[1] * 255))))
    b = b.point(lambda v: max(0, min(255, int(v * mul[2] + add[2] * 255))))
    Image.merge("RGB", (r, g, b)).save(dst, quality=92)
    return dst


def image(path, colorspace="sRGB"):
    img = bpy.data.images.load(str(path), check_existing=True)
    try:
        img.colorspace_settings.name = colorspace
    except TypeError:
        pass
    return img


# ---------------------------------------------------------------- materials

def _bsdf(mat):
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    return nt, bsdf


def _inp(bsdf, *names):
    for n in names:
        if n in bsdf.inputs:
            return bsdf.inputs[n]
    raise KeyError(names)


def img_material(name, color, normal=None, rough=None, rough_value=0.7, metal=0.0, tint_hex=None):
    """Principled BSDF fed by image textures on the UV map (export friendly)."""
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    nt, bsdf = _bsdf(mat)
    x = -700
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = image(color, "sRGB")
    tex.location = (x, 300)
    nt.links.new(tex.outputs["Color"], _inp(bsdf, "Base Color"))
    if rough:
        tr = nt.nodes.new("ShaderNodeTexImage")
        tr.image = image(rough, "Non-Color")
        tr.location = (x, 0)
        nt.links.new(tr.outputs["Color"], _inp(bsdf, "Roughness"))
    else:
        _inp(bsdf, "Roughness").default_value = rough_value
    _inp(bsdf, "Metallic").default_value = metal
    if normal:
        tn = nt.nodes.new("ShaderNodeTexImage")
        tn.image = image(normal, "Non-Color")
        tn.location = (x, -300)
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nm.location = (x + 300, -300)
        nt.links.new(tn.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], _inp(bsdf, "Normal"))
    mat.diffuse_color = srgb(tint_hex) if tint_hex else (0.6, 0.55, 0.5, 1)
    return mat


def flat_material(name, hexa, rough=0.6, metal=0.0, normal=None, normal_strength=0.6, transmission=0.0,
                  emission=None, emission_strength=0.0, alpha=1.0):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    nt, bsdf = _bsdf(mat)
    _inp(bsdf, "Base Color").default_value = srgb(hexa)
    _inp(bsdf, "Roughness").default_value = rough
    _inp(bsdf, "Metallic").default_value = metal
    if transmission:
        for key in ("Transmission Weight", "Transmission"):
            if key in bsdf.inputs:
                bsdf.inputs[key].default_value = transmission
                break
    if emission:
        _inp(bsdf, "Emission Color", "Emission").default_value = srgb(emission)
        _inp(bsdf, "Emission Strength").default_value = emission_strength
    if alpha < 1.0:
        _inp(bsdf, "Alpha").default_value = alpha
        for attr, val in (("surface_render_method", "DITHERED"), ("blend_method", "HASHED")):
            if hasattr(mat, attr):
                try:
                    setattr(mat, attr, val)
                except TypeError:
                    pass
    if normal:
        tn = nt.nodes.new("ShaderNodeTexImage")
        tn.image = image(normal, "Non-Color")
        tn.location = (-700, -300)
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nm.inputs["Strength"].default_value = normal_strength
        nm.location = (-400, -300)
        nt.links.new(tn.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], _inp(bsdf, "Normal"))
    mat.diffuse_color = srgb(hexa)
    return mat


def materials():
    """All minka materials, keyed by short name."""
    M = {}

    def ph(asset, res):
        return MINKA_TEX / asset

    def tex(folder):
        return (find(folder, "_diff_"), find(folder, "_nor_gl_"), find(folder, "_rough_"))

    linen_n = find(ph("rough_linen", "1k"), "_nor_gl_")
    c, n, r = tex(ph("reed_roof_04", "2k"))
    M["Thatch"] = img_material("M_Minka_Thatch", c, n, r, tint_hex="A88A52")
    c, n, r = tex(ph("thatch_roof_angled", "1k"))
    M["ThatchEdge"] = img_material("M_Minka_ThatchEdge", derived(c, "thatch_edge_warm", (1.12, 1.0, 0.82)), n, r,
                                   tint_hex="7A6440")
    wood = TEX / "characters" / "rough_wood"
    c, n, r = tex(wood)
    M["Timber"] = img_material("M_Minka_Timber", derived(c, "timber_dark", (0.62, 0.55, 0.50)), n, r,
                               tint_hex="3A2A1E")
    c, n, r = tex(ph("patterned_clay_plaster", "2k"))
    M["Plaster"] = img_material("M_Minka_Plaster", derived(c, "plaster_ivory", (1.10, 1.07, 1.0), (0.04, 0.04, 0.03),
                                                           sat=0.6), n, r, tint_hex="E6DCC6")
    c, n, r = tex(ph("clay_plaster", "1k"))
    M["Clay"] = img_material("M_Minka_Clay", c, n, r, tint_hex="9A7A58")
    c, n, r = tex(ph("dirt_floor", "2k"))
    M["Earth"] = img_material("M_Minka_Earth", c, n, r, tint_hex="7A6248")
    c, n, r = tex(ph("hinoki_planks", "2k"))
    M["FloorWood"] = img_material("M_Minka_FloorWood", derived(c, "hinoki_honey", (0.86, 0.74, 0.58)), n, r,
                                  tint_hex="B08850")
    c, n, r = tex(ph("weathered_brown_planks", "1k"))
    M["Engawa"] = img_material("M_Minka_Engawa", c, n, r, tint_hex="6E5440")
    c, n, r = tex(ph("japanese_cedar_planks", "1k"))
    M["Cedar"] = img_material("M_Minka_Cedar", derived(c, "cedar_aged", (0.72, 0.60, 0.50), sat=0.75), n, r,
                              tint_hex="7A5434")
    c, n, r = tex(ph("mossy_rock", "1k"))
    M["Stone"] = img_material("M_Minka_Stone", c, n, r, tint_hex="6A6A5E")
    c, n, r = tex(ph("tatami_mat", "2k"))
    M["Tatami"] = img_material("M_Minka_Tatami", c, n, r, tint_hex="A49A6A")
    c, n, r = tex(TEX / "characters" / "bamboo_veneer")
    M["Bamboo"] = img_material("M_Minka_Bamboo", c, n, r, tint_hex="B8945A")
    M["Washi"] = flat_material("M_Minka_Washi", "F2ECDC", rough=0.85, normal=linen_n, normal_strength=0.25,
                               transmission=0.35)
    M["Fusuma"] = flat_material("M_Minka_Fusuma", "E4D8BC", rough=0.8, normal=linen_n, normal_strength=0.3)
    M["Lacquer"] = flat_material("M_Minka_Lacquer", "2A1C14", rough=0.35)
    M["Heri"] = flat_material("M_Minka_TatamiBorder", "2E3524", rough=0.7, normal=linen_n, normal_strength=0.5)
    M["Iron"] = flat_material("M_Minka_Iron", "2A2826", rough=0.55, metal=0.7)
    M["Ash"] = flat_material("M_Minka_Ash", "8C867E", rough=1.0)
    M["Charcoal"] = flat_material("M_Minka_Charcoal", "141210", rough=0.9)
    M["Ember"] = flat_material("M_Minka_Ember", "2A0C04", rough=0.9, emission="FF5A1A", emission_strength=3.0)
    M["Olive"] = flat_material("M_Minka_CushionOlive", "6B7240", rough=0.85, normal=linen_n, normal_strength=0.6)
    M["FutonCream"] = flat_material("M_Minka_FutonCream", "E6DCC4", rough=0.9, normal=linen_n, normal_strength=0.5)
    M["FutonIndigo"] = flat_material("M_Minka_FutonIndigo", "2C3650", rough=0.9, normal=linen_n, normal_strength=0.5)
    M["Rope"] = flat_material("M_Minka_Rope", "4A3A26", rough=0.95)
    M["Ceramic"] = flat_material("M_Minka_Ceramic", "5A4A3C", rough=0.35)
    M["Lantern"] = flat_material("M_Minka_LanternGlow", "F4E6C8", rough=0.8, emission="FFC27A", emission_strength=4.0)
    M["Leaf"] = flat_material("M_Minka_Leaf", "4E6A2E", rough=0.6)
    M["Flower"] = flat_material("M_Minka_Flower", "E8D8E0", rough=0.6)
    return M


# ---------------------------------------------------------------- UVs

def uv_layer(bm):
    return bm.loops.layers.uv.verify()


def box_uv(faces, uvl, tile=1.0, swap=False, offset=(0.0, 0.0)):
    """World box mapping: each face projected on the plane of its dominant normal axis, `tile` meters per UV unit.
    swap rotates the pattern 90 degrees (planks along Y instead of X on floors, vertical grain on posts)."""
    for f in faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            p = loop.vert.co
            if ax == 2:
                u, v = p.x, p.y
            elif ax == 0:
                u, v = p.y, p.z
            else:
                u, v = p.x, p.z
            if swap:
                u, v = v, u
            loop[uvl].uv = (u / tile + offset[0], v / tile + offset[1])


def slope_uv(faces, uvl, tile=1.6):
    """Roof mapping: v runs down the slope (thatch stems), u along the contour."""
    for f in faces:
        n = f.normal
        down = Vector((0, 0, -1)) - n * (-n.z)
        if down.length < 1e-6:
            down = Vector((0, 1, 0))
        down.normalize()
        side = n.cross(down).normalized()
        for loop in f.loops:
            p = loop.vert.co
            loop[uvl].uv = (p.dot(side) / tile, -p.dot(down) / tile)


# ---------------------------------------------------------------- primitives

def add_box(bm, mn, mx, uvl=None, tile=1.0, swap=False):
    mn, mx = Vector(mn), Vector(mx)
    geom = bmesh.ops.create_cube(bm, size=1.0)
    c = (mn + mx) / 2
    s = mx - mn
    for v in geom["verts"]:
        v.co = Vector((c.x + v.co.x * s.x, c.y + v.co.y * s.y, c.z + v.co.z * s.z))
    faces = list({f for v in geom["verts"] for f in v.link_faces})
    bmesh.ops.recalc_face_normals(bm, faces=faces)
    if uvl is not None:
        box_uv(faces, uvl, tile, swap)
    return faces


def add_box_m(bm, M, size, uvl=None, tile=1.0, swap=False):
    """Box of `size` transformed by matrix M (rotated beams, leaning boards)."""
    geom = bmesh.ops.create_cube(bm, size=1.0)
    for v in geom["verts"]:
        v.co = M @ Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    faces = list({f for v in geom["verts"] for f in v.link_faces})
    bmesh.ops.recalc_face_normals(bm, faces=faces)
    if uvl is not None:
        box_uv(faces, uvl, tile, swap)
    return faces


def add_cyl(bm, p0, p1, r0, r1=None, segs=16, uvl=None, tile=1.0, caps=True, wobble=0.0, seed=0):
    p0, p1 = Vector(p0), Vector(p1)
    r1 = r0 if r1 is None else r1
    ax = p1 - p0
    L = ax.length
    d = ax.normalized()
    a = d.orthogonal().normalized()
    b = d.cross(a)
    rnd = random.Random(seed)
    n_rings = max(2, int(L / 0.25) + 1) if wobble else 2
    rings = []
    for k in range(n_rings):
        t = k / (n_rings - 1)
        c = p0 + ax * t
        if wobble:
            c += (a * rnd.uniform(-1, 1) + b * rnd.uniform(-1, 1)) * wobble * math.sin(math.pi * t)
        r = r0 + (r1 - r0) * t
        rings.append([bm.verts.new(c + r * (math.cos(TAU * i / segs) * a + math.sin(TAU * i / segs) * b))
                      for i in range(segs)])
    faces = []
    for k in range(n_rings - 1):
        for i in range(segs):
            i2 = (i + 1) % segs
            faces.append(bm.faces.new((rings[k][i], rings[k][i2], rings[k + 1][i2], rings[k + 1][i])))
    if caps:
        faces.append(bm.faces.new(rings[0][::-1]))
        faces.append(bm.faces.new(rings[-1]))
    bmesh.ops.recalc_face_normals(bm, faces=faces)
    if uvl is not None:
        # cylindrical: u around, v along
        for f in faces:
            for loop in f.loops:
                p = loop.vert.co - p0
                ang = math.atan2(p.dot(b), p.dot(a))
                loop[uvl].uv = ((ang / TAU) * (TAU * r0) / tile, p.dot(d) / tile)
    return faces


def add_revolve(bm, profile, center, segs=32, uvl=None, tile=1.0):
    """profile: [(r, z)] from bottom to top; closed at r = 0 ends."""
    c = Vector(center)
    rows = []
    for r, z in profile:
        if r < 1e-5:
            v = bm.verts.new(c + Vector((0, 0, z)))
            rows.append([v] * segs)
        else:
            rows.append([bm.verts.new(c + Vector((r * math.cos(TAU * i / segs), r * math.sin(TAU * i / segs), z)))
                         for i in range(segs)])
    faces = []
    for k in range(len(rows) - 1):
        for i in range(segs):
            i2 = (i + 1) % segs
            q = [rows[k][i], rows[k][i2], rows[k + 1][i2], rows[k + 1][i]]
            u = []
            for v in q:
                if v not in u:
                    u.append(v)
            if len(u) >= 3:
                faces.append(bm.faces.new(u))
    bmesh.ops.recalc_face_normals(bm, faces=faces)
    if uvl is not None:
        box_uv(faces, uvl, tile)
    return faces


def add_rock(bm, center, size, seed, uvl=None, tile=0.8, flat_top=None):
    """Irregular footing stone: convex hull of jittered points (flat top at z = flat_top if given)."""
    rnd = random.Random(seed)
    c = Vector(center)
    pts = []
    for i in range(26):
        z = rnd.uniform(-1, 1)
        a = rnd.uniform(0, TAU)
        r = math.sqrt(max(0.0, 1 - z * z)) * rnd.uniform(0.75, 1.0)
        p = Vector((math.cos(a) * r * size[0], math.sin(a) * r * size[1], z * size[2])) + c
        if flat_top is not None:
            p.z = min(p.z, flat_top)
        pts.append(p)
    tmp = bmesh.new()
    vs = [tmp.verts.new(p) for p in pts]
    bmesh.ops.convex_hull(tmp, input=vs)
    for v in [v for v in tmp.verts if not v.link_faces]:
        tmp.verts.remove(v)
    me = bpy.data.meshes.new("_rock")
    tmp.to_mesh(me)
    tmp.free()
    n0 = len(bm.faces)
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
    bm.faces.ensure_lookup_table()
    faces = [bm.faces[i] for i in range(n0, len(bm.faces))]
    if uvl is not None:
        box_uv(faces, uvl, tile)
    return faces


# ---------------------------------------------------------------- objects

def collection(name, parent=None):
    coll = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    par = parent or bpy.context.scene.collection
    if coll.name not in par.children:
        par.children.link(coll)
    return coll


def obj(name, bm, mats, coll, smooth=False, bevel=0.0, origin=None, autosmooth=None):
    me = bpy.data.meshes.new(name)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    for m in (mats if isinstance(mats, (list, tuple)) else [mats]):
        me.materials.append(m)
    for p in me.polygons:
        p.use_smooth = smooth
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    if origin is not None:
        o = Vector(origin)
        me.transform(Matrix.Translation(-o))
        ob.location = o
    if bevel:
        mod = ob.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 1
        mod.limit_method = "ANGLE"
        mod.harden_normals = False
    return ob


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)
