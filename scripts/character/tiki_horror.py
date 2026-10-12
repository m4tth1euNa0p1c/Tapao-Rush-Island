"""Veilleurs « Outlast » (T-274, lot L5) : refonte visuelle du Gardien Crabe, du Veilleur des Cendres et du Guerrier
Coco — modèles réalistes et inquiétants, textures PBR cuites, même squelette que les assets du jeu (réimport en place).

Blender 5.2 en arrière-plan uniquement, un mob par processus (RAM réservée à Unreal) :
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
      --python scripts/character/tiki_horror.py -- --only crab [--quick] [--no-bake] [--no-render] [--no-save]
  ... --python scripts/character/tiki_horror.py -- --lineup        (après les trois : planche avec le bean et l'humain)
Options :
  --only crab,ash,coco   mobs à construire (défaut : les trois, l'un après l'autre dans le même processus)
  --quick                construction + skinning seulement (essais de forme), rien d'écrit
  --no-bake              réutilise les textures déjà cuites (Horror/Textures)
  --no-render            pas de rendus de contrôle
  --no-save              n'écrit pas art/blender/characters/tiki_horror_<mob>.blend
Sorties : art/export/Characters/<Mob>/Horror/ (FBX, textures, horror_manifest.json),
art/_previews/horror_<clé>_{face,trois_quarts,nuit}.png, build/tiki_horror/<Mob>.json.
Réimport Unreal : scripts/character/tiki_horror_import_ue.py (action « mobsvisual » de la session du lead).
Package : scripts/character/horror/ (kits du 2e PC rapatriés + couches « horreur »).
"""
import argparse
import importlib
import json
import os
import sys
import time

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import bpy  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BLEND_DIR = os.path.join(ROOT, "art", "blender", "characters")
REPORT_DIR = os.path.join(ROOT, "build", "tiki_horror")


def load_modules():
    import horror
    from horror import base
    base.load()
    from horror import build, horror_geo, horror_mats, real_geo, real_gore, real_mats, real_rig, real_ue, render
    for m in (real_mats, real_geo, real_rig, real_gore, real_ue, horror_mats, horror_geo, build, render):
        importlib.reload(m)
    return horror, base, build, render


