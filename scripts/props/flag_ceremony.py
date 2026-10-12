"""Flag pole and British flag of the village flag ceremony (owner request 2026-10-07: "au centre un drapeau va
apparaître et pendant 1 minute le drapeau britannique va monter", the stone pile in place of the red circle).

The site (AFlagCeremonySite, DA_FlagCeremonyRules) stands on SM_Temple_Debris_StonePile (the small stone pile of the
reliquary temple maquette); this script builds what rises out of it:
- SM_FlagCeremony_Pole: a wooden pole 4.2 m high, pivot at its foot on its axis, a gilded ball on top, the halyard
  along it; one UCX box around the pole (the stones keep their own collision);
- SM_FlagCeremony_Flag: the Union Flag (cahier §6.2: "un drapeau britannique (Union Jack)"), 1.2 x 0.6 m, ratio 1:2,
  built as flat layers of cloth (blue field, white saltire of St Andrew, red saltire of St Patrick counterchanged,
  white and red cross of St George, official proportions on a 60 x 30 grid), on both faces (the back shows the mirror
  image, as cloth does), with a light baked wave; pivot at the bottom of the hoist on the pole axis, the fly along +X.
  No collision. The site slides it up the pole.
Frame: metres, +Z up; Unreal: x, -y, z (the flag flies along +X in both).

Run (background):
  blender --background --factory-startup --python scripts/props/flag_ceremony.py
Outputs: art/export/Props/FlagCeremony/ (FBX + flag_ceremony.json + preview.png)
"""
import json
import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/export/Props/FlagCeremony"

POLE = {"height": 4.20, "r_foot": 0.040, "r_top": 0.028, "ball": 0.055, "halyard_r": 0.004, "halyard_x": 0.050}
FLAG = {"width": 1.20, "height": 0.60, "hoist_gap": 0.045, "layer": 0.0007, "wave": 0.045, "wavelength": 0.75,
        "slices": 48}
# sRGB colours (Pantone 280 C blue, 186 C red of the Union Flag) and roughness
COLOURS = {"M_Flag_Blue": ("012169", 0.85), "M_Flag_White": ("F4F4F0", 0.85), "M_Flag_Red": ("C8102E", 0.85),
           "M_FlagPole_Wood": ("6B4A2E", 0.70), "M_FlagPole_Gold": ("C99A3E", 0.30), "M_FlagPole_Rope": ("D8C9A3", 0.90)}


