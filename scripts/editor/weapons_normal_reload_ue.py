"""Owner 2026-10-11: "remets le rechargement normal pour toutes les armes". Since 2026-10-09 a firearm with nothing loaded
and nothing left in its pouch was thrown away on the next trigger pull (UFirearmRules::bDiscardWhenEmpty, the shop said
"15 balles, puis il est usé"). Every firearm keeps its weapon now: empty, it clicks (DryFire) and reloads as soon as
ammunition is picked up or bought (R, or fire on an empty barrel with rounds in the pouch).
- every data asset of a UFirearmRules class under /Game/MonkeIsland (pistols, shotgun, blunderbuss, muskets, rifle,
  machine gun, grenade bag): bDiscardWhenEmpty = false;
- DA_ShopCatalog: the offers' lines that said the weapon wears out say it reloads.
Lead session action "normalreload" (editor open, no game running). Report: monke_island/Saved/T329/normal-reload.json.
Replays change nothing.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T329/normal-reload.json"
CATALOG = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
WORN = (", puis il est usé", " puis il est usé", ", puis elle est usée", " puis elle est usée")
RELOADS = ", se recharge avec ses munitions"
EAL = ue.EditorAssetLibrary


def main():
    report = {"rules": {}, "offers": []}
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_class(ue.TopLevelAssetPath("/Script/monke_island", "FirearmRules"), True):
        path = str(data.package_name)
        if not path.startswith("/Game/MonkeIsland/") or "/Backups/" in path:
            continue
        rules = EAL.load_asset(path)
        before = bool(rules.get_editor_property("discard_when_empty"))
        if before:
            rules.set_editor_property("discard_when_empty", False)
            if not EAL.save_loaded_asset(rules, only_if_is_dirty=False):
                raise RuntimeError(f"{path} not saved")
        report["rules"][path] = {"before": before, "after": bool(rules.get_editor_property("discard_when_empty"))}

    catalog = EAL.load_asset(CATALOG)
    if catalog:
        offers = list(catalog.get_editor_property("offers"))
        changed = False
        for offer in offers:
            line = str(offer.get_editor_property("description"))
            new = line
            for worn in WORN:
                if worn in new:
                    new = new.replace(worn, RELOADS)
            if new != line:
                offer.set_editor_property("description", new)
                changed = True
            report["offers"].append([line, new])
        if changed:
            catalog.set_editor_property("offers", offers)
            if not EAL.save_loaded_asset(catalog, only_if_is_dirty=False):
                raise RuntimeError(f"{CATALOG} not saved")
        report["catalog_saved"] = changed
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding="utf-8")
    ue.log(f"NORMAL_RELOAD {sum(1 for r in report['rules'].values() if r['before'])} rules changed")


main()