def reset(name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.name = f"SCN-Horror_{name}"
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 1.0
    sc.render.fps = 30
    return sc


def one(key, a):
    t0 = time.time()
    horror, base, build, render = None, None, None, None
    info_name = {"crab": "CrabGuardian", "ash": "AshWatcher", "coco": "CocoWarrior"}[key]
    reset(info_name)
    horror, base, build, render = load_modules()
    K = base.T["kit"]
    root = K.collection("HORROR")
    R = build.build_mob(key, root, quick=a.quick, bake=not a.no_bake)
    rep = {"mob": info_name, "key": key, "seconds_build": round(time.time() - t0, 1),
           "raw_tris": R.get("raw_tris"), "body_tris": base.T["kit"].tri_count(R["body"]),
           "weapon_tris": base.T["kit"].tri_count(R["weapon"]) if R.get("weapon") else 0}
    if a.quick:
        if a.look:
            look(key, R, render)
        print("[horror] quick", json.dumps(rep))
        return rep
    rep.update(lod_tris=[base.T["kit"].tri_count(o) for o in R["lods"]], manifest=os.path.relpath(R["manifest"], ROOT),
               modules={k: v["triangles"] for k, v in R["mrep"].items()})
    if not a.no_render:
        rep["previews"] = [os.path.relpath(p, ROOT) for p in render.previews(R, key)]
    if not a.no_save:
        os.makedirs(BLEND_DIR, exist_ok=True)
        path = os.path.join(BLEND_DIR, f"tiki_horror_{key}.blend")
        bpy.ops.wm.save_as_mainfile(filepath=path, compress=True)
        rep["blend"] = os.path.relpath(path, ROOT)
    rep["seconds_total"] = round(time.time() - t0, 1)
    os.makedirs(REPORT_DIR, exist_ok=True)
    with open(os.path.join(REPORT_DIR, f"{info_name}.json"), "w", encoding="utf-8") as fh:
        json.dump(rep, fh, indent=1, ensure_ascii=False)
    print("[horror] rapport", json.dumps(rep, ensure_ascii=False))
    return rep


def look(key, R, render):
    """Essai de forme : rendu EEVEE rapide avec les matériaux procéduraux (build/tiki_horror/look_<clé>_*.png)."""
    from mathutils import Vector
    sc = bpy.context.scene
    for o in sc.objects:
        if o.type == "MESH":
            o.hide_render = o not in (R["body"], R.get("weapon"))
    render.pose_menace(R["cr"], R["arm"])
    render.setup(sc, night=False)
    try:
        sc.eevee.taa_render_samples = 24
    except AttributeError:
        pass
    H = R["cr"].H
    tgt = Vector((0, 0, 0.58 * H))
    for name, yaw in (("face", 0), ("34", -38), ("dos", 160)):
        cam = render.camera(sc, "PRV_Cam_Look", tgt, yaw, 2.6 * H, 0.05 * H, 50)
        render.render(sc, cam, os.path.join(REPORT_DIR, f"look_{key}_{name}.png"), (700, 860))


def lineup(a):
    """Planche : les trois Veilleurs cuits, le bean joueur (1,05 m) et l'humain de 1,80 m, de jour et de nuit."""
    from mathutils import Vector
    reset("Lineup")
    horror, base, build, render = load_modules()
    sc = bpy.context.scene
    K = base.T["kit"]
    stage = base.T["stage"]
    root = K.collection("LINEUP")
    xs = {"coco": -0.55, "crab": 0.85, "ash": 2.45}
    for key, x in xs.items():
        path = os.path.join(BLEND_DIR, f"tiki_horror_{key}.blend")
        name = build.MOBS[key]["name"]
        if not os.path.exists(path):
            print("[horror] manquant", path)
            continue
        want = {f"SKEL_{name}", f"SK_{name}", f"SK_{name}_{build.MOBS[key]['weapon']}"}
        with bpy.data.libraries.load(path, link=False) as (src, dst):
            dst.objects = [o for o in src.objects if o in want]
        for o in dst.objects:
            if o is None:
                continue
            root.objects.link(o)
            if o.type == "ARMATURE":
                o.location = (x, 0.0, 0.0)
    refs = K.collection("REF_Echelle", root)
    stage.human(refs, (4.0, 0.0))
    _, cols = stage.append(stage.BEAN_BLEND, collections=("BeanSoldier",))
    if cols:
        bean = cols[0]
        for ch in list(bean.children):
            if ch.name.startswith(("Reference", "SM_Bean_FPArm")):
                bean.children.unlink(ch)
        holder = bpy.data.collections.new("REF_Bean_Library")
        sc.collection.children.link(holder)
        holder.children.link(bean)
        bpy.context.view_layer.layer_collection.children["REF_Bean_Library"].exclude = True
        stage.instance(refs, bean, "REF_Bean_Joueur", (-2.0, 0.0, 0.0))
    tgt = Vector((1.0, 0.0, 0.75))
    for night, suffix in ((False, ""), (True, "_nuit")):
        render.setup(sc, night)
        cam = render.camera(sc, "PRV_Cam_Lineup", tgt, -8, 7.4, 0.35, 40)
        render.render(sc, cam, os.path.join(render.PREV, f"horror_lineup{suffix}.png"), (2400, 1100))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--no-bake", action="store_true")
    ap.add_argument("--no-render", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    ap.add_argument("--lineup", action="store_true")
    ap.add_argument("--look", action="store_true")
    a = ap.parse_args(argv)
    if a.lineup:
        lineup(a)
        return
    keys = [k for k in ("crab", "ash", "coco") if not a.only or k in a.only.split(",")]
    for key in keys:
        one(key, a)


if __name__ == "__main__":
    main()
