"""Loads the map named in monke_island/Saved/T186/loadlevel.txt in the editor (no unsaved change expected). Action
"loadlevel"."""
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
target = (ROOT / "monke_island/Saved/T186/loadlevel.txt").read_text(encoding="utf-8").strip()
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(target):
    assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(target), f"cannot load {target}"
