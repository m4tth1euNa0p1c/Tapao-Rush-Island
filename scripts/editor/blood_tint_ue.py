"""T-162 (lot L5): blood of each species — a colour per tiki mob, the dark red of the humans and beans.

Run as the action "bloodtint" of the lead's editor session (scripts/editor/lead_session_t186.py, through
tmp/run_action_locked.sh): editor open, editor lock held, no game running. Idempotent and short (a few seconds; the
material instances compile in the background). Never touches a level.

Why copies: the NS_Blood_* (stateless copies of FountainLightweight, T-153) have no user parameter and none can be added
by script in this editor (the experimental NiagaraToolsets plugin, which can, is not enabled). So each species gets its
own copies of the three systems, rendered with a material instance of M_Blood_Particle in its colour; the game picks
them (UTikiBloodSubsystem + DA_BloodSpecies, by the skeleton of the bleeding body) and tints the decals itself.

Builds in /Game/MonkeIsland/VFX/Blood/ (cooked whole: DefaultGame.ini DirectoriesToAlwaysCook):
  1. MI_Blood_Particle_<Blood> (Sap, Hemolymph, Lava): BloodColor of the drops; the lava also glows (NightReadability).
  2. NS_Blood_<Kind>_<Blood>: copies of NS_Blood_MusketImpact / SliceSpray / Chunks whose sprite renderer draws with
     that instance (same shapes, sizes, speeds, durations as the red ones).
  3. DA_BloodSpecies (needs UTikiBloodSpeciesRules, i.e. the lead's editor build with T-162): Bean (players, soldiers,
     merchant) and Villager in dark red with the red systems, Coco in amber sap, Crab in blue hemolymph, Ash in lava.
     Before the build: reported under "pending".
Report: monke_island/Saved/T162/blood-tint-result.json (values read back after saving).
"""
from __future__ import annotations

import datetime as dt
import json
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T162"
FOLDER = "/Game/MonkeIsland/VFX/Blood"
PARTICLE_MATERIAL = f"{FOLDER}/M_Blood_Particle"
SPECIES_RULES = f"{FOLDER}/DA_BloodSpecies"
KINDS = {"MusketImpact": "musketImpactSystem", "SliceSpray": "sliceSpraySystem", "Chunks": "chunksSystem"}
CHARACTERS = "/Game/MonkeIsland/Characters"
# drops: BloodColor of the particle material (times the particle colour of each system: 1.3 fine drops, 1.0 jet, 0.55
# chunks); decals: Color of the species. Linear values.
BLOODS = {
    # coconut warrior: amber sap of the husk (the rating audit's "sève ambrée", not organic red)
    "Sap": {"particle": (0.42, 0.17, 0.02), "glow": 0.08},
    # crab guardian: blue hemolymph (hemocyanin, the blood of real crabs)
    "Hemolymph": {"particle": (0.03, 0.15, 0.45), "glow": 0.1},
    # ash watcher: molten lava of its veins, glowing (6 tonemapped to yellow in the thumbnail: 3, redder)
    "Lava": {"particle": (1.0, 0.20, 0.02), "glow": 3.0},
}
SPECIES = [
    {"name": "Bean", "skeletons": ["Bean/Meshes/SK_Bean_Body_Skeleton", "BeanMerchant/Meshes/SK_BeanMerchant_Skeleton"],
     "color": (0.30, 0.005, 0.004), "blood": None},
    {"name": "Villager", "skeletons": ["Villager/Meshes/SK_Villager_Skeleton"], "color": (0.32, 0.008, 0.006), "blood": None},
    {"name": "Coco", "skeletons": ["CocoWarrior/Meshes/SK_CocoWarrior_Skeleton"], "color": (0.30, 0.11, 0.012), "blood": "Sap"},
    {"name": "Crab", "skeletons": ["CrabGuardian/Meshes/SK_CrabGuardian_Skeleton"], "color": (0.015, 0.09, 0.30), "blood": "Hemolymph"},
    {"name": "Ash", "skeletons": ["AshWatcher/Meshes/SK_AshWatcher_Skeleton"], "color": (0.60, 0.13, 0.012), "blood": "Lava"},
]
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def object_path(path):
    name = path.rsplit("/", 1)[-1]
    return path if "." in name else f"{path}.{name}"


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    if not EAL.save_loaded_asset(asset, False):
        raise RuntimeError(f"could not save {asset.get_path_name()}")


def get(obj, names):
    raw = json.loads(ue.ToolsetLibrary.get_object_properties(obj, names), strict=False)
    lowered = {key.lower(): value for key, value in raw.items()} if isinstance(raw, dict) else {}
    return {name: lowered.get(name.lower()) for name in names}


def put(obj, **values):
    """Sets properties through ToolsetLibrary; a refusal is retried key by key to name the keys refused"""
    if ue.ToolsetLibrary.set_object_properties(obj, json.dumps(values)):
        return
    refused = [key for key, value in values.items() if not ue.ToolsetLibrary.set_object_properties(obj, json.dumps({key: value}))]
    if refused:
        raise RuntimeError(f"set_object_properties refused on {obj.get_path_name()}: {refused}")


def linear(rgb):
    return ue.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)


