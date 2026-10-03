"""Vegetation cleared under the generated villages (scripts/map/place_villages.py).

The clearing shapes (UE cm: rectangles of the buildings and small props including their eaves, circle of the
square, corridors of the road and of the shrine path) are stored with each village in
docs/reference/map/<ile>/villages/villages.json, without margin. The margin depends on the kind of shape and on
the plant (MARGIN_CM): a naupaka is up to 4.8 m wide, so it must stand 3 m beyond the eaves of a house not to
touch it, while a fern only needs 1 m.
Used by:
  - place_villages.py --apply-clearing: filters the vegetation CSVs already written, without moving any other
    instance (the k-d blocks keep their bounds, so only the DataTables under a village change);
  - place_vegetation.py and palms_to_datatable.py: a new vegetation run keeps the villages clear.
The unfiltered CSVs are kept in raw/<ile>_vegetation/_orig/: removing every village restores them.
"""
import csv
import json
import math
import shutil
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]

# plant categories, from the mesh name (first match wins; anything else is "ground")
CATEGORIES = (("SM_CoconutPalm_Fallen", "fallen"), ("SM_CoconutPalm_Juvenile", "young"),
              ("SM_CoconutPalm_Young", "young"), ("SM_CoconutPalm_Sprout", "ground"), ("SM_CoconutPalm_", "palm"),
              ("SM_Pandanus_Young", "young"), ("SM_Naupaka", "bush"), ("SM_MikiMiki", "bush"), ("SM_Tahinu", "bush"),
              ("SM_Pandanus", "bush"))
CATS = ("bush", "fallen", "ground", "palm", "young")
# margin (cm) beyond a shape, by kind of shape and plant category: crown radius plus a gap
MARGIN_CM = {
    "building": {"palm": 300.0, "young": 350.0, "bush": 300.0, "fallen": 500.0, "ground": 100.0},
    "small": {"palm": 80.0, "young": 240.0, "bush": 240.0, "fallen": 300.0, "ground": 40.0},   # fences: clean front yards
    "plaza": {"palm": 150.0, "young": 250.0, "bush": 250.0, "fallen": 400.0, "ground": 50.0},
    "road": {"palm": 80.0, "young": 150.0, "bush": 150.0, "fallen": 300.0, "ground": 30.0},
    "path": {"palm": 80.0, "young": 150.0, "bush": 150.0, "fallen": 300.0, "ground": 30.0},
}
PAD_CM = max(max(m.values()) for m in MARGIN_CM.values())


def category(mesh_name):
    return next((c for k, c in CATEGORIES if mesh_name.startswith(k)), "ground")


def memory_path(island):
    return ROOT / "docs" / "reference" / "map" / island / "villages" / "villages.json"


