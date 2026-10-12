"""T-017 (MVP, ADR-008): imports the bean's swim clips into Unreal and files them in DA_BeanAnimLibrary.

Source: art/export/Characters/Bean/Anims/A_Bean_Swim.fbx and A_Bean_TreadWater.fbx, made by
scripts/character/bean_swim_clips.py (Blender, report bean_swim_clips.json next to them).
1. each FBX, one at a time (the first failure stops everything), on the game skeleton of the bean
   (SK_Bean_Body_Skeleton, the one of every A_Bean_* clip, never a new skeleton) into
   /Game/MonkeIsland/Characters/Bean/Animations/Library, with the settings of import_bean_library.py (legacy FBX
   importer, exported time, no root motion); a clip already imported from the same FBX (sha256 in the last report)
   is not imported again;
2. checks: AnimSequence on that skeleton, length (1.2 s / 1.6 s), root bone scale at t = 0 equal to A_Bean_Idle's
   (x100: a root at 1 draws the bean a hundred times too small);
3. DA_BeanAnimLibrary.Swim / TreadWater (and SwimRootDepth = the clips' water line). The properties come with the
   C++ of T-017: before the build that adds them, the report says so and the script stops cleanly (clips imported).
Runs in the editor: scripts/editor/lead_session_t186.py, action "beanswim" (editor lock held, no game running).
Report: monke_island/Saved/T-017/beanswim-import.json. A failed import raises (the lead session then refuses
further imports until the editor restarts).
"""
import datetime as dt
import hashlib
import json
import os
import time
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "art/export/Characters/Bean/Anims"
CLIPS_REPORT = SRC / "bean_swim_clips.json"
OUT = ROOT / "monke_island/Saved/T-017"
REPORT = OUT / "beanswim-import.json"
BEAN = "/Game/MonkeIsland/Characters/Bean"
DEST = f"{BEAN}/Animations/Library"
SKELETON = f"{BEAN}/Meshes/SK_Bean_Body_Skeleton"
LIBRARY = f"{BEAN}/Animations/DA_BeanAnimLibrary"
REFERENCE_CLIP = f"{DEST}/A_Bean_Idle"
# clip -> (DA_BeanAnimLibrary property, expected length in s)
CLIPS = {"A_Bean_Swim": ("swim", 1.2), "A_Bean_TreadWater": ("tread_water", 1.6)}
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


class ImportFailed(RuntimeError):
    """An FBX import did not produce the expected animation"""


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def log(text):
    ue.log_warning(f"[beanswim] {text}")


def check_lock():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    owner = os.environ.get("MONKE_LOCK_OWNER")
    assert owner and lock["owner"] == owner, f"editor lock held by {lock['owner']}, not {owner}"
    assert dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone(), "editor lock expired"
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stable_file(path):
    """The FBX is complete: present, not empty, same size twice, no temporary file beside it"""
    if not path.is_file():
        return f"{path.name} missing (run scripts/character/bean_swim_clips.py --all)"
    size = path.stat().st_size
    time.sleep(0.5)
    if size == 0 or path.stat().st_size != size:
        return f"{path.name} empty or still being written"
    if any(p.name.startswith(path.stem) and p.suffix.lower() in (".tmp", ".part") for p in path.parent.iterdir()):
        return f"{path.name}: a temporary file is still beside it"
    return None


def clip_options(skeleton):
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
    return options


def root_scale(sequence):
    """Scale of the root bone at t = 0 (local space), or an error text"""
    try:
        pose = ue.AnimPoseExtensions.get_anim_pose_at_time(sequence, 0.0, ue.AnimPoseEvaluationOptions())
        scale = ue.AnimPoseExtensions.get_bone_pose(pose, "root", ue.AnimPoseSpaces.LOCAL).scale3d
        return [round(scale.x, 3), round(scale.y, 3), round(scale.z, 3)]
    except Exception as error:  # noqa: BLE001
        return f"unavailable: {error}"


def describe(sequence):
    info = {"length_s": round(sequence.get_play_length(), 4), "root_scale_t0": root_scale(sequence)}
    for key, fn in (("frames", "get_num_frames"), ("keys", "get_num_keys")):
        try:
            info[key] = getattr(ue.AnimationLibrary, fn)(sequence)
        except Exception:  # noqa: BLE001
            pass
    return info


def import_clip(name, fbx, skeleton):
    task = ue.AssetImportTask()
    task.filename = str(fbx)
    task.destination_path = DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = clip_options(skeleton)
    TOOLS.import_asset_tasks([task])
    sequence = load(f"{DEST}/{name}")
    if not isinstance(sequence, ue.AnimSequence):
        raise ImportFailed(f"import of {fbx.name} into {DEST}/{name} produced no AnimSequence")
    if sequence.get_editor_property("skeleton") != skeleton:
        raise ImportFailed(f"{name}: imported on another skeleton ({sequence.get_editor_property('skeleton')})")
    return sequence


