"""The cutlass in the galleon's shop (owner 2026-10-07: "ajoute le sabre dans la boutique"): lists the offers of
DA_ShopCatalog, then, when monke_island/Saved/T186/shopcutlass-apply.json exists ({"price": N, "description": "...",
"after": "<item name to insert after>"}), adds DA_Item_Cutlass with that price and line. Writes
monke_island/Saved/T186/shop-cutlass.json. Action "shopcutlass"."""
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

CATALOG = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
CUTLASS = "/Game/MonkeIsland/Weapons/Cutlass/DA_Item_Cutlass"
APPLY = ROOT / "monke_island/Saved/T186/shopcutlass-apply.json"
out = {}
kitlib.check_lock()
catalog = kitlib.load(CATALOG)
cutlass = kitlib.load(CUTLASS)
assert catalog and cutlass, "DA_ShopCatalog or DA_Item_Cutlass missing"


def rows():
    return [{"item": o.get_editor_property("item").get_name() if o.get_editor_property("item") else None,
             "price": o.get_editor_property("price"), "description": str(o.get_editor_property("description"))}
            for o in catalog.get_editor_property("offers")]


out["before"] = rows()
props = {}
for p in ("display_name", "description", "short_description", "icon", "grid_size", "mass_kg", "value"):
    try:
        props[p] = str(cutlass.get_editor_property(p))
    except Exception:  # noqa: BLE001
        pass
out["cutlass_props"] = props
if APPLY.exists():
    spec = json.loads(APPLY.read_text(encoding="utf-8"))
    offers = list(catalog.get_editor_property("offers"))
    if not any(o.get_editor_property("item") == cutlass for o in offers):
        offer = ue.ShopOffer()
        offer.set_editor_property("item", cutlass)
        offer.set_editor_property("price", int(spec["price"]))
        offer.set_editor_property("description", ue.Text(spec["description"]))
        at = len(offers)
        for i, o in enumerate(offers):
            if o.get_editor_property("item") and o.get_editor_property("item").get_name() == spec.get("after"):
                at = i + 1
        offers.insert(at, offer)
        catalog.set_editor_property("offers", offers)
        kitlib.save(catalog)
    out["after"] = rows()
(ROOT / "monke_island/Saved/T186/shop-cutlass.json").write_text(json.dumps(out, indent=1, ensure_ascii=False), encoding="utf-8")
