"""Generates Weapons/CutlassPoses.inl from the second PC's blade test (bean-blade-test/blade-report.json) and
combat timeline (bean-cutlass-anim/timeline.json).

The cutlass grip is keyed in the bean mesh space (Unreal puppet component space: cm, +Y forward, +Z up, -X right).
Blender world (m, Bean facing -Y) -> mesh space: (x, y, z) -> (x, -y, z) * 100, rotation R -> D R D, D = diag(1, -1, 1).

  python scripts/props/cutlass_poses.py [maquette-blender root]
"""
import json
import math
import os
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\matth\OneDrive\maquette-blender"
REPORT = os.path.join(ROOT, "maquettes", "bean-blade-test", "blade-report.json")
TIMELINE = os.path.join(ROOT, "maquettes", "bean-cutlass-anim", "timeline.json")
PARAMS = os.path.join(ROOT, "maquettes", "bean-cutlass", "item-report.json")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "monke_island", "Source", "monke_island",
                   "Weapons", "CutlassPoses.inl")
FPS = 24


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def translation(v):
    return [[1, 0, 0, v[0]], [0, 1, 0, v[1]], [0, 0, 1, v[2]], [0, 0, 0, 1]]


def inverse_rigid(m):
    r = [[m[j][i] for j in range(3)] for i in range(3)]
    t = [-sum(r[i][k] * m[k][3] for k in range(3)) for i in range(3)]
    return [r[0] + [t[0]], r[1] + [t[1]], r[2] + [t[2]], [0, 0, 0, 1]]


def to_unreal(m):
    """Blender world matrix -> (location cm, rotator pitch/yaw/roll deg) in the bean mesh space."""
    col = lambda j: [m[0][j], m[1][j], m[2][j]]  # noqa: E731
    flip = lambda v: [v[0], -v[1], v[2]]  # noqa: E731
    x, y, z = flip(col(0)), [-c for c in flip(col(1))], flip(col(2))
    loc = [m[0][3] * 100.0, -m[1][3] * 100.0, m[2][3] * 100.0]
    # FRotationMatrix / FMatrix::Rotator() convention (rows = axes)
    pitch = math.degrees(math.atan2(x[2], math.sqrt(x[0] ** 2 + x[1] ** 2)))
    yaw = math.degrees(math.atan2(x[1], x[0]))
    # roll: angle of the Y axis around X, measured from the yaw/pitch frame without roll
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    sy_axis = [-sy, cy, 0.0]
    sz_axis = [-sp * cy, -sp * sy, cp]
    # FRotationMatrix: Y = cos(roll) Y0 - sin(roll) Z0
    roll = math.degrees(math.atan2(-sum(a * b for a, b in zip(y, sz_axis)), sum(a * b for a, b in zip(y, sy_axis))))
    check_axes((pitch, yaw, roll), x, y, z)
    return loc, (pitch, yaw, roll)


def check_axes(rot, x, y, z):
    """Rebuilds the axes like FRotationMatrix and checks they match (the conversion is easy to get wrong)."""
    p, yw, r = (math.radians(a) for a in rot)
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(yw), math.cos(yw), math.sin(r), math.cos(r)
    ax = [cp * cy, cp * sy, sp]
    ay = [sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp]
    az = [-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp]
    for got, want in ((ax, x), (ay, y), (az, z)):
        if max(abs(a - b) for a, b in zip(got, want)) > 1e-3:
            raise ValueError("rotator {} does not rebuild the axes".format(rot))


