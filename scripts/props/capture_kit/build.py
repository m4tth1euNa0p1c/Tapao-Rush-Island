"""Capture kit of T-337 (lot L5, owner request 2026-10-11: "pouvoir poser cage pendant 2 minutes sur place, animation avec
F sur plage avec nouvelle item : cage ; crée nouvelle item, menotte en acier, permettant de menotter un pnj, de le ramener
dans cage et le vendre"): models, textures, animations, manifests and control renders, all procedural.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
      --python scripts/props/capture_kit/build.py -- --all
  options: --textures (CC0 recoloured; reused when present, --force-textures to redo), --models, --anims, --previews,
           --save (art/blender/props/capture_kit.blend), --all (= all of them)

Outputs (contract with T-336, which codes the gameplay):
  art/export/Props/Capture/        SM_Capture_Cage, SM_Capture_Cage_Door, SM_Capture_Cage_Folded, SM_Steel_Handcuffs,
                                   SM_Steel_Handcuffs_Open, SM_Chain_Link (.fbx for Unreal, .glb for the icons),
                                   Textures/T_Capture_{Wood,Iron,Steel}_{D,N,ORM}.png, manifest.json
  art/export/Characters/Capture/   A_Bean_Cage_Place, A_Bean_Cuff, A_Bean_Lead_Walk, A_BeanFR_Captive_Idle,
                                   A_BeanFR_Captive_Walk (.fbx on SK_Bean_Body_Skeleton), manifest.json
  art/_previews/capture/           control renders
Icons: scripts/ui/render_capture_icons.py then scripts/ui/finish_capture_icons.py (after this build).
Blender in the background only (RAM first for Unreal); bean_soldier.blend is read, never written.
"""
from __future__ import annotations

import argparse
import datetime as dt
import importlib
import json
import math
import sys
import time
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import kit as K  # noqa: E402
import textures as T  # noqa: E402
import models as M  # noqa: E402
import rig as R  # noqa: E402
import clips as C  # noqa: E402
import previews as P  # noqa: E402

for _m in (K, T, M, R, C, P):
    importlib.reload(_m)

PROPS_OUT = REPO / 'art' / 'export' / 'Props' / 'Capture'
GLB_OUT = REPO / 'build' / 'capture_kit' / 'glb'      # icons and previews only (build/ is not versioned)
CLIPS_OUT = REPO / 'art' / 'export' / 'Characters' / 'Capture'
PREV = REPO / 'art' / '_previews' / 'capture'
BLEND = REPO / 'art' / 'blender' / 'props' / 'capture_kit.blend'
SKELETON_UE = '/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton'
STATE = {}


def log(msg):
    print(f'[capture] {msg}', flush=True)


def rel(p):
    return Path(p).resolve().relative_to(REPO).as_posix()


# ------------------------------------------------------------------------------------------------- models
def socket_rows(sockets):
    out = {}
    for name, (loc, rot) in sockets.items():
        yaw_b = math.degrees(rot[2])
        out[name] = {'blender_m': [round(v, 4) for v in loc], 'unreal_cm': K.to_unreal_cm(loc),
                     'unreal_yaw_deg': round(-yaw_b, 2) if abs(yaw_b) > 1e-6 else 0.0}
    return out


def export_asset(ob, ucx, sockets, coll, glb=True):
    socks = []
    for n, (loc, rot) in sockets.items():
        s = K.socket(n, loc, ob, coll, rot)
        assert s.name == f'SOCKET_{n}', f'{ob.name}: socket name taken in the scene ({s.name})'
        socks.append(s)
    fbx = K.export_fbx([ob] + list(ucx) + socks, PROPS_OUT / f'{ob.name}.fbx')
    files = [rel(fbx)]
    for s in socks:                  # free the SOCKET_ names for the next asset (Blender would add .001)
        s.name = f'{ob.name}-{s.name}'
    if glb:
        try:
            GLB_OUT.mkdir(parents=True, exist_ok=True)
            K.export_glb([ob], GLB_OUT / f'{ob.name}.glb')
        except Exception as error:  # noqa: BLE001 - the GLB only feeds the icons
            log(f'GLB {ob.name} failed: {error!r}')
    stats = K.mesh_stats(ob)
    return {'files': files, 'triangles': stats['triangles'], 'vertices': stats['vertices'], 'size_m': stats['size_m'],
            'bounds_min_m': stats['min_m'], 'bounds_max_m': stats['max_m'],
            'size_cm_unreal': [round(v * 100, 1) for v in stats['size_m']],
            'collision': [u.name for u in ucx], 'sockets': socket_rows(sockets),
            'materials': [m.name for m in ob.data.materials]}


