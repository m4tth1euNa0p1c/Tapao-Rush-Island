"""Correct the Blender armature unit conversion on the isolated merchant clip."""
import datetime as dt
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
DEST="/Game/MonkeIsland/Characters/BeanMerchant"
anim=ue.load_asset(DEST+"/Animations/A_BeanMerchant_GreetIdle")
body=ue.load_asset(DEST+"/Meshes/SK_BeanMerchant")

def poses():
    return {name:str(ue.AnimationLibrary.get_bone_pose_for_time(anim,name,0,False))
            for name in ("root","pelvis","hand_r","foot_r")}

before=poses()
# Atomic FBX reimport uses the asset's saved import data, not only task.options.
anim.get_editor_property("asset_import_data").set_editor_property("import_uniform_scale",100.0)
options=ue.FbxImportUI()
options.import_mesh=False
options.import_animations=True
options.import_materials=False
options.import_textures=False
options.import_as_skeletal=True
options.skeleton=body.get_editor_property("skeleton")
options.mesh_type_to_import=ue.FBXImportType.FBXIT_ANIMATION
for data in (options.skeletal_mesh_import_data,options.anim_sequence_import_data):
    data.set_editor_property("convert_scene",True)
    data.set_editor_property("convert_scene_unit",True)
    data.set_editor_property("import_uniform_scale",100.0)
data=options.anim_sequence_import_data
data.set_editor_property("remove_redundant_keys",False)
data.set_editor_property("animation_length",ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
task=ue.AssetImportTask()
task.filename=str(ROOT/"art/export/Characters/BeanMerchant/A_BeanMerchant_GreetIdle.fbx")
task.destination_path=DEST+"/Animations"
task.destination_name="A_BeanMerchant_GreetIdle"
task.replace_existing=True
task.replace_existing_settings=True
task.automated=True
task.save=True
task.options=options
world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
flag="Interchange.FeatureFlags.Import.FBX"
previous=ue.SystemLibrary.get_console_variable_int_value(flag)
ue.SystemLibrary.execute_console_command(world,flag+" 0")
try:
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
finally:
    ue.SystemLibrary.execute_console_command(world,flag+" "+str(previous))
result={"before":before,"after":poses(),"body_bounds":str(body.get_imported_bounds()),
        "finished_at":dt.datetime.now(dt.timezone.utc).isoformat()}
(ROOT/"monke_island/Saved/T181/animation-units.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
