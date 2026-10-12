"""Tracer rounds of the firearms (owner 2026-10-10: "crée et ajoute les projectiles des balles des différentes armes, les
traces des balles comme des munitions traçantes", T-276).

- M_Tracer (/Game/MonkeIsland/VFX/Tracers): unlit, additive, two sided; emissive = custom primitive data 0-2 (colour)
  x custom primitive data 3 (brightness): AFirearmTracer sets them per round, no material instance per shot;
- the Tracer look of every firearm's rules (UFirearmRules.Tracer) and of the French soldiers' muskets
  (UFortSoldierRules.MusketTracer): streak = /Engine/BasicShapes/Cylinder, round = the lead ball of the packs.
The rules part needs the T-276 C++ (property "tracer"): before the build it is skipped with a note.
Lead session action "tracers". Report: monke_island/Saved/T276/tracers.json. Replays update in place.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T276/tracers.json"
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary
FOLDER = "/Game/MonkeIsland/VFX/Tracers"
MATERIAL = f"{FOLDER}/M_Tracer"
STREAK = "/Engine/BasicShapes/Cylinder"
BALL = "/Game/MonkeIsland/Weapons/Musket/Static/SM_Ammo_LeadBall"
W = "/Game/MonkeIsland/Weapons"
# rules -> look (speed cm/s, length cm, width cm, colour, brightness, every nth, round size cm)
LOOKS = {
    f"{W}/Musket/DA_PirateMusketRules": (22000, 200, 4.0, (1.0, 0.45, 0.12), 9, 1, 3.0),
    f"{W}/Pistol/DA_PistolRules": (20000, 140, 3.0, (1.0, 0.45, 0.12), 8, 1, 2.5),
    f"{W}/DoublePistol/DA_DoublePistolRules": (20000, 140, 3.0, (1.0, 0.45, 0.12), 8, 1, 2.5),
    f"{W}/Rifle/DA_RifleRules": (30000, 240, 3.5, (1.0, 0.5, 0.15), 10, 1, 2.5),
    f"{W}/Shotgun/DA_ShotgunRules": (18000, 80, 2.0, (1.0, 0.5, 0.15), 7, 1, 1.5),
    f"{W}/Blunderbuss/DA_BlunderbussRules": (18000, 90, 2.2, (1.0, 0.5, 0.15), 7, 1, 1.6),
}
SOLDIERS = "/Game/MonkeIsland/Characters/BeanFR/Data/DA_FortSoldierRules"
SOLDIER_LOOK = (20000, 200, 4.0, (1.0, 0.18, 0.06), 9, 1, 3.0)


def make_material(report):
    material = EAL.load_asset(MATERIAL)
    if not material:
        if not EAL.does_directory_exist(FOLDER):
            EAL.make_directory(FOLDER)
        material = ue.AssetToolsHelpers.get_asset_tools().create_asset("M_Tracer", FOLDER, ue.Material, ue.MaterialFactoryNew())
        material.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("blend_mode", ue.BlendMode.BLEND_ADDITIVE)
        material.set_editor_property("two_sided", True)
        color = MEL.create_material_expression(material, ue.MaterialExpressionVectorParameter, -500, 0)
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", ue.LinearColor(1.0, 0.55, 0.18, 1.0))
        color.set_editor_property("use_custom_primitive_data", True)
        color.set_editor_property("primitive_data_index", 0)
        power = MEL.create_material_expression(material, ue.MaterialExpressionScalarParameter, -500, 200)
        power.set_editor_property("parameter_name", "Intensity")
        power.set_editor_property("default_value", 30.0)
        power.set_editor_property("use_custom_primitive_data", True)
        power.set_editor_property("primitive_data_index", 3)
        product = MEL.create_material_expression(material, ue.MaterialExpressionMultiply, -250, 80)
        MEL.connect_material_expressions(color, "", product, "A")
        MEL.connect_material_expressions(power, "", product, "B")
        MEL.connect_material_property(product, "", ue.MaterialProperty.MP_EMISSIVE_COLOR)
        MEL.recompile_material(material)
        report["material"] = "created"
    else:
        report["material"] = "kept"
    EAL.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def look_struct(current, look, material, streak, ball):
    speed, length, width, color, power, nth, size = look
    current.set_editor_property("enabled", True)
    current.set_editor_property("every_nth", nth)
    current.set_editor_property("speed_cmps", float(speed))
    current.set_editor_property("length_cm", float(length))
    current.set_editor_property("width_cm", float(width))
    current.set_editor_property("color", ue.LinearColor(color[0], color[1], color[2], 1.0))
    current.set_editor_property("intensity", float(power))
    current.set_editor_property("streak_mesh", streak)
    current.set_editor_property("streak_material", material)
    current.set_editor_property("round_mesh", ball)
    current.set_editor_property("round_size_cm", float(size))
    return current


def main():
    report = {"rules": {}}
    material = make_material(report)
    streak = EAL.load_asset(STREAK)
    ball = EAL.load_asset(BALL)
    report["streak_mesh"] = bool(streak)
    report["round_mesh"] = bool(ball)
    targets = [(path, "tracer", look) for path, look in LOOKS.items()] + [(SOLDIERS, "musket_tracer", SOLDIER_LOOK)]
    for path, prop, look in targets:
        rules = EAL.load_asset(path)
        if not rules:
            report["rules"][path] = "missing asset"
            continue
        try:
            current = rules.get_editor_property(prop)
        except Exception:  # noqa: BLE001
            report["rules"][path] = f"no property {prop} yet (T-276 C++ not built)"
            continue
        rules.set_editor_property(prop, look_struct(current, look, material, streak, ball))
        report["rules"][path] = "set" if EAL.save_loaded_asset(rules, only_if_is_dirty=False) else "SAVE FAILED"
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")


main()
