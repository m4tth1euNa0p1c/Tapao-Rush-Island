"""Rendus de contrôle des Veilleurs « Outlast » (T-274), avec les matériaux CUITS (ce que reçoit Unreal) :
  art/_previews/horror_<mob>_face.png          de face, jour (HDRI de plage, soleil)
  art/_previews/horror_<mob>_trois_quarts.png  3/4 avant gauche, jour
  art/_previews/horror_<mob>_nuit.png          3/4, nuit : lune froide en contre-jour, torche chaude près du joueur
  art/_previews/horror_lineup.png / horror_lineup_nuit.png   les trois avec le bean (1,05 m) et l'humain de 1,80 m
Pose : la garde de chaque créature (CFG tiki), poussée vers une posture inquiétante (accroupi, buste en avant, tête
basse) : pour l'image seulement, les FBX sont exportés au repos avant.
"""
import math
import os

import bpy
from mathutils import Vector

from .base import ART, T, log

PREV = os.path.join(ART, "_previews")
HDRI = os.path.join(ART, "hdri", "secluded_beach_4k.hdr")
MENACE = {
    "coco": dict(crouch=14, spine=(10, 0, 4), chest=(6, 0, 6), head=(-12, 4, -8)),
    "crab": dict(crouch=16, spine=(8, -4, 0), chest=(4, 0, -6), head=(-10, -6, 6)),
    "ash": dict(crouch=12, spine=(14, 0, 0), chest=(6, 0, -4), head=(-16, 0, 6)),
}


def engine(scene):
    for e in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = e
            return e
        except TypeError:
            continue
    return scene.render.engine


def pose_menace(cr, arm):
    """Garde tiki + posture inquiétante (pose des os, sans action)."""
    try:
        an = T["anims"].Animator(cr, arm)
        sp = an.base()
        extra = MENACE.get(cr.key, {})
        sp["crouch"] = sp.get("crouch", 0) + extra.get("crouch", 0)
        for k in ("spine", "chest", "head"):
            if k in extra:
                base = list(sp.get(k, (0, 0, 0))) + [0, 0, 0]
                sp[k] = tuple(base[i] + extra[k][i] for i in range(3))
        p = an.build(sp)
        loc = an.rig.local(p)
        for n, (ql, tl, sc) in loc.items():
            pb = arm.pose.bones[n]
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = ql
            if n in ("root", "pelvis"):
                pb.location = tl
        bpy.context.view_layer.update()
        return True
    except Exception as ex:  # noqa: BLE001
        log("pose de rendu impossible, repos :", ex)
        return False


def clear_lights(scene):
    for o in [o for o in scene.objects if o.type == "LIGHT" or o.name.startswith("PRV_")]:
        bpy.data.objects.remove(o, do_unlink=True)


def world(scene, night):
    w = bpy.data.worlds.get("W_HorrorPreview") or bpy.data.worlds.new("W_HorrorPreview")
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputWorld")
    bg = nt.nodes.new("ShaderNodeBackground")
    nt.links.new(bg.outputs[0], out.inputs["Surface"])
    if not night and os.path.exists(HDRI):
        env = nt.nodes.new("ShaderNodeTexEnvironment")
        env.image = bpy.data.images.load(HDRI, check_existing=True)
        mp = nt.nodes.new("ShaderNodeMapping")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp.inputs["Rotation"].default_value = (0.0, 0.0, math.radians(40))
        nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], env.inputs["Vector"])
        nt.links.new(env.outputs["Color"], bg.inputs[0])
        bg.inputs[1].default_value = 0.7
    elif night:
        bg.inputs[0].default_value = (0.006, 0.008, 0.016, 1.0)
        bg.inputs[1].default_value = 1.0
    else:
        bg.inputs[0].default_value = (0.5, 0.55, 0.6, 1.0)
        bg.inputs[1].default_value = 0.8
    scene.world = w


def light(scene, name, kind, energy, color, loc, target, size=0.5):
    ld = bpy.data.lights.new(name, kind)
    ld.energy = energy
    ld.color = color
    if kind == "SUN":
        ld.angle = math.radians(size)
    else:
        ld.shadow_soft_size = size
    ob = bpy.data.objects.new(name, ld)
    scene.collection.objects.link(ob)
    ob.location = Vector(loc)
    ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    return ob


