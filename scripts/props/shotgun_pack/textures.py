"""Procedural PBR look of the shotgun pack and its bake to textures (T-247).

Source materials (object space, no UV needed): figured walnut, blued steel with edge wear and scratches, aged
brass, waxed red paper hull (scorched for the spent shell), kraft cardboard. They are baked with Cycles into three
maps per object, on a unique UV layout (Smart UV Project):
  <name>_BaseColor.png  sRGB
  <name>_Normal.png     tangent space, DirectX convention (green down: Blender's OpenGL bake, green flipped)
  <name>_ORM.png        linear: R ambient occlusion, G roughness, B metallic
and the exported material M_<name> carries them in three Image Texture nodes labelled BaseColor / Normal / ORM
(export_pack lists them by label in manifest.materials[].textures).
"""
import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector


# ------------------------------------------------------------------------------------------------ node helpers

def _nt(mat):
    mat.use_nodes = True
    return mat.node_tree


def _bsdf(nt):
    return next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")


def _output(nt):
    return next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")


def _link(nt, a, b):
    nt.links.new(a, b)


def _feed(nt, socket, value):
    """value: a socket (linked) or a constant (default_value)."""
    if isinstance(value, bpy.types.NodeSocket):
        _link(nt, value, socket)
    else:
        socket.default_value = value


def srgb(hexa):
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def _scaled(color, k, sat=1.0):
    r, g, b = color[:3]
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return [min(1.0, max(0.0, (lum + (v - lum) * sat) * k)) for v in (r, g, b)] + [1.0]


def _coords(nt, origin):
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sub = nt.nodes.new("ShaderNodeVectorMath")
    sub.operation = "SUBTRACT"
    _link(nt, tc.outputs["Object"], sub.inputs[0])
    sub.inputs[1].default_value = Vector(origin)
    return sub.outputs["Vector"]


def _mapping(nt, vec, rotation=(0, 0, 0), scale=(1, 1, 1)):
    mp = nt.nodes.new("ShaderNodeMapping")
    _link(nt, vec, mp.inputs["Vector"])
    mp.inputs["Rotation"].default_value = [math.radians(v) for v in rotation]
    mp.inputs["Scale"].default_value = scale
    return mp.outputs["Vector"]


def _noise(nt, vec, scale, detail=2.0, roughness=0.5, distortion=0.0):
    n = nt.nodes.new("ShaderNodeTexNoise")
    _link(nt, vec, n.inputs["Vector"])
    n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = detail
    n.inputs["Roughness"].default_value = roughness
    if "Distortion" in n.inputs:
        n.inputs["Distortion"].default_value = distortion
    return n.outputs["Fac"]


def _wave(nt, vec, scale, distortion, detail, detail_scale=1.0, direction="Z", profile="SIN", wave_type="BANDS"):
    w = nt.nodes.new("ShaderNodeTexWave")
    w.wave_type = wave_type
    if wave_type == "RINGS":
        w.rings_direction = direction
    else:
        w.bands_direction = direction
    w.wave_profile = profile
    _link(nt, vec, w.inputs["Vector"])
    w.inputs["Scale"].default_value = scale
    w.inputs["Distortion"].default_value = distortion
    w.inputs["Detail"].default_value = detail
    w.inputs["Detail Scale"].default_value = detail_scale
    return w.outputs["Fac"]


def _math(nt, op, a, b=None, c=None, clamp=False):
    m = nt.nodes.new("ShaderNodeMath")
    m.operation = op
    m.use_clamp = clamp
    _feed(nt, m.inputs[0], a)
    if b is not None:
        _feed(nt, m.inputs[1], b)
    if c is not None:
        _feed(nt, m.inputs[2], c)
    return m.outputs["Value"]


def _range(nt, v, fmin, fmax, tmin=0.0, tmax=1.0, smooth=True):
    r = nt.nodes.new("ShaderNodeMapRange")
    r.interpolation_type = "SMOOTHSTEP" if smooth else "LINEAR"
    r.clamp = True
    _feed(nt, r.inputs["Value"], v)
    r.inputs["From Min"].default_value = fmin
    r.inputs["From Max"].default_value = fmax
    r.inputs["To Min"].default_value = tmin
    r.inputs["To Max"].default_value = tmax
    return r.outputs["Result"]


