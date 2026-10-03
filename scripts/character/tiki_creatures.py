"""Build the tiki creatures of Crownfall Expeditions from the reference sheets in sprite/:
  mob-1 Guerrier Coco (lance), mob-2 Gardien Crabe (marteau-coquillage), mob-3 Veilleur des Cendres (massue
  d'obsidienne), pnj-1 Villageois Inquiet (panier de fruits), sniper-1 Rôdeur des Ronces (sarbacane)
-> models, held objects, skeletons, provisional animations, presentation scene (with the player bean, the boat and
coconut palms for scale), previews and FBX for Unreal.

Run (background):
  blender --background --factory-startup --python scripts/character/tiki_creatures.py -- [options]
Options:
  --only coco,crab     build only these creatures (villager, coco, crab, ash, bramble)
  --render             previews: art/_previews/tiki_lineup.png, tiki_poses.png, tiki_echelle.png, tiki_barque.png,
                       tiki_<key>_turnaround.png
  --quick              smaller / faster previews
  --export             FBX: art/export/Characters/<Name>/ (+ art/export/ue_manifest.json)
  --no-refs            skip the bean / boat / palm references
  --no-save            do not write art/blender/characters/tiki_creatures.blend
CC0 textures and models first: python scripts/character/fetch_character_assets.py
"""
import argparse
import importlib
import sys
import time
from pathlib import Path

import bpy

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import tiki  # noqa: E402
from tiki import anims, creatures, export, kit, mats, rig, stage  # noqa: E402

for _m in (kit, mats, creatures, rig, anims, stage, export):
    importlib.reload(_m)

ART = HERE.parents[1] / "art"
BLEND = ART / "blender" / "characters" / "tiki_creatures.blend"


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--no-refs", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    a = ap.parse_args(argv)
    t0 = time.time()

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Tiki_Lineup"
    stage.setup_scene(scene)
    root = kit.collection("TIKI")
    keys = [k for k in creatures.ORDER if not a.only or k in a.only.split(",")]
    built = []
    for key in keys:
        t = time.time()
        spec = creatures.SPECS[key]
        coll = kit.collection(spec["cid"], root)
        cr = creatures.Creature(key, coll).build()
        arm = rig.build_armature(cr, coll)
        meshes = rig.skin(cr, arm, coll)
        an = anims.Animator(cr, arm)
        actions = an.make_all()
        tris = {k: kit.tri_count(m) for k, m in meshes.items()}
        print(f"[tiki] {cr.name}: {tris} tris, {len(arm.data.bones)} bones, actions {list(actions)}, "
              f"{getattr(cr, 'leaf_count', 0)} leaves, {time.time() - t:.1f}s")
        built.append((cr, arm, meshes, actions))
        if a.export:
            export.export_creature(cr, arm, meshes, actions)

    stage.lineup([(cr, arm) for cr, arm, _, _ in built], root)
    stage.ground(kit.collection("PRES_Decor", root))
    refs = {} if a.no_refs else stage.references(scene, root)
    cams = stage.cameras(scene, root)
    stage.showcase(scene, [(cr, arm, actions) for cr, arm, _, actions in built])
    scene.frame_set(30)

    if a.render:
        res = (1600, 700) if a.quick else (2400, 1050)
        if a.quick:
            try:
                scene.eevee.taa_render_samples = 16
            except AttributeError:
                pass
        stage.render(scene, cams["lineup"], stage.PREV / "tiki_lineup.png", res, 30)
        others = [o for o in bpy.data.objects if o.name.startswith(("REF_", "SM_Barque", "SM_Coconut", "LBL_"))]
        stage.turnarounds(scene, [(cr, arm) for cr, arm, _, _ in built], others, frame=30)
        marks = [(m.name, m.frame) for m in sorted(scene.timeline_markers, key=lambda m: m.frame)]
        picks = [("Repos", 30), ("Marche", marks[1][1] + 9), ("Course", marks[2][1] + 5),
                 ("Attaque 1", marks[3][1] + 14), ("Attaque 2", marks[3][1] + 18), ("Mort", marks[5][1] + 60)]
        stage.contact_sheet(scene, cams["lineup"], picks, stage.PREV / "tiki_poses.png",
                            (1280, 560) if not a.quick else (960, 420))
        if refs:
            stage.render(scene, cams["echelle"], stage.PREV / "tiki_echelle.png", (1920, 1080), 30)
            stage.render(scene, cams["barque"], stage.PREV / "tiki_barque.png", (1920, 1080), 30)
        scene.camera = cams["lineup"]
        print("[tiki] previews in", stage.PREV)

    if not a.no_save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        print("[tiki] saved", BLEND)
    print(f"[tiki] done in {time.time() - t0:.0f}s")


if __name__ == "__main__":
    main()
