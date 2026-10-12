"""T-333: the trumpet call on A (owner request 2026-10-11) set up in the editor.

Run it once the editor is built with the T-333 code (UTrumpetEmoteRules.CallKey / CallAction / CallSound / CallIcon), out
of any game: lead session action "trumpetcall" (scripts/editor/lead_session_t186.py). Idempotent: a replay imports the
files again in place and maps the same key.
  1. imports the bugle call art/audio/trumpet_call/SW_Trumpet_Call.wav (scripts/audio/trumpet_call_prepare.py,
     "Assembly", U.S. Army, public domain) as /Game/MonkeIsland/Emotes/Trumpet/SW_Trumpet_Call, in the sound class of
     the effects, like the fanfare;
  2. imports the icon art/ui/icons/spells/T_Icon_TrumpetCall.png (scripts/ui/trumpet_call_icon.py) as a UI texture in
     /Game/MonkeIsland/UI/Icons/Spells (always cooked with /Game/MonkeIsland/UI/Icons);
  3. creates IA_TrumpetCall (a button that consumes its key) in /Game/MonkeIsland/Emotes/Input;
  4. IMC_TrumpetEmote: the V of the free fanfare is taken out, DA_TrumpetEmoteRules.CallKey (A) is mapped to
     IA_TrumpetCall instead (the context the trumpet component adds, priority MappingPriority = 1, above IMC_Default);
  5. DA_TrumpetEmoteRules: CallAction, CallSound, CallIcon (the timings and the stamina keep the values of the class or
     of the asset: nothing tuned is overwritten);
  6. lists every other mapping of the call key in the contexts of /Game (IMC_Default maps A to IA_Move, left on a
     QWERTY keyboard: the call's mapping consumes it while it is on);
  7. DA_TapaoUISkin.LoadingTips: the tip "Touche V : sonne la fanfare de la trompette." (false now) tells the call on A.
Report: monke_island/Saved/T333/trumpet-call.json.
"""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T333/trumpet-call.json"
SOUND_FILE = ROOT / "art/audio/trumpet_call/SW_Trumpet_Call.wav"
ICON_FILE = ROOT / "art/ui/icons/spells/T_Icon_TrumpetCall.png"
TRUMPET = "/Game/MonkeIsland/Emotes/Trumpet"
INPUT = "/Game/MonkeIsland/Emotes/Input"
ICONS = "/Game/MonkeIsland/UI/Icons/Spells"
RULES = f"{TRUMPET}/DA_TrumpetEmoteRules"
IMC = f"{INPUT}/IMC_TrumpetEmote"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
SKIN = "/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin"
NEW_TIP = "Touche A : sonne l'appel de la trompette, qui rend l'endurance (une fois par minute)."
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def load(path):
    return ue.load_asset(path) if EAL.does_asset_exist(path) else None


def import_file(filename, folder, name):
    task = ue.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    TOOLS.import_asset_tasks([task])
    asset = load(f"{folder}/{name}")
    assert asset, f"import of {filename} failed"
    return asset


def sound():
    wave = import_file(SOUND_FILE, TRUMPET, "SW_Trumpet_Call")
    effects = load(EFFECTS_CLASS)
    if effects:
        wave.set_editor_property("sound_class_object", effects)
    EAL.save_loaded_asset(wave, only_if_is_dirty=False)
    return wave


def icon():
    texture = import_file(ICON_FILE, ICONS, "T_Icon_TrumpetCall")
    # a picture of the interface: no mips, never streamed, full quality (as the weapon icons, T-264)
    texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", ue.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", ue.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    EAL.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def action():
    found = load(f"{INPUT}/IA_TrumpetCall")
    made = found or TOOLS.create_asset("IA_TrumpetCall", INPUT, ue.InputAction, ue.InputAction_Factory())
    assert made, "IA_TrumpetCall not created"
    made.set_editor_property("value_type", ue.InputActionValueType.BOOLEAN)
    # the call's key does nothing else while its context is on (IMC_Default maps A to IA_Move)
    made.set_editor_property("consume_input", True)
    EAL.save_loaded_asset(made, only_if_is_dirty=False)
    return made


def key_name(key):
    return str(key.get_editor_property("key_name"))


