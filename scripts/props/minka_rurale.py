"""Build the rural Japanese minka "Minka_Rurale" from the owner's brief (docs/art/briefs/minka-rurale.md).

Run (background):
  blender --background --factory-startup --python scripts/props/minka_rurale.py -- [options]
Options:
  --render     renders: art/_previews/minka_{trois_quarts,facade,profil,dessus,interieur}.png
               (then the board: python scripts/props/minka_board.py -> art/_previews/minka_planche.png)
  --export     art/export/Props/Minka/: Minka_Rurale.glb, SM_Minka_*.fbx (UCX for the structure), minka_panels.json,
               SM_Minka.fbx (whole house, one mesh, UCX + buried earth plinth), materials.json (for Unreal)
  --no-save    do not write art/blender/props/minka_rurale.blend
CC0 textures and models first: python scripts/props/fetch_minka_assets.py
"""
import argparse
import importlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from minka import build, geo  # noqa: E402

importlib.reload(geo)
importlib.reload(build)

ROOT = HERE.parents[1]
ART = ROOT / "art"
BLEND = ART / "blender" / "props" / "minka_rurale.blend"
PREV = ART / "_previews"
EXPORT = ART / "export" / "Props" / "Minka"
MODELS = ART / "models" / "props"
EXPORTED = {}       # material name -> material, for every mesh exported this run (geo.material_manifest)


# ---------------------------------------------------------------- CC0 models