def ground(scene, night):
    ob = bpy.data.objects.get("PRV_Ground")
    if ob is None:
        me = bpy.data.meshes.new("PRV_Ground")
        s = 14.0
        me.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
        ob = bpy.data.objects.new("PRV_Ground", me)
        scene.collection.objects.link(ob)
        m = bpy.data.materials.new("PRV_Ground")
        m.use_nodes = True
        me.materials.append(m)
    m = ob.data.materials[0]
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = (0.05, 0.045, 0.04, 1) if night else (0.42, 0.37, 0.30, 1)
    b.inputs["Roughness"].default_value = 0.35 if night else 0.9
    return ob


def setup(scene, night):
    engine(scene)
    try:
        scene.eevee.taa_render_samples = 96
    except AttributeError:
        pass
    for attr, val in (("use_raytracing", True), ("use_shadows", True)):
        try:
            setattr(scene.eevee, attr, val)
        except (AttributeError, TypeError):
            pass
    vs = scene.view_settings
    try:
        vs.view_transform = "AgX"
    except TypeError:
        pass
    for look in ("AgX - Medium High Contrast", "Medium High Contrast", "AgX - Base Contrast", "None"):
        try:
            vs.look = look
            break
        except TypeError:
            continue
    vs.exposure = -0.2 if night else 0.0
    clear_lights(scene)
    world(scene, night)
    ground(scene, night)
    if night:
        light(scene, "PRV_Moon", "SUN", 0.35, (0.55, 0.66, 1.0), (3.0, 4.0, 5.0), (0, 0, 0.5), 1.5)
        light(scene, "PRV_Torch", "POINT", 60.0, (1.0, 0.52, 0.22), (-0.9, -1.6, 1.1), (0, 0, 0.6), 0.08)
        light(scene, "PRV_Bounce", "POINT", 6.0, (1.0, 0.45, 0.2), (0.2, -0.6, 0.15), (0, 0, 0.5), 0.3)
    else:
        light(scene, "PRV_Sun", "SUN", 3.2, (1.0, 0.95, 0.88), (-3.0, -4.0, 6.0), (0, 0, 0), 2.0)


def camera(scene, name, target, yaw_deg, dist, height, lens=50):
    cd = bpy.data.cameras.get(name) or bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_end = 100
    cam = bpy.data.objects.get(name) or bpy.data.objects.new(name, cd)
    if cam.name not in scene.collection.objects:
        scene.collection.objects.link(cam)
    yaw = math.radians(yaw_deg)
    loc = Vector(target) + Vector((math.sin(yaw) * dist, -math.cos(yaw) * dist, 0)) + Vector((0, 0, height))
    cam.location = loc
    cam.rotation_euler = (Vector(target) - loc).to_track_quat("-Z", "Y").to_euler()
    return cam


def render(scene, cam, path, res=(1200, 1500)):
    r = scene.render
    r.image_settings.file_format = "PNG"
    r.resolution_x, r.resolution_y = res
    r.resolution_percentage = 100
    r.film_transparent = False
    scene.camera = cam
    r.filepath = path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.render.render(write_still=True, scene=scene.name)
    log("rendu", path)
    return path


def show_only(R):
    """Seules les copies d'export cuites (LOD0 + arme) sont rendues."""
    keep = {R["body_x"], R["weapon_x"]}
    for o in bpy.context.scene.objects:
        if o.type == "MESH" and not o.name.startswith("PRV_") and not o.name.startswith("REF_"):
            o.hide_render = o not in keep


def previews(R, key):
    scene = bpy.context.scene
    show_only(R)
    pose_menace(R["cr"], R["arm"])
    H = R["cr"].H
    tgt = Vector((0, 0, 0.58 * H))
    files = []
    setup(scene, night=False)
    files.append(render(scene, camera(scene, "PRV_Cam_Face", tgt, 0, 2.7 * H, -0.06 * H, 50),
                        os.path.join(PREV, f"horror_{key}_face.png")))
    files.append(render(scene, camera(scene, "PRV_Cam_34", tgt, -38, 2.6 * H, 0.12 * H, 50),
                        os.path.join(PREV, f"horror_{key}_trois_quarts.png")))
    setup(scene, night=True)
    files.append(render(scene, camera(scene, "PRV_Cam_Night", tgt + Vector((0, 0, -0.05 * H)), -28, 2.3 * H,
                                      -0.02 * H, 45), os.path.join(PREV, f"horror_{key}_nuit.png")))
    return files
