"""Owner requests of 2026-10-08 evening, in the editor (action "ownerbatch1008"); parts read from
monke_island/Saved/T186/ownerbatch1008-request.json ({"parts": [...]}), each reported on its own:

- zoneanims: the new GoreBox clips by zone of the body on the game ("ajoute et remplace les skins et nouvelles
  animations"): BP_TikiCocoWarrior and BP_TikiVillager (Anims.HitHead / HitTorso / HitArmL / HitArmR / HitLegL / HitLegR =
  A_<Mob>_Hit_*), DA_FortSoldierRules (HitHead = A_BeanFR_Hit_Head_Front, HitTorso = A_BeanFR_Hit_Torso_Front, arms, legs);
- moat_testflat: an AFortMoat by the fort of L_Test_Flat ("quand je tombe dans les douves du fort français je meurs dans
  l'eau"), on its walls (FortFR_Garrison) and its water (Fort_T198_SM_Fort_MoatWater); the level saved;
- testflat_site: a place for the land fort on the island of the generators of L_Test_Flat (its section of Raraka:
  "ajoute le fort français dans la map, section réservée à Raraka, et ajoute la génération du fort"): ground traced on a
  10 m grid, flat over 45 m, away from the villages of its fragment and the marae, the gate toward the player start; it
  goes in DA_FortSiteRules as FRG_TestFlat with TestMap = L_Test_Flat (UFortSiteSubsystem builds it at the start);
- moat_li: the same AFortMoat in LI_FortFrench_Land (loaded, saved, then L_Test_Flat loaded back);
- gore2: the hat and head pieces of the French soldiers and the GoreBox deaths (owner 2026-10-08 evening);
- gore3: the limbs of the French soldiers (owner 2026-10-09, "il manque les animations de démembrement"): the four limbs
  and the upper torso of each uniform copied from OneDrive, imported, put on the seven blueprints, the crawl / panic /
  one-armed / bisected clips and the limb values on DA_FortSoldierRules;
- gore3_tiki: the crawl, panic, writhe, crawl death and bisected death clips of the Coco Warrior and the Villager.
Writes monke_island/Saved/T186/ownerbatch1008.json.
"""
import datetime as dt
import json
import math
import shutil
import sys
import time
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

REQUEST = ROOT / "monke_island/Saved/T186/ownerbatch1008-request.json"
OUT = ROOT / "monke_island/Saved/T186/ownerbatch1008.json"
CHARS = "/Game/MonkeIsland/Characters"
TEST_FLAT = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
LAND = "/Game/MonkeIsland/Maps/Fragments/LI_FortFrench_Land"
SITE_RULES = "/Game/MonkeIsland/Environment/FortFrench/Data/DA_FortSiteRules"
SOLDIER_RULES = f"{CHARS}/BeanFR/Data/DA_FortSoldierRules"
ZONES = {"hit_head": "Head", "hit_torso": "Torso", "hit_arm_l": "Arm_L", "hit_arm_r": "Arm_R", "hit_leg_l": "Leg_L", "hit_leg_r": "Leg_R"}
BEAN_ZONES = {"hit_head": "Head_Front", "hit_torso": "Torso_Front", "hit_arm_l": "Arm_L", "hit_arm_r": "Arm_R", "hit_leg_l": "Leg_L",
              "hit_leg_r": "Leg_R"}
MOAT_LABEL = "FortFR_Moat"
FORT_FLAT_RADIUS = 4500.0
FORT_FLAT_DELTA = 150.0
VILLAGE_MARGIN = 6000.0
MARAE_CLEAR = 10000.0
EAL = ue.EditorAssetLibrary
report = {}


def actors():
    return ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()


def by_label(label):
    return next((a for a in actors() if a.get_actor_label() == label), None)


def world():
    return ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()


