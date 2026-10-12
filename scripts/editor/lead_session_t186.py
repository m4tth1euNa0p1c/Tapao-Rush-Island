"""Editor Python session of the lead for the evening of 2026-10-06 (T-186 houses, T-188 galleon deck, fixes).

Launch the editor with it (the lock held by MONKE_LOCK_OWNER, set in the launching environment):
  UnrealEditor.exe monke_island.uproject /Game/MonkeIsland/Maps/Test/L_Test_Flat -ExecutePythonScript=<this file>
Then write monke_island/Saved/T186/session-request.json {"action": "fixes" | "minka" | "galleon"}: the script of the
action runs once per new request, never during a game (a player's PIE is never touched); its result is in
session-result.json. Nothing runs at start.
"""
import datetime as dt
import json
import os
import runpy
import time
import traceback
from pathlib import Path

import unreal as ue

# __file__ exists only while -ExecutePythonScript runs this file: kept for the callbacks that run later
SELF = Path(__file__).resolve()
ROOT = SELF.parents[2]
OUT = ROOT / "monke_island/Saved/T186"
REQUEST = OUT / "session-request.json"
ACTIONS = {
    "fixes": "scripts/editor/reapply_showcase_fixes.py",
    "minka": "scripts/props/minka_doors_ue.py",
    "galleon": "scripts/props/galleon_deck_clear_ue.py",
    "shrine": "scripts/props/shrine_kit_ue.py",
    "sites": "scripts/map/testflat_sites_ue.py",
    "bean": "scripts/character/bean_moustache_ue.py",
    "kura": "scripts/props/kura_ue.py",
    "temples": "scripts/map/temple_spawn_setup_ue.py",
    "beanmats": "scripts/character/bean_materials_ue.py",
    "ceremony": "scripts/props/flag_ceremony_ue.py",
    "housenav": "scripts/props/house_nav_ue.py",
    "inventory": "scripts/editor/inventory_six_slots_ue.py",
    "ceremonycheck": "scripts/editor/ceremony_pie_check.py",
    "piesession": "scripts/editor/pie_session.py",
    "testdirector": "scripts/map/testflat_director_ue.py",
    "musketimport": "scripts/character/musket_reimport_ue.py",
    "crabimport": "scripts/character/import_giant_crab_ue.py",
    "crabsetup": "scripts/character/giant_crab_setup_ue.py",
    "paths": "scripts/editor/add_script_paths.py",
    "fortveg": "scripts/props/fort_rework_vegetation.py",
    "fortvegaudit": "scripts/props/fort_rework_vegetation_audit.py",
    "fortapply": "scripts/props/fort_rework_apply.py",
    "fortvalidate": "scripts/props/fort_rework_validate.py",
    "fortaudit": "scripts/props/fort_rework_audit.py",
    "ditchprobe": "scripts/props/fort_ditch_probe_ue.py",
    "ditchtraces": "scripts/props/fort_ditch_traces_ue.py",
    "moatbank": "scripts/props/fort_moat_bank_ue.py",
    "routeprobe": "scripts/props/fort_route_probe_ue.py",
    "garrisonprobe": "scripts/editor/fort_garrison_probe_ue.py",
    "beanfrimport": "scripts/character/import_bean_fr_ue.py",
    "garrisonsetup": "scripts/editor/fort_garrison_setup_ue.py",
    "garrisonnav": "scripts/editor/fort_garrison_nav_ue.py",
    "campaignsetup": "scripts/editor/campaign_setup_ue.py",
    "beanfrlods": "scripts/character/bean_fr_lods_ue.py",
    "probebeanfr": "scripts/editor/py_probe_beanfr.py",
    "beanfrmats": "scripts/character/bean_fr_materials_ue.py",
    "fortloot": "scripts/props/fort_loot_ue.py",
    "flagclaim": "scripts/props/flag_claim_ue.py",
    "fortcages": "scripts/props/fort_chapel_cages_ue.py",
    "music": "scripts/audio/music_setup_ue.py",
    "crabsand": "scripts/character/crab_emergence_setup_ue.py",
    "daytransition": "scripts/editor/day_transition_setup_ue.py",
    "musketskel": "scripts/character/musket_skeleton_save_ue.py",
    "crabbs": "scripts/character/crab_blendspace_fix_ue.py",
    "fortsurvey": "scripts/editor/fort_survey_ue.py",
    "fragsurvey": "scripts/editor/frag_survey_ue.py",
    "ownerbatch": "scripts/editor/owner_batch_1007_ue.py",
    "fortli": "scripts/editor/fort_li_build_ue.py",
    "fortisland": "scripts/editor/fort_island_ue.py",
    "fortnav": "scripts/editor/fort_island_nav_ue.py",
    "tikiunarm": "scripts/editor/tiki_unarm_ue.py",
    "soldierskel": "scripts/editor/soldier_skeleton_probe_ue.py",
    "dirtylist": "scripts/editor/dirty_list_ue.py",
    "galleondeck": "scripts/editor/galleon_deck_probe_ue.py",
    "execsetup": "scripts/editor/execution_setup_ue.py",
    "loadlevel": "scripts/editor/load_level_ue.py",
    "exectune": "scripts/editor/execution_tune_ue.py",
    "naniteaudit": "scripts/editor/nanite_audit_ue.py",
    "galleonnanite": "scripts/editor/galleon_nanite_ue.py",
    "shopcutlass": "scripts/editor/shop_cutlass_ue.py",
    "fortlidump": "scripts/editor/fort_li_dump_ue.py",
    "islandsdump": "scripts/editor/islands_dump_ue.py",
    "fortprofile": "scripts/editor/fort_terrain_profile_ue.py",
    "fortland": "scripts/editor/fort_land_ue.py",
    "cocoreal": "scripts/character/import_coco_real_ue.py",
    "ownerbatch1008": "scripts/editor/owner_batch_1008_ue.py",
    "probelod": "scripts/editor/py_probe_lod.py",
    "levelfolders": "scripts/editor/level_folders_ue.py",
    "probebpmats": "scripts/editor/py_probe_bp_mats.py",
    "probesetmat": "scripts/editor/py_probe_setmat.py",
    "probesections": "scripts/editor/py_probe_sections.py",
    "beanfrgore": "scripts/character/import_bean_fr_gore_ue.py",
    "goreanims": "scripts/character/import_gore_anims_ue.py",
    "mcpstart": "scripts/editor/mcp_start_ue.py",
    "ownerbatch1009": "scripts/editor/owner_batch_1009_ue.py",
    "weaponsmap": "scripts/editor/weapons_test_map_ue.py",
    "fparms": "scripts/editor/weapons_fp_arms_ue.py",
    # wave of 2026-10-09 evening: one action per agent script (run through tmp/run_action_locked.sh)
    "crateimport": "scripts/props/import_loot_crate_ue.py",
    "cratemap": "scripts/editor/loot_crate_test_map_ue.py",
    "weaponsaudio": "scripts/audio/weapons_audio_setup_ue.py",
    "owneraudio": "scripts/audio/owner_sounds_setup_ue.py",
    "crabv2import": "scripts/creatures/giant_crab/import_giant_crab_v2_ue.py",
    "crabboss": "scripts/editor/crab_boss_setup_ue.py",
    "stealthsetup": "scripts/editor/stealth_setup_ue.py",
    "voicesetup": "scripts/editor/voice_setup_ue.py",
    "weaponicons": "scripts/editor/weapon_icons_ue.py",
    "inputkeys": "scripts/editor/list_input_keys_ue.py",
    "prepclose": "scripts/editor/prepare_close_ue.py",
    # tickets wave of 2026-10-10 (agents run them through tmp/run_action_locked.sh)
    "mobweapons": "scripts/editor/mob_weapons_setup_ue.py",
    "shreddemo": "scripts/editor/shred_demo_setup_ue.py",
    "stormartifact": "scripts/editor/storm_artifact_setup_ue.py",
    "bloodtint": "scripts/editor/blood_tint_ue.py",
    "deathux": "scripts/editor/death_ux_setup_ue.py",
    "expeditionaudio": "scripts/audio/expedition_setup_ue.py",
    "boatdrag": "scripts/editor/boat_drag_setup_ue.py",
    "matusage": "scripts/editor/material_usage_fix_ue.py",
    # wave B of 2026-10-10
    "beanswim": "scripts/character/bean_swim_import_ue.py",
    "maraeplants": "scripts/editor/marae_no_plants_ue.py",
    "fortpaths": "scripts/editor/fort_paths_fix_ue.py",
    "dayendux": "scripts/editor/day_end_ux_setup_ue.py",
    "islandnames": "scripts/editor/island_names_ue.py",
    # 2026-10-10 evening: machine gun, horror mobs, tracers
    "mgimport": "scripts/props/machinegun_pack/import_machinegun_ue.py",
    "mgsetup": "scripts/editor/machinegun_setup_ue.py",
    "mobsvisual": "scripts/character/tiki_horror_import_ue.py",
    "mobsmech": "scripts/editor/tiki_mechanics_setup_ue.py",
    "tracers": "scripts/editor/tracer_setup_ue.py",
    "pickupcollision": "scripts/editor/pickup_collision_fix_ue.py",
    "smokebreakup": "scripts/vfx/smoke_breakup_ue.py",
    "ashcloud": "scripts/editor/ash_cloud_look_ue.py",
    "shreddemobounds": "scripts/editor/shred_demo_bounds_ue.py",
    "fortjute": "scripts/editor/fort_jute_collision_ue.py",
    "bootpc": "scripts/editor/boot_player_controller_ue.py",
    "netwavecheck": "scripts/editor/net_wave_t312_check_ue.py",
    "mpsettings": "scripts/editor/mp_play_settings_ue.py",
    # 2026-10-11 afternoon: realistic smoke (T-321), merchant on the deck (T-322)
    "smokeflipbook": "scripts/vfx/smoke_flipbook_ue.py",
    "deckmap": "scripts/editor/galleon_deck_map_ue.py",
    "merchantdeck": "scripts/editor/merchant_deck_centre_ue.py",
    "shopcategories": "scripts/editor/shop_categories_ue.py",
    "merchantbackups": "scripts/editor/merchant_deck_restore_backups_ue.py",
    "fxcone": "scripts/vfx/fx_cone_direction_ue.py",
    "normalreload": "scripts/editor/weapons_normal_reload_ue.py",
    "fortbuildingposts": "scripts/editor/fort_building_posts_ue.py",
    "trumpetcall": "scripts/editor/trumpet_call_ue.py",
    "crabunpin": "scripts/editor/crab_unpin_ue.py",
    "mobhealth": "scripts/editor/mob_health_ue.py",
    "newweapons": "scripts/editor/new_weapons_ue.py",
    "capture": "scripts/editor/capture_ue.py",
    "bandages": "scripts/editor/bandages_ue.py",
}
LAST = time.time()
ue.EditorPythonScripting.set_keep_python_script_alive(True)


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2), encoding="utf-8")


