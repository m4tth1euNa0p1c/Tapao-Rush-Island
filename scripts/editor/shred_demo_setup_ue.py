"""T-168 (lot L5): the baked shredding demos of the 2nd PC's combat-dismemberment pack (v0.2), in Unreal.

For the four mobs of the project (CocoWarrior, CrabGuardian, AshWatcher, Villager; BrambleStalker has no Blueprint):
- imports the 24 SK_Demo_<Mob>_<Clip>.fbx (Musket_Impact, Musket_Burst, Sabre_LeftToRight, Sabre_RightToLeft,
  Sabre_Down, Sabre_Low) into /Game/MonkeIsland/Characters/<Mob>/ShredDemos: the mesh of the 8 fragments
  (SK_Demo_<Mob>_<Clip>), its own skeleton (root + frag_<Part>, SK_Demo_<Mob>_<Clip>_Skeleton) and the baked detachment
  (A_Demo_<Mob>_<Clip>, 90 frames at 30 fps, contact at frame 12). Legacy FBX importer (the Interchange flag is turned
  off around the import, as for the giant crab v2), no physics asset, no material imported;
- gives the slots the materials of the project, as the dismember modules (scripts/character/import_dismember_parts.py):
  outer surfaces MAT-CD-PBR-<Mob>_<Name> -> M_<Mob>_<Name> (Characters/<Mob>/Meshes), cut faces MAT-CD-PBR-<Mob>-<Organ>
  -> M_<Mob>_Cut_<Organ> (Characters/<Mob>/Dismember/Materials, T-155);
- fills DA_<Mob>_ShredDemoSet (UTikiShredDemoSet) with the five lethal demos (Musket_Burst and the four cuts; the
  Musket_Impact demo does not kill: imported, never played, kept out of the set so it is not preloaded), contact 12/30 s,
  severed fragments frag_<Part> from detached_parts of combat-manifest.json;
- writes the set to ShredDemoSet of the class defaults of BP_Tiki<Mob>;
- reports, per Blueprint, the combat reaction clips it has (a demo replaces a lethal reaction T-150 picked: a mob without
  its sabre clips never shows its sabre demos) and its DroppedWeaponClass.

Request (optional) monke_island/Saved/T168/shred-demo-request.json, read at each run:
  {"mobs": ["AshWatcher"], "clips": ["Sabre_Down"], "steps": ["import", "sets", "blueprints"], "force": false,
   "tangents": "builtin"}
The import of a CocoWarrior, CrabGuardian or Villager demo takes ~4 s (one mob ~25 s). An AshWatcher demo takes ~4 min
with the MikkTSpace tangents (zero-length normals of its rock mesh: 110 s at the import, 110 s at the build): import it
with "tangents": "builtin" (legacy generator) and one clip per call, and wait for the result with a long enough timeout
(run_action_locked.sh shreddemo 1800): a wait that gives up while the editor still imports lets the next request run
inside the import (re-entrant poll) and crashed the editor on 2026-10-10 02:01. An imported demo is kept on a new run
unless "force" (then reimported on its own skeleton). Idempotent. The run stops at the first failed import (no further
import, no set, no Blueprint written): a failed FBX import may leave the FBX SDK unstable for the rest of the session.
Runs in the editor: scripts/editor/lead_session_t186.py, action "shreddemo" (lock held, no game running).
Or as a commandlet, editor closed (the lead's build window; same request file, or MONKE_SHRED_REQUEST=<json file>):
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      "C:/Users/matth/monk-island/monke_island/monke_island.uproject"
      -run=pythonscript -script="C:/Users/matth/monk-island/scripts/editor/shred_demo_setup_ue.py"
      -unattended -nosplash -nullrhi -nosourcecontrol
Report: monke_island/Saved/T168/shred-demo.json.

Dry run outside Unreal (files synced, FBX units, bones, materials and the plan; touches nothing):
  python scripts/editor/shred_demo_setup_ue.py --check
"""
import datetime as dt
import json
import os
import re
import struct
import sys
import time
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACK = Path(r"C:\Users\matth\OneDrive\maquette-blender\maquettes\combat-dismemberment")
CONTENT = ROOT / "monke_island/Content"
OUT = ROOT / "monke_island/Saved/T168"
REQUEST = Path(os.environ.get("MONKE_SHRED_REQUEST", str(OUT / "shred-demo-request.json")))
CHARS = "/Game/MonkeIsland/Characters"
MOBS = ["CocoWarrior", "CrabGuardian", "AshWatcher", "Villager"]
BLUEPRINTS = {
    "CocoWarrior": f"{CHARS}/CocoWarrior/Blueprints/BP_TikiCocoWarrior",
    "CrabGuardian": f"{CHARS}/CrabGuardian/Blueprints/BP_TikiCrabGuardian",
    "AshWatcher": f"{CHARS}/AshWatcher/Blueprints/BP_TikiAshWatcher",
    "Villager": f"{CHARS}/Villager/Blueprints/BP_TikiVillager",
}
CLIPS = ["Musket_Impact", "Musket_Burst", "Sabre_LeftToRight", "Sabre_RightToLeft", "Sabre_Down", "Sabre_Low"]
# clips a demo replaces (TikiShredDemo::IsShredClip): the lethal burst and the four cuts
LETHAL = ["Musket_Burst", "Sabre_LeftToRight", "Sabre_RightToLeft", "Sabre_Down", "Sabre_Low"]
FPS = 30
CONTACT_FRAME = 12
MATERIAL_RE = re.compile(r"MAT-CD-PBR-(?P<mob>[A-Za-z0-9]+)(?P<sep>[_-])(?P<name>[A-Za-z0-9_]+)")
PARTS = ["Head", "Torso", "Pelvis", "Arm_L", "Arm_R", "Leg_L", "Leg_R", "Weapon"]


