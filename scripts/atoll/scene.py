"""World, sun, render settings, cameras and the asset line-up scene."""
import math

import bpy
import numpy as np
from mathutils import Vector

from . import materials as M

HDRI = M.ART / "hdri" / "kloofendal_48d_partly_cloudy_puresky_4k.hdr"


def setup_render(scene, res=(1920, 1080), samples=64):
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    e = scene.eevee
    e.taa_render_samples = samples
    e.taa_samples = 16
    e.use_raytracing = True
    e.use_shadows = True
    e.use_fast_gi = True
    e.fast_gi_distance = 6.0
    e.shadow_resolution_scale = 1.0
    try:
        e.shadow_pool_size = "1024"
    except TypeError:
        pass
    vs = scene.view_settings
    try:
        vs.view_transform = "AgX"
    except TypeError:
        pass
    for look in ("AgX - Medium High Contrast", "Medium High Contrast"):
        try:
            vs.look = look
            break
        except TypeError:
            continue
    vs.exposure = 0.0


def hdri_sun_azimuth(image):
    """Azimuth/elevation (radians, Blender world frame) of the brightest HDRI pixel."""
    w, h = image.size
    px = np.empty(w * h * 4, np.float32)
    image.pixels.foreach_get(px)
    lum = px.reshape(h, w, 4)[..., :3].sum(-1)
    r, c = np.unravel_index(np.argmax(lum), lum.shape)
    u, v = (c + 0.5) / w, (r + 0.5) / h
    phi = (u - 0.5) * 2 * math.pi
    return math.atan2(math.sin(phi), -math.cos(phi)), (v - 0.5) * math.pi


def world(scene, sun_az, strength=1.0, clamp=5.0):
    """HDRI sky rotated so its sun matches the sun lamp; clamped so the lamp carries the direct light."""
    w = bpy.data.worlds.get("W_Atoll") or bpy.data.worlds.new("W_Atoll")
    scene.world = w
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    im = bpy.data.images.load(str(HDRI), check_existing=True)
    hdr_az, _ = hdri_sun_azimuth(im)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Rotation"].default_value[2] = hdr_az - sun_az
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = im
    mn = nt.nodes.new("ShaderNodeVectorMath")
    mn.operation = "MINIMUM"
    mn.inputs[1].default_value = (clamp, clamp, clamp)
    bg = nt.nodes.new("ShaderNodeBackground")
    bg.inputs["Strength"].default_value = strength
    out = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], env.inputs["Vector"])
    nt.links.new(env.outputs["Color"], mn.inputs[0])
    nt.links.new(mn.outputs["Vector"], bg.inputs["Color"])
    nt.links.new(bg.outputs["Background"], out.inputs["Surface"])
    if hasattr(w, "sun_threshold"):
        w.sun_threshold = 1000.0
    return w


def sun(scene, elev_deg=52.0, az_deg=215.0, strength=4.2, angle_deg=0.53):
    light = bpy.data.lights.new("Sun", "SUN")
    light.energy = strength
    light.angle = math.radians(angle_deg)
    light.color = (1.0, 0.965, 0.91)
    obj = bpy.data.objects.new("Sun", light)
    scene.collection.objects.link(obj)
    el, az = math.radians(elev_deg), math.radians(az_deg)
    s = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
    obj.rotation_euler = (-s).to_track_quat("-Z", "Y").to_euler()
    return obj, az


def camera(scene, name, loc, target, lens=28.0, coll=None):
    cam = bpy.data.cameras.new(name)
    cam.lens = lens
    cam.clip_start = 0.05
    cam.clip_end = 9000.0
    obj = bpy.data.objects.new(name, cam)
    (coll or scene.collection).objects.link(obj)
    obj.location = loc
    obj.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    return obj


def render(scene, cam, path, res=None, samples=None):
    scene.camera = cam
    if res:
        scene.render.resolution_x, scene.render.resolution_y = res
    if samples:
        scene.eevee.taa_render_samples = samples
    scene.render.filepath = str(path)
    scene.render.image_settings.file_format = "JPEG"
    scene.render.image_settings.quality = 90
    with bpy.context.temp_override(scene=scene):
        bpy.ops.render.render(write_still=True, scene=scene.name)


def text_label(text, loc, size=0.5, coll=None):
    cu = bpy.data.curves.new("LBL_" + text[:20], "FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    cu.materials.append(M.label())
    obj = bpy.data.objects.new("LBL_" + text[:20], cu)
    obj.location = loc
    obj.rotation_euler = (math.radians(70), 0, 0)
    (coll or bpy.context.scene.collection).objects.link(obj)
    return obj


def lineup_scene(rows, sun_az, chunk=None):
    """rows: list of (collection, spacing, y, [(obj, label)...]). Scene 'Lineup' with labels and one
    camera per group of `chunk` assets (so each asset is big enough to judge)."""
    sc = bpy.data.scenes.get("Lineup") or bpy.data.scenes.new("Lineup")
    setup_render(sc, (1920, 1080), 48)
    world(sc, sun_az)
    sun(sc, 50, math.degrees(sun_az))
    labels = bpy.data.collections.new("Lineup_Labels")
    sc.collection.children.link(labels)
    cams = []
    for row in rows:
        coll, spacing, y, items = row[:4]
        per_row = row[4] if len(row) > 4 else None
        if coll.name not in sc.collection.children:
            sc.collection.children.link(coll)
        n = len(items)
        xs = [(i - (n - 1) / 2) * spacing for i in range(n)]
        for (obj, lab), x in zip(items, xs):
            obj.location = (x, y, 0)
            text_label(lab, (x, y - spacing * 0.55, 0.02), max(0.12, spacing * 0.06), labels)
        per = per_row or chunk or n
        for c0 in range(0, n, per):
            grp = items[c0:c0 + per]
            gx = xs[c0:c0 + per]
            cx = (gx[0] + gx[-1]) / 2
            width = (gx[-1] - gx[0]) + spacing
            h = max(max(o.dimensions.z for o, _ in grp), spacing * 0.3)
            dist = max(width * 0.5 / math.tan(math.radians(25)), h * 1.3)
            cams.append((f"{coll.name}_{c0 // per}",
                         camera(sc, f"CAM_Lineup_{coll.name}_{c0 // per}", (cx, y - dist, h * 0.45),
                                (cx, y, h * 0.42), 35.0, labels)))
    from .terrain import plane
    gmb = plane(-400, 400, -400, 400, 0.0)
    me = gmb.build("Lineup_Ground", [M.lineup_ground()])
    me.uv_layers[0].data.foreach_set("uv", np.array([[-400, -400], [400, -400], [400, 400], [-400, 400]],
                                                    np.float32).ravel())
    gm = bpy.data.objects.new("Lineup_Ground", me)
    labels.objects.link(gm)
    return sc, cams
