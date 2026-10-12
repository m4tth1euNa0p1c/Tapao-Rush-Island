"""
Fort francais : assemblage de la scene (lot 'scene', dans le processus du builder).

Lie (link=True) les collections des modules depuis modules/<lot>.blend, fusionne les images / materiaux identiques des
differentes bibliotheques (une seule copie en memoire), instancie chaque module (collection instances) selon
params.scene_layout et les sockets consignes dans modules/<lot>.json ; ajoute le butin de la salle des coffres
(params.scene_layout.treasure_fill), les lanternes et leurs lumieres, la vegetation de l'atoll (instances), Bean x2,
le galion (GLB, images reduites a 1K, sans greement si la RAM depasse 5 Go) et la chaloupe.
"""
import ctypes
import json
import math
import os
import random

import bpy
from mathutils import Matrix, Vector

import fort_kit as FK

V = Vector
TAU = 2 * math.pi


def _rss_gb():
    """Memoire du processus (Windows : GetProcessMemoryInfo) ; 0 si indisponible."""
    try:
        class PMC(ctypes.Structure):
            _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong),
                        ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                        ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]
        pmc = PMC()
        pmc.cb = ctypes.sizeof(PMC)
        k32 = ctypes.windll.kernel32
        k32.GetCurrentProcess.restype = ctypes.c_void_p
        fn = k32.K32GetProcessMemoryInfo
        fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(PMC), ctypes.c_ulong]
        fn(k32.GetCurrentProcess(), ctypes.byref(pmc), pmc.cb)
        return round(pmc.WorkingSetSize / 2 ** 30, 2)
    except Exception:
        return 0.0


def xf(pos, rot_z=0.0, rot=None):
    M = Matrix.Translation(Vector(pos))
    if rot is not None:
        return M @ Matrix.Rotation(math.radians(rot[2]), 4, "Z") @ Matrix.Rotation(math.radians(rot[1]), 4, "Y") @ \
            Matrix.Rotation(math.radians(rot[0]), 4, "X")
    return M @ Matrix.Rotation(math.radians(rot_z), 4, "Z")