# ---------------------------------------------------------------- pure helpers (also used by --check)

def fbx_path(mob, clip):
    return PACK / "exports" / mob / f"SK_Demo_{mob}_{clip}.fbx"


def demo_names(mob, clip):
    """(folder, mesh, animation) of a demo in the project"""
    return f"{CHARS}/{mob}/ShredDemos", f"SK_Demo_{mob}_{clip}", f"A_Demo_{mob}_{clip}"


def load_manifest():
    return json.loads((PACK / "combat-manifest.json").read_text(encoding="utf-8-sig"))


def detached_parts(manifest, mob, clip):
    """Fragments that leave the body in a demo (manifest characters[].clips[].detached_parts)"""
    character = next((c for c in manifest.get("characters", []) if c.get("name") == mob), {})
    entry = next((c for c in character.get("clips", []) if c.get("name") == clip), {})
    return list(entry.get("detached_parts", [])), int(entry.get("contact", CONTACT_FRAME)), int(entry.get("fps", FPS))


def fbx_unit(path):
    data = path.read_bytes()
    i = data.find(b"UnitScaleFactor")
    if i < 0:
        return None
    j = data.find(b"D", data.find(b"Number", i))
    return struct.unpack("<d", data[j + 1:j + 9])[0]


def fbx_strings(path, pattern):
    return sorted(set(m.decode("latin1") for m in re.findall(pattern, path.read_bytes())))


def fbx_bones(path):
    return [b for b in fbx_strings(path, rb"(frag_[A-Za-z_]+|root)\x00\x01Model")]


def is_synced(path):
    """OneDrive: the file is complete (size stable, no temporary file next to it)"""
    if not path.is_file() or path.stat().st_size == 0:
        return False
    size = path.stat().st_size
    time.sleep(0.05)
    temporary = [f for f in os.listdir(path.parent) if f.startswith("~") or f.endswith((".tmp", ".partial"))]
    return path.stat().st_size == size and not temporary


def has_asset_file(game_path):
    rel = game_path.replace("/Game/", "", 1) + ".uasset"
    return (CONTENT / rel).is_file()


