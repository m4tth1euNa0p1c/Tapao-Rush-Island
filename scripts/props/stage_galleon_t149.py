"""Stage the T-149 galleon delivery without executing or modifying OneDrive sources."""
import ast
import datetime as dt
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path("C:/Users/matth/OneDrive/maquette-blender")
MODEL = SOURCE / "maquettes/galion-anglais"
DEST = ROOT / "art/export/Galleon_T149/20261006_1203"
MAP_KEYS = {
    "diff": ("_diff", "_color", "basecolor", "albedo"),
    "rough": ("_rough", "roughness"),
    "normal": ("nor_gl", "normalgl"),
    "ao": ("_ao", "ambientocclusion"),
    "metal": ("_metal", "metalness"),
}


def main():
    records = {}

    def stage(source, relative):
        data = source.read_bytes()
        stamp = source.stat()
        target = DEST / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() and target.read_bytes() != data:
            raise RuntimeError(f"Existing snapshot differs: {target}")
        target.write_bytes(data)
        records[relative] = dict(source=str(source), file=relative, size=len(data),
                                 mtime_ns=stamp.st_mtime_ns,
                                 sha256=hashlib.sha256(data).hexdigest())

    for source in sorted((MODEL / "exports").glob("*.fbx")):
        stage(source, source.name)
    for name in ("galleon-report.json", "build-report.json", "maquette.json"):
        stage(MODEL / name, name)
    stage(MODEL / "ue/GALLEON_UE.md", "GALLEON_UE.md")
    stage(SOURCE / "factory/kits/galleon/materials.py", "materials.py")
    stage(SOURCE / "factory/kits/galleon/bow_lion.py", "bow_lion.py")
    stage(SOURCE / "factory/kits/galleon/sails.py", "sails.py")
    stage(SOURCE / "factory/assets/textures/ship/SOURCES.md", "TEXTURE_SOURCES.md")
    tree = ast.parse((DEST / "materials.py").read_text(encoding="utf-8"))
    recipes = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "RECIPES" for t in node.targets
        ):
            for key, value in zip(node.value.keys, node.value.values):
                assert isinstance(value, ast.Call) and isinstance(value.func, ast.Name)
                assert value.func.id == "dict" and not value.args
                recipes[ast.literal_eval(key)] = {
                    item.arg: ast.literal_eval(item.value) for item in value.keywords
                }
    report = json.loads((DEST / "galleon-report.json").read_text(encoding="utf-8"))
    assert not report["build_errors"], report["build_errors"]
    used = {m for part in report["exports"].values() for m in part["material_names"]}
    recipes["canvas_patch"] = dict(recipes["canvas"], patch=True)
    recipes["canvas_seam"] = dict(kind="flat",color="8D7A68",rough=0.95,tones=False)
    recipes["gold_lion"] = dict(recipes["gold"])
    recipes["gold_lion_dark"] = dict(recipes["gold"],tone=0.55)
    materials = {}
    for key, recipe in recipes.items():
        name = "M_Galleon_" + "".join(word.capitalize() for word in key.split("_"))
        if name not in used:
            continue
        recipe["maps"] = {}
        if "asset" in recipe:
            asset = recipe["asset"]
            folders = [SOURCE / "factory/assets/textures/ship" / asset] + [
                SOURCE / "art/textures" / sub / asset
                for sub in ("characters", "props/minka", "props", "ground", "")
            ]
            folder = next((f for f in folders if f.is_dir()), None)
            if folder is None:
                raise RuntimeError(f"Missing texture folder: {asset}")
            for kind, patterns in MAP_KEYS.items():
                found = next((f for f in sorted(folder.iterdir(), reverse=True)
                              if f.suffix.lower() in (".jpg", ".png")
                              and "_dx" not in f.name.lower()
                              and any(s in f.name.lower() for s in patterns)), None)
                if found:
                    relative = f"textures/{asset}/{found.name}"
                    recipe["maps"][kind] = relative
                    if relative not in records:
                        stage(found, relative)
        materials[name] = recipe
    assert used == set(materials), used - set(materials)
    # A second complete read verifies stability across the staging interval.
    for record in records.values():
        source = Path(record["source"])
        assert source.stat().st_mtime_ns == record["mtime_ns"], source
        assert hashlib.sha256(source.read_bytes()).hexdigest() == record["sha256"], source
    manifest = dict(task="T-149", captured_at=dt.datetime.now(dt.timezone.utc).isoformat(),
                    files=list(records.values()))
    for name, data in (("material-recipes.json", materials), ("source-manifest.json", manifest)):
        (DEST / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Stable snapshot: {len(records)} files, {len(materials)} materials, "
          f"{sum(r['size'] for r in records.values()) / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
