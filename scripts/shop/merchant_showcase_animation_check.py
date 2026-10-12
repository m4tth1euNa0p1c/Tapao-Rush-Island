"""Round-trip the exported FBX greeting independently of the source Blender rig."""
import json
from pathlib import Path
import bpy
ROOT=Path(__file__).resolve().parents[2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(ROOT/"art/export/Characters/BeanMerchant/A_BeanMerchant_GreetIdle.fbx"))
arm=next(o for o in bpy.data.objects if o.type=="ARMATURE")
rows=[]
for frame in (1,50,100,120,145,200,289):
    bpy.context.scene.frame_set(frame)
    p=arm.matrix_world@arm.pose.bones["hand_r"].matrix.translation
    rows.append({"frame":frame,"hand":list(p)})
result={"actions":[a.name for a in bpy.data.actions],"samples":rows}
(ROOT/"monke_island/Saved/T181/exported-animation-check.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
print(json.dumps(result))
