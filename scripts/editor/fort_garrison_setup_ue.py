"""T-213: the French garrison of the fort of L_Test_Flat (owner request 2026-10-07: "récupérer les modèles de beans
français [...] les 7 les mettre dans le fort français, ils se baladent et patrouillent dans le fort, ils attaquent le
player, ils ne peuvent pas sortir du fort").

- DA_FortSoldierRules (UFortSoldierRules): the bean library clips and the four A_BeanFR_* clips; hit windows of the
  maquette's manifest (AN_HitWindow_Begin / _End); the grips of the player's musket (DA_MusketRules.HandOffset) and
  cutlass (DA_CutlassRules.LibraryGripInHand); the sockets of the maquette (spine_02/musket_back, pelvis/scabbard,
  pelvis/sabre_sheathed) converted to Unreal like LibraryGripInHand; the shot effects of DA_MusketRules; the sounds
  of the musket and the cutlass; the roles of the manifest (fr4 skirmisher, fr5 grenadier, fr6 sergeant, fr7 officer);
- BP_BeanFR_<Name> x7 (AFortSoldier with its uniform, kind and rules);
- in L_Test_Flat, folder FortFrench/Garrison: FortFR_Garrison (AFortGarrisonZone: the cordon of the walls of the
  maquette, the posts of the court and of the ramparts found on the ground), FortFR_NavBounds (a copy of the arena's
  navigation bounds over the fort), the seven soldiers at their posts. The level and the assets are saved by name.
Runs in the editor: scripts/editor/lead_session_t186.py, action "garrisonsetup"; then "garrisonnav" once the
navigation mesh is built (paths between the posts, level saved).
Result: monke_island/Saved/T213/garrison-setup.json
"""
import datetime as dt
import json
import math
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T213"
MANIFEST = ROOT / "art/export/Characters/BeanFR/Source/BeanFR_EnemyManifest.json"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
BASE = "/Game/MonkeIsland/Characters/BeanFR"
LIBRARY = "/Game/MonkeIsland/Characters/Bean/Animations/Library"
RULES = BASE + "/Data/DA_FortSoldierRules"
FOLDER = "FortFrench/Garrison"
ORIGIN = ue.Vector(-66000.0, 0.0, 115.0)        # the fort in L_Test_Flat (scripts/props/fort_showcase_setup.py)
WALKWAY_Z = 2.97                                  # maquette fort-francais, levels_z.walkway (m)
# cordon of the bastions and curtains (maquette fort-francais, site.trace_cordon_m, m)
TRACE = [(24.2, 14.0), (29.2, 14.0), (31.7, 27.7), (18.0, 25.2), (18.0, 20.2), (-18.0, 20.2), (-18.0, 25.2), (-31.7, 27.7),
         (-29.2, 14.0), (-24.2, 14.0), (-24.2, -14.0), (-29.2, -14.0), (-31.7, -27.7), (-18.0, -25.2), (-18.0, -20.2),
         (18.0, -20.2), (18.0, -25.2), (31.7, -27.7), (29.2, -14.0), (24.2, -14.0)]
# posts (m, frame of the maquette): the court (z 0) and the walkway of the ramparts (z 2.97)
POSTS = [("gate", 0.0, -15.0, 0), ("cistern", 0.0, -5.2, 0), ("governor", 0.0, 6.2, 0), ("barracks", -9.0, 0.0, 0),
         ("kitchen", -6.0, -7.4, 0), ("chapel", 14.0, 6.2, 0), ("prison", 9.2, -2.2, 0), ("guardhouse", 6.0, -10.6, 0),
         ("court_sw", -16.5, -12.5, 0), ("court_se", 16.5, -12.5, 0), ("court_nw", -16.5, 12.5, 0), ("court_ne", 16.5, 12.5, 0),
         ("magazine", -10.0, 9.5, 0), ("ramp_w_top", -19.0, 12.0, 1), ("ramp_e_top", 19.0, -12.0, 1),
         ("walk_s_w", -10.0, -17.6, 1), ("walk_s_e", 10.0, -17.6, 1), ("walk_n_w", -8.0, 17.6, 1), ("walk_n_e", 8.0, 17.6, 1),
         ("walk_w", -21.6, 4.0, 1), ("walk_e", 21.6, -4.0, 1)]
