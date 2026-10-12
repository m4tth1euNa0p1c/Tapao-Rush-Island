"""Export the procedural atoll library to FBX for Unreal (run inside Blender, background).

  blender --background --factory-startup --python scripts/atoll/export_ue.py -- [groupe|SM_Nom ...]

Groups: palms, sprouts, shrubs, groundcover, props, wind (default: everything except palms, already in UE).
Writes art/export/<Category>/<SM_Name>.fbx and art/export/ue_manifest.json (read by the UE import).

Unreal conventions (docs/conventions/asset-naming.md):
  - centimeters (UE reads FBX positions as cm), +Z up, pivot at the base on the ground (z = 0);
    leaning palms lean toward +X;
  - vertex color: RGB = tint / 2 (tints up to 2.0 survive 8-bit), A = dryness
    -> masters M_Foliage_Leaf / M_Vegetation_*: tint = VC.rgb * 2;
  - each material slot is named after the UE material instance that goes in it (MI_*);
    palms keep their historic slot names M_CoconutPalm_* (mapped to MI_CoconutPalm_* in UE);
  - UCX_<Mesh>_NN convex collision only where the player should be blocked
    (trunks, dense bushes, rocks, nuts); grass, ferns, creepers and leaves have none.
"""
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
from atoll import palms, plants, props  # noqa: E402

ART = SCRIPTS.parent / "art"
EXPORT = ART / "export"
UE = "/Game/MonkeIsland/Environment/"

PALM_SLOTS = ["M_CoconutPalm_Bark", "M_CoconutPalm_Leaf", "M_CoconutPalm_LeafBase", "M_CoconutPalm_Nut",
              "M_CoconutPalm_Seedling"]
SPROUT_SLOTS = ["MI_CoconutPalm_Bark", "MI_CoconutPalm_Leaf", "MI_CoconutPalm_LeafBase", "MI_CoconutPalm_Nut",
                "MI_CoconutPalm_Seedling"]


def _palm(key):
    kw = dict(palms.VARIANTS[key])
    kind, seed = kw.pop("kind"), kw.pop("seed")
    return lambda: palms.build(kind, seed, **kw)


