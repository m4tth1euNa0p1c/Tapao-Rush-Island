"""Chaîne de production d'un Veilleur « Outlast » (T-274) : construction -> squelette tiki (identique à celui du jeu)
-> skinning au budget LOD0 -> UV de cuisson -> 7 modules de démembrement -> cuisson Cycles (Body 2048, Weapon 1024,
Gore 1024 ; BC / N / ORM / E) -> copies d'export, LOD1 / LOD2 -> FBX -> manifeste pour le réimport en place.

Sorties : art/export/Characters/<Mob>/Horror/ (SK_<Mob>.fbx, LOD/, SK_ et SM_ de l'arme, Dismember/, Textures/,
horror_manifest.json). Ordre des opérations et conventions FBX de la maquette mob-coco-realiste (2e PC), déjà
importée en place dans le jeu pour le Guerrier Coco le 2026-10-08.
"""
import datetime as dt
import json
import os
import time

import bpy
import numpy as np
from mathutils import Matrix

from . import horror_geo, horror_mats, real_gore, real_rig, real_ue
from .base import ART, ROOT, T, log

VERSION = "1.0"
SEVEN = ("Head", "Torso", "Pelvis", "Arm_L", "Arm_R", "Leg_L", "Leg_R")
MOBS = {
    "coco": dict(name="CocoWarrior", label="Guerrier Coco", weapon="Spear", profile="coco",
                 emissive={"Body": True, "Weapon": False, "Gore": False},
                 ue_scalars={"Body": dict(EmissiveStrength=1.0, EmissivePulse=0.0, EmissivePulseSpeed=0.3),
                             "Weapon": dict(EmissiveStrength=0.0), "Gore": dict(EmissiveStrength=0.0)}),
    "crab": dict(name="CrabGuardian", label="Gardien Crabe", weapon="ShellHammer", profile="crab",
                 emissive={"Body": True, "Weapon": False, "Gore": False},
                 ue_scalars={"Body": dict(EmissiveStrength=1.0, EmissivePulse=0.0, EmissivePulseSpeed=0.3),
                             "Weapon": dict(EmissiveStrength=0.0), "Gore": dict(EmissiveStrength=0.0)}),
    "ash": dict(name="AshWatcher", label="Veilleur des Cendres", weapon="ObsidianClub", profile="ash",
                emissive={"Body": True, "Weapon": True, "Gore": True},
                ue_scalars={"Body": dict(EmissiveStrength=5.0, EmissivePulse=0.35, EmissivePulseSpeed=0.32),
                            "Weapon": dict(EmissiveStrength=3.0, EmissivePulse=0.3, EmissivePulseSpeed=0.27),
                            "Gore": dict(EmissiveStrength=4.0, EmissivePulse=0.25, EmissivePulseSpeed=0.4)}),
}
ORDER = ("crab", "ash", "coco")
P = dict(budget_body=54000, budget_weapon=6000, tex_body=2048, tex_weapon=1024, tex_gore=1024, lod1=0.5, lod2=0.2,
         bake_samples=8, ao_samples=48)


def out_dir(name):
    return os.path.join(ART, "export", "Characters", name, "Horror")


def rel(path):
    return os.path.relpath(path, ROOT).replace("\\", "/")


def bbox(ob):
    co = np.zeros(len(ob.data.vertices) * 3, np.float32)
    ob.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    return [[round(float(x), 4) for x in co.min(0)], [round(float(x), 4) for x in co.max(0)]]


def layer_kinds(profile):
    prof = real_gore.PROFILES[profile]
    kinds = []
    for lst in prof.values():
        for kind, *_ in lst:
            if kind not in kinds:
                kinds.append(kind)
    return kinds


