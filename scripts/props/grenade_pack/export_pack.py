"""Portable FBX bases / animations and the static pickup (with its UCX collision) of the grenade pack (T-253); same
layout as the pistol pack of the second PC and the local musket pack: exports/{Bases,Animations,Static}, metres
(UnitScaleFactor 100), every animation with a bind mesh, the armature exported as "Armature".

The weapon material is the baked atlas (BaseColor / Normal / ORM): for the export the DirectX normal image is linked
straight into the Normal Map node (what Unreal expects), the Blender-only green flip is put back afterwards.
"""
from __future__ import annotations

import hashlib
import shutil
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

import common as C
from grenade_model import BALL, R_BALL, pickup_points


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def select(objs):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        o.hide_set(False)
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def all_materials(meshes):
    out = []
    for ob in meshes:
        for m in ob.data.materials:
            if m and m not in out:
                out.append(m)
    return out


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
    restore = C.fbx_simplify_normals(all_materials(meshes))
    select([arm] + meshes)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE', 'MESH'},
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
                             use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, use_armature_deform_only=bean,
                             bake_anim=animated, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_step=1., bake_anim_simplify_factor=0.,
                             # the bases carry their textures (.fbm); the animations only name them (imported without materials)
                             path_mode='STRIP' if animated else 'COPY', embed_textures=False)
    restore()
    arm.name = name
    for p in arm.pose.bones:
        p.matrix_basis = oldpose[p.name]
    if arm.animation_data:
        arm.animation_data.action = saved
    scene.frame_set(scene.frame_current)
    bpy.context.view_layer.update()


def mesh_export(path, meshes, kind, collisions=()):
    path.parent.mkdir(parents=True, exist_ok=True)
    restore = C.fbx_simplify_normals(all_materials(meshes))
    if kind == 'fbx':
        select(list(meshes) + list(collisions))
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
                                 apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
                                 use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, path_mode='COPY',
                                 embed_textures=False)
    else:
        select(list(meshes))
        bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True, use_active_scene=True,
                                  export_animations=False, export_cameras=False, export_lights=False, export_apply=True)
    restore()


def pickup_objects(template, scene):
    """The grenade alone (ball, plug, fuse: no match, no bag) at a bottom-centre pivot, with its UCX hull."""
    src = template['object']
    pivot = Vector((BALL.x, BALL.y, BALL.z - R_BALL))
    me = src.data.copy()
    me.name = 'SM_Bean_PirateGrenade'
    ob = bpy.data.objects.new('SM_Bean_PirateGrenade', me)
    scene.collection.objects.link(ob)
    drop = [g.index for g in src.vertex_groups if g.name in ('match', 'bag')]
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    deform = bm.verts.layers.deform.active
    doomed = []
    if deform is not None:
        for v in bm.verts:
            if any(v[deform].get(i, 0.0) > 0.5 for i in drop):
                doomed.append(v)
    bmesh.ops.delete(bm, geom=doomed, context='VERTS')
    bm.to_mesh(me)
    bm.free()
    me.transform(Matrix.Translation(-pivot))
    for g in list(ob.vertex_groups):
        ob.vertex_groups.remove(g)
    ucx = C.convex('UCX_SM_Bean_PirateGrenade_00', [p - pivot for p in pickup_points()], scene.collection)
    return ob, ucx, pivot