def _ramp(nt, fac, stops, interpolation="LINEAR"):
    r = nt.nodes.new("ShaderNodeValToRGB")
    r.color_ramp.interpolation = interpolation
    el = r.color_ramp.elements
    while len(el) > 1:
        el.remove(el[-1])
    el[0].position = stops[0][0]
    el[0].color = stops[0][1]
    for pos, col in stops[1:]:
        e = el.new(pos)
        e.color = col
    _link(nt, fac, r.inputs["Fac"])
    return r.outputs["Color"]


def _mix(nt, fac, a, b, blend="MIX"):
    """Colour mix (ShaderNodeMix, RGBA sockets by index: 0 factor, 6 A, 7 B, out 2)."""
    m = nt.nodes.new("ShaderNodeMix")
    m.data_type = "RGBA"
    m.blend_type = blend
    m.clamp_result = True
    _feed(nt, m.inputs[0], fac)
    _feed(nt, m.inputs[6], a)
    _feed(nt, m.inputs[7], b)
    return m.outputs[2]


def _pointiness(nt):
    g = nt.nodes.new("ShaderNodeNewGeometry")
    return g.outputs["Pointiness"]


def _xyz(nt, vec):
    s = nt.nodes.new("ShaderNodeSeparateXYZ")
    _link(nt, vec, s.inputs["Vector"])
    return s.outputs["X"], s.outputs["Y"], s.outputs["Z"]


def _bump(nt, height, strength, distance):
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = strength
    b.inputs["Distance"].default_value = distance
    _feed(nt, b.inputs["Height"], height)
    return b.outputs["Normal"]


def _reset(mat):
    """Empties the tree, returns (tree, new principled, output)."""
    nt = _nt(mat)
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    _link(nt, bsdf.outputs["BSDF"], out.inputs["Surface"])
    return nt, bsdf, out


# ------------------------------------------------------------------------------------------------ looks

def _wear(nt, lo=0.56, hi=0.74, mask_scale=260.0, vec=None):
    """Edge wear mask from the mesh curvature (Cycles pointiness), broken by a noise."""
    w = _range(nt, _pointiness(nt), lo, hi)
    if vec is not None:
        w = _math(nt, "MULTIPLY", w, _range(nt, _noise(nt, vec, mask_scale, 2.0), 0.35, 0.75))
    return w


