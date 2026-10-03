"""Build the Tuamotu atoll look-dev .blend: procedural vegetation library, motu terrain, scatter.

Run (background):
  blender --background --factory-startup --python scripts/blender_atoll_lookdev.py -- [options]
Options:
  --only palms,plants,props   build only these asset groups (+ line-up), skip the motu scene
  --render lineup,scene        render previews to art/_previews/
  --quick                      low samples / resolution for previews
  --no-save                    do not write the .blend
Scenes in the saved file: "Atoll" (motu), "Lineup" (every asset side by side with labels).
"""
import argparse
import importlib
import math
import sys
import time
from pathlib import Path

import bpy

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
from atoll import geo, materials, palms, plants, props, scatter, scene as sc, terrain  # noqa: E402

for _m in (geo, materials, palms, plants, props, terrain, scatter, sc):
    importlib.reload(_m)
M = materials
ART = M.ART
PREV = ART / "_previews"


def args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--render", default="")
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--cams", default="")
    ap.add_argument("--no-save", action="store_true")
    return ap.parse_args(argv)


def lib(name):
    return bpy.data.collections.get(name) or bpy.data.collections.new(name)


def make(name, mb, mats, coll, smooth=True):
    me = mb.build(name, mats, smooth)
    obj = bpy.data.objects.new(name, me)
    coll.objects.link(obj)
    return obj


_T0 = time.time()


def log(msg):
    print(f"[atoll {time.time() - _T0:6.1f}s] {msg}", flush=True)


def coconut_leaf():
    return M.leaf("coconut_leaflet", dry=(0.52, 0.40, 0.24), translucency=0.25, rough_mul=1.2)


# ---------------------------------------------------------------- asset libraries

def build_palms():
    coll = lib("LIB_Palms")
    mats = [M.palm_bark(), coconut_leaf(), M.palm_fiber(), M.coconut(),
            M.leaf("coconut_eophyll", dry=(0.50, 0.40, 0.25), translucency=0.25)]
    items = []
    for key, kw in palms.VARIANTS.items():
        kw = dict(kw)
        kind = kw.pop("kind")
        seed = kw.pop("seed")
        obj = make(f"SM_CoconutPalm_{key}", palms.build(kind, seed, **kw), mats, coll)
        items.append((obj, palms.LABELS_FR[key]))
        log(f"palm {key}: {len(obj.data.vertices)} verts")
    lod = lib("LIB_PalmsLOD")
    for i in range(3):
        make(f"SM_CoconutPalm_LOD_{'ABC'[i]}", palms.lod_palm(300 + i, 15 + 3 * i), mats, lod)
    return coll, items


def build_plants():
    shrubs = lib("LIB_Shrubs")
    ground = lib("LIB_GroundCover")
    bark = M.shrub_bark()
    fl = M.flower()
    pand = [M.pandanus_bark(), M.leaf("pandanus", dry=(0.66, 0.55, 0.36)), M.fruit()]
    naup = [bark, M.leaf("naupaka", dry=(0.6, 0.5, 0.25), translucency=0.3), fl]
    tahi = [bark, M.leaf("tahinu", dry=(0.55, 0.5, 0.4), translucency=0.15, sheen=0.3), fl]
    miki = [bark, M.leaf_card("mikimiki_twig", dry=(0.5, 0.42, 0.28), translucency=0.15), fl]
    specs = [
        ("SM_Pandanus_A", lambda: plants.pandanus(201), pand, "Pandanus (fara)"),
        ("SM_Pandanus_B", lambda: plants.pandanus(202), pand, "Pandanus ramifié"),
        ("SM_Pandanus_Young", lambda: plants.pandanus(203, young=True), pand, "Jeune pandanus"),
        ("SM_Naupaka_A", lambda: plants.shrub("naupaka", 211), naup, "Naupaka"),
        ("SM_Naupaka_B", lambda: plants.shrub("naupaka", 212), naup, "Naupaka"),
        ("SM_Naupaka_C", lambda: plants.shrub("naupaka", 213), naup, "Naupaka"),
        ("SM_Tahinu_A", lambda: plants.shrub("tahinu", 221), tahi, "Tahinu"),
        ("SM_Tahinu_B", lambda: plants.shrub("tahinu", 222), tahi, "Tahinu"),
        ("SM_MikiMiki_A", lambda: plants.shrub("mikimiki", 231), miki, "Miki-miki"),
        ("SM_MikiMiki_B", lambda: plants.shrub("mikimiki", 232), miki, "Miki-miki"),
        ("SM_MikiMiki_C", lambda: plants.shrub("mikimiki", 233), miki, "Miki-miki"),
    ]
    items_s, items_g = [], []
    for name, fn, mats, label in specs:
        obj = make(name, fn(), mats, shrubs)
        items_s.append((obj, label))
        log(f"{name}: {len(obj.data.vertices)} verts")
    fern_m = [bark, M.leaf("fern_lobe", dry=(0.5, 0.42, 0.25), translucency=0.3)]
    grass_m = [bark, M.leaf("grass", dry=(0.70, 0.62, 0.42), translucency=0.25)]
    ipo_m = [bark, M.leaf("ipomoea", dry=(0.5, 0.45, 0.25), translucency=0.25), fl]
    gspecs = [("SM_Fern_A", plants.fern(241), fern_m, "Fougère"),
              ("SM_Fern_B", plants.fern(242), fern_m, "Fougère"),
              ("SM_Grass_Short", plants.grass(251, (20, 32), (0.10, 0.24), 0.15), grass_m, "Herbe rase"),
              ("SM_Grass_Tuft", plants.grass(252, (28, 44), (0.22, 0.45), 0.35), grass_m, "Touffe"),
              ("SM_Grass_Dry", plants.grass(253, (18, 30), (0.15, 0.32), 0.85), grass_m, "Touffe sèche"),
              ("SM_Ipomoea_A", plants.ipomoea(261), ipo_m, "Ipomée (pōhue)"),
              ("SM_Ipomoea_B", plants.ipomoea(262), ipo_m, "Ipomée")]
    for name, mb, mats, label in gspecs:
        items_g.append((make(name, mb, mats, ground), label))
    return shrubs, items_s, ground, items_g


