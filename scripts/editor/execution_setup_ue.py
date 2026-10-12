"""Assets of the execution scene (owner request 2026-10-07: the crew that missed the quota is shot on a plank by French
soldiers; AExecutionScene). Imports art/audio/execution/SW_Execution_DrumRoll.wav (scripts/props/execution_sounds.py)
into /Game/MonkeIsland/Galleon/Execution/Audio and makes DA_ExecutionRules there: the drum roll, the plank (the engine's
1 m cube stretched, the planks of the camps), four French uniforms for the squad. Places and times keep the class
defaults (UExecutionRules), tuned in PIE. Writes monke_island/Saved/T186/execution-setup.json. Action "execsetup"."""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

FOLDER = "/Game/MonkeIsland/Galleon/Execution"
RULES = FOLDER + "/DA_ExecutionRules"
SQUAD = ["BP_BeanFR_Grenadier", "BP_BeanFR_Fusilier_Bicorne", "BP_BeanFR_Fusilier_1767", "BP_BeanFR_Sergent"]
report = {}
try:
    kitlib.check_lock()
    tools = ue.AssetToolsHelpers.get_asset_tools()
    wav = ROOT / "art/audio/execution/SW_Execution_DrumRoll.wav"
    task = ue.AssetImportTask()
    task.filename = str(wav)
    task.destination_path = FOLDER + "/Audio"
    task.destination_name = "SW_Execution_DrumRoll"
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    sound = kitlib.load(FOLDER + "/Audio/SW_Execution_DrumRoll")
    assert sound, "the drum roll did not import"
    effects = kitlib.load("/Game/MonkeIsland/Settings/SoundClasses/SC_Effects")
    if effects:
        sound.set_editor_property("sound_class_object", effects)
    kitlib.save(sound)
    report["drum"] = [sound.get_path_name(), round(sound.get_editor_property("duration"), 2)]

    if not ue.EditorAssetLibrary.does_asset_exist(RULES):
        created = tools.create_asset("DA_ExecutionRules", FOLDER, ue.ExecutionRules, ue.DataAssetFactory())
        assert created, "DA_ExecutionRules not created"
    rules = kitlib.load(RULES)
    rules.set_editor_property("drum_sound", sound)
    rules.set_editor_property("plank_mesh", kitlib.load("/Engine/BasicShapes/Cube"))
    rules.set_editor_property("plank_material", kitlib.load("/Game/MonkeIsland/Environment/Camp/Materials/MI_Camp_Planks"))
    classes = []
    for name in SQUAD:
        cls = ue.EditorAssetLibrary.load_blueprint_class(f"/Game/MonkeIsland/Characters/BeanFR/Blueprints/{name}")
        if cls:
            classes.append(cls)
    rules.set_editor_property("squad_classes", classes)
    kitlib.save(rules)
    report["rules"] = rules.get_path_name()
    report["squad"] = [c.get_name() for c in rules.get_editor_property("squad_classes")]
    report["plank"] = [str(rules.get_editor_property("plank_mesh")), str(rules.get_editor_property("plank_material"))]
finally:
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    (ROOT / "monke_island/Saved/T186/execution-setup.json").write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