def build_models(scene, mats, bound):
    coll = bpy.data.collections.new('COL-Capture-Models')
    scene.collection.children.link(coll)
    assets = {}
    objects = {}
    ob, ucx, socks, info = M.build_cage(coll, mats)
    info['frame_size_m'] = [M.CAGE['depth'], M.CAGE['width'], M.CAGE['height']]
    info['note'] = ('cadre 1,2 x 1,6 x 1,9 m ; la boîte englobante ajoute les anneaux de levage (5 cm de chaque côté en X), '
                    'les bouts des patins (Y) et la quincaillerie de la porte')
    assets['SM_Capture_Cage'] = dict(export_asset(ob, ucx, socks, coll), **{'info': info})
    objects['SM_Capture_Cage'] = ob
    ob, ucx, socks_d, info = M.build_door(coll, mats)
    assets['SM_Capture_Cage_Door'] = dict(export_asset(ob, ucx, socks_d, coll), **{'info': info})
    objects['SM_Capture_Cage_Door'] = ob
    ob, ucx, socks, info = M.build_folded(coll, mats)
    assets['SM_Capture_Cage_Folded'] = dict(export_asset(ob, ucx, socks, coll), **{'info': info})
    objects['SM_Capture_Cage_Folded'] = ob
    STATE['crate_grip_local'] = Vector(socks['grip'][0])
    ob, info = M.build_chain_link(coll, mats)
    assets['SM_Chain_Link'] = dict(export_asset(ob, [], {}, coll), **{'info': info})
    objects['SM_Chain_Link'] = ob
    # closed cuffs from the captive's bound wrists (reference pose)
    ob, socks, info, Mframe = M.build_handcuffs(coll, mats, bound.cuff['r'], bound.cuff['l'])
    assets['SM_Steel_Handcuffs'] = dict(export_asset(ob, [], socks, coll), **{'info': info})
    objects['SM_Steel_Handcuffs'] = ob
    STATE['cuffs_frame'] = Mframe
    ob, socks, info = M.build_handcuffs_open(coll, mats)
    assets['SM_Steel_Handcuffs_Open'] = dict(export_asset(ob, [], socks, coll), **{'info': info})
    objects['SM_Steel_Handcuffs_Open'] = ob
    for o in coll.objects:
        if o.type == 'EMPTY' or o.name.startswith('UCX_'):
            o.hide_viewport = True
    STATE['models'] = objects
    STATE['model_coll'] = coll
    return assets


# ------------------------------------------------------------------------------------------------- held props
def held_transforms(rig):
    """Rest pose with the right mitten turned to carry (bone straight down, thumb forward): the folded cage by its handle
    and the open cuffs by their ring, both at the mitten centre; relative transforms to hand_r"""
    mats = rig.fk({})
    rot = rig.hand_rotation('r', rig.hand_frame((0, 0, -1), (0, -1, 0)))
    grip = rig.grip_of(mats, 'r') + Vector((0.0, -0.01, 0.0))
    rig.solve_arm(mats, 'r', rig.wrist_for_grip('r', grip, rot), rot)
    grip = rig.grip_of(mats, 'r')
    held = Matrix(((0, 1, 0), (-1, 0, 0), (0, 0, 1))).to_4x4()      # X forward (-Y), Y = +X, Z up
    crate_world = Matrix.Translation(grip) @ held @ Matrix.Translation(-STATE['crate_grip_local'])
    cuffs_world = Matrix.Translation(grip) @ held
    return {'crate_rel': mats['hand_r'].inverted() @ crate_world, 'cuffs_rel': mats['hand_r'].inverted() @ cuffs_world,
            'crate_world': crate_world, 'cuffs_world': cuffs_world, 'mats': mats}


# ------------------------------------------------------------------------------------------------- clips
def build_clips(scene, rig, bound):
    held = held_transforms(rig)
    STATE['held'] = held
    built = []
    built.append(C.captive_idle(rig, bound))
    built.append(C.captive_walk(rig, bound))
    built.append(C.lead_walk(rig))
    built.append(C.cuff(rig, bound, held['cuffs_rel']))
    built.append(C.cage_place(rig, held['crate_rel']))
    rows = {}
    actions = {}
    for clip, samples in built:
        action = rig.bake(samples, clip.name)
        actions[clip.name] = action
        path = R.export_clip(rig.arm, action, CLIPS_OUT / f'{clip.name}.fbx', clip.frames)
        row = {'file': rel(path), 'skeleton': SKELETON_UE, 'target': clip.target, 'summary': clip.summary,
               'fps': C.FPS, 'frames': clip.frames, 'keys': clip.frames + 1, 'seconds': round(clip.seconds, 3),
               'loop': clip.loop, 'root_motion': False, 'events': clip.events,
               'qa': {k: v for k, v in clip.qa.items() if k != 'per_frame'}}
        if clip.qa['max_hand_error_m'] > 0.005:
            log(f'{clip.name}: hand error per frame {clip.qa["per_frame"]}')
        if hasattr(clip, 'speed_cm_s'):
            row['no_slide_speed_cm_s'] = clip.speed_cm_s
        rows[clip.name] = row
        log(f'{clip.name}: {clip.frames} frames, qa {clip.qa}')
    STATE['clips'] = {c.name: c for c, _ in built}
    STATE['samples'] = {c.name: s for c, s in built}
    STATE['actions'] = actions
    rig.arm.animation_data.action = None
    return rows


