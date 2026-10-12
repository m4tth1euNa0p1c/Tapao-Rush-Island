"""
Rendus de contrôle du crabe géant v2 (T-260) : art/_previews/giant_crab_v2_{trois_quarts,face,pattes,avec_bean}.png
(+ giant_crab_v2_roar.png : pose de menace du clip Roar).

Scène : sable, ciel HDRI de plage tropicale (art/hdri/secluded_beach_4k.hdr, CC0 Poly Haven, s'il est présent),
soleil chaud rasant, contre-jour ; EEVEE. Le bean (art/blender/characters/bean_soldier.blend, 1,05 m) et la silhouette
humaine de référence (REF_Human_180cm, 1,80 m) donnent l'échelle. Rien de cette scène n'est exporté.
"""
import math
import os

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
HDRI = os.path.join(ROOT, "art", "hdri", "secluded_beach_4k.hdr")
BEAN_BLEND = os.path.join(ROOT, "art", "blender", "characters", "bean_soldier.blend")
HDRI_YAW = 300.0

CAMERAS = {
    # nom : (position, cible, focale mm)
    "trois_quarts": ((-6.2, -7.4, 3.3), (-0.2, -0.5, 1.05), 32),
    "face": ((0.5, -5.9, 2.05), (0.0, -1.15, 1.40), 48),
    "pattes": ((6.6, 3.3, 1.7), (2.2, 0.45, 0.85), 34),
    "avec_bean": ((-2.6, -9.8, 1.25), (-0.6, -1.0, 1.0), 30),
    "roar": ((-3.4, -7.4, 0.55), (0.0, -0.8, 1.9), 24),
}


def _coll(name):
    c = bpy.data.collections.get(name)
    if c is None:
        c = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(c)
    return c


def _engine(scene):
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = eng
            return eng
        except TypeError:
            continue
    return scene.render.engine


def setup(scene):
    """Monde, sol, lumières, réglages de rendu. Retourne la collection du décor."""
    deco = _coll("REF-Decor")
    _engine(scene)
    ee = scene.eevee
    for attr, val in (("taa_render_samples", 48), ("use_raytracing", True), ("use_shadows", True),
                      ("use_gtao", True), ("gtao_distance", 0.6)):
        try:
            setattr(ee, attr, val)
        except (AttributeError, TypeError):
            pass
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = False
    try:
        scene.view_settings.view_transform = "AgX"
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass
    # monde : HDRI de plage (sinon ciel uni)
    world = bpy.data.worlds.get("REF-World") or bpy.data.worlds.new("REF-World")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    bg = nt.nodes.new("ShaderNodeBackground")
    out = nt.nodes.new("ShaderNodeOutputWorld")
    if os.path.exists(HDRI):
        env = nt.nodes.new("ShaderNodeTexEnvironment")
        env.image = bpy.data.images.load(HDRI, check_existing=True)
        mp = nt.nodes.new("ShaderNodeMapping")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp.inputs["Rotation"].default_value = (0, 0, math.radians(HDRI_YAW))
        nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], env.inputs["Vector"])
        nt.links.new(env.outputs["Color"], bg.inputs["Color"])
        bg.inputs["Strength"].default_value = 0.65
    else:
        bg.inputs["Color"].default_value = (0.55, 0.65, 0.8, 1)
        bg.inputs["Strength"].default_value = 0.8
    nt.links.new(bg.outputs["Background"], out.inputs["Surface"])
    # sol de sable
    me = bpy.data.meshes.new("REF-Sable")
    s = 30.0
    me.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("REF-Sable", me)
    deco.objects.link(ground)
    m = bpy.data.materials.new("REF-Sable")
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    nt = m.node_tree
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 6.0
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(noise.outputs["Factor"], mix.inputs[0])
    mix.inputs[6].default_value = (0.30, 0.25, 0.17, 1)
    mix.inputs[7].default_value = (0.40, 0.34, 0.24, 1)
    nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.95
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.25
    n2 = nt.nodes.new("ShaderNodeTexNoise")
    n2.inputs["Scale"].default_value = 60.0
    nt.links.new(tc.outputs["Object"], n2.inputs["Vector"])
    nt.links.new(n2.outputs["Factor"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    me.materials.append(m)
    # soleil chaud, rasant de face-gauche ; contre-jour froid
    sun_d = bpy.data.lights.new("REF-Soleil", "SUN")
    sun_d.energy = 3.4
    sun_d.angle = math.radians(1.2)
    sun_d.color = (1.0, 0.93, 0.82)
    sun = bpy.data.objects.new("REF-Soleil", sun_d)
    deco.objects.link(sun)
    sun.rotation_euler = (math.radians(52), 0.0, math.radians(-38))
    rim_d = bpy.data.lights.new("REF-ContreJour", "AREA")
    rim_d.energy = 900.0
    rim_d.size = 5.0
    rim_d.color = (0.75, 0.85, 1.0)
    rim = bpy.data.objects.new("REF-ContreJour", rim_d)
    deco.objects.link(rim)
    rim.location = (3.0, 7.0, 4.5)
    rim.rotation_euler = (Vector((0, 0, 1.2)) - rim.location).to_track_quat("-Z", "Y").to_euler()
    return deco


def add_bean(location, yaw_deg=0.0, with_human=None):
    """Bean soldat (collection BeanSoldier de bean_soldier.blend), sans le bras FP ; silhouette humaine optionnelle."""
    if not os.path.exists(BEAN_BLEND):
        return None
    with bpy.data.libraries.load(BEAN_BLEND, link=False) as (src, dst):
        dst.collections = [c for c in src.collections if c == "BeanSoldier"]
    root = dst.collections[0]
    root.name = "REF-Bean"
    bpy.context.scene.collection.children.link(root)
    arm = next(o for o in root.all_objects if o.type == "ARMATURE")
    for c in root.children_recursive:
        if c.name.startswith("SM_Bean_FPArm"):
            c.hide_render = True
            c.hide_viewport = True
        if c.name.startswith("Reference"):
            c.hide_render = with_human is None
            c.hide_viewport = with_human is None
    arm.location = location
    arm.rotation_euler = (0.0, 0.0, math.radians(yaw_deg))
    if with_human is not None:
        for o in root.all_objects:
            if o.name.startswith("REF_Human"):
                o.parent = None
                o.location = with_human
                o.hide_render = False
                o.hide_viewport = False
    return arm


def camera(name):
    pos, tgt, lens = CAMERAS[name]
    cd = bpy.data.cameras.get("CAM-" + name) or bpy.data.cameras.new("CAM-" + name)
    cd.lens = lens
    cd.clip_start = 0.05
    cam = bpy.data.objects.get("CAM-" + name)
    if cam is None:
        cam = bpy.data.objects.new("CAM-" + name, cd)
        _coll("REF-Cameras").objects.link(cam)
    cam.location = pos
    cam.rotation_euler = (Vector(tgt) - Vector(pos)).to_track_quat("-Z", "Y").to_euler()
    return cam


def render(scene, name, out_dir, frame=None):
    scene.camera = camera(name)
    if frame is not None:
        scene.frame_set(frame)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, f"giant_crab_v2_{name}.png")
    scene.render.filepath = path
    scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)
    return path