# the seven uniforms of the maquette (manifest roles), their kind and first post
SOLDIERS = [("CFM_Fusilier", "FUSILIER", "gate"), ("Fusilier_1767", "FUSILIER", "guardhouse"),
            ("Fusilier_Bicorne", "FUSILIER", "ramp_w_top"), ("Chasseur_Shako", "SKIRMISHER", "ramp_e_top"),
            ("Grenadier", "GRENADIER", "cistern"), ("Sergent", "SERGEANT", "barracks"), ("Officier_Marine", "OFFICER", "governor")]
ROLES = {"FUSILIER": (1.0, 1.0, True), "SKIRMISHER": (1.0, 1.0, False), "GRENADIER": (1.3, 1.2, True),
         "SERGEANT": (1.3, 1.1, False), "OFFICER": (2.0, 1.25, False)}
# sockets of the maquette (manifest "sockets", Blender bone space) for the gear
SOCKETS = {"musket_grip": ([163.27, 12.71, -6.05], [-0.00529, 0.07532, -0.00213]),
           "musket_back": ([180.0, 2.87, 56.47], [-0.08827, -0.09321, -0.29198]),
           "sabre_sheathed": ([-150.48, 46.04, -67.82], [0.2274, 0.18777, 0.23255]),
           "scabbard": ([-150.48, 46.04, -67.82], [0.24228, 0.19, 0.13318])}
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


# ------------------------------------------------------------------------------------------------ conversions

