"""T-296 follow-up (security review M3): L_Boot ran with the engine's APlayerController, so the refusal of a remote
client's serverexec / EnableCheats / pause (Amonke_islandPlayerController) did not cover that map. BP_BootGameMode gets
the project's player controller (BP_FirstPersonPlayerController, child of Amonke_islandPlayerController).
Lead session action "bootpc" (no game running). Report: monke_island/Saved/T296/boot-player-controller.json.
Replays change nothing.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T296/boot-player-controller.json"
GAME_MODE = "/Game/MonkeIsland/Expedition/Blueprints/BP_BootGameMode"
CONTROLLER = "/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"


def main():
    report = {}
    mode_bp = ue.EditorAssetLibrary.load_asset(GAME_MODE)
    controller_bp = ue.EditorAssetLibrary.load_asset(CONTROLLER)
    if not mode_bp or not controller_bp:
        raise RuntimeError(f"missing: {GAME_MODE if not mode_bp else CONTROLLER}")
    controller_class = controller_bp.generated_class()
    parent = controller_class
    chain = []
    while parent:
        chain.append(parent.get_name())
        parent = ue.SystemLibrary.get_super_class(parent) if hasattr(ue.SystemLibrary, "get_super_class") else None
        if len(chain) > 8:
            break
    report["controller_chain"] = chain
    cdo = ue.get_default_object(mode_bp.generated_class())
    before = cdo.get_editor_property("player_controller_class")
    report["before"] = before.get_name() if before else None
    if before != controller_class:
        cdo.set_editor_property("player_controller_class", controller_class)
        ue.BlueprintEditorLibrary.compile_blueprint(mode_bp)
        report["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(mode_bp, only_if_is_dirty=False))
    after = ue.get_default_object(mode_bp.generated_class()).get_editor_property("player_controller_class")
    report["after"] = after.get_name() if after else None
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"BOOT_PC {report['before']} -> {report['after']}")


main()
