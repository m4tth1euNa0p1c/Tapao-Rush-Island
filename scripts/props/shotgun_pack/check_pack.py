"""Offline check of the local shotgun pack (T-247), outside Blender and Unreal: the same controls as
scripts/character/import_pirate_weapons.py --check (files, sha256 of the manifest, FBX UnitScaleFactor 100, destination
plan, generated clip table) plus the contract with the second PC's v0.1 pack: 16 gestures with identical lengths,
events, bones and sockets (the C++ table PirateWeaponClips.inl and the import do not change), Discard as the only
extra gesture, textures listed by label, UCX hulls in the static FBX.

  python scripts/props/shotgun_pack/check_pack.py [--pack build/packs/bean-pirate-shotgun] [--reference <v0.1 manifest>]
"""
import argparse
import hashlib
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_PACK = os.path.join(ROOT, "build", "packs", "bean-pirate-shotgun")
REFERENCE = r"C:\Users\matth\OneDrive\maquette-blender\maquettes\bean-pirate-shotgun\shotgun-manifest.json"
INL = os.path.join(ROOT, "monke_island", "Source", "monke_island", "Weapons", "PirateWeaponClips.inl")
V01_CLIPS = ["Equip", "Idle", "Walk", "Sprint", "AimIn", "AimIdle", "AimOut", "FirePump", "AimFirePump", "ReloadStart", "ReloadInsert",
             "ReloadEnd", "ReloadEmpty", "DryFire", "Inspect", "Holster"]
EXTRA_CLIPS = ["Discard"]
IMPORT_STATIC = ["SM_Bean_PirateShotgun", "SM_Shotgun_Full", "SM_Shotgun_Spent", "SM_Shotgun_Slug"]
AMMO_STATIC = ["SM_Shotgun_Spent", "SM_Ammo_Shell", "SM_Ammo_ShellBox"]
GESTURE = {"FirePump": "Fire", "AimFirePump": "AimFire"}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fbx_unit(path):
    data = open(path, "rb").read()
    i = data.find(b"UnitScaleFactor")
    if i < 0:
        return None
    j = data.find(b"D", data.find(b"Number", i))
    return struct.unpack("<d", data[j + 1:j + 9])[0]


