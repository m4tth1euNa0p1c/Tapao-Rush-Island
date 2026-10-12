"""Owner 2026-10-07 "fix la position de tir / l'animation de rechargement des soldats français": what the French bean and
the player's bean share. Skeletons of BP_BeanFR_* and of the player bean, reference pose of the arms (local and component
space) in each, the musket clips and their skeleton, and the musket grips of DA_FortSoldierRules / DA_MusketRules; for
both skeletons, the pose of A_Bean_Musket_Fire at the aim (FireAnimationTriggerTime) and of A_Bean_Musket_Reload: hands
and the musket (track sample) in component space. Writes monke_island/Saved/T186/soldier-skeleton.json. Action
"soldierskel"."""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186/soldier-skeleton.json"
report = {}
BONES = ("root", "pelvis", "spine_01", "spine_02", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "hand_l", "lowerarm_l")


def tf(t):
    r = t.rotation.rotator()
    return [round(t.translation.x, 2), round(t.translation.y, 2), round(t.translation.z, 2),
            round(r.pitch, 1), round(r.yaw, 1), round(r.roll, 1), round(t.scale3d.x, 2)]


def pose_of(pose, space):
    out = {}
    for b in BONES:
        try:
            out[b] = tf(ue.AnimPoseExtensions.get_bone_pose(pose, b, space))
        except Exception as error:  # noqa: BLE001
            out[b] = f"? {error}"[:80]
    return out


def ref_pose(skeleton):
    pose = ue.AnimPoseExtensions.get_reference_pose(skeleton)
    return {"local": pose_of(pose, ue.AnimPoseSpaces.LOCAL), "world": pose_of(pose, ue.AnimPoseSpaces.WORLD)}


def anim_pose(seq, time, mesh):
    opts = ue.AnimPoseEvaluationOptions()
    opts.set_editor_property("optional_skeletal_mesh", mesh)
    opts.set_editor_property("evaluation_type", ue.AnimDataEvalType.RAW)
    pose = ue.AnimPoseExtensions.get_anim_pose_at_time(seq, time, opts)
    return pose_of(pose, ue.AnimPoseSpaces.WORLD)


try:
    fr_rules = ue.EditorAssetLibrary.load_asset("/Game/MonkeIsland/Characters/BeanFR/Data/DA_FortSoldierRules")
    poses = fr_rules.get_editor_property("musket_poses")
    report["fr_rules"] = {"hand_bone": str(fr_rules.get_editor_property("hand_bone")),
                          "musket_in_hand": tf(fr_rules.get_editor_property("musket_in_hand")),
                          "musket_mesh": str(fr_rules.get_editor_property("musket_mesh").get_path_name()),
                          "musket_poses": poses.get_path_name() if poses else None}
    report["musket_rules"] = {"hand_bone": str(poses.get_editor_property("hand_bone")), "hand_offset": tf(poses.get_editor_property("hand_offset")),
                              "musket_mesh": poses.get_editor_property("musket_mesh").get_path_name(),
                              "fire": poses.get_editor_property("body_fire_animation").get_path_name(),
                              "reload": poses.get_editor_property("body_reload_animation").get_path_name(),
                              "trigger": poses.get_editor_property("fire_animation_trigger_time"),
                              "fire_track": [tf(t) for t in list(poses.get_editor_property("fire_musket_track"))[::4]],
                              "reload_track_n": len(poses.get_editor_property("reload_musket_track"))}
    fire = poses.get_editor_property("body_fire_animation")
    reload_ = poses.get_editor_property("body_reload_animation")
    report["fire_skeleton"] = fire.get_editor_property("skeleton").get_path_name()
    meshes = {}
    for path in ue.EditorAssetLibrary.list_assets("/Game/MonkeIsland/Characters/BeanFR/Blueprints", False, False):
        bp = ue.EditorAssetLibrary.load_asset(path.split(".")[0])
        cdo = ue.get_default_object(bp.generated_class())
        mesh = cdo.get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")
        meshes[path.split("/")[-1].split(".")[0]] = mesh.get_path_name() if mesh else None
    report["fr_meshes"] = meshes
    fr_mesh = ue.EditorAssetLibrary.load_asset(next(v for v in meshes.values() if v).split(".")[0])
    player_mesh = ue.EditorAssetLibrary.load_asset("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body")
    report["fr_skeleton"] = fr_mesh.get_editor_property("skeleton").get_path_name()
    report["player_skeleton"] = player_mesh.get_editor_property("skeleton").get_path_name() if player_mesh else None
    report["fr_ref"] = ref_pose(fr_mesh.get_editor_property("skeleton"))
    if player_mesh:
        report["player_ref"] = ref_pose(player_mesh.get_editor_property("skeleton"))
    trig = poses.get_editor_property("fire_animation_trigger_time")
    for name, mesh in (("fr", fr_mesh), ("player", player_mesh)):
        if not mesh:
            continue
        try:
            report[f"{name}_fire_aim"] = anim_pose(fire, trig, mesh)
            report[f"{name}_reload_2s"] = anim_pose(reload_, 2.0, mesh)
        except Exception as error:  # noqa: BLE001
            report[f"{name}_anim_error"] = str(error)[:300]
finally:
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