def build_instances(report):
    parent = load(PARTICLE_MATERIAL)
    if not parent:
        raise RuntimeError(f"{PARTICLE_MATERIAL} missing (T-153, scripts/vfx/build_blood_assets_ue.py)")
    params = [str(name) for name in MEL.get_vector_parameter_names(parent)] + [str(name) for name in MEL.get_scalar_parameter_names(parent)]
    report["particle_parameters"] = params
    if "BloodColor" not in params:
        raise RuntimeError(f"{PARTICLE_MATERIAL} has no BloodColor parameter: {params}")
    instances = {}
    for blood, look in BLOODS.items():
        path = f"{FOLDER}/MI_Blood_Particle_{blood}"
        instance = load(path)
        if not instance:
            factory = ue.MaterialInstanceConstantFactoryNew()
            instance = TOOLS.create_asset(path.rsplit("/", 1)[-1], FOLDER, ue.MaterialInstanceConstant, factory)
            report["created"].append(path)
        MEL.set_material_instance_parent(instance, parent)
        MEL.set_material_instance_vector_parameter_value(instance, "BloodColor", linear(look["particle"]))
        if "NightReadability" in params:
            MEL.set_material_instance_scalar_parameter_value(instance, "NightReadability", look["glow"])
        MEL.update_material_instance(instance)
        save(instance)
        instances[blood] = instance
        report["instances"][path] = {
            "BloodColor": str(MEL.get_material_instance_vector_parameter_value(instance, "BloodColor")),
            "NightReadability": MEL.get_material_instance_scalar_parameter_value(instance, "NightReadability") if "NightReadability" in params else None,
        }
    return instances


def sprite_renderers(system_path):
    """Sprite renderers of the emitter of a blood system (Fountain_1, as T-153 built them)"""
    emitter = ue.load_object(None, f"{object_path(system_path)}:Fountain_1")
    if not emitter:
        raise RuntimeError(f"emitter Fountain_1 of {system_path} not found")
    found = []
    for entry in get(emitter, ["RendererProperties"]).get("RendererProperties") or []:
        path = entry.get("refPath") if isinstance(entry, dict) else entry
        renderer = ue.load_object(None, path) if isinstance(path, str) else None
        if renderer and "Sprite" in renderer.get_class().get_name():
            found.append(renderer)
    if not found:
        renderer = ue.find_object(None, f"{emitter.get_path_name()}.Renderer")
        if renderer:
            found.append(renderer)
    if not found:
        raise RuntimeError(f"no sprite renderer in {system_path}")
    return found


def build_systems(instances, report):
    for blood, instance in instances.items():
        for kind in KINDS:
            source = f"{FOLDER}/NS_Blood_{kind}"
            path = f"{FOLDER}/NS_Blood_{kind}_{blood}"
            if not load(source):
                raise RuntimeError(f"{source} missing (T-153)")
            if not load(path):
                if not EAL.duplicate_asset(source, path):
                    raise RuntimeError(f"could not duplicate {source} to {path}")
                report["created"].append(path)
            system = load(path)
            for renderer in sprite_renderers(path):
                put(renderer, Material={"refPath": instance.get_path_name()})
            save(system)
            report["systems"][path] = [get(r, ["Material"])["Material"] for r in sprite_renderers(path)]


def build_species(report):
    if not hasattr(ue, "TikiBloodSpeciesRules"):
        report["pending"].append("DA_BloodSpecies: UTikiBloodSpeciesRules is not in this editor build (lead's build with "
                                 "T-162), run the action bloodtint again after it")
        return
    rules = load(SPECIES_RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.TikiBloodSpeciesRules)
        rules = TOOLS.create_asset("DA_BloodSpecies", FOLDER, ue.TikiBloodSpeciesRules, factory)
        report["created"].append(SPECIES_RULES)
    species = []
    for entry in SPECIES:
        skeletons = [object_path(f"{CHARACTERS}/{s}") for s in entry["skeletons"]]
        missing = [s for s in skeletons if not EAL.does_asset_exist(s.split(".")[0])]
        if missing:
            report["warnings"].append(f"{entry['name']}: skeleton(s) not found {missing}")
        value = {"name": entry["name"], "skeletons": skeletons,
                 "color": {"r": entry["color"][0], "g": entry["color"][1], "b": entry["color"][2], "a": 1.0}}
        for kind, key in KINDS.items():
            value[key] = object_path(f"{FOLDER}/NS_Blood_{kind}_{entry['blood']}") if entry["blood"] else ""
        species.append(value)
    put(rules, Species=species, SameWoundRadius=300.0)
    save(rules)
    report["species"] = get(rules, ["Species", "SameWoundRadius"])


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"task": "T-162", "started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "created": [], "instances": {},
              "systems": {}, "pending": [], "warnings": [], "success": False}
    try:
        instances = build_instances(report)
        build_systems(instances, report)
        build_species(report)
        report["success"] = True
    except Exception:
        report["error"] = traceback.format_exc()
        ue.log_error(report["error"])
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT / "blood-tint-result.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    if not report["success"]:
        raise RuntimeError("bloodtint failed, see monke_island/Saved/T162/blood-tint-result.json")


main()
