"""
Fort francais : construction d'UN lot de modules dans un Blender separe (la memoire est rendue a la fin).

    blender --background --factory-startup --python factory/kits/fort_lot.py -- <dir maquette> <lot>

Pour chaque module du lot (params.lots[lot].modules) : construction a l'origine (fort_kit.assemble), export
exports/SM_*.fbx (avec UCX_ / SOCKET_) + exports/SM_*.glb (geometrie seule, JPEG 85), collection nommee comme le SM
(liee ensuite par le lot scene). Sauve modules/<lot>.blend (compresse, chemins relatifs, images non empaquetees) et
modules/<lot>.json (tris, cotes, sockets locaux avec rotation, UCX, materiaux, infos).
FORT_PREVIEW=1 : rend en plus une vignette EEVEE par module dans _tests/out/ (controle visuel).
"""
import json
import math
import os
import sys
import time
import traceback

import bpy
from mathutils import Matrix, Vector

KITS = os.path.dirname(os.path.abspath(__file__))
if KITS not in sys.path:
    sys.path.insert(0, KITS)


def _reset():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for c in list(bpy.data.collections):
        bpy.data.collections.remove(c)
    for blk in (bpy.data.meshes, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for d in list(blk):
            if d.users == 0:
                blk.remove(d)


def _engine():
    ids = {i.identifier for i in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    return "BLENDER_EEVEE" if "BLENDER_EEVEE" in ids else "BLENDER_EEVEE_NEXT"


def _descendants(ob):
    out = []
    for c in ob.children:
        out.append(c)
        out += _descendants(c)
    return out


def export(r, exp_dir):
    ob = r["object"]
    name = r["export_name"]
    vl = bpy.context.view_layer
    vl.update()
    for o in vl.objects:
        o.select_set(False)
    kids = _descendants(ob)
    for o in [ob] + kids:
        o.hide_set(False)
        o.select_set(True)
    vl.objects.active = ob
    fbx = os.path.join(exp_dir, name + ".fbx")
    glb = os.path.join(exp_dir, name + ".glb")
    bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={"MESH", "EMPTY"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             path_mode="RELATIVE", colors_type="LINEAR")
    for o in kids:
        if o.name.startswith(("UCX_", "SOCKET_")) or o.type == "EMPTY":
            o.select_set(False)
    # GLB d'apercu : images embarquees reduites a 1K (le FBX reference les scans 2K du depot) ; restaurees apres
    scaled = []
    for img in _images_of([ob] + kids):
        if img.packed_file is None and img.filepath and max(img.size) > 1024:
            f = 1024 / max(img.size)
            img.scale(max(1, int(img.size[0] * f)), max(1, int(img.size[1] * f)))
            scaled.append(img)
    try:
        bpy.ops.export_scene.gltf(filepath=glb, export_format="GLB", use_selection=True, export_apply=True,
                                  export_image_format="JPEG", export_image_quality=85)
    except TypeError:
        bpy.ops.export_scene.gltf(filepath=glb, export_format="GLB", use_selection=True, export_apply=True)
    for img in scaled:
        img.reload()
    for o in vl.objects:
        o.select_set(False)
    return [os.path.basename(fbx), os.path.basename(glb)]


def _images_of(objs):
    out = set()
    seen = set()

    def walk(nt):
        if nt is None or nt in seen:
            return
        seen.add(nt)
        for n in nt.nodes:
            if n.type == "TEX_IMAGE" and n.image is not None:
                out.add(n.image)
            elif n.type == "GROUP":
                walk(n.node_tree)
    for o in objs:
        if o.type != "MESH":
            continue
        for slot in o.material_slots:
            if slot.material and slot.material.node_tree:
                walk(slot.material.node_tree)
    return out


def module_report(r, files, dt):
    ob = r["object"]
    bpy.context.view_layer.update()
    objs = [ob] + [c for c in r.get("child_objects", [])]
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    lo = [min(p[k] for p in pts) for k in range(3)]
    hi = [max(p[k] for p in pts) for k in range(3)]
    socks = {}
    for s in r.get("socket_objects", []):
        nm = s.name.split("SOCKET_", 1)[-1]
        socks[nm] = {"pos": [round(v, 4) for v in s.location], "rot": [round(math.degrees(a), 3)
                                                                        for a in s.rotation_euler]}
    return {
        "key": r["key"], "export": r["export_name"], "collection": r["export_name"],
        "dims_m": [round(hi[k] - lo[k], 3) for k in range(3)],
        "bbox": {"min": [round(v, 3) for v in lo], "max": [round(v, 3) for v in hi]},
        "height_vs_bean": round((hi[2] - lo[2]) / 1.05, 2),
        "tris": r["info"].get("tris"), "tris_raw": r["info"].get("tris_raw"), "budget": r["info"].get("budget"),
        "within_budget": (r["info"].get("budget") is None or r["info"]["tris"] <= r["info"]["budget"]),
        "materials": r["info"].get("materials"),
        "ucx": [u.name for u in r.get("ucx_objects", [])],
        "children": [c.name for c in r.get("child_objects", [])],
        "sockets": socks,
        "info": {k: v for k, v in r["info"].items() if k not in ("tris", "tris_raw", "budget", "materials")},
        "files": files, "build_s": round(dt, 2),
    }


def _preview(scene, coll, out_png, size=(960, 640)):
    """Vignette EEVEE d'une collection de module (QA)."""
    import fort_mat  # noqa: F401
    objs = [o for o in coll.all_objects if o.type == "MESH" and not o.name.startswith("UCX_")]
    if not objs:
        return
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    lo = Vector([min(p[k] for p in pts) for k in range(3)])
    hi = Vector([max(p[k] for p in pts) for k in range(3)])
    ctr, rad = (lo + hi) / 2, max((hi - lo).length / 2, 0.05)
    vis = {}
    for c in scene.collection.children_recursive:
        vis[c.name] = c.hide_render
    lc = bpy.context.view_layer.layer_collection

    def walk(l):
        yield l
        for k in l.children:
            yield from walk(k)
    states = {}
    for l in walk(lc):
        if l.collection is scene.collection:
            continue
        states[l.name] = l.exclude
        keep = l.collection is coll or coll.name in [c.name for c in l.collection.children_recursive] or \
            l.collection.name.startswith("PREVIEW")
        l.exclude = not keep
    import bmesh
    gme = bpy.data.meshes.new("PREVIEW_ground")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=max(rad * 4, 4))
    bm.transform(Matrix.Translation((ctr.x, ctr.y, lo.z - 0.005)))
    bm.to_mesh(gme)
    bm.free()
    gm = bpy.data.materials.get("PREVIEW_sand") or bpy.data.materials.new("PREVIEW_sand")
    gm.diffuse_color = (0.55, 0.5, 0.42, 1)
    gm.use_nodes = True
    bsdf = next(n for n in gm.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (0.55, 0.5, 0.42, 1)
    bsdf.inputs["Roughness"].default_value = 0.95
    gme.materials.append(gm)
    gob = bpy.data.objects.new("PREVIEW_ground", gme)
    pc = bpy.data.collections.get("PREVIEW_light")
    pc.objects.link(gob)
    cam_d = bpy.data.cameras.new("PREVIEW_cam")
    cam_d.lens = 40
    cam = bpy.data.objects.new("PREVIEW_cam", cam_d)
    scene.collection.objects.link(cam)
    d = Vector((-0.75, -1.25, 0.7)).normalized()
    half = math.atan(36 / 2 / 40) * size[1] / size[0]
    dist = rad / math.sin(half) * 1.02
    cam.location = ctr + d * dist
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam_d.clip_start = max(0.005, dist / 200)
    cam_d.clip_end = dist * 4
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.get("Render Result")
    if img is not None:
        img.buffers_free()
    bpy.data.objects.remove(cam, do_unlink=True)
    bpy.data.objects.remove(gob, do_unlink=True)
    for l in walk(lc):
        if l.name in states:
            l.exclude = states[l.name]


def _setup_preview_world(scene):
    scene.render.engine = _engine()
    scene.eevee.taa_render_samples = 24
    for attr, val in (("use_raytracing", True), ("use_shadows", True), ("use_fast_gi", True),
                      ("fast_gi_distance", 2.0)):
        try:
            setattr(scene.eevee, attr, val)
        except (AttributeError, TypeError):
            pass
    w = scene.world or bpy.data.worlds.new("PREVIEW_world")
    scene.world = w
    w.use_nodes = True
    nt = w.node_tree
    bg = next(n for n in nt.nodes if n.type == "BACKGROUND")
    hdri = os.path.join(os.path.dirname(os.path.dirname(KITS)), "factory", "assets", "hdri",
                        "kloofendal_48d_partly_cloudy_puresky_2k.hdr")
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(hdri, check_existing=True)
    nt.links.new(env.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 1.0
    coll = bpy.data.collections.new("PREVIEW_light")
    scene.collection.children.link(coll)
    sd = bpy.data.lights.new("PREVIEW_sun", "SUN")
    sd.energy = 4.0
    sd.angle = math.radians(1.5)
    sun = bpy.data.objects.new("PREVIEW_sun", sd)
    coll.objects.link(sun)
    sun.rotation_euler = (math.radians(42), 0, math.radians(-125))
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast" if "AgX - Medium High Contrast" in [
        i.identifier for i in scene.view_settings.bl_rna.properties["look"].enum_items] else "None"
    return coll


def run(mdir, lot):
    t_start = time.time()
    mdir = os.path.abspath(mdir)
    with open(os.path.join(mdir, "maquette.json"), encoding="utf-8") as fh:
        spec = json.load(fh)
    P = spec["params"]
    keys = P["lots"][lot]["modules"]
    import fort_kit as FK
    reg = FK.all_modules()
    _reset()
    scene = bpy.context.scene
    scene.name = "LOT-" + lot
    root = bpy.data.collections.new("LOT-" + lot)
    scene.collection.children.link(root)
    exp_dir = os.path.join(mdir, "exports")
    mod_dir = os.path.join(mdir, "modules")
    os.makedirs(exp_dir, exist_ok=True)
    os.makedirs(mod_dir, exist_ok=True)
    preview = os.environ.get("FORT_PREVIEW") == "1"
    only = [k for k in os.environ.get("FORT_ONLY", "").split(",") if k]
    report = {"lot": lot, "blender": bpy.app.version_string, "modules": {}, "errors": {}}
    built = []
    for key in keys:
        if only and key not in only:
            continue
        if key not in reg:
            report["errors"][key] = "builder absent"
            continue
        t0 = time.time()
        tmp = bpy.data.collections.new("TMP-" + key)
        root.children.link(tmp)
        try:
            results = FK.assemble(key, tmp, registry=reg)
        except Exception:
            report["errors"][key] = traceback.format_exc()
            print(report["errors"][key])
            continue
        for r in results:
            name = r["export_name"]
            coll = tmp if len(results) == 1 else bpy.data.collections.new(name)
            if coll is not tmp:
                root.children.link(coll)
                for o in [r["object"]] + _descendants(r["object"]):
                    for c in list(o.users_collection):
                        c.objects.unlink(o)
                    coll.objects.link(o)
            coll.name = name
            files = []
            if not name.startswith("REF-"):
                files = export(r, exp_dir)
            for s in r.get("socket_objects", []):
                s.name = f"EMP-{name}__{s.name}"
            for u in r.get("ucx_objects", []):
                u.hide_viewport = True
            report["modules"][name] = module_report(r, files, time.time() - t0)
            built.append((name, coll))
            print(f"[fort_lot] {lot}/{key} -> {name} : {report['modules'][name]['tris']} tris "
                  f"({report['modules'][name]['budget']}) en {time.time() - t0:.1f} s")
        if tmp.name.startswith("TMP-") and not tmp.objects:
            bpy.data.collections.remove(tmp)
    # sauvegarde (chemins relatifs, images non empaquetees sauf textures generees)
    blend = os.path.join(mod_dir, lot + ".blend")
    if not only:
        for img in bpy.data.images:
            if img.packed_file is None and img.filepath:
                img.filepath = bpy.path.relpath(bpy.path.abspath(img.filepath), start=mod_dir)
        try:
            bpy.context.preferences.filepaths.save_version = 0      # pas de .blend1 a cote des modules
        except AttributeError:
            pass
        bpy.ops.wm.save_as_mainfile(filepath=blend, check_existing=False, compress=True, relative_remap=True)
    report["build_s"] = round(time.time() - t_start, 1)
    report["images"] = sorted({os.path.basename(i.filepath) for i in bpy.data.images if i.filepath})
    if not only:
        with open(os.path.join(mod_dir, lot + ".json"), "w", encoding="utf-8") as fh:
            json.dump(report, fh, indent=2, ensure_ascii=False, default=str)
    if preview:
        out = os.path.join(mdir, "_tests", "out")
        os.makedirs(out, exist_ok=True)
        _setup_preview_world(scene)
        for name, coll in built:
            try:
                _preview(scene, coll, os.path.join(out, f"{lot}__{name}.png"))
            except Exception:
                traceback.print_exc()
    if report["errors"]:
        print("[fort_lot] ERREURS :", list(report["errors"]))
    return report


def _main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) < 2:
        print("usage : blender -b --factory-startup --python fort_lot.py -- <dir maquette> <lot>")
        sys.exit(2)
    try:
        rep = run(argv[0], argv[1])
    except Exception:
        traceback.print_exc()
        sys.exit(1)
    sys.exit(1 if rep["errors"] else 0)


if __name__ == "__main__":
    _main()
