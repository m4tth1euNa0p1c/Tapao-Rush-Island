"""Proximity voice chat (T-263, docs/features/chat-vocal.md): the assets of the voice.

- /Game/MonkeIsland/Voice/Input/IA_PushToTalk: boolean, consumes its key;
- /Game/MonkeIsland/Voice/Input/IMC_Voice: V -> IA_PushToTalk (the voice chat adds this context to the local player at
  DA_VoiceRules.MappingPriority, above the pawn's contexts: V talks and does nothing else);
- /Game/MonkeIsland/Settings/SoundClasses/SC_VoiceChat: class of the other players' voices (DefaultEngine.ini
  [/Script/Engine.AudioSettings] VoiPSoundClass), under the engine Master class like the other channels;
- after the build with the T-263 code: /Game/MonkeIsland/Voice/Data/DA_VoiceRules (UVoiceRules: the action and the
  context) and the VoiceChat channel of DA_SettingsRules (ESettingsVolume::VoiceChat -> SC_VoiceChat, "Volume des voix").

Lists every other action mapped on the push-to-talk key in the project's contexts (they no longer get the key while
IMC_Voice is on top). Idempotent. Runs in the editor: scripts/editor/lead_session_t186.py, action "voicesetup".
Before the build, the steps that need the new classes are skipped and the script fails at the end saying so; run it
again after the build. Result also written to monke_island/Saved/T186/voice-setup.json.
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186/voice-setup.json"
VOICE = "/Game/MonkeIsland/Voice"
INPUT = VOICE + "/Input"
DATA = VOICE + "/Data"
ACTION = INPUT + "/IA_PushToTalk"
CONTEXT = INPUT + "/IMC_Voice"
RULES = DATA + "/DA_VoiceRules"
SOUND_CLASSES = "/Game/MonkeIsland/Settings/SoundClasses"
VOICE_CLASS = SOUND_CLASSES + "/SC_VoiceChat"
SETTINGS_RULES = "/Game/MonkeIsland/Settings/DA_SettingsRules"
MASTER = "/Engine/EngineSounds/Master"
PUSH_TO_TALK_KEY = "T"  # lead 2026-10-09: V stays the trumpet fanfare (IMC_TrumpetEmote); T is free
MAPPING_PRIORITY = 10

eal = ue.EditorAssetLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
report = {"created": [], "updated": [], "conflicts": [], "pending_build": [], "notes": []}


def log(message):
    ue.log_warning(f"[voice] {message}")


def load(path):
    return ue.load_asset(path) if eal.does_asset_exist(path) else None


def load_or_create(path, cls, factory):
    asset = load(path)
    if asset:
        return asset
    folder, name = path.rsplit("/", 1)
    asset = tools.create_asset(name, folder, cls, factory)
    if not asset:
        raise RuntimeError(f"could not create {path}")
    report["created"].append(path)
    return asset


def key(name):
    k = ue.Key()
    k.set_editor_property("key_name", name)
    return k


def mappings_of(context):
    """Mappings of an input mapping context (DefaultKeyMappings since 5.7, the old array before)"""
    try:
        return list(context.get_editor_property("default_key_mappings").get_editor_property("mappings"))
    except Exception:
        try:
            return list(context.get_editor_property("mappings"))
        except Exception:
            return []


def key_name(mapping):
    try:
        return str(mapping.get_editor_property("key").get_editor_property("key_name"))
    except Exception:
        return ""


def input_assets():
    action = load_or_create(ACTION, ue.InputAction, ue.InputAction_Factory())
    action.set_editor_property("value_type", ue.InputActionValueType.BOOLEAN)
    # the push-to-talk key talks only (consumed while IMC_Voice is on top)
    action.set_editor_property("consume_input", True)
    eal.save_loaded_asset(action)

    context = load_or_create(CONTEXT, ue.InputMappingContext, ue.InputMappingContext_Factory())
    context.unmap_all_keys_from_action(action)
    context.map_key(action, key(PUSH_TO_TALK_KEY))
    eal.save_loaded_asset(context)
    report["updated"].append(f"{CONTEXT}: {PUSH_TO_TALK_KEY} -> IA_PushToTalk")
    return action, context


def key_conflicts(action):
    """Every other action on the push-to-talk key in the contexts of /Game"""
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_class(ue.TopLevelAssetPath("/Script/EnhancedInput", "InputMappingContext")):
        package = str(data.package_name)
        if not package.startswith("/Game/") or package == CONTEXT:
            continue
        context = ue.load_asset(package)
        for mapping in mappings_of(context):
            other = mapping.get_editor_property("action")
            if other and other != action and key_name(mapping) == PUSH_TO_TALK_KEY:
                found.append(f"{package}: {PUSH_TO_TALK_KEY} -> {other.get_name()}")
    return found


def voice_class():
    sound_class = load_or_create(VOICE_CLASS, ue.SoundClass, ue.SoundClassFactory())
    master = ue.load_asset(MASTER)
    if master and sound_class.get_editor_property("parent_class") != master:
        sound_class.set_editor_property("parent_class", master)
    eal.save_loaded_asset(sound_class)
    return sound_class


def voice_rules(action, context):
    if not hasattr(ue, "VoiceRules"):
        report["pending_build"].append("DA_VoiceRules (class UVoiceRules not built yet)")
        return
    rules = load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.VoiceRules)
        rules = tools.create_asset("DA_VoiceRules", DATA, ue.VoiceRules, factory)
        if not rules:
            raise RuntimeError(f"could not create {RULES}")
        report["created"].append(RULES)
    rules.set_editor_property("push_to_talk_action", action)
    rules.set_editor_property("mapping_context", context)
    rules.set_editor_property("mapping_priority", MAPPING_PRIORITY)
    eal.save_loaded_asset(rules)
    report["updated"].append(f"{RULES}: IA_PushToTalk, IMC_Voice, priority {MAPPING_PRIORITY}")


def settings_channel(sound_class):
    channel = getattr(ue.SettingsVolume, "VOICE_CHAT", None)
    if channel is None:
        report["pending_build"].append("DA_SettingsRules VoiceChat channel (ESettingsVolume::VoiceChat not built yet)")
        return
    rules = load(SETTINGS_RULES)
    if not rules:
        report["notes"].append(f"{SETTINGS_RULES} missing (scripts/settings/setup_settings_assets.py): no voice volume")
        return
    classes = dict(rules.get_editor_property("sound_classes"))
    if classes.get(channel) != sound_class:
        classes[channel] = sound_class
        rules.set_editor_property("sound_classes", classes)
        eal.save_loaded_asset(rules)
        report["updated"].append(f"{SETTINGS_RULES}: VoiceChat -> SC_VoiceChat")


def main():
    action, context = input_assets()
    report["conflicts"] = key_conflicts(action)
    for line in report["conflicts"]:
        log(f"also on {PUSH_TO_TALK_KEY} (shadowed by IMC_Voice, priority {MAPPING_PRIORITY}): {line}")
    sound_class = voice_class()
    voice_rules(action, context)
    settings_channel(sound_class)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    log(f"done: {json.dumps(report, ensure_ascii=False)}")
    if report["pending_build"]:
        raise RuntimeError("voicesetup: input and sound class done; to run again after the build with the T-263 code: "
                           + "; ".join(report["pending_build"]))


main()
