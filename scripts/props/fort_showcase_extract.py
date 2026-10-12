"""Extract local integration data from the immutable authored Blender scene."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import bpy
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path('C:/Users/matth/OneDrive/maquette-blender')
STAGE = ROOT/'art/export/Fort_T193'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'maquettes/fort-francais/fort-francais.blend'), load_ui=False)
report = json.loads((STAGE/'fort-report.json').read_text(encoding='utf-8'))
modules = {name:info for lot in report['modules'].values() for name,info in lot['modules'].items() if name.startswith('SM_')}
used = {m for info in modules.values() for m in info['materials']} | {'MAT-Site_Ground','MAT-Site_Lagoon'}
manifest = json.loads((STAGE/'source-manifest.json').read_text(encoding='utf-8'))
materials = {}
for mat in bpy.data.materials:
    if mat.name not in used:
        continue
    images = {}
    for node in mat.node_tree.nodes if mat.node_tree else []:
        if node.type != 'TEX_IMAGE' or not node.image:
            continue
        img = node.image
        source = Path(bpy.path.abspath(img.filepath, library=img.library)).resolve()
        if source.is_file():
            relative = Path('textures')/source.relative_to(SOURCE)
            dest = STAGE/relative
            if relative.as_posix() not in manifest['files']:
                before=source.stat(); data=source.read_bytes(); after=source.stat()
                assert (before.st_size,before.st_mtime_ns)==(after.st_size,after.st_mtime_ns), source
                dest.parent.mkdir(parents=True,exist_ok=True); dest.write_bytes(data)
                manifest['files'][relative.as_posix()]={'source':str(source),'bytes':len(data),'mtime_ns':after.st_mtime_ns,'sha256':hashlib.sha256(data).hexdigest()}
        elif img.packed_file:
            relative=Path('textures/derived')/(img.name.split('.')[0]+'.png')
            dest=STAGE/relative; dest.parent.mkdir(parents=True,exist_ok=True)
            img.save_render(str(dest))
        else:
            raise FileNotFoundError(source)
        images[source.name or img.name]=relative.as_posix()
    materials[mat.name]={'images':images,'color':list(mat.diffuse_color),'ue_recipe':mat.get('ue_recipe')}

mirror=Matrix.Diagonal((1,-1,1,1))
layout=[]
for obj in bpy.context.scene.objects:
    if obj.type!='EMPTY' or not obj.instance_collection:
        continue
    name=obj.instance_collection.name
    if name not in modules and not name.startswith('LIB-SM_'):
        continue
    matrix=mirror@obj.matrix_world@mirror
    loc,rot,scale=matrix.decompose()
    layout.append({'name':obj.name,'mesh':name.removeprefix('LIB-'),'kind':'module' if name in modules else 'vegetation',
                   'location_cm':[v*100 for v in loc],'quaternion_xyzw':[rot.x,rot.y,rot.z,rot.w],
                   'scale':list(scale),'folder':obj.users_collection[0].name})

# Static terrain preview for Test Flat, including its authored hole over the vault.
# Store MaskB in UV1/2 because FBX/Unreal preserves only one vertex-color layer.
derived=[]
coll=bpy.data.collections.get('REF-site_terrain')
assert coll
for src in list(coll.all_objects):
    source_name=src.name.split('.')[0]
    if src.type!='MESH' or source_name not in ('REF-Site_Terrain_Near','REF-Site_Terrain_Far','REF-Court_Paths','REF-Lagoon'):
        continue
    obj=src.copy(); obj.data=src.data.copy(); bpy.context.scene.collection.objects.link(obj)
    obj.name={'REF-Site_Terrain_Near':'SM_Fort_TerrainNear','REF-Site_Terrain_Far':'SM_Fort_TerrainFar',
              'REF-Court_Paths':'SM_Fort_CourtPaths','REF-Lagoon':'SM_Fort_Lagoon'}[source_name]
    me=obj.data
    if 'MaskA' in me.color_attributes:
        maskb=me.color_attributes['MaskB']
        uv1=me.uv_layers.new(name='MaskB_RG'); uv2=me.uv_layers.new(name='MaskB_BA')
        for loop in me.loops:
            color=maskb.data[loop.vertex_index].color
            uv1.data[loop.index].uv=color[:2]; uv2.data[loop.index].uv=color[2:]
        me.color_attributes.remove(maskb)
        me.color_attributes.active_color_index=0
        me.color_attributes.render_color_index=0
    elif 'Depth' in me.color_attributes:
        # Normalize depth for COLOR_0, foam in G, terrain shore remains in the geometry.
        depth=me.color_attributes['Depth']; foam=me.color_attributes['Foam']
        for i in range(len(depth.data)):
            depth.data[i].color=(min(depth.data[i].color[0]/18,1),foam.data[i].color[0],0,1)
        me.color_attributes.remove(foam)
        me.color_attributes.active_color_index=0; me.color_attributes.render_color_index=0
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    dest=STAGE/'derived'/(obj.name+'.fbx'); dest.parent.mkdir(exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(dest),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
                             axis_forward='-Y',axis_up='Z',bake_anim=False,use_custom_props=False,mesh_smooth_type='FACE',colors_type='LINEAR')
    derived.append({'name':obj.name,'source':dest.relative_to(STAGE).as_posix(),'materials':[m.name for m in me.materials],
                    'color_attributes':[a.name for a in me.color_attributes],'uv_layers':[u.name for u in me.uv_layers]})
    bpy.data.objects.remove(obj,do_unlink=True)

for filename, data in [('source-manifest.json',manifest),('material-sources.json',materials),
                        ('layout.json',layout),('derived.json',derived)]:
    (STAGE/filename).write_text(json.dumps(data,indent=2),encoding='utf-8')
print('T193_EXTRACT_SUCCESS',len(modules),len(layout),len(materials),len(manifest['files']),derived,flush=True)
print('T193_SITE_OBJECTS',[(o.name,o.type) for o in coll.all_objects],flush=True)
