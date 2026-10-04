"""TAPAO interface kit (maquette ui-tapao of the second PC, owner request 2026-10-04): textures, fonts, sounds,
DA_TapaoUISkin and the main menu map L_MainMenu.

Sources (copied from OneDrive into the repo): art/ui/tapao (sprites, ui_manifest.json, fonts), art/audio/ui
(scripts/ui/tapao_sounds.py), art/ui/tapao/backgrounds (menu / loading backgrounds, if present).
In the editor (the fonts need Slate, a commandlet crashes on them), editor built with UTapaoUISkin:
  UnrealEditor.exe monke_island.uproject -ExecutePythonScript="<this file>"   (the editor quits at the end)
Idempotent (re-imports replace the textures).
"""
import glob
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
KIT = os.path.join(ROOT, "art", "ui", "tapao")
MANIFEST = json.load(open(os.path.join(KIT, "ui_manifest.json"), encoding="utf-8"))
DEST = "/Game/MonkeIsland/UI/Tapao"
TEX = f"{DEST}/Textures"
MENU_MAP = "/Game/MonkeIsland/Maps/L_MainMenu"
PLAY_MAP = "/Game/MonkeIsland/Maps/L_Boot"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

TIPS = [
    "Pacifie une zone : plante le drapeau et tiens ton sermon 20 secondes, sans bouger.",
    "Chaque zone pacifiée repousse la montée des eaux de deux minutes.",
    "Arrime ton butin dans la barque : ce qui reste à terre est perdu à la submersion.",
    "La torche s'éteint dans l'eau ; garde-la au sec et rallume-la au silex.",
    "Les campements de pêcheurs cachent des outils de valeur et du poisson pour reprendre des forces.",
    "Le Marae des Perles garde un artefact rare : approche-toi avec prudence.",
    "Touche V : sonne la fanfare de la trompette.",
    "Une arme lâchée par un mob se revend : ramasse-la et range-la dans la barque.",
]
CREDITS = [
    "TAPAO : Rush Island",
    "Conception et direction : l'équipe monke_island",
    "Interface, modèles et animations : maquettes Blender de l'usine (bois flotté, corde, laiton)",
    "Polices : Pirata One et Alegreya SC (SIL Open Font License)",
    "Textures et HDRI : Poly Haven, ambientCG (CC0)",
    "Atoll de Raraka : données Te Fenua / DAF, Licence Ouverte Etalab 2.0",
    "Le village de Motutapu est réel ; les personnages et événements du jeu sont fictifs.",
    "Moteur : Unreal Engine 5.8",
]