def registry():
    """(name, group, export subdir, UE folder, builder, slot names by material index, collision, foliage)."""
    R = []
    for key in ["Straight_A", "Straight_B", "Leaning_A", "Leaning_B", "Sabre_A", "Sabre_B", "SCurve_A", "Fallen_A",
                "Young_A", "Young_B", "Juvenile_A", "Juvenile_B", "DeadSnag_A"]:
        R.append((f"SM_CoconutPalm_{key}", "palms", "Vegetation", UE + "Vegetation/Palms/Meshes", _palm(key),
                  PALM_SLOTS, "trunk", True))
    for key in ["Sprout_A", "Sprout_B"]:
        R.append((f"SM_CoconutPalm_{key}", "sprouts", "Vegetation/Palms", UE + "Vegetation/Palms/Meshes",
                  _palm(key), SPROUT_SLOTS, None, True))
    sh = UE + "Vegetation/Shrubs/Meshes"
    pand = ["MI_Pandanus_Bark", "MI_Pandanus_Leaf", "MI_Pandanus_Fruit"]
    R += [("SM_Pandanus_A", "shrubs", "Vegetation/Shrubs", sh, lambda: plants.pandanus(201), pand, "pandanus", True),
          ("SM_Pandanus_B", "shrubs", "Vegetation/Shrubs", sh, lambda: plants.pandanus(202), pand, "pandanus", True),
          ("SM_Pandanus_Young", "shrubs", "Vegetation/Shrubs", sh, lambda: plants.pandanus(203, young=True), pand,
           "pandanus", True)]
    for i, seed in enumerate([211, 212, 213]):
        R.append((f"SM_Naupaka_{'ABC'[i]}", "shrubs", "Vegetation/Shrubs", sh,
                  (lambda s=seed: plants.shrub("naupaka", s)), ["MI_Shrub_Bark", "MI_Naupaka_Leaf", "MI_Flower"],
                  "bush", True))
    for i, seed in enumerate([221, 222]):
        R.append((f"SM_Tahinu_{'AB'[i]}", "shrubs", "Vegetation/Shrubs", sh,
                  (lambda s=seed: plants.shrub("tahinu", s)), ["MI_Shrub_Bark", "MI_Tahinu_Leaf", "MI_Flower"],
                  "woodbase", True))
    for i, seed in enumerate([231, 232, 233]):
        R.append((f"SM_MikiMiki_{'ABC'[i]}", "shrubs", "Vegetation/Shrubs", sh,
                  (lambda s=seed: plants.shrub("mikimiki", s)), ["MI_Shrub_Bark", "MI_MikiMiki_Twig", "MI_Flower"],
                  "bush", True))
    gc = UE + "Vegetation/GroundCover/Meshes"
    R += [("SM_Fern_A", "groundcover", "Vegetation/GroundCover", gc, lambda: plants.fern(241),
           ["MI_Shrub_Bark", "MI_Fern_Leaf"], None, True),
          ("SM_Fern_B", "groundcover", "Vegetation/GroundCover", gc, lambda: plants.fern(242),
           ["MI_Shrub_Bark", "MI_Fern_Leaf"], None, True),
          ("SM_Grass_Short", "groundcover", "Vegetation/GroundCover", gc,
           lambda: plants.grass(251, (20, 32), (0.10, 0.24), 0.15), ["MI_Shrub_Bark", "MI_Grass_Blade"], None, True),
          ("SM_Grass_Tuft", "groundcover", "Vegetation/GroundCover", gc,
           lambda: plants.grass(252, (28, 44), (0.22, 0.45), 0.35), ["MI_Shrub_Bark", "MI_Grass_Blade"], None, True),
          ("SM_Grass_Dry", "groundcover", "Vegetation/GroundCover", gc,
           lambda: plants.grass(253, (18, 30), (0.15, 0.32), 0.85), ["MI_Shrub_Bark", "MI_Grass_Blade"], None, True),
          ("SM_Ipomoea_A", "groundcover", "Vegetation/GroundCover", gc, lambda: plants.ipomoea(261),
           ["MI_Shrub_Bark", "MI_Ipomoea_Leaf", "MI_Flower"], None, True),
          ("SM_Ipomoea_B", "groundcover", "Vegetation/GroundCover", gc, lambda: plants.ipomoea(262),
           ["MI_Shrub_Bark", "MI_Ipomoea_Leaf", "MI_Flower"], None, True)]
    co = UE + "Props/Coconut/Meshes"
    for nm, seed, m in [("Brown", 301, 0.95), ("Green", 302, 0.1), ("Yellow", 303, 0.5)]:
        R.append((f"SM_Coconut_{nm}", "props", "Props/Coconut", co, (lambda s=seed, mm=m: props.coconut(s, mm)),
                  ["MI_CoconutPalm_Nut"], "all", False))
    for i, (seed, ln) in enumerate([(311, 4.8), (312, 3.6)]):
        R.append((f"SM_CoconutFrond_Dry_{'AB'[i]}", "props", "Props/Coconut", co,
                  (lambda s=seed, L=ln: props.dry_frond(s, L)), ["MI_CoconutPalm_Bark", "MI_CoconutPalm_Leaf"],
                  None, True))
    cr = UE + "Props/Coral/Meshes"
    for i, size in enumerate([0.5, 0.8, 1.1, 1.5, 2.0, 0.35]):
        R.append((f"SM_CoralBlock_{'ABCDEF'[i]}", "props", "Props/Coral", cr,
                  (lambda s=321 + i, z=size: props.coral_block(s, z)), ["MI_Coral_Rock"], "all", False))
    for i in range(8):
        R.append((f"SM_CoralRubble_{'ABCDEFGH'[i]}", "props", "Props/Coral", cr,
                  (lambda s=341 + i, k=("chunk" if i % 2 else "branch"): props.rubble(s, k)), ["MI_Coral_Rubble"],
                  None, False))
    be = UE + "Props/Beach/Meshes"
    for i, seed in enumerate([361, 362]):
        R.append((f"SM_Driftwood_{'AB'[i]}", "props", "Props/Beach", be, (lambda s=seed: props.driftwood(s)),
                  ["MI_Driftwood"], "segments", False))
    R.append(("SM_LambisShell", "props", "Props/Beach", be, "lambis", ["MI_LambisShell"], None, False))
    wl = UE + "Wind/Meshes"
    for key, mat in [("PalmLeaflet", "MI_CoconutPalm_Leaf"), ("Naupaka", "MI_Naupaka_Leaf"), ("Dry", "MI_Tahinu_Leaf")]:
        R.append((f"SM_WindLeaf_{key}", "wind", "Wind", wl, (lambda k=key: plants.wind_leaf(k)),
                  ["MI_Shrub_Bark", mat], None, True))
    return R


