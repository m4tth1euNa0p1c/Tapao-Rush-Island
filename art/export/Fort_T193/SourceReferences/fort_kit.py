"""
Fort francais (Fort Saint-Louis-des-Perles, fictif) : modules SM_Fort_* (rempart, porte, tour, batiments, poudriere,
chapelle, prison, habillage) a l'echelle Bean (1,05 m ; monde x0,58).

Toutes les cotes viennent de maquettes/fort-francais/maquette.json (params.modules) ; conventions : metres, Z haut,
front = -Y (sauf SM_Fort_Bastion), pivot decrit dans params.modules[cle].local_frame.

Contrat (voir maquettes/fort-francais/scripts/scene.py) :
  MODULES = {cle: (export_name | None, builder(coll, p) -> res)} ; res au format bean_kit
  ({"parts", "sockets", "ucx", "params", "keep_uv"}) + extensions :
    "children": [(suffixe, [objets], pivot_local)]  sous-objets animables (SM_<x>_<suffixe>, enfants du SM)
    "socket_rot": {socket: (rx, ry, rz) degres}
    "info": {...} consigne dans modules/<lot>.json
  assemble(cle, coll) fusionne (attributs UVMap / Blend / OrnMask conserves), cree UCX_<SM>_NN et SOCKET_<nom>,
  applique le budget de triangles (DECIMATE collapse si depassement) et retourne le res complete.

Les sous-modules sont dans fort_rampart / fort_gate / fort_tower / fort_build / fort_prison / fort_props ;
fort_port, fort_loot et fort_site exposent leurs propres MODULES (meme format).
"""
import json
import math
import os

import bpy
from mathutils import Matrix, Vector

import bean_kit as K

ROOT = K.ROOT
SPEC = os.path.join(ROOT, "maquettes", "fort-francais", "maquette.json")
_SPEC = {}
V = K.V


def params():
    if "p" not in _SPEC:
        with open(SPEC, encoding="utf-8") as fh:
            _SPEC["p"] = json.load(fh)["params"]
    return _SPEC["p"]


def mod(key):
    return dict(params()["modules"][key])


def sockets_of(d, skip=()):
    out = {}
    for k, v in d.items():
        if k in skip or not isinstance(v, (list, tuple)) or len(v) != 3:
            continue
        out[k] = ("root", Vector(v))
    return out


def res(parts, sockets, ucx, p, **extra):
    return {"parts": {"root": [o for o in parts if o is not None]}, "sockets": sockets, "ucx": ucx,
            "params": {k: v for k, v in (p or {}).items() if not str(k).startswith("_")}, "keep_uv": True, **extra}


def budget_for(name):
    tm = params()["budget"]["tris_max"]
    if name in tm:
        return tm[name]
    for k, v in tm.items():
        if k.endswith("*") and name.startswith(k[:-1]):
            return v
    return None


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def fit_budget(ob, max_tris):
    """DECIMATE (collapse) applique si le module depasse son budget ; (tris avant, tris apres)."""
    before = tri_count(ob)
    if not max_tris or before <= max_tris:
        return before, before
    for _ in range(4):
        cur = tri_count(ob)
        if cur <= max_tris:
            break
        md = ob.modifiers.new("Decimate", "DECIMATE")
        md.decimate_type = "COLLAPSE"
        md.ratio = max(0.03, max_tris * 0.95 / cur)
        md.use_collapse_triangulate = True
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
        old = ob.data
        ob.modifiers.clear()
        ob.data = me
        bpy.data.meshes.remove(old)
        me.name = ob.name
    return before, tri_count(ob)


def join(name, objs, coll):
    """Fusionne des objets maillages (attributs conserves) en un objet a l'origine."""
    objs = [o for o in objs if o is not None and o.type == "MESH"]
    if not objs:
        me = bpy.data.meshes.new(name)
        ob = bpy.data.objects.new(name, me)
        coll.objects.link(ob)
        return ob
    vl = bpy.context.view_layer
    vl.update()
    dg = bpy.context.evaluated_depsgraph_get()
    for o in objs:                                       # modificateurs appliques, transformation appliquee
        if o.modifiers:
            me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
            old = o.data
            o.modifiers.clear()
            o.data = me
            if old.users == 0:
                bpy.data.meshes.remove(old)
        if o.matrix_world != Matrix.Identity(4) or o.parent is not None:
            mw = o.matrix_world.copy()
            o.parent = None
            o.data.transform(mw)
            o.matrix_world = Matrix.Identity(4)
    main = objs[0]
    if len(objs) > 1:
        for o in objs:
            if o.name not in vl.objects:
                coll.objects.link(o)
        with bpy.context.temp_override(active_object=main, object=main, selected_objects=objs,
                                       selected_editable_objects=objs):
            bpy.ops.object.join()
    main.name = name
    main.data.name = name
    return main


