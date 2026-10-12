"""Portable FBX bases / animations, static pickups, ammunition and textures of the pistol packs (T-248).

Local adaptation of the second PC's bean-pirate-pistol export_pack.py (read only) and of the musket pack's: every
animation FBX contains a bind mesh (animation-only armature FBXs lose their bind pose on some round trips); the
armature is exported as "Armature" (Unreal drops the node). Metres (FBX UnitScaleFactor 100). The baked textures of
<pack>/textures/ are listed in manifest.materials[<material>].textures by the label of their Image Texture node
(BaseColor, Normal, ORM); the FBX files reference them by relative path (no .fbm copies: the importer reads the
manifest's textures itself).
"""
from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

import meshkit as K


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def select(objs):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        o.hide_set(False)
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def fbx(path, arm, meshes, animated, bean=False):
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
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE', 'MESH'},
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
                             use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, use_armature_deform_only=bean,
                             bake_anim=animated, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_step=1., bake_anim_simplify_factor=0.,
                             path_mode='RELATIVE', embed_textures=False)
    arm.name = name
    for p in arm.pose.bones:
        p.matrix_basis = oldpose[p.name]
    if arm.animation_data:
        arm.animation_data.action = saved
    scene.frame_set(scene.frame_current)
    bpy.context.view_layer.update()


def mesh_export(path, meshes, kind):
    path.parent.mkdir(parents=True, exist_ok=True)
    select(meshes)
    if kind == 'fbx':
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
                                 apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
                                 use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, path_mode='RELATIVE',
                                 embed_textures=False)
    else:
        bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True, use_active_scene=True,
                                  export_animations=False, export_cameras=False, export_lights=False, export_apply=True)


def material_table(root):
    """manifest.materials: every material of a mesh of the file, its textures by node label (copied under
    <root>/textures/ when they live elsewhere)."""
    root = Path(root)
    materials = {}
    for obj in bpy.data.objects:
        if obj.type != 'MESH':
            continue
        for mat in obj.data.materials:
            if not mat or mat.name in materials:
                continue
            p = K.principled(mat) if mat.use_nodes else None
            if not p:
                continue
            textures = {}
            base_color_texture = None
            for n in mat.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image and n.image.filepath:
                    source = Path(bpy.path.abspath(n.image.filepath)).resolve()
                    try:
                        relative = source.relative_to(root.resolve()).as_posix()
                    except ValueError:
                        target = root / 'textures' / source.name
                        target.parent.mkdir(exist_ok=True, parents=True)
                        if source.exists():
                            shutil.copy2(source, target)
                        relative = target.relative_to(root).as_posix()
                        n.image.filepath = '//' + relative
                    textures[n.label or n.name] = relative
                    if any(link.from_node == n for link in p.inputs['Base Color'].links):
                        base_color_texture = relative
            entry = dict(base_color=list(p.inputs['Base Color'].default_value), metallic=p.inputs['Metallic'].default_value,
                         roughness=p.inputs['Roughness'].default_value, textures=textures, base_color_texture=base_color_texture)
            if textures:
                entry['texture_conventions'] = {'BaseColor': 'sRGB', 'Normal': 'tangent space, DirectX (green down)',
                                                'ORM': 'linear: R ambient occlusion, G roughness, B metallic'}
                entry['textures_sha256'] = {label: sha(root / rel) for label, rel in textures.items() if (root / rel).exists()}
            materials[mat.name] = entry
    return materials


