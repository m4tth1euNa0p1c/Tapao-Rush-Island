"""Control renders of the capture kit (T-337): art/_previews/capture/*.png (EEVEE, background Blender, one at a time).

Characters are clones of the loaded SKEL_Bean (player: the red coat bean of bean_soldier.blend; captive: the meshes of
art/export/Characters/BeanFR/Fusilier_1767/SK_BeanFR_Fusilier_1767.glb bound to a clone of the same rig, the French
soldiers share SK_Bean_Body_Skeleton), each playing its baked action.
"""
from __future__ import annotations

import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

REPO = Path(__file__).resolve().parents[3]
OUT = REPO / 'art' / '_previews' / 'capture'
FR_GLB = REPO / 'art' / 'export' / 'Characters' / 'BeanFR' / 'Fusilier_1767' / 'SK_BeanFR_Fusilier_1767.glb'


def set_engine(scene):
    for eng in ('BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT', 'BLENDER_WORKBENCH'):
        try:
            scene.render.engine = eng
            return eng
        except TypeError:
            continue


def new_scene(name, sky=(0.62, 0.74, 0.88), floor=(0.74, 0.66, 0.52)):
    scene = bpy.data.scenes.new(name)
    set_engine(scene)
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.film_transparent = False
    scene.render.fps = 30
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    try:
        scene.eevee.taa_render_samples = 32
    except AttributeError:
        pass
    world = bpy.data.worlds.new('W-' + name)
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs['Color'].default_value = (*sky, 1.0)
    bg.inputs['Strength'].default_value = 0.85
    scene.world = world
    coll = bpy.data.collections.new('COL-Stage-' + name)
    scene.collection.children.link(coll)
    me = bpy.data.meshes.new('REF-Floor-' + name)
    s = 30.0
    me.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new('REF-Floor-' + name, me)
    coll.objects.link(ob)
    m = bpy.data.materials.new('MAT-Floor-' + name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    p.inputs['Base Color'].default_value = (*floor, 1.0)
    p.inputs['Roughness'].default_value = 0.95
    ob.data.materials.append(m)
    sun = bpy.data.lights.new('LGT-Sun-' + name, 'SUN')
    sun.energy = 3.4
    sun.angle = math.radians(4)
    lo = bpy.data.objects.new(sun.name, sun)
    coll.objects.link(lo)
    lo.rotation_euler = (math.radians(48), math.radians(8), math.radians(140))
    fill = bpy.data.lights.new('LGT-Fill-' + name, 'SUN')
    fill.energy = 0.8
    fo = bpy.data.objects.new(fill.name, fill)
    coll.objects.link(fo)
    fo.rotation_euler = (math.radians(70), 0, math.radians(-40))
    cam_d = bpy.data.cameras.new('CAM-' + name)
    cam = bpy.data.objects.new(cam_d.name, cam_d)
    coll.objects.link(cam)
    scene.camera = cam
    return scene, coll, cam


def aim(cam, target, yaw_deg, elev_deg, dist, lens=None, ortho=None):
    """yaw 0 = camera on the -Y side looking toward +Y (in front of a bean facing -Y); 90 = on its left (+X)"""
    yaw, el = math.radians(yaw_deg), math.radians(elev_deg)
    d = Vector((math.sin(yaw) * math.cos(el), -math.cos(yaw) * math.cos(el), math.sin(el)))
    cam.location = Vector(target) + d * dist
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    if ortho:
        cam.data.type = 'ORTHO'
        cam.data.ortho_scale = ortho
    else:
        cam.data.type = 'PERSP'
        cam.data.lens = lens or 50
    cam.data.clip_start = 0.02
    cam.data.clip_end = 200


def render(scene, path, frame=None, res=(720, 720)):
    bpy.context.window.scene = scene
    if frame is not None:
        scene.frame_set(frame)
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = str(path)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True, scene=scene.name)
    return Path(path)


def strip(paths, out_path, columns=None):
    """Images (same size) side by side, or in rows of `columns`, into one PNG; the parts are deleted"""
    images = [bpy.data.images.load(str(p)) for p in paths]
    w, h = images[0].size
    tiles = []
    for img in images:
        buf = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(buf)
        tiles.append(buf.reshape(h, w, 4))
    cols = columns or len(tiles)
    rows = []
    for r in range(0, len(tiles), cols):
        row = tiles[r:r + cols]
        while len(row) < cols:
            row.append(np.ones_like(tiles[0]) * 0.2)
        rows.append(np.concatenate(row, axis=1))
    # Blender images start at the bottom row: the first row of tiles goes on top
    joined = np.concatenate(rows[::-1], axis=0)
    H, W = joined.shape[:2]
    total = bpy.data.images.new('strip-' + Path(out_path).stem, width=W, height=H, alpha=False)
    total.pixels.foreach_set(joined.ravel())
    total.filepath_raw = str(out_path)
    total.file_format = 'PNG'
    total.save()
    for img in images:
        bpy.data.images.remove(img)
    bpy.data.images.remove(total)
    for p in paths:
        Path(p).unlink(missing_ok=True)
    return Path(out_path)