def _mm(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def blender_socket(euler_deg, loc_m):
    """A socket of the maquette (bone space, Blender XYZ euler) as an Unreal grip: the conversion of
    scripts/props/musket_tp_tracks.py (it gives DA_CutlassRules.LibraryGripInHand back from hand_r/sabre_grip)"""
    x, y, z = (math.radians(v) for v in euler_deg)
    rx = [[1, 0, 0], [0, math.cos(x), -math.sin(x)], [0, math.sin(x), math.cos(x)]]
    ry = [[math.cos(y), 0, math.sin(y)], [0, 1, 0], [-math.sin(y), 0, math.cos(y)]]
    rz = [[math.cos(z), -math.sin(z), 0], [math.sin(z), math.cos(z), 0], [0, 0, 1]]
    r = _mm(rz, _mm(ry, rx))
    X = (r[0][0], -r[1][0], r[2][0])
    Y = (-r[0][1], r[1][1], -r[2][1])
    Z = (r[0][2], -r[1][2], r[2][2])
    pitch = math.atan2(X[2], math.hypot(X[0], X[1]))
    yaw = math.atan2(X[1], X[0])
    side = (-math.sin(yaw), math.cos(yaw), 0.0)
    roll = math.atan2(sum(a * b for a, b in zip(Z, side)), sum(a * b for a, b in zip(Y, side)))
    rotation = ue.Rotator(roll=math.degrees(roll), pitch=math.degrees(pitch), yaw=math.degrees(yaw))
    return ue.Transform(location=ue.Vector(loc_m[0] * 100.0, -loc_m[1] * 100.0, loc_m[2] * 100.0), rotation=rotation,
                        scale=ue.Vector(1.0, 1.0, 1.0))


def xf(t):
    r = t.rotation.rotator()
    return {"location": [round(t.translation.x, 2), round(t.translation.y, 2), round(t.translation.z, 2)],
            "rotation_pyr": [round(r.pitch, 2), round(r.yaw, 2), round(r.roll, 2)]}


def fort_point(x, y, z=0.0):
    return ORIGIN + ue.Vector(x * 100.0, -y * 100.0, z * 100.0)


# ------------------------------------------------------------------------------------------------ rules

def manifest_windows():
    """Hit windows of every clip of the manifest: pairs of AN_HitWindow_Begin / AN_HitWindow_End (s)"""
    doc = json.loads(MANIFEST.read_text(encoding="utf-8"))
    windows = {}
    for anim in doc["anims"]:
        begin = None
        spans = []
        for notify in anim.get("notifies", []):
            if notify["name"] == "AN_HitWindow_Begin":
                begin = notify["time_s"]
            elif notify["name"] == "AN_HitWindow_End" and begin is not None:
                spans.append((begin, notify["time_s"]))
                begin = None
        windows[anim["asset"]] = spans
    return windows, doc


def clip(path):
    sequence = kitlib.load(path)
    assert sequence, f"missing clip {path}"
    return sequence


def blow(sequence, reach, damage, windows, cone, recovery, weight=1.0, lunge=0.0):
    b = ue.FortSoldierBlowRules()
    b.set_editor_property("clip", sequence)
    b.set_editor_property("reach_cm", reach)
    b.set_editor_property("damage", damage)
    b.set_editor_property("hit_windows", [ue.Vector2D(a, e) for a, e in windows])
    b.set_editor_property("cone_degrees", cone)
    b.set_editor_property("recovery", recovery)
    b.set_editor_property("weight", weight)
    b.set_editor_property("lunge_speed", lunge)
    return b


def make_rules():
    windows, doc = manifest_windows()
    rules = kitlib.load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.FortSoldierRules)
        rules = TOOLS.create_asset("DA_FortSoldierRules", BASE + "/Data", ue.FortSoldierRules, factory)
    assert rules, RULES
    lib = lambda name: clip(f"{LIBRARY}/{name}")  # noqa: E731
    fr = lambda name: clip(f"{BASE}/Animations/{name}")  # noqa: E731
    for prop, sequence in (("idle_guard", fr("A_BeanFR_Idle_Guard")), ("patrol", fr("A_BeanFR_Patrol")), ("walk", lib("A_Bean_Walk")),
                           ("run", lib("A_Bean_Run")), ("musket_hold", lib("A_Bean_Musket_Hold")), ("sabre_idle", lib("A_Bean_Sabre_Idle")),
                           ("aim", lib("A_Bean_Musket_Aim")), ("fire", lib("A_Bean_Musket_Fire")), ("reload", lib("A_Bean_Musket_Reload")),
                           ("sabre_draw", lib("A_Bean_Sabre_Draw")), ("sabre_sheathe", lib("A_Bean_Sabre_Sheathe")),
                           ("hit_react", lib("A_Bean_Hit_React")), ("death", lib("A_Bean_Death"))):
        rules.set_editor_property(prop, sequence)
    damage = doc["ai"]["damage_suggested"]
    combo = windows["A_BeanFR_Sabre_Combo"]
    rules.set_editor_property("bayonet", blow(fr("A_BeanFR_Bayonet_Thrust"), doc["ai"]["ranges"]["bayonet"]["max"], damage["bayonet"],
                                              windows["A_BeanFR_Bayonet_Thrust"], 40.0, 0.6, 1.0, 60.0))
    rules.set_editor_property("slash_right", blow(lib("A_Bean_Sabre_Slash_R"), doc["ai"]["ranges"]["sabre"]["max"], damage["sabre_slash"],
                                                  windows["A_Bean_Sabre_Slash_R"], 60.0, 0.5))
    rules.set_editor_property("slash_left", blow(lib("A_Bean_Sabre_Slash_L"), doc["ai"]["ranges"]["sabre"]["max"], damage["sabre_slash"],
                                                 windows["A_Bean_Sabre_Slash_L"], 60.0, 0.5))
    rules.set_editor_property("thrust", blow(lib("A_Bean_Sabre_Thrust"), doc["ai"]["ranges"]["sabre"]["max"] + 10.0, damage["sabre_slash"] + 3.0,
                                             windows["A_Bean_Sabre_Thrust"], 35.0, 0.5, 0.7))
    rules.set_editor_property("combo", blow(fr("A_BeanFR_Sabre_Combo"), doc["ai"]["ranges"]["sabre"]["max"],
                                            round(damage["sabre_combo_total"] / max(1, len(combo)), 1), combo, 60.0, 0.8, 0.6))
    rules.set_editor_property("min_fire_cm", float(doc["ai"]["ranges"]["musket_fire"]["min"]))
    rules.set_editor_property("max_fire_cm", float(doc["ai"]["ranges"]["musket_fire"]["max"]))
    rules.set_editor_property("melee_switch_cm", float(doc["ai"]["ranges"]["switch_to_sabre"]["below"]))
    draw = next(a for a in doc["anims"] if a["asset"] == "A_Bean_Sabre_Draw")
    rules.set_editor_property("draw_attach_time", next(n["time_s"] for n in draw["notifies"] if n["name"] == "AN_Draw_AttachToHand"))
    fire = next(a for a in doc["anims"] if a["asset"] == "A_Bean_Musket_Fire")
    rules.set_editor_property("fire_shot_time", next(n["time_s"] for n in fire["notifies"] if n["name"] == "AN_FireShot"))

    roles = []
    for kind, (health, damage_scale, bayonet) in ROLES.items():
        role = ue.FortSoldierRoleRules()
        role.set_editor_property("role", getattr(ue.FortSoldierRole, kind))
        role.set_editor_property("health_scale", health)
        role.set_editor_property("damage_scale", damage_scale)
        role.set_editor_property("bayonet", bayonet)
        roles.append(role)
    rules.set_editor_property("roles", roles)

    # gear: the player's grips where they exist, the maquette's sockets converted otherwise
    musket_rules = kitlib.load("/Game/MonkeIsland/Weapons/Musket/DA_MusketRules")
    cutlass_rules = kitlib.load("/Game/MonkeIsland/Weapons/Cutlass/DA_CutlassRules")
    assert musket_rules and cutlass_rules, "DA_MusketRules / DA_CutlassRules missing"
    hand = musket_rules.get_editor_property("hand_offset")
    grip = blender_socket(*SOCKETS["musket_grip"])
    back = blender_socket(*SOCKETS["musket_back"])
    # the musket of the player (SK_Bean_Musket) is turned from the maquette's: the same turn on the back
    correction = ue.MathLibrary.compose_transforms(hand, ue.MathLibrary.invert_transform(grip))
    on_back = ue.MathLibrary.compose_transforms(correction, back)
    static_musket = kitlib.load("/Game/MonkeIsland/Weapons/Musket/Static/SM_Bean_Musket")
    muzzle = static_musket.find_socket("muzzle") if static_musket else None
    rules.set_editor_property("musket_mesh", musket_rules.get_editor_property("musket_mesh"))
    rules.set_editor_property("hand_bone", musket_rules.get_editor_property("hand_bone"))
    rules.set_editor_property("musket_in_hand", hand)
    # the shot and the reload of the player's musket, its musket following the hands (owner 2026-10-07)
    rules.set_editor_property("musket_poses", musket_rules)
    rules.set_editor_property("back_bone", "spine_02")
    rules.set_editor_property("musket_on_back", on_back)
    if muzzle:
        rules.set_editor_property("muzzle_offset", muzzle.get_editor_property("relative_location"))
        rules.set_editor_property("bayonet_on_muzzle", ue.Transform(rotation=muzzle.get_editor_property("relative_rotation")))
    rules.set_editor_property("bayonet_mesh", kitlib.load(f"{BASE}/Meshes/SM_Bean_Bayonet"))
    rules.set_editor_property("sabre_mesh", cutlass_rules.get_editor_property("cutlass_mesh"))
    rules.set_editor_property("sabre_in_hand", cutlass_rules.get_editor_property("library_grip_in_hand"))
    rules.set_editor_property("hip_bone", "pelvis")
    rules.set_editor_property("sabre_sheathed", blender_socket(*SOCKETS["sabre_sheathed"]))
    rules.set_editor_property("scabbard_mesh", cutlass_rules.get_editor_property("scabbard_mesh"))
    rules.set_editor_property("scabbard_on_hip", blender_socket(*SOCKETS["scabbard"]))
    for prop, source in (("flash_mesh", "flash_mesh"), ("flash_material", "flash_material"), ("flash_scale", "flash_scale"),
                         ("flash_seconds", "flash_seconds"), ("smoke_mesh", "smoke_mesh"), ("smoke_material", "smoke_material"),
                         ("smoke_opacity_parameter", "smoke_opacity_parameter"), ("shot_sound", "fire_sound")):
        rules.set_editor_property(prop, musket_rules.get_editor_property(source))
    for prop in ("swing_sound", "hit_sound", "draw_sound"):
        rules.set_editor_property(prop, cutlass_rules.get_editor_property(prop))
    rules.set_editor_property("shot_attenuation", cutlass_rules.get_editor_property("sound_attenuation"))
    kitlib.save(rules)
    return rules, {"musket_in_hand": xf(hand), "musket_on_back": xf(on_back), "muzzle": muzzle is not None,
                   "sabre_sheathed": xf(rules.get_editor_property("sabre_sheathed")),
                   "scabbard_on_hip": xf(rules.get_editor_property("scabbard_on_hip")),
                   "combo_windows": combo, "bayonet_windows": windows["A_BeanFR_Bayonet_Thrust"]}


