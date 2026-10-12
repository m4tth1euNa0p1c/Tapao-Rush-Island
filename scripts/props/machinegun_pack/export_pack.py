"""FBX bases / animations and the static meshes of the machine gun pack (T-272), after the long arm and shotgun packs'
exporters (scripts/props/longarm_common/export.py, scripts/props/shotgun_pack/export_pack.py).

Every animation FBX carries a bind mesh (an armature-only FBX loses its bind pose on some Blender / Unreal round trips);
the armature is exported as "Armature" (Unreal drops that node). Metres with UnitScaleFactor 100 (apply_unit_scale,
FBX_SCALE_UNITS): Unreal converts to centimetres with an import scale of 1 (convert_scene_unit), as for every weapon
pack of scripts/character/import_pirate_weapons.py. Textures: path_mode COPY for the bases and the statics (a .fbm
next to the FBX), STRIP for the animations (imported without materials). Static meshes carry UCX_<Mesh>_NN hulls.
"""
from __future__ import annotations

import hashlib
import shutil
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def select(objs):
    """Selects exactly objs in the window's view layer (the one the export operators use)."""
    for sc in bpy.data.scenes:
        for vl in sc.view_layers:
            for o in list(vl.objects):
                if o is not None:
                    o.select_set(False, view_layer=vl)
    vl = bpy.context.window.view_layer
    for o in objs:
        o.hide_viewport = False
        o.hide_set(False, view_layer=vl)
        o.select_set(True, view_layer=vl)
    vl.objects.active = objs[0]


def fbx(path, arm, meshes, animated, bean=False, path_mode='STRIP'):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    scene = bpy.context.scene
    saved = arm.animation_data.action if arm.animation_data else None
    name = arm.name
    oldpose = {p.name: p.matrix_basis.copy() for p in arm.pose.bones}
    if not animated:
        if arm.animation_data:
            arm.animation_data.action = None
        for p in arm.pose.bones:
            p.matrix_basis = Matrix.Identity(4)
    other = bpy.data.objects.get('Armature')
    if other is not None and other != arm:
        other.name = 'Armature-renamed-for-export'
    arm.name = 'Armature'
    assert arm.name == 'Armature', arm.name
    select([arm] + meshes)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE', 'MESH'}, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True,
                             mesh_smooth_type='FACE', add_leaf_bones=False, use_armature_deform_only=bean, bake_anim=animated,
                             bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                             bake_anim_force_startend_keying=True, bake_anim_step=1., bake_anim_simplify_factor=0., path_mode=path_mode,
                             embed_textures=False)
    arm.name = name
    for p in arm.pose.bones:
        p.matrix_basis = oldpose[p.name]
    if arm.animation_data:
        arm.animation_data.action = saved
    scene.frame_set(scene.frame_current)
    bpy.context.view_layer.update()


def mesh_export(path, meshes, kind):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    select(meshes)
    if kind == 'fbx':
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                                 apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z', use_mesh_modifiers=True,
                                 mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, path_mode='COPY', embed_textures=False)
    else:
        bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True, use_active_scene=True,
                                  export_animations=False, export_cameras=False, export_lights=False, export_apply=True)


