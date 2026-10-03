"""Japanese villages of the expedition fragments: one village on each village site of every fragment (sites.villages
Z1-Z3 of docs/reference/map/<ile>/fragments/fragments.json, one per pacifiable zone), built with the layout of
scripts/map/place_villages.py (road, square, shrine, plots, points of interest, lights, doors, clearing shapes).

Usage:
  python scripts/map/fragment_villages.py raraka [--fragment FRG_Raraka_19 ...] [--regen] [--no-preview]

How a site becomes a village:
  - the layout is tried from the site centre and from 4 points 5 m away (along and across the site axis), with the
    axis of the site and the axis of the motu; the best score whose square stays within MAX_OFFSET_M of the site
    centre wins (the camps of scripts/map/set_camp_fragments.py keep 120 m away from the site centres);
  - the marae sites of the fragment are kept free (MARAE_RADIUS_M);
  - themes inside a fragment as in place_villages.py: richesses on the lagoon side nearest to the shore, bois where
    the grove is thickest, vivres for the third; the name comes from the theme and the fragment number (fictional).
  - a village of a zone fills every plot its site allows (ZONE_MAX_HOUSES, 16 plots along its road);
  - hamlets (hameaux) between the zones, HAMLETS per fragment at most: 4 to HAMLET_MAX_HOUSES houses on the wide
    land left HAMLET_SPACING_M from every village and hamlet and HAMLET_MARAE_CLEAR_M from the marae sites; no flag,
    no guardian, a few villagers (villager_count). The camps (scripts/map/place_camps.py) must keep away from them too.
Nothing is placed in the level: the game builds the villages of the fragment it plays (AVillageSpawner, see
docs/features/villages-japonais.md) and clears the vegetation under them at runtime with the same shapes.

Memory (tracked): villages/fragments/<FRG>.json, one per fragment, kept as is unless --regen; villages/fragments/
index.json sums them up (centres, radii, themes, names, arrival of the fragment). Previews: villages/fragments/
preview_<FRG>.png. The Unreal data assets are filled from the memory by scripts/map/fragment_villages_ue.py.
"""
import argparse
import importlib.util
import json
import math
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("place_villages", ROOT / "scripts" / "map" / "place_villages.py")
pvil = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pvil)

MAX_OFFSET_M = 20.0         # square -> site centre (m): the camps keep 120 m from the site centre
FALLBACK_OFFSET_M = 35.0    # tried when nothing fits within MAX_OFFSET_M, reported
SHIFT_M = 5.0               # trial centres around the site centre (m)
MARAE_RADIUS_M = 22.0       # a marae platform and its yard, kept free (m)
ZONE_MAX_HOUSES = 16        # a village of a zone fills every plot its site allows (16 plots along the road)
# hamlets between the zones (the owner wants more villages in a game): no flag, no guardian, a few villagers
HAMLETS = 3                 # wanted per fragment
HAMLET_MAX_HOUSES = 6
HAMLET_SPACING_M = 220.0    # hamlet square -> any other village or hamlet square (m)
HAMLET_MARAE_CLEAR_M = 120.0    # hamlet square -> a marae site of the fragment (m)
HAMLET_MIN_SHORE_M = 22.0   # candidate centre -> water (m)
HAMLET_STEP_M = 16.0        # candidate grid inside the land of the fragment (m)
HAMLET_TRIES = 36           # full trial layouts per fragment at most
HAMLET_NAMES = ("Aoi", "Akane", "Hasu", "Hibari", "Hotate", "Ibuki", "Iwa", "Kaede", "Kaki", "Karasu",
                "Kasa", "Kikyo", "Kogi", "Kujaku", "Kurumi", "Kusa", "Mame", "Masu", "Mikan", "Mitsu",
                "Momo", "Naka", "Nashi", "Natsu", "Negi", "Niji", "Nire", "Ogi", "Rin", "Saba",
                "Sagi", "Sasa", "Seri", "Shiba", "Shika", "Sora", "Suzu", "Tachi", "Taki", "Tane",
                "Tanuki", "Tara", "Tsuge", "Tsuta", "Uguisu", "Uri", "Usagi", "Uzura", "Wara", "Washi",
                "Yomogi", "Yuki", "Yuri", "Zakuro", "Hagi", "Kibi", "Kuri", "Mugi", "Nazuna", "Suge")
