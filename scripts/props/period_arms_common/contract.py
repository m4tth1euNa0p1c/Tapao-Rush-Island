"""Contract of the period weapon packs of T-334 (owner 2026-10-11: "crée plein d'autres armes de cette époque, modélise
toutes les actions..."): the bolt-action sniper rifle (scripts/props/boltrifle_pack), the drum sub-machine gun
(scripts/props/smg_pack) and the bayonet musket (scripts/props/bayonet_musket_pack).

Every pack writes art/export/Weapons/<Weapon>/pack.json, a manifest in the format of the weapon packs of this
repository (bean-musket-anim/1: scenes, weapon_contract, assets, fp_framing, materials, unreal_import; read by
scripts/character/import_pirate_weapons.py and pirate_weapon_clips.py), plus a gesture-centric "clips" table (gesture,
frames, fps, duration, loop, events with their times, the four FBX of the gesture). Pure Python, no bpy: the provisional
manifests (write_provisional.py) and the Blender builds (build.py of each pack) take the names, clips, frames, events,
bones and sockets from the same spec module (spec.py of each pack), so the contract does not drift.

Asset names (Unreal, from the FBX file names):
  SK_Bean_<Tag>                weapon skeletal mesh, its own skeleton (skeleton group <Tag>)
  SK_Bean_<Tag>_FP / _TP       the bean bases: FP arms (FPArm sleeves on the 20-bone bean skeleton) / full TP bean
                               (the TP base is not imported: in game the bean is the project's puppet)
  A_Bean_<Tag>_<View>_<Clip>   bean clip (project bean skeleton SK_Bean_Body_Skeleton)
  A_<Tag>_<View>_<Clip>        weapon clip (weapon skeleton)
  SM_Bean_<Tag>                pickup (weapon at rest, pivot bottom centre, UCX hulls)
  SM_Ammo_* / SM_<Tag>_*       ammunition pickups / effect meshes (casings, empty clips, dropped drums)
FBX: metres, apply_unit_scale + FBX_SCALE_UNITS (UnitScaleFactor 100: Unreal converts to cm with an import scale of 1,
root scale of the bean clips 100 like every weapon pack), axis_forward -Y, axis_up Z, armature named "Armature",
no leaf bones; the weapon root and the bean root share the same world transform in every clip (do not attach the
weapon to the hand a second time).
"""
from __future__ import annotations

FORMAT = 'bean-musket-anim/1'
VIEWS = ('TP', 'FP')


def ev(frame, name, socket=None):
    e = {'frame': int(frame), 'name': name}
    if socket:
        e['socket'] = socket
    return e


def events(spec, clip):
    return [ev(*e) for e in spec.EVENTS.get(clip, [])]


def bean_action(spec, view, clip):
    return f'A_Bean_{spec.TAG}_{view}_{clip}'


def weapon_action(spec, view, clip):
    return f'A_{spec.TAG}_{view}_{clip}'


def gesture_of(clip):
    """EFirearmGesture name of a clip (the packs name their clips after the gestures)."""
    return clip


def socket_bone(name):
    return name if name.startswith('socket_') else 'socket_' + name


def clip_table(spec):
    """Gesture-centric view of the clips: what the game logic needs (lengths, events and their times, files)."""
    rows = []
    for clip, frames in spec.CLIPS.items():
        evs = []
        for e in events(spec, clip):
            row = dict(e)
            row['time_s'] = round(e['frame'] / spec.FPS, 4)
            evs.append(row)
        files = {}
        for view in VIEWS:
            files[f'bean_{view.lower()}'] = f'exports/Animations/{bean_action(spec, view, clip)}.fbx'
            files[f'weapon_{view.lower()}'] = f'exports/Animations/{weapon_action(spec, view, clip)}.fbx'
        rows.append(dict(gesture=gesture_of(clip), clip=clip, frames=frames, fps=spec.FPS, duration_s=round(frames / spec.FPS, 4),
                         loop=clip in spec.LOOPS, new_gesture=clip in getattr(spec, 'NEW_GESTURES', ()), events=evs,
                         description=getattr(spec, 'CLIP_NOTES', {}).get(clip, ''), files=files))
    return rows


def weapon_contract(spec, positions=None):
    """Bones and sockets of the weapon skeleton (positions in weapon space, metres: +X muzzle, +Z up, -Y right side).
    positions: measured {bone: head} from the built model (the spec's values otherwise)."""
    base = list(getattr(spec, 'MUSKET_BONES', []))           # the bayonet musket keeps the musket's skeleton first
    bones = base + [b for b in spec.BONES if b not in base] + list(getattr(spec, 'MARKERS', {}))
    sockets = {}
    socket_positions = {}
    socket_parents = {}
    for name in getattr(spec, 'MUSKET_SOCKETS', []):
        bone = socket_bone(name)
        if positions and bone in positions:
            sockets[name] = socket_positions[bone] = [round(v, 5) for v in positions[bone]]
        bones.append(bone)
    for name, val in spec.SOCKETS.items():
        parent, pos = val[0], val[1]
        bone = socket_bone(name)
        p = [round(v, 5) for v in (positions.get(bone, pos) if positions else pos)]
        sockets[name] = p
        socket_positions[bone] = p
        socket_parents[bone] = parent
        bones.append(bone)
    out = dict(bones=bones, deform_bones=base + [b for b in spec.BONES if b not in base], marker_bones=list(getattr(spec, 'MARKERS', {})),
               sockets=sockets, socket_positions=socket_positions, socket_parents=socket_parents,
               root_bone=spec.ROOT_BONE, grip_sockets={'r': 'socket_grip_r', 'l': 'socket_grip_l'}, muzzle_socket='socket_muzzle',
               contract_names=dict(spec.CONTRACT_NAMES), units='metres', forward_axis='+X', up_axis='+Z', right_side='-Y',
               moving_parts=dict(getattr(spec, 'MOVING_PARTS', {})))
    for key in ('BOUNDS_PLANNED',):
        if hasattr(spec, key):
            out['bounds_planned_m'] = getattr(spec, key)
    return out


