"""Archive the atoll world as a self-contained, reusable environment kit.

Usage (from the repo root):
  blender --background --factory-startup art/blender/atoll_lookdev.blend --python scripts/atoll/archive_world.py
          [-- --out art/archives/environnements/atoll_tuamotu]

Output (<out>):
  scene/atoll_tuamotu.blend   compressed, orphans purged, every image relinked into the kit (relative paths, nothing packed)
  textures/<Category>/        Ground, Vegetation, Props, Water, Sky — T_<Name>_<D|N_GL|R|CH|ARM>
                              (normals in OpenGL convention, as Blender expects; T_Water_N stays DirectX for Unreal)
  previews/                   look-dev renders
  manifest.json               every texture: category, size, colour space, source, materials using it
"""
import json
import shutil
import sys
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "art"
NAME = "atoll_tuamotu"

LAYERS = {"sand_dry": "SandDry", "sand_wet": "SandWet", "soil": "Soil", "litter": "Litter", "rubble": "Rubble",
          "feo": "Feo", "wrack": "Wrack"}
LAYER_MAPS = {"ch": "CH", "n": "N_GL"}
PLANTS = {"coconut_leaflet": "CoconutLeaflet", "coconut_eophyll": "CoconutEophyll", "pandanus": "PandanusLeaf",
          "naupaka": "NaupakaLeaf", "tahinu": "TahinuLeaf", "mikimiki_twig": "MikiMikiTwig", "fern_lobe": "FernLobe",
          "ipomoea": "IpomoeaLeaf", "grass": "GrassBlade"}
PLANT_MAPS = {"basecolor": "D", "normal_gl": "N_GL", "roughness": "R"}
SCANS = {"palm_tree_bark": ("Vegetation", "PalmTrunk"), "palm_bark": ("Vegetation", "PalmFiber"),
         "bark_brown_02": ("Vegetation", "ShrubBark"), "lambis_shell": ("Props", "LambisShell")}
SCAN_MAPS = {"diff": "D", "nor_gl": "N_GL", "rough": "R", "arm": "ARM"}
# Not used by the scene but part of the world: the other sky options and the Unreal water normal
EXTRA = [(ART / "hdri" / "secluded_beach_4k.hdr", "Sky"), (ART / "hdri" / "spiaggia_di_mondello_4k.hdr", "Sky"),
         (ART / "textures" / "water" / "T_Water_N.png", "Water")]
PREVIEWS = ["sheet_*.jpg", "scene_CAM_*.jpg", "lineup_LIB_*.jpg", "terrain_layers.jpg", "leaf_textures.jpg",
            "ue_assets_thumbs.jpg"]


def target(src):
    """(category, kit file name) for a source texture of the look-dev."""
    p = Path(src)
    stem, ext = p.stem, p.suffix.lower()
    if p.parent.name == "terrain_layers":
        layer, kind = stem.rsplit("_", 1)
        return "Ground", f"T_{LAYERS[layer]}_{LAYER_MAPS[kind]}{ext}"
    if p.parent.name == "plants":
        for suffix, short in PLANT_MAPS.items():
            if stem.endswith("_" + suffix):
                return "Vegetation", f"T_{PLANTS[stem[:-len(suffix) - 1]]}_{short}{ext}"
    for key, (cat, name) in SCANS.items():
        if stem.startswith(key + "_"):
            kind = stem[len(key) + 1:].rsplit("_", 1)[0]  # diff_2k -> diff, nor_gl_1k -> nor_gl
            return cat, f"T_{name}_{SCAN_MAPS[kind]}{ext}"
    if ext in (".hdr", ".exr"):
        return "Sky", p.name
    raise KeyError(f"no kit name for {src}")


def image_users():
    """Image name -> names of the materials / worlds / node groups whose node tree uses it."""
    users = {}
    for owner in list(bpy.data.materials) + list(bpy.data.worlds) + list(bpy.data.node_groups):
        tree = owner if isinstance(owner, bpy.types.NodeTree) else owner.node_tree
        for node in tree.nodes if tree else ():
            if getattr(node, "image", None):
                users.setdefault(node.image.name, set()).add(owner.name)
    return users


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = Path(argv[argv.index("--out") + 1]) if "--out" in argv else ART / "archives" / "environnements" / NAME
    out = out if out.is_absolute() else ROOT / out
    tex = out / "textures"
    users = image_users()
    manifest = {"scene": f"scene/{NAME}.blend", "source_blend": bpy.data.filepath, "textures": []}

    for im in bpy.data.images:
        if not im.filepath:
            continue
        src = Path(bpy.path.abspath(im.filepath)).resolve()
        cat, fname = target(src)
        dst = tex / cat / fname
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.exists():
            shutil.copy2(src, dst)
        elif im.packed_file:
            dst.write_bytes(im.packed_file.data)
        else:
            raise FileNotFoundError(src)
        if im.packed_file:
            im.unpack(method="USE_ORIGINAL")  # drop the packed copy first: unpacking resets the image path
        im.filepath = str(dst)
        manifest["textures"].append({"file": dst.relative_to(out).as_posix(), "category": cat,
                                     "size": list(im.size), "colorspace": im.colorspace_settings.name,
                                     "source": src.relative_to(ROOT).as_posix() if src.is_relative_to(ROOT) else str(src),
                                     "used_by": sorted(users.get(im.name, ()))})
        print(f"  {cat:10s} {fname:34s} <- {src.name}")

    for src, cat in EXTRA:
        dst = tex / cat / src.name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        manifest["textures"].append({"file": dst.relative_to(out).as_posix(), "category": cat, "size": None,
                                     "colorspace": None, "source": src.relative_to(ROOT).as_posix(), "used_by": []})
        print(f"  {cat:10s} {src.name:34s} (extra)")

    prev = out / "previews"
    prev.mkdir(parents=True, exist_ok=True)
    for pattern in PREVIEWS:
        for p in sorted((ART / "_previews").glob(pattern)):
            shutil.copy2(p, prev / p.name)

    bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    bpy.context.preferences.filepaths.save_version = 0  # no .blend1 next to the archive
    blend = out / "scene" / f"{NAME}.blend"
    blend.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend), compress=True, relative_remap=True)
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_mainfile(compress=True)

    manifest["textures"].sort(key=lambda t: t["file"])
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"[archive] {blend} ({blend.stat().st_size / 1e6:.1f} MB), {len(manifest['textures'])} textures")


if __name__ == "__main__":
    main()