# fictional village names by theme, one per fragment number (FRG_..._01 takes the first)
NAMES = {
    "richesses": ("Kogane", "Shinju", "Tama", "Gin", "Akari", "Hikari", "Hoshi", "Tsuki", "Asahi", "Yuhi",
                  "Hinode", "Sango", "Kiku", "Fuji", "Tsubaki", "Hotaru", "Misaki", "Tsuru", "Kasumi", "Nagisa"),
    "bois": ("Take", "Matsu", "Sugi", "Hinoki", "Kusu", "Yanagi", "Kaba", "Momiji", "Sakura", "Ume",
             "Yashi", "Ashi", "Yoshi", "Kaya", "Tochi", "Kashi", "Keyaki", "Kiri", "Shii", "Enoki"),
    "vivres": ("Uo", "Kani", "Ebi", "Ami", "Nori", "Kai", "Tako", "Kame", "Same", "Shio",
               "Hama", "Iso", "Umi", "Ura", "Oki", "Seto", "Minato", "Fune", "Izumi", "Mizu"),
}
LABELS = {"richesses": "Richesses et crique d'extraction", "bois": "Bois et passage court",
          "vivres": "Nourriture et abri"}


def fragment_number(fid):
    return int(fid.rsplit("_", 1)[-1])


def best_layout(ctx, site, seed):
    """Best trial layout of a site: (layout, centre tried, axis name, offset of the square in m) or None."""
    G = ctx["G"]
    c0 = np.asarray(site["center_cm"], float) / 100.0
    axes = (("site", pvil.unit(np.asarray(site["axis"], float))), ("motu", pvil.motu_axis(G, c0)))
    t, n = axes[0][1], pvil.perp(axes[0][1])
    centres = [c0, c0 + SHIFT_M * t, c0 - SHIFT_M * t, c0 + SHIFT_M * n, c0 - SHIFT_M * n]
    trials = []
    for k, c in enumerate(centres):
        for name, axis in axes:
            rng = np.random.default_rng(seed + 31 * k + (0 if name == "site" else 7))
            lay = pvil.Layout(ctx, c, axis, None, rng, ZONE_MAX_HOUSES).build()
            if lay.score() < 0 or getattr(lay, "plaza", None) is None:
                continue
            trials.append((lay.score(), float(np.linalg.norm(lay.plaza - c0)), lay, c, name, seed + 31 * k + (0 if name == "site" else 7)))
    for limit in (MAX_OFFSET_M, FALLBACK_OFFSET_M):
        ok = [tr for tr in trials if tr[1] <= limit]
        if ok:
            return max(ok, key=lambda tr: tr[0])
    return None


def in_land(P, rings):
    """Even-odd test of points P (n, 2), in m, in the land rings of a fragment (cm, {hole, points})."""
    P = np.asarray(P, float).reshape(-1, 2) * 100.0
    inside = np.zeros(len(P), bool)
    for ring in rings:
        Q = np.asarray(ring["points"], float)
        x1, y1 = Q[:, 0], Q[:, 1]
        x2, y2 = np.roll(x1, -1), np.roll(y1, -1)
        for a, b, c, d in zip(x1, y1, x2, y2):
            cross = ((b > P[:, 1]) != (d > P[:, 1])) & (P[:, 0] < (c - a) * (P[:, 1] - b) / ((d - b) or 1e-9) + a)
            inside ^= cross
    return inside