def zoneanims():
    out = {}
    for mob, bp_path in (("CocoWarrior", f"{CHARS}/CocoWarrior/Blueprints/BP_TikiCocoWarrior"),
                         ("Villager", f"{CHARS}/Villager/Blueprints/BP_TikiVillager")):
        generated = EAL.load_blueprint_class(bp_path)
        cdo = ue.get_default_object(generated)
        anims = cdo.get_editor_property("anims")
        set_ = {}
        for field, zone in ZONES.items():
            clip = EAL.load_asset(f"{CHARS}/{mob}/Anims/Gore/A_{mob}_Hit_{zone}")
            assert isinstance(clip, ue.AnimSequenceBase), f"{mob} {zone}"
            anims.set_editor_property(field, clip)
            set_[field] = clip.get_name()
        cdo.set_editor_property("anims", anims)
        assert EAL.save_asset(bp_path, only_if_is_dirty=False), bp_path
        check = ue.get_default_object(EAL.load_blueprint_class(bp_path)).get_editor_property("anims").get_editor_property("hit_head")
        out[mob] = {"set": set_, "check_hit_head": check.get_name() if check else None}
    rules = kitlib.load(SOLDIER_RULES)
    set_ = {}
    for field, zone in BEAN_ZONES.items():
        clip = EAL.load_asset(f"{CHARS}/BeanFR/Animations/Gore/A_BeanFR_Hit_{zone}")
        assert isinstance(clip, ue.AnimSequence), f"BeanFR {zone}"
        rules.set_editor_property(field, clip)
        set_[field] = clip.get_name()
    kitlib.save(rules)
    out["DA_FortSoldierRules"] = set_
    return out


def place_moat():
    garrison = by_label("FortFR_Garrison")
    water = by_label("Fort_T198_SM_Fort_MoatWater")
    assert garrison and water, f"garrison {garrison}, water {water}"
    eas = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for old in [a for a in actors() if a.get_actor_label() == MOAT_LABEL]:
        eas.destroy_actor(old)
    moat = eas.spawn_actor_from_class(ue.FortMoat, garrison.get_actor_location(), ue.Rotator(0.0, 0.0, 0.0))
    moat.set_actor_label(MOAT_LABEL)
    moat.set_folder_path(str(garrison.get_folder_path()))
    moat.set_editor_property("garrison", garrison)
    moat.set_editor_property("water", water)
    o, e = water.get_actor_bounds(False)
    return {"moat": moat.get_name(), "garrison": garrison.get_actor_label(), "water_top_z": round(o.z + e.z),
            "water_extent_m": [round(e.x / 100), round(e.y / 100)], "saved": ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()}


def moat_testflat():
    assert world().get_path_name().startswith(TEST_FLAT), world().get_path_name()
    return place_moat()


def moat_li():
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert levels.load_level(LAND), LAND
    try:
        return place_moat()
    finally:
        levels.load_level(TEST_FLAT)


def ground(x, y, island):
    hit = ue.SystemLibrary.line_trace_single(world(), ue.Vector(x, y, 20000.0), ue.Vector(x, y, -20000.0),
                                             ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], ue.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    if not t or not t[0]:
        return None
    comp = t[10]
    owner = comp.get_owner() if comp else None
    return t[4].z if owner == island else None