def import_model(asset, size, location, coll, name=None, up=False, max_tris=2500):
    """Import a Poly Haven glTF, join it, scale its largest side to `size` (m), rest it on `location`."""
    path = MODELS / asset / f"{asset}_1k.gltf"
    if not path.exists():
        print(f"[minka] missing model {path}")
        return None
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    names = [o.name for o in new]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        return None
    for o in new:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        coll.objects.link(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(meshes) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    for n in names:
        o = bpy.data.objects.get(n)
        if o is not None and o is not ob:
            bpy.data.objects.remove(o)
    me = ob.data
    lo = Vector((min(v.co[i] for v in me.vertices) for i in range(3)))
    hi = Vector((max(v.co[i] for v in me.vertices) for i in range(3)))
    k = size / max(hi - lo)
    base = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    for v in me.vertices:
        v.co = (v.co - base) * k
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    if tris > max_tris:  # Poly Haven scans are dense: keep props light for the game
        mod = ob.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = max_tris / tris
        bpy.ops.object.select_all(action="DESELECT")
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.modifier_apply(modifier=mod.name)
    ob.location = location
    if up:
        ob.rotation_euler = (math.radians(-12), 0, 0)
    ob.name = me.name = f"Minka_{name or asset}"
    for i, m in enumerate(me.materials):
        if m and not m.name.startswith("M_"):
            m.name = f"M_Minka_{asset}" + (f"_{i}" if i else "")
    bpy.ops.object.select_all(action="DESELECT")
    return ob


# ---------------------------------------------------------------- presentation

def studio(scene):
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = eng
            break
        except TypeError:
            continue
    world = bpy.data.worlds.new("W_MinkaStudio")
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs[0].default_value = (1.0, 1.0, 1.0, 1)
    bg.inputs[1].default_value = 1.25
    scene.world = world
    sun = bpy.data.lights.new("Minka_Sun", "SUN")
    sun.energy = 3.2
    sun.angle = math.radians(4)
    so = bpy.data.objects.new("Minka_Sun", sun)
    so.rotation_euler = (math.radians(48), math.radians(6), math.radians(32))
    scene.collection.objects.link(so)
    try:
        scene.eevee.taa_render_samples = 64
        scene.eevee.use_shadows = True
    except AttributeError:
        pass
    try:
        scene.view_settings.view_transform = "AgX"
        scene.view_settings.look = "AgX - Medium Contrast"
    except TypeError:
        pass
    # white shadow-catching ground (board style)
    me = bpy.data.meshes.new("PRES_Ground")
    s = 60.0
    me.from_pydata([(-s, -s, -0.02), (s, -s, -0.02), (s, s, -0.02), (-s, s, -0.02)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("PRES_Ground", me)
    mat = geo.flat_material("M_PRES_Ground", "F3F2EE", rough=0.9)
    me.materials.append(mat)
    return ob


def camera(name, loc, target, lens=40, ortho=None, coll=None):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_end = 300
    if ortho:
        cd.type = "ORTHO"
        cd.ortho_scale = ortho
    ob = bpy.data.objects.new(name, cd)
    ob.location = loc
    ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    (coll or bpy.context.scene.collection).objects.link(ob)
    return ob


def render(scene, cam, path, res, hide=()):
    states = [(o, o.hide_render) for o in hide]
    for o in hide:
        o.hide_render = True
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = str(path)
    scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)
    for o, s in states:
        o.hide_render = s
    return path


# ---------------------------------------------------------------- export

def export_group(objs, name, extra=(), local_origin=None, out_dir=None):
    """Duplicate, apply modifiers and transforms, join, export FBX (cm in Unreal), delete the copies."""
    out_dir = Path(out_dir or EXPORT)
    out_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    copies = []
    for o in objs:
        if o.type != "MESH":
            continue
        c = o.copy()
        c.data = o.data.copy()
        c.animation_data_clear()
        bpy.context.scene.collection.objects.link(c)
        copies.append(c)
    for c in copies:
        c.select_set(True)
    bpy.context.view_layer.objects.active = copies[0]
    bpy.ops.object.convert(target="MESH")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(copies) > 1:
        bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    # the exported object must carry exactly `name` (UCX_<name>_NN are matched on it in Unreal): move aside a
    # library object that already holds the name (Blender would otherwise call the copy "<name>.001")
    clash = bpy.data.objects.get(name)
    if clash is not None and clash is not joined:
        clash.name = f"{name}__src"
    joined.name = joined.data.name = name
    for m in joined.data.materials:
        if m is not None:
            EXPORTED[m.name] = m
    if local_origin is not None:
        joined.data.transform(Matrix.Translation(-Vector(local_origin)))
    sel = [joined] + list(extra)
    bpy.ops.object.select_all(action="DESELECT")
    for o in sel:
        o.hide_set(False)
        o.select_set(True)
    path = out_dir / f"{name}.fbx"
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", global_scale=1.0, axis_forward="-Y", axis_up="Z",
                             mesh_smooth_type="FACE", use_mesh_modifiers=True, use_tspace=True, path_mode="RELATIVE",
                             embed_textures=False, bake_anim=False)
    tris = geo.tri_count(joined)
    bpy.data.objects.remove(joined)
    if clash is not None and clash.name == f"{name}__src":
        clash.name = name
    print(f"[minka] {path.name}: {tris} tris")
    return path, tris


HOUSE_GROUND = [(0.0, 0.0, 9.0, 7.2), (1.8, -1.2, 10.2, 0.0), (9.0, 0.0, 10.2, 7.2)]   # body, engawa (m)
PLINTH_DEPTH = 0.8


def export_house(mk, out_dir=None):
    """Whole house as one static mesh for villages (panels in their current state: entrance door 65 % open,
    furniture inside), its UCX boxes, and a buried earth plinth under the body and the engawa: on uneven ground
    the house is set on the highest point of its footprint and the plinth fills the gap down to PLINTH_DEPTH."""
    full = []
    for c in ("Structure", "Toiture", "Murs", "Planchers", "Engawa", "Shoji", "Fusuma", "Tatamis", "Mobilier"):
        full += [o for o in bpy.data.collections[f"Minka_{c}"].all_objects if o.type == "MESH"]
    lo = [min(v) for v in zip(*[(o.matrix_world @ Vector(b))[:] for o in full for b in o.bound_box])]
    hi = [max(v) for v in zip(*[(o.matrix_world @ Vector(b))[:] for o in full for b in o.bound_box])]
    import bmesh
    bm = bmesh.new()
    uvl = geo.uv_layer(bm)
    for x0, y0, x1, y1 in HOUSE_GROUND:
        geo.add_box(bm, (x0 + 0.02, y0 + 0.02, -PLINTH_DEPTH), (x1 - 0.02, y1 - 0.02, -0.005), uvl, 2.0)
    plinth = geo.obj("Minka_Plinth", bm, mk.M["Earth"], bpy.context.scene.collection)
    ucx_full = []
    for i, o in enumerate(mk.ucx_objects):
        c = o.copy()
        c.data = o.data.copy()
        c.name = f"UCX_SM_Minka_{i:02d}"
        bpy.context.scene.collection.objects.link(c)
        ucx_full.append(c)
    _, tris = export_group(full + [plinth], "SM_Minka", extra=ucx_full, out_dir=out_dir)
    for c in ucx_full + [plinth]:
        bpy.data.objects.remove(c)
    return {"tris": tris, "bbox": [[round(v, 3) for v in lo[:2]] + [-PLINTH_DEPTH], [round(v, 3) for v in hi]],
            "ground": [list(r) for r in HOUSE_GROUND], "ucx": len(ucx_full)}


def export_all(mk):
    report = {}
    coll = lambda n: [o for o in bpy.data.collections[f"Minka_{n}"].all_objects if o.type == "MESH"]
    structure = coll("Structure") + coll("Murs") + coll("Planchers") + coll("Engawa")
    structure = [o for o in structure if "panel_type" not in o]
    report["SM_Minka_Structure"] = export_group(structure, "SM_Minka_Structure", extra=mk.ucx_objects)[1]
    report["SM_Minka_Roof"] = export_group([o for o in coll("Toiture") if o is not mk.ceiling], "SM_Minka_Roof")[1]
    report["SM_Minka_Ceilings"] = export_group([mk.ceiling], "SM_Minka_Ceilings")[1]
    report["SM_Minka_Tatamis"] = export_group(coll("Tatamis"), "SM_Minka_Tatamis")[1]
    report["SM_Minka_Furniture"] = export_group(coll("Mobilier"), "SM_Minka_Furniture")[1]
    panels, done = [], {}
    for o in mk.panels:
        me = o.data.name
        if me not in done:
            tmp = bpy.data.objects.new(me, o.data)
            bpy.context.scene.collection.objects.link(tmp)
            done[me] = export_group([tmp], me)[1]
            bpy.data.objects.remove(tmp)
        panels.append({"name": o.name, "mesh": me, "type": o["panel_type"],
                       "closed_cm": [round(o["closed_x"] * 100, 1), round(o["closed_y"] * 100, 1),
                                     round(o.location.z * 100, 1)],
                       "travel_cm": [round(o["travel_x"] * 100, 1), round(o["travel_y"] * 100, 1)],
                       "yaw_deg": round(math.degrees(o.rotation_euler.z), 1), "open": o["open"]})
    report.update(done)
    (EXPORT / "minka_panels.json").write_text(json.dumps({"origin": "front-left corner of the main body, ground",
                                                          "panels": panels}, indent=1), encoding="utf-8")
    report["SM_Minka"] = export_house(mk)["tris"]
    (EXPORT / "materials.json").write_text(json.dumps(geo.material_manifest(EXPORTED.values()), indent=1),
                                           encoding="utf-8")
    # glTF of the whole visible house (review / other engines)
    bpy.ops.object.select_all(action="DESELECT")
    for c in ("Structure", "Toiture", "Murs", "Planchers", "Engawa", "Shoji", "Fusuma", "Tatamis", "Mobilier"):
        for o in bpy.data.collections[f"Minka_{c}"].all_objects:
            if o.type == "MESH":
                o.select_set(True)
    glb = EXPORT / "Minka_Rurale.glb"
    try:
        bpy.ops.export_scene.gltf(filepath=str(glb), export_format="GLB", use_selection=True, export_apply=True)
        report["glb_mb"] = round(glb.stat().st_size / 1e6, 1)
    except Exception as exc:  # noqa: BLE001 - report the exporter limitation instead of aborting the build
        report["glb_error"] = str(exc)[:200]
    return report


# ---------------------------------------------------------------- main

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    a = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Minka_Rurale"
    scene.unit_settings.system = "METRIC"
    root = geo.collection("Minka_Rurale")
    M = geo.materials()
    mk = build.Minka(root, M).build(import_model)
    ground = studio(scene)
    mk.c["Presentation"].objects.link(ground)

    tris = sum(geo.tri_count(o) for o in bpy.data.objects if o.type == "MESH" and not o.name.startswith(("UCX_", "PRES_")))
    print(f"[minka] objects {len(bpy.data.objects)}, panels {len(mk.panels)}, UCX {len(mk.ucx_objects)}, "
          f"~{tris} tris before modifiers")

    cams = {
        "trois_quarts": camera("CAM_Minka_34", (17.5, -15.5, 10.0), (4.6, 2.6, 2.2), 38, coll=mk.c["Presentation"]),
        "facade": camera("CAM_Minka_Facade", (4.9, -40.0, 3.4), (4.9, 3.0, 3.4), ortho=13.5, coll=mk.c["Presentation"]),
        "profil": camera("CAM_Minka_Profil", (50.0, 3.0, 3.4), (-5.0, 3.0, 3.4), ortho=12.5, coll=mk.c["Presentation"]),
        "dessus": camera("CAM_Minka_Dessus", (4.95, 3.0, 40.0), (4.95, 3.0, 0.0), ortho=11.6, coll=mk.c["Presentation"]),
        "interieur": camera("CAM_Minka_Interieur", (11.6, -2.9, 4.6), (7.0, 3.9, 0.55), 26, coll=mk.c["Presentation"]),
    }
    cams["dessus"].rotation_euler = (0, 0, 0)
    scene.camera = cams["trois_quarts"]
    roofless = [o for o in bpy.data.collections["Minka_Toiture"].all_objects]
    roofless += [o for o in bpy.data.objects if o.name in ("Minka_TieLogs", "Minka_Irori_JizaiKagi")]

    if a.render:
        paths = {}
        for key, cam in cams.items():
            hide = roofless if key in ("dessus", "interieur") else ()
            res = (1800, 1100) if key == "trois_quarts" else (1400, 900)
            paths[key] = render(scene, cam, PREV / f"minka_{key}.png", res, hide)
        print("[minka] previews in", PREV, "- board: python scripts/props/minka_board.py (PIL, outside Blender)")

    if a.export:
        rep = export_all(mk)
        print("[minka] export:", json.dumps(rep))

    if not a.no_save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        print("[minka] saved", BLEND)


if __name__ == "__main__":
    main()
