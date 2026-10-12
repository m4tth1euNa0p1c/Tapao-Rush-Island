"""Lists the unsaved packages of the editor (before closing it). Writes monke_island/Saved/T186/dirty.json. Action
"dirtylist"."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
maps = [p.get_name() for p in ue.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
content = [p.get_name() for p in ue.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
(ROOT / "monke_island/Saved/T186/dirty.json").write_text(json.dumps({"maps": maps, "content": content, "level": world.get_path_name() if world else None}, indent=1), encoding="utf-8")