def fill_library(rep, clips, water_line_cm):
    lib = load(LIBRARY)
    if lib is None:
        rep["library"] = {"status": "missing", "path": LIBRARY}
        return
    missing, set_props = [], []
    for name, (prop, _length) in CLIPS.items():
        if name not in clips:
            continue
        try:
            lib.set_editor_property(prop, clips[name])
            set_props.append(prop)
        except Exception as error:  # noqa: BLE001
            missing.append(f"{prop}: {error}")
    if water_line_cm is not None:
        try:
            if abs(lib.get_editor_property("swim_root_depth") - water_line_cm) > 0.01:
                lib.set_editor_property("swim_root_depth", float(water_line_cm))
            set_props.append(f"swim_root_depth={water_line_cm}")
        except Exception as error:  # noqa: BLE001
            missing.append(f"swim_root_depth: {error}")
    if missing:
        # the C++ of T-017 (UBeanAnimLibrary::Swim, TreadWater, SwimRootDepth) is not built yet: nothing half set
        rep["library"] = {"status": "pending build: DA_BeanAnimLibrary has no swim properties yet; rebuild then run "
                                    "beanswim again (the clips will not be imported twice)", "errors": missing}
        log(rep["library"]["status"])
        return
    save(lib)
    rep["library"] = {"status": "filled", "set": set_props}


def main():
    check_lock()
    OUT.mkdir(parents=True, exist_ok=True)
    previous = json.loads(REPORT.read_text(encoding="utf-8")) if REPORT.exists() else {}
    rep = {"task": "T-017", "action": "beanswim", "started_at": now(), "owner": os.environ.get("MONKE_LOCK_OWNER"),
           "clips": {}, "warnings": []}
    failure = None
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    legacy = ue.SystemLibrary.get_console_variable_int_value(flag)
    try:
        skeleton = load(SKELETON)
        assert isinstance(skeleton, ue.Skeleton), f"{SKELETON} missing"
        reference = load(REFERENCE_CLIP)
        rep["reference"] = {"clip": REFERENCE_CLIP, **describe(reference)} if reference else {"clip": "missing"}
        clips_report = json.loads(CLIPS_REPORT.read_text(encoding="utf-8")) if CLIPS_REPORT.exists() else {}
        water_line_cm = clips_report.get("water_line_cm")
        rep["water_line_cm"] = water_line_cm
        ue.SystemLibrary.execute_console_command(world, flag + " 0")
        imported = {}
        for name, (_prop, length) in CLIPS.items():
            fbx = SRC / f"{name}.fbx"
            entry = {"fbx": str(fbx.relative_to(ROOT)).replace("\\", "/")}
            rep["clips"][name] = entry
            problem = stable_file(fbx)
            if problem:
                entry["status"] = problem
                raise ImportFailed(f"{name}: {problem}")
            entry["sha256"] = digest = sha256(fbx)
            existing = load(f"{DEST}/{name}")
            known = previous.get("clips", {}).get(name, {})
            if isinstance(existing, ue.AnimSequence) and known.get("sha256") == digest and known.get("status") == "imported":
                sequence = existing
                entry["status"] = "imported"
                entry["note"] = "already imported from this FBX"
            else:
                t0 = time.time()
                sequence = import_clip(name, fbx, skeleton)  # raises ImportFailed: nothing after it runs
                save(sequence)
                entry["status"] = "imported"
                entry["import_s"] = round(time.time() - t0, 1)
            entry["asset"] = f"{DEST}/{name}"
            entry["skeleton"] = sequence.get_editor_property("skeleton").get_path_name()
            entry.update(describe(sequence))
            if abs(entry["length_s"] - length) > 0.02:
                rep["warnings"].append(f"{name}: {entry['length_s']} s instead of {length} s")
            ref_scale = rep["reference"].get("root_scale_t0")
            if isinstance(ref_scale, list) and isinstance(entry["root_scale_t0"], list) \
                    and max(abs(a - b) for a, b in zip(ref_scale, entry["root_scale_t0"])) > 0.01 * max(ref_scale):
                rep["warnings"].append(f"{name}: root scale {entry['root_scale_t0']} differs from A_Bean_Idle "
                                       f"{ref_scale} (PoseRoot::KeepReferenceScale keeps the mesh's at run time)")
            imported[name] = sequence
            log(f"{name}: {entry}")
        fill_library(rep, imported, water_line_cm)
        rep["passed"] = True
    except Exception as error:  # noqa: BLE001
        rep["passed"] = False
        rep["error"] = traceback.format_exc()[-3000:]
        failure = error
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(legacy))
        rep["finished_at"] = now()
        rep["next"] = ("PIE (BP_IslanderBean, 3rd person): swim in deep water - breaststroke when moving, treading water "
                       "at rest, cross fade, eyes and tricorne above the water, no mannequin pose; walk out of the water "
                       "(back to the walk without a landing clip); jump out of the water toward the boat (jump clip)")
        REPORT.write_text(json.dumps(rep, indent=1, default=str, ensure_ascii=False), encoding="utf-8")
    if failure is not None:
        raise failure


if __name__ == "__main__":
    main()
