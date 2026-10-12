"""Import T-181 and build a cosmetic child of the existing authoritative shop.

Full editor only, shared lock held. Keeps the T-177 catalog, purchase RPC and
TAPAO screen; the stand and displayed items are fixed cosmetic components.
"""
import datetime as dt
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/"art/export/Characters/BeanMerchant"
OUT=ROOT/"monke_island/Saved/T181"
DEST="/Game/MonkeIsland/Characters/BeanMerchant"
BP_PATH="/Game/MonkeIsland/Galleon/Shop/Merchant/BP_BeanMerchant"
CATALOG="/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
EAL=ue.EditorAssetLibrary
TOOLS=ue.AssetToolsHelpers.get_asset_tools()
MEL=ue.MaterialEditingLibrary


def save(obj):
    assert EAL.save_loaded_asset(obj,False),obj.get_path_name()


def load(path):
    return ue.load_asset(path) if EAL.does_asset_exist(path) else None


def material(name,recipe):
    path=DEST+"/Materials/"+name
    obj=load(path) or TOOLS.create_asset(name,DEST+"/Materials",ue.Material,ue.MaterialFactoryNew())
    MEL.delete_all_material_expressions(obj)
    rgb=[int(recipe["color"][i:i+2],16)/255 for i in (0,2,4)]
    rgb=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb]
    color=MEL.create_material_expression(obj,ue.MaterialExpressionConstant3Vector,-300,0)
    color.set_editor_property("constant",ue.LinearColor(*rgb,1))
    assert MEL.connect_material_property(color,"",ue.MaterialProperty.MP_BASE_COLOR)
    for index,(key,prop) in enumerate((("roughness",ue.MaterialProperty.MP_ROUGHNESS),("metallic",ue.MaterialProperty.MP_METALLIC))):
        value=MEL.create_material_expression(obj,ue.MaterialExpressionConstant,-300,200+index*150)
        value.set_editor_property("r",float(recipe[key]))
        assert MEL.connect_material_property(value,"",prop)
    MEL.recompile_material(obj);save(obj)
    return obj