def cuff_fit(rig, bound):
    """The closed cuffs, rigid on hand_r: how far their left bracelet drifts from the left wrist over the captive clips"""
    M_ref = STATE['cuffs_frame']
    local = {}
    C.captive_trunk(rig, local, 0.0, 0.0, 0.0)
    mats = rig.fk(local)
    bound.solve(mats)
    rel_r = mats['hand_r'].inverted() @ M_ref
    left_in_cuffs = M_ref.inverted() @ bound.cuff['l'].translation
    worst = 0.0
    for name in ('A_BeanFR_Captive_Idle', 'A_BeanFR_Captive_Walk'):
        for mats in STATE['samples'][name]:
            cuffs = mats['hand_r'] @ rel_r
            want = rig.carried(mats, 'hand_l') @ (bound.rig.bones['hand_l'].head_local +
                                                  (bound.rig.bones['hand_l'].tail_local - bound.rig.bones['hand_l'].head_local).normalized() * C.CUFF_ALONG)
            worst = max(worst, ((cuffs @ left_in_cuffs) - want).length)
    STATE['cuffs_rel_hand_r'] = rel_r
    STATE['cuff_ref_mats'] = mats
    return round(worst, 4), R.relative_to_bone(rig, STATE['samples']['A_BeanFR_Captive_Idle'][0], 'hand_r',
                                               STATE['samples']['A_BeanFR_Captive_Idle'][0]['hand_r'] @ rel_r)


