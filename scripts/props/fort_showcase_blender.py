"""Read the delivered scene, preserving OneDrive, and extract integration data."""
import datetime as dt
import json
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
SOURCE=Path('C:/Users/matth/OneDrive/maquette-blender/maquettes/fort-francais')
STAGE=ROOT/'art/export/Fort_T193'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'fort-francais.blend'),load_ui=False)
rows=[]
materials={}
for o in bpy.context.scene.objects:
    if o.type=='EMPTY' and o.instance_collection:
        rows.append({'name':o.name,'collection':o.instance_collection.name,
                     'matrix':[list(r) for r in o.matrix_world],
                     'collections':[c.name for c in o.users_collection]})
    elif o.type in ('MESH','LIGHT'):
        row={'name':o.name,'type':o.type,'matrix':[list(r) for r in o.matrix_world],
             'collections':[c.name for c in o.users_collection]}
        if o.type=='MESH':
            row.update(vertices=len(o.data.vertices),materials=[m.name for m in o.data.materials],
                       color_attributes=[a.name for a in o.data.color_attributes])
        else:row.update(energy=o.data.energy,color=list(o.data.color),light_type=o.data.type)
        rows.append(row)
for m in bpy.data.materials:
    imgs=[]
    if m.node_tree:
        for n in m.node_tree.nodes:
            if n.type=='TEX_IMAGE' and n.image:
                imgs.append({'name':n.image.name,'path':bpy.path.abspath(n.image.filepath,library=n.image.library),
                             'packed':bool(n.image.packed_file),'colorspace':n.image.colorspace_settings.name})
    materials[m.name]={'color':list(m.diffuse_color),'ue_recipe':m.get('ue_recipe'),'images':imgs}
result={'objects':rows,'materials':materials,'libraries':[l.filepath for l in bpy.data.libraries],
        'finished_at':dt.datetime.now(dt.timezone.utc).isoformat()}
(STAGE/'scene-inspection.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('T193_SCENE_INSPECTION',len(rows),len(materials),flush=True)
