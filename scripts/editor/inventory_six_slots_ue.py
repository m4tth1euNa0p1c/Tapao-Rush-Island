"""Inventory of six slots, without the bag (owner request 2026-10-07: "ajoute deux slots, E permet uniquement d'ouvrir
la ligne de notre inventaire, enlève les 18 autres slots").

- IA_Slot5 and IA_Slot6 (copies of IA_Slot4) in /Game/MonkeIsland/Weapons/Input, mapped in IMC_Inventory like the
  first four: the digit, the AZERTY key of the top row without shift (5 = "(", 6 = "-") and the keypad;
- BP_IslanderBean, inventory component: 6 slots in the bar, no bag (the C++ defaults now), the six slot actions;
- lists the other mappings of these keys in the contexts of the project (a conflict to look at, nothing changed).
Runs in the editor: scripts/editor/lead_session_t186.py, action "inventory" (editor lock held, no game running).
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/Inventory"
INPUT = "/Game/MonkeIsland/Weapons/Input"
IMC = INPUT + "/IMC_Inventory"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
NEW_KEYS = {5: ["Five", "LeftParantheses", "NumPadFive"], 6: ["Six", "Hyphen", "NumPadSix"]}
OTHER_CONTEXTS = [
    "/Game/Input/IMC_Default",
    "/Game/MonkeIsland/Player/Input/IMC_Islander_Actions",
    "/Game/MonkeIsland/Player/Input/IMC_Islander_AZERTY",
    "/Game/MonkeIsland/Emotes/Input/IMC_TrumpetEmote",
    "/Game/MonkeIsland/Emotes/Input/IMC_FlagCeremony",
    "/Game/MonkeIsland/Weapons/Input/IMC_Musket",
    "/Game/MonkeIsland/Weapons/Input/IMC_Cutlass",
    "/Game/MonkeIsland/Weapons/Input/IMC_Torch",
]
EAL = ue.EditorAssetLibrary


def key(name):
    k = ue.Key()
    k.set_editor_property("key_name", name)
    return k


def mappings_of(imc):
    value = imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
    return list(value)


def main():
    kitlib.check_lock()
    imc = kitlib.load(IMC)
    assert imc, IMC
    actions = [kitlib.load(f"{INPUT}/IA_Slot{n}") for n in range(1, 5)]
    assert all(actions), "IA_Slot1-4 missing"
    for slot, names in NEW_KEYS.items():
        path = f"{INPUT}/IA_Slot{slot}"
        action = kitlib.load(path) or EAL.duplicate_asset(f"{INPUT}/IA_Slot4", path)
        assert action, path
        kitlib.save(action)
        imc.unmap_all_keys_from_action(action)
        for name in names:
            imc.map_key(action, key(name))
        actions.append(action)
    kitlib.save(imc)

    taken = {name for names in NEW_KEYS.values() for name in names}
    conflicts = []
    for path in OTHER_CONTEXTS:
        other = kitlib.load(path)
        if not other:
            continue
        for mapping in mappings_of(other):
            name = str(mapping.get_editor_property("key").get_editor_property("key_name"))
            if name in taken:
                conflicts.append({"context": path, "key": name, "action": mapping.get_editor_property("action").get_name()})

    player = kitlib.load(PLAYER_BP)
    assert player, PLAYER_BP
    subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
    sdl = ue.SubobjectDataBlueprintFunctionLibrary
    component = None
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(player):
        obj = sdl.get_object(sdl.get_data(handle))
        if obj and isinstance(obj, ue.IslanderInventoryComponent):
            component = obj
    assert component, "BP_IslanderBean has no inventory component"
    component.set_editor_property("slot_count", 6)
    component.set_editor_property("bag_slot_count", 0)
    component.set_editor_property("slot_actions", actions)
    ue.BlueprintEditorLibrary.compile_blueprint(player)
    kitlib.save(player)

    OUT.mkdir(parents=True, exist_ok=True)
    result = {"imc": IMC, "slot_actions": [a.get_name() for a in actions], "new_keys": NEW_KEYS,
              "bar": component.get_editor_property("slot_count"), "bag": component.get_editor_property("bag_slot_count"),
              "conflicts": conflicts, "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "inventory-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("INVENTORY_SIX_SLOTS_SUCCESS")


if __name__ == "__main__":
    main()