def actions_on_disk():
    """ACTIONS as written in this file now: actions added after the editor started become available"""
    import ast
    tree = ast.parse(SELF.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "ACTIONS" for t in node.targets):
            return ast.literal_eval(node.value)
    return {}


def execute(action):
    try:
        ACTIONS.update(actions_on_disk())
        lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
        owner = os.environ.get("MONKE_LOCK_OWNER")
        assert owner and lock["owner"] == owner, f"editor lock held by {lock['owner']}"
        assert dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone(), "editor lock expired"
        assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"
        assert action in ACTIONS, f"unknown action {action}"
        runpy.run_path(str(ROOT / ACTIONS[action]), run_name="__main__")
        result = {"action": action, "passed": True}
    except Exception:
        result = {"action": action, "passed": False, "error": traceback.format_exc()}
        ue.log_error(result["error"])
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    write("session-result.json", result)


# guards (2026-10-10, after the crash of 02:01:51: an FBX import failed in one action, the next action's import crashed
# in libfbxsdk within the same frame): one action per frame at most, a pause after each, and after an action whose
# import failed the editor refuses further imports until it is restarted
LAST_FRAME = -1
LAST_END = 0.0
IMPORT_BROKEN = None
MIN_GAP_SECONDS = 2.0


BUSY = False