def material(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    hex_colour, rough = COLOURS[name]
    rgb = [int(hex_colour[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb]
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*lin, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    if "Gold" in name:
        bsdf.inputs["Metallic"].default_value = 1.0
    return mat


def mesh_object(name, bm, mats):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(material(m))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


# ---------------------------------------------------------------- flag layers (60 x 30 grid, origin bottom left)

def clip(poly, a, b, c):
    """Keeps the part of a convex polygon where a*x + b*y <= c (Sutherland-Hodgman, one half-plane)."""
    out = []
    for i, p in enumerate(poly):
        q = poly[(i + 1) % len(poly)]
        fp, fq = a * p[0] + b * p[1] - c, a * q[0] + b * q[1] - c
        if fp <= 0:
            out.append(p)
        if (fp < 0 < fq) or (fq < 0 < fp):
            t = fp / (fp - fq)
            out.append((p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1])))
    return out


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def band(direction, s0, s1, region):
    """Part of region (convex) between s0 and s1 along the upward normal of the line through the centre (30, 15)."""
    dx, dy = direction
    length = math.hypot(dx, dy)
    nx, ny = -dy / length, dx / length
    if ny < 0:
        nx, ny = -nx, -ny
    c0 = nx * 30.0 + ny * 15.0
    poly = clip(region, nx, ny, c0 + s1)          # s <= s1
    return clip(poly, -nx, -ny, -(c0 + s0))       # s >= s0


def union_flag_layers():
    """(material, convex polygon) from the back layer to the front one; the 60 x 30 grid of the 1:2 flag."""
    full = rect(0.0, 0.0, 60.0, 30.0)
    layers = [("M_Flag_Blue", full)]
    for direction in ((60.0, 30.0), (60.0, -30.0)):                 # St Andrew: white, 6 wide
        layers.append(("M_Flag_White", band(direction, -3.0, 3.0, full)))
    # St Patrick: red, 2 wide, counterchanged: under the centre line of each arm on the hoist half, over it on the fly
    for quadrant, direction in ((rect(0, 15, 30, 30), (60.0, -30.0)), (rect(0, 0, 30, 15), (60.0, 30.0)),
                                (rect(30, 15, 60, 30), (60.0, 30.0)), (rect(30, 0, 60, 15), (60.0, -30.0))):
        hoist = quadrant[0][0] < 30.0
        layers.append(("M_Flag_Red", band(direction, -2.0, 0.0, quadrant) if hoist else band(direction, 0.0, 2.0, quadrant)))
    layers.append(("M_Flag_White", rect(25.0, 0.0, 35.0, 30.0)))    # St George: white border, 10 wide
    layers.append(("M_Flag_White", rect(0.0, 10.0, 60.0, 20.0)))
    layers.append(("M_Flag_Red", rect(27.0, 0.0, 33.0, 30.0)))      # red cross, 6 wide
    layers.append(("M_Flag_Red", rect(0.0, 12.0, 60.0, 18.0)))
    return [(m, p) for m, p in layers if len(p) >= 3]


def wave(u):
    """Offset of the cloth along -Y at the distance u from the hoist (m): still at the pole, rippling at the fly."""
    f = FLAG
    return f["wave"] * (u / f["width"]) ** 1.3 * math.sin(2.0 * math.pi * u / f["wavelength"])


def build_flag():
    f = FLAG
    names = ["M_Flag_Blue", "M_Flag_White", "M_Flag_Red"]
    bm = bmesh.new()
    layers = union_flag_layers()
    scale = f["width"] / 60.0
    for side in (1.0, -1.0):                       # front (-Y in Blender = towards the viewer) then back
        for depth, (mat, poly) in enumerate(layers):
            verts = [bm.verts.new((f["hoist_gap"] + x * scale, 0.0, y * scale)) for x, y in poly]
            face = bm.faces.new(verts if side > 0 else list(reversed(verts)))
            face.material_index = names.index(mat)
            face.normal_update()
            face.smooth = True
            face.tag = True
            # layer offset on its side: later layers lie over the earlier ones
            for v in verts:
                v.co.y = -side * (depth + 1) * f["layer"]
    # slices across the fly so the wave bends the cloth
    for k in range(1, f["slices"]):
        x = f["hoist_gap"] + f["width"] * k / f["slices"]
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(x, 0.0, 0.0), plane_no=(1.0, 0.0, 0.0))
    for v in bm.verts:
        v.co.y += -wave(v.co.x - f["hoist_gap"])
    bmesh.ops.triangulate(bm, faces=[fc for fc in bm.faces if len(fc.verts) > 4])
    uv = bm.loops.layers.uv.new("UVMap")
    for fc in bm.faces:
        for loop in fc.loops:
            loop[uv].uv = ((loop.vert.co.x - f["hoist_gap"]) / f["width"], loop.vert.co.z / f["height"])
    bm.normal_update()
    return mesh_object("SM_FlagCeremony_Flag", bm, names), len(layers)


def ring(bm, z, r, n=16):
    return [bm.verts.new((r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n), z)) for i in range(n)]