class Clearing:
    def __init__(self, shapes):
        self.shapes = shapes
        pts = []
        for s in shapes:
            if s["type"] == "rect":
                r = math.hypot(*s["half"]) + PAD_CM
                pts += [(s["center"][0] - r, s["center"][1] - r), (s["center"][0] + r, s["center"][1] + r)]
            elif s["type"] == "circle":
                r = s["radius"] + PAD_CM
                pts += [(s["center"][0] - r, s["center"][1] - r), (s["center"][0] + r, s["center"][1] + r)]
            else:
                p = np.asarray(s["points"], float)
                pts += [tuple(p.min(0) - s["half_width"] - PAD_CM), tuple(p.max(0) + s["half_width"] + PAD_CM)]
        P = np.asarray(pts, float) if pts else np.zeros((0, 2))
        self.lo, self.hi = (P.min(0), P.max(0)) if len(P) else (None, None)

    @classmethod
    def load(cls, island):
        p = memory_path(island)
        if not p.exists():
            return None
        doc = json.loads(p.read_text(encoding="utf-8"))
        shapes = [s for v in doc.get("villages", []) for s in v.get("clear", [])]
        return cls(shapes) if shapes else None

    def mask(self, xy, cats=None):
        """xy (n, 2) in UE cm, cats (n,) plant categories (CATS, "ground" when omitted) -> True where the instance
        stands under a village or close enough for its crown to touch a building."""
        xy = np.asarray(xy, float).reshape(-1, 2)
        out = np.zeros(len(xy), bool)
        if self.lo is None or not len(xy):
            return out
        near = np.nonzero(np.all((xy >= self.lo) & (xy <= self.hi), axis=1))[0]
        if not len(near):
            return out
        Q = xy[near]
        cat_index = {c: i for i, c in enumerate(CATS)}
        C = np.array([cat_index.get(c, cat_index["ground"]) for c in
                      (np.asarray(cats)[near] if cats is not None else ["ground"] * len(near))], int)
        hit = np.zeros(len(Q), bool)
        for s in self.shapes:
            table = MARGIN_CM[s.get("kind", "building")]
            m = np.array([table[c] for c in CATS])[C]
            if s["type"] == "rect":
                a = math.radians(s["yaw"])
                d = Q - np.asarray(s["center"], float)
                lx = d[:, 0] * math.cos(a) + d[:, 1] * math.sin(a)
                ly = -d[:, 0] * math.sin(a) + d[:, 1] * math.cos(a)
                hit |= (np.abs(lx) <= s["half"][0] + m) & (np.abs(ly) <= s["half"][1] + m)
            elif s["type"] == "circle":
                hit |= np.sqrt(((Q - np.asarray(s["center"], float)) ** 2).sum(1)) <= s["radius"] + m
            else:
                pts = np.asarray(s["points"], float)
                for a, b in zip(pts[:-1], pts[1:]):
                    ab = b - a
                    t = np.clip(((Q - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
                    hit |= np.sqrt(((Q - (a + t[:, None] * ab)) ** 2).sum(1)) <= s["half_width"] + m
        out[near] = hit
        return out


def _rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.reader(f))
    return rows[0], rows[1:]


def xy_of(rows):
    """UE cm of GameplayTagTableRow rows (DevComment = "x,y,z|p,y,r|sx,sy,sz")."""
    return np.array([[float(v) for v in r[2].split("|", 1)[0].split(",")[:2]] for r in rows], float).reshape(-1, 2)


def cats_of(rows):
    """Plant category of GameplayTagTableRow rows (Tag = mesh path)."""
    cache = {}
    for r in rows:
        mesh = r[1].rsplit("/", 1)[-1].split(".")[0]
        if mesh not in cache:
            cache[mesh] = category(mesh)
    return [cache[r[1].rsplit("/", 1)[-1].split(".")[0]] for r in rows]


def filter_csv(src, dst, clearing):
    header, rows = _rows(src)
    keep = np.ones(len(rows), bool) if clearing is None or not rows else ~clearing.mask(xy_of(rows), cats_of(rows))
    with open(dst, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, quoting=csv.QUOTE_MINIMAL)
        w.writerow(header)
        for r, k in zip(rows, keep):
            if k:
                w.writerow(r)
    return int(len(rows) - keep.sum())


def reapply(raw, clearing):
    """Filter every DT_*.csv of raw/ from its unfiltered copy (made the first time). Returns {file: removed}."""
    raw = Path(raw)
    orig = raw / "_orig"
    orig.mkdir(exist_ok=True)
    report = {}
    for f in sorted(raw.glob("DT_*.csv")):
        o = orig / f.name
        if not o.exists():
            shutil.copy2(f, o)
        report[f.name] = filter_csv(o, f, clearing)
    return report


def refresh(raw, names, clearing):
    """After a vegetation script wrote fresh CSVs: keep them as the unfiltered copies, then filter."""
    raw = Path(raw)
    orig = raw / "_orig"
    orig.mkdir(exist_ok=True)
    report = {}
    for n in names:
        shutil.copy2(raw / n, orig / n)
        report[n] = filter_csv(orig / n, raw / n, clearing)
    return report