def build_mob(key, root, quick=False, bake=True):
    """Construit, skinne, découpe, cuit et exporte un mob ; retourne le dictionnaire R (objets, fichiers, rapport)."""
    K = T["kit"]
    info = MOBS[key]
    name = info["name"]
    t0 = time.time()
    coll = K.collection(f"CHR_Mob_{name}", root)
    dcoll = K.collection(f"CHR_Mob_{name}_Dismember", root)
    xcoll = K.collection(f"EXPORT_{name}", root)
    classes = horror_geo.make_classes()
    cr = classes[key](coll).build()
    arm = T["rig"].build_armature(cr, coll)
    log(name, "construit", round(time.time() - t0, 1), "s,", len(cr.parts), "pièces,", len(cr.weapon_parts), "pour l'arme")
    cr.shell_ob.vertex_groups.new(name=real_rig.TAG + "Shell").add(list(range(len(cr.shell_ob.data.vertices))), 1.0,
                                                                     "REPLACE")
    meshes = real_rig.skin_mob(cr, arm, coll, P["budget_body"], P["budget_weapon"])
    body, weapon = meshes["body"], meshes.get("weapon")
    real_rig.strip_tmp_groups(body)
    body.name = body.data.name = f"GEO-{name}_Body"
    if weapon:
        weapon.name = weapon.data.name = f"GEO-{name}_{info['weapon']}"
    log(name, "skinné", round(time.time() - t0, 1), "s, tris", real_rig.tris_of(body), "+",
        real_rig.tris_of(weapon) if weapon else 0)
    R = dict(key=key, info=info, cr=cr, arm=arm, body=body, weapon=weapon, raw_tris=meshes.get("raw_tris"),
             coll=coll, xcoll=xcoll)
    if quick:
        return R
    real_ue.bake_uv(body)
    if weapon:
        real_ue.bake_uv(weapon)
    # ---------------------------------------------------------- démembrement (7 modules du jeu)
    profile = info["profile"]
    order = horror_mats.LAYER_ORDER[profile]
    layer_mats = {kind: horror_mats.gore_layer(f"MAT-{name}_Gore_{kind}", kind) for kind in layer_kinds(profile)}
    cuts, sites = real_gore.cut_sites(cr)
    modules, mrep = real_gore.build_modules(body, cr, dcoll, layer_mats, order, profile, cuts, sites, only=SEVEN)
    for mname, ob in modules.items():
        ob.name = ob.data.name = f"GEO-{name}_Mod_{mname}"
        for md in ob.modifiers:
            if md.type == "ARMATURE":
                md.object = arm
        ob.parent = arm
        ob.matrix_parent_inverse = Matrix.Identity(4)
        ob.matrix_basis = Matrix.Identity(4)
    log(name, "modules", {k: v["triangles"] for k, v in mrep.items()}, round(time.time() - t0, 1), "s")
    R.update(modules=modules, mrep=mrep, cuts=cuts, sites=sites, layer_mats=layer_mats)
    # ---------------------------------------------------------- cuisson
    od = out_dir(name)
    tdir = os.path.join(od, "Textures")
    scene = bpy.context.scene
    engine0 = scene.render.engine
    tex = {}
    if bake:
        real_ue.use_gpu(scene)
        em = info["emissive"]
        tex["Body"] = real_ue.bake_set(scene, body, P["tex_body"], tdir, f"T_{name}_Body", emissive=em["Body"],
                                       samples=P["bake_samples"], ao_samples=P["ao_samples"])
        log(name, "Body cuit", round(time.time() - t0, 1), "s")
        if weapon:
            tex["Weapon"] = real_ue.bake_set(scene, weapon, P["tex_weapon"], tdir, f"T_{name}_Weapon",
                                             emissive=em["Weapon"], samples=P["bake_samples"],
                                             ao_samples=P["ao_samples"])
        atlas = real_ue.gore_atlas_mesh(layer_mats, order, coll)
        tex["Gore"] = real_ue.bake_set(scene, atlas, P["tex_gore"], tdir, f"T_{name}_Gore", emissive=em["Gore"],
                                       samples=P["bake_samples"], ao_samples=8)
        bpy.data.objects.remove(atlas, do_unlink=True)
        try:
            scene.render.engine = engine0
        except TypeError:
            pass
        log(name, "textures cuites", round(time.time() - t0, 1), "s")
    else:
        tex = load_baked(name, tdir, info)
    preview_k = {"Body": 4.0 if key == "ash" else 1.2, "Weapon": 3.0, "Gore": 4.0}
    mat_body = real_ue.baked_material(f"M_{name}_Body", tex["Body"], preview_k["Body"])
    mat_weapon = real_ue.baked_material(f"M_{name}_Weapon", tex["Weapon"], preview_k["Weapon"]) if weapon else None
    mat_gore = real_ue.baked_material(f"M_{name}_Gore", tex["Gore"], preview_k["Gore"])
    gore_set = set(layer_mats.values())
    # ---------------------------------------------------------- copies d'export, LOD
    body_x = real_ue.export_copy(body, f"SK_{name}", {i: mat_body for i in range(len(body.material_slots))})
    weapon_x = real_ue.export_copy(weapon, f"SK_{name}_{info['weapon']}",
                                   {i: mat_weapon for i in range(len(weapon.material_slots))}) if weapon else None
    for o in (body_x, weapon_x):
        if o:
            for c in list(o.users_collection):
                c.objects.unlink(o)
            xcoll.objects.link(o)
    lods = [body_x, real_ue.make_lod(body_x, f"SK_{name}_LOD1", P["lod1"]),
            real_ue.make_lod(body_x, f"SK_{name}_LOD2", P["lod2"])]
    mods_x = {}
    for mname in SEVEN:
        ob = modules[mname]
        smap = {i: (mat_gore if s.material in gore_set else mat_body) for i, s in enumerate(ob.material_slots)}
        x = real_ue.export_copy(ob, f"SK_{name}_{mname}", smap)
        for c in list(x.users_collection):
            c.objects.unlink(x)
        xcoll.objects.link(x)
        mods_x[mname] = x
    # ---------------------------------------------------------- FBX
    files = {"SK": [], "LOD": [], "Dismember": []}
    with real_ue.AsArmature(arm):
        files["SK"].append(real_ue.fbx([body_x, arm], os.path.join(od, f"SK_{name}.fbx")))
        if weapon_x:
            files["SK"].append(real_ue.fbx([weapon_x, arm], os.path.join(od, f"SK_{name}_{info['weapon']}.fbx")))
        for i, lo in enumerate(lods[1:], 1):
            files["LOD"].append(real_ue.fbx([lo, arm], os.path.join(od, "LOD", f"SK_{name}_LOD{i}.fbx")))
        for mname, x in mods_x.items():
            files["Dismember"].append(real_ue.fbx([x, arm], os.path.join(od, "Dismember", f"SK_{name}_{mname}.fbx")))
    sm_path = None
    if weapon_x:
        sm = T["export"].static_weapon(cr, weapon_x)
        sm.name = f"SM_{name}_{info['weapon']}"
        sm_path = real_ue.fbx([sm], os.path.join(od, f"SM_{name}_{info['weapon']}.fbx"), types=("MESH",))
        R["sm_bbox"] = bbox(sm)
        bpy.data.objects.remove(sm, do_unlink=True)
    for o in lods[1:] + list(mods_x.values()) + list(modules.values()):
        o.hide_render = True
        o.hide_viewport = True
    R.update(tex=tex, mats=dict(body=mat_body, weapon=mat_weapon, gore=mat_gore), body_x=body_x, weapon_x=weapon_x,
             lods=lods, mods_x=mods_x, files=files, sm_path=sm_path, od=od)
    write_manifest(R)
    log(name, "exporté", round(time.time() - t0, 1), "s")
    return R