# ------------------------------------------------------------------------------------------------- previews
def previews(rig, bean_meshes, mats):
    PREV.mkdir(parents=True, exist_ok=True)
    out = []
    models = STATE['models']

    def prop(name, coll, label=None):
        src = models[name]
        ob = bpy.data.objects.new(label or ('PRV-' + name), src.data)
        coll.objects.link(ob)
        return ob

    def captive(coll, name, action):
        a, meshes = P.french_captive(rig.arm, coll, name)
        if a is None:
            a, meshes = P.clone_character(rig.arm, bean_meshes, coll, name)
        R.assign_action(a, STATE['actions'][action])
        return a, meshes

    def cuffs_on(coll, arm_obj, label):
        ob = prop('SM_Steel_Handcuffs', coll, label)
        return ob

    def place_cuffs(ob, arm_obj):
        pb = arm_obj.pose.bones['hand_r']
        ob.matrix_world = arm_obj.matrix_world @ pb.matrix @ STATE['cuffs_rel_hand_r']

    # 1. the cage, closed with the captive inside, then open
    scene, coll, cam = P.new_scene('PRV-Cage')
    cage = prop('SM_Capture_Cage', coll)
    door = prop('SM_Capture_Cage_Door', coll)
    hinge = Vector(STATE['assets']['SM_Capture_Cage']['sockets']['door_hinge']['blender_m'])
    a, _m = captive(coll, 'FR-Cage', 'A_BeanFR_Captive_Idle')
    # prisoner on its socket, facing the door (+X): the bean faces -Y, turn it +90 deg
    a.matrix_world = Matrix.Translation((0, 0, 0.2)) @ Matrix.Rotation(math.radians(90), 4, 'Z')
    cuffs = cuffs_on(coll, a, 'PRV-Cuffs-Cage')
    bpy.context.window.scene = scene
    scene.frame_set(0)
    place_cuffs(cuffs, a)
    door.matrix_world = Matrix.Translation(hinge)
    P.aim(cam, (0.0, 0.0, 0.85), 125, 16, 5.2, lens=40)
    parts = [P.render(scene, PREV / '_cage_a.png', 0, (900, 900))]
    door.matrix_world = Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(-100), 4, 'Z')
    for o in [a] + list(_m) + [cuffs]:
        o.hide_render = True
    P.aim(cam, (0.0, 0.0, 0.85), 55, 18, 5.2, lens=40)
    parts.append(P.render(scene, PREV / '_cage_b.png', 0, (900, 900)))
    out.append(P.strip(parts, PREV / 'cage_closed_open.png'))
    # 2. folded crate, cuffs (closed and open), chain links
    scene, coll, cam = P.new_scene('PRV-Items', sky=(0.32, 0.33, 0.35), floor=(0.36, 0.35, 0.34))
    crate = prop('SM_Capture_Cage_Folded', coll)
    crate.matrix_world = Matrix.Translation((-0.35, 0.05, 0.0)) @ Matrix.Rotation(math.radians(20), 4, 'Z')
    c1 = prop('SM_Steel_Handcuffs', coll)
    c1.matrix_world = Matrix.Translation((0.06, -0.20, 0.13)) @ Matrix.Rotation(math.radians(-15), 4, 'Z')
    c2 = prop('SM_Steel_Handcuffs_Open', coll)
    c2.matrix_world = Matrix.Translation((0.30, 0.08, 0.20)) @ Matrix.Rotation(math.radians(30), 4, 'Z')
    from models import LINK
    for k in range(7):
        ln = prop('SM_Chain_Link', coll, f'PRV-Link-{k}')
        ln.matrix_world = Matrix.Translation((0.05 + k * LINK['inner_l'], 0.12, 0.012)) @ \
            Matrix.Rotation(math.radians(90 * (k % 2)), 4, 'X') @ Matrix.Rotation(math.radians(0), 4, 'Z')
    P.aim(cam, (0.0, -0.02, 0.10), 20, 34, 1.55, lens=50)
    out.append(P.render(scene, PREV / 'items_crate_cuffs_chain.png', 0, (1280, 800)))
    # 3. captive idle and walk, three views each, cuffs on
    for action, label, phases in (('A_BeanFR_Captive_Idle', 'captive_idle', (0, 0, 0)),
                                  ('A_BeanFR_Captive_Walk', 'captive_walk', (0, 12, 30))):
        scene, coll, cam = P.new_scene('PRV-' + label)
        a, _m = captive(coll, 'FR-' + label, action)
        cuffs = cuffs_on(coll, a, 'PRV-Cuffs-' + label)
        parts = []
        for k, (yaw, el) in enumerate(((0, 6), (90, 6), (35, 22), (12, 18))):
            bpy.context.window.scene = scene
            f = phases[min(k, 2)]
            scene.frame_set(f)
            place_cuffs(cuffs, a)
            if k < 3:
                P.aim(cam, (0.0, 0.0, 0.55), yaw, el, 9.0, ortho=1.45)
            else:                    # close-up of the bound wrists
                P.aim(cam, (0.0, -0.3, 0.46), yaw, el, 9.0, ortho=0.5)
            parts.append(P.render(scene, PREV / f'_{label}_{k}.png', f, (620, 720)))
        out.append(P.strip(parts, PREV / f'{label}_3views.png'))
    # 4. the player sets the cage down (crate, then the cage at CagePlaced)
    clip = STATE['clips']['A_Bean_Cage_Place']
    scene, coll, cam = P.new_scene('PRV-CagePlace')
    a, _m = P.clone_character(rig.arm, bean_meshes, coll, 'Bean-Place')
    R.assign_action(a, STATE['actions']['A_Bean_Cage_Place'])
    crate = prop('SM_Capture_Cage_Folded', coll)
    cage = prop('SM_Capture_Cage', coll)
    door = prop('SM_Capture_Cage_Door', coll)
    parts = []
    for f in (0, 8, 12, 20, 28, 45):
        bpy.context.window.scene = scene
        scene.frame_set(f)
        vis, m = clip.props['SM_Capture_Cage_Folded'][f]
        P.place_prop(crate, vis, m)
        vis_c, mc = clip.props['SM_Capture_Cage'][f]
        P.place_prop(cage, vis_c, mc)
        P.place_prop(door, vis_c, mc @ Matrix.Translation(hinge))
        P.aim(cam, (0.0, -0.95, 0.75), 114, 14, 5.0, lens=45)
        parts.append(P.render(scene, PREV / f'_place_{f}.png', f, (640, 640)))
    out.append(P.strip(parts, PREV / 'player_cage_place.png', columns=3))
    # 5. the player cuffs the captive standing in front of him
    clip = STATE['clips']['A_Bean_Cuff']
    scene, coll, cam = P.new_scene('PRV-Cuff')
    a, _m = P.clone_character(rig.arm, bean_meshes, coll, 'Bean-Cuff')
    R.assign_action(a, STATE['actions']['A_Bean_Cuff'])
    fr, _fm = captive(coll, 'FR-Cuffed', 'A_BeanFR_Captive_Idle')
    fr.matrix_world = clip.pair['captive_root']
    cuffs_open = prop('SM_Steel_Handcuffs_Open', coll)
    cuffs = cuffs_on(coll, fr, 'PRV-Cuffs-Cuff')
    parts = []
    for f in (0, 10, 21, 33):
        bpy.context.window.scene = scene
        scene.frame_set(f)
        vis, m = clip.props['SM_Steel_Handcuffs_Open'][f]
        P.place_prop(cuffs_open, vis, m)
        place_cuffs(cuffs, fr)
        cuffs.hide_render = f < 21
        P.aim(cam, (0.0, -0.43, 0.55), 72, 10, 3.0, lens=50)
        parts.append(P.render(scene, PREV / f'_cuff_{f}.png', f, (640, 640)))
    out.append(P.strip(parts, PREV / 'player_cuff.png', columns=2))
    # QA: do the two characters touch (hats, bodies) while the player cuffs?
    from mathutils.bvhtree import BVHTree

    def tree(objs):
        dg = bpy.context.evaluated_depsgraph_get()
        verts, polys = [], []
        for o in objs:
            ev = o.evaluated_get(dg)
            me = ev.to_mesh()
            base = len(verts)
            verts += [ev.matrix_world @ v.co for v in me.vertices]
            polys += [[base + i for i in p.vertices] for p in me.polygons]
            ev.to_mesh_clear()
        return BVHTree.FromPolygons(verts, polys)
    hats_p = [o for o in _m if 'Hat' in o.name]
    hats_c = [o for o in _fm if 'Hat' in o.name]
    body_p = [o for o in _m if o.name.endswith(('Bean_Body', 'Coat'))]
    body_c = [o for o in _fm if o.name.endswith(('Bean_Body', 'Coat'))]
    touches = {}
    for f in range(0, clip.frames + 1, 3):
        scene.frame_set(f)
        touches[f] = {'hats': len(tree(hats_p).overlap(tree(hats_c))), 'bodies': len(tree(body_p).overlap(tree(body_c)))}
    STATE['cuff_contacts'] = touches
    log(f'cuff contacts (overlapping face pairs per frame): {touches}')
    # 6. leading the captive on the leash
    scene, coll, cam = P.new_scene('PRV-Lead')
    a, _m = P.clone_character(rig.arm, bean_meshes, coll, 'Bean-Lead')
    R.assign_action(a, STATE['actions']['A_Bean_Lead_Walk'])
    fr, _fm = captive(coll, 'FR-Lead', 'A_BeanFR_Captive_Walk')
    fr.matrix_world = Matrix.Translation((0.0, 1.30, 0.0))
    cuffs = cuffs_on(coll, fr, 'PRV-Cuffs-Lead')
    bpy.context.window.scene = scene
    scene.frame_set(0)
    place_cuffs(cuffs, fr)
    bpy.context.view_layer.update()
    hand = a.matrix_world @ a.pose.bones['hand_l'].matrix @ (rig.rest['hand_l'].inverted() @ Matrix.Translation(rig.grip_rest['l']))
    anchor = cuffs.matrix_world @ Vector(STATE['assets']['SM_Steel_Handcuffs']['sockets']['chain_anchor']['blender_m'])

    def link_copy(name, c):
        return prop('SM_Chain_Link', c, name)
    P.chain_along(link_copy, hand.to_translation(), anchor, coll, sag=0.10)
    P.aim(cam, (0.0, 0.62, 0.5), 100, 14, 5.0, lens=38)
    out.append(P.render(scene, PREV / 'lead_walk_leash.png', 0, (1100, 640)))
    return [rel(p) for p in out]


