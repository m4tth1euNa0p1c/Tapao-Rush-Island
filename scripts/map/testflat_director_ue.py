"""T-195: a tiki director in L_Test_Flat, as in LI_ExpeditionCore (scripts/editor/setup_fragment_map.py): without it the
generated village Uo-mura has no life (villagers of the square, villagers of the houses, gathering at the flag
ceremony, thefts) in the test map. Same rules and classes as the fragment maps.
Runs in the editor: scripts/editor/lead_session_t186.py, action "testdirector" (editor lock held, no game running).
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

G = "/Game/MonkeIsland/"
LEVEL = G + "Maps/Test/L_Test_Flat"
LABEL = "TestFlat_TikiDirector"
FOLDER = "TestFlat/Zone_Generateurs"
OUT = ROOT / "monke_island/Saved/T195"


def main():
    kitlib.check_lock()
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world or "L_Test_Flat" not in world.get_path_name():
        assert levels.load_level(LEVEL), LEVEL
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    existing = [a for a in actors.get_all_level_actors() if isinstance(a, ue.TikiDirector)]
    director = existing[0] if existing else actors.spawn_actor_from_class(ue.TikiDirector, ue.Vector(-30000.0, 0.0, 300.0),
                                                                          ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    director.set_actor_label(LABEL)
    director.set_folder_path(FOLDER)
    director.set_editor_property("rules", kitlib.load(G + "Characters/Data/DA_TikiRules"))
    director.set_editor_property("villager_class", ue.load_class(None, G + "Characters/Villager/Blueprints/BP_TikiVillager.BP_TikiVillager_C"))
    director.set_editor_property("warrior_classes", [
        ue.load_class(None, G + "Characters/CocoWarrior/Blueprints/BP_TikiCocoWarrior.BP_TikiCocoWarrior_C"),
        ue.load_class(None, G + "Characters/CrabGuardian/Blueprints/BP_TikiCrabGuardian.BP_TikiCrabGuardian_C"),
        ue.load_class(None, G + "Characters/AshWatcher/Blueprints/BP_TikiAshWatcher.BP_TikiAshWatcher_C")])
    director.set_editor_property("coconut_class", ue.load_class(None, G + "Items/Blueprints/BP_CoconutPickup.BP_CoconutPickup_C"))
    director.set_editor_property("village_radius", 5000.0)
    assert levels.save_current_level()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "testdirector-result.json").write_text(json.dumps({"level": LEVEL, "director": LABEL, "was_there": bool(existing),
        "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}, indent=2), encoding="utf-8")
    ue.log("T195_TEST_DIRECTOR_SUCCESS")


if __name__ == "__main__":
    main()
