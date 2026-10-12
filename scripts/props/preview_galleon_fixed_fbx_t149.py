"""Render the repaired FBX, reusing the source scene's materials and lighting."""
import json
import re
from pathlib import Path
import bpy
from mathutils import Vector

root = Path(__file__).resolve().parents[2]
model = Path("C:/Users/matth/OneDrive/maquette-blender/maquettes/galion-anglais")
staged = root / "art/export/Galleon_T149/20261006_1203"
bpy.ops.wm.open_mainfile(filepath=str(model / "galion-anglais.blend"))
materials = {m.name: m for m in bpy.data.materials}
report = json.loads((staged / "galleon-report.json").read_text(encoding="utf-8"))
for names in report["objects"].values():
    for name in names:
        bpy.data.objects[name].hide_render = True
bpy.ops.import_scene.fbx(filepath=str(staged / "materialfix/SM_Galleon.fbx"))
for obj in bpy.context.selected_objects:
    if obj.type != "MESH" or obj.name.startswith("UCX_"):
        obj.hide_render = True
        continue
    for slot in obj.material_slots:
        name = re.sub(r"\.\d+$", "", slot.material.name)
        slot.material = materials[name]
    obj.hide_render = False
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x = scene.render.resolution_y = 850
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
out = root / "monke_island/Saved/T149"
out.mkdir(parents=True, exist_ok=True)
scene.camera = bpy.data.objects["CAM-detail_canon"]
scene.render.filepath = str(out / "cannons-fixed-fbx.png")
bpy.ops.render.render(write_still=True)
scene.camera.location = (0.0, 5.0, 2.4)
scene.camera.rotation_euler = (Vector((0.2, 9.3, 2.15)) - scene.camera.location).to_track_quat("-Z", "Y").to_euler()
scene.camera.data.lens = 18
scene.render.filepath = str(out / "cabin-fixed-fbx.png")
bpy.ops.render.render(write_still=True)
print("T149_FIXED_FBX_PREVIEWS " + str(out))
