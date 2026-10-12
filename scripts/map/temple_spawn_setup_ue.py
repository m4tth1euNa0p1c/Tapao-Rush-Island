"""T-180: a reliquary temple in every played fragment (owner request 2026-10-06: "au moins un par zone, entre les marae
pyramides et les villages, au milieu du tuamotu, en spawn, supprime arbres et buissons s'il faut").

- DA_TempleReliquaryRules (UMaraeRules of T-187) gets its placement in the fragment: one temple, on the free camp spot
  (scripts/map/place_camps.py: land, away from the villages and hamlets) nearest to the middle of the fragment's land,
  120 m at least from the marae, 150 m from the players at the start (found by exploring), on level land above the
  beach (ground looked at 14 m around), its gate turned towards the arrival; a second pass guarantees one.
- The ACampSpawner of LI_ExpeditionCore (the core of every L_Frag map) builds it: TempleRules set on its instance.
  Lvl_Raraka (110 MB) keeps its camp spawner without temple until it is opened anyway.
Runs in the editor: scripts/editor/lead_session_t186.py, action "temples".
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T180"
RULES = "/Game/MonkeIsland/Environment/TempleReliquary/Data/DA_TempleReliquaryRules"
CORE = "/Game/MonkeIsland/Maps/Fragments/LI_ExpeditionCore"
BACK = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
PLACEMENT = {
    "count_per_fragment": 1,
    "central": True,
    "guaranteed_near_count": 0,
    "min_distance_from_players": 15000.0,
    "min_distance_from_sites": 12000.0,
    "ground_check_radius": 1400.0,
    "max_ground_delta": 150.0,
    "min_ground_above_sea": 80.0,
    "face_players": True,
}


def main():
    kitlib.check_lock()
    rules = kitlib.load(RULES)
    assert rules, RULES
    for name, value in PLACEMENT.items():
        rules.set_editor_property(name, value)
    kitlib.save(rules)

    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert levels.load_level(CORE), CORE
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    spawners = [a for a in actors if isinstance(a, ue.CampSpawner)]
    assert spawners, "no ACampSpawner in LI_ExpeditionCore"
    for spawner in spawners:
        spawner.set_editor_property("temple_rules", rules)
    # the actors of the core level are gone once the test level is loaded back: their labels are read before
    labels = [s.get_actor_label() for s in spawners]
    assert levels.save_current_level()
    assert levels.load_level(BACK)
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-180", "rules": RULES, "placement": PLACEMENT, "core": CORE,
              "spawners": labels, "lvl_raraka": "not set (heavy level, to do when open)",
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "temples-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T180_TEMPLES_SUCCESS")


if __name__ == "__main__":
    main()
