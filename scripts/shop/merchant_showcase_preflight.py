"""Verify T-181 grounded feet, actual greeting motion, FBX scale/materials."""
import datetime as dt
import json
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"art/export/Characters/BeanMerchant"


def main():
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/"art/blender/characters/bean_merchant.blend"))
    arm=next(o for o in bpy.data.objects if o.type=="ARMATURE")
    feet=[];hands=[]
    for frame in range(1,290,4):
        bpy.context.scene.frame_set(frame)
        deps=bpy.context.evaluated_depsgraph_get()
        soles=[]
        for ob in bpy.data.objects:
            if ob.name.startswith("Boot_Sole"):
                evaluated=ob.evaluated_get(deps)
                soles.extend((evaluated.matrix_world@v.co).z for v in evaluated.data.vertices)
        feet.append(min(soles))
        hands.append((arm.matrix_world@arm.pose.bones['hand_r'].matrix).translation.z)
    assert max(abs(z) for z in feet)<.002,feet
    assert max(hands)-min(hands)>.25,"No visible greeting motion"
    result={"task":"T-181","foot_error_max_cm":max(abs(z) for z in feet)*100,
            "hand_vertical_motion_cm":(max(hands)-min(hands))*100,"meshes":{}}
    report=json.loads((OUT/"merchant-report.json").read_text(encoding="utf-8"))
    for name in ("SK_BeanMerchant","SM_MerchantStand"):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(OUT/(name+".fbx")))
        meshes=[o for o in bpy.data.objects if o.type=="MESH"]
        assert meshes
        points=[o.matrix_world@Vector(c) for o in meshes for c in o.bound_box]
        bounds=[[min(p[i] for p in points),max(p[i] for p in points)] for i in range(3)]
        slots={m.name for o in meshes for m in o.data.materials}
        assert slots.issubset(report['materials']),slots
        assert abs(bounds[2][0])<.01,bounds
        assert .95<bounds[2][1]<1.8,bounds
        if name.startswith('SK'):
            arms=[o for o in bpy.data.objects if o.type=='ARMATURE']
            assert len(arms)==1
            assert all(len(v.groups)>0 for o in meshes for v in o.data.vertices),'Unweighted vertices'
        result['meshes'][name]={"object_count":len(meshes),"bounds_m":bounds,"materials":sorted(slots),
                                "triangles":sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in meshes)}
    result.update(passed=True,finished_at=dt.datetime.now(dt.timezone.utc).isoformat())
    (OUT/"preflight-result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    print("T181_PREFLIGHT_SUCCESS",json.dumps(result))


if __name__=='__main__':
    main()