def testflat_site():
    assert world().get_path_name().startswith(TEST_FLAT), world().get_path_name()
    island = by_label("TestFlat_Ile_Generateurs")
    assert island, "no TestFlat_Ile_Generateurs"
    origin, extent = island.get_actor_bounds(False)
    start = next((a for a in actors() if isinstance(a, ue.PlayerStart)), None)
    # the villages of the fragment of the test map and the marae
    avoid = []
    spawner = next((a for a in actors() if a.get_class().get_name().startswith("BP_VillageSpawner")), None)
    fragment_id = "FRG_TestFlat"
    if spawner:
        for entry in spawner.get_editor_property("fragments"):
            soft = entry.get_editor_property("set")
            vset = soft if isinstance(soft, ue.Object) else EAL.load_asset(str(soft))
            if vset is None:
                try:
                    vset = ue.SystemLibrary.load_asset_blocking(soft)
                except Exception:  # noqa: BLE001
                    vset = None
            fragment_id = str(entry.get_editor_property("fragment_id")) or fragment_id
            for village in (vset.get_editor_property("villages") if vset else []):
                c = village.get_editor_property("center")
                avoid.append((c.x, c.y, village.get_editor_property("radius") + VILLAGE_MARGIN))
    for marae in [a for a in actors() if isinstance(a, ue.MaraeSite)]:
        loc = marae.get_actor_location()
        avoid.append((loc.x, loc.y, MARAE_CLEAR))
    best = None
    step = 1000.0
    x = origin.x - extent.x + FORT_FLAT_RADIUS
    tried = 0
    while x <= origin.x + extent.x - FORT_FLAT_RADIUS:
        y = origin.y - extent.y + FORT_FLAT_RADIUS
        while y <= origin.y + extent.y - FORT_FLAT_RADIUS:
            tried += 1
            if all(math.hypot(x - ax, y - ay) > r for ax, ay, r in avoid):
                z0 = ground(x, y, island)
                if z0 is not None:
                    heights = [z0]
                    ok = True
                    for k in range(8):
                        a = 2 * math.pi * k / 8
                        for radius in (FORT_FLAT_RADIUS / 3, 2 * FORT_FLAT_RADIUS / 3, FORT_FLAT_RADIUS):
                            z = ground(x + math.cos(a) * radius, y + math.sin(a) * radius, island)
                            if z is None:
                                ok = False
                                break
                            heights.append(z)
                        if not ok:
                            break
                    if ok and max(heights) - min(heights) <= FORT_FLAT_DELTA:
                        clearance = min((math.hypot(x - ax, y - ay) - r for ax, ay, r in avoid), default=1e9)
                        score = (max(heights) - min(heights)) - clearance * 0.001
                        if best is None or score < best[0]:
                            best = (score, x, y, z0, max(heights) - min(heights), clearance)
            y += step
        x += step
    assert best, f"no flat place on the island ({tried} tried, {len(avoid)} zones to avoid)"
    _, sx, sy, sz, relief, clearance = best
    yaw = 0.0
    if start:
        dx, dy = start.get_actor_location().x - sx, start.get_actor_location().y - sy
        yaw = math.degrees(math.atan2(-dx, dy))
    rules = kitlib.load(SITE_RULES)
    entries = [e for e in rules.get_editor_property("fragments") if str(e.get_editor_property("fragment_id")) != fragment_id]
    entry = ue.FortFragmentSites()
    entry.set_editor_property("fragment_id", fragment_id)
    spot = ue.FortSiteSpot()
    spot.set_editor_property("location", ue.Vector(sx, sy, sz))
    spot.set_editor_property("yaw", yaw)
    entry.set_editor_property("sites", [spot])
    entry.set_editor_property("test_map", EAL.load_asset(TEST_FLAT))
    entries.append(entry)
    rules.set_editor_property("fragments", entries)
    kitlib.save(rules)
    return {"fragment": fragment_id, "site_m": [round(sx / 100), round(sy / 100), round(sz / 100, 2)], "yaw": round(yaw, 1),
            "relief_cm": round(relief), "clearance_m": round(clearance / 100), "tried": tried, "avoid": len(avoid),
            "island_m": [round((origin.x - extent.x) / 100), round((origin.x + extent.x) / 100), round((origin.y - extent.y) / 100),
                         round((origin.y + extent.y) / 100)]}


