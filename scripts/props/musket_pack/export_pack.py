"""Portable FBX bases/animations, the static pickup and the ammunition of the musket pack (local copy of the second
PC's bean-musket-anim v0.2 export_pack.py, T-212; v2 T-246: baked textures, ammunition with UCX collision).

Every animation contains a bind mesh; animation-only armature FBXs lose their bind pose on some Blender/Unreal round
trips. Unreal imports those as animations. The armature is exported as "Armature" (Unreal then drops the node): any
other object of that name is renamed for the time of the export, so the FBX never says "Armature.001" (the v0.2 pack
did, hence scripts/character/fbx_rename_armature.py). The baked textures live in <pack>/textures and are copied next
to the base and static FBX (path_mode COPY: Unreal picks them up with the materials); the 100+ animation FBX only
name them (STRIP).
"""
from __future__ import annotations
import hashlib, json, shutil
from pathlib import Path
import bmesh
import bpy
from mathutils import Matrix, Vector


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def select(objs):
    for o in bpy.context.selected_objects: o.select_set(False)
    for o in objs: o.hide_set(False); o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def fbx(path, arm, meshes, animated, bean=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    scene = bpy.context.scene; saved = arm.animation_data.action if arm.animation_data else None
    name = arm.name; oldpose = {p.name: p.matrix_basis.copy() for p in arm.pose.bones}
    if not animated:
        if arm.animation_data: arm.animation_data.action = None
        for p in arm.pose.bones: p.matrix_basis = Matrix.Identity(4)
    other = bpy.data.objects.get('Armature')
    if other is not None and other != arm: other.name = 'Armature-renamed-for-export'
    arm.name = 'Armature'
    assert arm.name == 'Armature', arm.name
    select([arm] + meshes); bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE', 'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
        use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, use_armature_deform_only=bean,
        bake_anim=animated, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_step=1., bake_anim_simplify_factor=0.,
        path_mode='STRIP' if animated else 'COPY', embed_textures=False)
    arm.name = name
    for p in arm.pose.bones: p.matrix_basis = oldpose[p.name]
    if arm.animation_data: arm.animation_data.action = saved
    scene.frame_set(scene.frame_current); bpy.context.view_layer.update()


def mesh_export(path, meshes, kind):
    path.parent.mkdir(parents=True, exist_ok=True); select(meshes)
    if kind == 'fbx':
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
            apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
            use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, path_mode='COPY', embed_textures=False)
    else:
        bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True, use_active_scene=True,
                                  export_animations=False, export_cameras=False, export_lights=False, export_apply=True)


def convex_hull(name, source, coll):
    """UCX_<name>_00: the convex hull of the evaluated mesh, no material (Unreal's simple collision)."""
    dg = bpy.context.evaluated_depsgraph_get()
    me = source.evaluated_get(dg).to_mesh()
    bm = bmesh.new()
    verts = [bm.verts.new(v.co) for v in me.vertices]
    source.evaluated_get(dg).to_mesh_clear()
    bmesh.ops.convex_hull(bm, input=verts)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bmesh.ops.dissolve_limit(bm, angle_limit=0.08, verts=bm.verts, edges=bm.edges)
    hull = bpy.data.meshes.new('UCX_%s_00' % name)
    bm.to_mesh(hull); bm.free()
    ob = bpy.data.objects.new('UCX_%s_00' % name, hull)
    coll.objects.link(ob)
    return ob


def mesh_stats(ob):
    dg = bpy.context.evaluated_depsgraph_get()
    me = ob.evaluated_get(dg).to_mesh()
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
    out = dict(triangles=tris, size_m=[round(max(xs) - min(xs), 4), round(max(ys) - min(ys), 4), round(max(zs) - min(zs), 4)],
               min_m=[round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)])
    ob.evaluated_get(dg).to_mesh_clear()
    return out


