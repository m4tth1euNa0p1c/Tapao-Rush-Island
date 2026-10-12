"""Waypoints of the villager of a minka (owner request 2026-10-07: one villager per village house, with a routine in
its house, who follows a player coming in), for UVillageHouseKit::Nodes of DA_MinkaKit (scripts/props/house_nav_ue.py).

Plain Python (no Blender): the plan of scripts/props/minka/build.py (Blender frame, metres, facade towards -Y, origin
at the front-left corner of the main body): doma A x 0-1.8 at z 0 (entrance door in the facade bay x 0-1.8, kamado
north, firewood west), raised floor at z 0.45: B board room with the irori (x 1.8-5.4, y 0-3.6), C front tatami room
(x 5.4-9, y 0-3.6, low table), D rear board room (x 1.8-5.4, y 3.6-7.2, tansu, futon), E rear tatami room
(x 5.4-9, y 3.6-6.3, andon) and the tokonoma (x 5.4-7.2, y 6.3-7.2) where the Tidal Relic stands. The step stone
joins the doma and B (bay x 1.8, y 1.8-3.6); fusuma join B-C (x 5.4, y 1.8-3.6), B-D (y 3.6, x 3.6-5.4),
C-E (y 3.6, x 5.4-7.2) and D-E (x 5.4, y 3.6-5.4). Every link is a straight line through a room or a doorway bay,
clear of the furniture, at 30 cm or more from the object slots of minka_doors.py where it can.
The Unreal house frame is x, -y, z in cm (minka_doors.ue); a yaw turns the same way (-Blender yaw).
"""
import math

F = 0.45   # raised floor (build.P["floor"])

# name, (x, y, z) Blender metres, activity, facing (Blender degrees, 0 = +X, 90 = +Y towards the back)
NODES = [
    ("doma_door", (0.95, 0.70, 0.0), "", 0.0),
    ("doma_mid", (0.85, 2.70, 0.0), "", 0.0),
    ("doma_kitchen", (1.05, 5.45, 0.0), "work", 90.0),     # at the kamado
    ("step_b", (2.25, 2.70, F), "", 0.0),
    ("b_west", (2.45, 1.70, F), "rest", 0.0),             # by the hearth, facing it
    ("b_south", (3.60, 0.80, F), "work", 90.0),            # tending the hearth
    ("b_southeast", (4.70, 1.10, F), "", 0.0),
    ("b_northeast", (4.60, 2.85, F), "", 0.0),
    ("c_west", (6.00, 2.70, F), "", 0.0),
    ("c_table", (7.20, 3.00, F), "rest", -90.0),            # kneeling at the low table
    ("c_east", (8.30, 2.20, F), "watch", 0.0),             # looking out to the side engawa
    ("d_south", (4.50, 4.20, F), "", 0.0),
    ("d_middle", (3.30, 5.20, F), "work", 90.0),           # at the tansu
    ("d_futon", (4.15, 5.95, F), "rest", 90.0),            # by the futon
    ("e_west", (6.10, 4.30, F), "", 0.0),
    ("e_tokonoma", (6.30, 6.00, F), "pray", 90.0),         # before the tokonoma and its relic
    ("e_east", (7.90, 4.70, F), "rest", 0.0),              # on the tatami, facing the garden
]
LINKS = [
    ("doma_door", "doma_mid"), ("doma_mid", "doma_kitchen"), ("doma_mid", "step_b"),
    ("step_b", "b_west"), ("step_b", "b_northeast"), ("b_west", "b_south"), ("b_south", "b_southeast"),
    ("b_southeast", "b_northeast"), ("b_northeast", "c_west"), ("b_northeast", "d_south"),
    ("c_west", "c_table"), ("c_table", "c_east"), ("c_west", "e_west"),
    ("d_south", "d_middle"), ("d_middle", "d_futon"), ("d_south", "e_west"),
    ("e_west", "e_tokonoma"), ("e_west", "e_east"),
]
DOOR_NODE = "doma_door"
RELIC_NODE = "e_tokonoma"
OUTSIDE = (0.95, -1.00, 0.0)                 # in front of the doma door, past the outer step stone
INTERIOR = ((0.0, 0.0, -0.5), (9.0, 7.2, 4.0))   # the main body, walls included (the engawa is outside)


def ue(p):
    """Blender metres (house frame) to Unreal centimetres."""
    return [round(p[0] * 100.0, 1), round(-p[1] * 100.0, 1), round(p[2] * 100.0, 1)]


def ue_yaw(blender_degrees):
    return round(-blender_degrees, 1)


def kit_data():
    """Nodes, door and relic nodes, outside point and inside box in the Unreal house frame."""
    index = {name: i for i, (name, _, _, _) in enumerate(NODES)}
    links = {i: [] for i in range(len(NODES))}
    for a, b in LINKS:
        links[index[a]].append(index[b])
        links[index[b]].append(index[a])
    nodes = []
    for i, (name, p, activity, yaw) in enumerate(NODES):
        nodes.append({"name": name, "location": ue(p), "room": name.split("_")[0], "links": sorted(links[i]),
                      "activity": activity, "yaw": ue_yaw(yaw)})
    lo, hi = INTERIOR
    a, b = ue(lo), ue(hi)
    box = ([min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2])], [max(a[0], b[0]), max(a[1], b[1]), max(a[2], b[2])])
    return {"nodes": nodes, "door_node": index[DOOR_NODE], "relic_node": index[RELIC_NODE], "outside": ue(OUTSIDE),
            "interior": box}


def check():
    """Every node reachable from the door, links no longer than a room."""
    data = kit_data()
    seen, todo = {data["door_node"]}, [data["door_node"]]
    while todo:
        for n in data["nodes"][todo.pop()]["links"]:
            if n not in seen:
                seen.add(n)
                todo.append(n)
    assert len(seen) == len(NODES), "a waypoint cannot be reached from the door"
    for a, b in LINKS:
        pa = next(p for n, p, _, _ in NODES if n == a)
        pb = next(p for n, p, _, _ in NODES if n == b)
        assert math.dist(pa[:2], pb[:2]) < 3.2, (a, b)
    return data


if __name__ == "__main__":
    import json
    print(json.dumps(check(), indent=1))
