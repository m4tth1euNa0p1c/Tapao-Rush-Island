"""T-181: reproducible beige, pot-bellied Bean merchant and compact deck stall.

Run in background Blender. Reuses the local Bean rig construction, writes only
the isolated Merchant delivery, and never changes OneDrive or the player model.
"""
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/export/Characters/BeanMerchant"
PREVIEW = ROOT / "art/_previews/merchant"
sys.path.insert(0, str(ROOT / "scripts/character"))
import bean_soldier as bean

PALETTE = {
    "M_Bean_Skin": ("D4B48D", .58, 0),
    "M_Bean_Visor": ("F5E7CD", .48, 0),
    "M_Merchant_Moustache": ("3A241D", .75, 0),
    "M_Merchant_Linen": ("DDC69F", .85, 0),
    "M_Merchant_Seam": ("917457", .8, 0),
    "M_Merchant_Leather": ("50352A", .58, 0),
    "M_Merchant_Brass": ("C9A45B", .3, .8),
    "M_Merchant_Wood": ("755036", .76, 0),
    "M_Merchant_WoodLight": ("A17750", .73, 0),
    "M_Merchant_Iron": ("303437", .44, .72),
    "M_Merchant_Canvas": ("E8D6B0", .95, 0),
    "M_Merchant_Green": ("516854", .9, 0),
}


def tube(name, points, *args, **kwargs):
    return bean.tube(name, [Vector(p) for p in points], *args, **kwargs)


def key_pose(arm, frame, pose):
    for name, rotations in pose.items():
        pb = arm.pose.bones[name]
        pb.rotation_mode = "QUATERNION"
        rotation = Quaternion()
        for axis, angle in rotations:
            rotation = Quaternion(Vector(axis), math.radians(angle)) @ rotation
        basis = arm.data.bones[name].matrix_local.to_quaternion()
        pb.rotation_quaternion = basis.inverted() @ rotation @ basis
        pb.keyframe_insert("rotation_quaternion", frame=frame)