def main():
    with open(REPORT, encoding="utf-8") as fh:
        rep = json.load(fh)
    with open(TIMELINE, encoding="utf-8") as fh:
        tl = json.load(fh)
    with open(PARAMS, encoding="utf-8") as fh:
        blade_len = json.load(fh)["params"]["blade_len"]
    pl = rep["placements"]
    ph = tl["phases_s"]
    att = {n: a["keys"] for n, a in pl["attacks"].items()}
    guard = att["guard"][0]
    scab_rest, scab_end = pl["scabbard"], pl["draw"]["scab_end"]
    sword_end = pl["draw"]["sword_end"]
    # sword sheathed, in the scabbard frame: fully drawn = pushed out along the scabbard by the blade length
    x_end = [scab_end[0][0], scab_end[1][0], scab_end[2][0]]
    n = math.sqrt(sum(c * c for c in x_end))
    out = [c / n * (blade_len + 0.012) for c in x_end]
    in_scab = mat_mul(inverse_rigid(scab_end), mat_mul(translation(out), sword_end))
    sheathed_rest = mat_mul(scab_rest, in_scab)
    sheathed_end = mat_mul(scab_end, in_scab)

    def hit(frame, phase):
        return (frame - 1) / FPS - ph[phase][0]

    def rel(phase, start, edge=1):
        return ph[phase][edge] - ph[start][0]

    tracks = {
        "DrawKeys": [(0.0, sheathed_rest, 0.0), (rel("grab", "grab"), sheathed_end, 1.0),
                     (rel("pull", "grab"), sword_end, 1.0), (rel("to_guard", "grab"), guard, 0.0)],
        "SlashRightKeys": [(0.0, guard, 0.0)] + [(rel(p, "r_wind"), k, 0.0) for p, k in
                                                  zip(("r_wind", "r_strike", "r_follow"), att["slash_r"])]
                          + [(rel("r_back", "r_wind"), guard, 0.0)],
        "SlashLeftKeys": [(0.0, guard, 0.0)] + [(rel(p, "l_wind"), k, 0.0) for p, k in
                                                 zip(("l_wind", "l_strike", "l_follow"), att["slash_l"])]
                         + [(rel("l_back", "l_wind"), guard, 0.0)],
        "FinisherKeys": [(0.0, guard, 0.0), (rel("f_charge", "f_charge"), att["finisher"][0], 0.0),
                         (rel("f_strike", "f_charge", 0) + 2.0 / FPS, att["finisher"][0], 0.0),
                         (rel("f_strike", "f_charge"), att["finisher"][1], 0.0),
                         (rel("f_follow", "f_charge"), att["finisher"][2], 0.0),
                         (rel("f_back", "f_charge"), guard, 0.0)],
        "ThrustKeys": [(0.0, guard, 0.0), (rel("t_wind", "t_wind"), att["thrust"][0], 0.0),
                       (rel("t_strike", "t_wind"), att["thrust"][1], 0.0), (rel("t_back", "t_wind"), guard, 0.0)],
        "SheatheKeys": [(0.0, guard, 0.0), (rel("sh_to", "sh_to"), sword_end, 1.0),
                        (rel("sh_push", "sh_to"), sheathed_end, 1.0), (rel("sh_release", "sh_to"), sheathed_rest, 0.0)],
    }
    hits = tl["hit_frames"]
    hit_times = {"SlashRightHitTime": hit(hits["r"], "r_wind"), "SlashLeftHitTime": hit(hits["l"], "l_wind"),
                 "FinisherHitTime": hit(hits["f"], "f_charge"),
                 "ThrustHitTime": rel("t_strike", "t_wind")}

    def key(t, m, alpha, note=""):
        loc, rot = to_unreal(m)
        return ("MakeCutlassKey({:.3f}f, FVector({:.2f}, {:.2f}, {:.2f}), FRotator({:.2f}, {:.2f}, {:.2f}), {:.2f}f)"
                .format(t, *loc, *rot, alpha))

    lines = ["// monke_island - generated by scripts/props/cutlass_poses.py from blade-report.json and timeline.json, "
             "do not edit", ""]
    lines.append("GuardKey = {};".format(key(0.0, guard, 0.0)))
    lines.append("SheathedKey = {};".format(key(0.0, sheathed_rest, 0.0)))
    for name, m in (("ScabbardRest", scab_rest), ("ScabbardDrawn", scab_end)):
        loc, rot = to_unreal(m)
        lines.append("{} = FTransform(FRotator({:.2f}, {:.2f}, {:.2f}), FVector({:.2f}, {:.2f}, {:.2f}));"
                     .format(name, *rot, *loc))
    for name, value in hit_times.items():
        lines.append("{} = {:.3f}f;".format(name, value))
    for name, keys in tracks.items():
        lines.append("{} = {{".format(name))
        for t, m, alpha in keys:
            lines.append("\t{},".format(key(t, m, alpha)))
        lines.append("};")
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
