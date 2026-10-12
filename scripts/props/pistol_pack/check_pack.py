"""Checks a built pistol pack outside Unreal (T-248): manifest against the files, FBX units, bone and socket names of
the weapon base, the 15 gestures in TP and FP with identical lengths and events, the v0.1 event names, the baked
textures and their labels, the ammunition and the pickup statics. Also prints the mapping the integrator needs.

  python scripts/props/pistol_pack/check_pack.py                 both packs
  python scripts/props/pistol_pack/check_pack.py single|double   one pack
Exit 1 when a check fails. Reads only; no Blender, no Unreal.
"""
import hashlib
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
SPEC = json.load(open(os.path.join(HERE, "pack.json"), encoding="utf-8"))
V01_EVENTS = {
    "Equip": ["DrawWeapon", "Ready"], "Fire": ["Fire", "HammerDrop"], "AimFire": ["Fire", "HammerDrop"],
    "Reload": ["AssistLeft", "PaperPickup", "PaperTransfer", "RamrodGesture", "RamrodReturned", "HammerCocked", "Ready"],
    "DryFire": ["DryClick", "HammerDrop"], "Holster": ["StowWeapon"], "Cock": ["CockHammer"],
}
V01_CLIPS = ["Equip", "Idle", "Walk", "Sprint", "AimIn", "AimIdle", "AimOut", "Fire", "AimFire", "Reload", "DryFire", "Inspect", "Holster", "Cock"]
NEW_EVENTS = {"Discard": ["Empty", "Release"]}
V01_BONES = ["gun_root", "hammer", "trigger", "ramrod", "charge_visual", "socket_grip_r", "socket_muzzle", "socket_load_port", "socket_hammer",
             "socket_ramrod_grip"]
EXTRA_BONES = {"single": ["frizzen"], "double": ["frizzen", "hammer2", "frizzen2", "socket_muzzle2", "socket_hammer2"]}


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
    """Model node names of a binary FBX (bones are LimbNode models; the armature root is 'Armature')."""
    data = open(path, "rb").read()
    names = set()
    i = 0
    while True:
        i = data.find(b"\x00\x01Model", i)
        if i < 0:
            break
        j = data.rfind(b"\x00", 0, i - 1)
        # the string before "\x00\x01Model" is the node name (binary FBX: name\x00\x01class)
        k = i
        start = k
        while start > 0 and data[start - 1] >= 32 and data[start - 1] < 127:
            start -= 1
        names.add(data[start:k].decode("ascii", "ignore"))
        i += 7
    return names