def export_pack(root, manifest, variant, source, template, ammo=None):
    """variant = spec of pack.json: weapon, fp_base, tp_base, clip_weapon..."""
    root = Path(root)
    assets = []
    saved_scene = bpy.context.scene
    weapon_name = variant['weapon']

    def asset(path, kind, group, **kw):
        row = dict(path=path.relative_to(root).as_posix(), type=kind, skeleton_group=group, sha256=sha(path), **kw)
        assets.append(row)
        return row

    # bean bases (TP body, FP arms), same original twenty-bone rest hierarchy
    for view, base in (('TP', variant['tp_base']), ('FP', variant['fp_base'])):
        record = next((r for r in manifest['scenes'] if r['view'] == view), None)
        if record is None:
            continue
        scene = bpy.data.scenes[record['scene']]
        bpy.context.window.scene = scene
        scene.frame_set(0)
        actor = record['actors']['bean']
        arm = bpy.data.objects[actor['armature']]
        meshes = [bpy.data.objects[n] for n in actor['meshes']]
        path = root / 'exports' / 'Bases' / (base + '.fbx')
        fbx(path, arm, meshes, False, True)
        asset(path, 'skeletal_mesh', 'Bean', view=view, bones=20, bind_mesh=True)
    record = manifest['scenes'][0]
    scene = bpy.data.scenes[record['scene']]
    bpy.context.window.scene = scene
    actor = record['actors']['weapon']
    arm = bpy.data.objects[actor['armature']]
    meshes = [bpy.data.objects[n] for n in actor['meshes']]
    path = root / 'exports' / 'Bases' / ('SK_Bean_%s.fbx' % weapon_name)
    fbx(path, arm, meshes, False)
    asset(path, 'skeletal_mesh', weapon_name, view='shared', bones=len(arm.data.bones), bind_mesh=True,
          sockets=[b.name for b in arm.data.bones if b.name.startswith('socket_')])
    for record in manifest['scenes']:
        print('[pistol export]', record['view'], record['clip'], flush=True)
        scene = bpy.data.scenes[record['scene']]
        bpy.context.window.scene = scene
        scene.frame_start = record['start']
        scene.frame_end = record['end']
        scene.frame_set(0)
        for actor_name, group in [('bean', 'Bean'), ('weapon', weapon_name)]:
            actor = record['actors'][actor_name]
            arm = bpy.data.objects[actor['armature']]
            meshes = [bpy.data.objects[n] for n in actor['meshes']]
            if actor_name == 'bean' and record['view'] == 'TP':
                meshes = [next((o for o in meshes if 'Bean_Body' in o.name), meshes[0])]
            path = root / 'exports' / 'Animations' / ('%s.fbx' % actor['action'])
            fbx(path, arm, meshes, True, actor_name == 'bean')
            actor['animation_fbx'] = path.relative_to(root).as_posix()
            asset(path, 'animation', group, view=record['view'], clip=record['clip'], fps=record['fps'], start=record['start'],
                  end=record['end'], sample_count=record['end'] - record['start'] + 1, bind_mesh=True, events=record['events'],
                  loop=record['clip'] in manifest['loop_clips'])
    # static pickup from the authoritative template, bottom-centre pivot (hammer at full cock, cartridge hidden)
    scene = bpy.data.scenes.new('SCN-%s-Exports' % weapon_name)
    bpy.context.window.scene = scene
    scene.collection.children.link(template['collection'])
    arm = template['armature']
    for p in arm.pose.bones:
        p.matrix_basis = Matrix.Identity(4)
    arm.pose.bones['charge_visual'].scale = (.0001,) * 3
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    meshes = []
    bounds = template['bounds']
    pivot = Vector(((bounds['min'][0] + bounds['max'][0]) / 2, (bounds['min'][1] + bounds['max'][1]) / 2, bounds['min'][2]))
    for ob in template['weapon_meshes']:
        me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), depsgraph=dg, preserve_all_data_layers=True)
        me.transform(Matrix.Translation(-pivot) @ ob.matrix_world)
        clone = bpy.data.objects.new('GEO-Pickup-' + ob.name, me)
        scene.collection.objects.link(clone)
        meshes.append(clone)
    for kind in ('fbx', 'glb'):
        path = root / 'exports' / 'Static' / ('SM_Bean_%s.%s' % (weapon_name, kind))
        mesh_export(path, meshes, kind)
        asset(path, 'static_mesh', 'none', view='pickup', pivot='bottom_center', source_pivot_offset_m=[*pivot])
    # ammunition (the single pistol pack): pivot bottom centre, UCX box collision in the FBX; the cartridge is also
    # written as SM_Pistol_PaperToken (the v0.1 name the importer expects)
    if ammo:
        items = dict(ammo)
        items['SM_Pistol_PaperToken'] = ammo['SM_Ammo_PistolCartridge']
        for name, (ob, collisions, bb) in items.items():
            ob.location = (0, 0, 0)
            for c in collisions:
                c.location = (0, 0, 0)
            scene.collection.objects.link(ob) if ob.name not in scene.collection.objects else None
            for c in collisions:
                if c.name not in scene.collection.objects:
                    scene.collection.objects.link(c)
            for kind in ('fbx', 'glb'):
                path = root / 'exports' / 'Static' / ('%s.%s' % (name, kind))
                token = name == 'SM_Pistol_PaperToken'
                mesh_export(path, [ob] + (collisions if (kind == 'fbx' and not token) else []), kind)
                asset(path, 'static_mesh', 'none', view='ammo' if not token else 'token', pivot='bottom_center',
                      collision=[c.name for c in collisions] if (kind == 'fbx' and not token) else [],
                      size_m=[round(v, 4) for v in bb['size']], material=ob.data.materials[0].name if ob.data.materials else None,
                      alias_of='SM_Ammo_PistolCartridge' if token else None)
    scene.collection.children.unlink(template['collection'])
    manifest['static_export_scene'] = scene.name
    manifest['materials'] = material_table(root)
    manifest['unreal_import'] = {'bean_source_skeleton_bones': 20, 'bean_helper_grips_exported': False, 'weapon_sockets_exported': True,
                                 'animation_only_import': True, 'import_mesh': False,
                                 'fbx_frame_mapping': 'Blender frames 0..N reimport as 1..N+1',
                                 'actor_attachment': 'Both Bean and weapon actor roots use the same world transform; do not attach the weapon to the hand a second time.',
                                 'textures': 'BaseColor sRGB; Normal tangent DirectX (no green flip in Unreal); ORM linear R=AO G=roughness B=metallic',
                                 'runtime_tested': False}
    bpy.context.window.scene = saved_scene
    return assets