# ------------------------------------------------------------------------------------------------- manifests
def props_manifest(assets, tex):
    cage = assets['SM_Capture_Cage']
    return {
        'format': 'capture-kit/1', 'task': 'T-337 (lot L5), contrat avec T-336', 'status': 'built',
        'built_by': 'scripts/props/capture_kit/build.py', 'built_at': dt.datetime.now().isoformat(timespec='seconds'),
        'blender': bpy.app.version_string,
        'units': {
            'blender': 'mètres, Z haut, avant de l\'objet = +X',
            'unreal': 'cm ; Unreal = (x*100, -y*100, z*100) depuis Blender ; avant = +X dans Unreal ; un lacet Blender de a '
                      'degrés vaut -a dans Unreal',
            'fbx': 'FBX_SCALE_NONE, global_scale 1, apply_unit_scale, axis_forward -Y, axis_up Z (réglages de '
                   'scripts/props/barque.py) : géométrie déjà en cm dans le fichier, importer à l\'échelle 1',
            'sockets': 'empties SOCKET_<nom> dans le FBX -> sockets Unreal ; remettre relative_scale à 1 après l\'import '
                       '(piège x100 des sockets, mémoire ue-mcp-import-pitfalls)',
            'collision': 'UCX_<NomMesh>_NN (boîtes convexes) dans le FBX de la cage, de la porte et de la caisse : importer '
                         'sans collision auto ; menottes et maillon sans UCX (objets attachés, NoCollision ; pour un '
                         'objet au sol, ajouter une boîte : action pickupcollision)'},
        'assets': assets,
        'gameplay_hints': {
            'cage_door': 'SM_Capture_Cage_Door attachée au socket door_hinge de la cage ; fermée = lacet relatif 0 ; '
                         'ouverte vers l\'extérieur (+X) = lacet relatif POSITIF dans Unreal jusqu\'à +100 (Blender -100)',
            'prisoner': 'socket prisoner : pieds du captif au centre du plancher (20 cm), regard vers la porte (+X)',
            'entry': 'socket entry : 45 cm devant la porte, dehors, regard vers l\'intérieur (lacet 180) ; captor : place du '
                     'joueur pour pousser le captif',
            'clear_inside_m': cage['info']['clear_inside_m'], 'door_opening_m': cage['info']['door_opening_m'],
            'handcuffs': 'SM_Steel_Handcuffs se fixe sur l\'os hand_r du captif avec la transformation donnée dans '
                         'art/export/Characters/Capture/manifest.json (attachments.handcuffs_on_captive) : ses deux bracelets '
                         'tombent sur les poignets dans A_BeanFR_Captive_Idle et A_BeanFR_Captive_Walk',
            'chain': 'laisse = SM_Chain_Link répété au pas (info.pitch_m) le long de X, chaque maillon tourné de 90 deg '
                     'autour de X par rapport au précédent ; du socket chain_anchor des menottes à la mitaine gauche du '
                     'joueur (attachments.leash_hand)'},
        'materials': {
            'M_Capture_Wood': {'textures': {k: v for k, v in tex['Wood']['files'].items() if k in ('D', 'N', 'ORM')}, 'source': tex['Wood']['source_url'], 'license': 'CC0 1.0',
                               'look': 'teck / bois de cocotier patiné (rampe brun chaud)'},
            'M_Capture_Iron': {'textures': {k: v for k, v in tex['Iron']['files'].items() if k in ('D', 'N', 'ORM')}, 'source': tex['Iron']['source_url'], 'license': 'CC0 1.0',
                               'look': 'fer forgé noirci, rouille dans les creux (comme les cages du fort)'},
            'M_Capture_Steel': {'textures': {k: v for k, v in tex['Steel']['files'].items() if k in ('D', 'N', 'ORM')}, 'source': tex['Steel']['source_url'], 'license': 'CC0 1.0',
                                'look': 'acier usé, rayé, légère rouille'},
            'convention': 'D sRGB ; N DirectX (vert vers le bas : TC_Normalmap sans flip) ; ORM linéaire (TC_Masks, sRGB off) '
                          'R = AO, G = rugosité, B = métal ; textures en répétition (UV en mètres / tuile : bois 0,6 m, fer '
                          '1 m, acier 0,35 m) : brancher sur M_Character_Baked (BaseColor / ORM / Normal) ou un maître '
                          'équivalent ; les FBX ne portent que les noms des emplacements (aucun chemin de texture : '
                          'créer les MI depuis ce manifeste)',
            'slots': 'les noms d\'emplacements des FBX sont les noms des matériaux (M_Capture_Wood, M_Capture_Iron, '
                     'M_Capture_Steel)'},
        'icons': {'T_Icon_Capture_Cage': 'art/ui/icons/capture/T_Icon_Capture_Cage.png',
                  'T_Icon_Capture_Cage_Folded': 'art/ui/icons/capture/T_Icon_Capture_Cage_Folded.png',
                  'T_Icon_Steel_Handcuffs': 'art/ui/icons/capture/T_Icon_Steel_Handcuffs.png',
                  'T_Icon_Steel_Handcuffs_Open': 'art/ui/icons/capture/T_Icon_Steel_Handcuffs_Open.png',
                  'format': '512 x 512 RGBA, fond transparent, ombre douce (comme art/ui/icons/weapons)',
                  'made_by': 'scripts/ui/render_capture_icons.py (Blender, depuis build/capture_kit/glb) puis '
                             'scripts/ui/finish_capture_icons.py (Pillow)',
                  'suggested': {'cage': 'T_Icon_Capture_Cage', 'menottes': 'T_Icon_Steel_Handcuffs'}},
        'previews': STATE.get('previews', []),
        'runtime_tested': False,
    }


