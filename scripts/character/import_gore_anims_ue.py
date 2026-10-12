"""The new animations of the 2nd PC (maquettes mob-coco-realiste and bean-francais-gore, 2026-10-07/08) on the skeletons
of the game (owner 2026-10-08: "charge OneDrive, ajoute et remplace les skins et nouvelles animations"), in Unreal.

Clips imported on the game's own skeletons (the bone names are the same), only those the game does not have yet: the
clips already in the project keep their assets and references.
- Guerrier Coco / Villageois: the 16 GoreBox clips (hits by zone, heavy stagger, musket knockback, deaths by headshot,
  decapitation, bisection and explosion, armless panic, legless crawl, writhing) into Characters/<Mob>/Anims/Gore;
- French beans (skeleton of the player bean): the 27 gore clips (hits by zone and side, hat knocked off, deaths,
  surrender, flight, crawl...) into Characters/BeanFR/Animations/Gore.
The clips with root motion (the maquettes' manifests) get EnableRootMotion and the ref pose root lock.
Runs in the editor: scripts/editor/lead_session_t186.py, action "goreanims" (editor lock held, no game running).
Writes monke_island/Saved/T186/gore-anims.json.
"""
import datetime as dt
import json
import sys
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T186/gore-anims.json"
ART = ROOT / "art/export/Characters"
CHARS = "/Game/MonkeIsland/Characters"
SETS = {
    "CocoWarrior": {"source": ART / "CocoReal/CocoWarrior/Anims", "skeleton": f"{CHARS}/CocoWarrior/Meshes/SK_CocoWarrior_Skeleton",
                    "existing": f"{CHARS}/CocoWarrior/Anims", "dest": f"{CHARS}/CocoWarrior/Anims/Gore"},
    "Villager": {"source": ART / "CocoReal/Villager/Anims", "skeleton": f"{CHARS}/Villager/Meshes/SK_Villager_Skeleton",
                 "existing": f"{CHARS}/Villager/Anims", "dest": f"{CHARS}/Villager/Anims/Gore"},
    "BeanFR": {"source": ART / "BeanFRGore/Anims", "skeleton": None, "existing": f"{CHARS}/BeanFR/Animations",
               "dest": f"{CHARS}/BeanFR/Animations/Gore"},
}
ROOT_MOTION = ("Stagger_Heavy", "Knockback_Musket", "Death_Decapitated", "Legless_Crawl_Loop", "Legless_Crawl_Death")
EAL = ue.EditorAssetLibrary
report = {}


def existing_clips(folder):
    return {path.split(".")[0].rsplit("/", 1)[-1] for path in EAL.list_assets(folder, True, False)}


def clip_task(fbx, skeleton, dest):
    options = ue.FbxImportUI()
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = True
    options.skeleton = skeleton
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_ANIMATION
    data = options.anim_sequence_import_data
    data.set_editor_property("import_translation", ue.Vector(0, 0, 0))
    data.set_editor_property("animation_length", ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property("remove_redundant_keys", False)
    task = ue.AssetImportTask()
    task.filename = str(fbx)
    task.destination_path = dest
    task.destination_name = fbx.stem
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = options
    return task


def do_set(name, spec):
    if spec["skeleton"]:
        skeleton = kitlib.load(spec["skeleton"])
    else:
        mesh = kitlib.load(f"{CHARS}/BeanFR/Meshes/SK_BeanFR_CFM_Fusilier")
        skeleton = mesh.get_editor_property("skeleton") if mesh else None
    assert skeleton, f"{name}: no skeleton"
    have = existing_clips(spec["existing"])
    tasks, skipped = [], []
    for fbx in sorted(spec["source"].glob("A_*.fbx")):
        if fbx.stem in have and not EAL.does_asset_exist(f"{spec['dest']}/{fbx.stem}"):
            skipped.append(fbx.stem)
            continue
        tasks.append(clip_task(fbx, skeleton, spec["dest"]))
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    imported, failed = [], []
    for task in tasks:
        path = f"{spec['dest']}/{task.destination_name}"
        anim = EAL.load_asset(path) if EAL.does_asset_exist(path) else None
        if not isinstance(anim, ue.AnimSequence):
            failed.append(task.destination_name)
            continue
        if anim.get_editor_property("skeleton") != skeleton:
            failed.append(task.destination_name + " (other skeleton)")
            continue
        if task.destination_name.endswith(ROOT_MOTION):
            anim.set_editor_property("enable_root_motion", True)
            anim.set_editor_property("root_motion_root_lock", ue.RootMotionRootLock.REF_POSE)
        kitlib.save(anim)
        imported.append([task.destination_name, round(anim.get_editor_property("sequence_length"), 2)])
    return {"skeleton": skeleton.get_path_name(), "imported": imported, "skipped_existing": skipped, "failed": failed}


def main():
    kitlib.check_lock()
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        for name, spec in SETS.items():
            try:
                report[name] = do_set(name, spec)
            except Exception:  # noqa: BLE001
                report[name] = {"error": traceback.format_exc()[-1200:]}
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")


if __name__ == "__main__":
    main()