def fbx_names(path):
    data = open(path, "rb").read()
    return set(m.decode(errors="replace") for m in re.findall(rb"([A-Za-z0-9_.\-]{3,64})\x00\x01Model", data))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--pack", default=DEFAULT_PACK)
    p.add_argument("--reference", default=REFERENCE)
    args = p.parse_args()
    pack = os.path.abspath(args.pack)
    problems, notes = [], []
    manifest = json.load(open(os.path.join(pack, "shotgun-manifest.json"), encoding="utf-8"))
    report = {"pack": pack, "format": manifest["format"], "version": manifest["version"], "fps": manifest["fps"]}

    # files, hashes, units
    units = {}
    for entry in manifest["assets"]:
        path = os.path.join(pack, entry["path"])
        if not os.path.isfile(path):
            problems.append("missing " + entry["path"])
            continue
        if entry.get("sha256") and sha256(path) != entry["sha256"]:
            problems.append("hash mismatch " + entry["path"])
        if path.endswith(".fbx"):
            u = fbx_unit(path)
            units[str(u)] = units.get(str(u), 0) + 1
            if u != 100.0:
                problems.append("UnitScaleFactor %s in %s" % (u, entry["path"]))
    report["fbx_units"] = units

    # clips: 17 gestures in both views, names of the animation files
    scenes = {(s["view"], s["clip"]): s for s in manifest["scenes"]}
    for clip in V01_CLIPS + EXTRA_CLIPS:
        for view in ("TP", "FP"):
            s = scenes.get((view, clip))
            if s is None:
                problems.append("scene missing %s %s" % (view, clip))
                continue
            bean = "exports/Animations/A_Bean_%s_Shotgun_%s.fbx" % (view, clip)
            weapon = "exports/Animations/A_Shotgun_%s_%s.fbx" % (view, clip)
            for rel in (bean, weapon):
                if not os.path.isfile(os.path.join(pack, rel)):
                    problems.append("animation missing " + rel)
            if s["actors"]["bean"].get("animation_fbx") != bean or s["actors"]["weapon"].get("animation_fbx") != weapon:
                problems.append("animation names of %s %s differ from the import contract" % (view, clip))
            if s["fps"] != 30:
                problems.append("fps %s in %s %s" % (s["fps"], view, clip))
    report["clips"] = {"views": sorted(set(v for v, _ in scenes)), "gestures": sorted(set(c for _, c in scenes)), "count": len(scenes)}
    anims = [a for a in manifest["assets"] if a["type"] == "animation"]
    report["animation_assets"] = {"bean": sum(1 for a in anims if a["skeleton_group"] == "Bean"),
                                  "weapon": sum(1 for a in anims if a["skeleton_group"] == "PirateShotgun")}

    # bases and statics of the import list, ammunition statics, UCX nodes
    for rel in ("exports/Bases/SK_Bean_Shotgun_TP.fbx", "exports/Bases/SK_Bean_Shotgun_FP.fbx", "exports/Bases/SK_Bean_PirateShotgun.fbx"):
        if not os.path.isfile(os.path.join(pack, rel)):
            problems.append("base missing " + rel)
    for name in IMPORT_STATIC + AMMO_STATIC + ["SM_Ammo_Shell", "SM_Ammo_ShellBox"]:
        path = os.path.join(pack, "exports", "Static", name + ".fbx")
        if not os.path.isfile(path):
            problems.append("static missing " + name)
            continue
        names = fbx_names(path)
        if name not in names:
            problems.append("%s: no mesh node named %s in the FBX (%s)" % (name, name, sorted(names)))
        if not any(n.startswith("UCX_" + name) for n in names):
            problems.append("%s: no UCX_%s_NN hull" % (name, name))
    ammo_rows = [a for a in manifest["assets"] if a["type"] == "static_mesh" and a.get("view") == "ammo"]
    report["ammo_assets"] = sorted(set(os.path.basename(a["path"]) for a in ammo_rows))
    for base in ("exports/Bases/SK_Bean_PirateShotgun.fbx", "exports/Animations/A_Bean_TP_Shotgun_Idle.fbx", "exports/Animations/A_Shotgun_FP_Idle.fbx"):
        path = os.path.join(pack, base)
        if os.path.isfile(path):
            names = fbx_names(path)
            if "Armature" not in names:
                problems.append(base + ": no node named Armature (" + ", ".join(sorted(names)) + ")")
            if any(n.startswith("Armature.") for n in names):
                problems.append(base + ": an Armature.NNN node (extra root bone in Unreal)")

    # contract with v0.1: lengths, events, bones, sockets
    if os.path.isfile(args.reference):
        ref = json.load(open(args.reference, encoding="utf-8"))
        ref_scenes = {(s["view"], s["clip"]): s for s in ref["scenes"]}
        for clip in V01_CLIPS:
            for view in ("TP", "FP"):
                a, b = scenes.get((view, clip)), ref_scenes.get((view, clip))
                if a is None or b is None:
                    continue
                if (a["end"] - a["start"]) != (b["end"] - b["start"]):
                    problems.append("%s %s: %d frames, v0.1 has %d" % (view, clip, a["end"] - a["start"], b["end"] - b["start"]))
                ea = sorted((e["frame"], e["name"], e.get("socket")) for e in a["events"])
                eb = sorted((e["frame"], e["name"], e.get("socket")) for e in b["events"])
                if ea != eb:
                    problems.append("%s %s: events differ from v0.1: %s vs %s" % (view, clip, ea, eb))
        ref_bones = set(ref["weapon_contract"]["bones"])
        bones = set(manifest["weapon_contract"]["bones"])
        if bones != ref_bones:
            problems.append("weapon bones differ: +%s -%s" % (sorted(bones - ref_bones), sorted(ref_bones - bones)))
        for name, pos in ref["weapon_contract"]["socket_positions"].items():
            mine = manifest["weapon_contract"]["socket_positions"].get(name)
            if mine is None or any(abs(x - y) > 1e-4 for x, y in zip(mine, pos)):
                problems.append("socket %s: %s, v0.1 %s" % (name, mine, pos))
        report["v01_reference"] = {"format": ref["format"], "version": ref["version"], "bones_identical": bones == ref_bones}
    else:
        notes.append("v0.1 reference manifest not found (OneDrive): lengths/events compared with the C++ table only")

    # generated clip table against the .inl on disk (Shotgun section): the 16 gestures identical, Discard extra
    inl = open(INL, encoding="utf-8").read() if os.path.isfile(INL) else ""
    section = inl.split("AddShotgunClips")[1].split("AddMusketClips")[0] if "AddShotgunClips" in inl else ""
    table_problems = []
    for clip in V01_CLIPS:
        s = scenes.get(("TP", clip))
        if s is None:
            continue
        ev = ", ".join("{ TEXT(\"%s\"), %d }" % (e["name"], e["frame"] - s["start"]) for e in s["events"])
        loop = "true" if clip in {"Idle", "Walk", "Sprint", "AimIdle"} else "false"
        line = "Out.Add(FFirearmClip::Make(EFirearmGesture::%s, %d, %d, %s, { %s }));" % (GESTURE.get(clip, clip), s["end"] - s["start"], s["fps"], loop, ev) \
            if ev else "Out.Add(FFirearmClip::Make(EFirearmGesture::%s, %d, %d, %s, {}));" % (GESTURE.get(clip, clip), s["end"] - s["start"], s["fps"], loop)
        if line not in section:
            table_problems.append(line)
    if table_problems:
        problems.append("clip table: %d gesture(s) not in PirateWeaponClips.inl: %s" % (len(table_problems), table_problems[:2]))
    report["clip_table"] = "16 v0.1 gestures identical to PirateWeaponClips.inl" if not table_problems else "differs"
    notes.append("Discard (TP+FP) is delivered beyond the 16 gestures: pirate_weapon_clips.py would emit EFirearmGesture::Discard, "
                 "which the enum does not have yet (integrator: add it, or skip unknown gestures in the generator).")

    # textures by label
    for mat in ("M_Shotgun", "M_Shotgun_Spent", "M_Ammo_Shell", "M_Ammo_ShellBox"):
        row = manifest.get("materials", {}).get(mat)
        if not row:
            problems.append("material %s not in manifest.materials" % mat)
            continue
        for label in ("BaseColor", "Normal", "ORM"):
            rel = row.get("textures", {}).get(label)
            if not rel or not os.path.isfile(os.path.join(pack, rel)):
                problems.append("%s: texture %s missing (%s)" % (mat, label, rel))
    report["textures"] = manifest.get("textures", [])
    report["fp_framing"] = manifest.get("fp_framing")
    report["weapon"] = {"bounds_m": manifest["weapon_contract"]["bounds"]["size"], "triangles": manifest["weapon_contract"].get("triangle_count"),
                        "bones": len(manifest["weapon_contract"]["bones"])}
    report["plan"] = {
        "base": "/Game/MonkeIsland/Weapons/Shotgun/SK_Bean_PirateShotgun (own skeleton)",
        "fp_arms": "/Game/MonkeIsland/Weapons/Shotgun/SK_Bean_Shotgun_FP on /Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton",
        "bean_clips": report["animation_assets"]["bean"], "weapon_clips": report["animation_assets"]["weapon"],
        "static": ["/Game/MonkeIsland/Weapons/Shotgun/Static/" + s for s in IMPORT_STATIC + ["SM_Ammo_Shell", "SM_Ammo_ShellBox"]],
        "runtime_tested_by_pack": manifest.get("unreal_import", {}).get("runtime_tested"),
        "integrator": "import_pirate_weapons.py WEAPONS['Shotgun']['local_pack'] = True and pirate_weapon_clips.py WEAPONS['Shotgun'] -> "
                      "build/packs/bean-pirate-shotgun/shotgun-manifest.json; add SM_Ammo_Shell / SM_Ammo_ShellBox to the static list",
    }
    report["problems"] = problems
    report["notes"] = notes
    print(json.dumps(report, indent=1, ensure_ascii=False))
    print("CHECK OK" if not problems else "CHECK FAILED (%d)" % len(problems))
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