def gorewire():
    """The other new clips the game plays (owner 2026-10-08: "alors, les nouvelles animations"): the heavy stagger of the
    tikis (A_<Mob>_Stagger_Heavy) and their death by a shot in the head (Combat.HeadshotDeath = A_<Mob>_Death_Headshot);
    the death of the French soldiers by a shot in the head (DA_FortSoldierRules.DeathHeadshot)"""
    out = {}
    for mob, bp_path in (("CocoWarrior", f"{CHARS}/CocoWarrior/Blueprints/BP_TikiCocoWarrior"),
                         ("Villager", f"{CHARS}/Villager/Blueprints/BP_TikiVillager")):
        cdo = ue.get_default_object(EAL.load_blueprint_class(bp_path))
        anims = cdo.get_editor_property("anims")
        stagger = EAL.load_asset(f"{CHARS}/{mob}/Anims/Gore/A_{mob}_Stagger_Heavy")
        headshot = EAL.load_asset(f"{CHARS}/{mob}/Anims/Gore/A_{mob}_Death_Headshot")
        assert stagger and headshot, mob
        anims.set_editor_property("stagger", stagger)
        combat = anims.get_editor_property("combat")
        combat.set_editor_property("headshot_death", headshot)
        anims.set_editor_property("combat", combat)
        cdo.set_editor_property("anims", anims)
        assert EAL.save_asset(bp_path, only_if_is_dirty=False), bp_path
        check = ue.get_default_object(EAL.load_blueprint_class(bp_path)).get_editor_property("anims")
        out[mob] = {"stagger": check.get_editor_property("stagger").get_name(),
                    "headshot_death": check.get_editor_property("combat").get_editor_property("headshot_death").get_name()}
    rules = kitlib.load(SOLDIER_RULES)
    death = EAL.load_asset(f"{CHARS}/BeanFR/Animations/Gore/A_BeanFR_Death_Headshot")
    assert death, "A_BeanFR_Death_Headshot"
    rules.set_editor_property("death_headshot", death)
    kitlib.save(rules)
    out["DA_FortSoldierRules"] = {"death_headshot": rules.get_editor_property("death_headshot").get_name()}
    return out


SKINS = ("CFM_Fusilier", "Fusilier_1767", "Fusilier_Bicorne", "Chasseur_Shako", "Grenadier", "Sergent", "Officier_Marine")
GORE_ART = ROOT / "art/export/Characters/BeanFRGore"


def piece_task(skin, part):
    """Static mesh of a piece of a French bean (hat, head: SM_BeanFR_<Skin>_<Part>.fbx with its UCX), no materials"""
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", True)
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", False)
    task = ue.AssetImportTask()
    task.filename = str(GORE_ART / skin / "Gore" / f"SM_BeanFR_{skin}_{part}.fbx")
    task.destination_path = f"{CHARS}/BeanFR/Meshes/Gore"
    task.destination_name = f"SM_BeanFR_{skin}_{part}"
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = options
    return task