def export_pack(root, manifest, source, template):
    root = Path(root)
    assets = []
    saved_scene = bpy.context.scene

    def asset(path, kind, group, **kw):
        row = dict(path=path.relative_to(root).as_posix(), type=kind, skeleton_group=group, sha256=sha(path), **kw)
        assets.append(row)
        return row

    # bean bases: TP body, FP arms (same twenty-bone rest hierarchy as the project's bean)
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
        path = root / 'exports' / 'Bases' / ('SK_Bean_Grenade_' + view + '.fbx')
        fbx(path, arm, meshes, False, True)
        asset(path, 'skeletal_mesh', 'Bean', view=view, bones=20, bind_mesh=True)
    record = manifest['scenes'][0]
    scene = bpy.data.scenes[record['scene']]
    bpy.context.window.scene = scene
    actor = record['actors']['weapon']
    arm = bpy.data.objects[actor['armature']]
    meshes = [bpy.data.objects[n] for n in actor['meshes']]
    path = root / 'exports' / 'Bases' / 'SK_Bean_PirateGrenade.fbx'
    fbx(path, arm, meshes, False)
    asset(path, 'skeletal_mesh', 'PirateGrenade', view='shared', bones=len(arm.data.bones), bind_mesh=True)
    for record in manifest['scenes']:
        print('[grenade export]', record['view'], record['clip'], flush=True)
        scene = bpy.data.scenes[record['scene']]
        bpy.context.window.scene = scene
        scene.frame_start = record['start']
        scene.frame_end = record['end']
        scene.frame_set(0)
        for actor_name, group in [('bean', 'Bean'), ('weapon', 'PirateGrenade')]:
            actor = record['actors'][actor_name]
            arm = bpy.data.objects[actor['armature']]
            meshes = [bpy.data.objects[n] for n in actor['meshes']]
            if actor_name == 'bean' and record['view'] == 'TP':
                meshes = [next((o for o in meshes if 'Bean_Body' in o.name), meshes[0])]
            path = root / 'exports' / 'Animations' / ('%s.fbx' % actor['action'])
            fbx(path, arm, meshes, True, actor_name == 'bean')
            actor['animation_fbx'] = path.relative_to(root).as_posix()
            asset(path, 'animation', group, view=record['view'], clip=record['clip'], fps=record['fps'], start=record['start'],
                  end=record['end'], sample_count=record['end'] - record['start'] + 1, bind_mesh=True, events=record['events'])
    # static pickup from the template, bottom-centre pivot, UCX hull
    scene = bpy.data.scenes.new('SCN-Grenade-Exports')
    bpy.context.window.scene = scene
    ob, ucx, pivot = pickup_objects(template, scene)
    for kind in ('fbx', 'glb'):
        path = root / 'exports' / 'Static' / ('SM_Bean_PirateGrenade.' + kind)
        mesh_export(path, [ob], kind, [ucx] if kind == 'fbx' else ())
        asset(path, 'static_mesh', 'none', view='pickup', pivot='bottom_center', source_pivot_offset_m=[*pivot],
              collision='UCX_SM_Bean_PirateGrenade_00 (convex hull)' if kind == 'fbx' else 'none',
              triangles=C.triangle_count([ob]), bounds=C.world_bounds([ob]))
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
                        n.image.filepath = '//' + relative
                    textures[n.label or n.name] = relative
                    if any(link.from_node == n for link in p.inputs['Base Color'].links):
                        base_color_texture = relative
            materials[mat.name] = dict(base_color=list(p.inputs['Base Color'].default_value), metallic=p.inputs['Metallic'].default_value,
                                       roughness=p.inputs['Roughness'].default_value, textures=textures, base_color_texture=base_color_texture)
    manifest['materials'] = materials
    manifest['unreal_import'] = {'bean_source_skeleton_bones': 20, 'bean_helper_grips_exported': False, 'weapon_sockets_exported': True,
                                 'animation_only_import': True, 'import_mesh': False, 'fbx_frame_mapping': 'Blender frames0..N reimport as1..N+1',
                                 'actor_attachment': 'Both Bean and weapon actor roots use the same world transform; do not attach weapon to hand a second time.',
                                 'textures': 'BaseColor (sRGB) is wired in the FBX material; Normal (DirectX) through the Normal Map node; ORM (AO, '
                                             'roughness, metallic; linear) is NOT carried by FBX: import textures/T_*_ORM.png and wire G -> Roughness, B -> Metallic.',
                                 'runtime_tested': False}
    bpy.context.window.scene = saved_scene
    return assets
