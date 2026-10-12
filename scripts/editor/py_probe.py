"""Diagnosis (read only): root and arm scales of the clips a French soldier and a tiki play."""
import json
from pathlib import Path
import unreal as ue

OUT = Path(r"C:/Users/matth/monk-island/monke_island/Saved/T186/py-probe.json")
A = ue.AnimPoseExtensions
rep = {}
CLIPS = ["/Game/MonkeIsland/Characters/BeanFR/Animations/A_BeanFR_Idle_Guard", "/Game/MonkeIsland/Characters/BeanFR/Animations/A_BeanFR_Patrol",
         "/Game/MonkeIsland/Characters/BeanFR/Animations/Gore/A_BeanFR_Hit_Head_Front", "/Game/MonkeIsland/Characters/BeanFR/Animations/Gore/A_BeanFR_Hat_Knocked_Off",
         "/Game/MonkeIsland/Characters/Villager/Anims/Gore/A_Villager_Hit_Head", "/Game/MonkeIsland/Characters/BeanFR/Animations/Gore/A_BeanFR_Death_Headshot"]
# the library clips of the bean and the musket poses the rules name
rules = ue.load_asset("/Game/MonkeIsland/Characters/BeanFR/Data/DA_FortSoldierRules")
for prop in ("idle_guard", "patrol", "walk", "run", "fire", "reload", "aim", "hit_react", "death", "hit_head", "hit_torso", "hit_arm_l", "death_headshot", "sabre_idle"):
    try:
        clip = rules.get_editor_property(prop)
        if clip:
            CLIPS.append(clip.get_path_name().split(".")[0])
    except Exception as error:  # noqa: BLE001
        rep.setdefault("rules_errors", []).append(f"{prop}: {error}")
try:
    poses = rules.get_editor_property("musket_poses")
    for prop in ("body_fire_animation", "body_reload_animation"):
        clip = poses.get_editor_property(prop) if poses else None
        if clip:
            CLIPS.append(clip.get_path_name().split(".")[0])
except Exception as error:  # noqa: BLE001
    rep.setdefault("rules_errors", []).append(f"musket_poses: {error}")
seen = set()
for path in CLIPS:
    if path in seen:
        continue
    seen.add(path)
    try:
        anim = ue.load_asset(path)
        opts = ue.AnimPoseEvaluationOptions()
        pose = A.get_anim_pose_at_time(anim, 0.2, opts)
        row = {}
        for b in ("root", "pelvis", "upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "hat", "head"):
            try:
                t = A.get_bone_pose(pose, b, ue.AnimPoseSpaces.LOCAL)
                row[b] = [round(v, 3) for v in (t.scale3d.x, t.scale3d.y, t.scale3d.z)]
            except Exception:  # noqa: BLE001
                row[b] = None
        tracks = [str(n) for n in ue.AnimationLibrary.get_animation_track_names(anim)]
        rep[path.rsplit("/", 1)[1]] = {"skeleton": anim.get_editor_property("skeleton").get_name(), "tracks": len(tracks), "has_root_track": "root" in tracks, "scales": row,
                                      "len": round(anim.get_play_length(), 2)}
    except Exception as error:  # noqa: BLE001
        rep[path] = str(error)
OUT.write_text(json.dumps(rep, indent=1), encoding="utf-8")
