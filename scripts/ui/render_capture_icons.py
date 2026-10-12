"""Inventory icons of the capture items (T-337, lot L5): the prisoner cage and the steel handcuffs, rendered from the very
models the game shows (GLB copies of art/export/Props/Capture/*.fbx in build/capture_kit/glb, written by
scripts/props/capture_kit/build.py), framed like the v2 weapon
icons (scripts/ui/render_weapon_icons.py): objects in three quarter view from above, transparent background.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/ui/render_capture_icons.py

Output: art/ui/icons/capture/_raw/T_Icon_<Name>.png (512 x 512 RGBA) + icons.json; then the soft shadow and the contact
sheet by scripts/ui/finish_capture_icons.py (system Python with Pillow). Blender in the background only, a few seconds per
icon (RAM first for Unreal).
  T_Icon_Capture_Cage            the cage standing, door closed (the item "cage")
  T_Icon_Capture_Cage_Folded     the folded cage, the object carried in hand
  T_Icon_Steel_Handcuffs         the closed cuffs and their chain (the item "menottes en acier")
  T_Icon_Steel_Handcuffs_Open    the open cuffs, as held in hand
"""
import json
import math
from pathlib import Path

import bpy
from mathutils import Euler, Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "art/export/Props/Capture"          # manifest.json (sockets)
GLB = ROOT / "build/capture_kit/glb"              # the GLB copies written by build.py
OUT = ROOT / "art/ui/icons/capture"
SIZE = 512

# icon -> (glb files with their placement (Blender metres) in the icon's model space, turn (deg X, Y, Z))
ICONS = {
    "Capture_Cage": ([("SM_Capture_Cage.glb", None), ("SM_Capture_Cage_Door.glb", "door_hinge")], (24, 0, -38)),
    "Capture_Cage_Folded": ([("SM_Capture_Cage_Folded.glb", None)], (24, 0, -32)),
    "Steel_Handcuffs": ([("SM_Steel_Handcuffs.glb", None)], (58, 0, 8)),
    "Steel_Handcuffs_Open": ([("SM_Steel_Handcuffs_Open.glb", None)], (10, 0, -18)),
}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_EEVEE"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = scene.render.resolution_y = SIZE
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    try:
        scene.view_settings.view_transform = "AgX"
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass
    try:
        scene.eevee.taa_render_samples = 64
    except AttributeError:
        pass
    world = bpy.data.worlds.new("World-Icon")
    scene.world = world
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs["Color"].default_value = (0.55, 0.57, 0.6, 1)
    bg.inputs["Strength"].default_value = 0.6
    return scene


def light(scene, name, location, energy, size, color=(1, 1, 1)):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.size = size
    data.color = color
    ob = bpy.data.objects.new(name, data)
    scene.collection.objects.link(ob)
    ob.location = location
    ob.rotation_euler = (Vector((0, 0, 0)) - Vector(location)).to_track_quat("-Z", "Y").to_euler()
    return ob


def sockets():
    manifest = json.loads((SRC / "manifest.json").read_text(encoding="utf-8"))
    return {name: row.get("sockets", {}) for name, row in manifest["assets"].items()}


def import_glb(path, pivot, offset):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    objects = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in objects if o.type == "MESH"]
    holder = bpy.data.objects.new("HOLD-" + path.stem, None)
    bpy.context.scene.collection.objects.link(holder)
    holder.location = offset
    holder.parent = pivot
    for o in objects:
        if o.parent is None:
            o.parent = holder
    return meshes


def world_points(meshes):
    points = []
    dg = bpy.context.evaluated_depsgraph_get()
    for o in meshes:
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        step = max(1, len(me.vertices) // 4000)
        points.extend(o.matrix_world @ me.vertices[i].co for i in range(0, len(me.vertices), step))
        ev.to_mesh_clear()
    return points


def frame(scene, meshes, margin=1.10):
    pts = world_points(meshes)
    xs, ys, zs = [p.x for p in pts], [p.y for p in pts], [p.z for p in pts]
    centre = Vector(((min(xs) + max(xs)) / 2, min(ys) - 3.0, (min(zs) + max(zs)) / 2))
    extent = max(max(xs) - min(xs), max(zs) - min(zs))
    data = bpy.data.cameras.new("CAM-Icon")
    data.type = "ORTHO"
    data.ortho_scale = extent * margin
    data.clip_start = 0.01
    data.clip_end = 100
    cam = bpy.data.objects.new("CAM-Icon", data)
    scene.collection.objects.link(cam)
    cam.location = centre
    cam.rotation_euler = Euler((math.radians(90), 0, 0), "XYZ")  # looks along +Y
    scene.camera = cam
    return extent


def render_one(name, parts, turn, socks):
    scene = reset()
    pivot = bpy.data.objects.new("PIVOT", None)
    scene.collection.objects.link(pivot)
    meshes = []
    for glb, socket in parts:
        path = GLB / glb
        if not path.exists():
            return {"name": name, "error": f"missing {path.relative_to(ROOT).as_posix()}"}
        offset = (0, 0, 0)
        if socket:
            offset = tuple(socks["SM_Capture_Cage"][socket]["blender_m"])
        meshes += import_glb(path, pivot, offset)
    # centre the model on the pivot, then turn it in front of the camera (looking along +Y)
    bpy.context.view_layer.update()
    pts = world_points(meshes)
    c = sum(pts, Vector()) / len(pts)
    for child in pivot.children:
        child.location = child.location - c
    pivot.rotation_euler = Euler([math.radians(v) for v in turn], "XYZ")
    bpy.context.view_layer.update()
    extent = frame(scene, meshes)
    s = max(extent, 0.05)
    light(scene, "KEY", (-1.6 * s - 0.4, -2.2 * s - 0.6, 2.0 * s + 0.6), 420 * s * s + 60, 1.6 * s + 0.3, (1.0, 0.96, 0.9))
    light(scene, "FILL", (2.0 * s + 0.4, -1.6 * s - 0.5, 0.6 * s + 0.2), 160 * s * s + 25, 2.0 * s + 0.4, (0.85, 0.9, 1.0))
    light(scene, "RIM", (0.4 * s, 2.4 * s + 0.6, 1.6 * s + 0.4), 300 * s * s + 40, 1.2 * s + 0.3)
    target = OUT / "_raw" / f"T_Icon_{name}.png"
    target.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(target)
    bpy.ops.render.render(write_still=True)
    return {"name": name, "glb": [p for p, _ in parts], "turn_deg": list(turn),
            "raw": target.relative_to(ROOT).as_posix(), "png": f"art/ui/icons/capture/T_Icon_{name}.png",
            "meshes": len(meshes), "extent_m": round(extent, 3)}


def main():
    socks = sockets()
    rows = []
    for name, (parts, turn) in ICONS.items():
        try:
            rows.append(render_one(name, parts, turn, socks))
        except Exception as error:  # noqa: BLE001 - reported in icons.json, the others go on
            rows.append({"name": name, "error": repr(error)})
        print("ICON", json.dumps(rows[-1]))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "icons.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")


main()
