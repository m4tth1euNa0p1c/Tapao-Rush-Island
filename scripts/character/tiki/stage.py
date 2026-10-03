"""Presentation scene of the tiki creatures: line-up on the sand with the player bean and a 1.80 m human,
the expedition boat and two coconut palms for scale, labels, cameras, an NLA showcase of every animation
(timeline markers name the segments), and the preview renders.
"""
import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

from . import mats
from .kit import collection

ART = Path(__file__).resolve().parents[3] / "art"
PREV = ART / "_previews"
BEAN_BLEND = ART / "blender" / "characters" / "bean_soldier.blend"
BARQUE_BLEND = ART / "blender" / "props" / "barque.blend"
ATOLL_BLEND = ART / "archives" / "environnements" / "atoll_tuamotu" / "scene" / "atoll_tuamotu.blend"
SAND = ART / "textures" / "ground" / "coast_sand_01"

SPACING = 1.6
BOAT_PROPOSAL = 0.75   # uniform scale proposed for the boat (beans of 1.05 m), not applied to barque.py
SEGMENTS = [("Repos", 120), ("Marche", 72), ("Course", 48), ("Attaque / Tir / Parole", 150), ("Touché", 32),
            ("Mort", 78)]


# ---------------------------------------------------------------- environment

def setup_scene(scene):
    scene.render.fps = 30
    scene.unit_settings.system = "METRIC"
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = eng
            break
        except TypeError:
            continue
    world = bpy.data.worlds.new("W_Tropical")
    world.use_nodes = True
    nt = world.node_tree
    bg = next(n for n in nt.nodes if n.type == "BACKGROUND")
    hdri = ART / "hdri" / "kloofendal_48d_partly_cloudy_puresky_4k.hdr"
    if hdri.exists():
        env = nt.nodes.new("ShaderNodeTexEnvironment")
        env.image = bpy.data.images.load(str(hdri), check_existing=True)
        mp = nt.nodes.new("ShaderNodeMapping")
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp.inputs["Rotation"].default_value = (0.0, 0.0, math.radians(120))
        nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], env.inputs["Vector"])
        nt.links.new(env.outputs["Color"], bg.inputs[0])
        bg.inputs[1].default_value = 0.8
    else:
        bg.inputs[0].default_value = (0.55, 0.72, 0.92, 1)
        bg.inputs[1].default_value = 0.9
    scene.world = world
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy = 4.2
    sun.angle = math.radians(3)
    sun.color = (1.0, 0.96, 0.9)
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = (math.radians(46), math.radians(8), math.radians(-28))
    scene.collection.objects.link(so)
    fill = bpy.data.lights.new("Fill", "SUN")
    fill.energy = 1.0
    fill.color = (0.8, 0.88, 1.0)
    fo = bpy.data.objects.new("Fill", fill)
    fo.rotation_euler = (math.radians(70), 0, math.radians(150))
    scene.collection.objects.link(fo)
    try:
        scene.eevee.taa_render_samples = 48
        scene.eevee.use_shadows = True
    except AttributeError:
        pass
    try:
        scene.view_settings.view_transform = "AgX"
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass


def ground(coll):
    me = bpy.data.meshes.new("PRES_Sand")
    s = 400.0
    me.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("PRES_Sand", me)
    coll.objects.link(ob)
    mat = bpy.data.materials.new("M_PRES_Sand")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    G = mats.Graph(nt)
    out = G.node("ShaderNodeOutputMaterial")
    bsdf = G.node("ShaderNodeBsdfPrincipled")
    G.link(bsdf.outputs[0], out.inputs["Surface"])
    maps = mats.find_maps("ground/coast_sand_01")
    if maps["color"]:
        tc = G.node("ShaderNodeTexCoord").outputs["Object"]
        vec = G.mapping(tc, (0.45, 0.45, 0.45))
        c = G.image(maps["color"], vec)
        warm = G.mix(0.35, c, mats.srgb("E8D8B4"))
        G.link(warm, mats.sock(bsdf.inputs, "Base Color"))
        if maps["rough"]:
            G.link(G.bw(G.image(maps["rough"], vec, "Non-Color")), mats.sock(bsdf.inputs, "Roughness"))
        bump = G.node("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.4
        h = G.bw(G.image(maps["height"], vec, "Non-Color")) if maps["height"] else G.bw(c)
        G.link(h, bump.inputs["Height"])
        G.link(bump.outputs["Normal"], mats.sock(bsdf.inputs, "Normal"))
    else:
        mats.sock(bsdf.inputs, "Base Color").default_value = mats.srgb("E2D2AE")
    me.materials.append(mat)
    return ob


def label(coll, text, loc, size=0.12, hexa="2B2622", name=None):
    cu = bpy.data.curves.new(name or f"LBL_{text[:20]}", "FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    cu.align_y = "BOTTOM"
    cu.extrude = 0.003
    ob = bpy.data.objects.new(name or f"LBL_{text[:20]}", cu)
    ob.location = loc
    ob.rotation_euler = (math.radians(80), 0.0, 0.0)
    coll.objects.link(ob)
    m = bpy.data.materials.get(f"M_PRES_Label_{hexa}")
    if not m:
        m = bpy.data.materials.new(f"M_PRES_Label_{hexa}")
        m.use_nodes = True
        b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
        b.inputs["Base Color"].default_value = mats.srgb(hexa)
        b.inputs["Roughness"].default_value = 0.6
    cu.materials.append(m)
    return ob


def human(coll, loc):
    """1.80 m adult silhouette (cahier §10 scale reference; not exported)."""
    import bmesh
    from .kit import bm_blob, bm_tube, obj_from_bm
    mats.REGISTRY["M_PRES_Human"] = dict(kind="clay", base="9AA3AE", rough=0.6, tone=0.04, bump=0.0)
    bm = bmesh.new()
    x, y = loc[0], loc[1]
    bm_blob(bm, (x, y, 1.66), (0.095, 0.11, 0.125))
    bm_tube(bm, [Vector((x, y, 1.50)), Vector((x, y, 1.40)), Vector((x, y, 1.05)), Vector((x, y, 0.92))],
            [0.06, 0.2, 0.17, 0.17], segs=20, squash=0.65)
    for s in (1, -1):
        bm_tube(bm, [Vector((x + s * 0.2, y, 1.42)), Vector((x + s * 0.25, y, 1.12)), Vector((x + s * 0.27, y, 0.82))],
                [0.055, 0.045, 0.04], segs=12)
        bm_tube(bm, [Vector((x + s * 0.1, y, 0.95)), Vector((x + s * 0.11, y, 0.5)), Vector((x + s * 0.11, y, 0.06))],
                [0.08, 0.06, 0.05], segs=12)
        bm_blob(bm, (x + s * 0.11, y - 0.06, 0.035), (0.05, 0.12, 0.035), cut_below=0.0)
    return obj_from_bm("REF_Humain_1m80", bm, mats.make("M_PRES_Human"), coll, tag="ref", recalc=True)


# ---------------------------------------------------------------- appended references

def append(path, objects=(), collections=()):
    path = Path(path)
    if not path.exists():
        print(f"[tiki] reference missing: {path}")
        return [], []
    with bpy.data.libraries.load(str(path), link=False) as (src, dst):
        dst.objects = [o for o in src.objects if o in objects]
        dst.collections = [c for c in src.collections if c in collections]
    return [o for o in dst.objects if o], [c for c in dst.collections if c]


def references(scene, root):
    """Player bean, boat with beans on board, palms and coconuts for scale. Returns placed objects."""
    out = {}
    c_ref = collection("REF_Echelle", root)
    _, cols = append(BEAN_BLEND, collections=("BeanSoldier",))
    if cols:
        bean = cols[0]
        bean.name = "REF_BeanSoldier"
        for ch in list(bean.children):
            if ch.name.startswith(("Reference", "SM_Bean_FPArm")):
                bean.children.unlink(ch)
        out["bean_coll"] = bean
        holder = bpy.data.collections.new("REF_Bean_Library")
        scene.collection.children.link(holder)
        holder.children.link(bean)
        bpy.context.view_layer.layer_collection.children["REF_Bean_Library"].exclude = True
        inst = instance(c_ref, bean, "REF_Bean_Joueur", (-2 * SPACING + 0.4 - 1.55, 0.0, 0.0))
        out["bean"] = inst
    obs, _ = append(BARQUE_BLEND, objects=("SM_Barque", "REF_Oar_L", "REF_Oar_R"))
    if obs:
        hull = next((o for o in obs if o.name.startswith("SM_Barque")), None)
        boat = bpy.data.objects.new("REF_Barque_Pivot", None)
        c_ref.objects.link(boat)
        boat.location = (0.6, 4.3, 0.37)
        boat.rotation_euler = (0, 0, math.radians(-8))
        for o in obs:
            for c in list(o.users_collection):
                c.objects.unlink(o)
            c_ref.objects.link(o)
            o.parent = boat
        out["boat"] = boat
        if "bean_coll" in out:
            for k, (x, y) in enumerate(((-0.6, -0.42), (0.35, 0.42))):
                b = instance(c_ref, out["bean_coll"], f"REF_Bean_Barque_{k}", (x, y, -0.10))
                b.parent = boat
                b.rotation_euler = (0, 0, math.radians(90 if k else -90))
        # proposal: the same boat at BOAT_PROPOSAL scale, the beans keep their 1.05 m
        k = BOAT_PROPOSAL
        small = bpy.data.objects.new("REF_Barque_Proposition_Pivot", None)
        c_ref.objects.link(small)
        small.location = (-6.4, 5.6, 0.37 * k)
        small.rotation_euler = (0, 0, math.radians(-8))
        small.scale = (k, k, k)
        for o in obs:
            d = o.copy()
            d.name = f"{o.name}_x{k:.2f}"
            c_ref.objects.link(d)
            d.parent = small
        if "bean_coll" in out:
            for i, (x, y) in enumerate(((-0.6, -0.42), (0.35, 0.42))):
                M = (Matrix.Translation(small.location) @ Matrix.Rotation(small.rotation_euler.z, 4, "Z")
                     @ Matrix.Diagonal((k, k, k, 1.0)))
                p = M @ Vector((x, y, -0.10))
                b = instance(c_ref, out["bean_coll"], f"REF_Bean_BarqueProposition_{i}", p)
                b.rotation_euler = (0, 0, math.radians((90 if i else -90) - 8))
        out["boat_small"] = small
        c_lbl = collection("PRES_Etiquettes", root)
        label(c_lbl, "Barque actuelle : 6,4 m × 2,25 m\nplat-bord à 0,60 m au-dessus du plancher", (0.6, 2.55, 0.02),
              size=0.16, name="LBL_Barque_Actuelle")
        label(c_lbl, f"Proposition ×{k:.2f} : {6.41 * k:.1f} m × {2.25 * k:.2f} m\nplat-bord à {0.60 * k:.2f} m"
              .replace(".", ","), (-6.4, 4.1, 0.02), size=0.16, name="LBL_Barque_Proposition")
    obs, _ = append(ATOLL_BLEND, objects=("SM_CoconutPalm_Young_A", "SM_CoconutPalm_Straight_B", "SM_Coconut_Brown",
                                          "SM_Coconut_Green", "SM_Coconut_Yellow"))
    places = {"SM_CoconutPalm_Young_A": ((-12.5, 9.5, 0.0), 20), "SM_CoconutPalm_Straight_B": ((10.5, 12.0, 0.0), 150)}
    for o in obs:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        c_ref.objects.link(o)
        if o.name in places:
            o.location, yaw = places[o.name][0], places[o.name][1]
            o.rotation_euler = (0, 0, math.radians(yaw))
        elif o.name.startswith("SM_Coconut_"):
            o.location = {"SM_Coconut_Brown": (-11.7, 8.4, 0), "SM_Coconut_Green": (-11.1, 8.9, 0),
                          "SM_Coconut_Yellow": (-12.1, 9.1, 0)}[o.name]
        out[o.name] = o
    out["human"] = human(c_ref, (2 * SPACING + 0.4 + 1.6, 0.0))
    return out


def instance(coll, src_coll, name, loc):
    e = bpy.data.objects.new(name, None)
    e.instance_type = "COLLECTION"
    e.instance_collection = src_coll
    e.location = loc
    coll.objects.link(e)
    return e


# ---------------------------------------------------------------- line-up, labels, cameras

def lineup(entries, root):
    """entries: list of (creature, armature) in display order."""
    c_lbl = collection("PRES_Etiquettes", root)
    n = len(entries)
    for i, (cr, arm) in enumerate(entries):
        x = (i - (n - 1) / 2) * SPACING + 0.4
        arm.location = (x, 0.0, 0.0)
        s = cr.s
        label(c_lbl, f"{s['label']}\n{s['sheet']} · {s['H']:.2f} m · {s['weapon_label']}".replace(".", ","),
              (x, -1.45, 0.02), size=0.085, name=f"LBL_{cr.name}")
    return c_lbl


def cameras(scene, root):
    c = collection("PRES_Cameras", root)

    def cam(name, loc, target, lens):
        cd = bpy.data.cameras.new(name)
        cd.lens = lens
        cd.clip_end = 400
        ob = bpy.data.objects.new(name, cd)
        ob.location = loc
        d = Vector(target) - Vector(loc)
        ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        c.objects.link(ob)
        return ob

    cams = {"lineup": cam("CAM_Lineup", (0.3, -12.0, 1.9), (0.3, 0.0, 0.62), 36),
            "echelle": cam("CAM_Echelle", (-1.0, -26.0, 4.2), (-1.0, 6.0, 5.6), 24),
            "barque": cam("CAM_Barque", (-2.6, -5.8, 3.4), (-2.6, 4.4, 0.4), 28)}
    scene.camera = cams["lineup"]
    return cams


# ---------------------------------------------------------------- NLA showcase

def showcase(scene, entries):
    starts = []
    f = 1
    for name, length in SEGMENTS:
        starts.append((name, f, f + length))
        scene.timeline_markers.new(name, frame=f)
        f += length
    scene.frame_start, scene.frame_end = 1, f - 1
    for cr, arm, actions in entries:
        ad = arm.animation_data_create()
        ad.action = None
        for t in list(ad.nla_tracks):
            ad.nla_tracks.remove(t)
        track = ad.nla_tracks.new()
        track.name = "Showcase"
        role = cr.key
        plan = {
            "Repos": ["Idle"],
            "Marche": ["Walk"],
            "Course": ["Run"],
            "Attaque / Tir / Parole": (["Aim", "Shoot", "Reload", "Shoot"] if "Reload" in actions else
                                       ["Cower", "Talk"] if "Talk" in actions else ["Attack", "Idle", "Attack"]),
            "Touché": ["Hit"],
            "Mort": ["Death"],
        }
        last = None
        for seg, s0, s1 in starts:
            cur = s0
            names = plan[seg]
            for k, an in enumerate(names):
                act = actions.get(an)
                if not act:
                    continue
                a0, a1 = act.frame_range
                ln = max(1.0, a1 - a0)
                room = s1 - cur
                if room <= 1:
                    break
                cyclic = bool(act.get("tiki_cyclic"))
                share = room if k == len(names) - 1 else min(room, ln if not cyclic else room / (len(names) - k))
                if not cyclic and ln > room + 0.5:
                    continue
                try:
                    strip = track.strips.new(f"{an}_{seg[:6]}_{k}", math.ceil(cur), act)
                except RuntimeError:
                    print("[tiki] NLA", cr.name, seg, an, cur, act.frame_range[:],
                          [(s.name, s.frame_start, s.frame_end) for s in track.strips])
                    raise
                if hasattr(strip, "action_slot") and act.slots:
                    try:
                        strip.action_slot = act.slots[0]
                    except (TypeError, RuntimeError):
                        pass
                if cyclic:
                    strip.repeat = max(0.01, math.floor(share) / ln)
                strip.extrapolation = "HOLD_FORWARD"
                strip.blend_type = "REPLACE"
                cur = strip.frame_end
                last = strip
            pad_act = actions.get("Idle")
            pad_len = (pad_act.frame_range[1] - pad_act.frame_range[0]) if pad_act else 0
            if pad_act and seg != "Mort" and s1 - math.ceil(cur) >= max(2.0, 0.02 * pad_len):
                act = pad_act
                a0, a1 = act.frame_range
                strip = track.strips.new(f"Idle_pad_{seg[:6]}", math.ceil(cur), act)
                if hasattr(strip, "action_slot") and act.slots:
                    try:
                        strip.action_slot = act.slots[0]
                    except (TypeError, RuntimeError):
                        pass
                strip.repeat = max(0.01, (s1 - math.ceil(cur)) / max(1.0, a1 - a0))
                strip.extrapolation = "HOLD_FORWARD"
        if last:
            last.extrapolation = "HOLD_FORWARD"
        ad.use_nla = True


# ---------------------------------------------------------------- renders

def render(scene, cam, path, res=(1920, 1080), frame=None):
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    if frame is not None:
        scene.frame_set(frame)
    PREV.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(path)
    scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)
    return path


def turnarounds(scene, entries, others, frame=30):
    """Face / profile / back / 3-4 of each creature (orthographic, like the sheets), one PNG per creature."""
    import numpy as np
    cd = bpy.data.cameras.new("CAM_Turn")
    cd.type = "ORTHO"
    cam = bpy.data.objects.new("CAM_Turn", cd)
    scene.collection.objects.link(cam)
    out = []
    W, Hh = 700, 900
    hidden = []
    for ob in others:
        if not ob.hide_render:
            ob.hide_render = True
            hidden.append(ob)
    for cr, arm in entries:
        group = [arm] + [c for c in arm.children]
        for c2, a2 in entries:
            if a2 is not arm:
                for ob in [a2] + list(a2.children):
                    ob.hide_render = True
        for ob in group:
            ob.hide_render = False
        top = cr.H * 1.35
        cd.ortho_scale = max(1.55, top * 1.25)
        tiles = []
        for tag, yaw in (("face", 0), ("profil", 90), ("dos", 180), ("trois_quarts", 35)):
            a = math.radians(yaw)
            c = arm.location + Vector((0, 0, top * 0.46))
            cam.location = c + Vector((6 * math.sin(a), -6 * math.cos(a), 0.4))
            cam.rotation_euler = (Vector(c) - cam.location).to_track_quat("-Z", "Y").to_euler()
            p = PREV / f"tiki_{cr.key}_{tag}.png"
            render(scene, cam, p, (W, Hh), frame)
            img = bpy.data.images.load(str(p))
            px = np.array(img.pixels[:], dtype=np.float32).reshape(Hh, W, 4)
            tiles.append(px)
            bpy.data.images.remove(img)
        sheet = np.concatenate(tiles, axis=1)
        img = bpy.data.images.new(f"turn_{cr.key}", sheet.shape[1], sheet.shape[0], alpha=True)
        img.pixels.foreach_set(sheet.ravel())
        dest = PREV / f"tiki_{cr.key}_turnaround.png"
        img.filepath_raw = str(dest)
        img.file_format = "PNG"
        img.save()
        bpy.data.images.remove(img)
        out.append(dest)
    for c2, a2 in entries:
        for ob in [a2] + list(a2.children):
            ob.hide_render = False
    for ob in hidden:
        ob.hide_render = False
    bpy.data.objects.remove(cam)
    return out


def contact_sheet(scene, cam, frames, dest, res=(1280, 560)):
    import numpy as np
    tiles = []
    for name, fr in frames:
        p = PREV / f"_tmp_{fr}.png"
        render(scene, cam, p, res, fr)
        img = bpy.data.images.load(str(p))
        tiles.append(np.array(img.pixels[:], dtype=np.float32).reshape(res[1], res[0], 4))
        bpy.data.images.remove(img)
        p.unlink()
    cols = 2
    rows = (len(tiles) + cols - 1) // cols
    blank = np.ones_like(tiles[0])
    grid = []
    for r in range(rows):
        row = [tiles[r * cols + c] if r * cols + c < len(tiles) else blank for c in range(cols)]
        grid.append(np.concatenate(row, axis=1))
    sheet = np.concatenate(grid[::-1], axis=0)
    img = bpy.data.images.new("contact", sheet.shape[1], sheet.shape[0], alpha=True)
    img.pixels.foreach_set(sheet.ravel())
    img.filepath_raw = str(dest)
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)
    return dest