# ---------------------------------------------------------------- helpers

def material(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def compact_slots(mb, slots):
    """Keep only the material slots actually used, renumbering the faces."""
    used = sorted({int(m[0]) for _, m in mb.polys})
    remap = {u: i for i, u in enumerate(used)}
    mb.polys = [(idx, np.full(len(m), remap[int(m[0])], np.int32)) for idx, m in mb.polys]
    return [slots[u] for u in used], used


def positions(mb, mat=None):
    co = np.concatenate(mb.co).astype(np.float64)
    if mat is None:
        return co
    ids = np.unique(np.concatenate([idx.ravel() for idx, m in mb.polys if int(m[0]) == mat]))
    return co[ids]


def hull_obj(name, pts, k, rng, max_pts=320):
    import bmesh

    pts = np.unique(np.round(np.asarray(pts, np.float64), 4), axis=0)
    if len(pts) > max_pts:
        pts = pts[rng.choice(len(pts), max_pts, replace=False)]
    bm = bmesh.new()
    for p in pts:
        bm.verts.new(p)
    res = bmesh.ops.convex_hull(bm, input=bm.verts, use_existing_faces=False)
    junk = list({g for g in res["geom_interior"] + res["geom_unused"] if isinstance(g, bmesh.types.BMVert)})
    bmesh.ops.delete(bm, geom=junk, context="VERTS")
    me = bpy.data.meshes.new(f"UCX_{name}_{k:02d}")
    bm.to_mesh(me)
    bm.free()
    return bpy.data.objects.new(me.name, me)


def trunk_hulls(name, C, R, start=0, segment=2.6, sides=8, pad=0.02, zmin=-1e9):
    """Convex prisms (one per trunk segment) around a trunk axis."""
    objs = []
    L = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(C, axis=0), axis=1))])
    cuts = np.unique(np.r_[np.arange(0.0, L[-1], segment), L[-1]])
    for k in range(len(cuts) - 1):
        a, b = cuts[k], cuts[k + 1]
        if b - a < 0.3:
            continue
        pa = np.array([np.interp(a, L, C[:, i]) for i in range(3)])
        pb = np.array([np.interp(b, L, C[:, i]) for i in range(3)])
        if max(pa[2], pb[2]) < zmin:
            continue
        ra, rb = float(np.interp(a, L, R)) + pad, float(np.interp(b, L, R)) + pad
        d = (pb - pa) / np.linalg.norm(pb - pa)
        ref = np.array([0.0, 0.0, 1.0]) if abs(d[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
        e1 = np.cross(d, ref)
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(d, e1)
        ang = np.linspace(0, 2 * math.pi, sides, endpoint=False)
        ring = np.cos(ang)[:, None] * e1 + np.sin(ang)[:, None] * e2
        verts = np.vstack([pa + ring * ra, pb + ring * rb])
        faces = [[i, (i + 1) % sides, sides + (i + 1) % sides, sides + i] for i in range(sides)]
        faces += [list(range(sides))[::-1], list(range(sides, 2 * sides))]
        me = bpy.data.meshes.new(f"UCX_{name}_{start + len(objs):02d}")
        me.from_pydata(verts.tolist(), [], faces)
        me.update()
        objs.append(bpy.data.objects.new(me.name, me))
    return objs


def collisions(name, kind, mb, rng):
    if kind is None:
        return []
    if kind == "trunk":
        out = []
        for C, R in getattr(mb, "trunks", []):
            out += trunk_hulls(name, C, R, len(out))
        return out
    if kind == "pandanus":  # stilt roots + trunk base as one cone, upper trunk as prisms
        bark = positions(mb, 0)
        base = bark[bark[:, 2] < 1.9]
        out = [hull_obj(name, base, 0, rng)]
        for C, R in getattr(mb, "trunks", []):
            out += trunk_hulls(name, C, R, len(out), zmin=1.9)
        return out
    if kind == "bush":  # dense thicket: squat hull of the lower canopy, pulled toward the stem
        p = positions(mb)
        H = p[:, 2].max()
        low = p[p[:, 2] < min(1.3, 0.65 * H)].copy()
        c = low[:, :2].mean(0)
        low[:, :2] = c + (low[:, :2] - c) * 0.72
        return [hull_obj(name, low, 0, rng)]
    if kind == "woodbase":  # small tree: trunk and limb bases only
        bark = positions(mb, 0)
        base = bark[bark[:, 2] < 1.3].copy()
        c = base[:, :2].mean(0)
        base[:, :2] = c + (base[:, :2] - c) * 0.85
        return [hull_obj(name, base, 0, rng)]
    if kind == "all":
        return [hull_obj(name, positions(mb), 0, rng)]
    if kind == "segments":  # long log: three hulls along its length
        p = positions(mb)
        edges = np.quantile(p[:, 0], [0, 1 / 3, 2 / 3, 1])
        return [hull_obj(name, p[(p[:, 0] >= edges[k] - 0.05) & (p[:, 0] <= edges[k + 1] + 0.05)], k, rng)
                for k in range(3)]
    raise ValueError(kind)


def ue_colors(me, neutral=False):
    """'ColUE' = (tint / 2, dryness) as the only color attribute; neutral = 0.5 grey, not dry."""
    n = len(me.vertices)
    if neutral or "Col" not in me.color_attributes:
        c = np.tile(np.array([0.5, 0.5, 0.5, 0.0], np.float32), (n, 1))
    else:
        src = me.color_attributes["Col"]
        c = np.empty(len(src.data) * 4, np.float32)
        src.data.foreach_get("color", c)
        c = c.reshape(-1, 4)
        c[:, :3] = np.clip(c[:, :3] * 0.5, 0, 1)
    for a in list(me.color_attributes):
        me.color_attributes.remove(a)
    dst = me.color_attributes.new("ColUE", "FLOAT_COLOR", "POINT")
    dst.data.foreach_set("color", c.ravel())
    me.color_attributes.active_color = dst
    me.color_attributes.render_color_index = 0


def lambis_object(name, slot):
    path = ART / "models" / "props" / "lambis_shell" / "lambis_shell_1k.gltf"
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    obj = next(o for o in new if o.type == "MESH")
    mw = obj.matrix_world.copy()
    for o in new:
        if o is not obj:
            bpy.data.objects.remove(o, do_unlink=True)
    obj.parent = None
    obj.data.transform(mw)
    obj.matrix_world.identity()
    zmin = min(v.co.z for v in obj.data.vertices)
    for v in obj.data.vertices:
        v.co.z -= zmin + 0.005  # resting on the sand, barely sunk
    obj.name = obj.data.name = name
    obj.data.materials.clear()
    obj.data.materials.append(material(slot))
    ue_colors(obj.data, neutral=True)
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    return obj


def export_fbx(objs, path):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             colors_type="LINEAR", prioritize_active_color=True, add_leaf_bones=False,
                             bake_anim=False, use_tspace=True)


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    groups = {"palms", "sprouts", "shrubs", "groundcover", "props", "wind"}
    want_groups = {a for a in args if a in groups} or (groups - {"palms"} if not args else set())
    want_names = {a for a in args if a not in groups}
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    rng = np.random.default_rng(7)
    manifest_path = EXPORT / "ue_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    for name, group, sub, folder, build, slots, coll, foliage in registry():
        if group not in want_groups and name not in want_names:
            continue
        out_dir = EXPORT / sub
        out_dir.mkdir(parents=True, exist_ok=True)
        if build == "lambis":
            obj = lambis_object(name, slots[0])
            used_slots, hulls = slots, []
        else:
            mb = build()
            used_slots, _ = compact_slots(mb, slots)
            hulls = collisions(name, coll, mb, rng)
            me = mb.build(name, [material(s) for s in used_slots])
            ue_colors(me)
            obj = bpy.data.objects.new(name, me)
        scene.collection.objects.link(obj)
        for h in hulls:
            scene.collection.objects.link(h)
        path = out_dir / f"{name}.fbx"
        export_fbx([obj] + hulls, path)
        dims = [round(float(d), 2) for d in obj.dimensions]
        manifest[name] = {"folder": folder, "fbx": str(path).replace("\\", "/"), "slots": used_slots,
                          "foliage": foliage, "ucx": len(hulls), "group": group, "size_m": dims,
                          "verts": len(obj.data.vertices)}
        print(f"[export] {name}: {len(obj.data.vertices)} verts, slots {used_slots}, {len(hulls)} UCX, "
              f"{dims} m", flush=True)
        for o in [obj] + hulls:
            bpy.data.objects.remove(o, do_unlink=True)
    manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"[export] manifest: {manifest_path} ({len(manifest)} assets)")


main()