def clips_manifest(rows, attachments):
    return {
        'format': 'capture-clips/1', 'task': 'T-337 (lot L5), contrat avec T-336', 'status': 'built',
        'built_by': 'scripts/props/capture_kit/build.py (clips.py, rig.py)',
        'built_at': dt.datetime.now().isoformat(timespec='seconds'),
        'skeleton': {'blender': 'SKEL_Bean (art/blender/characters/bean_soldier.blend, lu sans être modifié)',
                     'unreal': SKELETON_UE,
                     'note': 'même squelette pour le joueur (BP_IslanderBean) et les soldats français (SK_BeanFR_*)'},
        'export': {
            'fps': C.FPS, 'root_motion': False, 'frame_mapping': 'images Blender 0..N, toutes clés cuites (N+1 clés)',
            'settings': 'réglages des clips de la bibliothèque (anim_export.export_range, '
                        'scripts/character/bean_swim_clips.py) : armature seule renommée Armature, FBX_SCALE_NONE + '
                        'apply_unit_scale, -Y avant / Z haut, os déformants, primary Y / secondary X',
            'root_scale': 'os root à x100 comme tous les A_Bean_* (dépliage du noeud Armature) : pas de PoseRoot requis',
            'arm_scale_keys': 'bras étirés uniformément quand il le faut (qa.max_stretch), la mitaine gardée à sa taille '
                              '(échelle locale inverse), comme les clips de la bibliothèque',
            'unreal_import': 'commandlet (le MCP n\'importe pas un FBX d\'animation seule) : options de '
                             'scripts/character/import_bean_library.py task_for (skeleton = SK_Bean_Body_Skeleton, '
                             'FBXALIT_EXPORTED_TIME, remove_redundant_keys False) ; cible conseillée '
                             '/Game/MonkeIsland/Capture/Animations/'},
        'clips': rows,
        'attachments': attachments,
        'qa_contacts_A_Bean_Cuff': {'frames_checked': sorted(STATE.get('cuff_contacts', {})),
                                   'hats_overlapping_faces_max': max((v['hats'] for v in STATE.get('cuff_contacts', {}).values()), default=None),
                                   'bodies_overlapping_faces_max': max((v['bodies'] for v in STATE.get('cuff_contacts', {}).values()), default=None),
                                   'how': 'joueur (bean, tricorne) et captif (SK_BeanFR_Fusilier_1767.glb) dans la paire cuff_pair, '
                                          'captif dans la pose A_BeanFR_Captive_Idle : faces des chapeaux et des corps qui se '
                                          'croisent (BVH), une image sur trois'},
        'loops': 'les boucles ont leur dernière image égale à la première',
        'veilleurs_tiki': STATE.get('tiki_note'),
        'runtime_tested': False,
    }