def gore2():
    """The GoreBox deaths and reactions of the French soldiers and the tikis (owner 2026-10-08: "les animations type gorebox
    ne sont pas la"): the hat and head pieces imported (SM_BeanFR_<Skin>_Hat / _Head, flown off by AFortSoldier), their
    materials, DA_FortSoldierRules (DeathDecapitated, DeathExplode, KnockbackMusket, HatKnockedOff, BloodRules), the pieces on
    the seven blueprints, and the decapitated death of the tikis (Combat.DecapitatedDeath)"""
    out = {"pieces": {}}
    tools = ue.AssetToolsHelpers.get_asset_tools()
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        tools.import_asset_tasks([piece_task(skin, part) for skin in SKINS for part in ("Hat", "Head")])
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
    body_mi = EAL.load_asset(f"{CHARS}/BeanFR/Materials/MI_BeanFR_Body")
    cap_mi = EAL.load_asset(f"{CHARS}/Villager/Materials/MI_Villager_Gore") or body_mi
    for skin in SKINS:
        uniform_mi = EAL.load_asset(f"{CHARS}/BeanFR/Materials/MI_BeanFR_{skin}_Uniform")
        for part in ("Hat", "Head"):
            path = f"{CHARS}/BeanFR/Meshes/Gore/SM_BeanFR_{skin}_{part}"
            mesh = EAL.load_asset(path)
            if not mesh:
                out["pieces"][f"{skin}_{part}"] = "import failed"
                continue
            # the slots by the material each one imported (the array's items are copies in 5.8)
            slots = list(mesh.get_editor_property("static_materials"))
            names = []
            for slot in slots:
                imported = str(slot.get_editor_property("imported_material_slot_name"))
                mi = body_mi if imported == "M_BeanFR_Body" else (cap_mi if "GoreCap" in imported else uniform_mi)
                slot.set_editor_property("material_interface", mi)
                names.append([imported, mi.get_name() if mi else None])
            mesh.set_editor_property("static_materials", slots)
            bounds = mesh.get_bounds()
            out["pieces"][f"{skin}_{part}"] = {"slots": names, "bounds_origin_cm": [round(bounds.origin.x), round(bounds.origin.y), round(bounds.origin.z)],
                                              "extent_cm": [round(bounds.box_extent.x), round(bounds.box_extent.y), round(bounds.box_extent.z)],
                                              "collision": mesh.get_editor_property("body_setup") is not None}
            assert EAL.save_asset(path, only_if_is_dirty=False), path
        # the pieces on the blueprint of the skin
        bp_path = f"{CHARS}/BeanFR/Blueprints/BP_BeanFR_{skin}"
        cdo = ue.get_default_object(EAL.load_blueprint_class(bp_path))
        cdo.set_editor_property("hat_mesh", EAL.load_asset(f"{CHARS}/BeanFR/Meshes/Gore/SM_BeanFR_{skin}_Hat"))
        cdo.set_editor_property("head_mesh", EAL.load_asset(f"{CHARS}/BeanFR/Meshes/Gore/SM_BeanFR_{skin}_Head"))
        assert EAL.save_asset(bp_path, only_if_is_dirty=False), bp_path
        check = ue.get_default_object(EAL.load_blueprint_class(bp_path))
        out[skin] = {"hat": check.get_editor_property("hat_mesh").get_name() if check.get_editor_property("hat_mesh") else None,
                     "head": check.get_editor_property("head_mesh").get_name() if check.get_editor_property("head_mesh") else None}
    # the rules of the soldiers
    rules = kitlib.load(SOLDIER_RULES)
    gore = f"{CHARS}/BeanFR/Animations/Gore"
    for prop, clip in (("death_decapitated", "A_BeanFR_Death_Decapitated"), ("death_explode", "A_BeanFR_Death_Explode"),
                       ("knockback_musket", "A_BeanFR_Knockback_Musket"), ("hat_knocked_off", "A_BeanFR_Hat_Knocked_Off")):
        asset = EAL.load_asset(f"{gore}/{clip}")
        assert asset, clip
        rules.set_editor_property(prop, asset)
    blood = EAL.load_asset(f"{CHARS}/Data/DA_TikiRules")
    assert blood, "DA_TikiRules"
    rules.set_editor_property("blood_rules", blood)
    kitlib.save(rules)
    out["DA_FortSoldierRules"] = {prop: rules.get_editor_property(prop).get_name() for prop in
                                  ("death_decapitated", "death_explode", "knockback_musket", "hat_knocked_off", "blood_rules")}
    # the tikis: the decapitated death
    for mob, bp_path in (("CocoWarrior", f"{CHARS}/CocoWarrior/Blueprints/BP_TikiCocoWarrior"), ("Villager", f"{CHARS}/Villager/Blueprints/BP_TikiVillager")):
        cdo = ue.get_default_object(EAL.load_blueprint_class(bp_path))
        anims = cdo.get_editor_property("anims")
        clip = EAL.load_asset(f"{CHARS}/{mob}/Anims/Gore/A_{mob}_Death_Decapitated")
        assert clip, mob
        combat = anims.get_editor_property("combat")
        combat.set_editor_property("decapitated_death", clip)
        anims.set_editor_property("combat", combat)
        cdo.set_editor_property("anims", anims)
        assert EAL.save_asset(bp_path, only_if_is_dirty=False), bp_path
        check = ue.get_default_object(EAL.load_blueprint_class(bp_path)).get_editor_property("anims").get_editor_property("combat")
        out[mob] = {"decapitated_death": check.get_editor_property("decapitated_death").get_name()}
    return out


