"""Categories of the merchant's offers (owner 2026-10-11, T-320: "organiser la boutique").

FShopOffer.Category (EShopCategory) groups the shop screen: Armes à feu, Armes blanches, Explosifs, Munitions, Outils et
objets, Consommables, each group cheapest first. An offer left on Auto is deduced at run time from what it gives
(UShopCatalog::ResolveCategory: a packet of ammunition, a grenade, a firearm, a blade, an item class named *Consumable*,
else a tool), so the screen is ordered even without this script. This script writes that deduced category into each
offer of DA_ShopCatalog still on Auto, so that the asset shows it and a designer can change it there; an offer whose
category was set by hand is kept. Nothing else is touched (items, prices, lines, order of the array). Idempotent: a
second run changes nothing.

Run after the C++ build of T-320 (the property `category` and `get_offer_category` must exist), editor open, no game
running (a save during a PIE fails). Lead session action suggested: "shopcategories".
MONKE_SHOP_CATEGORIES_DRY=1: report only, nothing saved.
Report: monke_island/Saved/T-320/shop-categories.json (offers before / after, the order of the screen, the layout).
"""
import json
import os
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T-320/shop-categories.json"
CATALOG = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
EAL = ue.EditorAssetLibrary
DRY = bool(os.environ.get("MONKE_SHOP_CATEGORIES_DRY"))
# the order of the screen (EShopCategory after Auto) and the titles of its groups
ORDER = ["FIREARM", "BLADE", "EXPLOSIVE", "AMMUNITION", "TOOL", "CONSUMABLE"]
TITLES = {"FIREARM": "Armes à feu", "BLADE": "Armes blanches", "EXPLOSIVE": "Explosifs", "AMMUNITION": "Munitions",
          "TOOL": "Outils et objets", "CONSUMABLE": "Consommables"}


def enum_name(value):
    """EShopCategory as Python gives it (ShopCategory.FIREARM, <ShopCategory.FIREARM: 1>) -> FIREARM"""
    name = getattr(value, "name", None)
    if isinstance(name, str) and name:
        return name.upper()
    text = str(value).strip("<>")
    return text.split(".")[-1].split(":")[0].strip().upper()


def in_game():
    try:
        return bool(ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor())
    except Exception:  # noqa: BLE001
        return False


def lock_state():
    """Who holds the editor lock (for the report only: this script saves one DataAsset, no close, no build)"""
    try:
        lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
        return {"owner": lock.get("owner"), "expires": lock.get("expires"), "session_owner": os.environ.get("MONKE_LOCK_OWNER")}
    except Exception as error:  # noqa: BLE001
        return {"error": str(error)}


def main():
    report = {"task": "T-320", "catalog": CATALOG, "dry_run": DRY, "lock": lock_state(), "offers": [], "changed": 0,
              "saved": False, "errors": []}
    catalog = EAL.load_asset(CATALOG) if EAL.does_asset_exist(CATALOG) else None
    if not catalog:
        report["errors"].append(f"{CATALOG} not found")
    elif not hasattr(ue, "ShopCategory") or not hasattr(catalog, "get_offer_category"):
        report["errors"].append("EShopCategory / UShopCatalog::GetOfferCategory missing: build the C++ of T-320 and restart the editor first")
    else:
        offers = list(catalog.get_editor_property("offers"))
        auto = ue.ShopCategory.AUTO
        for index, offer in enumerate(offers):
            item = offer.get_editor_property("item")
            ammo = offer.get_editor_property("ammo")
            before = offer.get_editor_property("category")
            # the C++ rule on the stored offer: its own category, else the deduced one
            deduced = catalog.get_offer_category(index)
            row = {"index": index, "item": item.get_name() if item else None, "ammo": ammo.get_name() if ammo else None,
                   "price": offer.get_editor_property("price"), "before": enum_name(before), "after": enum_name(before)}
            if before == auto and deduced != auto:
                offer.set_editor_property("category", deduced)
                row["after"] = enum_name(deduced)
                report["changed"] += 1
            report["offers"].append(row)
        if report["changed"] and not DRY:
            if in_game():
                report["errors"].append("a game is running: nothing saved, run again after the PIE")
            else:
                catalog.set_editor_property("offers", offers)
                report["saved"] = bool(EAL.save_loaded_asset(catalog, only_if_is_dirty=False))
                if not report["saved"]:
                    report["errors"].append("save_loaded_asset failed (asset read-only or checked out elsewhere?)")
        # the screen as ShopRules::Arrange orders it: groups in the fixed order, cheapest first, then catalog order
        rank = {name: position for position, name in enumerate(ORDER)}
        rows = sorted(report["offers"], key=lambda r: (rank.get(r["after"], rank["TOOL"]), r["price"], r["index"]))
        screen = []
        for r in rows:
            title = TITLES.get(r["after"], TITLES["TOOL"])
            if not screen or screen[-1]["group"] != title:
                screen.append({"group": title, "offers": []})
            screen[-1]["offers"].append(f"{r['item'] or r['ammo']} {r['price']} c")
        report["screen_order"] = screen
        layout = catalog.get_editor_property("screen")
        report["screen_layout"] = {key: layout.get_editor_property(key) for key in
                                   ("width", "max_width_fraction", "max_height_fraction", "navigation_padding", "wheel_scroll_multiplier")}
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding="utf-8")
    ue.log(f"[shop_categories] {report['changed']} offer(s) categorised, saved={report['saved']}, errors={report['errors']}")


main()