class Scene:
    def __init__(self, ctx, reports):
        self.ctx = ctx
        self.scene = ctx["scene"]
        self.P = ctx["spec"]["params"]
        self.lay = self.P["scene_layout"]
        self.reports = reports
        self.mods = {}            # export -> rapport de module (sockets...)
        for lot, rep in reports.items():
            for name, m in rep.get("modules", {}).items():
                m = dict(m)
                m["lot"] = lot
                self.mods[name] = m
        self.colls = {}
        self.count = {}
        self.lights = []
        self.warnings = []
        self.root = {(k[4:] if k.startswith("COL-") else k): v for k, v in ctx["collections"].items()}

    # ------------------------------------------------------------------ bibliotheques
    def link_all(self):
        mdir = os.path.join(self.ctx["dir"], "modules")
        for lot, rep in self.reports.items():
            path = os.path.join(mdir, lot + ".blend")
            if not os.path.isfile(path):
                self.warnings.append("bibliotheque absente : " + path)
                continue
            names = list(rep.get("modules", {}))
            with bpy.data.libraries.load(path, link=True, relative=True) as (src, dst):
                dst.collections = [n for n in names if n in src.collections]
            for c in dst.collections:
                if c is not None:
                    self.colls[c.name] = c
        self.dedupe()

    def dedupe(self):
        """Une seule copie des images et materiaux identiques venant de plusieurs bibliotheques."""
        seen, n_img = {}, 0
        for img in list(bpy.data.images):
            if not img.filepath or img.packed_file is not None:
                continue
            key = os.path.normcase(os.path.abspath(bpy.path.abspath(img.filepath, library=img.library)))
            if key in seen and seen[key] is not img:
                try:
                    img.user_remap(seen[key])
                    n_img += 1
                except Exception as e:  # noqa: BLE001
                    self.warnings.append(f"remap image {img.name}: {e}")
            else:
                seen[key] = img
        seen_m, n_mat = {}, 0
        for m in list(bpy.data.materials):
            if m.library is None:
                continue
            key = m.name
            if key in seen_m and seen_m[key] is not m:
                try:
                    m.user_remap(seen_m[key])
                    n_mat += 1
                except Exception as e:  # noqa: BLE001
                    self.warnings.append(f"remap materiau {m.name}: {e}")
            else:
                seen_m[key] = m
        self.dedup = {"images": n_img, "materials": n_mat}

    # ------------------------------------------------------------------ instances
    def coll_for(self, key_or_export):
        exp = key_or_export
        if exp not in self.colls:
            m = self.P["modules"].get(key_or_export)
            if m and m.get("export"):
                exp = m["export"]
        return self.colls.get(exp), exp

    def inst(self, key, M, parent="Props", name=None):
        coll, exp = self.coll_for(key)
        if coll is None:
            self.warnings.append("module absent : " + str(key))
            return None
        n = self.count.get(exp, 0)
        self.count[exp] = n + 1
        e = bpy.data.objects.new(name or f"GEO-{exp[3:] if exp.startswith('SM_') else exp}_{n:02d}", None)
        e.instance_type = "COLLECTION"
        e.instance_collection = coll
        e.empty_display_size = 0.2
        e.matrix_world = M
        tgt = self.root.get(parent) or self.scene.collection
        tgt.objects.link(e)
        return e

    def sock(self, export, name, M=Matrix.Identity(4)):
        """Matrice monde d'un socket d'un module place en M."""
        m = self.mods.get(export)
        if not m or name not in m.get("sockets", {}):
            self.warnings.append(f"socket absent : {export}.{name}")
            return None
        s = m["sockets"][name]
        return M @ xf(s["pos"], rot=s.get("rot", (0, 0, 0)))

    def export_of(self, key):
        return self.P["modules"][key]["export"]

    def light(self, name, M_or_pos, energy, color, radius=0.06, kind="POINT", size=0.4, parent="Props"):
        d = bpy.data.lights.new(name, kind)
        d.energy = energy
        d.color = color[:3]
        if kind == "POINT":
            d.shadow_soft_size = radius
        elif kind == "AREA":
            d.size = size
        ob = bpy.data.objects.new(name, d)
        (self.root.get(parent) or self.scene.collection).objects.link(ob)
        if isinstance(M_or_pos, Matrix):
            ob.matrix_world = M_or_pos
        else:
            ob.location = M_or_pos
        self.lights.append(name)
        return ob

    def lantern(self, M_wall, light=True, hanging=False, parent="Props"):
        key = "lantern_hanging" if hanging else "lantern"
        e = self.inst(key, M_wall, parent)
        if e is None or not light:
            return e
        p = self.P["modules"][key]
        ls = self.mods.get(self.export_of(key), {}).get("sockets", {}).get("Light")
        pos = M_wall @ Vector(ls["pos"]) if ls else M_wall.translation
        self.light(f"LGT-lantern_{len(self.lights):02d}", pos, p["light"]["energy_W"], p["light"]["color"], 0.05,
                   parent=parent)
        return e


# ----------------------------------------------------------------------------
# Construction

