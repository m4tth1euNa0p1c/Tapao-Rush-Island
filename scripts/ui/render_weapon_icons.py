"""Icons of the v2 weapons and of their ammunition for the inventory cells and the shop rows (owner 2026-10-09:
"ajoute bien la modélisation dans inventaire et boutique des modèles de nouvelles armes", T-264).

Each icon is a render of the very model the game shows (the pickup static meshes of the weapon packs, GLB with their
baked textures), on a transparent background, framed like the kit pictures of the TAPAO interface (art/ui/tapao/
sprites/items/item_musket.png): long arms on the diagonal, butt low on the left and muzzle high on the right; pistols
side on, muzzle to the right; packets and boxes in three quarter view. A soft shadow keeps the outline readable on the
dark cells.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/ui/render_weapon_icons.py
  [-- --only Rifle,SM_Ammo_ShotBag]

Output: art/ui/icons/weapons/T_Icon_<Name>.png (512 x 512 RGBA) + icons.json; then the soft shadow and the contact
sheet _sheet.png by scripts/ui/finish_weapon_icons.py (system Python with Pillow: Blender's Python has no Pillow).
Packs: build/packs/<pack>/exports/Static/*.glb (+ build/packs/ammo/exports). The convention of the packs: muzzle +X,
up +Z (metres).
"""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Euler, Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
PACKS = ROOT / "build/packs"
EXPORTS = ROOT / "art/export/Weapons"          # packs written into the repository (T-334: pack.json)
OUT = ROOT / "art/ui/icons/weapons"
SIZE = 512

# icon name -> (glb, pose)
WEAPONS = {
    "Musket": ("bean-musket-anim/exports/Static/SM_Bean_PirateMusket.glb", "long"),
    "Pistol": ("bean-pirate-pistol/exports/Static/SM_Bean_PiratePistol.glb", "pistol"),
    "DoublePistol": ("bean-pirate-pistol-double/exports/Static/SM_Bean_PirateDoublePistol.glb", "pistol_top"),
    "Shotgun": ("bean-pirate-shotgun/exports/Static/SM_Bean_PirateShotgun.glb", "long"),
    "Blunderbuss": ("bean-blunderbuss-anim/exports/Static/SM_Bean_PirateBlunderbuss.glb", "long"),
    "Rifle": ("bean-rifle-anim/exports/Static/SM_Bean_PirateRifle.glb", "long"),
    "Grenade": ("bean-grenade/exports/Static/SM_Bean_PirateGrenade.glb", "object"),
}


def machinegun_specs():
    """The crank machine gun of T-272 (T-273), once its pack has exported the pickup model: build/packs/<pack>/exports/
    Static/SM_Bean_MachineGun.glb, a long arm on the diagonal; its box SM_Ammo_MGBox comes through ammo_specs"""
    for glb in sorted(PACKS.glob("*/exports/**/SM_Bean_MachineGun.glb")):
        return {"MachineGun": (str(glb.relative_to(PACKS)).replace("\\", "/"), "long")}
    return {}


def period_arms_specs():
    """The T-334 period weapons, built into art/export/Weapons/<Weapon>/ (pack.json): the bolt-action sniper rifle, the
    drum sub-machine gun and the bayonet musket (long arms on the diagonal), and their ammunition models (SM_Ammo_*)"""
    specs = {}
    for name in ("BoltRifle", "Mitraillette", "BayonetMusket"):
        static = EXPORTS / name / "exports" / "Static"
        glb = static / f"SM_Bean_{name}.glb"
        if glb.exists():
            specs[name] = (str(glb), "long")
        for ammo in sorted(static.glob("SM_Ammo_*.glb")):
            specs.setdefault(ammo.stem, (str(ammo), "object"))
    return specs


def ammo_specs():
    """Every SM_Ammo_* model of the packs, icon named after the mesh (T_Icon_SM_Ammo_X)"""
    specs = {}
    for glb in sorted(PACKS.glob("*/exports/**/SM_Ammo_*.glb")):
        if ".onedrive" in str(glb) or ".bak" in str(glb):
            continue
        specs.setdefault(glb.stem, (str(glb.relative_to(PACKS)).replace("\\", "/"), "object"))
    return specs


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


