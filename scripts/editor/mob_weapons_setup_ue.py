"""T-161 (lot L5): the mobs' weapons, picked up at their death, become melee weapons of the bean.

A tiki warrior killed lets its weapon fall as a pickup (DroppedWeaponClass of its BP_Tiki*): "Ramasser <name>" with F,
then a melee weapon in an inventory slot, played with the cutlass's controls (left click combo, held for the long blow,
right click held to parry). For each weapon (spear of the CocoWarrior, obsidian club of the AshWatcher, shell hammer of
the CrabGuardian), in /Game/MonkeIsland/Weapons/MobWeapons:
- sounds SW_MobWeapon_* (scripts/props/mob_weapon_sounds.py, wood and stone, no steel; Effects sound class, the
  cutlass's attenuation) and icons T_Icon_<Weapon> (scripts/ui/render_mob_weapon_icons.py + finish_mob_weapon_icons.py);
- DA_<Weapon>Rules: a copy of DA_CutlassRules (its tracks, input and feel) with the weapon's mesh, no scabbard, its
  damage, reach, blade span measured on the mesh, sounds, and (after the T-161 build) bEdged, bShowWhenPutAway off and
  MeshInGrip (pitch -90: the meshes are in grip space with their long axis on +Z);
- BP_MobWeapon_<Weapon>: child of BP_Cutlass (same keys), Rules = DA_<Weapon>Rules;
- DA_Item_<Weapon>: name, short name, icon, item class, the drop's mesh and mass, PickupClass = BP_Pickup_<Weapon>;
- BP_Pickup_<Weapon> (after the T-161 build, class MobWeaponPickup): Definition = DA_Item_<Weapon>, the sale value of
  the BP_Drop_* it replaces (UValuableComponent, BaseValue and impact settings copied);
- BP_Tiki<Mob>.DroppedWeaponClass = BP_Pickup_<Weapon> (after the build; until then the BP_Drop_* stay).
Before the build the steps needing MobWeaponPickup or the new UCutlassRules properties are skipped and reported; run
the action again after the build. Idempotent (existing assets updated in place).
Runs in the editor: scripts/editor/lead_session_t186.py, action "mobweapons" (lock held, no game running), ~10 s.
Report: monke_island/Saved/T161/mob-weapons.json.
"""
import datetime as dt
import json
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T161"
DEST = "/Game/MonkeIsland/Weapons/MobWeapons"
CUTLASS = "/Game/MonkeIsland/Weapons/Cutlass"
DROPS = "/Game/MonkeIsland/Items/MobWeapons"
CHARS = "/Game/MonkeIsland/Characters"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
SOUND_SRC = ROOT / "art/audio/weapons"
ICON_SRC = ROOT / "art/ui/icons/mob_weapons"
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()