def assemble_res(export_name, r, coll):
    """Fusion + UCX + sockets + budget pour un res deja construit."""
    ucx_objs = K.make_ucx(export_name, r.get("ucx", []), coll)
    parts = [o for objs in r["parts"].values() for o in objs]
    if r.get("keep_separate"):
        # objets de reference (terrain, eau) : pas de fusion, pas de budget ; enfants du premier objet
        main = parts[0]
        for o in parts[1:]:
            o.parent = main
        info = dict(r.get("info", {}))
        info.update(tris=sum(tri_count(o) for o in parts), tris_raw=None, budget=None,
                    materials=sorted({m.name for o in parts for m in o.data.materials if m}))
        r.update(object=main, ucx_objects=ucx_objs, socket_objects=[], export_name=export_name, info=info,
                 child_objects=parts[1:])
        return r
    mesh = join(export_name, parts, coll)
    mesh.data.update()
    raw, tris = fit_budget(mesh, budget_for(export_name))
    if hasattr(mesh.data, "set_sharp_from_angle") and not r.get("no_sharp"):
        mesh.data.set_sharp_from_angle(angle=math.radians(r.get("sharp", 40)))
    for u in ucx_objs:
        u.parent = mesh
        u.hide_viewport = False
    children = []
    for suffix, objs, pivot in r.get("children", []):
        cname = f"{export_name}_{suffix}"
        ch = join(cname, objs, coll)
        piv = Vector(pivot)
        ch.data.transform(Matrix.Translation(-piv))
        ch.parent = mesh
        ch.location = piv
        children.append(ch)
        tris += tri_count(ch)
    socks = K.make_sockets(mesh, r.get("sockets", {}), coll)
    rots = r.get("socket_rot", {})
    for s in socks:
        rr = rots.get(s.name[len("SOCKET_"):])
        if rr:
            s.rotation_euler = [math.radians(a) for a in rr]
        s.empty_display_size = 0.15
    info = dict(r.get("info", {}))
    mats = [m.name for m in mesh.data.materials]
    for ch in children:
        mats += [m.name for m in ch.data.materials if m.name not in mats]
    info.update(tris_raw=raw, tris=tris, materials=mats, budget=budget_for(export_name))
    r.update(object=mesh, ucx_objects=ucx_objs, socket_objects=socks, export_name=export_name, info=info,
             child_objects=children)
    return r


def assemble(key, coll, p=None, registry=None):
    """Construit le module `key` a l'origine ; renvoie une liste de res (un module peut exporter plusieurs SM)."""
    reg = registry or all_modules()
    export_name, builder = reg[key]
    out = builder(coll, p)
    lst = out if isinstance(out, list) else [out]
    done = []
    for r in lst:
        name = r.get("export_name") or export_name or ("REF-" + key)
        assemble_res(name, r, coll)
        r["key"] = key
        done.append(r)
    return done


# ----------------------------------------------------------------------------
# Registre

MODULES = {}


def _load():
    if MODULES:
        return MODULES
    import importlib
    for name in ("fort_rampart", "fort_gate", "fort_tower", "fort_build", "fort_prison", "fort_props"):
        try:
            MODULES.update(importlib.import_module(name).MODULES)
        except ImportError as e:
            if name not in str(e):
                raise
            print("[fort_kit] sous-module absent :", name)
    return MODULES


def all_modules():
    import importlib
    reg = dict(_load())
    for name in ("fort_port", "fort_loot", "fort_site"):
        try:
            reg.update(importlib.import_module(name).MODULES)
        except ImportError as e:
            if name not in str(e):
                raise
            print("[fort_kit] kit absent :", name)
    return reg


try:                     # MODULES rempli a l'import (contrat de scene.py) ; les sous-modules importent fort_kit
    _load()
except ImportError as _e:    # pragma: no cover - sous-module manquant pendant le developpement
    print("[fort_kit] registre incomplet :", _e)
