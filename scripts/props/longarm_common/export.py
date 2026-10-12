"""FBX bases / animations, static pickups and ammunition of a long arm pack (T-250), after the musket's export_pack.py.

Every animation FBX carries a bind mesh (an armature-only FBX loses its bind pose on some Blender / Unreal round
trips); Unreal imports them as animations. The armature is exported as "Armature" (dropped by Unreal). Textures:
the baked PNG stay in <pack>/textures/ (listed by label in manifest.materials[].textures); the base skeletal FBX and
the static FBX are exported with path_mode COPY (a copy next to the FBX, where the Unreal importer finds it), the
animation FBX with STRIP (no texture copies, they import without materials).
"""
from __future__ import annotations

import shutil
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

from musket_ref import MX


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
    MX.select([arm] + meshes)
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


def static_copy(scene, ob, name, pivot, dg):
    """A copy of the evaluated mesh at a bottom-centre pivot, linked to scene."""
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), depsgraph=dg, preserve_all_data_layers=True)
    me.transform(Matrix.Translation(-Vector(pivot)) @ ob.matrix_world)
    clone = bpy.data.objects.new(name, me)
    scene.collection.objects.link(clone)
    return clone


def export_pack(root, manifest, source, template, profile, ammo):
    root = Path(root)
    assets = []
    saved_scene = bpy.context.scene
    tag, group = profile.name, profile.skeleton_group

    def asset(path, kind, grp, **kw):
        row = dict(path=path.relative_to(root).as_posix(), type=kind, skeleton_group=grp, sha256=MX.sha(path), **kw)
        assets.append(row)
        return row

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
        path = root / 'exports' / 'Bases' / f'SK_Bean_{tag}_{view}.fbx'
        fbx(path, arm, meshes, False, True, 'COPY')
        asset(path, 'skeletal_mesh', 'Bean', view=view, bones=20, bind_mesh=True)
    record = manifest['scenes'][0]
    scene = bpy.data.scenes[record['scene']]
    bpy.context.window.scene = scene
    actor = record['actors']['weapon']
    arm = bpy.data.objects[actor['armature']]
    meshes = [bpy.data.objects[n] for n in actor['meshes']]
    path = root / 'exports' / 'Bases' / f'SK_Bean_Pirate{tag}.fbx'
    fbx(path, arm, meshes, False, False, 'COPY')
    asset(path, 'skeletal_mesh', group, view='shared', bones=len(arm.data.bones), bind_mesh=True,
          textures={k: v.relative_to(root).as_posix() for k, v in template.get('textures', {}).items()})
    for record in manifest['scenes']:
        print('[longarm export]', record['view'], record['clip'], flush=True)
        scene = bpy.data.scenes[record['scene']]
        bpy.context.window.scene = scene
        scene.frame_start = record['start']
        scene.frame_end = record['end']
        scene.frame_set(0)
        for actor_name, grp in (('bean', 'Bean'), ('weapon', group)):
            actor = record['actors'][actor_name]
            arm = bpy.data.objects[actor['armature']]
            meshes = [bpy.data.objects[n] for n in actor['meshes']]
            if actor_name == 'bean' and record['view'] == 'TP':
                meshes = [next((o for o in meshes if 'Bean_Body' in o.name), meshes[0])]
            path = root / 'exports' / 'Animations' / f"{actor['action']}.fbx"
            fbx(path, arm, meshes, True, actor_name == 'bean', 'STRIP')
            actor['animation_fbx'] = path.relative_to(root).as_posix()
            asset(path, 'animation', grp, view=record['view'], clip=record['clip'], fps=record['fps'], start=record['start'],
                  end=record['end'], sample_count=record['end'] - record['start'] + 1, bind_mesh=True, events=record['events'])
    # statics: the pickup at a bottom-centre pivot, the ammunition as built (pivot at its bottom centre already)
    scene = bpy.data.scenes.new(f'SCN-{tag}-Exports')
    bpy.context.window.scene = scene
    scene.collection.children.link(template['collection'])
    arm = template['armature']
    for p in arm.pose.bones:
        p.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    bounds = template['bounds']
    pivot = Vector(((bounds['min'][0] + bounds['max'][0]) / 2, (bounds['min'][1] + bounds['max'][1]) / 2, bounds['min'][2]))
    pickup = [static_copy(scene, ob, 'GEO-Pickup-' + ob.name, pivot, dg) for ob in template['weapon_meshes']]
    for kind in ('fbx', 'glb'):
        path = root / 'exports' / 'Static' / f'SM_Bean_Pirate{tag}.{kind}'
        MX.mesh_export(path, pickup, kind)
        asset(path, 'static_mesh', 'none', view='pickup', pivot='bottom_center', source_pivot_offset_m=[*pivot])
    for item in ammo:
        ob = item['object']
        clone = static_copy(scene, ob, 'GEO-Export-' + ob.name, (0, 0, 0), dg)
        for kind in ('fbx', 'glb'):
            path = root / 'exports' / 'Static' / f"{item['name']}.{kind}"
            MX.mesh_export(path, [clone], kind)
            asset(path, 'static_mesh', 'none', view='ammo', pivot='bottom_center', size_m=item.get('size_m'),
                  textures={k: v.relative_to(root).as_posix() for k, v in item.get('textures', {}).items()})
    scene.collection.children.unlink(template['collection'])
    manifest['static_export_scene'] = scene.name
    materials = {}
    for obj in bpy.data.objects:
        if obj.type != 'MESH':
            continue
        for mat in obj.data.materials:
            if not mat or mat.name in materials:
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
            base = p.inputs['Base Color'].default_value
            materials[mat.name] = dict(base_color=list(base), metallic=p.inputs['Metallic'].default_value,
                                       roughness=p.inputs['Roughness'].default_value, textures=textures,
                                       base_color_texture=base_color_texture)
    manifest['materials'] = materials
    manifest['unreal_import'] = {
        'bean_source_skeleton_bones': 20, 'bean_helper_grips_exported': False, 'weapon_sockets_exported': True,
        'animation_only_import': True, 'import_mesh': False, 'fbx_frame_mapping': 'Blender frames 0..N reimport as 1..N+1',
        'actor_attachment': 'Both Bean and weapon actor roots use the same world transform; do not attach weapon to hand a second time.',
        'weapon_class': 'APirateMusketWeapon (UPirateMusketRules DataAsset of its own: DA_Pirate%sRules)' % tag,
        'textures': 'manifest.materials[M_Bean_Pirate%s].textures by label: BaseColor (sRGB), Normal (DirectX, green down), '
                    'ORM (AO / roughness / metallic, linear); wire ORM by hand in the Unreal material' % tag,
        'runtime_tested': False}
    bpy.context.window.scene = saved_scene
    return assets