def import_model(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    objects = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in objects if o.type == "MESH" and not o.name.upper().startswith(("UCX_", "UBX_", "USP_", "UCP_"))]
    for o in objects:
        if o.type == "MESH" and o not in meshes:
            o.hide_render = True
    # one parent to turn the whole model (glTF imports may nest the meshes under empties)
    pivot = bpy.data.objects.new("PIVOT", None)
    bpy.context.scene.collection.objects.link(pivot)
    for o in objects:
        if o.parent is None:
            o.parent = pivot
    return pivot, meshes


def world_points(meshes):
    points = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for o in meshes:
        evaluated = o.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        matrix = o.matrix_world
        step = max(1, len(mesh.vertices) // 4000)
        points.extend(matrix @ mesh.vertices[i].co for i in range(0, len(mesh.vertices), step))
        evaluated.to_mesh_clear()
    return points


def pose(pivot, kind):
    """Turns the model in front of a camera looking along +Y (screen right = +X, up = +Z); a positive turn about X
    shows the top of the model"""
    if kind == "long":
        # muzzle (+X) up to the right on the diagonal, a little of the top seen
        pivot.rotation_euler = Euler((math.radians(14), math.radians(-38), math.radians(-10)), "XYZ")
    elif kind == "pistol":
        pivot.rotation_euler = Euler((math.radians(12), math.radians(-8), math.radians(-14)), "XYZ")
    elif kind == "pistol_top":
        # the double pistol from higher up: both barrels and both locks show
        pivot.rotation_euler = Euler((math.radians(40), math.radians(-8), math.radians(-18)), "XYZ")
    else:
        # three quarter view from above, front to the left
        pivot.rotation_euler = Euler((math.radians(26), 0, math.radians(-35)), "XYZ")
    bpy.context.view_layer.update()


def frame(scene, meshes, margin=1.14):
    points = world_points(meshes)
    xs = [p.x for p in points]
    zs = [p.z for p in points]
    ys = [p.y for p in points]
    centre = Vector(((min(xs) + max(xs)) / 2, min(ys) - 2.0, (min(zs) + max(zs)) / 2))
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


def render_one(name, rel, kind):
    scene = reset()
    path = Path(rel) if Path(rel).is_absolute() else PACKS / rel
    if not path.exists():
        return {"name": name, "error": f"missing {rel}"}
    pivot, meshes = import_model(path)
    if not meshes:
        return {"name": name, "error": "no mesh"}
    pose(pivot, kind)
    extent = frame(scene, meshes)
    s = max(extent, 0.05)
    light(scene, "KEY", (-1.6 * s - 0.4, -2.2 * s - 0.6, 2.0 * s + 0.6), 420 * s * s + 60, 1.6 * s + 0.3, (1.0, 0.96, 0.9))
    light(scene, "FILL", (2.0 * s + 0.4, -1.6 * s - 0.5, 0.6 * s + 0.2), 160 * s * s + 25, 2.0 * s + 0.4, (0.85, 0.9, 1.0))
    light(scene, "RIM", (0.4 * s, 2.4 * s + 0.6, 1.6 * s + 0.4), 300 * s * s + 40, 1.2 * s + 0.3)
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / f"T_Icon_{name}.png"
    scene.render.filepath = str(target)
    bpy.ops.render.render(write_still=True)
    images = sorted({i.name for i in bpy.data.images if i.source == "FILE" or i.packed_file})
    if Path(rel).is_absolute() and Path(rel).is_relative_to(ROOT):
        rel = str(Path(rel).relative_to(ROOT)).replace("\\", "/")
    return {"name": name, "glb": rel, "pose": kind, "png": str(target.relative_to(ROOT)).replace("\\", "/"),
            "meshes": len(meshes), "extent_m": round(extent, 3), "textures": images[:8]}


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    only = set(args[args.index("--only") + 1].split(",")) if "--only" in args else None
    specs = dict(WEAPONS)
    specs.update(machinegun_specs())
    specs.update(period_arms_specs())
    specs.update(ammo_specs())
    rows = []
    for name, (rel, kind) in specs.items():
        if only and name not in only:
            continue
        try:
            rows.append(render_one(name, rel, kind))
        except Exception as error:  # noqa: BLE001
            rows.append({"name": name, "error": repr(error)})
        print("ICON", json.dumps(rows[-1]))
    previous = {}
    if (OUT / "icons.json").exists():
        previous = {r["name"]: r for r in json.loads((OUT / "icons.json").read_text(encoding="utf-8"))}
    previous.update({r["name"]: r for r in rows})
    (OUT / "icons.json").write_text(json.dumps(list(previous.values()), indent=1), encoding="utf-8")


main()
