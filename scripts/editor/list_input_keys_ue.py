"""Every key mapped in the input mapping contexts of /Game (key -> context: action), to pick a free key.
Lead session action "inputkeys". Report: monke_island/Saved/T186/input-keys.json."""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T186/input-keys.json"
registry = ue.AssetRegistryHelpers.get_asset_registry()
keys = {}
for data in registry.get_assets_by_class(ue.TopLevelAssetPath("/Script/EnhancedInput", "InputMappingContext")):
    package = str(data.package_name)
    if not package.startswith("/Game/"):
        continue
    context = ue.load_asset(package)
    try:
        mappings = list(context.get_editor_property("default_key_mappings").get_editor_property("mappings"))
    except Exception:  # noqa: BLE001
        try:
            mappings = list(context.get_editor_property("mappings"))
        except Exception:  # noqa: BLE001
            mappings = []
    for mapping in mappings:
        action = mapping.get_editor_property("action")
        key = str(mapping.get_editor_property("key").get_editor_property("key_name"))
        keys.setdefault(key, []).append(f"{package.rsplit('/', 1)[1]}: {action.get_name() if action else None}")
OUT.write_text(json.dumps(dict(sorted(keys.items())), indent=1), encoding="utf-8")
