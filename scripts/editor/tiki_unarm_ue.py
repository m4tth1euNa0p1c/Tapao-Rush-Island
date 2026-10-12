"""Owner 2026-10-07: "enlève l'idée des mobs avec fusil, laisse-les avec leurs armes": no tiki comes out with a musket
any more (DA_TikiRules.Musket.ArmedShare = 0); the musket rules stay in place, unused. Action "tikiunarm"."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
rules = ue.EditorAssetLibrary.load_asset("/Game/MonkeIsland/Characters/Data/DA_TikiRules")
musket = rules.get_editor_property("musket")
musket.set_editor_property("armed_share", 0.0)
rules.set_editor_property("musket", musket)
saved = ue.EditorAssetLibrary.save_asset("/Game/MonkeIsland/Characters/Data/DA_TikiRules", only_if_is_dirty=False)
(ROOT / "monke_island/Saved/T186/tiki-unarm.json").write_text(json.dumps({"armed_share": rules.get_editor_property("musket").get_editor_property("armed_share"), "saved": bool(saved)}), encoding="utf-8")
