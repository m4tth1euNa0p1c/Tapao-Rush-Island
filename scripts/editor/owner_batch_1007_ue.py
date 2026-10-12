"""Assets of the owner requests of 2026-10-07 afternoon, in one editor action ("ownerbatch"):

1. boat sounds: imports art/audio/boat/SW_Boat_*.wav (scripts/props/boat_sounds.py) into /Game/MonkeIsland/Boat/Audio
   (sound class SC_Effects; the wake loops), and names them in DA_BoatRules (OarSplashSound, OarDripSound, WakeSound,
   OarSoundAttenuation = ATT_Footstep when none);
2. armed tiki: DA_TikiRules.Musket gets the musket SM_Bean_Musket, its muzzle (from the mesh bounds) and the report and
   flame of the French soldiers (DA_FortSoldierRules); the grip on hand_r is tuned in PIE (garrison_pie_check);
3. galleon: DA_GalleonRules (the game) takes the meshes of DA_GalleonRules_TestFlat (T-188 / T-197: deck cleared,
   forecastle door and bulkhead solid), after a backup Backups/DA_GalleonRules_PreT188_<date>; the rest of the rules
   (merchant, spawn, mooring...) is kept.

Writes monke_island/Saved/T186/owner-batch.json. Each part is reported; a failing part does not stop the others.
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

OUT = ROOT / "monke_island/Saved/T186/owner-batch.json"
report = {}


def part(name):
    def wrap(fn):
        try:
            report[name] = fn()
        except Exception:  # noqa: BLE001
            report[name] = {"error": traceback.format_exc()[-1500:]}
        return fn
    return wrap


def props_of(obj):
    out = {}
    for p in sorted(n for n in dir(obj) if not n.startswith("_")):
        try:
            v = obj.get_editor_property(p)
        except Exception:  # noqa: BLE001
            continue
        out[p] = v
    return out


def boat_sounds():
    tools = ue.AssetToolsHelpers.get_asset_tools()
    src = ROOT / "art/audio/boat"
    dest = "/Game/MonkeIsland/Boat/Audio"
    tasks = []
    for wav in sorted(src.glob("SW_Boat_*.wav")):
        task = ue.AssetImportTask()
        task.filename = str(wav)
        task.destination_path = dest
        task.destination_name = wav.stem
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    assert tasks, "no boat sound: run scripts/props/boat_sounds.py"
    tools.import_asset_tasks(tasks)
    effects = kitlib.load("/Game/MonkeIsland/Settings/SoundClasses/SC_Effects")
    out = {}
    for task in tasks:
        name = Path(task.filename).stem
        sound = kitlib.load(f"{dest}/{name}")
        assert sound, f"{name} did not import"
        sound.set_editor_property("looping", name == "SW_Boat_Wake")
        if effects:
            sound.set_editor_property("sound_class_object", effects)
        kitlib.save(sound)
        out[name] = round(sound.get_editor_property("duration"), 2)
    rules = kitlib.load("/Game/MonkeIsland/Boat/Data/DA_BoatRules")
    assert rules, "DA_BoatRules not found"
    rules.set_editor_property("oar_splash_sound", kitlib.load(f"{dest}/SW_Boat_OarSplash_01"))
    rules.set_editor_property("oar_drip_sound", kitlib.load(f"{dest}/SW_Boat_OarDrip_01"))
    rules.set_editor_property("wake_sound", kitlib.load(f"{dest}/SW_Boat_Wake"))
    if not rules.get_editor_property("oar_sound_attenuation"):
        rules.set_editor_property("oar_sound_attenuation", kitlib.load("/Game/MonkeIsland/Player/Footsteps/Audio/ATT_Footstep"))
    kitlib.save(rules)
    return {"sounds": out, "rules": rules.get_path_name(), "attenuation": str(rules.get_editor_property("oar_sound_attenuation"))}


def tiki_musket():
    rules = kitlib.load("/Game/MonkeIsland/Characters/Data/DA_TikiRules")
    soldier = kitlib.load("/Game/MonkeIsland/Characters/BeanFR/Data/DA_FortSoldierRules")
    mesh = kitlib.load("/Game/MonkeIsland/Weapons/Musket/Static/SM_Bean_Musket")
    assert rules and soldier and mesh, "DA_TikiRules, DA_FortSoldierRules or SM_Bean_Musket missing"
    box = mesh.get_bounding_box()
    lo, hi = box.min, box.max
    # the barrel runs along the longest axis; the muzzle is its far end, near the top
    size = [hi.x - lo.x, hi.y - lo.y, hi.z - lo.z]
    musket = rules.get_editor_property("musket")
    musket.set_editor_property("mesh", mesh)
    muzzle = ue.Vector(hi.x - 1.0, (lo.y + hi.y) * 0.5, hi.z - 2.0) if size[0] >= max(size[1], size[2]) else ue.Vector((lo.x + hi.x) * 0.5, hi.y - 1.0, hi.z - 2.0)
    musket.set_editor_property("muzzle_offset", muzzle)
    for prop in ("shot_sound", "shot_attenuation", "flash_mesh", "flash_material", "flash_scale", "flash_seconds"):
        musket.set_editor_property(prop, soldier.get_editor_property(prop))
    rules.set_editor_property("musket", musket)
    kitlib.save(rules)
    return {"mesh_bounds": [[round(lo.x, 1), round(lo.y, 1), round(lo.z, 1)], [round(hi.x, 1), round(hi.y, 1), round(hi.z, 1)]],
            "muzzle": [round(muzzle.x, 1), round(muzzle.y, 1), round(muzzle.z, 1)],
            "shot_sound": str(musket.get_editor_property("shot_sound")), "armed_share": musket.get_editor_property("armed_share")}


def galleon():
    main_path = "/Game/MonkeIsland/Galleon/DA_GalleonRules"
    test_path = "/Game/MonkeIsland/Galleon/TestFlat/DA_GalleonRules_TestFlat"
    main = kitlib.load(main_path)
    test = kitlib.load(test_path)
    if not test:
        found = ue.EditorAssetLibrary.list_assets("/Game/MonkeIsland/Galleon", True, False)
        cand = [a for a in found if "DA_GalleonRules_TestFlat" in a and "Backups" not in a]
        assert cand, f"no TestFlat galleon rules among {found}"
        test = kitlib.load(cand[0].split(".")[0])
    assert main and test, "galleon rules missing"
    a, b = props_of(main), props_of(test)
    diffs = {k: [str(a[k])[:160], str(b[k])[:160]] for k in a if k in b and str(a[k]) != str(b[k])}
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M")
    backup = f"/Game/MonkeIsland/Galleon/Backups/DA_GalleonRules_PreT188_{stamp}"
    if not ue.EditorAssetLibrary.does_asset_exist(backup):
        assert ue.EditorAssetLibrary.duplicate_asset(main_path, backup), "backup failed"
    copied = []
    for key in diffs:
        value = b[key]
        # the meshes and their materials of the T-188 / T-197 galleon; the gameplay rules of the game stay
        if isinstance(value, (ue.StaticMesh, ue.MaterialInterface)) or "mesh" in key or "material" in key:
            main.set_editor_property(key, value)
            copied.append(key)
    kitlib.save(main)
    return {"backup": backup, "differences": diffs, "copied": copied}


@part("boat_sounds")
def _boat():
    return boat_sounds()


@part("tiki_musket")
def _tiki():
    return tiki_musket()


@part("galleon")
def _galleon():
    return galleon()


kitlib.check_lock()
OUT.parent.mkdir(parents=True, exist_ok=True)
report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