ONEDRIVE_GORE = Path(r"C:\Users\matth\OneDrive\maquette-blender\maquettes\bean-francais-gore")
LIMBS = ("Arm_L", "Arm_R", "Leg_L", "Leg_R", "Torso_Upper")
LIMB_PROPS = {"Arm_L": "arm_l_mesh", "Arm_R": "arm_r_mesh", "Leg_L": "leg_l_mesh", "Leg_R": "leg_r_mesh", "Torso_Upper": "torso_upper_mesh"}
SOLDIER_GORE_CLIPS = {"legless_crawl_loop": "A_BeanFR_Legless_Crawl_Loop", "legless_crawl_death": "A_BeanFR_Legless_Crawl_Death",
                      "writhe_ground_loop": "A_BeanFR_Writhe_Ground_Loop", "armless_panic_loop": "A_BeanFR_Armless_Panic_Loop",
                      "flee_loop": "A_BeanFR_Flee_Loop", "death_bisected": "A_BeanFR_Death_Bisected"}
SOLDIER_LIMB_VALUES = {"limb_health_arm": 25.0, "limb_health_leg": 30.0, "crawl_speed": 40.0, "crawl_bleed_out_seconds": 6.0,
                       "crawl_death_health": 15.0, "bisect_chance": 0.5, "crawl_no_slide_speed": 18.0, "crawl_capsule_half_height": 30.0,
                       "bisect_time": 0.05}
MUSKET_DROP = "/Game/MonkeIsland/Weapons/Musket/Static/SM_Bean_Musket"
TIKI_GORE_ANIMS = {"legless_crawl": "Legless_Crawl_Loop", "writhe_ground": "Writhe_Ground_Loop", "armless_panic": "Armless_Panic_Loop"}
TIKI_GORE_COMBAT = {"legless_crawl_death": "Legless_Crawl_Death", "bisected_death": "Death_Bisected"}


def synced_copy(source, target):
    """Copies a OneDrive export once it is fully synchronised (same size twice, a second apart); the source is never
    touched (the folder is read only on this side), the copy carries no attribute of it"""
    size = source.stat().st_size
    time.sleep(1.0)
    assert size > 0 and source.stat().st_size == size, f"{source} still syncing ({size} bytes)"
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.stat().st_size != size:
        shutil.copyfile(source, target)
    return size


def piece_materials(mesh, skin, body_mi, cap_mi):
    """Assigns the materials of a piece by the material each slot imported (the array's items are copies in 5.8):
    the common body, the uniform of the skin, the gore cap (the closed cut)"""
    uniform_mi = EAL.load_asset(f"{CHARS}/BeanFR/Materials/MI_BeanFR_{skin}_Uniform")
    slots = list(mesh.get_editor_property("static_materials"))
    names = []
    for slot in slots:
        imported = str(slot.get_editor_property("imported_material_slot_name"))
        mi = body_mi if imported == "M_BeanFR_Body" else (cap_mi if "GoreCap" in imported else uniform_mi)
        slot.set_editor_property("material_interface", mi)
        names.append([imported, mi.get_name() if mi else None])
    mesh.set_editor_property("static_materials", slots)
    return names


def armless_hit_windows():
    """Hit windows (s) of A_BeanFR_Armless_Fight_On from the manifest of the maquette (pairs AN_HitWindow_Begin / _End);
    the three windows of the maquette of 2026-10-07 when the manifest cannot be read"""
    try:
        manifest = json.loads((ONEDRIVE_GORE / "ue/BeanFR_GoreManifest.json").read_text(encoding="utf-8"))
        clip = next(a for a in manifest["anims"] if a.get("asset") == "A_BeanFR_Armless_Fight_On")
        windows, start = [], None
        for notify in clip.get("notifies", []):
            if notify["name"] == "AN_HitWindow_Begin":
                start = float(notify["time_s"])
            elif notify["name"] == "AN_HitWindow_End" and start is not None:
                windows.append((start, float(notify["time_s"])))
                start = None
        assert windows
        return windows
    except Exception:  # noqa: BLE001
        return [(0.167, 0.292), (0.583, 0.708), (1.0, 1.125)]