def wood(mat, origin, hexa, rough, grain_scale=330.0, pith=(0.0, 0.07, 0.11), rotation=(0, 0, 2.0)):
    """Figured walnut: growth rings around a pith line along X (off the piece, so the grain draws arches on the
    sides), pores, medullary rays, a little lighter wear on the edges."""
    nt, bsdf, _ = _reset(mat)
    base = srgb(hexa)
    vec = _coords(nt, origin)
    sub = nt.nodes.new("ShaderNodeVectorMath")
    sub.operation = "SUBTRACT"
    _link(nt, vec, sub.inputs[0])
    sub.inputs[1].default_value = Vector(pith)
    rings = _mapping(nt, sub.outputs["Vector"], rotation=rotation)
    grain = _wave(nt, rings, grain_scale, 2.6, 2.8, 1.4, "X", wave_type="RINGS")
    fine = _wave(nt, rings, grain_scale * 3.4, 1.2, 1.5, 1.0, "X", wave_type="RINGS")
    pores = _noise(nt, _mapping(nt, vec, scale=(0.25, 1.0, 1.0)), 900.0, 1.0)
    figure = _noise(nt, vec, 14.0, 2.0, 0.55)
    ray = _noise(nt, _mapping(nt, vec, scale=(1.0, 7.0, 7.0)), 9.0, 1.0)
    col = _ramp(nt, grain, [(0.0, _scaled(base, 0.40)), (0.28, _scaled(base, 0.80)), (0.62, _scaled(base, 1.12)), (1.0, _scaled(base, 1.38, 0.92))])
    col = _mix(nt, _range(nt, figure, 0.3, 0.7, 0.0, 0.28), col, _scaled(base, 1.45, 0.85))
    col = _mix(nt, _range(nt, ray, 0.4, 0.75, 0.0, 0.20), col, _scaled(base, 0.6))
    col = _mix(nt, _math(nt, "MULTIPLY", fine, 0.10), col, _scaled(base, 0.5))
    col = _mix(nt, _range(nt, pores, 0.62, 0.85, 0.0, 0.35), col, _scaled(base, 0.45))
    wear = _wear(nt, 0.53, 0.66, 220.0, vec)
    col = _mix(nt, _math(nt, "MULTIPLY", wear, 0.45), col, _scaled(base, 1.6, 0.6))
    _link(nt, col, bsdf.inputs["Base Color"])
    r = _math(nt, "ADD", rough, _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", 1.0, grain), 0.14))
    r = _math(nt, "ADD", r, _math(nt, "MULTIPLY", _range(nt, pores, 0.62, 0.85), 0.15))
    r = _math(nt, "SUBTRACT", r, _math(nt, "MULTIPLY", wear, 0.12), clamp=True)
    _link(nt, r, bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = 0.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.45
    height = _math(nt, "ADD", _math(nt, "MULTIPLY", grain, 0.5), _math(nt, "ADD", _math(nt, "MULTIPLY", fine, 0.25), _math(nt, "MULTIPLY", pores, 0.25)))
    _link(nt, _bump(nt, height, 0.2, 0.0004), bsdf.inputs["Normal"])


def steel(mat, origin, hexa, rough, bright=False):
    nt, bsdf, _ = _reset(mat)
    base = srgb(hexa)
    vec = _coords(nt, origin)
    bare = srgb("B4B8BC") if bright else srgb("9DA2A6")
    blue = _mix(nt, _range(nt, _noise(nt, vec, 55.0, 2.0), 0.35, 0.65, 0.0, 0.4), base, srgb("2E3650") if not bright else srgb("848C95"))
    scratch = _range(nt, _noise(nt, _mapping(nt, vec, scale=(1.5, 120.0, 120.0)), 1.0, 1.0, 0.5), 0.63, 0.72)
    speck = _range(nt, _noise(nt, vec, 900.0, 1.0), 0.76, 0.92, 0.0, 0.35)
    wear = _wear(nt, 0.545, 0.66, 300.0, vec)
    col = _mix(nt, _math(nt, "MULTIPLY", scratch, 0.6), blue, bare)
    col = _mix(nt, _math(nt, "MULTIPLY", wear, 0.85), col, bare)
    col = _mix(nt, speck, col, _scaled(base, 1.4))
    grime = _range(nt, _noise(nt, vec, 24.0, 3.0), 0.55, 0.8, 0.0, 0.25)
    col = _mix(nt, grime, col, srgb("0B0C0E"))
    _link(nt, col, bsdf.inputs["Base Color"])
    r = _math(nt, "ADD", rough, _math(nt, "MULTIPLY", scratch, 0.28))
    r = _math(nt, "ADD", r, _math(nt, "MULTIPLY", _noise(nt, vec, 140.0, 2.0), 0.10))
    r = _math(nt, "SUBTRACT", r, _math(nt, "MULTIPLY", wear, 0.14), clamp=True)
    _link(nt, r, bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = 1.0
    height = _math(nt, "ADD", _math(nt, "MULTIPLY", scratch, 0.5), _math(nt, "MULTIPLY", speck, 0.3))
    _link(nt, _bump(nt, height, 0.18, 0.00018), bsdf.inputs["Normal"])


def brass(mat, origin, hexa, rough):
    nt, bsdf, _ = _reset(mat)
    base = srgb(hexa)
    vec = _coords(nt, origin)
    patina = _range(nt, _noise(nt, vec, 42.0, 3.0, 0.6), 0.4, 0.75, 0.0, 0.5)
    col = _mix(nt, patina, base, srgb("5A4220"))
    wear = _wear(nt, 0.53, 0.66, 300.0, vec)
    col = _mix(nt, _math(nt, "MULTIPLY", wear, 0.7), col, srgb("DCBB6A"))
    col = _mix(nt, _range(nt, _noise(nt, vec, 700.0, 1.0), 0.7, 0.9, 0.0, 0.3), col, _scaled(base, 0.7))
    _link(nt, col, bsdf.inputs["Base Color"])
    r = _math(nt, "ADD", rough, _math(nt, "MULTIPLY", patina, 0.2))
    r = _math(nt, "SUBTRACT", r, _math(nt, "MULTIPLY", wear, 0.12), clamp=True)
    _link(nt, r, bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = 1.0
    _link(nt, _bump(nt, _noise(nt, vec, 500.0, 1.0), 0.08, 0.0001), bsdf.inputs["Normal"])


def dark(mat, origin, hexa, rough, metal):
    nt, bsdf, _ = _reset(mat)
    vec = _coords(nt, origin)
    base = srgb(hexa)
    col = _mix(nt, _range(nt, _noise(nt, vec, 120.0, 2.0), 0.3, 0.7, 0.0, 0.3), base, _scaled(base, 2.5))
    _link(nt, col, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal


def paper(mat, origin, hexa, rough, spent=False, band=True):
    """Waxed red paper hull, a cream band printed around it; spent: scorched and sooty toward the open mouth."""
    nt, bsdf, _ = _reset(mat)
    base = srgb(hexa)
    vec = _coords(nt, origin)
    x, _, _ = _xyz(nt, vec)
    fibre = _noise(nt, vec, 520.0, 3.0, 0.6)
    ribs = _wave(nt, vec, 480.0, 0.4, 0.5, 1.0, "Y")
    col = _mix(nt, _range(nt, fibre, 0.3, 0.7, 0.0, 0.18), base, _scaled(base, 0.7))
    col = _mix(nt, _math(nt, "MULTIPLY", ribs, 0.08), col, _scaled(base, 1.25))
    if band:
        stripe = _math(nt, "MULTIPLY", _range(nt, x, 0.0035, 0.0045), _range(nt, x, 0.0085, 0.0075))
        col = _mix(nt, stripe, col, srgb("E6D7B2"))
    r = _math(nt, "ADD", rough, _math(nt, "MULTIPLY", fibre, 0.15))
    if spent:
        scorch = _math(nt, "MULTIPLY", _range(nt, x, 0.002, 0.021), _range(nt, _noise(nt, vec, 60.0, 2.0), 0.2, 0.8, 0.55, 1.0))
        col = _mix(nt, scorch, col, srgb("1A0E08"))
        soot = _range(nt, _noise(nt, vec, 260.0, 2.0), 0.55, 0.8, 0.0, 0.5)
        col = _mix(nt, _math(nt, "MULTIPLY", soot, _range(nt, x, -0.006, 0.012)), col, srgb("07060A"))
        r = _math(nt, "ADD", r, _math(nt, "MULTIPLY", scorch, 0.35))
    _link(nt, col, bsdf.inputs["Base Color"])
    _link(nt, _math(nt, "MINIMUM", r, 1.0), bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = 0.0
    _link(nt, _bump(nt, _math(nt, "ADD", _math(nt, "MULTIPLY", fibre, 0.5), _math(nt, "MULTIPLY", ribs, 0.5)), 0.12, 0.0001), bsdf.inputs["Normal"])


def kraft(mat, origin, hexa, rough):
    nt, bsdf, _ = _reset(mat)
    base = srgb(hexa)
    vec = _coords(nt, origin)
    fibre = _noise(nt, vec, 420.0, 3.0, 0.65)
    mottle = _noise(nt, vec, 18.0, 2.0)
    col = _mix(nt, _range(nt, fibre, 0.3, 0.7, 0.0, 0.2), base, _scaled(base, 0.72))
    col = _mix(nt, _range(nt, mottle, 0.35, 0.7, 0.0, 0.25), col, _scaled(base, 1.2))
    wear = _wear(nt, 0.58, 0.78, 200.0, vec)
    col = _mix(nt, _math(nt, "MULTIPLY", wear, 0.4), col, _scaled(base, 1.35, 0.7))
    _link(nt, col, bsdf.inputs["Base Color"])
    _link(nt, _math(nt, "ADD", rough, _math(nt, "MULTIPLY", fibre, 0.1), clamp=True), bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = 0.0
    _link(nt, _bump(nt, fibre, 0.15, 0.0001), bsdf.inputs["Normal"])


def label(mat, origin, hexa, rough):
    nt, bsdf, _ = _reset(mat)
    base = srgb(hexa)
    vec = _coords(nt, origin)
    col = _mix(nt, _range(nt, _noise(nt, vec, 300.0, 2.0), 0.3, 0.7, 0.0, 0.12), base, _scaled(base, 0.8))
    _link(nt, col, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = 0.0


def lead(mat, origin, hexa, rough, metal):
    nt, bsdf, _ = _reset(mat)
    base = srgb(hexa)
    vec = _coords(nt, origin)
    col = _mix(nt, _range(nt, _noise(nt, vec, 300.0, 2.0), 0.3, 0.7, 0.0, 0.3), base, _scaled(base, 0.6))
    _link(nt, col, bsdf.inputs["Base Color"])
    _link(nt, _math(nt, "ADD", rough, _math(nt, "MULTIPLY", _noise(nt, vec, 200.0, 2.0), 0.15)), bsdf.inputs["Roughness"])
    bsdf.inputs["Metallic"].default_value = metal
    _link(nt, _bump(nt, _noise(nt, vec, 600.0, 1.0), 0.2, 0.0002), bsdf.inputs["Normal"])


def apply_procedural(mat):
    """Gives a source material (shotgun_model.MATERIALS: 'bake_kind', 'bake_hex', optional 'bake_origin' and
    'bake_band' of a placed shell) its procedural tree."""
    kind = mat.get("bake_kind")
    if not kind:
        return
    bsdf = _bsdf(_nt(mat))
    hexa = mat["bake_hex"]
    origin = tuple(mat.get("bake_origin", (0.0, 0.0, 0.0)))
    band = bool(mat.get("bake_band", True))
    rough = float(bsdf.inputs["Roughness"].default_value)
    metal = float(bsdf.inputs["Metallic"].default_value)
    if kind == "wood":
        if "Pump" in mat.name:
            wood(mat, origin, hexa, rough, grain_scale=430.0, pith=(0.0, 0.055, 0.095), rotation=(0, 0, 0))
        else:
            wood(mat, origin, hexa, rough, grain_scale=330.0, pith=(0.0, 0.07, 0.11), rotation=(0, 0, 2.0))
    elif kind == "steel":
        steel(mat, origin, hexa, rough, bright=False)
    elif kind == "steel_bright":
        steel(mat, origin, hexa, rough, bright=True)
    elif kind == "brass":
        brass(mat, origin, hexa, rough)
    elif kind == "dark":
        dark(mat, origin, hexa, rough, metal)
    elif kind == "paper":
        paper(mat, origin, hexa, rough, spent=False, band=band)
    elif kind == "paper_spent":
        paper(mat, origin, hexa, rough, spent=True, band=band)
    elif kind == "kraft":
        kraft(mat, origin, hexa, rough)
    elif kind == "label":
        label(mat, origin, hexa, rough)
    elif kind == "lead":
        lead(mat, origin, hexa, rough, metal)
    mat["procedural"] = True


# ------------------------------------------------------------------------------------------------ UV and bake

def _select_only(ob):
    for sc in bpy.data.scenes:
        for vl in sc.view_layers:
            for o in list(vl.objects):
                if o is not None:
                    o.select_set(False, view_layer=vl)
    vl = bpy.context.window.view_layer
    ob.hide_viewport = False
    ob.hide_set(False, view_layer=vl)
    ob.select_set(True, view_layer=vl)
    vl.objects.active = ob


def unwrap(ob, margin=0.004, angle=66.0):
    """One unique UV layout for the bake (replaces the box projection of the kit)."""
    _select_only(ob)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")


def _image(name, size, colorspace):
    old = bpy.data.images.get(name)
    if old:
        bpy.data.images.remove(old)
    img = bpy.data.images.new(name, size, size, alpha=False)
    img.colorspace_settings.name = colorspace
    return img


def _save(img, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.filepath_raw = str(path)
    img.file_format = "PNG"
    img.save()
    return path


def _pixels(img):
    arr = np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(img.size[1], img.size[0], 4)


def bake_textures(ob, basename, size, out_dir, ao_samples=24, ao_distance=0.05, margin=8):
    """Bakes BaseColor / Normal (DirectX) / ORM of ob (its materials are the procedural sources) to
    out_dir/<basename>_<map>.png and returns {label: Path}."""
    scene = bpy.context.scene
    out_dir = Path(out_dir)
    mats = [slot.material for slot in ob.material_slots if slot.material]
    engine, device = scene.render.engine, None
    scene.render.engine = "CYCLES"
    try:
        device = scene.cycles.device
        scene.cycles.device = "CPU"
    except AttributeError:
        pass
    scene.cycles.samples = 1
    scene.cycles.use_denoising = False
    bake = scene.render.bake
    bake.use_selected_to_active = False
    bake.margin = margin
    try:
        bake.margin_type = "EXTEND"
    except (AttributeError, TypeError):
        pass
    bake.use_clear = True
    bake.target = "IMAGE_TEXTURES"
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World-Bake")
    try:
        scene.world.light_settings.distance = ao_distance
    except AttributeError:
        pass
    _select_only(ob)

    def target(img):
        for mat in mats:
            nt = _nt(mat)
            node = nt.nodes.get("BakeTarget")
            if node is None:
                node = nt.nodes.new("ShaderNodeTexImage")
                node.name = "BakeTarget"
            node.image = img
            node.select = True
            nt.nodes.active = node

    def run(kind, **kw):
        bpy.ops.object.bake(type=kind, margin=margin, use_clear=True, **kw)

    def emit_bake(socket_name):
        """Bakes the value feeding a Principled socket (link or constant) as emission: the raw albedo of a metal is
        black in a DIFFUSE/COLOR bake, and there is no bake type for the metallic input."""
        restore = []
        for mat in mats:
            nt = _nt(mat)
            out = _output(nt)
            old = out.inputs["Surface"].links[0].from_socket if out.inputs["Surface"].links else None
            bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
            em = nt.nodes.new("ShaderNodeEmission")
            em.name = "BakeEmit"
            em.inputs["Strength"].default_value = 1.0
            sock = bsdf.inputs[socket_name] if bsdf else None
            if sock is not None and sock.links:
                _link(nt, sock.links[0].from_socket, em.inputs["Color"])
            elif sock is not None:
                v = sock.default_value
                em.inputs["Color"].default_value = tuple(v)[:4] if hasattr(v, "__len__") else (v, v, v, 1.0)
            _link(nt, em.outputs["Emission"], out.inputs["Surface"])
            restore.append((nt, out, old, em))
        run("EMIT")
        for nt, out, old, em in restore:
            nt.nodes.remove(em)
            if old is not None:
                _link(nt, old, out.inputs["Surface"])

    print(f"[shotgun bake] {basename} {size}px: base colour (emission of the Base Color input)", flush=True)
    base = _image(basename + "_BaseColor", size, "sRGB")
    target(base)
    emit_bake("Base Color")
    paths = {"BaseColor": _save(base, out_dir / (basename + "_BaseColor.png"))}

    print(f"[shotgun bake] {basename}: normal", flush=True)
    normal = _image(basename + "_Normal", size, "Non-Color")
    target(normal)
    run("NORMAL")
    arr = _pixels(normal)
    arr[..., 1] = 1.0 - arr[..., 1]           # OpenGL -> DirectX (green down)
    normal.pixels.foreach_set(arr.ravel())
    paths["Normal"] = _save(normal, out_dir / (basename + "_Normal.png"))

    print(f"[shotgun bake] {basename}: roughness", flush=True)
    rough = _image(basename + "_Rough", size, "Non-Color")
    target(rough)
    run("ROUGHNESS")
    rough_px = _pixels(rough)[..., 0].copy()

    print(f"[shotgun bake] {basename}: metallic (emission of the Metallic input)", flush=True)
    metal = _image(basename + "_Metal", size, "Non-Color")
    target(metal)
    emit_bake("Metallic")
    metal_px = _pixels(metal)[..., 0].copy()

    print(f"[shotgun bake] {basename}: ambient occlusion ({ao_samples} samples)", flush=True)
    scene.cycles.samples = ao_samples
    ao = _image(basename + "_AO", size, "Non-Color")
    target(ao)
    run("AO")
    ao_px = _pixels(ao)[..., 0].copy()
    scene.cycles.samples = 1

    orm = _image(basename + "_ORM", size, "Non-Color")
    arr = np.ones((size, size, 4), dtype=np.float32)
    arr[..., 0] = ao_px
    arr[..., 1] = rough_px
    arr[..., 2] = metal_px
    orm.pixels.foreach_set(arr.ravel())
    paths["ORM"] = _save(orm, out_dir / (basename + "_ORM.png"))

    for mat in mats:
        node = _nt(mat).nodes.get("BakeTarget")
        if node:
            _nt(mat).nodes.remove(node)
    for img in (rough, metal, ao):
        bpy.data.images.remove(img)
    scene.render.engine = engine
    if device is not None:
        try:
            scene.cycles.device = device
        except AttributeError:
            pass
    return paths


def baked_material(name, paths):
    """M_<name>: Image Texture nodes labelled BaseColor (sRGB), Normal (DirectX, flipped back for Blender's render)
    and ORM (linear; G roughness, B metallic, R occlusion left for the engine)."""
    mat = bpy.data.materials.get(name)
    if mat:
        bpy.data.materials.remove(mat)
    mat = bpy.data.materials.new(name)
    nt, bsdf, _ = _reset(mat)

    def tex(label, colorspace, x, y):
        node = nt.nodes.new("ShaderNodeTexImage")
        node.label = label
        node.name = label
        node.location = (x, y)
        img = bpy.data.images.load(str(paths[label]), check_existing=True)
        img.colorspace_settings.name = colorspace
        node.image = img
        return node

    base = tex("BaseColor", "sRGB", -700, 300)
    _link(nt, base.outputs["Color"], bsdf.inputs["Base Color"])
    orm = tex("ORM", "Non-Color", -700, 0)
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    _link(nt, orm.outputs["Color"], sep.inputs["Color"])
    _link(nt, sep.outputs["Green"], bsdf.inputs["Roughness"])
    _link(nt, sep.outputs["Blue"], bsdf.inputs["Metallic"])
    normal = tex("Normal", "Non-Color", -700, -300)
    sepn = nt.nodes.new("ShaderNodeSeparateColor")
    _link(nt, normal.outputs["Color"], sepn.inputs["Color"])
    flip = _math(nt, "SUBTRACT", 1.0, sepn.outputs["Green"])
    comb = nt.nodes.new("ShaderNodeCombineColor")
    _link(nt, sepn.outputs["Red"], comb.inputs["Red"])
    _link(nt, flip, comb.inputs["Green"])
    _link(nt, sepn.outputs["Blue"], comb.inputs["Blue"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nm.space = "TANGENT"
    _link(nt, comb.outputs["Color"], nm.inputs["Color"])
    _link(nt, nm.outputs["Normal"], bsdf.inputs["Normal"])
    mat["baked"] = True
    mat["normal_convention"] = "DirectX (green down)"
    mat["orm"] = "R occlusion, G roughness, B metallic"
    return mat


def assign(ob, mat):
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    idx = np.zeros(len(ob.data.polygons), dtype=np.int32)
    ob.data.polygons.foreach_set("material_index", idx)
    ob.data.update()