def build_scene(ctx, reports):
    S = Scene(ctx, reports)
    rep = {"rss_gb": {"start": _rss_gb()}}
    S.link_all()
    rep["rss_gb"]["linked"] = _rss_gb()
    lay, P = S.lay, S.P
    rng = random.Random(lay["vegetation"].get("seed", 1717))

    # --- site : terrain, eau, fosse
    S.inst("REF-site_terrain", Matrix.Identity(4), "Site", "REF-Site")
    S.inst("ditch", Matrix.Identity(4), "Site")

    # --- remparts
    for c in lay["curtains"]:
        M = xf(c["pos"], c["rot_z"])
        S.inst(c["m"], M, "Fort_Rampart")
    bast = {}
    for b in lay["bastions"]:
        key = b.get("m", "bastion")
        M = xf(b["pos"], b["rot_z"])
        S.inst(key, M, "Fort_Rampart", f"GEO-Fort_Bastion_{b['name']}")
        bast[b["name"]] = (key, M)
        Me = S.sock(S.export_of(key), "Echauguette", M)
        if Me is not None:
            S.inst("echauguette", Me, "Fort_Rampart")
            le = S.sock(S.export_of("echauguette"), "Lantern", Me)
    for r in lay["ramps"]:
        S.inst("ramp", xf(r["pos"], r["rot_z"]), "Fort_Rampart")
    for s in lay["stairs"]:
        S.inst("stair", xf(s["pos"], s["rot_z"]), "Fort_Rampart")

    # --- porte
    g = lay["gatehouse"]
    Mg = xf(g["pos"], g["rot_z"])
    exp_g = S.export_of("gatehouse")
    S.inst("gatehouse", Mg, "Fort_Gate")
    for side, ang in (("L", 80.0), ("R", -80.0)):
        Ms = S.sock(exp_g, "Door_" + side, Mg)
        if Ms is not None:
            S.inst("gate_door_" + side.lower(), Ms @ Matrix.Rotation(math.radians(ang), 4, "Z"), "Fort_Gate")
    Mp = S.sock(exp_g, "Portcullis", Mg)
    if Mp is not None:
        S.inst("portcullis", Matrix.Translation((0, 0, 2.5)) @ Mp, "Fort_Gate")
    S.inst("drawbridge", xf(lay["drawbridge"]["pos"], lay["drawbridge"]["rot_z"]), "Fort_Gate")
    S.inst("bridge_fixed", xf(lay["bridge_fixed"]["pos"], lay["bridge_fixed"]["rot_z"]), "Fort_Gate")
    for side in ("L", "R"):
        s_ = S.mods.get(exp_g, {}).get("sockets", {}).get("Lantern_" + side)
        if s_:
            x, _y, z = s_["pos"]
            S.lantern(Mg @ xf((x, -0.53, z + 0.18), 180.0), parent="Fort_Gate")
    gh = lay["guardhouse"]
    Mgh = xf(gh["pos"], gh["rot_z"])
    S.inst("guardhouse", Mgh, "Fort_Gate")
    Ml = S.sock(S.export_of("guardhouse"), "Lantern", Mgh)
    if Ml is not None:
        S.lantern(Matrix.Translation((0, 0, 2.805 - Ml.translation.z)) @ Ml, hanging=True, parent="Fort_Gate")

    # --- tour, pavillon, fanal
    t = lay["signal_tower"]
    Mt = xf(t["pos"], t["rot_z"])
    exp_t = S.export_of("signal_tower")
    S.inst("signal_tower", Mt, "Fort_Tower")
    Mf = S.sock(exp_t, "FlagTop", Mt)
    if Mf is not None:
        S.inst("flag", Matrix.Translation(Mf.translation) @ Matrix.Rotation(math.radians(190), 4, "Z"), "Fort_Tower")
    Mfire = S.sock(exp_t, "FanalFire", Mt)
    if Mfire is not None:
        S.light("LGT-fanal", Mfire.translation + V((0, 0, 0.25)), 60.0, (1.0, 0.5, 0.18), 0.15, parent="Fort_Tower")
    Md = S.sock(exp_t, "Door", Mt)
    if Md is not None:
        S.lantern(Md @ Matrix.Translation((0.85, 0.1, 2.1)) @ Matrix.Rotation(math.pi, 4, "Z"), parent="Fort_Tower")

    # --- batiments
    for key, coll in (("governor_house", "Fort_Buildings"), ("barracks", "Fort_Buildings"),
                      ("kitchen", "Fort_Buildings"), ("cistern", "Fort_Buildings"),
                      ("powder_magazine", "Fort_Buildings"), ("chapel", "Fort_Buildings")):
        L = lay[key]
        M = xf(L["pos"], L["rot_z"])
        S.inst(key, M, coll)
        if key == "governor_house":
            for side in ("L", "R"):
                Ml = S.sock(S.export_of(key), "Lantern_" + side, M)
                if Ml is not None:
                    S.lantern(Ml @ Matrix.Translation((0, 0.1, 0.18)) @ Matrix.Rotation(math.pi, 4, "Z"),
                              parent=coll)
        if key == "powder_magazine":
            for y in (-0.55, 0.55):
                for k in range(6):
                    S.inst("powder_barrel", M @ xf((-1.5 + 0.6 * k, y, 0.23), rng.uniform(0, 360)), coll)
    # --- prison, cages, potence
    pr = lay["prison"]
    Mpr = xf(pr["pos"], pr["rot_z"])
    exp_pr = S.export_of("prison")
    S.inst("prison", Mpr, "Fort_Prison")
    for k in (1, 2, 3):
        Mc = S.sock(exp_pr, f"CellDoorHinge_{k}", Mpr)
        if Mc is not None:
            S.inst("cell_door", Mc @ Matrix.Rotation(math.radians(-70 if k == 2 else 0), 4, "Z"), "Fort_Prison")
        Msh = S.sock(exp_pr, f"Shackles_{k}", Mpr)
        if Msh is not None:
            S.inst("wall_shackles", Msh, "Fort_Prison")
    pp = P["modules"]["prison"]
    hxp = pp["footprint_m"][0] / 2 - pp["walls_m"]
    for k in (1, 2, 3):
        yc = pp["sockets"][f"Cell_{k}"][1]
        S.light(f"LGT-prison_soupirail_{k}", Mpr @ Matrix.Translation((hxp - 0.15, yc, 1.75)) @
                Matrix.Rotation(math.radians(90), 4, "Y"), 30.0, (0.75, 0.85, 1.0), kind="AREA", size=0.35,
                parent="Fort_Prison")
    Mdh = S.sock(exp_pr, "DoorHinge", Mpr)
    if Mdh is not None:
        S.inst("studded_door", Mdh @ Matrix.Rotation(math.radians(75), 4, "Z"), "Fort_Prison")
    Ml = S.sock(exp_pr, "Lantern", Mpr)
    if Ml is not None:
        S.lantern(Matrix.Translation((0, 0, 2.31 - Ml.translation.z)) @ Ml, hanging=True, parent="Fort_Prison")
    for c in lay["cages"]:
        M = xf(c["pos"], c["rot_z"])
        S.inst(c["m"], M, "Fort_Prison")
        if c["m"] == "gibbet":
            Mr = S.sock(S.export_of("gibbet"), "CageRing", M)
            if Mr is not None:
                S.inst("cage_hanging", Matrix.Translation(Mr.translation) @ Matrix.Rotation(0.3, 4, "Z"), "Fort_Prison")

    # --- salle des coffres et butin
    tv = lay["treasure_vault"]
    Mv = xf(tv["pos"], tv["rot_z"])
    exp_v = S.export_of("treasure_vault")
    S.inst("treasure_vault", Mv, "Fort_Treasure")
    for sk, key, ang in (("VaultDoorHinge", "vault_door", 100.0), ("GrilleCenter", "vault_grille", 0.0),
                         ("CourtDoorHinge", "studded_door", -95.0)):
        Ms = S.sock(exp_v, sk, Mv)
        if Ms is not None:
            S.inst(key, Ms @ Matrix.Rotation(math.radians(ang), 4, "Z"), "Fort_Treasure")
    for k in (1, 2, 3, 4):
        Ms = S.sock(exp_v, f"Pallet_{k}", Mv)
        if Ms is not None:
            S.inst("pallet", Ms, "Fort_Treasure")
    fill = lay["treasure_fill"]
    for sk, what in fill.items():
        Ms = S.sock(exp_v, sk, Mv)
        if Ms is None:
            continue
        w = what.split()[0]
        if w == "gold_bar":
            for i in range(3):
                S.inst("gold_bar", Ms @ xf((rng.uniform(-0.25, 0.25), rng.uniform(-0.15, 0.15), 0.032 * (i == 2)),
                                           rng.uniform(0, 360)), "Fort_Treasure")
        elif w == "gems":
            for i, gname in enumerate(("Ruby", "Sapphire", "Emerald", "Amethyst", "Topaz")):
                c, _ = S.coll_for("SM_Loot_Gem_" + gname)
                if c is not None:
                    S.inst("SM_Loot_Gem_" + gname, Ms @ xf((0.06 * math.cos(i * 1.3), 0.05 * math.sin(i * 1.7), 0),
                                                         rng.uniform(0, 360)), "Fort_Treasure")
        else:
            rz = rng.uniform(-8, 8) + (90 if sk == "Loot_Chest_5" else 0)
            S.inst(w, Ms @ Matrix.Rotation(math.radians(rz), 4, "Z"), "Fort_Treasure")
    vlights = P["modules"]["treasure_vault"].get("lights_scene", [])
    for i, L in enumerate(vlights):
        pos = Mv @ Vector(L["pos_local"])
        S.light(f"LGT-vault_{i}", pos, L["energy_W"], L["color"], 0.06, kind=L["type"], size=L.get("size", 0.4),
                parent="Fort_Treasure")
    for sk, hang in (("Lantern_1", True), ("Lantern_2", False), ("Lantern_3", False)):
        Ms = S.sock(exp_v, sk, Mv)
        if Ms is None:
            continue
        if hang:
            S.lantern(Matrix.Translation((0, 0, Mv.translation.z + FK.mod("treasure_vault")["chamber"]["crown_z"]
                                          - Ms.translation.z)) @ Matrix.Translation(Ms.translation), light=False,
                      hanging=True, parent="Fort_Treasure")
        else:
            x = Ms.translation.x
            wall_x = Mv.translation.x + (-2.5 if x < Mv.translation.x else 2.5)
            rz = -90 if x < Mv.translation.x else 90
            S.lantern(Matrix.Translation((wall_x, Ms.translation.y, Ms.translation.z + 0.18)) @
                      Matrix.Rotation(math.radians(rz), 4, "Z"), light=False, parent="Fort_Treasure")

    # --- port
    port = lay["port"]
    for key, lst in (("quay_6m", port["quay"]), ("quay_stairs", port["quay_stairs"]), ("jetty_6m", port["jetty"]),
                     ("pier_6m", port["pier"])):
        for q in lst:
            M = xf(q["pos"], q["rot_z"])
            S.inst(key, M, "Port")
            for sname in [n for n in S.mods.get(S.export_of(key), {}).get("sockets", {}) if n.startswith("Bollard")]:
                Mb = S.sock(S.export_of(key), sname, M)
                if Mb is not None:
                    S.inst("bollard", Mb @ Matrix.Rotation(rng.uniform(0, 6.28), 4, "Z"), "Port")
    ph = port["pier_head"]
    Mph = xf(ph["pos"], ph["rot_z"])
    S.inst("pier_head", Mph, "Port")
    for sname in ("Bollard_E", "Bollard_W"):
        Mb = S.sock(S.export_of("pier_head"), sname, Mph)
        if Mb is not None:
            S.inst("bollard", Mb, "Port")
    Ml = S.sock(S.export_of("pier_head"), "LanternMount", Mph)
    if Ml is not None:
        S.lantern(Ml, parent="Port")
    cr = port["crane"]
    S.inst("crane", xf(cr["pos"], cr["rot_z"]), "Port")
    wh = port["warehouse"]
    S.inst("warehouse", xf(wh["pos"], wh["rot_z"]), "Port")

    # --- canons, boulets, tonneaux, caisses
    n_can = 0
    for name, (key, M) in bast.items():
        exp = S.export_of(key)
        for sname in [n for n in S.mods.get(exp, {}).get("sockets", {}) if n.startswith("Gun_")]:
            Mc = S.sock(exp, sname, M)
            if Mc is not None:
                S.inst("cannon", Mc, "Fort_Rampart")
                n_can += 1
        for sname in ("ShotPile_1", "ShotPile_2"):
            Ms = S.sock(exp, sname, M)
            if Ms is not None:
                S.inst("shot_pile", Ms, "Fort_Rampart")
        if name == "SE":
            for k in range(2):
                S.inst("powder_barrel", M @ xf((-2.6 + 0.4 * k, 1.8, 2.97), rng.uniform(0, 360)), "Fort_Rampart")
    exp_e = S.export_of("curtain_4m_embrasure")
    for c in lay["curtains"]:
        if c["m"] != "curtain_4m_embrasure":
            continue
        M = xf(c["pos"], c["rot_z"])
        if c["rot_z"] == 180:            # courtine nord : plates-formes vides (affuts absents)
            continue
        Mc = S.sock(exp_e, "Gun", M)
        if Mc is not None:
            S.inst("cannon", Mc, "Fort_Rampart")
            n_can += 1
        Ms = S.sock(exp_e, "ShotPile", M)
        if Ms is not None:
            S.inst("shot_pile", Ms, "Fort_Rampart")
    props = lay["props"]
    for pos in props["barrels"]:
        S.inst("barrel", xf(pos, rng.uniform(0, 360)), "Props")
    for pos in props["crates"]:
        S.inst("crate_a" if rng.random() < 0.6 else "crate_b", xf(pos, rng.uniform(-15, 15)), "Props")
    # lanternes des courtines (murs interieurs)
    exp_c = S.export_of("curtain_4m")
    for c in lay["curtains"]:
        if c["m"] == "curtain_4m" and (tuple(c["pos"][:2]) in ((-8, -20.2), (8, -20.2), (-12, 20.2), (12, 20.2),
                                                                (24.2, -4), (-24.2, 4))):
            M = xf(c["pos"], c["rot_z"])
            Ml = S.sock(exp_c, "Lantern", M)
            if Ml is not None:
                S.lantern(Ml @ Matrix.Translation((0, -0.05, 0)), parent="Fort_Rampart")
    rep["rss_gb"]["modules"] = _rss_gb()

    # --- vegetation de l'atoll
    try:
        rep["vegetation"] = vegetation(S, rng)
    except Exception as e:  # noqa: BLE001
        S.warnings.append("vegetation : " + repr(e))
    rep["rss_gb"]["vegetation"] = _rss_gb()

    # --- Bean x2
    try:
        import bean_fit as FIT
        root, arm = FIT.append_bean(S.scene)
        S.scene.collection.children.unlink(root)
        for b in lay["bean"]:
            e = bpy.data.objects.new(b["name"], None)
            e.instance_type = "COLLECTION"
            e.instance_collection = root
            e.matrix_world = xf(b["pos"], b["rot_z_deg"])
            (S.root.get("REF") or S.scene.collection).objects.link(e)
        rep["bean"] = [b["name"] for b in lay["bean"]]
    except Exception as e:  # noqa: BLE001
        S.warnings.append("bean : " + repr(e))

    # --- galion et chaloupe (GLB, images reduites a 1K)
    rep["rss_gb"]["before_ships"] = _rss_gb()
    rep["ships"] = ships(S, rep)
    rep["rss_gb"]["end"] = _rss_gb()

    # --- rendu EEVEE
    ee = getattr(S.scene, "eevee", None)
    for attr, val in (("use_raytracing", True), ("use_shadows", True), ("use_fast_gi", True), ("fast_gi_distance", 3.0),
                      ("shadow_ray_count", 2), ("shadow_step_count", 8)):
        try:
            setattr(ee, attr, val)
        except (AttributeError, TypeError):
            pass
    try:
        S.scene.view_settings.view_transform = "AgX"
        looks = [i.identifier for i in S.scene.view_settings.bl_rna.properties["look"].enum_items]
        if "AgX - Medium High Contrast" in looks:
            S.scene.view_settings.look = "AgX - Medium High Contrast"
    except Exception:  # noqa: BLE001
        pass
    rep.update(instances=dict(sorted(S.count.items())), instances_total=sum(S.count.values()), cannons=n_can,
               lights=len(S.lights), dedupe=S.dedup, warnings=S.warnings)
    return rep


