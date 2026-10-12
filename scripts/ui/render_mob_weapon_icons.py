"""Icons of the mobs' weapons the bean can pick up (T-161): the inventory cell pictures of DA_Item_Spear,
DA_Item_ObsidianClub and DA_Item_ShellHammer, rendered from the very static meshes the game shows on the ground
(art/export/Characters/<Mob>/SM_<Mob>_<Weapon>.fbx, the BP_Drop_* / pickup meshes), framed like the kit pictures of the
TAPAO interface and the v2 weapons (scripts/ui/render_weapon_icons.py): long arms on the diagonal, grip low on the left,
head high on the right, transparent background.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/ui/render_mob_weapon_icons.py

Output: art/ui/icons/mob_weapons/_raw/T_Icon_<Weapon>.png (512 x 512 RGBA) + icons.json; then the soft shadow and the
contact sheet by scripts/ui/finish_mob_weapon_icons.py (system Python with Pillow). The meshes are in grip space with
their long axis on +Z (scripts/character/tiki/export.py): turned onto +X before the pose. Blender in the background
only, a few seconds per icon (RAM first for Unreal).
"""
import json
import math
from pathlib import Path

import bpy
from mathutils import Euler, Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/ui/icons/mob_weapons"
SIZE = 512

# icon name -> static mesh of the pickup (grip space, long axis +Z)
WEAPONS = {
    "Spear": "art/export/Characters/CocoWarrior/Horror/SM_CocoWarrior_Spear.fbx",
    "ObsidianClub": "art/export/Characters/AshWatcher/Horror/SM_AshWatcher_ObsidianClub.fbx",
    "ShellHammer": "art/export/Characters/CrabGuardian/Horror/SM_CrabGuardian_ShellHammer.fbx",
}
# colours of their materials (scripts/character/tiki/creatures.py, SPECS palettes): the textured ones ("tex", "lava")
# leave the FBX white, so they take the middle of their dark and light tones; "emit" glows. Key: M_<Mob>_<Key>.
PALETTE = {
    "CocoWarrior": {"Knot": ("54321A", None), "Rope": ("B3801E", None), "Stone": ("97938B", None), "Wood": ("634022", None)},
    "AshWatcher": {"Handle": ("4A2F1A", None), "Rock": ("2B2826", None), "Rope": ("641D17", None), "Seam": ("FF5A14", 1.6)},
    "CrabGuardian": {"Rope": ("B3801E", None), "Shell": ("F0B888", None), "Wood": ("634022", None)},
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
    background = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    background.inputs["Color"].default_value = (0.55, 0.57, 0.6, 1)
    background.inputs["Strength"].default_value = 0.6
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
    """The FBX under ALIGN (long axis +Z turned onto +X) under PIVOT (the pose of the icon)"""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    objects = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in objects if o.type == "MESH" and not o.name.upper().startswith(("UCX_", "UBX_", "USP_", "UCP_"))]
    for o in objects:
        if o.type == "MESH" and o not in meshes:
            o.hide_render = True
    pivot = bpy.data.objects.new("PIVOT", None)
    align = bpy.data.objects.new("ALIGN", None)
    bpy.context.scene.collection.objects.link(pivot)
    bpy.context.scene.collection.objects.link(align)
    align.parent = pivot
    # a quarter turn about Y takes +Z to +X: the head of the weapon to the right
    align.rotation_euler = Euler((0.0, math.radians(90.0), 0.0), "XYZ")
    for o in objects:
        if o.parent is None:
            o.parent = align
    return pivot, meshes


def linear(hexa):
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [1.0]


def paint(meshes, rel):
    """The palette colours on the materials of the FBX (a textured tiki material comes in white)"""
    mob = Path(rel).parent.name
    painted = []
    for o in meshes:
        for slot in o.material_slots:
            material = slot.material
            if not material:
                continue
            key = material.name.split(".")[0].replace(f"M_{mob}_", "")
            colour, glow = PALETTE.get(mob, {}).get(key, (None, None))
            if not colour:
                continue
            material.use_nodes = True
            bsdf = next((n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
            if not bsdf:
                continue
            for link in list(bsdf.inputs["Base Color"].links):
                material.node_tree.links.remove(link)
            bsdf.inputs["Base Color"].default_value = linear(colour)
            # the FBX may carry an emissive colour (the lava of the obsidian): only the seams glow
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = 0.0
            if glow:
                for name in ("Emission Color", "Emission"):
                    if name in bsdf.inputs:
                        bsdf.inputs[name].default_value = linear(colour)
                        break
                if "Emission Strength" in bsdf.inputs:
                    bsdf.inputs["Emission Strength"].default_value = glow
            painted.append(material.name)
    return painted


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


def frame(scene, meshes, margin=1.12):
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


def render_one(name, rel):
    scene = reset()
    path = ROOT / rel
    if not path.exists():
        return {"name": name, "error": f"missing {rel}"}
    pivot, meshes = import_model(path)
    if not meshes:
        return {"name": name, "error": "no mesh"}
    painted = paint(meshes, rel)
    # head up to the right on the diagonal, a little of its top seen (the "long" pose of the v2 weapons)
    pivot.rotation_euler = Euler((math.radians(14), math.radians(-40), math.radians(-10)), "XYZ")
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
    materials = sorted({s.material.name for o in meshes for s in o.material_slots if s.material})
    return {"name": name, "fbx": rel, "raw": str(target.relative_to(ROOT)).replace("\\", "/"),
            "png": f"art/ui/icons/mob_weapons/T_Icon_{name}.png", "meshes": len(meshes), "extent_m": round(extent, 3),
            "materials": materials, "painted": painted}


def main():
    rows = []
    for name, rel in WEAPONS.items():
        try:
            rows.append(render_one(name, rel))
        except Exception as error:  # noqa: BLE001 - reported in icons.json, the others go on
            rows.append({"name": name, "error": repr(error)})
        print("ICON", json.dumps(rows[-1]))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "icons.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")


main()