def build_pole():
    p = POLE
    bm = bmesh.new()
    n = 16
    bottom, top = ring(bm, 0.0, p["r_foot"], n), ring(bm, p["height"], p["r_top"], n)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((bottom[i], bottom[j], top[j], top[i])).material_index = 0
    bm.faces.new(list(reversed(bottom))).material_index = 0
    bm.faces.new(top).material_index = 0
    # gilded ball on the truck
    ball = bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=p["ball"])
    for v in ball["verts"]:
        v.co.z += p["height"] + p["ball"] * 0.85
    for fc in {fc for v in ball["verts"] for fc in v.link_faces}:
        fc.material_index = 1
    # halyard: a thin line from the cleat to the truck, beside the pole
    hal_bottom = [bm.verts.new((p["halyard_x"] + p["halyard_r"] * math.cos(2 * math.pi * i / 6),
                                p["halyard_r"] * math.sin(2 * math.pi * i / 6), 0.55)) for i in range(6)]
    hal_top = [bm.verts.new((v.co.x, v.co.y, p["height"] - 0.03)) for v in hal_bottom]
    for i in range(6):
        j = (i + 1) % 6
        bm.faces.new((hal_bottom[i], hal_bottom[j], hal_top[j], hal_top[i])).material_index = 2
    # cleat the halyard is tied to
    cleat = bmesh.ops.create_cube(bm, size=1.0)
    for v in cleat["verts"]:
        v.co = Vector((v.co.x * 0.03 + p["r_foot"] + 0.01, v.co.y * 0.10, v.co.z * 0.025 + 0.55))
    for fc in {fc for v in cleat["verts"] for fc in v.link_faces}:
        fc.material_index = 0
    for fc in bm.faces:
        fc.smooth = True
    bm.normal_update()
    ob = mesh_object("SM_FlagCeremony_Pole", bm, ["M_FlagPole_Wood", "M_FlagPole_Gold", "M_FlagPole_Rope"])
    # simple collision: one box around the pole (the halyard and the ball are too thin to matter)
    ucx_bm = bmesh.new()
    box = bmesh.ops.create_cube(ucx_bm, size=1.0)
    for v in box["verts"]:
        v.co = Vector((v.co.x * 0.10, v.co.y * 0.10, (v.co.z + 0.5) * p["height"]))
    ucx = mesh_object("UCX_SM_FlagCeremony_Pole_00", ucx_bm, [])
    return ob, ucx


def export(objs, path):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    # same settings as the minka and the boat (imported at scale 1 in Unreal)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", global_scale=1.0, axis_forward="-Y", axis_up="Z",
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False, use_custom_props=False)


def preview(objs):
    scene = bpy.context.scene
    cam_data = bpy.data.cameras.new("Preview")
    cam = bpy.data.objects.new("Preview", cam_data)
    scene.collection.objects.link(cam)
    cam.location = Vector((1.6, -4.2, 3.1))
    direction = Vector((0.55, 0.0, 2.9)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam_data.lens = 45
    scene.camera = cam
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.rotation_euler = (0.9, 0.2, 0.6)
    scene.collection.objects.link(sun)
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        try:
            scene.render.engine = "BLENDER_EEVEE"
        except TypeError:
            pass
    world = scene.world or bpy.data.worlds.new("World")
    scene.world = world
    world.color = (0.55, 0.62, 0.70)
    scene.render.resolution_x, scene.render.resolution_y = 900, 900
    scene.render.filepath = str(OUT / "preview.png")
    scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)


def main():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob)
    OUT.mkdir(parents=True, exist_ok=True)
    flag, layer_count = build_flag()
    pole, ucx = build_pole()
    export([flag], OUT / "SM_FlagCeremony_Flag.fbx")
    export([pole, ucx], OUT / "SM_FlagCeremony_Pole.fbx")
    # the flag at mid height for the preview only
    flag.location.z = 2.2
    ucx.hide_render = True
    preview([flag, pole])
    info = {"pole": POLE, "flag": FLAG, "flag_layers": layer_count, "materials": COLOURS,
            "pivots": {"pole": "foot, on its axis", "flag": "bottom of the hoist, on the pole axis; fly along +X"},
            "unreal": {"pole_height_cm": POLE["height"] * 100.0, "flag_size_cm": [FLAG["width"] * 100.0, FLAG["height"] * 100.0]}}
    (OUT / "flag_ceremony.json").write_text(json.dumps(info, indent=2), encoding="utf-8")
    print("FLAG_CEREMONY_EXPORT_SUCCESS", json.dumps({"layers": layer_count}))


if __name__ == "__main__":
    main()
