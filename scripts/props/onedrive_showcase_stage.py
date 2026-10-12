"""Stage the delivered T-179 meshes and textures; OneDrive remains read-only."""
import ast
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path("C:/Users/matth/OneDrive/maquette-blender")
DEST = ROOT / "art/export/OneDriveShowcase/20261006"
FILES = {}


def copy(source, relative):
    target = DEST / relative
    before = source.stat()
    data = source.read_bytes()
    after = source.stat()
    assert (before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), f"Sync in progress: {source}"
    assert data, f"Empty delivery: {source}"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    FILES[relative] = {"source": str(source), "bytes": len(data),
                       "sha256": hashlib.sha256(data).hexdigest(), "mtime_ns": after.st_mtime_ns}
    return relative


def main():
    for package in ("artefacts-polynesiens", "temple-reliquaire"):
        folder = SOURCE / "maquettes" / package
        for p in folder.glob("*.json"):
            copy(p, f"{package}/{p.name}")
        for p in (folder / "exports").glob("*.fbx"):
            copy(p, f"{package}/exports/{p.name}")
        # Artefact maps include explicit DirectX normals and packed ORM maps.
        if package == "artefacts-polynesiens":
            for p in (folder / "textures").rglob("*"):
                if p.is_file():
                    copy(p, f"{package}/textures/{p.relative_to(folder / 'textures').as_posix()}")
    # Extract the source temple's literal PBR recipes without importing Blender
    # modules or running its generator (which would rebuild the entire scene).
    kit = SOURCE / "factory/kits/temple_kit.py"
    tree = ast.parse(kit.read_text(encoding="utf-8"))
    recipes = {}
    func = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "mats")
    mapping = next(n.value for n in func.body if isinstance(n, ast.Return))
    for call in mapping.values:
        name, folder = [ast.literal_eval(a) for a in call.args]
        recipe = {k.arg: ast.literal_eval(k.value) for k in call.keywords}
        texture_dir = SOURCE / "art/textures" / folder
        recipe["maps"] = {}
        for kind, keys in {"diff": ("_diff", "_color"), "rough": ("_rough", "roughness"),
                           "normal": ("nor_gl", "normalgl")}.items():
            found = [p for p in sorted(texture_dir.iterdir()) if p.suffix.lower() in (".png", ".jpg")
                     and any(k in p.name.lower() for k in keys)]
            assert found, f"Missing {kind}: {texture_dir}"
            p = found[0]
            recipe["maps"][kind] = copy(p, f"TempleTextures/{folder}/{p.name}")
        recipes[name] = recipe
    recipes["MAT-Temple_TikiWood"] = dict(recipes["MAT-Temple_WoodWeathered"])
    (DEST / "temple-material-recipes.json").write_text(json.dumps(recipes, indent=2), encoding="utf-8")
    for relative in ("factory/kits/temple_kit.py", "factory/kits/marae_kit.py", "factory/kits/artifacts/treasure.py"):
        copy(SOURCE / relative, "SourceReferences/" + Path(relative).name + ".txt")
    for folder in (SOURCE / "art", SOURCE / "art/textures"):
        for p in folder.glob("*.md"):
            copy(p, "SourceReferences/" + folder.name + "_" + p.name)
    result = {"task": "T-179", "staged_at": dt.datetime.now(dt.timezone.utc).isoformat(), "files": FILES}
    (DEST / "source-manifest.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({"files": len(FILES), "bytes": sum(f["bytes"] for f in FILES.values()), "destination": str(DEST)}))


if __name__ == "__main__":
    main()