# ----------------------------------------------------------------------------
# Vegetation

GRASS = ["SM_Grass_Tuft", "SM_Grass_Short", "SM_Grass_Dry"]
SHRUBS = ["SM_Naupaka_A", "SM_Naupaka_B", "SM_Naupaka_C", "SM_Pandanus_Young", "SM_Pandanus_A", "SM_Pandanus_B",
          "SM_MikiMiki_A", "SM_MikiMiki_B", "SM_Tahinu_A", "SM_Tahinu_B", "SM_Ipomoea_A", "SM_Ipomoea_B",
          "SM_Fern_A", "SM_Fern_B"]
PALMS = ["SM_CoconutPalm_Leaning_A", "SM_CoconutPalm_Leaning_B", "SM_CoconutPalm_Straight_A",
         "SM_CoconutPalm_Straight_B", "SM_CoconutPalm_Sabre_A", "SM_CoconutPalm_SCurve_A", "SM_CoconutPalm_Young_A",
         "SM_CoconutPalm_Young_B", "SM_CoconutPalm_Juvenile_A"]
PROPS = ["SM_CoconutFrond_Dry_A", "SM_CoconutFrond_Dry_B", "SM_Coconut_Brown", "SM_Coconut_Green",
         "SM_CoralRubble_A", "SM_CoralRubble_B", "SM_CoralRubble_D", "SM_CoralRubble_F", "SM_CoralBlock_A",
         "SM_CoralBlock_B", "SM_CoralBlock_C", "SM_Driftwood_A", "SM_Driftwood_B", "SM_LambisShell"]