# property of UCutlassRules -> sound (shared by the three weapons)
SOUNDS = {
    "swing_sound": "SW_MobWeapon_Swing",
    "finisher_swing_sound": "SW_MobWeapon_HeavySwing",
    "hit_sound": "SW_MobWeapon_Hit",
    "kill_sound": "SW_MobWeapon_Kill",
    "parry_sound": "SW_MobWeapon_Parry",
    "draw_sound": "SW_MobWeapon_Draw",
    "sheathe_sound": "SW_MobWeapon_Sheathe",
}
ATTACKS = ("slash_right", "slash_left", "thrust", "finisher")
# Tuning of each weapon (the cutlass: 35 / 35 / 45 / 70, reach 100 cm, 8 % of the stamina a blow; a Coco Warrior has 80).
# attacks: damage, bCut, arc (deg), lunge (cm). head: share of the length, from the far end, that hits (the blade sweep).
WEAPONS = {
    "Spear": dict(
        mob="CocoWarrior", drop="BP_Drop_Spear", mesh="SM_CocoWarrior_Spear",
        display="Lance à dents de pierre", short="Lance",
        # stone teeth, no edge: no cut, no sabre reaction; long reach, the thrust is its blow
        edged=False,
        attacks={"slash_right": (28.0, False, 120.0, 30.0), "slash_left": (28.0, False, 120.0, 30.0),
                 "thrust": (50.0, False, 30.0, 45.0), "finisher": (60.0, False, 50.0, 0.0)},
        reach=150.0, stamina=0.08, head=0.35, sweep_radius=14.0, knockback=220.0, impulse=30000.0,
        sharpness_loss=0.0, parry=0.6),
    "ObsidianClub": dict(
        mob="AshWatcher", drop="BP_Drop_ObsidianClub", mesh="SM_AshWatcher_ObsidianClub",
        display="Massue d'obsidienne", short="Massue",
        # obsidian flakes cut like a blade (macuahuitl): its cuts take limbs off and the sabre reactions; they chip
        edged=True,
        attacks={"slash_right": (38.0, True, 110.0, 30.0), "slash_left": (38.0, True, 110.0, 30.0),
                 "thrust": (25.0, False, 40.0, 0.0), "finisher": (75.0, True, 60.0, 0.0)},
        reach=115.0, stamina=0.10, head=0.45, sweep_radius=16.0, knockback=260.0, impulse=35000.0,
        sharpness_loss=0.02, parry=0.7),
    "ShellHammer": dict(
        mob="CrabGuardian", drop="BP_Drop_ShellHammer", mesh="SM_CrabGuardian_ShellHammer",
        display="Marteau-coquillage", short="Marteau",
        # a heavy blunt head: no cut, strong knockback, the costliest blows
        edged=False,
        attacks={"slash_right": (40.0, False, 100.0, 25.0), "slash_left": (40.0, False, 100.0, 25.0),
                 "thrust": (22.0, False, 40.0, 0.0), "finisher": (85.0, False, 60.0, 0.0)},
        reach=105.0, stamina=0.11, head=0.35, sweep_radius=18.0, knockback=320.0, impulse=45000.0,
        sharpness_loss=0.0, parry=0.5),
}
# the static meshes are in grip space with their long axis on +Z: a pitch of -90 lays it along the blade (+X)
MESH_IN_GRIP = ue.Transform(location=ue.Vector(0.0, 0.0, 0.0), rotation=ue.Rotator(roll=0.0, pitch=-90.0, yaw=0.0),
                            scale=ue.Vector(1.0, 1.0, 1.0))
VALUABLE_PROPS = ("base_value", "safe_impact_speed", "heavy_impact_speed", "max_loss_per_hit", "hit_cooldown")

report = {"started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "sounds": {}, "icons": {}, "weapons": {},
          "needs_build": [], "warnings": []}


def warn(message):
    report["warnings"].append(message)
    ue.log_warning(f"[mob_weapons] {message}")


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        warn(f"could not save {asset.get_path_name()}")


def setp(obj, prop, value, build_prop=False):
    """Sets a property; one the T-161 build adds is reported (needs_build) instead of failing"""
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as error:  # noqa: BLE001
        if build_prop:
            report["needs_build"].append(f"{obj.get_name()}.{prop}")
        else:
            warn(f"{obj.get_name()}.{prop}: {error}")
        return False


def data_asset(path, cls):
    asset = load(path)
    if asset:
        return asset
    factory = ue.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    folder, name = path.rsplit("/", 1)
    return TOOLS.create_asset(name, folder, cls, factory)


def blueprint(path, parent):
    asset = load(path)
    if asset:
        return asset
    factory = ue.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    folder, name = path.rsplit("/", 1)
    return TOOLS.create_asset(name, folder, ue.Blueprint, factory)


def import_file(source, folder, name):
    task = ue.AssetImportTask()
    task.filename = str(source)
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    return load(f"{folder}/{name}")


def import_sounds():
    effects = load(EFFECTS_CLASS)
    sounds = {}
    for prop, name in SOUNDS.items():
        source = SOUND_SRC / f"{name}.wav"
        if not source.is_file():
            warn(f"{source} missing: python scripts/props/mob_weapon_sounds.py")
            continue
        sound = import_file(source, f"{DEST}/Audio", name)
        if not sound:
            warn(f"{name}: import failed")
            continue
        if effects:
            setp(sound, "sound_class_object", effects)
        save(sound)
        sounds[prop] = sound
        report["sounds"][prop] = sound.get_path_name().split(".")[0]
    return sounds