def check(variant_key):
    variant = SPEC["variants"][variant_key]
    root = os.path.join(ROOT, "build", "packs", variant["pack"])
    problems = []
    info = {"pack": root.replace(os.sep, "/"), "variant": variant_key}
    mpath = os.path.join(root, variant["manifest"])
    if not os.path.isfile(mpath):
        return [f"manifest missing: {mpath}"], info
    m = json.load(open(mpath, encoding="utf-8"))
    info["format"] = f"{m['format']} v{m['version']}, {m['fps']} fps"
    # files and hashes
    units = {}
    for a in m["assets"]:
        p = os.path.join(root, a["path"])
        if not os.path.isfile(p):
            problems.append(f"missing file {a['path']}")
            continue
        if a.get("sha256") and sha256(p) != a["sha256"]:
            problems.append(f"sha256 mismatch {a['path']}")
        if p.lower().endswith(".fbx"):
            u = fbx_unit(p)
            units[str(u)] = units.get(str(u), 0) + 1
    info["fbx_units_cm_per_unit"] = units
    if set(units) - {"100.0"}:
        problems.append(f"FBX units not metres: {units}")
    # clips: 15 gestures, TP and FP, same lengths and events, v0.1 names
    scenes = {(s["view"], s["clip"]): s for s in m["scenes"]}
    clips = sorted({c for _, c in scenes})
    expected = V01_CLIPS + ["Discard"]
    if sorted(expected) != clips:
        problems.append(f"clips differ from the 15 expected: {sorted(set(expected) ^ set(clips))}")
    for c in clips:
        tp, fp = scenes.get(("TP", c)), scenes.get(("FP", c))
        if not tp or not fp:
            problems.append(f"{c}: missing view")
            continue
        if (tp["end"], tp["events"]) != (fp["end"], fp["events"]):
            problems.append(f"{c}: TP/FP differ")
        names = [e["name"] for e in tp["events"]]
        want = V01_EVENTS.get(c, NEW_EVENTS.get(c, []))
        if names != want:
            problems.append(f"{c}: events {names} != {want}")
        if tp["fps"] != 30:
            problems.append(f"{c}: fps {tp['fps']}")
    info["clips"] = {c: {"frames": scenes[("TP", c)]["end"], "seconds": round(scenes[("TP", c)]["end"] / 30.0, 3),
                         "events": {e["name"]: e["frame"] for e in scenes[("TP", c)]["events"]}} for c in clips if ("TP", c) in scenes}
    anims = [a for a in m["assets"] if a["type"] == "animation"]
    info["animation_fbx"] = len(anims)
    if len(anims) != 4 * len(clips):
        problems.append(f"{len(anims)} animation FBX, expected {4 * len(clips)}")
    # names of the clips
    cw = variant["clip_weapon"]
    for a in anims:
        name = os.path.splitext(os.path.basename(a["path"]))[0]
        exp = (f"A_Bean_{a['view']}_{cw}_{a['clip']}" if a["skeleton_group"] == "Bean" else f"A_{cw}_{a['view']}_{a['clip']}")
        if name != exp:
            problems.append(f"animation name {name} != {exp}")
    # bases and bones
    base = next((a for a in m["assets"] if a["type"] == "skeletal_mesh" and a["skeleton_group"] == variant["weapon"]), None)
    if not base:
        problems.append("weapon base missing")
    else:
        info["weapon_base"] = os.path.basename(base["path"])
        if os.path.basename(base["path"]) != f"SK_Bean_{variant['weapon']}.fbx":
            problems.append(f"weapon base name {base['path']}")
        bones = m["weapon_contract"]["bones"]
        want = V01_BONES + EXTRA_BONES[variant_key]
        if sorted(bones) != sorted(want):
            problems.append(f"bones {sorted(bones)} != {sorted(want)}")
        info["bones"] = bones
        info["sockets"] = m["weapon_contract"]["socket_positions"]
        fbx_bones = fbx_names(os.path.join(root, base["path"]))
        missing = [b for b in bones if b not in fbx_bones]
        if missing:
            problems.append(f"bones missing in the FBX: {missing}")
    fp = next((a for a in m["assets"] if a["type"] == "skeletal_mesh" and a["skeleton_group"] == "Bean" and a.get("view") == "FP"), None)
    if not fp or os.path.basename(fp["path"]) != variant["fp_base"] + ".fbx":
        problems.append(f"FP arms base: {fp and fp['path']}")
    info["fp_arms"] = fp and os.path.basename(fp["path"])
    # textures
    mat = m["materials"].get(variant["material"])
    if not mat:
        problems.append(f"material {variant['material']} missing from manifest.materials")
    else:
        labels = sorted(mat["textures"])
        if labels != ["BaseColor", "Normal", "ORM"]:
            problems.append(f"texture labels {labels}")
        for label, rel in mat["textures"].items():
            if not os.path.isfile(os.path.join(root, rel)):
                problems.append(f"texture missing {rel}")
        info["textures"] = mat["textures"]
    # statics
    statics = [a for a in m["assets"] if a["type"] == "static_mesh" and a["path"].lower().endswith(".fbx")]
    info["statics"] = {os.path.basename(a["path"]): a.get("view") for a in statics}
    if not any(a.get("view") == "pickup" for a in statics):
        problems.append("no pickup static")
    if variant_key == "single":
        for name in ("SM_Ammo_PistolCartridge", "SM_Ammo_BallPouch"):
            a = next((x for x in statics if os.path.basename(x["path"]) == name + ".fbx"), None)
            if not a or a.get("view") != "ammo" or not a.get("collision"):
                problems.append(f"ammo {name}: missing, not view ammo or no UCX")
        amat = m["materials"].get("M_Ammo_Pistol")
        if not amat or sorted(amat["textures"]) != ["BaseColor", "Normal", "ORM"]:
            problems.append("M_Ammo_Pistol textures")
    c = m["weapon_contract"]
    info["triangles"] = c["triangle_count"]
    info["bounds_m"] = [round(v, 4) for v in c["bounds"]["size"]]
    info["fp_framing"] = m["fp_framing"]
    L = c["bounds"]["size"][0]
    if not 0.32 <= L <= 0.38:
        problems.append(f"length {L} outside the v0.1 contract 0.32-0.38 m")
    return problems, info


def main():
    keys = [a for a in sys.argv[1:] if a in SPEC["variants"]] or list(SPEC["variants"])
    failed = False
    for key in keys:
        problems, info = check(key)
        print(json.dumps(info, indent=1, ensure_ascii=False))
        for p in problems:
            print("PROBLEM", key, p)
        print(f"{key}: {'CHECK OK' if not problems else 'CHECK FAILED'}")
        failed |= bool(problems)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
