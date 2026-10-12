"""Saves SK_Bean_PirateMusket_Skeleton once the editor merged the bones its mesh needs (T-212 musket pack).

Loading SK_Bean_PirateMusket logs "the skeleton is missing bones that SkeletalMesh needs (cartridge). They will be added
now. Please save the Skeleton!": the bone is added in memory at each load, never on disk. This loads the mesh (the merge
happens then), and saves the skeleton alone, even when the editor did not mark it dirty.
Runs from scripts/editor/lead_session_t186.py, action "musketskel".
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
MESH = "/Game/MonkeIsland/Weapons/Musket/SK_Bean_PirateMusket"
SKELETON = "/Game/MonkeIsland/Weapons/Musket/SK_Bean_PirateMusket_Skeleton"

mesh = ue.EditorAssetLibrary.load_asset(MESH)
skeleton = ue.EditorAssetLibrary.load_asset(SKELETON)
assert mesh and skeleton, "musket mesh or skeleton missing"
assert mesh.get_editor_property("skeleton") == skeleton, "the mesh uses another skeleton"
saved = ue.EditorAssetLibrary.save_asset(SKELETON, only_if_is_dirty=False)
(ROOT / "monke_island/Saved/T186/musket-skeleton.json").write_text(json.dumps({"saved": bool(saved)}), encoding="utf-8")
assert saved, "the skeleton was not saved"
