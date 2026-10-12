"""Import the local hold-battery revision and transparent cabin glazing."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import sys
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import import_galleon_t149 as prior
OUT = ROOT / "monke_island/Saved/T184"
STAGE = ROOT / "art/export/Galleon_T184"
DEST = "/Game/MonkeIsland/Galleon/Revisions/T184"

def main():
    prior.check_lock()
    report = json.loads((STAGE / "export-result.json").read_text(encoding="utf-8"))
    assert hashlib.sha256((STAGE / "SM_Galleon.fbx").read_bytes()).hexdigest() == report["exports"]["SM_Galleon"]["sha256"]
    materials = {name: prior.load(prior.DEST + "/Materials/" + name) for name in prior.RECIPES}
    assert all(materials.values())
    glass_path = DEST + "/Materials/M_Galleon_CabinGlass"
    glass = prior.load(glass_path) or prior.TOOLS.create_asset("M_Galleon_CabinGlass", DEST + "/Materials", ue.Material, ue.MaterialFactoryNew())
    glass.set_editor_property("blend_mode", ue.BlendMode.BLEND_TRANSLUCENT)
    glass.set_editor_property("two_sided", True)
    glass.set_editor_property("translucency_lighting_mode", ue.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    prior.MEL.delete_all_material_expressions(glass)
    graph = prior.Graph(glass)
    graph.out(graph.color(prior.linear("ABC2C5")), ue.MaterialProperty.MP_BASE_COLOR)
    graph.out(graph.scalar(.08), ue.MaterialProperty.MP_ROUGHNESS)
    graph.out(graph.scalar(.5), ue.MaterialProperty.MP_SPECULAR)
    fresnel = graph.node(ue.MaterialExpressionFresnel, exponent=4.0, base_reflect_fraction=.04)
    opacity = graph.custom("return lerp(0.13, 0.5, F);", ue.CustomMaterialOutputType.CMOT_FLOAT1, F=fresnel)
    graph.out(opacity, ue.MaterialProperty.MP_OPACITY)
    graph.out(graph.scalar(1.01), ue.MaterialProperty.MP_REFRACTION)
    prior.MEL.recompile_material(glass)
    prior.save(glass)
    materials["M_Galleon_Glass"] = glass
    old_globals = (prior.DEST, prior.MESH_SRC, prior.VARIANT, prior.RESULT)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        prior.DEST, prior.MESH_SRC, prior.VARIANT, prior.RESULT = DEST, STAGE, {"ucx": report["exports"]["SM_Galleon"]["ucx"]}, {"meshes": {}}
        mesh = prior.import_mesh("SM_Galleon", materials, scale=1.0, retry=True)
        info = prior.RESULT["meshes"]["SM_Galleon"]
    finally:
        prior.DEST, prior.MESH_SRC, prior.VARIANT, prior.RESULT = old_globals
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
    # Conventional geometry is required for the translucent panes in this mesh.
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    prior.save(mesh)
    path = prior.DEST + "/DA_GalleonRules_TestFlat"
    rules = prior.load(path)
    backup = "/Game/MonkeIsland/Galleon/Backups/DA_GalleonRules_TestFlat_PreT184"
    if not prior.load(backup):
        prior.save(prior.EAL.duplicate_asset(path, backup))
    rules.set_editor_property("hull_mesh", mesh)
    prior.save(rules)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    ship = next(a for a in actors if a.get_actor_label() == "Beach_Galleon")
    ship.set_editor_property("rules", rules)
    hull = ship.get_editor_property("hull")
    hull.set_static_mesh(mesh)
    hull.set_editor_property("override_materials", [])
    assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"mesh": info, "glass": glass_path, "hold_battery_triangles": report["hold_battery_triangles"],
              "removed_crates": report["removed_crates"], "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "import-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T184_IMPORT_SUCCESS")

if __name__ == "__main__":
    main()