def log(msg):
    unreal.log_warning(f"[tapao] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def import_files(files, folder):
    tasks = []
    for path in files:
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = folder
        task.destination_name = os.path.splitext(os.path.basename(path))[0]
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    return [f"{folder}/{os.path.splitext(os.path.basename(p))[0]}" for p in files]


def ui_texture(path):
    tex = load(path)
    if not tex:
        return None
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("srgb", True)
    eal.save_loaded_asset(tex)
    return tex


def textures():
    out = {}
    for sub in ("raw", "states", "fills", "items"):
        files = sorted(glob.glob(os.path.join(KIT, "sprites", sub, "*.png")))
        # already imported (a first run): kept as they are, the kit only changes with a new maquette
        missing = [f for f in files if not eal.does_asset_exist(f"{TEX}/{sub}/{os.path.splitext(os.path.basename(f))[0]}")]
        for path in import_files(missing, f"{TEX}/{sub}"):
            ui_texture(path)
        for f in files:
            name = os.path.splitext(os.path.basename(f))[0]
            out[(sub, name)] = load(f"{TEX}/{sub}/{name}")
        log(f"{len(files)} textures in {sub}")
    return out


def background(name):
    files = glob.glob(os.path.join(KIT, "backgrounds", f"{name}.*"))
    if not files:
        return None
    path = import_files(files[:1], f"{DEST}/Backgrounds")[0]
    tex = load(path)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("never_stream", True)
    eal.save_loaded_asset(tex)
    return tex


def font(ttf):
    """Font face from the TTF (FTapaoStyle builds the composite font around it; Python cannot fill a UFont)"""
    face_path = import_files([os.path.join(KIT, "fonts", ttf)], f"{DEST}/Fonts")[0]
    face = load(face_path)
    if not face:
        log(f"font face {ttf} not imported")
        return None
    eal.save_loaded_asset(face)
    return face


def sounds():
    out = {}
    effects = load(EFFECTS_CLASS)
    files = sorted(glob.glob(os.path.join(ROOT, "art", "audio", "ui", "SW_UI_*.wav")))
    for path in import_files(files, f"{DEST}/Audio"):
        wave = load(path)
        if wave and effects:
            wave.set_editor_property("sound_class_object", effects)
        eal.save_loaded_asset(wave)
        out[path.rsplit("/", 1)[-1]] = wave
    return out


def menu_map():
    if not eal.does_asset_exist(MENU_MAP):
        eal.duplicate_asset("/Engine/Maps/Entry", MENU_MAP)
    world = load(MENU_MAP)
    try:
        settings = world.get_world_settings()
        settings.set_editor_property("default_game_mode", unreal.GameModeBase)
        eal.save_loaded_asset(world)
        log("L_MainMenu: GameModeBase")
    except Exception as error:   # noqa: BLE001 - the map still works with the project game mode
        log(f"L_MainMenu game mode not set: {error}")
    return world


def main():
    tex = textures()
    sprites = {}
    for name, entry in MANIFEST["sprites"].items():
        sub = entry["file"].split("/")[1]
        sprite = unreal.TapaoSprite()
        sprite.set_editor_property("texture", tex.get((sub, name)))
        for state in ("hover", "pressed", "disabled"):
            if entry.get("states", {}).get(state):
                sprite.set_editor_property(state, tex.get(("states", f"{name}_{state}")))
        margin = entry.get("unreal_margin")
        if margin:
            sprite.set_editor_property("margin", unreal.Margin(margin[0], margin[1], margin[0], margin[1]))
        size = entry.get("size_px")
        if size:
            sprite.set_editor_property("size_px", unreal.Vector2D(size[0], size[1]))
        sprites[name] = sprite
    fills = {k[1]: v for k, v in tex.items() if k[0] == "fills"}
    items = {k[1]: v for k, v in tex.items() if k[0] == "items"}

    path = f"{DEST}/DA_TapaoUISkin"
    skin = load(path)
    if not skin:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TapaoUISkin)
        skin = tools.create_asset("DA_TapaoUISkin", DEST, unreal.TapaoUISkin, factory)
    skin.set_editor_property("sprites", sprites)
    skin.set_editor_property("fills", fills)
    skin.set_editor_property("items", items)
    skin.set_editor_property("title_font", font("PirataOne-Regular.ttf"))
    skin.set_editor_property("ui_font", font("AlegreyaSC-Black.ttf"))
    skin.set_editor_property("text_font", font("AlegreyaSC-Bold.ttf"))
    for prop, name in (("menu_background", "T_Tapao_MenuBackground"), ("loading_background", "T_Tapao_LoadingBackground")):
        bg = background(name)
        if bg:
            skin.set_editor_property(prop, bg)
    snd = sounds()
    skin.set_editor_property("hover_sound", snd.get("SW_UI_Hover"))
    skin.set_editor_property("click_sound", snd.get("SW_UI_Click"))
    menu_map()
    skin.set_editor_property("main_menu_map", load(MENU_MAP))
    skin.set_editor_property("play_map", load(PLAY_MAP))
    skin.set_editor_property("min_loading_seconds", 3.0)
    skin.set_editor_property("version_text", unreal.Text("TAPAO : Rush Island · v0.1"))
    skin.set_editor_property("loading_tips", [unreal.Text(t) for t in TIPS])
    skin.set_editor_property("credits", [unreal.Text(t) for t in CREDITS])
    eal.save_loaded_asset(skin)
    log(f"DA_TapaoUISkin: {len(sprites)} sprites, {len(fills)} fills, {len(items)} items")


main()
