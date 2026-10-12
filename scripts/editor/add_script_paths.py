"""Puts the repository's script folders on the editor's Python path (scripts/props, scripts/map, scripts/character,
scripts/editor), so the actions of scripts/editor/lead_session_t186.py can run scripts that import their neighbours
(Codex's fort scripts: fort_showcase_import, fort_showcase_setup...). Action "paths"; harmless to repeat."""
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
for folder in ("scripts/props", "scripts/map", "scripts/character", "scripts/editor"):
    path = str(ROOT / folder)
    if path not in sys.path:
        sys.path.insert(0, path)
ue.log(f"[paths] {[p for p in sys.path if 'monk-island' in p]}")