def vegetation(S, rng):
    import marae_scene as MS
    import fort_site as FS
    lib = MS.append_atoll(GRASS + SHRUBS + PALMS + PROPS)
    for o in lib.values():
        for c in list(o.users_collection):
            c.objects.unlink(o)
        o.parent = None
        o.matrix_world = Matrix.Identity(4)
    # bibliotheque hors scene : on instancie la collection de chaque objet (pas de copie de maillage)
    per = {}
    for name, o in lib.items():
        c = bpy.data.collections.new("LIB-" + name)
        c.objects.link(o)
        per[name] = c
    site = FS.Site()
    tgt = S.root.get("Vegetation") or S.scene.collection
    n = [0]

    def put(name, x, y, rot=None, scale=1.0, z=None):
        if name not in per:
            return None
        zz = site.height(x, y) if z is None else z
        e = bpy.data.objects.new(f"GEO-veg_{name[3:]}_{n[0]:03d}", None)
        e.instance_type = "COLLECTION"
        e.instance_collection = per[name]
        e.matrix_world = Matrix.Translation((x, y, zz - 0.02)) @ Matrix.Rotation(
            rng.uniform(0, TAU) if rot is None else rot, 4, "Z") @ Matrix.Diagonal((scale, scale, scale, 1))
        tgt.objects.link(e)
        n[0] += 1
        return e
    lean = {}
    for name in PALMS:
        o = lib.get(name)
        if o is None or o.type != "MESH":
            continue
        me = o.data
        zmax = max(v.co.z for v in me.vertices)
        top = [v.co for v in me.vertices if v.co.z > zmax * 0.8]
        c = sum(top, Vector()) / max(1, len(top))
        lean[name] = math.atan2(c.y, c.x) if c.xy.length > 0.3 else None
    C = site.C

    def far_enough(x, y, d=15.0):
        return not G_inside(C, x, y) and FS.poly_dist(C, (x, y)) > d

    def G_inside(poly, x, y):
        import fort_geo as G
        return G.inside(poly, (x, y))

    def palm(x, y, want=math.radians(200)):
        name = rng.choice(PALMS)
        L = lean.get(name)
        rot = (want - L) if L is not None else rng.uniform(0, TAU)
        return put(name, x, y, rot + rng.uniform(-0.5, 0.5), rng.uniform(0.85, 1.1))
    # cocoteraie ouest (x < -50) et bande nord (y 44..56), motu voisin
    for _ in range(70):
        x, y = rng.uniform(-170, -52), rng.uniform(-34, 52)
        if site.height(x, y) > -0.2:
            palm(x, y)
    for _ in range(22):
        x, y = rng.uniform(-50, 46), rng.uniform(46, 55)
        if far_enough(x, y) and site.height(x, y) > 0.1:
            palm(x, y)
    for _ in range(7):
        x, y = rng.uniform(106, 128), rng.uniform(-40, 60)
        if site.height(x, y) > 0.0:
            palm(x, y)
    # lisiere oceane : pandanus, naupaka, miki-miki ; sous-bois de la cocoteraie
    for _ in range(70):
        x, y = rng.uniform(-170, 46), rng.uniform(47, 58)
        if far_enough(x, y, 12) and site.height(x, y) > 0.05:
            put(rng.choice(SHRUBS[:11]), x, y, scale=rng.uniform(0.8, 1.2))
    for _ in range(60):
        x, y = rng.uniform(-170, -50), rng.uniform(-36, 50)
        if site.height(x, y) > -0.1:
            put(rng.choice(SHRUBS), x, y, scale=rng.uniform(0.7, 1.1))
    # glacis : herbes rases et ipomees seulement
    for _ in range(120):
        x, y = rng.uniform(-46, 46), rng.uniform(-38, 46)
        if not G_inside(C, x, y) and FS.poly_dist(C, (x, y)) > 2.5 and site.height(x, y) > -0.2:
            if -42 < y < -34 and abs(x) < 28:
                continue
            put(rng.choice(GRASS + ["SM_Ipomoea_A", "SM_Ipomoea_B"]), x, y, scale=rng.uniform(0.6, 1.1))
    # cour : 2 jeunes cocotiers pres du logis, touffes au pied des murs
    for x, y in ((-8.6, 4.6), (8.4, 4.4)):
        put("SM_CoconutPalm_Young_A", x, y, scale=1.0, z=0.0)
    for _ in range(40):
        side = rng.randrange(4)
        if side == 0:
            x, y = rng.uniform(-19, 19), rng.uniform(15.3, 15.6)
        elif side == 1:
            x, y = rng.uniform(-19, 19), rng.uniform(-15.6, -15.3)
        elif side == 2:
            x, y = rng.uniform(19.3, 19.6), rng.uniform(-15, 15)
        else:
            x, y = rng.uniform(-19.6, -19.3), rng.uniform(-15, 15)
        if abs(x) < 1.6 and y < 0:
            continue
        put(rng.choice(GRASS), x, y, scale=rng.uniform(0.5, 0.9), z=0.0)
    # plages : laisse de mer, bois flotte, cocos ; platier : blocs de corail
    for _ in range(40):
        x = rng.uniform(-160, -30) if rng.random() < 0.6 else rng.uniform(30, 48)
        y = site.shore_y(x) + rng.uniform(-0.5, 3.0)
        put(rng.choice(PROPS[:4] + PROPS[11:]), x, y, scale=rng.uniform(0.8, 1.2))
    for _ in range(40):
        x, y = rng.uniform(-160, 50), rng.uniform(56, 62)
        put(rng.choice(PROPS[:8] + PROPS[11:13]), x, y, scale=rng.uniform(0.8, 1.3))
    for _ in range(35):
        x, y = rng.uniform(-150, 50), rng.uniform(64, 90)
        put(rng.choice(PROPS[4:11]), x, y, scale=rng.uniform(0.8, 1.6))
    return {"instances": n[0], "library": sorted(lib)}