def import_icons():
    icons = {}
    for name in WEAPONS:
        source = ICON_SRC / f"T_Icon_{name}.png"
        if not source.is_file():
            warn(f"{source} missing: scripts/ui/render_mob_weapon_icons.py then finish_mob_weapon_icons.py")
            continue
        texture = import_file(source, f"{DEST}/Icons", f"T_Icon_{name}")
        if not texture:
            warn(f"T_Icon_{name}: import failed")
            continue
        # a picture of the interface: no mips, never streamed, full quality (as the v2 weapon icons)
        setp(texture, "compression_settings", ue.TextureCompressionSettings.TC_EDITOR_ICON)
        setp(texture, "lod_group", ue.TextureGroup.TEXTUREGROUP_UI)
        setp(texture, "mip_gen_settings", ue.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        setp(texture, "never_stream", True)
        setp(texture, "srgb", True)
        save(texture)
        icons[name] = texture
        report["icons"][name] = texture.get_path_name().split(".")[0]
    return icons


def mesh_span(mesh):
    """(min Z, max Z) of a static mesh in grip space (cm): the butt behind the grip, the head at the far end"""
    bounds = mesh.get_bounds()
    return bounds.origin.z - bounds.box_extent.z, bounds.origin.z + bounds.box_extent.z


def setup_rules(name, cfg, mesh, sounds, attenuation):
    path = f"{DEST}/DA_{name}Rules"
    rules = load(path)
    created = rules is None
    if created:
        rules = EAL.duplicate_asset(f"{CUTLASS}/DA_CutlassRules", path)
    if not rules:
        raise RuntimeError(f"could not create {path}")
    low, high = mesh_span(mesh)
    length = high - low
    blade_start = max(0.0, high - cfg["head"] * length)
    setp(rules, "cutlass_mesh", mesh)
    setp(rules, "scabbard_mesh", None)
    for prop in ATTACKS:
        damage, cut, arc, lunge = cfg["attacks"][prop]
        attack = rules.get_editor_property(prop)
        # native names: FindPropertyByName resolves them whatever the Python spelling of the bool
        for field, value in (("Damage", damage), ("bCut", cut), ("ArcDegrees", arc), ("LungeDistanceCm", lunge)):
            setp(attack, field, value)
        setp(rules, prop, attack)
    for prop, value in (("reach_cm", cfg["reach"]), ("stamina_cost_fraction", cfg["stamina"]),
                        ("knockback_speed", cfg["knockback"]), ("physics_impulse", cfg["impulse"]),
                        ("sharpness_loss_per_hit", cfg["sharpness_loss"]), ("parry_reduction", cfg["parry"]),
                        ("blade_start_cm", round(blade_start, 1)), ("blade_tip_cm", round(high, 1)),
                        ("blade_sweep_radius_cm", cfg["sweep_radius"]),
                        ("first_person_hilt_cm", round(max(0.0, -low) + 2.0, 1))):
        setp(rules, prop, value)
    for prop, sound in sounds.items():
        setp(rules, prop, sound)
    if attenuation:
        setp(rules, "sound_attenuation", attenuation)
    # T-161 properties (after the build)
    setp(rules, "bEdged", cfg["edged"], build_prop=True)
    setp(rules, "bShowWhenPutAway", False, build_prop=True)
    setp(rules, "MeshInGrip", MESH_IN_GRIP, build_prop=True)
    save(rules)
    return rules, {"asset": path, "created": created, "mesh_span_cm": [round(low, 1), round(high, 1)],
                   "blade_cm": [round(blade_start, 1), round(high, 1)], "first_person_hilt_cm": round(max(0.0, -low) + 2.0, 1),
                   "attacks": {k: list(v) for k, v in cfg["attacks"].items()}, "reach_cm": cfg["reach"], "edged": cfg["edged"]}


def setup_weapon(name, cfg, rules, cutlass_bp):
    bp = blueprint(f"{DEST}/BP_MobWeapon_{name}", cutlass_bp.generated_class())
    ue.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = ue.get_default_object(bp.generated_class())
    cdo.set_editor_property("rules", rules)
    ue.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    return bp


def setup_item(name, cfg, bp, mesh, icon, drop_cdo):
    item = data_asset(f"{DEST}/DA_Item_{name}", ue.InventoryItemDefinition)
    setp(item, "display_name", ue.Text(cfg["display"]))
    setp(item, "short_name", ue.Text(cfg["short"]))
    if icon:
        setp(item, "icon", icon)
    setp(item, "max_stack", 1)
    setp(item, "item_class", bp.generated_class())
    setp(item, "pickup_mesh", mesh)
    if drop_cdo:
        setp(item, "pickup_mass_kg", float(drop_cdo.get_editor_property("mass_kg")))
    save(item)
    return item


def setup_pickup(name, item, drop_cdo):
    """BP_Pickup_<Weapon>: class MobWeaponPickup (T-161 build), the item and the sale value of the BP_Drop_*"""
    cls = getattr(ue, "MobWeaponPickup", None)
    if cls is None:
        report["needs_build"].append(f"BP_Pickup_{name} (class MobWeaponPickup)")
        return None
    bp = blueprint(f"{DEST}/BP_Pickup_{name}", cls)
    ue.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = ue.get_default_object(bp.generated_class())
    cdo.set_editor_property("definition", item)
    values = {}
    if drop_cdo:
        source = drop_cdo.get_editor_property("valuable")
        target = cdo.get_editor_property("valuable")
        for prop in VALUABLE_PROPS:
            value = source.get_editor_property(prop)
            target.set_editor_property(prop, value)
            values[prop] = value
    ue.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    setp(item, "pickup_class", bp.generated_class())
    save(item)
    return bp, values


def main():
    try:
        cutlass_bp = load(f"{CUTLASS}/BP_Cutlass")
        assert cutlass_bp, f"{CUTLASS}/BP_Cutlass missing"
        attenuation = load(f"{CUTLASS}/Audio/ATT_Cutlass")
        sounds = import_sounds()
        icons = import_icons()
        for name, cfg in WEAPONS.items():
            entry = {}
            mesh = load(f"{DROPS}/Meshes/{cfg['mesh']}")
            if not mesh:
                warn(f"{name}: mesh {DROPS}/Meshes/{cfg['mesh']} missing, skipped")
                continue
            drop_bp = load(f"{DROPS}/{cfg['drop']}")
            drop_cdo = ue.get_default_object(drop_bp.generated_class()) if drop_bp else None
            rules, entry["rules"] = setup_rules(name, cfg, mesh, sounds, attenuation)
            bp = setup_weapon(name, cfg, rules, cutlass_bp)
            entry["weapon"] = bp.get_path_name().split(".")[0]
            item = setup_item(name, cfg, bp, mesh, icons.get(name), drop_cdo)
            entry["item"] = item.get_path_name().split(".")[0]
            pickup = setup_pickup(name, item, drop_cdo)
            if pickup:
                pickup_bp, values = pickup
                entry["pickup"] = pickup_bp.get_path_name().split(".")[0]
                entry["value"] = values
                tiki_path = f"{CHARS}/{cfg['mob']}/Blueprints/BP_Tiki{cfg['mob']}"
                generated = EAL.load_blueprint_class(tiki_path) if EAL.does_asset_exist(tiki_path) else None
                if generated:
                    tiki = ue.get_default_object(generated)
                    before = tiki.get_editor_property("dropped_weapon_class")
                    tiki.set_editor_property("dropped_weapon_class", pickup_bp.generated_class())
                    EAL.save_asset(tiki_path, only_if_is_dirty=False)
                    entry["mob"] = {"blueprint": tiki_path, "dropped_before": before.get_name() if before else None,
                                    "dropped_now": pickup_bp.generated_class().get_name()}
                else:
                    warn(f"{name}: no Blueprint {tiki_path}")
            entry["drop_mass_kg"] = float(drop_cdo.get_editor_property("mass_kg")) if drop_cdo else None
            report["weapons"][name] = entry
            ue.log(f"[mob_weapons] {name}: {json.dumps(entry, default=str)}")
        report["passed"] = not report["warnings"]
    except Exception:  # noqa: BLE001
        report["error"] = traceback.format_exc()[-3000:]
        report["passed"] = False
    finally:
        report["needs_build"] = sorted(set(report["needs_build"]))
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "mob-weapons.json").write_text(json.dumps(report, indent=1, default=str, ensure_ascii=False), encoding="utf-8")
        ue.log(f"[mob_weapons] {'ok' if report.get('passed') else 'FAILED'}; needs build: {report['needs_build']}")
    if "error" in report:
        raise RuntimeError(report["error"])


if __name__ == "__main__":
    main()