def attachments_block(rig, cuffs_info):
    held = STATE['held']
    clip_place = STATE['clips']['A_Bean_Cage_Place']
    clip_cuff = STATE['clips']['A_Bean_Cuff']
    mats_rest = rig.fk({})
    grip_l = R.relative_to_bone(rig, mats_rest, 'hand_l', Matrix.Translation(rig.grip_rest['l']))
    grip_r = R.relative_to_bone(rig, mats_rest, 'hand_r', Matrix.Translation(rig.grip_rest['r']))
    fit_error, rel_row = cuffs_info
    door_gap = round(-(clip_place.cage_spawn.translation.y) * 100 - M.CAGE['hinge_x'] * 100)
    pair_cm = round(C.PAIR['distance'] * 100)
    for row in (grip_l, grip_r):
        row['note'] = 'seule la position compte (centre de la mitaine)'
    return {
        'note': 'transformations relatives à un os : espace os Blender (exact) et conversion Unreal du projet '
                '(location cm (x, -y, z), FRotator(-ey, -ez, ex) comme TorchRules::GripInHand) À VÉRIFIER dans l\'éditeur ; '
                'repère personnage = X avant du bean, Y sa droite, Z haut, origine aux pieds (cm, lacet en degrés)',
        'folded_cage_in_hand_r': dict(R.relative_to_bone(rig, held['mats'], 'hand_r', held['crate_world']),
                                      mesh='SM_Capture_Cage_Folded', socket_at_mitten='grip',
                                      what='origine du maillage (dessous de la caisse) relative à hand_r',
                                      grip_socket=R.relative_to_bone(rig, held['mats'], 'hand_r', held['crate_world'] @
                                                                     Matrix.Translation(STATE['crate_grip_local'])),
                                      use='objet tenu (main droite, sur le côté, comme une valise) et début de A_Bean_Cage_Place ; '
                                          'DA_CaptureRules.FoldedCageInHand = grip_socket.unreal_guess (socket grip sur hand_r)'),
        'open_handcuffs_in_hand_r': dict(R.relative_to_bone(rig, held['mats'], 'hand_r', held['cuffs_world']),
                                         mesh='SM_Steel_Handcuffs_Open', socket_at_mitten='grip',
                                         what='origine du maillage = son socket grip (anneau du milieu) relative à hand_r',
                                         use='objet tenu et début de A_Bean_Cuff (cachées à CuffsClosed) ; '
                                             'DA_CaptureRules.HandcuffsInHand = unreal_guess'),
        'handcuffs_on_captive': dict(rel_row, mesh='SM_Steel_Handcuffs', fit_error_max_m=fit_error,
                                     what='origine du maillage (milieu des bracelets) relative à hand_r du captif ; '
                                          'DA_CaptureRules.CuffsOnBone = unreal_guess, CuffBones = [hand_r]',
                                     two_point_fallback={
                                         'cuff_r': {'bone': 'hand_r', 'unreal_bone_space_cm': [0.0, round(-C.CUFF_ALONG * 100, 2), 0.0]},
                                         'cuff_l': {'bone': 'hand_l', 'unreal_bone_space_cm': [0.0, round(-C.CUFF_ALONG * 100, 2), 0.0]},
                                         'note': 'centre de chaque bracelet = 3,1 cm le long de l’os de la main (vers la '
                                                 'mitaine) : placer le maillage pour que ses sockets cuff_r / cuff_l tombent sur '
                                                 'ces deux points si la rotation ci-dessus se lit mal dans l’éditeur'},
                                     use='attacher à hand_r du captif (KeepRelative avec cette transformation) dès '
                                         'CuffsClosed ; le bracelet gauche suit le poignet gauche dans les deux clips '
                                         'captifs (écart max fit_error_max_m)'),
        'leash_hand': dict(grip_l, use='bout de la laisse dans la mitaine gauche du joueur (A_Bean_Lead_Walk) : '
                                       'centre de la mitaine relatif à hand_l'),
        'mitten_centre_r': grip_r,
        'cage_place': {
            'crate_down': dict(R.character_space(clip_place.crate_down), event='CrateDown',
                               use='la caisse quitte la main et reste au sol à cette place (pivot = dessous)'),
            'cage_spawn': dict(R.character_space(clip_place.cage_spawn), event='CagePlaced',
                               use=f'SM_Capture_Cage (centre de la base) : porte face au joueur, son plan à ~{door_gap} cm '
                                   'de lui ; la caisse disparaît'),
        },
        'cuff_pair': dict(R.character_space(clip_cuff.pair['captive_root'], forward=(0.0, -1.0, 0.0)),
                          use=f'racine du captif (pieds) dans le repère du joueur pendant A_Bean_Cuff : {pair_cm} cm devant '
                              'lui, face à lui (lacet 180) ; le captif tient déjà la pose A_BeanFR_Captive_Idle (les mains du '
                              'joueur visent ses poignets dans cette pose, chapeaux et corps sans contact, mesuré) ; '
                              'menottes visibles à CuffsClosed'),
    }