def make_blueprints(rules):
    classes = {}
    for name, kind, _ in SOLDIERS:
        path = f"{BASE}/Blueprints/BP_BeanFR_{name}"
        blueprint = kitlib.load(path)
        if not blueprint:
            factory = ue.BlueprintFactory()
            factory.set_editor_property("parent_class", ue.FortSoldier)
            blueprint = TOOLS.create_asset(f"BP_BeanFR_{name}", f"{BASE}/Blueprints", ue.Blueprint, factory)
        assert blueprint, path
        cdo = ue.get_default_object(blueprint.generated_class())
        skin = kitlib.load(f"{BASE}/Meshes/SK_BeanFR_{name}")
        assert skin, f"SK_BeanFR_{name} missing (action beanfrimport first)"
        cdo.set_editor_property("skin", skin)
        cdo.set_editor_property("kind", getattr(ue.FortSoldierRole, kind))
        cdo.set_editor_property("rules", rules)
        ue.BlueprintEditorLibrary.compile_blueprint(blueprint)
        kitlib.save(blueprint)
        classes[name] = blueprint.generated_class()
    return classes


# ------------------------------------------------------------------------------------------------ level

def trace(world, start, end):
    hit = ue.SystemLibrary.line_trace_single(world, start, end, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                             ue.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return t[4], (t[9].get_actor_label() if t[9] else None)


def find_post(world, x, y, level):
    """The ground of a post (court or walkway) with room for a bean around it, tried a little around the point"""
    expected = ORIGIN.z + (WALKWAY_Z * 100.0 if level else 0.0)
    for dx, dy in ((0, 0), (0.8, 0), (-0.8, 0), (0, 0.8), (0, -0.8), (1.5, 0), (-1.5, 0), (0, 1.5), (0, -1.5)):
        point = fort_point(x + dx, y + dy)
        hit = trace(world, ue.Vector(point.x, point.y, expected + 500.0), ue.Vector(point.x, point.y, expected - 300.0))
        if not hit or abs(hit[0].z - expected) > 45.0:
            continue
        ground = ue.Vector(point.x, point.y, hit[0].z)
        chest = ground + ue.Vector(0.0, 0.0, 45.0)
        if any(trace(world, chest, chest + ue.Vector(ox * 55.0, oy * 55.0, 0.0)) for ox, oy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            continue
        return ground, hit[1], (x + dx, y + dy)
    return None


def main():
    kitlib.check_lock()
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world or "L_Test_Flat" not in world.get_path_name():
        assert levels.load_level(LEVEL)
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    rules, grips = make_rules()
    classes = make_blueprints(rules)
    actors_sub = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    actors = actors_sub.get_all_level_actors()
    by_label = {a.get_actor_label(): a for a in actors}

    # the posts found on the ground
    posts, rejected = [], []
    for name, x, y, level in POSTS:
        found = find_post(world, x, y, level)
        if found:
            posts.append((name, found))
        else:
            rejected.append(name)
    assert len(posts) >= 8, f"too few posts on the ground: {rejected}"
    index = {name: i for i, (name, _) in enumerate(posts)}

    # the zone of the fort: the cordon of the walls, the posts in its frame
    zone = by_label.get("FortFR_Garrison")
    if not zone:
        zone = actors_sub.spawn_actor_from_class(ue.FortGarrisonZone, ORIGIN, ue.Rotator(0.0, 0.0, 0.0))
        zone.set_actor_label("FortFR_Garrison")
    zone.set_actor_location(ORIGIN, False, False)
    zone.set_actor_rotation(ue.Rotator(0.0, 0.0, 0.0), False)
    zone.set_folder_path(FOLDER)
    zone.set_editor_property("outline", [ue.Vector2D(x * 100.0, -y * 100.0) for x, y in TRACE])
    zone.set_editor_property("min_z", -300.0)
    zone.set_editor_property("max_z", 1500.0)
    zone.set_editor_property("patrol_points", [ground - ORIGIN for _, (ground, _, _) in posts])

    # navigation over the fort: a copy of the arena's bounds (a volume spawned by script has no brush)
    nav = by_label.get("FortFR_NavBounds")
    if not nav:
        source = by_label.get("NavMeshBounds")
        assert source, "no NavMeshBounds to copy in L_Test_Flat"
        nav = actors_sub.duplicate_actor(source, world, ue.Vector(0.0, 0.0, 0.0))
        nav.set_actor_label("FortFR_NavBounds")
    _, source_extent = (by_label["NavMeshBounds"].get_actor_bounds(False) if "NavMeshBounds" in by_label else (None, ue.Vector(5000, 5000, 500)))
    scale = nav.get_actor_scale3d()
    base_extent = ue.Vector(source_extent.x / max(scale.x, 1e-3), source_extent.y / max(scale.y, 1e-3), source_extent.z / max(scale.z, 1e-3)) \
        if "NavMeshBounds" in by_label and nav is not by_label["NavMeshBounds"] else ue.Vector(100.0, 100.0, 100.0)
    nav.set_actor_location(ORIGIN + ue.Vector(0.0, 0.0, 250.0), False, False)
    wanted = ue.Vector(3500.0, 3100.0, 750.0)
    nav.set_actor_scale3d(ue.Vector(wanted.x / max(base_extent.x, 1.0), wanted.y / max(base_extent.y, 1.0), wanted.z / max(base_extent.z, 1.0)))
    nav.set_folder_path(FOLDER)

    # the soldiers at their posts (a replay moves them back)
    placed = {}
    for name, kind, post in SOLDIERS:
        label = f"FortFR_{name}"
        slot = index.get(post, 0)
        ground = posts[slot][1][0]
        location = ground + ue.Vector(0.0, 0.0, 57.0)
        facing = math.degrees(math.atan2(ORIGIN.y - ground.y, ORIGIN.x - ground.x))
        soldier = by_label.get(label)
        if soldier and soldier.get_class() != classes[name]:
            actors_sub.destroy_actor(soldier)
            soldier = None
        if not soldier:
            soldier = actors_sub.spawn_actor_from_class(classes[name], location, ue.Rotator(0.0, 0.0, facing))
            soldier.set_actor_label(label)
        soldier.set_actor_location(location, False, False)
        soldier.set_actor_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=facing), False)
        soldier.set_folder_path(FOLDER)
        soldier.set_editor_property("first_post", slot)
        placed[label] = {"post": posts[slot][0], "at": [round(location.x), round(location.y), round(location.z)], "kind": kind}

    ue.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    assert levels.save_current_level()
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-213", "rules": RULES, "grips": grips, "blueprints": sorted(c.get_path_name() for c in classes.values()),
              "posts": [{"name": n, "ground": [round(g.x), round(g.y), round(g.z)], "on": a, "fort_m": list(m)} for n, (g, a, m) in posts],
              "rejected_posts": rejected, "soldiers": placed, "nav_bounds_scale": [round(v, 2) for v in (nav.get_actor_scale3d().x,
              nav.get_actor_scale3d().y, nav.get_actor_scale3d().z)], "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "garrison-setup.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T213_GARRISON_SETUP_SUCCESS")


if __name__ == "__main__":
    main()