def mesh_without_groups(src, drop_groups, transform):
    """A copy of src's mesh without the vertices weighted on drop_groups, transformed."""
    me = src.data.copy()
    ids = {src.vertex_groups[n].index for n in drop_groups if n in src.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(me)
    layer = bm.verts.layers.deform.active
    if layer is not None and ids:
        doomed = [v for v in bm.verts if any(g in ids and w > 0.5 for g, w in v[layer].items())]
        bmesh.ops.delete(bm, geom=doomed, context='VERTS')
    bm.transform(transform)
    bm.to_mesh(me)
    bm.free()
    return me


def mesh_of_groups(src, keep_groups, transform):
    """A copy of src's mesh with only the vertices weighted on keep_groups, transformed."""
    ids = {src.vertex_groups[n].index for n in keep_groups if n in src.vertex_groups}
    me = src.data.copy()
    bm = bmesh.new()
    bm.from_mesh(me)
    layer = bm.verts.layers.deform.active
    doomed = [v for v in bm.verts if not any(g in ids and w > 0.5 for g, w in v[layer].items())]
    bmesh.ops.delete(bm, geom=doomed, context='VERTS')
    bm.transform(transform)
    bm.to_mesh(me)
    bm.free()
    return me


def bounds(me):
    xs = [v.co for v in me.vertices]
    lo = Vector([min(v[i] for v in xs) for i in range(3)])
    hi = Vector([max(v[i] for v in xs) for i in range(3)])
    return lo, hi


def ucx_box(scene, name, lo, hi):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector(((lo.x + hi.x) / 2 + v.co.x * (hi.x - lo.x), (lo.y + hi.y) / 2 + v.co.y * (hi.y - lo.y),
                       (lo.z + hi.z) / 2 + v.co.z * (hi.z - lo.z)))
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    ob.display_type = 'WIRE'
    return ob


def clean_mesh_object(scene, name, me):
    """A clean static object: the weapon's vertex groups and its armature modifier gone."""
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    return ob


def export_pack(root, manifest, template, statics, model_only=False):
    """Writes every FBX / GLB of the pack and returns the manifest's asset rows."""
    root = Path(root)
    assets = []
    saved_scene = bpy.context.scene

    def asset(path, kind, group, **kw):
        row = dict(path=path.relative_to(root).as_posix(), type=kind, skeleton_group=group, sha256=sha(path), **kw)
        assets.append(row)
        return row

    if not model_only:
        for view in ('TP', 'FP'):
            record = next((r for r in manifest['scenes'] if r['view'] == view), None)
            if record is None:
                continue
            scene = bpy.data.scenes[record['scene']]
            bpy.context.window.scene = scene
            scene.frame_set(0)
            actor = record['actors']['bean']
            arm = bpy.data.objects[actor['armature']]
            meshes = [bpy.data.objects[n] for n in actor['meshes']]
            path = root / 'exports' / 'Bases' / f'SK_Bean_MachineGun_{view}.fbx'
            fbx(path, arm, meshes, False, True, 'COPY')
            asset(path, 'skeletal_mesh', 'Bean', view=view, bones=20, bind_mesh=True)
    # the weapon base from the template (bind pose = rest: the belt on its fed path, the spare pockets inside)
    scene = bpy.data.scenes.new('SCN-MachineGun-Exports')
    bpy.context.window.scene = scene
    scene.collection.children.link(template['collection'])
    arm = template['armature']
    path = root / 'exports' / 'Bases' / 'SK_Bean_MachineGun.fbx'
    fbx(path, arm, template['meshes'], False, False, 'COPY')
    asset(path, 'skeletal_mesh', 'MachineGun', view='shared', bones=len(arm.data.bones), bind_mesh=True,
          materials=[m.name for m in template['meshes'][0].data.materials],
          textures={k: str(v.relative_to(root).as_posix()) for k, v in template.get('texture_files', {}).items()})
    if not model_only:
        for record in manifest['scenes']:
            print('[mg export]', record['view'], record['clip'], flush=True)
            sc = bpy.data.scenes[record['scene']]
            bpy.context.window.scene = sc
            sc.frame_start = record['start']
            sc.frame_end = record['end']
            sc.frame_set(0)
            for actor_name, group in (('bean', 'Bean'), ('weapon', 'MachineGun')):
                actor = record['actors'][actor_name]
                a = bpy.data.objects[actor['armature']]
                meshes = [bpy.data.objects[n] for n in actor['meshes']]
                if actor_name == 'bean' and record['view'] == 'TP':
                    meshes = [next((o for o in meshes if 'Bean_Body' in o.name), meshes[0])]
                p = root / 'exports' / 'Animations' / f"{actor['action']}.fbx"
                fbx(p, a, meshes, True, actor_name == 'bean', 'STRIP')
                actor['animation_fbx'] = p.relative_to(root).as_posix()
                asset(p, 'animation', group, view=record['view'], clip=record['clip'], fps=record['fps'], start=record['start'],
                      end=record['end'], sample_count=record['end'] - record['start'] + 1, bind_mesh=True, events=record['events'],
                      loop=record.get('loop', False))
        bpy.context.window.scene = scene
    # statics (built by build.build_statics): pickup, ammunition box, casing, belt link
    for name, info in statics.items():
        ob, hulls = info['object'], info['hulls']
        if ob.name not in scene.collection.objects:
            for c in list(ob.users_collection):
                c.objects.unlink(ob)
            scene.collection.objects.link(ob)
        for h in hulls:
            if h.name not in scene.collection.objects:
                for c in list(h.users_collection):
                    c.objects.unlink(h)
                scene.collection.objects.link(h)
        for kind in ('fbx', 'glb'):
            p = root / 'exports' / 'Static' / f'{name}.{kind}'
            mesh_export(p, [ob] + (hulls if kind == 'fbx' else []), kind)
            asset(p, 'static_mesh', 'none', view=info['view'], pivot=info.get('pivot', 'bottom_center'),
                  collision=f'UCX x{len(hulls)}' if kind == 'fbx' and hulls else 'none', size_m=info.get('size_m'),
                  triangles=info.get('triangles'), role=info.get('role'), material=[m.name for m in ob.data.materials])
    scene.collection.children.unlink(template['collection'])
    manifest['static_export_scene'] = scene.name
    materials = {}
    for obj in bpy.data.objects:
        if obj.type != 'MESH':
            continue
        for mat in obj.data.materials:
            if not mat or mat.name in materials or mat.get('mg_source'):
                continue
            p = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if mat.use_nodes else None
            if not p:
                continue
            textures = {}
            base_color_texture = None
            for n in mat.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image and n.image.filepath:
                    src = Path(bpy.path.abspath(n.image.filepath)).resolve()
                    try:
                        relative = src.relative_to(root.resolve()).as_posix()
                    except ValueError:
                        target = root / 'textures' / src.name
                        target.parent.mkdir(exist_ok=True, parents=True)
                        if src.exists():
                            shutil.copy2(src, target)
                        relative = target.relative_to(root).as_posix()
                        n.image.filepath = str(target)
                    textures[n.label or n.name] = dict(path=relative, colorspace=n.image.colorspace_settings.name,
                                                       size=[n.image.size[0], n.image.size[1]])
                    if any(link.from_node == n for link in p.inputs['Base Color'].links):
                        base_color_texture = relative
            materials[mat.name] = dict(base_color=list(p.inputs['Base Color'].default_value), metallic=p.inputs['Metallic'].default_value,
                                       roughness=p.inputs['Roughness'].default_value, textures=textures,
                                       base_color_texture=base_color_texture)
    manifest['materials'] = materials
    manifest['textures'] = sorted(p.relative_to(root).as_posix() for p in (root / 'textures').glob('*.png')) if (root / 'textures').is_dir() else []
    manifest['unreal_import'] = {
        'bean_source_skeleton_bones': 20, 'bean_helper_grips_exported': False, 'weapon_sockets_exported': True,
        'animation_only_import': True, 'import_mesh': False, 'fbx_frame_mapping': 'Blender frames 0..N reimport as 1..N+1',
        'actor_attachment': 'Both Bean and weapon actor roots use the same world transform; do not attach the weapon to the hand a second time.',
        'weapon_class': 'AMachineGunWeapon (T-273) with UMachineGunRules (DA_MachineGunRules)',
        'textures': 'manifest.materials[M_MG_*].textures by label: BaseColor (sRGB), Normal (DirectX, green down), ORM (AO / roughness / '
                    'metallic, linear); "Heat" on M_MG_Jacket: glow mask of the hot barrel (Emissive of MI_MachineGun_Jacket, '
                    'EmissiveStrength 0 by default, set by scripts/props/machinegun_pack/import_machinegun_ue.py)',
        'runtime_tested': False}
    manifest['coordinate_system'] = {'blender': '+X weapon forward, +Z up, -Y right side (crank); bean looks along -Y',
                                     'fbx': 'axis_forward -Y, axis_up Z, metres (UnitScaleFactor 100)',
                                     'unreal': 'convert_scene + convert_scene_unit, import scale 1; a weapon point (x, y, z) m -> (x, -y, z) * 100 cm'}
    bpy.context.window.scene = saved_scene
    return assets