def animate(arm):
    scene = bpy.context.scene
    scene.render.fps = 24
    scene.frame_start, scene.frame_end = 1, 289
    X, Y, Z = (1,0,0), (0,1,0), (0,0,1)
    names = [b.name for b in arm.pose.bones]
    # 12-second cycle: breathe, look around, greet the visitor, settle. Root,
    # pelvis and feet never move, so the merchant stays firmly on the deck.
    for frame in range(1, 290, 4):
        t = (frame-1)/24
        pose = {name: [] for name in names}
        pose["spine_01"] = [(X, .65*math.sin(t*math.tau/4))]
        pose["head"] = [(Z, 5*math.sin(t*math.tau/12)), (X, math.sin(t*math.tau/6))]
        pose["upperarm_l"] = [(Y, -7)]
        pose["upperarm_r"] = [(Y, 7)]
        if 3 <= t <= 6.2:
            lift = min(1, (t-3)/.45, (6.2-t)/.5)
            lift = max(0, lift)
            lift = lift*lift*(3-2*lift)
            wave = math.sin((t-3.4)*math.tau*2)
            pose["upperarm_r"] = [(X,-25*lift),(Y,7+143*lift)]
            pose["lowerarm_r"] = [(X,-(22+12*wave)*lift)]
            pose["hand_r"] = [(Y,18*wave*lift)]
            pose["head"] = [(Z,7*lift),(X,-3*lift)]
        if 8 < t < 10:
            nod = math.sin((t-8)*math.pi)**2
            pose["head"] = [(X,7*nod),(Z,-4*nod)]
            pose["lowerarm_l"] = [(X,-20*nod)]
        if frame in (1,289):
            pose = {name:[] for name in names}
            pose.update(upperarm_l=[(Y,-7)],upperarm_r=[(Y,7)])
        key_pose(arm,frame,pose)
    arm.animation_data.action.name = "A_BeanMerchant_GreetIdle"


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    OUT.mkdir(parents=True,exist_ok=True)
    PREVIEW.mkdir(parents=True,exist_ok=True)
    bean.PALETTE.update(PALETTE)
    bean.P.update(rx=.225, ry=.18, belly=.18, belly_z=.43, belly_h=.19, belly_side=.25)
    scene=bpy.context.scene
    scene.unit_settings.system="METRIC"
    body_coll=bean.collection("BeanMerchant")
    rig_coll=bean.collection("MerchantRig")
    stand_coll=bean.collection("MerchantStand")
    body=bean.build_body(body_coll)
    # Sleeves, trousers and flat-soled shoes: no soldier coat or hat.
    for side,sign in (("l",1),("r",-1)):
        sx,sz=bean.P["shoulder"];ex,ez=bean.P["elbow"];wx,wz=bean.P["wrist"]
        body.append(tube("Sleeve_"+side,[(sign*sx,0,sz),(sign*ex,0,ez),(sign*wx,-.01,wz)],
                              [.067,.057,.052],"M_Merchant_Linen",body_coll))
        body.append(tube("Breeches_"+side,[(sign*.085,0,.25),(sign*.085,0,.1)],
                              [.079,.069],"M_Merchant_Linen",body_coll))
        body.append(bean.blob("Boot_"+side,(sign*.085,-.043,.061),(.077,.121,.069),
                              "M_Merchant_Leather",body_coll,cut_below=0))
        # Soles are rigidly weighted below; lower face exactly at z=0.
        body.append(bean.box("Boot_Sole_"+side,(sign*.085,-.048,.012),(.148,.229,.024),
                             "M_Merchant_Leather",body_coll,bevel=.009))
    # Cream apron follows the round belly; the sides of the beige body remain visible.
    body.append(bean.shell("Merchant_Apron",.24,.615,.008,"M_Merchant_Linen",body_coll,
                           keep=lambda theta,z: abs(theta)<1.0,nu=64,solidify=.008))
    body.append(bean.shell("Merchant_Belt",.295,.345,.018,"M_Merchant_Leather",body_coll,nu=64))
    buckle=bean.surf(0,.325,.029)
    for dx,dz,sx,sz in ((-.037,0,.012,.055),(.037,0,.012,.055),(0,.026,.08,.009),(0,-.026,.08,.009)):
        body.append(bean.box("Merchant_Buckle",buckle+Vector((dx,-.005,dz)),(sx,.014,sz),"M_Merchant_Brass",body_coll))
    # Sweeping two-piece handlebar moustache on the lower face, plus subtle brows.
    for sign in (-1,1):
        points=[(sign*.004, -.210,.732),(sign*.035,-.216,.718),(sign*.068,-.211,.720),
                (sign*.092,-.202,.737),(sign*.108,-.192,.752)]
        body.append(tube("Merchant_Moustache",points,[.014,.022,.019,.012,.002],"M_Merchant_Moustache",body_coll,segs=12))
        body.append(tube("Merchant_Brow",[(sign*.032,-.198,.856),(sign*.052,-.205,.864),(sign*.074,-.194,.861)],
                              [.005,.009,.003],"M_Merchant_Moustache",body_coll,segs=10))
    # Small side pouch and brass fastener, outside the apron.
    body.append(bean.blob("Merchant_Pouch",(.235,.025,.305),(.074,.064,.095),"M_Merchant_Leather",body_coll))
    body.append(bean.blob("Merchant_PouchButton",(.282,-.013,.327),(.012,.01,.012),"M_Merchant_Brass",body_coll))
    bean.bake_transforms(body)
    bean.box_uvs(body)
    arm=bean.build_armature(rig_coll)
    bean.weight_and_bind(body,arm)
    for ob in body:
        rigid = "head" if any(k in ob.name for k in ("Moustache","Brow","Eye","Visor")) else None
        if ob.name.startswith("Boot"):
            rigid = "foot_l" if ob.name.endswith("_l") else "foot_r"
        if rigid:
            ob.vertex_groups.clear()
            ob.vertex_groups.new(name=rigid).add(list(range(len(ob.data.vertices))),1,"REPLACE")
    # Trestle counter, brass corner fittings, awning posts and striped cloth.
    stand=[]
    def box(name,pos,size,mat,bevel=.008):
        ob=bean.box(name,pos,size,mat,stand_coll,bevel=bevel);stand.append(ob);return ob
    for x in (-.73,.73):
        for y in (-.93,-.53):
            box("Stand_Leg",(x,y,.285),(.075,.075,.57),"M_Merchant_Wood")
        box("Stand_Foot",(x,-.73,.038),(.14,.62,.076),"M_Merchant_Wood")
        box("Stand_Brace",(x,-.73,.22),(.075,.45,.08),"M_Merchant_WoodLight")
    for i in range(6):
        box("Counter_Plank",(-.69+i*.276,-.73,.585),(.269,.59,.055),"M_Merchant_WoodLight")
    box("Counter_Fascia",(0,-1.016,.51),(1.68,.042,.13),"M_Merchant_Wood")
    for x in (-.76,.76):
        box("Counter_BrassCorner",(x,-1.041,.52),(.055,.009,.10),"M_Merchant_Brass",.002)
    box("Stand_Shelf",(0,-.73,.18),(1.50,.37,.042),"M_Merchant_Wood")
    for x in (-.86,.86):
        box("Awning_Post",(x,-.46,.83),(.054,.054,1.66),"M_Merchant_Wood")
    box("Awning_Crossbar",(0,-.46,1.63),(1.83,.06,.06),"M_Merchant_WoodLight")
    box("Awning_FrontBar",(0,-1.08,1.52),(1.87,.04,.04),"M_Merchant_WoodLight")
    for i in range(10):
        ob=box("Awning_Stripe",(-.855+i*.19,-.77,1.58),(.192,.69,.015),
               "M_Merchant_Canvas" if i%2==0 else "M_Merchant_Green",.003)
        pivot=Vector((-.855+i*.19,-.77,1.58))
        rotation=Matrix.Rotation(math.radians(10),3,"X")
        for vertex in ob.data.vertices:
            vertex.co=pivot+rotation@(vertex.co-pivot)
        box("Awning_Valance",(-.855+i*.19,-1.113,1.48),(.192,.016,.12),
            "M_Merchant_Canvas" if i%2==0 else "M_Merchant_Green",.004)
    # A low rack supports the displayed long guns without blocking the greeting.
    for x in (-.67,.67):
        box("GunRack_Support",(x,-.60,.76),(.04,.04,.35),"M_Merchant_Wood")
        box("GunRack_Fork",(x,-.60,.92),(.20,.045,.035),"M_Merchant_WoodLight")
    bean.bake_transforms(stand);bean.box_uvs(stand,size=.32)
    bean.export_fbx(body,arm,OUT/"SK_BeanMerchant.fbx")
    bean.export_fbx(stand,arm,OUT/"SM_MerchantStand.fbx",with_armature=False)
    animate(arm)
    bpy.ops.object.select_all(action="DESELECT");arm.select_set(True)
    previous=arm.name;arm.name="Armature"
    bpy.ops.export_scene.fbx(filepath=str(OUT/"A_BeanMerchant_GreetIdle.fbx"),use_selection=True,object_types={"ARMATURE"},
        apply_unit_scale=True,apply_scale_options="FBX_SCALE_NONE",axis_forward="-Y",axis_up="Z",add_leaf_bones=False,
        primary_bone_axis="Y",secondary_bone_axis="X",use_armature_deform_only=True,bake_anim=True,
        bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
    arm.name=previous
    scene.frame_set(1)
    # Export the exact palette; Unreal material import is deterministic.
    mats={m.name:{"color":bean.PALETTE[m.name][0],"roughness":bean.PALETTE[m.name][1],"metallic":bean.PALETTE[m.name][2]}
          for m in bpy.data.materials if m.name in bean.PALETTE}
    report={"task":"T-181","height_m":1.05,"belly_m":.18,"materials":mats,
            "animation":{"name":"A_BeanMerchant_GreetIdle","seconds":12,"fps":24,"root_motion":False,"feet_fixed":True},
            "body_objects":len(body),"stand_objects":len(stand),"stand_collision":"Three simple boxes added in Blueprint: counter and two posts"}
    (OUT/"merchant-report.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    floor_coll=bean.collection("PreviewOnly")
    bean.box("Preview_Ground",(0,0,-.025),(200,200,.05),"M_Merchant_Canvas",floor_coll,bevel=0)
    cam=bean.setup_render(scene)
    scene.render.resolution_x=1200;scene.render.resolution_y=1000
    cam.data.ortho_scale=2.5
    for tag,position,frame in (("front",(0,-4,.95),1),("quarter",(3,-5,2.6),1),("greeting",(1.7,-4,1.8),108),
                                ("character",(2,-4,1.5),1)):
        stand_coll.hide_render=tag=="character"
        cam.data.ortho_scale=1.4 if tag=="character" else 2.5
        scene.frame_set(frame)
        cam.location=position;cam.rotation_euler=(Vector((0,0,.54) if tag=="character" else (0,-.35,.8))-cam.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(PREVIEW/(tag+".png"));bpy.ops.render.render(write_still=True)
    stand_coll.hide_render=False
    scene.frame_set(1)
    dest=ROOT/"art/blender/characters/bean_merchant.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(dest),compress=True)
    print("T181_MODEL_SUCCESS",json.dumps(report))


if __name__=="__main__":
    main()