# --------------------------------------------------------------------------------------------- characters
def clone_character(arm, meshes, coll, name):
    """Copy of an armature and its skinned meshes (data shared, modifiers retargeted)"""
    a = arm.copy()
    a.data = arm.data.copy()
    a.name = 'SKEL-' + name
    a.animation_data_clear()
    coll.objects.link(a)
    a.matrix_world = Matrix.Identity(4)
    out = []
    for m in meshes:
        c = m.copy()
        c.name = 'GEO-' + name + '-' + m.name
        coll.objects.link(c)
        c.parent = a
        c.matrix_parent_inverse = Matrix.Identity(4)
        c.matrix_basis = m.matrix_basis.copy()
        for mod in c.modifiers:
            if mod.type == 'ARMATURE':
                mod.object = a
        c.hide_render = False
        out.append(c)
    a.hide_render = True
    return a, out


def french_captive(rig_arm, coll, name='FR'):
    """The meshes of a French soldier (glTF) on a clone of SKEL_Bean; None when the file is missing"""
    if not FR_GLB.exists():
        return None, []
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(FR_GLB))
    new = [o for o in bpy.data.objects if o not in before]
    a = rig_arm.copy()
    a.data = rig_arm.data.copy()
    a.name = 'SKEL-' + name
    a.animation_data_clear()
    coll.objects.link(a)
    a.matrix_world = Matrix.Identity(4)
    meshes = []
    for o in new:
        # the skinned parts only (the glTF also carries a stray 2 m 'Icosphere' without vertex groups)
        if o.type == 'MESH' and len(o.vertex_groups) and o.name.startswith('GEO-'):
            mw = o.matrix_world.copy()
            for c in list(o.users_collection):
                c.objects.unlink(o)
            coll.objects.link(o)
            o.parent = a
            o.matrix_parent_inverse = Matrix.Identity(4)
            o.matrix_world = mw
            mods = [m for m in o.modifiers if m.type == 'ARMATURE']
            if not mods:
                mods = [o.modifiers.new('Armature', 'ARMATURE')]
            for m in mods:
                m.object = a
            meshes.append(o)
    for o in new:
        if o not in meshes:
            bpy.data.objects.remove(o, do_unlink=True)
    a.hide_render = True
    return a, meshes


def place_prop(ob, visible, matrix, parent_world=None):
    ob.matrix_world = (parent_world @ matrix) if parent_world is not None else matrix
    ob.hide_render = not visible
    ob.hide_viewport = not visible


def chain_along(builder_fn, a, b, coll, sag=0.12, name='Leash'):
    """Instances (linked copies) of the chain link from a to b on a hanging curve, alternate 90 deg roll"""
    from models import LINK
    pitch = LINK['inner_l']
    pts = []
    n = 60
    for i in range(n + 1):
        t = i / n
        pts.append(a.lerp(b, t) + Vector((0, 0, -sag * 4 * t * (1 - t))))
    length = sum((pts[i + 1] - pts[i]).length for i in range(n))
    count = max(2, int(length / pitch))
    obs = []
    acc = [0.0]
    for i in range(n):
        acc.append(acc[-1] + (pts[i + 1] - pts[i]).length)
    for k in range(count):
        s = (k + 0.5) * length / count
        i = max(0, min(n - 1, next((j for j in range(n) if acc[j + 1] >= s), n - 1)))
        t = (s - acc[i]) / max(acc[i + 1] - acc[i], 1e-9)
        p = pts[i].lerp(pts[i + 1], t)
        d = (pts[i + 1] - pts[i]).normalized()
        up = Vector((0, 0, 1))
        y = up.cross(d)
        if y.length < 1e-6:
            y = Vector((0, 1, 0))
        y.normalize()
        z = d.cross(y)
        R = Matrix((d, y, z)).transposed().to_4x4()
        R = R @ Matrix.Rotation(math.radians(90 * (k % 2)), 4, 'X')
        R.translation = p
        ob = builder_fn(f'{name}_{k:03d}', coll)
        ob.matrix_world = R
        obs.append(ob)
    return obs