def import_asset(name,kind,skeleton=None):
    folder=DEST+("/Animations" if kind=="animation" else "/Meshes")
    existing=load(folder+"/"+name)
    if existing:
        return existing
    options=ue.FbxImportUI()
    options.import_materials=False;options.import_textures=False
    options.import_mesh=kind!="animation"
    options.import_animations=kind=="animation"
    options.import_as_skeletal=kind!="static"
    options.create_physics_asset=False
    if skeleton:
        options.skeleton=skeleton
    options.mesh_type_to_import={"static":ue.FBXImportType.FBXIT_STATIC_MESH,"skeletal":ue.FBXImportType.FBXIT_SKELETAL_MESH,
                                 "animation":ue.FBXImportType.FBXIT_ANIMATION}[kind]
    data=options.static_mesh_import_data if kind=="static" else options.skeletal_mesh_import_data if kind=="skeletal" else options.anim_sequence_import_data
    data.set_editor_property("convert_scene",True)
    data.set_editor_property("convert_scene_unit",True)
    # Animation-only FBX loses the Blender Armature unit scale when UE removes
    # that wrapper node. Restore its 100 cm/m scale for the merchant skeleton.
    data.set_editor_property("import_uniform_scale",100.0 if kind=="animation" else 1.0)
    if kind=="static":
        data.set_editor_property("combine_meshes",True)
        data.set_editor_property("auto_generate_collision",False)
    if kind=="animation":
        data.set_editor_property("animation_length",ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        data.set_editor_property("remove_redundant_keys",False)
    task=ue.AssetImportTask();task.filename=str(SOURCE/(name+".fbx"))
    task.destination_path=folder;task.destination_name=name
    task.automated=True;task.save=True;task.options=options
    TOOLS.import_asset_tasks([task])
    result=load(folder+"/"+name)
    assert result,name
    return result


def main():
    lock=json.loads((ROOT/".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    assert lock["owner"]=="codex" and dt.datetime.fromisoformat(lock["expires"])>dt.datetime.now().astimezone()
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor()
    report=json.loads((SOURCE/"merchant-report.json").read_text(encoding="utf-8"))
    mats={name:material(name,recipe) for name,recipe in report["materials"].items()}
    body=import_asset("SK_BeanMerchant","skeletal")
    stand=import_asset("SM_MerchantStand","static")
    anim=import_asset("A_BeanMerchant_GreetIdle","animation",body.get_editor_property("skeleton"))
    assert abs(anim.get_play_length()-12)<.1,anim.get_play_length()
    for obj,prop in ((body,"materials"),(stand,"static_materials")):
        slots=list(obj.get_editor_property(prop))
        for slot in slots:
            name=str(slot.get_editor_property("material_slot_name"))
            assert name in mats,(obj.get_name(),name)
            slot.set_editor_property("material_interface",mats[name])
        obj.set_editor_property(prop,slots);save(obj)
    catalog=load(CATALOG)
    assert catalog
    offers=list(catalog.get_editor_property("offers"))
    assert len(offers)==4,"Review display layout if catalog changed"
    blueprint=load(BP_PATH)
    if not blueprint:
        factory=ue.BlueprintFactory();factory.set_editor_property("parent_class",ue.ShopkeeperActor)
        blueprint=TOOLS.create_asset("BP_BeanMerchant",BP_PATH.rsplit("/",1)[0],ue.Blueprint,factory)
    sub=ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
    lib=ue.SubobjectDataBlueprintFunctionLibrary

    def component(cls,name):
        handles=sub.k2_gather_subobject_data_for_blueprint(blueprint)
        for handle in handles:
            obj=lib.get_object(lib.get_data(handle))
            if obj and obj.get_name().startswith(name):
                return obj
        params=ue.AddNewSubobjectParams(parent_handle=handles[0],new_class=cls,blueprint_context=blueprint)
        handle,error=sub.add_new_subobject(params)
        assert str(error) in ("","None"),str(error)
        sub.rename_subobject(handle,ue.Text(name))
        return lib.get_object(lib.get_data(handle))

    stand_comp=component(ue.StaticMeshComponent,"MerchantStand")
    stand_comp.set_editor_property("static_mesh",stand)
    stand_comp.set_editor_property("relative_location",ue.Vector(0,0,-55))
    stand_comp.set_editor_property("relative_rotation",ue.Rotator(roll=0.0, pitch=0.0, yaw=-90))
    stand_comp.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    stand_comp.set_mobility(ue.ComponentMobility.MOVABLE)
    # Actor faces +X. These simple boxes follow the animated hull attachment,
    # with an open space behind the counter for the Bean and its arms.
    for name,position,extent in (("CounterCollision",(73,0,-25),(30,85,30)),
                               ("PostLeftCollision",(46,-86,28),(3,3,83)),
                               ("PostRightCollision",(46,86,28),(3,3,83))):
        comp=component(ue.BoxComponent,name)
        comp.set_editor_property("relative_location",ue.Vector(*position))
        comp.set_editor_property("box_extent",ue.Vector(*extent))
        comp.set_collision_profile_name("BlockAll")
        comp.set_mobility(ue.ComponentMobility.MOVABLE)
    displays=[]
    # Center in merchant actor frame, desired longest dimension, direction.
    specs=[((65,-64,43),76,(.18,0,1)),((83,-20,10),30,(0,1,0)),
           ((65,64,40),70,(.18,0,1)),((82,25,24),37,(0,0,1))]
    for i,(offer,(center,size,direction)) in enumerate(zip(offers,specs)):
        item=offer.get_editor_property("item")
        model=item.get_editor_property("pickup_mesh")
        assert model,(item.get_name(),"missing display mesh")
        comp=component(ue.StaticMeshComponent,f"ItemDisplay_{i}")
        comp.set_editor_property("static_mesh",model)
        bounds=model.get_bounding_box()
        low,high=bounds.min,bounds.max
        dims=[high.x-low.x,high.y-low.y,high.z-low.z]
        axis=max(range(3),key=lambda j:dims[j]);scale=size/dims[axis]
        rotation=(ue.MathLibrary.make_rot_from_x,ue.MathLibrary.make_rot_from_y,ue.MathLibrary.make_rot_from_z)[axis](ue.Vector(*direction))
        midpoint=(low+high)*.5
        rotated=ue.MathLibrary.quat_rotate_vector(ue.MathLibrary.conv_rotator_to_quaternion(rotation),midpoint)
        comp.set_editor_property("relative_location",ue.Vector(*center)-rotated*scale)
        comp.set_editor_property("relative_rotation",rotation)
        comp.set_editor_property("relative_scale3d",ue.Vector(scale,scale,scale))
        comp.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        comp.set_mobility(ue.ComponentMobility.MOVABLE)
        displays.append({"item":item.get_path_name(),"mesh":model.get_path_name(),"price":offer.get_editor_property("price")})

    def text(name,title,position,size):
        comp=component(ue.TextRenderComponent,name)
        comp.set_editor_property("relative_location",ue.Vector(*position))
        comp.set_editor_property("relative_rotation",ue.Rotator(roll=0.0, pitch=0.0, yaw=0))
        comp.set_text(ue.Text(title));comp.set_world_size(size)
        comp.set_horizontal_alignment(ue.HorizTextAligment.EHTA_CENTER)
        comp.set_text_render_color(ue.Color(55,38,26,255))
        comp.set_mobility(ue.ComponentMobility.MOVABLE)
    text("MerchantTitle","LE MARCHAND",(113,0,94),12)
    text("MerchantPrompt","F  -  Parler / Acheter",(105,0,-4),6)
    for i,offer in enumerate(offers):
        item=offer.get_editor_property("item")
        text(f"OfferPrice_{i}",f"{offer.get_editor_property('price')} c",(106,-63+i*42,-15),6)
    ue.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo=ue.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("body_mesh",body)
    cdo.set_editor_property("outfit_mesh",None)
    cdo.set_editor_property("idle_animation",anim)
    cdo.set_editor_property("catalog",catalog)
    cdo.set_editor_property("prompt",ue.Text("Parler au marchand - Acheter"))
    cdo.get_editor_property("body").set_skeletal_mesh_asset(body)
    cdo.get_editor_property("outfit").set_skeletal_mesh_asset(None)
    cdo.modify()
    ue.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo=ue.get_default_object(blueprint.generated_class())
    assert cdo.get_editor_property("idle_animation")==anim,str(cdo.get_editor_property("idle_animation"))
    assert cdo.get_editor_property("body_mesh")==body,str(cdo.get_editor_property("body_mesh"))
    save(blueprint)
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/"import-result.json").write_text(json.dumps({"task":"T-181","blueprint":BP_PATH,"displays":displays,
        "body":body.get_path_name(),"animation":anim.get_path_name(),"duration_s":anim.get_play_length(),
        "finished_at":dt.datetime.now(dt.timezone.utc).isoformat(),"validation":"Imported; placement and PIE still required"},indent=2),encoding="utf-8")
    ue.log("T181_IMPORT_SUCCESS")


if __name__=="__main__":
    world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag="Interchange.FeatureFlags.Import.FBX"
    previous=ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world,flag+" 0")
    try:
        main()
    finally:
        ue.SystemLibrary.execute_console_command(world,flag+" "+str(previous))