def planned_assets(spec):
    """The asset rows the build will write (sha256 None until built)."""
    rows = []

    def row(path, kind, group, **kw):
        rows.append(dict(path=path, type=kind, skeleton_group=group, sha256=None, planned=True, **kw))

    for view in VIEWS:
        row(f'exports/Bases/SK_Bean_{spec.TAG}_{view}.fbx', 'skeletal_mesh', 'Bean', view=view, bones=20, bind_mesh=True)
    row(f'exports/Bases/SK_Bean_{spec.TAG}.fbx', 'skeletal_mesh', spec.TAG, view='shared', bind_mesh=True)
    for view in VIEWS:
        for clip, frames in spec.CLIPS.items():
            for actor, group, name in (('bean', 'Bean', bean_action(spec, view, clip)), ('weapon', spec.TAG, weapon_action(spec, view, clip))):
                row(f'exports/Animations/{name}.fbx', 'animation', group, view=view, clip=clip, fps=spec.FPS, start=0, end=frames,
                    sample_count=frames + 1, bind_mesh=True, events=events(spec, clip), loop=clip in spec.LOOPS)
    for name, info in spec.STATICS.items():
        for kind in ('fbx', 'glb'):
            row(f'exports/Static/{name}.{kind}', 'static_mesh', 'none', view=info['view'], pivot='bottom_center', role=info['role'])
    return rows


def scenes(spec):
    rows = []
    for view in VIEWS:
        for clip, frames in spec.CLIPS.items():
            rows.append(dict(scene=f'{spec.TAG.upper()}-{view}-{clip}', view=view, clip=clip, start=0, end=frames, fps=spec.FPS,
                             loop=clip in spec.LOOPS,
                             actors=dict(bean=dict(action=bean_action(spec, view, clip),
                                                   animation_fbx=f'exports/Animations/{bean_action(spec, view, clip)}.fbx'),
                                         weapon=dict(action=weapon_action(spec, view, clip),
                                                     animation_fbx=f'exports/Animations/{weapon_action(spec, view, clip)}.fbx')),
                             events=events(spec, clip), preview_fx=[],
                             grip_contract=dict(bean_tail_bones={'l': 'grip_l', 'r': 'grip_r'},
                                                weapon_head_bones={'l': 'socket_grip_l', 'r': 'socket_grip_r'})))
    return rows


def unreal_hints(spec):
    return dict(destination=f'/Game/MonkeIsland/Weapons/{spec.TAG}',
                weapon_mesh=f'SK_Bean_{spec.TAG}', first_person_arms=f'SK_Bean_{spec.TAG}_FP', pickup=f'SM_Bean_{spec.TAG}',
                idle_clip_tp=bean_action(spec, 'TP', spec.IDLE_CLIP), bean_skeleton='/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton',
                rules=dict(spec.RULES_HINTS), import_registry_entry=dict(spec.IMPORT_ENTRY),
                new_gestures=list(getattr(spec, 'NEW_GESTURES', ())))


def manifest_skeleton(spec, version, status, pack_root):
    """The manifest before the build (status 'provisional') or its frame (the build fills scenes, assets, materials,
    fp_framing with measured values)."""
    return dict(format=FORMAT, pack=spec.PACK_ID, weapon=spec.TAG, display_name=spec.DISPLAY, version=version, status=status,
                fps=spec.FPS, local_build=True, task='T-334', pack_root=pack_root,
                built_by=f'scripts/props/{spec.SCRIPT_DIR}/build.py (T-334)',
                source=dict(blend='art/blender/characters/bean_soldier.blend'),
                views=list(VIEWS), clips=clip_table(spec), scenes=scenes(spec), weapon_contract=weapon_contract(spec),
                assets=planned_assets(spec), loops=sorted(spec.LOOPS),
                requested_clips=list(spec.CLIPS), delivered_clips=[], not_delivered_clips=list(spec.CLIPS),
                statics={k: dict(v) for k, v in spec.STATICS.items()},
                unreal_hints=unreal_hints(spec), notes=list(spec.NOTES),
                coordinate_system={'blender': '+X weapon forward, +Z up, -Y right side; the bean looks along -Y, its right side is -X',
                                   'fbx': 'axis_forward -Y, axis_up Z, metres (UnitScaleFactor 100)',
                                   'unreal': 'convert_scene + convert_scene_unit, import scale 1; a weapon point (x, y, z) m -> (x, -y, z) * 100 cm'})