def export_pack(root, manifest, source, template, ammo=None):
    root = Path(root); assets = []; saved_scene = bpy.context.scene
    def asset(path, kind, group, **kw):
        row = dict(path=path.relative_to(root).as_posix(), type=kind, skeleton_group=group, sha256=sha(path), **kw)
        assets.append(row); return row
    # Separate FP mesh base, same original twenty-bone Bean rest hierarchy.
    for view in ('TP', 'FP'):
        record = next((r for r in manifest['scenes'] if r['view'] == view), None)
        if record is None: continue
        scene = bpy.data.scenes[record['scene']]; bpy.context.window.scene = scene; scene.frame_set(0)
        actor = record['actors']['bean']; arm = bpy.data.objects[actor['armature']]; meshes = [bpy.data.objects[n] for n in actor['meshes']]
        path = root / 'exports' / 'Bases' / ('SK_Bean_Musket_' + view + '.fbx')
        fbx(path, arm, meshes, False, True); asset(path, 'skeletal_mesh', 'Bean', view=view, bones=20, bind_mesh=True)
    record = manifest['scenes'][0]; scene = bpy.data.scenes[record['scene']]; bpy.context.window.scene = scene
    actor = record['actors']['weapon']; arm = bpy.data.objects[actor['armature']]; meshes = [bpy.data.objects[n] for n in actor['meshes']]
    path = root / 'exports' / 'Bases' / 'SK_Bean_PirateMusket.fbx'; fbx(path, arm, meshes, False)
    asset(path, 'skeletal_mesh', 'PirateMusket', view='shared', bones=len(arm.data.bones), bind_mesh=True,
          triangles=manifest['weapon_contract'].get('triangles'))
    for record in manifest['scenes']:
        print('[musket export]', record['view'], record['clip'], flush=True)
        scene = bpy.data.scenes[record['scene']]; bpy.context.window.scene = scene
        scene.frame_start = record['start']; scene.frame_end = record['end']; scene.frame_set(0)
        for actor_name, group in [('bean', 'Bean'), ('weapon', 'PirateMusket')]:
            actor = record['actors'][actor_name]; arm = bpy.data.objects[actor['armature']]
            meshes = [bpy.data.objects[n] for n in actor['meshes']]
            if actor_name == 'bean' and record['view'] == 'TP': meshes = [next((o for o in meshes if 'Bean_Body' in o.name), meshes[0])]
            path = root / 'exports' / 'Animations' / ('%s.fbx' % actor['action'])
            fbx(path, arm, meshes, True, actor_name == 'bean')
            actor['animation_fbx'] = path.relative_to(root).as_posix()
            asset(path, 'animation', group, view=record['view'], clip=record['clip'], fps=record['fps'], start=record['start'], end=record['end'],
                  sample_count=record['end'] - record['start'] + 1, bind_mesh=True, events=record['events'])
    # Static prop derives from the authoritative template, at a bottom-centre pivot.
    scene = bpy.data.scenes.new('SCN-Musket-Exports'); bpy.context.window.scene = scene
    scene.collection.children.link(template['collection']); arm = template['armature']
    for p in arm.pose.bones: p.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update(); dg = bpy.context.evaluated_depsgraph_get()
    meshes = []; bounds = template['bounds']; pivot = Vector(((bounds['min'][0] + bounds['max'][0]) / 2, (bounds['min'][1] + bounds['max'][1]) / 2, bounds['min'][2]))
    for ob in template['weapon_meshes']:
        me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), depsgraph=dg, preserve_all_data_layers=True)
        me.transform(Matrix.Translation(-pivot) @ ob.matrix_world)
        clone = bpy.data.objects.new('GEO-Pickup-' + ob.name, me); scene.collection.objects.link(clone); meshes.append(clone)
    for kind in ('fbx', 'glb'):
        path = root / 'exports' / 'Static' / ('SM_Bean_PirateMusket.' + kind); mesh_export(path, meshes, kind)
        asset(path, 'static_mesh', 'none', view='pickup', pivot='bottom_center', source_pivot_offset_m=[*pivot],
              triangles=manifest['weapon_contract'].get('triangles'))
    # Ammunition: bottom-centre pivot already, one convex UCX each in the FBX (the glb is a preview copy).
    ammo_coll = bpy.data.collections.new('COL-Ammo-Exports'); scene.collection.children.link(ammo_coll)
    for name, ob in (ammo or {}).items():
        clone = ob.copy(); clone.name = name; clone.matrix_world = Matrix.Identity(4); ammo_coll.objects.link(clone)
        hull = convex_hull(name, clone, ammo_coll)
        stats = mesh_stats(clone)
        path = root / 'exports' / 'Static' / (name + '.fbx'); mesh_export(path, [clone, hull], 'fbx')
        asset(path, 'static_mesh', 'none', view='ammo', pivot='bottom_center', collision=hull.name, collision_vertices=len(hull.data.vertices),
              size_m=stats['size_m'], triangles=stats['triangles'], material=next((m.name for m in clone.data.materials if m), None))
        path = root / 'exports' / 'Static' / (name + '.glb'); mesh_export(path, [clone], 'glb')
        asset(path, 'static_mesh', 'none', view='ammo', pivot='bottom_center', size_m=stats['size_m'], triangles=stats['triangles'])
        clone.name = 'GEO-Export-' + name
    # Export scene is an optional inspectable appendix, not a presentation clip.
    scene.collection.children.unlink(template['collection'])
    manifest['static_export_scene'] = scene.name
    materials = {}
    for obj in bpy.data.objects:
        if obj.type != 'MESH': continue
        for mat in obj.data.materials:
            if not mat or mat.name in materials: continue
            p = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if mat.use_nodes else None
            if not p: continue
            textures = {}; base_color_texture = None
            for n in mat.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image and n.image.filepath:
                    source = Path(bpy.path.abspath(n.image.filepath)).resolve()
                    try: relative = source.relative_to(root.resolve()).as_posix()
                    except ValueError:
                        target = root / 'textures' / source.name; target.parent.mkdir(exist_ok=True, parents=True)
                        if source.exists(): shutil.copy2(source, target)
                        relative = target.relative_to(root).as_posix(); n.image.filepath = '//' + relative
                    textures[n.label or n.name] = relative
                    if any(link.from_node == n for link in p.inputs['Base Color'].links): base_color_texture = relative
            row = dict(base_color=list(p.inputs['Base Color'].default_value), metallic=p.inputs['Metallic'].default_value,
                       roughness=p.inputs['Roughness'].default_value, textures=textures, base_color_texture=base_color_texture)
            if mat.get('baked'):
                row.update(baked=True, atlas=mat.get('atlas'), normal_convention='DirectX (green down): import in Unreal without flip',
                           orm_channels='R = ambient occlusion, G = roughness, B = metallic (linear)', base_color_space='sRGB')
            materials[mat.name] = row
    manifest['materials'] = materials
    manifest['unreal_import'] = {'bean_source_skeleton_bones': 20, 'bean_helper_grips_exported': False, 'weapon_sockets_exported': True,
        'animation_only_import': True, 'import_mesh': False, 'fbx_frame_mapping': 'Blender frames0..N reimport as1..N+1',
        'actor_attachment': 'Both Bean and weapon actor roots use the same world transform; do not attach weapon to hand a second time.',
        'ammunition': 'exports/Static/SM_Ammo_*.fbx: static meshes, pivot bottom centre, UCX_<Name>_00 convex collision in the file '
                      '(import with collision from file, not auto generated); not in import_pirate_weapons.py WEAPONS["Musket"]["static"].',
        'runtime_tested': False}
    bpy.context.window.scene = saved_scene
    return assets