# ------------------------------------------------------------------------------------------------- main
def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    ap = argparse.ArgumentParser()
    for flag in ('textures', 'force-textures', 'models', 'anims', 'previews', 'save', 'all'):
        ap.add_argument('--' + flag, action='store_true')
    a = ap.parse_args(argv)
    if a.all:
        a.textures = a.models = a.anims = a.previews = a.save = True
    t0 = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = 'SCN-CaptureKit'
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    scene.render.fps = C.FPS
    tex = T.build_all(force=a.force_textures) if (a.textures or a.force_textures) else T.build_all(force=False)
    mats = {name: K.material(name, T.texture_paths) for name in ('M_Capture_Wood', 'M_Capture_Iron', 'M_Capture_Steel')}
    bean_coll, arm, bean_meshes = R.load_bean(scene)
    rig = R.BeanRig(arm, bean_meshes)
    bound = C.BoundHands(rig)
    assets = build_models(scene, mats, bound) if (a.models or a.anims or a.previews) else {}
    STATE['assets'] = assets
    log(f'models: { {k: v["triangles"] for k, v in assets.items()} } in {time.time() - t0:.0f} s')
    rows = {}
    attachments = {}
    if a.anims or a.previews:
        rows = build_clips(scene, rig, bound)
        fit = cuff_fit(rig, bound)
        log(f'cuffs fit on the captive: {fit[0]} m')
        attachments = attachments_block(rig, fit)
    STATE['tiki_note'] = ('non livré : chaque veilleur (Guerrier Coco, Veilleur des Cendres, Gardien Crabe) a son propre '
                          'squelette et ses proportions (scripts/character/tiki_horror.py) et ses clips de jeu viennent de '
                          'packs différents (racine x1 des clips GoreBox) ; un clip captif par mob demande un réglage par '
                          'squelette et une vérification en jeu : pose procédurale des bras côté code (poignets devant, '
                          'mêmes menottes attachées à hand_r)')
    if a.previews:
        STATE['previews'] = previews(rig, bean_meshes, mats)
        log(f'previews: {STATE["previews"]}')
    if a.models or a.anims:
        K.write_json(PROPS_OUT / 'manifest.json', props_manifest(assets, tex))
    if a.anims:
        K.write_json(CLIPS_OUT / 'manifest.json', clips_manifest(rows, attachments))
    if a.save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.context.window.scene = scene
        for s in [s for s in bpy.data.scenes if s.name.startswith('PRV-')]:
            bpy.data.scenes.remove(s)
        for _ in range(3):
            bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
        for action in STATE.get('actions', {}).values():
            action.use_fake_user = True
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        log(f'saved {rel(BLEND)} ({BLEND.stat().st_size // 1024} KB)')
    log(f'finished in {time.time() - t0:.0f} s')


if __name__ == '__main__':
    main()
