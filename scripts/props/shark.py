"""Requin à pointes noires du lagon (cahier §8, T-058) : corps + queue séparés pour la nage animée en C++.

Procédural et reproductible, stylisé (silhouette lisible de haut : aileron dorsal hors de l'eau).
Longueur 2,0 m (corps 1,45 m + queue 0,55 m), museau vers +X (= +X dans UE), origine du corps au centre de
la nageoire dorsale, origine de la queue au point d'articulation (pédoncule caudal).
Couleurs : dos gris-bleu, ventre blanc, pointes des nageoires noires (Carcharhinus melanopterus).

  blender --background --factory-startup --python scripts/props/shark.py -- [--export] [--render] [--no-save]
Sorties : art/blender/props/shark.blend, art/export/Creatures/SM_Shark_Body.fbx, SM_Shark_Tail.fbx,
art/_previews/shark.png ; UE : /Game/MonkeIsland/Characters/Shark/Meshes.
"""
import argparse
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ART = Path(__file__).resolve().parents[2] / "art"
BLEND = ART / "blender" / "props" / "shark.blend"
EXPORT = ART / "export" / "Creatures"
PREVIEW = ART / "_previews" / "shark.png"

BODY_LEN = 1.45      # snout to tail joint (m)
TAIL_LEN = 0.55      # tail joint to fin tip (m)
JOINT_X = -0.62      # tail joint, relative to the body origin
COLORS = {"Back": (0.30, 0.36, 0.42, 1.0), "Belly": (0.88, 0.88, 0.86, 1.0), "Tip": (0.03, 0.03, 0.035, 1.0)}


def material(name, rgba):
    mat = bpy.data.materials.new(f"M_Shark_{name}")
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = rgba
    bsdf.inputs["Roughness"].default_value = 0.45
    mat.diffuse_color = rgba
    return mat


PROFILE = [(0.0, 0.085, 0.070), (0.25, 0.160, 0.130), (0.55, 0.205, 0.175), (0.80, 0.165, 0.145),
           (0.93, 0.100, 0.090), (1.0, 0.020, 0.020)]


def radius_profile(t):
    """Body half-height and half-width along the length, t = 0 at the joint, 1 at the snout (smooth keys)."""
    for (t0, h0, w0), (t1, h1, w1) in zip(PROFILE, PROFILE[1:]):
        if t <= t1:
            u = (t - t0) / (t1 - t0)
            u = u * u * (3 - 2 * u)
            return h0 + (h1 - h0) * u, w0 + (w1 - w0) * u
    return PROFILE[-1][1], PROFILE[-1][2]


def loft(bm, xs, profile, segs=16, z_off=None):
    rings = []
    for i, x in enumerate(xs):
        h, w = profile(i / (len(xs) - 1))
        zc = z_off(i / (len(xs) - 1)) if z_off else 0.0
        ring = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            y = math.sin(a) * w
            z = math.cos(a) * h
            z = z * (0.85 if z < 0 else 1.0)  # flatter belly
            ring.append(bm.verts.new((x, y, z + zc)))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(segs):
            bm.faces.new((r0[k], r0[(k + 1) % segs], r1[(k + 1) % segs], r1[k]))
    for ring, flip in ((rings[0], True), (rings[-1], False)):
        c = bm.verts.new(sum((v.co for v in ring), Vector()) / len(ring))
        for k in range(segs):
            f = (ring[k], ring[(k + 1) % segs], c) if not flip else (ring[(k + 1) % segs], ring[k], c)
            bm.faces.new(f)
    return rings


def fin(bm, root_front, root_back, tip, thick=0.012, split=0.62):
    """Fin as a closed wedge, cut at `split` of its height so the tip can take its own colour (black tips)."""
    f, b, t = Vector(root_front), Vector(root_back), Vector(tip)
    mf, mb = f.lerp(t, split), b.lerp(t, split)
    n = (b - f).cross(t - f).normalized() * thick
    for quad in ((f, b, mb, mf), (mf, mb, t)):
        a = [bm.verts.new(p + n) for p in quad]
        c = [bm.verts.new(p - n) for p in quad]
        bm.faces.new(a)
        bm.faces.new(list(reversed(c)))
        for i in range(len(quad)):
            j = (i + 1) % len(quad)
            bm.faces.new((a[i], c[i], c[j], a[j]))