def mappings_of(context):
    try:
        return list(context.get_editor_property("default_key_mappings").get_editor_property("mappings"))
    except Exception:  # noqa: BLE001 - older layout of the mapping context
        try:
            return list(context.get_editor_property("mappings"))
        except Exception:  # noqa: BLE001
            return []


def others_on(key, call_action):
    """Every mapping of the key in the contexts of /Game, but the call's own"""
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_class(ue.TopLevelAssetPath("/Script/EnhancedInput", "InputMappingContext")):
        package = str(data.package_name)
        if not package.startswith("/Game/"):
            continue
        context = ue.load_asset(package)
        for mapping in mappings_of(context):
            mapped = mapping.get_editor_property("action")
            if key_name(mapping.get_editor_property("key")) == key and mapped != call_action:
                found.append(f"{package.rsplit('/', 1)[1]}: {mapped.get_name() if mapped else None}")
    return found


def loading_tip():
    """The loading tip of the old key V tells the call on A instead (the other tips are left alone)"""
    skin = load(SKIN)
    if not skin:
        return "no DA_TapaoUISkin"
    tips = list(skin.get_editor_property("loading_tips"))
    replaced = [index for index, tip in enumerate(tips) if "fanfare" in str(tip) and "V" in str(tip)]
    if not replaced:
        return "no tip of the key V"
    for index in replaced:
        tips[index] = ue.Text(NEW_TIP)
    skin.set_editor_property("loading_tips", tips)
    EAL.save_loaded_asset(skin, only_if_is_dirty=False)
    return f"{len(replaced)} tip(s) replaced"


def main():
    rules = load(RULES)
    assert rules, f"{RULES} missing"
    try:
        rules.get_editor_property("call_action")
    except Exception as error:  # noqa: BLE001 - an editor built before T-333 does not know the property
        raise RuntimeError("the editor runs without the T-333 code (UTrumpetEmoteRules.CallAction): build it first") from error
    wave = sound()
    texture = icon()
    call_action = action()

    key = rules.get_editor_property("call_key")
    if not key_name(key) or key_name(key) == "None":
        key = ue.Key()
        key.set_editor_property("key_name", "A")
        rules.set_editor_property("call_key", key)
    imc = load(IMC) or TOOLS.create_asset("IMC_TrumpetEmote", INPUT, ue.InputMappingContext, ue.InputMappingContext_Factory())
    assert imc, f"{IMC} not created"
    before = [f"{key_name(m.get_editor_property('key'))}: {m.get_editor_property('action').get_name() if m.get_editor_property('action') else None}"
              for m in mappings_of(imc)]
    # the free fanfare of V goes (T-333: one trumpet, on A)
    imc.unmap_all()
    imc.map_key(call_action, key)
    EAL.save_loaded_asset(imc, only_if_is_dirty=False)

    rules.set_editor_property("call_action", call_action)
    rules.set_editor_property("call_sound", wave)
    rules.set_editor_property("call_icon", texture)
    rules.set_editor_property("mapping_context", imc)
    EAL.save_loaded_asset(rules, only_if_is_dirty=False)
    tip = loading_tip()

    try:
        seconds = round(float(wave.get_editor_property("duration")), 3)
    except Exception:  # noqa: BLE001 - only for the report
        seconds = None
    report = {
        "task": "T-333",
        "sound": wave.get_path_name(),
        "sound_seconds": seconds,
        "icon": texture.get_path_name(),
        "action": call_action.get_path_name(),
        "mapping_context": imc.get_path_name(),
        "mapping_before": before,
        "mapping_after": [f"{key_name(m.get_editor_property('key'))}: {m.get_editor_property('action').get_name()}" for m in mappings_of(imc)],
        "call_key": key_name(key),
        "other_mappings_of_the_key": others_on(key_name(key), call_action),
        "loading_tip": tip,
        "rules": {name: rules.get_editor_property(name) for name in
                  ("call_seconds", "call_cooldown", "call_stamina_fraction", "call_stamina_seconds", "equip_seconds",
                   "unequip_seconds", "mapping_priority", "request_burst", "requests_per_second")},
        "finished_at": dt.datetime.now(dt.timezone.utc).isoformat(),
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
    ue.log(f"[trumpet_call] key {report['call_key']}, sound {report['sound_seconds']} s, also on the key: {report['other_mappings_of_the_key']}")
    ue.log("T333_TRUMPET_CALL_SUCCESS")


main()
