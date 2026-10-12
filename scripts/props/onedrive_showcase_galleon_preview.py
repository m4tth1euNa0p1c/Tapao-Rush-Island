"""Render the exact T184 FBX with source PBR materials; save a local Blender maquette."""
import json
import re
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
MODEL=Path("C:/Users/matth/OneDrive/maquette-blender/maquettes/galion-anglais")
OUT=ROOT/"art/_previews/galleon_t184"
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(MODEL/"galion-anglais.blend"))
materials={m.name:m for m in bpy.data.materials}
for obj in bpy.data.objects:
    if obj.type=="MESH":
        obj.hide_render=True
        obj.hide_set(True)
bpy.ops.import_scene.fbx(filepath=str(ROOT/"art/export/Galleon_T184/SM_Galleon.fbx"))
for obj in bpy.context.selected_objects:
    if obj.type!="MESH" or obj.name.startswith("UCX_"):
        obj.hide_render=True;obj.hide_set(True)
        continue
    for slot in obj.material_slots:
        name=re.sub(r"\.\d+$","",slot.material.name)
        slot.material=materials[name]
    obj.hide_render=False;obj.hide_set(False)
    hull=obj
scene=bpy.context.scene
samples=[]
for y in (-5.3,-4.9,-4.5,-4,-3.5,-3,-2.5,-2.1):
    origin=Vector((0,y,2.8)); layers=[]
    for _ in range(12):
        hit,point,normal,index=hull.ray_cast(hull.matrix_world.inverted()@origin,Vector((0,0,-1)))
        if not hit:
            break
        point=hull.matrix_world@point
        layers.append({"z":round(point.z,4),"material":hull.data.materials[hull.data.polygons[index].material_index].name})
        origin=point-Vector((0,0,.001))
    samples.append({"y":y,"layers":layers})
(OUT/"stair-opening-rays.json").write_text(json.dumps(samples,indent=2),encoding="utf-8")
scene.render.engine="CYCLES";scene.cycles.samples=20;scene.cycles.use_denoising=True
scene.render.resolution_x=1100;scene.render.resolution_y=760;scene.render.resolution_percentage=100
scene.render.image_settings.file_format="PNG"
data=bpy.data.cameras.new("T184_PreviewCamera");cam=bpy.data.objects.new(data.name,data)
scene.collection.objects.link(cam);scene.camera=cam;cam.data.lens=20
for i,(pos,power) in enumerate([((0,-3,1.28),65),((0,1,1.24),85),((0,5,1.24),70)]):
    d=bpy.data.lights.new(f"T184_HoldPreviewLight_{i}","POINT");d.energy=power;d.color=(1,.67,.36);d.shadow_soft_size=.18
    ob=bpy.data.objects.new(d.name,d);scene.collection.objects.link(ob);ob.location=pos
for name,pos,target,lens in [("hold",(0,-1.5,1.02),(1.25,4.2,.9),19),
                              ("hold_cannons",(.2,2,1.12),(2.65,1.6,.88),22),
                              ("stairwell",(-1.35,-5.4,2.7),(0,-3.1,.75),23),
                              ("hold_stairs",(0,.2,.98),(0,-4.7,1.4),20)]:
    cam.location=pos;cam.data.lens=lens;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat("-Z","Y").to_euler()
    scene.render.filepath=str(OUT/(name+".png"));bpy.ops.render.render(write_still=True)
dest=ROOT/"art/blender/galleon_t184_interior.blend"
bpy.ops.wm.save_as_mainfile(filepath=str(dest),compress=True)
print("T184_PREVIEWS_SUCCESS",str(OUT))