def gore3():
    """The limbs of the French soldiers (owner 2026-10-09: "il manque les animations de démembrement des soldats français"):
    SM_BeanFR_<Skin>_{Arm_L,Arm_R,Leg_L,Leg_R,Torso_Upper}.fbx (pivot at the socket GoreLimb_<Module>, UCX) copied from
    OneDrive into art/export/Characters/BeanFRGore/<Skin>/Gore (like the hats), imported into Characters/BeanFR/Meshes/Gore
    with their materials, put on the seven blueprints (ArmLMesh, ArmRMesh, LegLMesh, LegRMesh, TorsoUpperMesh), and on
    DA_FortSoldierRules the GoreBox clips of the wounds (crawl loop and death, writhe, panic, flee, bisected death, the
    one-armed blow ArmlessFightOn with the windows of the manifest), the limb values and the musket that falls"""
    out = {"copied": {}, "pieces": {}}
    for skin in SKINS:
        for part in LIMBS:
            source = ONEDRIVE_GORE / "exports" / skin / "Dismember" / f"SM_BeanFR_{skin}_{part}.fbx"
            out["copied"][f"{skin}_{part}"] = synced_copy(source, GORE_ART / skin / "Gore" / source.name)
    tools = ue.AssetToolsHelpers.get_asset_tools()
    editor_world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(editor_world, flag + " 0")
    try:
        tools.import_asset_tasks([piece_task(skin, part) for skin in SKINS for part in LIMBS])
    finally:
        ue.SystemLibrary.execute_console_command(editor_world, flag + " " + str(previous))
    body_mi = EAL.load_asset(f"{CHARS}/BeanFR/Materials/MI_BeanFR_Body")
    cap_mi = EAL.load_asset(f"{CHARS}/Villager/Materials/MI_Villager_Gore") or body_mi
    for skin in SKINS:
        pieces = {}
        for part in LIMBS:
            path = f"{CHARS}/BeanFR/Meshes/Gore/SM_BeanFR_{skin}_{part}"
            mesh = EAL.load_asset(path)
            if not mesh:
                out["pieces"][f"{skin}_{part}"] = "import failed"
                continue
            names = piece_materials(mesh, skin, body_mi, cap_mi)
            bounds = mesh.get_bounds()
            out["pieces"][f"{skin}_{part}"] = {"slots": names, "bounds_origin_cm": [round(bounds.origin.x), round(bounds.origin.y), round(bounds.origin.z)],
                                              "extent_cm": [round(bounds.box_extent.x), round(bounds.box_extent.y), round(bounds.box_extent.z)],
                                              "collision": mesh.get_editor_property("body_setup") is not None}
            assert EAL.save_asset(path, only_if_is_dirty=False), path
            pieces[part] = mesh
        # the pieces on the blueprint of the skin
        bp_path = f"{CHARS}/BeanFR/Blueprints/BP_BeanFR_{skin}"
        cdo = ue.get_default_object(EAL.load_blueprint_class(bp_path))
        for part, mesh in pieces.items():
            cdo.set_editor_property(LIMB_PROPS[part], mesh)
        assert EAL.save_asset(bp_path, only_if_is_dirty=False), bp_path
        check = ue.get_default_object(EAL.load_blueprint_class(bp_path))
        out[skin] = {part: (check.get_editor_property(prop).get_name() if check.get_editor_property(prop) else None) for part, prop in LIMB_PROPS.items()}
    # the rules of the soldiers: the clips of the wounds, the one-armed blow, the limb values, the musket that falls
    rules = kitlib.load(SOLDIER_RULES)
    gore = f"{CHARS}/BeanFR/Animations/Gore"
    for prop, clip in SOLDIER_GORE_CLIPS.items():
        asset = EAL.load_asset(f"{gore}/{clip}")
        assert asset, clip
        rules.set_editor_property(prop, asset)
    fight_on = rules.get_editor_property("armless_fight_on")
    model = rules.get_editor_property("slash_right")
    for prop in ("reach_cm", "damage", "cone_degrees", "recovery", "weight", "lunge_speed"):
        fight_on.set_editor_property(prop, model.get_editor_property(prop))
    clip = EAL.load_asset(f"{gore}/A_BeanFR_Armless_Fight_On")
    assert clip, "A_BeanFR_Armless_Fight_On"
    fight_on.set_editor_property("clip", clip)
    windows = armless_hit_windows()
    fight_on.set_editor_property("hit_windows", [ue.Vector2D(a, b) for a, b in windows])
    rules.set_editor_property("armless_fight_on", fight_on)
    for prop, value in SOLDIER_LIMB_VALUES.items():
        rules.set_editor_property(prop, value)
    musket = EAL.load_asset(MUSKET_DROP)
    if musket:
        rules.set_editor_property("musket_drop_mesh", musket)
    kitlib.save(rules)
    out["DA_FortSoldierRules"] = {prop: rules.get_editor_property(prop).get_name() for prop in SOLDIER_GORE_CLIPS}
    out["DA_FortSoldierRules"].update({prop: rules.get_editor_property(prop) for prop in SOLDIER_LIMB_VALUES})
    out["DA_FortSoldierRules"]["armless_fight_on"] = {"clip": rules.get_editor_property("armless_fight_on").get_editor_property("clip").get_name(),
                                                     "hit_windows": windows}
    out["DA_FortSoldierRules"]["musket_drop_mesh"] = musket.get_name() if musket else None
    return out