def material_target(mob, slot, exists):
    """(kind, project material) of an FBX material slot; exists(path) tells whether a project asset is there"""
    match = MATERIAL_RE.fullmatch(slot)
    if not match:
        return "unknown", None
    name = match.group("name")
    if match.group("sep") == "-":
        return "cut", f"{CHARS}/{mob}/Dismember/Materials/M_{mob}_Cut_{name}"
    for path in (f"{CHARS}/{mob}/Meshes/M_{mob}_{name}", f"{CHARS}/{mob}/Dismember/Materials/M_{mob}_{name}",
                 f"{CHARS}/{mob}/Materials/M_{mob}_{name}"):
        if exists(path):
            return "outer", path
    return "missing", None


def check():
    manifest = load_manifest()
    rows, problems = [], []
    for mob in MOBS:
        for clip in CLIPS:
            fbx = fbx_path(mob, clip)
            if not is_synced(fbx):
                problems.append(f"missing or not synced: {fbx}")
                continue
            unit = fbx_unit(fbx)
            bones = fbx_bones(fbx)
            parts, contact, fps = detached_parts(manifest, mob, clip)
            severed = [f"frag_{p}" for p in parts]
            plan = {slot: material_target(mob, slot, has_asset_file) for slot in fbx_strings(fbx, rb"MAT-CD-PBR-[A-Za-z0-9_\-]+")}
            if unit not in (1.0, 100.0):
                problems.append(f"{fbx.name}: unexpected UnitScaleFactor {unit}")
            if any(b not in bones for b in severed):
                problems.append(f"{fbx.name}: severed bones {severed} not all in {bones}")
            for slot, (kind, path) in plan.items():
                if kind in ("missing", "unknown") or (kind == "cut" and not has_asset_file(path)):
                    problems.append(f"{fbx.name}: no project material for {slot} ({kind}, {path})")
            folder, mesh, anim = demo_names(mob, clip)
            rows.append({"mob": mob, "clip": clip, "unit_cm": unit, "bones": bones, "contact_s": round(contact / fps, 3),
                         "severed": severed, "in_set": clip in LETHAL, "materials": {s: p for s, (k, p) in plan.items()},
                         "dest": f"{folder}/{mesh}", "anim": f"{folder}/{anim}", "imported": has_asset_file(f"{folder}/{mesh}")})
    print(json.dumps({"pack": str(PACK), "demos": len(rows), "problems": problems, "rows": rows}, indent=1))
    return 0 if not problems else 1


# ---------------------------------------------------------------- in Unreal