def build_props():
    coll = lib("LIB_Props")
    items = []
    nut = [M.coconut()]
    leafm = [M.palm_bark(), coconut_leaf()]
    specs = [("SM_Coconut_Brown", props.coconut(301, 0.95), nut, "Coco sec"),
             ("SM_Coconut_Green", props.coconut(302, 0.1), nut, "Coco vert"),
             ("SM_Coconut_Yellow", props.coconut(303, 0.5), nut, "Coco jaune"),
             ("SM_CoconutFrond_Dry_A", props.dry_frond(311, 4.8), leafm, "Palme sèche"),
             ("SM_CoconutFrond_Dry_B", props.dry_frond(312, 3.6), leafm, "Palme sèche")]
    for i, size in enumerate([0.5, 0.8, 1.1, 1.5, 2.0, 0.35]):
        specs.append((f"SM_CoralBlock_{'ABCDEF'[i]}", props.coral_block(321 + i, size), [M.coral_rock()],
                      "Bloc corallien"))
    for i in range(8):
        specs.append((f"SM_CoralRubble_{'ABCDEFGH'[i]}", props.rubble(341 + i, "chunk" if i % 2 else "branch"),
                      [M.rubble()], "Gravats"))
    specs.append(("SM_Driftwood_A", props.driftwood(361), [M.driftwood()], "Bois flotté"))
    specs.append(("SM_Driftwood_B", props.driftwood(362), [M.driftwood()], "Bois flotté"))
    for name, mb, mats, label in specs:
        items.append((make(name, mb, mats, coll, not name.startswith("SM_CoralRubble")), label))
    shell = ART / "models" / "props" / "lambis_shell" / "lambis_shell_1k.gltf"
    if shell.exists():
        obj = props.import_lambis(shell)
        obj.name = "SM_LambisShell"
        coll.objects.link(obj)
        items.append((obj, "Lambis"))
    return coll, items


def main():
    a = args()
    only = set(filter(None, a.only.split(",")))
    renders = set(filter(None, a.render.split(",")))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    PREV.mkdir(parents=True, exist_ok=True)
    rows = []
    built = {}
    if not only or "palms" in only:
        c, items = build_palms()
        rows.append((c, 13.0, 0.0, items))
        built["palms"] = c
        built["palms_lod"] = bpy.data.collections["LIB_PalmsLOD"]
    if not only or "plants" in only:
        cs, its, cg, itg = build_plants()
        rows.append((cs, 7.0, -100.0, its, 3))
        rows.append((cg, 1.8, -140.0, itg, 3))
        built["shrubs"], built["ground"] = cs, cg
    if not only or "props" in only:
        c, items = build_props()
        rows.append((c, 1.4, -160.0, items, 6))
        built["props"] = c
    log("assets built")

    sun_az = math.radians(215.0)
    lineup, cams = sc.lineup_scene(rows, sun_az, chunk=4)
    if a.quick:
        lineup.render.resolution_percentage = 60
        lineup.eevee.taa_render_samples = 16

    atoll_scene = None
    if not only:
        atoll_scene = scatter.build_motu_scene(built, sun_az, quick=a.quick)
        log("motu scene built")

    if "lineup" in renders:
        for name, cam in cams:
            sc.render(lineup, cam, PREV / f"lineup_{name}.jpg")
            log(f"rendered lineup {name}")
    if "scene" in renders and atoll_scene:
        for cam in sorted((o for o in atoll_scene.objects if o.type == "CAMERA"), key=lambda o: o.name):
            if a.cams and not any(k in cam.name for k in a.cams.split(",")):
                continue
            sc.render(atoll_scene, cam, PREV / f"scene_{cam.name}.jpg")
            log(f"rendered {cam.name}")

    if not a.no_save:
        name = "atoll_lookdev.blend" if not only else f"atoll_lib_{'_'.join(sorted(only))}.blend"
        out = ART / "blender" / name
        out.parent.mkdir(parents=True, exist_ok=True)
        if atoll_scene:
            for s in list(bpy.data.scenes):
                if s not in (atoll_scene, lineup):
                    bpy.data.scenes.remove(s)
        bpy.ops.wm.save_as_mainfile(filepath=str(out), compress=True)
        log(f"saved {out}")


main()