def hamlets_of(ctx, frag, taken, num):
    """Hamlets of a fragment on the wide land left between its villages and its marae: (layout, seed) list."""
    G, objs = ctx["G"], ctx["objs"]
    x0, y0, x1, y1 = (v / 100.0 for v in frag["bbox_cm"])
    xs = np.arange(x0, x1, HAMLET_STEP_M)
    ys = np.arange(y0, y1, HAMLET_STEP_M)
    P = np.array([(x, y) for y in ys for x in xs])
    if not len(P):
        return []
    ok = (G.shore(P) >= HAMLET_MIN_SHORE_M) & (G.bldg(P) >= pvil.BLDG_MIN)
    P = P[ok]
    P = P[in_land(P, frag["land_polygons_cm"])] if len(P) else P
    marae = [np.asarray(m["center_cm"], float) / 100.0 for m in frag["sites"].get("marae", [])]
    # the square stands about 14 m from the centre tried: 15 m of slack, checked again on the square
    for c in taken:
        if len(P):
            P = P[np.linalg.norm(P - c, axis=1) >= HAMLET_SPACING_M - 15.0]
    for m in marae:
        if len(P):
            P = P[np.linalg.norm(P - m, axis=1) >= HAMLET_MARAE_CLEAR_M - 15.0]
    if not len(P):
        return []
    # wide land first, few palm trunks to cut
    wide = np.minimum(G.shore(P, smooth=True), 60.0)
    trunks = objs.trunk_tree.query_ball_point(P, 30.0, return_length=True) if objs.trunk_tree is not None else 0
    order = np.argsort(-(wide - 0.25 * np.asarray(trunks)))
    chosen, tried = [], 0
    squares = list(taken)
    for i in order:
        if len(chosen) >= HAMLETS or tried >= HAMLET_TRIES:
            break
        c = P[i]
        if any(np.linalg.norm(c - s) < HAMLET_SPACING_M - 15.0 for s in squares):
            continue
        tried += 1
        seed = (int(frag.get("seed", 0)) * 1009 + num * 131 + int(i)) % (2 ** 31)
        lay = pvil.Layout(ctx, c, pvil.motu_axis(G, c), None, np.random.default_rng(seed), HAMLET_MAX_HOUSES).build()
        if lay.score() < 0 or getattr(lay, "plaza", None) is None:
            continue
        if any(np.linalg.norm(lay.plaza - s) < HAMLET_SPACING_M for s in squares) or \
                any(np.linalg.norm(lay.plaza - m) < HAMLET_MARAE_CLEAR_M for m in marae):
            continue
        chosen.append((lay, seed))
        squares.append(lay.plaza)
    return chosen


def themes_of(ctx, layouts):
    """Theme of each layout of a fragment, as place_villages.assign_themes (one of each for three villages)."""
    G, objs = ctx["G"], ctx["objs"]
    info = []
    for k, lay in enumerate(layouts):
        c = lay.plaza
        lagoon = float(G.lagoon([c])[0]) > 0.5
        trunks = len(objs.trunk_tree.query_ball_point(c, 120.0)) if objs.trunk_tree is not None else 0
        info.append((k, (0 if lagoon else 1, float(G.shore([c])[0])), trunks))
    out = {}
    rest = list(info)
    if rest:
        k = min(rest, key=lambda x: x[1])
        out[k[0]] = "richesses"
        rest.remove(k)
    if rest:
        k = max(rest, key=lambda x: x[2])
        out[k[0]] = "bois"
        rest.remove(k)
    for k in rest:
        out[k[0]] = "vivres"
    return out