def load_baked(name, tdir, info):
    """Textures déjà cuites (option --no-bake) : rechargées depuis les PNG."""
    tex = {}
    for sset in ("Body", "Weapon", "Gore"):
        maps = {}
        for k in ("BC", "N", "ORM", "E"):
            p = os.path.join(tdir, f"T_{name}_{sset}_{k}.png")
            if os.path.exists(p):
                img = bpy.data.images.load(p, check_existing=True)
                img.colorspace_settings.name = "sRGB" if k in ("BC", "E") else "Non-Color"
                maps[k] = img
        if maps:
            tex[sset] = maps
    return tex


def write_manifest(R):
    info, name = R["info"], R["info"]["name"]
    od = R["od"]
    kinds = {"BC": "color", "N": "normal_gl", "ORM": "masks", "E": "emissive"}
    textures, materials = [], {}
    for sset, maps in R["tex"].items():
        if sset == "Weapon" and not R["weapon_x"]:
            continue
        params = {}
        for k, img in maps.items():
            asset = f"T_{name}_{sset}_{k}"
            textures.append(dict(asset=asset, file=rel(bpy.path.abspath(img.filepath)), kind=kinds[k]))
            params[{"BC": "BaseColor", "N": "Normal", "ORM": "ORM", "E": "Emissive"}[k]] = asset
        materials[f"M_{name}_{sset}"] = dict(mi=f"MI_{name}_{sset}", textures=params,
                                             scalars=info["ue_scalars"].get(sset, {}))
    lod_tris = [real_ue.tris(o) for o in R["lods"]]
    man = {
        "format": "tiki-horror/1", "version": VERSION, "task": "T-274", "mob": name, "label": info["label"],
        "built_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "skeleton": {"blender": f"SKEL_{name}", "unreal": f"/Game/MonkeIsland/Characters/{name}/Meshes/SK_{name}_Skeleton",
                     "bones": [b.name for b in R["arm"].data.bones],
                     "heads_m": {b.name: [round(v, 5) for v in b.head_local] for b in R["arm"].data.bones}},
        "height_m": round(R["cr"].H, 3),
        "bbox_m": bbox(R["body_x"]),
        "meshes": {"lod0": rel(R["files"]["SK"][0]), "lod0_tris": lod_tris[0],
                   "lods": [{"lod": i, "file": rel(R["files"]["LOD"][i - 1]), "tris": lod_tris[i]} for i in (1, 2)],
                   "weapon_sk": rel(R["files"]["SK"][1]) if R["weapon_x"] else None,
                   "weapon_sm": rel(R["sm_path"]) if R["sm_path"] else None,
                   "weapon_tris": real_ue.tris(R["weapon_x"]) if R["weapon_x"] else 0,
                   "weapon_sm_bbox_m": R.get("sm_bbox")},
        "modules": [{"module": m, "file": rel(f), "tris": R["mrep"][m]["triangles"],
                     "boundary_edges": R["mrep"][m]["boundary_edges"], "mass_kg": R["mrep"][m]["mass_kg"]}
                    for m, f in zip(SEVEN, R["files"]["Dismember"])],
        "textures": textures,
        "materials": materials,
        "material_slots": {"body": [f"M_{name}_Body"], "weapon": [f"M_{name}_Weapon"],
                           "modules": [f"M_{name}_Body", f"M_{name}_Gore"]},
        "texture_settings": {"_BC": "sRGB, TC_Default", "_N": "TC_Normalmap, Flip Green Channel (normale OpenGL)",
                             "_ORM": "sRGB off, TC_Masks (R = AO, G = rugosité, B = métal)", "_E": "sRGB, émissif"},
        "gore_atlas": list(horror_mats.LAYER_ORDER[info["profile"]]),
    }
    path = os.path.join(od, "horror_manifest.json")
    os.makedirs(od, exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(man, fh, indent=1, ensure_ascii=False)
    R["manifest"] = path
    return path