def build_body():
    bm = bmesh.new()
    xs = [JOINT_X + BODY_LEN * i / 23 for i in range(24)]
    loft(bm, xs, radius_profile, z_off=lambda t: -0.02 * max(0.0, t - 0.8) / 0.2)
    # dorsal fin, second dorsal, pectorals (swept back), pelvic
    fin(bm, (0.12, 0, 0.20), (-0.12, 0, 0.17), (-0.16, 0, 0.47))
    fin(bm, (-0.46, 0, 0.12), (-0.55, 0, 0.11), (-0.58, 0, 0.19), 0.008)
    for s in (1, -1):
        fin(bm, (0.30, s * 0.10, -0.10), (0.12, s * 0.12, -0.10), (0.02, s * 0.42, -0.20))
        fin(bm, (-0.36, s * 0.06, -0.10), (-0.45, s * 0.05, -0.09), (-0.50, s * 0.15, -0.16), 0.008)
    # eyes
    for s in (1, -1):
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=0.018,
                                   matrix=__import__("mathutils").Matrix.Translation((0.62, s * 0.085, 0.045)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new("SM_Shark_Body")
    bm.to_mesh(mesh)
    bm.free()
    return bpy.data.objects.new("SM_Shark_Body", mesh)


def build_tail():
    bm = bmesh.new()
    # peduncle from the joint (origin) backward
    xs = [-0.22 * i / 7 for i in range(8)]
    loft(bm, xs, lambda t: (0.085 - 0.04 * t, 0.07 - 0.035 * t), segs=16)
    # heterocercal caudal fin: long upper lobe, short lower lobe
    fin(bm, (-0.16, 0, 0.03), (-0.24, 0, 0.0), (-0.52, 0, 0.36))
    fin(bm, (-0.17, 0, -0.03), (-0.24, 0, 0.0), (-0.40, 0, -0.22))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new("SM_Shark_Tail")
    bm.to_mesh(mesh)
    bm.free()
    return bpy.data.objects.new("SM_Shark_Tail", mesh)


def paint(ob, mats, tip_rules):
    for m in mats.values():
        ob.data.materials.append(m)
    names = list(mats)
    for p in ob.data.polygons:
        c = ob.data.vertices[p.vertices[0]].co * 0
        for vi in p.vertices:
            c += ob.data.vertices[vi].co
        c /= len(p.vertices)
        if any(rule(c) for rule in tip_rules):
            p.material_index = names.index("Tip")
        elif p.normal.z < -0.25 or c.z < -0.06:
            p.material_index = names.index("Belly")
        else:
            p.material_index = names.index("Back")
        p.use_smooth = True


def export(ob, path):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_tspace=True)
    print("[shark] export:", path)


def render(objs):
    scene = bpy.context.scene
    scene.render.engine = scene.render.engine  # keep the default engine
    scene.render.resolution_x, scene.render.resolution_y = 1200, 700
    world = bpy.data.worlds.new("W")
    world.color = (0.55, 0.75, 0.85)
    scene.world = world
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(40), 0, math.radians(30))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.location = (1.2, -2.6, 1.2)
    cam.rotation_euler = (math.radians(68), 0, math.radians(22))
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.filepath = str(PREVIEW)
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True)
    print("[shark] preview:", PREVIEW)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    a = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = {k: material(k, v) for k, v in COLORS.items()}
    body = build_body()
    tail = build_tail()
    for ob in (body, tail):
        bpy.context.scene.collection.objects.link(ob)
    paint(body, mats, [lambda c: c.z > 0.36, lambda c: abs(c.y) > 0.30, lambda c: c.z > 0.165 and c.x < -0.53])
    paint(tail, mats, [lambda c: c.z > 0.24 or c.z < -0.15])
    tail.location = (JOINT_X, 0, 0)  # shown attached; exported with its pivot at the origin

    print(f"[shark] body {body.dimensions.x:.2f} x {body.dimensions.y:.2f} x {body.dimensions.z:.2f} m, "
          f"tail {tail.dimensions.x:.2f} m, {len(body.data.polygons) + len(tail.data.polygons)} faces")
    if a.render:
        render([body, tail])
    if a.export:
        export(body, EXPORT / "SM_Shark_Body.fbx")
        tail.location = (0, 0, 0)
        export(tail, EXPORT / "SM_Shark_Tail.fbx")
        tail.location = (JOINT_X, 0, 0)
    if not a.no_save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        print("[shark] saved", BLEND)


if __name__ == "__main__":
    main()