def gore3_tiki():
    """The GoreBox clips of the wounds of the realistic mobs (owner 2026-10-09): on BP_TikiCocoWarrior and BP_TikiVillager
    the crawl loop, the writhe loop and the panic loop (Anims), the crawl death and the bisected death (Anims.Combat)"""
    out = {}
    for mob, bp_path in (("CocoWarrior", f"{CHARS}/CocoWarrior/Blueprints/BP_TikiCocoWarrior"),
                         ("Villager", f"{CHARS}/Villager/Blueprints/BP_TikiVillager")):
        cdo = ue.get_default_object(EAL.load_blueprint_class(bp_path))
        anims = cdo.get_editor_property("anims")
        set_ = {}
        for prop, clip in TIKI_GORE_ANIMS.items():
            asset = EAL.load_asset(f"{CHARS}/{mob}/Anims/Gore/A_{mob}_{clip}")
            assert isinstance(asset, ue.AnimSequenceBase), f"{mob} {clip}"
            anims.set_editor_property(prop, asset)
            set_[prop] = asset.get_name()
        combat = anims.get_editor_property("combat")
        for prop, clip in TIKI_GORE_COMBAT.items():
            asset = EAL.load_asset(f"{CHARS}/{mob}/Anims/Gore/A_{mob}_{clip}")
            assert isinstance(asset, ue.AnimSequenceBase), f"{mob} {clip}"
            combat.set_editor_property(prop, asset)
            set_[prop] = asset.get_name()
        anims.set_editor_property("combat", combat)
        cdo.set_editor_property("anims", anims)
        assert EAL.save_asset(bp_path, only_if_is_dirty=False), bp_path
        check = ue.get_default_object(EAL.load_blueprint_class(bp_path)).get_editor_property("anims")
        out[mob] = {"set": set_, "check_legless_crawl": check.get_editor_property("legless_crawl").get_name(),
                    "check_bisected_death": check.get_editor_property("combat").get_editor_property("bisected_death").get_name()}
    return out


PARTS = {"gorewire": gorewire, "zoneanims": zoneanims, "moat_testflat": moat_testflat, "testflat_site": testflat_site, "moat_li": moat_li, "gore2": gore2,
         "gore3": gore3, "gore3_tiki": gore3_tiki}


def main():
    kitlib.check_lock()
    parts = json.loads(REQUEST.read_text(encoding="utf-8")).get("parts", list(PARTS))
    for part in parts:
        try:
            report[part] = PARTS[part]()
        except Exception:  # noqa: BLE001
            report[part] = {"error": traceback.format_exc()[-1500:]}
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")


main()