def build_fragment(ctx, frag, kit):
    """The villages of one fragment (memory dicts, like villages.json), with the marae of the fragment kept free."""
    G, objs = ctx["G"], ctx["objs"]
    marae = [(np.asarray(m["center_cm"], float) / 100.0, MARAE_RADIUS_M, "marae") for m in frag["sites"].get("marae", [])]
    fctx = dict(ctx, actors=list(ctx["actors"]) + marae)
    num = fragment_number(frag["id"])
    chosen = []
    for site in frag["sites"]["villages"]:
        seed = int(frag.get("seed", 0)) * 1009 + num * 101 + int(site["id"].lstrip("Z") or 0)
        got = best_layout(fctx, site, seed % (2 ** 31))
        if got is None:
            print(f"[fragments] {frag['id']} {site['id']}: no village fits")
            continue
        chosen.append((site, got))
    themes = themes_of(fctx, [got[2] for _, got in chosen])
    villages = []
    squares = []
    for k, (site, (score, offset, lay, centre, axis_name, seed)) in enumerate(chosen):
        theme = themes[k]
        lay.theme = theme
        # rebuilt with its theme: the food village has a storehouse behind every house
        lay = pvil.Layout(fctx, centre, lay.t, theme, np.random.default_rng(seed), lay.max_houses).build()
        if lay.score() < 0:
            print(f"[fragments] {frag['id']} {site['id']}: the {theme} layout no longer fits")
            continue
        offset = float(np.linalg.norm(lay.plaza - np.asarray(site["center_cm"], float) / 100.0))
        vid = f"F{num:02d}_{site['id']}"
        name = f"{NAMES[theme][(num - 1) % len(NAMES[theme])]}-mura"
        v = lay.to_memory(vid, name, theme, LABELS[theme], frag["id"], seed)
        v["kind"] = "zone"
        v["fragment"] = frag["id"]
        v["site"] = {"id": site["id"], "center_cm": site["center_cm"], "axis": site["axis"],
                     "houses_estimate": site.get("houses_estimate"), "axis_used": axis_name,
                     "offset_m": round(offset, 1)}
        v["stats"]["cleared"], v["stats"]["cleared_trunks"] = pvil.clearing_stats(v, objs)
        v["pois"], v["lights"] = pvil.village_extras(v, G, kit)
        v["doors"] = pvil.village_doors(v, G)
        v["_removed"] = sorted(lay.removed)
        villages.append(v)
        squares.append(lay.plaza)
        flag = "" if offset <= MAX_OFFSET_M else f"  (square {offset:.0f} m from the site: beyond {MAX_OFFSET_M:.0f} m)"
        print(f"[fragments] {vid} {name} ({theme}): {v['stats']['houses']} houses, {len(v['spawns'])} spawns, "
              f"{len(v['pois'])} points of interest, {len(v['lights'])} lights, {len(v['doors'])} doors, "
              f"square {offset:.1f} m from the site{flag}")
    # hamlets on the wide land left between the zones and the marae
    for k, (lay, seed) in enumerate(hamlets_of(fctx, frag, squares, num), start=1):
        vid = f"F{num:02d}_H{k}"
        name = f"{HAMLET_NAMES[((num - 1) * HAMLETS + k - 1) % len(HAMLET_NAMES)]}-sato"
        v = lay.to_memory(vid, name, "hameau", "Hameau entre les zones", frag["id"], seed)
        v["kind"] = "hameau"
        v["fragment"] = frag["id"]
        v["villager_count"] = int(min(max(v["stats"]["houses"], 3), 6))
        c = np.asarray(v["zone"]["center_cm"][:2], float) / 100.0
        v["site"] = {"id": f"H{k}", "center_cm": [round(c[0] * 100, 1), round(c[1] * 100, 1)], "axis": None,
                     "houses_estimate": None, "axis_used": "motu", "offset_m": 0.0}
        v["stats"]["cleared"], v["stats"]["cleared_trunks"] = pvil.clearing_stats(v, objs)
        v["pois"], v["lights"] = pvil.village_extras(v, G, kit)
        v["doors"] = pvil.village_doors(v, G)
        v["_removed"] = sorted(lay.removed)
        villages.append(v)
        print(f"[fragments] {vid} {name} (hameau): {v['stats']['houses']} houses, {v['villager_count']} villagers, "
              f"{len(v['pois'])} points of interest, {len(v['lights'])} lights")
    return villages


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("island")
    ap.add_argument("--fragment", action="append", default=[], help="only this fragment id (repeatable)")
    ap.add_argument("--regen", action="store_true", help="rebuild the fragments even when they are in memory")
    ap.add_argument("--no-preview", action="store_true")
    args = ap.parse_args()

    ref = ROOT / "docs" / "reference" / "map" / args.island
    doc = json.loads((ref / "fragments" / "fragments.json").read_text(encoding="utf-8"))
    out = ref / "villages" / "fragments"
    out.mkdir(parents=True, exist_ok=True)
    frags = [f for f in doc["fragments"] if not args.fragment or f["id"] in args.fragment]
    todo = [f for f in frags if args.regen or not (out / f"{f['id']}.json").exists()]

    t0 = time.time()
    A = pvil.pv.Atoll(ref)
    G = pvil.Ground(A)
    kit = pvil.Kit(pvil.KIT)
    objs = pvil.Objects(args.island)
    actors, _ = pvil.load_actors(ref)
    ctx = {"G": G, "kit": kit, "objs": objs, "actors": actors, "start": np.array(pvil.DEFAULT_START_CM) / 100.0}
    print(f"[fragments] context ready in {time.time() - t0:.0f} s; {len(todo)} fragment(s) to build")

    for frag in todo:
        villages = build_fragment(ctx, frag, kit)
        mem = {"version": 1, "island": args.island, "generator": "scripts/map/fragment_villages.py",
               "frame": "UE cm: X east, Y south, Z up; yaw in degrees (Unreal); meshes " + pvil.UE_MESHES,
               "fragment": frag["id"], "display_name": frag.get("display_name"),
               "arrival_cm": frag["arrival"]["location_cm"],
               "villages": [{k: val for k, val in v.items() if not k.startswith("_")} for v in villages]}
        (out / f"{frag['id']}.json").write_text(json.dumps(mem, indent=1, ensure_ascii=False), encoding="utf-8")
        if not args.no_preview:
            arrival = np.asarray(frag["arrival"]["location_cm"][:2], float) / 100.0
            pvil.preview(out / f"preview_{frag['id']}.png", ctx, villages, {}, arrival, kit)

    # the summary of every fragment in memory
    index = {"version": 1, "island": args.island, "generator": "scripts/map/fragment_villages.py",
             "frame": "UE cm: X east, Y south, Z up", "fragments": []}
    for frag in doc["fragments"]:
        p = out / f"{frag['id']}.json"
        if not p.exists():
            continue
        mem = json.loads(p.read_text(encoding="utf-8"))
        index["fragments"].append({
            "id": frag["id"], "arrival_cm": mem["arrival_cm"],
            "villages": [{"id": v["id"], "kind": v.get("kind", "zone"), "site": v["site"]["id"], "name": v["name"], "theme": v["theme"],
                          "center_cm": v["zone"]["center_cm"], "radius_cm": v["zone"]["radius_cm"],
                          "site_center_cm": v["site"]["center_cm"], "offset_m": v["site"]["offset_m"],
                          "houses": v["stats"]["houses"]} for v in mem["villages"]]})
    (out / "index.json").write_text(json.dumps(index, indent=1, ensure_ascii=False), encoding="utf-8")
    zones = [v for f in index["fragments"] for v in f["villages"] if v["kind"] == "zone"]
    hamlets = [v for f in index["fragments"] for v in f["villages"] if v["kind"] == "hameau"]
    far = [(v["id"], v["offset_m"]) for v in zones if v["offset_m"] > MAX_OFFSET_M]
    print(f"[fragments] index: {len(index['fragments'])} fragments, {len(zones)} villages ({sum(v['houses'] for v in zones)} houses), "
          f"{len(hamlets)} hamlets ({sum(v['houses'] for v in hamlets)} houses); squares beyond {MAX_OFFSET_M:.0f} m: {far}")
    print(f"[fragments] done in {time.time() - t0:.0f} s")


if __name__ == "__main__":
    main()