# ----------------------------------------------------------------------------
# Navires

def _import_glb(path, coll, scale=1.0):
    import marae_kit as MK
    lc = MK._layer_coll(coll)
    if lc is not None:
        bpy.context.view_layer.active_layer_collection = lc
    before = set(bpy.data.objects)
    before_img = set(bpy.data.images)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    imgs = [i for i in bpy.data.images if i not in before_img]
    roots = [o for o in new if o.parent is None]
    return new, roots, imgs


def ships(S, rep):
    import temple_scene as TS
    out = {}
    tex = os.path.join(S.ctx["dir"], "textures", "ships_1k")
    seen = {}
    gl = S.lay["galleon"]
    root = os.path.abspath(os.path.join(S.ctx["dir"], "..", ".."))
    coll = S.root.get("REF") or S.scene.collection
    if gl.get("enabled", True):
        gdir = os.path.join(root, "maquettes", "galion-anglais", "exports")
        parts = ["SM_Galleon.glb", "SM_Galleon_Sails.glb", "SM_Galleon_Flag.glb", "SM_Galleon_Pennants.glb",
                 "SM_Galleon_Rudder.glb", "SM_Galleon_Wheel.glb"]
        if _rss_gb() < 5.0:
            parts.append("SM_Galleon_Rigging.glb")
        holder = bpy.data.objects.new("REF-Galleon", None)
        coll.objects.link(holder)
        holder.matrix_world = xf(gl["pos"], gl["rot_z"])
        done = []
        for f in parts:
            path = os.path.join(gdir, f)
            if not os.path.isfile(path):
                continue
            try:
                new, roots, imgs = _import_glb(path, coll)
                TS._shrink_images(imgs, 1024, tex, seen)
                for o in new:
                    if o.name.startswith(("UCX_", "SOCKET_")):
                        o.hide_render = o.hide_viewport = True
                for r in roots:
                    r.parent = holder
                    r.matrix_parent_inverse = Matrix.Identity(4)
                done.append(f)
            except Exception as e:  # noqa: BLE001
                S.warnings.append(f"galion {f} : {e!r}")
            if _rss_gb() > 5.5:
                S.warnings.append("RAM > 5,5 Go : import du galion interrompu")
                break
        out["galleon"] = done
    b = S.lay["port"]["boat"]
    path = os.path.join(root, b["src"])
    if os.path.isfile(path):
        try:
            new, roots, imgs = _import_glb(path, coll)
            TS._shrink_images(imgs, 1024, tex, seen)
            holder = bpy.data.objects.new(b["name"], None)
            coll.objects.link(holder)
            for r in roots:
                r.parent = holder
                r.matrix_parent_inverse = Matrix.Identity(4)
            holder.matrix_world = xf(b["pos"], b["rot_z"]) @ Matrix.Diagonal((b["scale"],) * 3 + (1,))
            out["boat"] = os.path.basename(path)
        except Exception as e:  # noqa: BLE001
            S.warnings.append("chaloupe : " + repr(e))
    return out