def run_in_unreal():
    import unreal as ue

    eal = ue.EditorAssetLibrary
    tools = ue.AssetToolsHelpers.get_asset_tools()
    request = {}
    if REQUEST.exists():
        try:
            request = json.loads(REQUEST.read_text(encoding="utf-8-sig"))
        except ValueError:
            request = {}
    mobs = [m for m in request.get("mobs", MOBS) if m in MOBS]
    clips = [c for c in request.get("clips", CLIPS) if c in CLIPS]
    steps = request.get("steps", ["import", "sets", "blueprints"])
    force = bool(request.get("force", False))
    builtin_tangents = str(request.get("tangents", "")).lower() == "builtin"
    manifest = load_manifest()
    report = {"started_at": dt.datetime.now(dt.timezone.utc).isoformat(),
              "request": {"mobs": mobs, "clips": clips, "steps": steps, "force": force, "builtin_tangents": builtin_tangents},
              "imported": [], "kept": [], "failed": [], "sets": {}, "blueprints": {}, "warnings": [], "timings_s": {}}

    def warn(message):
        report["warnings"].append(message)
        ue.log_warning(f"[shred_demo] {message}")

    def load(path):
        return eal.load_asset(path) if eal.does_asset_exist(path) else None

    def setp(obj, prop, value):
        try:
            obj.set_editor_property(prop, value)
            return True
        except Exception as error:  # noqa: BLE001 - reported, the rest goes on
            warn(f"{obj.get_name()}.{prop}: {error}")
            return False

    def bone_names(mesh):
        try:
            component = ue.new_object(ue.SkeletalMeshComponent)
            component.set_skeletal_mesh_asset(mesh)
            return [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
        except Exception as error:  # noqa: BLE001
            warn(f"bones of {mesh.get_name()}: {error}")
            return []

    def extent(asset):
        box = asset.get_bounds().box_extent
        return [round(box.x, 1), round(box.y, 1), round(box.z, 1)]

    def import_options():
        options = ue.FbxImportUI()
        options.import_mesh = True
        options.import_as_skeletal = True
        options.import_animations = True
        options.import_materials = False
        options.import_textures = False
        options.create_physics_asset = False
        options.mesh_type_to_import = ue.FBXImportType.FBXIT_SKELETAL_MESH
        data = options.skeletal_mesh_import_data
        for prop, value in (("import_uniform_scale", 1.0), ("convert_scene", True), ("force_front_x_axis", False),
                            ("convert_scene_unit", True), ("update_skeleton_reference_pose", False),
                            ("use_t0_as_ref_pose", False), ("import_meshes_in_bone_hierarchy", True),
                            ("import_morph_targets", False)):
            setp(data, prop, value)
        if builtin_tangents:
            # the legacy generator: MikkTSpace takes minutes on the zero-length normals of the AshWatcher's rock
            method = next((getattr(ue.FBXNormalGenerationMethod, n) for n in dir(ue.FBXNormalGenerationMethod)
                           if n.replace("_", "").lower() == "builtin"), None)
            if method is not None:
                setp(data, "normal_generation_method", method)
        anim = options.anim_sequence_import_data
        for prop, value in (("animation_length", ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME),
                            ("import_bone_tracks", True), ("remove_redundant_keys", False),
                            ("use_default_sample_rate", False), ("custom_sample_rate", FPS),
                            ("convert_scene", True), ("convert_scene_unit", True), ("force_front_x_axis", False),
                            ("import_uniform_scale", 1.0)):
            setp(anim, prop, value)
        return options

    def assign_materials(mob, mesh):
        slots = list(mesh.get_editor_property("materials"))
        kinds = {"outer": 0, "cut": 0, "missing": [], "unknown": []}
        for slot in slots:
            imported = str(slot.get_editor_property("imported_material_slot_name")) or str(slot.get_editor_property("material_slot_name"))
            kind, path = material_target(mob, imported, eal.does_asset_exist)
            material = load(path) if path else None
            if material:
                slot.set_editor_property("material_interface", material)
                kinds[kind] += 1
            else:
                kinds["missing" if kind != "unknown" else "unknown"].append(imported)
        mesh.set_editor_property("materials", slots)
        if kinds["missing"] or kinds["unknown"]:
            warn(f"{mesh.get_name()}: no project material for {kinds['missing'] + kinds['unknown']}")
        return kinds

    def find_anim(folder, name, skeleton):
        anim = load(f"{folder}/{name}")
        if isinstance(anim, ue.AnimSequence):
            return anim
        # an importer that named the clip otherwise: the sequence of the folder on this skeleton
        for path in eal.list_assets(folder, recursive=False, include_folder=False):
            asset = load(path.split(".")[0])
            if isinstance(asset, ue.AnimSequence) and asset.get_editor_property("skeleton") == skeleton:
                return asset
        return None

    def import_demo(mob, clip):
        folder, name, anim_name = demo_names(mob, clip)
        fbx = fbx_path(mob, clip)
        existing = load(f"{folder}/{name}")
        if isinstance(existing, ue.SkeletalMesh) and not force:
            anim = find_anim(folder, anim_name, existing.get_editor_property("skeleton"))
            if anim:
                report["kept"].append(f"{folder}/{name}")
                return existing, anim
        if not is_synced(fbx):
            raise RuntimeError(f"{fbx} missing or not synced")
        options = import_options()
        options.set_editor_property("override_animation_name", anim_name)
        if isinstance(existing, ue.SkeletalMesh):
            # a reimport keeps the demo's own skeleton
            options.skeleton = existing.get_editor_property("skeleton")
        task = ue.AssetImportTask()
        task.filename = str(fbx)
        task.destination_path = folder
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = False
        task.options = options
        tools.import_asset_tasks([task])
        mesh = load(f"{folder}/{name}")
        if not isinstance(mesh, ue.SkeletalMesh):
            raise RuntimeError(f"no skeletal mesh after the import of {fbx.name} (imported: {list(task.imported_object_paths)})")
        skeleton = mesh.get_editor_property("skeleton")
        anim = find_anim(folder, anim_name, skeleton)
        if not anim:
            raise RuntimeError(f"no animation on {skeleton.get_name()} after the import of {fbx.name}")
        kinds = assign_materials(mob, mesh)
        for asset in (mesh, skeleton, anim):
            eal.save_loaded_asset(asset, only_if_is_dirty=False)
        bones = bone_names(mesh)
        body = load(f"{CHARS}/{mob}/Meshes/SK_{mob}")
        entry = {"mesh": mesh.get_path_name().split(".")[0], "skeleton": skeleton.get_path_name().split(".")[0],
                 "anim": anim.get_path_name().split(".")[0], "length_s": round(anim.get_play_length(), 3),
                 "bones": bones, "materials": kinds, "extent_cm": extent(mesh),
                 "body_extent_cm": extent(body) if body else None}
        report["imported"].append(entry)
        ue.log(f"[shred_demo] {entry['mesh']}: {entry['length_s']} s, {len(bones)} bones, extent {entry['extent_cm']} "
               f"(body {entry['body_extent_cm']})")
        return mesh, anim

    def clip_enum(clip):
        wanted = clip.replace("_", "").lower()
        for name in dir(ue.TikiCombatClip):
            if name.replace("_", "").lower() == wanted:
                return getattr(ue.TikiCombatClip, name)
        raise RuntimeError(f"no ETikiCombatClip for {clip}")

    def fill_set(mob, demos):
        folder = f"{CHARS}/{mob}/ShredDemos"
        name = f"DA_{mob}_ShredDemoSet"
        asset = load(f"{folder}/{name}")
        if asset is None:
            factory = ue.DataAssetFactory()
            factory.set_editor_property("data_asset_class", ue.TikiShredDemoSet)
            asset = tools.create_asset(name, folder, ue.TikiShredDemoSet, factory)
        if not isinstance(asset, ue.TikiShredDemoSet):
            raise RuntimeError(f"{folder}/{name} is not a TikiShredDemoSet")
        entries, summary = [], []
        for clip in LETHAL:
            if clip not in demos:
                warn(f"{mob} {clip}: demo not imported, left out of the set")
                continue
            mesh, anim = demos[clip]
            parts, contact, fps = detached_parts(manifest, mob, clip)
            bones = bone_names(mesh)
            severed = [f"frag_{p}" for p in parts if not bones or f"frag_{p}" in bones]
            entry = ue.TikiShredDemoClip()
            entry.set_editor_property("clip", clip_enum(clip))
            entry.set_editor_property("mesh", mesh)
            entry.set_editor_property("anim", anim)
            entry.set_editor_property("contact_time", contact / float(fps))
            entry.set_editor_property("severed_bones", severed)
            entries.append(entry)
            summary.append({"clip": clip, "mesh": mesh.get_name(), "anim": anim.get_name(), "contact_s": round(contact / fps, 3),
                            "severed": severed})
        asset.set_editor_property("demos", entries)
        eal.save_loaded_asset(asset, only_if_is_dirty=False)
        report["sets"][mob] = {"asset": f"{folder}/{name}", "demos": summary}
        return asset

    def describe_blueprint(mob, cdo):
        anims = cdo.get_editor_property("anims")
        combat = anims.get_editor_property("combat")
        clips = {}
        for prop in ("musket_impact", "musket_burst", "musket_stagger_recover", "musket_articulated_fall", "sabre_left_to_right",
                     "sabre_right_to_left", "sabre_down", "sabre_low", "headshot_death", "decapitated_death",
                     "legless_crawl_death", "bisected_death"):
            try:
                clip = combat.get_editor_property(prop)
            except Exception:  # noqa: BLE001 - a property of a newer build
                continue
            clips[prop] = clip.get_name() if clip else None
        dropped = cdo.get_editor_property("dropped_weapon_class")
        shred = cdo.get_editor_property("shred_demo_set")
        dismember = cdo.get_editor_property("dismember_set")
        return {"combat_clips": clips, "missing_combat_clips": sorted(k for k, v in clips.items() if not v),
                "dropped_weapon_class": dropped.get_name() if dropped else None,
                "shred_demo_set": shred.get_name() if shred else None,
                "dismember_set": dismember.get_name() if dismember else None,
                "rules": cdo.get_editor_property("rules").get_name() if cdo.get_editor_property("rules") else None}

    try:
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    except Exception:  # noqa: BLE001 - a commandlet may have no editor world: the cvar needs none
        world = None
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    t0 = time.time()
    try:
        if not hasattr(ue, "TikiShredDemoSet"):
            raise RuntimeError("class TikiShredDemoSet missing: build the editor with the T-168 C++")
        ue.SystemLibrary.execute_console_command(world, flag + " 0")
        stopped = None
        for mob in mobs:
            demos = {}
            t_mob = time.time()
            for clip in CLIPS:
                if "import" in steps and clip in clips:
                    try:
                        demos[clip] = import_demo(mob, clip)
                    except Exception:  # noqa: BLE001 - reported; nothing else is imported after it
                        report["failed"].append({"mob": mob, "clip": clip, "error": traceback.format_exc()[-1500:]})
                        ue.log_error(f"[shred_demo] {mob} {clip}: import FAILED, the run stops here")
                        # a failed import may leave the FBX SDK unstable: no further import, no set, no Blueprint
                        stopped = f"{mob} {clip}"
                        break
                    continue
                folder, name, anim_name = demo_names(mob, clip)
                mesh = load(f"{folder}/{name}")
                anim = find_anim(folder, anim_name, mesh.get_editor_property("skeleton")) if mesh else None
                if mesh and anim:
                    demos[clip] = (mesh, anim)
            report["timings_s"][mob] = round(time.time() - t_mob, 1)
            if stopped:
                report["stopped_at"] = stopped
                break
            demo_set = fill_set(mob, demos) if "sets" in steps else load(f"{CHARS}/{mob}/ShredDemos/DA_{mob}_ShredDemoSet")
            bp_path = BLUEPRINTS[mob]
            generated = eal.load_blueprint_class(bp_path) if eal.does_asset_exist(bp_path) else None
            if not generated:
                warn(f"{mob}: no Blueprint {bp_path}")
                continue
            cdo = ue.get_default_object(generated)
            if "blueprints" in steps and demo_set is not None:
                cdo.set_editor_property("shred_demo_set", demo_set)
                eal.save_asset(bp_path, only_if_is_dirty=False)
            report["blueprints"][mob] = describe_blueprint(mob, cdo)
        report["passed"] = not report["failed"]
    except Exception:  # noqa: BLE001
        report["error"] = traceback.format_exc()[-3000:]
        report["passed"] = False
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
        report["total_s"] = round(time.time() - t0, 1)
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "shred-demo.json").write_text(json.dumps(report, indent=1, default=str, ensure_ascii=False), encoding="utf-8")
        ue.log(f"[shred_demo] {'ok' if report.get('passed') else 'FAILED'}: {len(report['imported'])} imported, "
               f"{len(report['kept'])} kept, {len(report['failed'])} failed in {report['total_s']} s")
    if not report.get("passed"):
        raise RuntimeError(report.get("error") or f"failed: {[f['mob'] + ' ' + f['clip'] for f in report['failed']]}")


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(check())
    try:
        import unreal  # noqa: F401
    except ImportError:
        sys.exit("run inside Unreal (lead session action shreddemo) or with --check")
    run_in_unreal()