def poll(delta):
    global LAST, LAST_FRAME, LAST_END, IMPORT_BROKEN, BUSY
    # a long action (an import) pumps Slate, which calls this callback again: never run an action inside another
    if BUSY or not (REQUEST.exists() and REQUEST.stat().st_mtime > LAST):
        return
    frame = ue.SystemLibrary.get_frame_count()
    if frame == LAST_FRAME or time.time() - LAST_END < MIN_GAP_SECONDS:
        return  # taken at a later frame
    LAST = REQUEST.stat().st_mtime
    LAST_FRAME = frame
    try:
        action = json.loads(REQUEST.read_text(encoding="utf-8-sig"))["action"]
        if IMPORT_BROKEN:
            write("session-result.json", {"action": action, "passed": False, "finished_at": dt.datetime.now(dt.timezone.utc).isoformat(),
                                          "error": f"refused: an import failed in action '{IMPORT_BROKEN}'; the FBX importer may be unstable "
                                                   "until the lead restarts the editor"})
        else:
            BUSY = True
            try:
                execute(action)
            finally:
                BUSY = False
            result = json.loads((OUT / "session-result.json").read_text(encoding="utf-8"))
            error = (result.get("error") or "").lower()
            # an asset import that failed (FBX importer, import tasks, Interchange), never a Python "import" of a module
            # (a ModuleNotFoundError refused every later action until a restart, 2026-10-11)
            asset_import = any(word in error for word in ("fbx", "assetimporttask", "import_asset_tasks", "interchange", "importer"))
            if not result.get("passed") and asset_import:
                IMPORT_BROKEN = action
                ue.log_warning(f"lead session: action {action} failed during an import; further actions refused until a restart")
    except Exception:
        ue.log_error(traceback.format_exc())
    LAST_END = time.time()


HANDLE = ue.register_slate_post_tick_callback(poll)
write("session-ready.json", {"started_at": dt.datetime.now(dt.timezone.utc).isoformat(),
                             "owner": os.environ.get("MONKE_LOCK_OWNER")})
