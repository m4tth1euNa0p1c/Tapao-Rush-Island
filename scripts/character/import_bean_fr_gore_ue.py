"""The textured French beans of the 2nd PC (maquette bean-francais-gore, 2026-10-07) on the garrison of the forts (owner
2026-10-08: "exporte les nouveaux skins sur les PNJ et mobs", "charge OneDrive, ajoute et remplace les skins"), in Unreal.

For the 7 uniforms (SK_BeanFR_<Skin>, on the skeleton of the player bean):
- the baked textures (art/export/Characters/BeanFRGore, copies of the maquette's exports): the common body (BaseColor,
  Normal, ORM, Emissive: red eyes) and each uniform with its headwear and worn kit (BaseColor, Normal, ORM 2048), in
  /Game/MonkeIsland/Characters/BeanFR/Textures; BaseColor sRGB, Normal OpenGL (green flipped), ORM linear masks;
- MI_BeanFR_Body (emissive x4) and MI_BeanFR_<Skin>_Uniform, children of M_Character_Baked (import_coco_real_ue.py);
- SK_BeanFR_<Skin> reimported IN PLACE from the new LOD0 (44 k tris instead of about 90 k) on its skeleton and physics
  asset (the blueprints, the garrison and the execution squad keep their references), then LOD1 / LOD2 (50 % / 20 %,
  the engine's screen sizes 0.5 / 0.25): the LODs the French beans were missing.
Runs in the editor: scripts/editor/lead_session_t186.py, action "beanfrgore" (editor lock held, no game running).
Writes monke_island/Saved/T186/bean-fr-gore.json.
"""
import datetime as dt
import json
import sys
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
sys.path.insert(0, str(ROOT / "scripts/character"))
import minka_doors_ue as kitlib  # noqa: E402
import import_coco_real_ue as coco  # noqa: E402

ART = ROOT / "art/export/Characters/BeanFRGore"
OUT = ROOT / "monke_island/Saved/T186/bean-fr-gore.json"
BASE = "/Game/MonkeIsland/Characters/BeanFR"
SKINS = ["CFM_Fusilier", "Fusilier_1767", "Fusilier_Bicorne", "Chasseur_Shako", "Grenadier", "Sergent", "Officier_Marine"]
LOD_SCREEN = {1: 0.42, 2: 0.18}
BODY_EMISSIVE = 4.0
report = {}


def texture(path, name, kind):
    tex = coco.import_task(path, f"{BASE}/Textures", name)
    assert tex, name
    if kind == "Normal":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)
    elif kind == "ORM":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    kitlib.save(tex)
    return tex


def instance(name, master, textures, emissive=0.0):
    folder = f"{BASE}/Materials"
    mi = kitlib.load(f"{folder}/{name}") or coco.TOOLS.create_asset(name, folder, ue.MaterialInstanceConstant,
                                                                     ue.MaterialInstanceConstantFactoryNew())
    coco.MEL.set_material_instance_parent(mi, master)
    for param, tex in textures.items():
        coco.MEL.set_material_instance_texture_parameter_value(mi, param, tex)
    coco.MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", emissive)
    kitlib.save(mi)
    return mi


def body_instance(master):
    folder = ART / "Common" / "Textures"
    textures = {param: texture(folder / f"T_BeanFR_Body_{kind}.png", f"T_BeanFR_Body_{kind}", kind)
                for param, kind in (("BaseColor", "BaseColor"), ("Normal", "Normal"), ("ORM", "ORM"), ("Emissive", "Emissive"))}
    return instance("MI_BeanFR_Body", master, textures, BODY_EMISSIVE)


def uniform_instance(skin, master):
    folder = ART / skin / "Textures"
    textures = {param: texture(folder / f"T_BeanFR_{skin}_Uniform_{kind}.png", f"T_BeanFR_{skin}_Uniform_{kind}", kind)
                for param, kind in (("BaseColor", "BaseColor"), ("Normal", "Normal"), ("ORM", "ORM"))}
    return instance(f"MI_BeanFR_{skin}_Uniform", master, textures)


def do_skin(skin, master, body_mi):
    path = f"{BASE}/Meshes/SK_BeanFR_{skin}"
    before = kitlib.load(path)
    assert before, path
    skeleton = before.get_editor_property("skeleton")
    uniform_mi = uniform_instance(skin, master)
    mis = {"M_BeanFR_Body": body_mi, f"M_BeanFR_{skin}_Uniform": uniform_mi}
    mesh = coco.reimport(path, ART / skin / "SK" / f"SK_BeanFR_{skin}.fbx", skeleton)
    sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    lods = []
    for lod, screen in LOD_SCREEN.items():
        try:
            lods.append([lod, sub.import_lod(mesh, lod, str(ART / skin / "LOD" / f"SK_BeanFR_{skin}_LOD{lod}.fbx"))])
        except Exception as error:  # noqa: BLE001
            lods.append([lod, f"failed: {error}"])
    slots = coco.assign(mesh, mis)
    kitlib.save(mesh)
    return {"slots": slots, "lods": lods, "lod_count": sub.get_lod_count(mesh), "verts_per_lod": coco.vertices(mesh),
            "skeleton": skeleton.get_path_name()}


def main():
    kitlib.check_lock()
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        master = coco.master_material()
        body_mi = body_instance(master)
        report["body_mi"] = body_mi.get_path_name()
        for skin in SKINS:
            try:
                report[skin] = do_skin(skin, master, body_mi)
            except Exception:  # noqa: BLE001
                report[skin] = {"error": traceback.format_exc()[-1200:]}
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")


if __name__ == "__main__":
    main()
