"""T-192: sets every material slot of SK_Bean_Body to its MI (MI_<slot without M_>) and checks it held.

The reimport of bean_moustache_ue.py sets the slots right after the import, while the skeletal mesh may still be
building: the assignment was lost (moustache and hair without material, navy on a placeholder of the Meshes folder).
This runs apart, once the reimport is done, and reads the slots back from the saved mesh.
It also makes M_Bean_Hidden (masked, opacity 0): UBeanPuppetComponent gives it to the hair and moustache slots on the
owner's machine in first person (they rode the head bone in front of the camera, owner report 2026-10-07).
Runs in the editor: scripts/editor/lead_session_t186.py, action "beanmats".
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T192"
MATS = "/Game/MonkeIsland/Characters/Bean/Materials"
MESH = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body"


HIDDEN = MATS + "/M_Bean_Hidden"


def hidden_material():
    """M_Bean_Hidden: a surface material masked to nothing, usable on skeletal meshes"""
    mat = kitlib.load(HIDDEN)
    if mat:
        return mat
    mat = ue.AssetToolsHelpers.get_asset_tools().create_asset("M_Bean_Hidden", MATS, ue.Material, ue.MaterialFactoryNew())
    assert mat, HIDDEN
    mat.set_editor_property("blend_mode", ue.BlendMode.BLEND_MASKED)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mel = ue.MaterialEditingLibrary
    zero = mel.create_material_expression(mat, ue.MaterialExpressionConstant, -300, 0)
    zero.set_editor_property("r", 0.0)
    mel.connect_material_property(zero, "", ue.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(mat)
    kitlib.save(mat)
    return mat


def main():
    kitlib.check_lock()
    hidden = hidden_material()
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"
    mesh = kitlib.load(MESH)
    assert mesh, MESH
    slots = mesh.get_editor_property("materials")
    for slot in slots:
        name = str(slot.get_editor_property("material_slot_name"))
        mi = kitlib.load(f"{MATS}/MI_{name[2:]}") if name.startswith("M_") else None
        assert mi, f"no MI for {name}"
        slot.set_editor_property("material_interface", mi)
    mesh.modify()
    mesh.set_editor_property("materials", slots)
    kitlib.save(mesh)
    check = {}
    for slot in kitlib.load(MESH).get_editor_property("materials"):
        mi = slot.get_editor_property("material_interface")
        check[str(slot.get_editor_property("material_slot_name"))] = mi.get_path_name() if mi else None
    wrong = {k: v for k, v in check.items() if not v or not v.startswith(MATS + "/MI_")}
    assert not wrong, f"slots without their MI: {wrong}"
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "bean-materials.json").write_text(json.dumps({"mesh": MESH, "slots": check, "hidden": hidden.get_path_name(),
        "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}, indent=2), encoding="utf-8")
    ue.log("T192_BEAN_MATERIALS_SUCCESS")


if __name__ == "__main__":
    main()
