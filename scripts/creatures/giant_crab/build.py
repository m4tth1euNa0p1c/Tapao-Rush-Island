"""
Générateur du crabe géant « Tupa Nui » v2 (T-260, lot L5) : modèle, rig, cuisson PBR, clips, exports, rendus.

Blender 5.2 en arrière-plan uniquement (RAM réservée à Unreal) :
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
      --python scripts/creatures/giant_crab/build.py -- [--bake] [--export] [--render] [--no-save] [--lods 0,1,2,3]

  --bake     cuit les textures (Cycles CPU) dans art/export/Characters/GiantCrab/Textures (sinon : réutilise les
             textures déjà cuites si elles existent, ou un aperçu en couleur de sommet)
  --export   FBX (SK_GiantCrab + LOD1..3, clips A_GiantCrab_*) et manifeste GiantCrabManifest_v2.json
  --render   rendus de contrôle art/_previews/giant_crab_v2_*.png
  --no-save  ne réécrit pas art/blender/characters/giant_crab_v2.blend
  --quick    LOD0 seul, sans clips (essais de forme)
Sorties : art/export/Characters/GiantCrab/ (FBX, Textures/, Anims/, manifeste), build/giant_crab_v2/build_report.json.
Le squelette, l'échelle et les sockets sont ceux de la v1 (voir crab_kit) : réimport en place dans Unreal par
scripts/creatures/giant_crab/import_giant_crab_v2_ue.py (action crabv2import).
"""
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

import crab_anims as CA  # noqa: E402
import crab_bake as CB  # noqa: E402
import crab_kit as CK  # noqa: E402
import crab_render as RN  # noqa: E402
import crab_rig as CR  # noqa: E402

OUT = os.path.join(ROOT, "art", "export", "Characters", "GiantCrab")
TEX_DIR = os.path.join(OUT, "Textures")
ANIM_DIR = os.path.join(OUT, "Anims")
PREVIEWS = os.path.join(ROOT, "art", "_previews")
BLEND = os.path.join(ROOT, "art", "blender", "characters", "giant_crab_v2.blend")
REPORT = os.path.join(ROOT, "build", "giant_crab_v2", "build_report.json")
UE_ROOT = "/Game/MonkeIsland/Characters/GiantCrab"
T0 = time.time()


def log(*a):
    print(f"[crab v2 {time.time() - T0:6.1f}s]", *a, flush=True)


def args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    lods = (0, 1, 2, 3)
    if "--lods" in argv:
        lods = tuple(int(x) for x in argv[argv.index("--lods") + 1].split(","))
    quick = "--quick" in argv
    return dict(bake="--bake" in argv, export="--export" in argv, render="--render" in argv,
                save="--no-save" not in argv and not quick, lods=(0,) if quick else lods, quick=quick,
                anims=not quick)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.name = "SCN-GiantCrab_v2"
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 1.0
    sc.render.fps = CA.FPS
    return sc


def measures(st):
    """Contrôles de dimensions sur le LOD0 au repos (dos, ventre, envergure, ouverture de pince)."""
    ob = st["meshes"][0]
    me = ob.data
    co = np.zeros(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    if me.shape_keys:                                   # coordonnées de Basis
        me.shape_keys.key_blocks["Basis"].data.foreach_get("co", co.ravel())
    names = {g.index: g.name for g in ob.vertex_groups}
    body_w = np.zeros(len(co))
    for v in me.vertices:
        for g in v.groups:
            if names[g.group] in ("body", "abdomen") and g.weight > 0.5:
                body_w[v.index] = 1.0
    under = (body_w > 0) & (np.abs(co[:, 0]) < 1.0) & (np.abs(co[:, 1] + 0.05) < 0.9)
    Lay = st["Lay"]
    tips = [Lay.tail(f"leg_{s}{n}_dactyl") for s in CK.SIDES for n in CK.PAIRS]
    tx = [t.x for t in tips]
    cara_top = float(co[body_w > 0][:, 2].max())
    return dict(bbox_min_m=[round(float(v), 3) for v in co.min(0)], bbox_max_m=[round(float(v), 3) for v in co.max(0)],
                max_height_with_spines_m=round(float(co[:, 2].max()), 3), body_top_with_spines_m=round(cara_top, 3),
                carapace_dorsal_top_m=st["P"].top_z,
                belly_clearance_m=round(float(co[under][:, 2].min()), 3) if under.any() else None,
                leg_span_tips_m=round(max(tx) - min(tx), 3),
                tips_min_z_m=round(min(t.z for t in tips), 4))


def ground_check(st, step=2):
    """Point le plus bas du maillage (LOD2, ou le LOD le plus léger disponible) par clip, une image sur step :
    contrôle de pénétration dans le sable."""
    arm = st["arm"]
    lod = 2 if 2 in st["meshes"] else max(st["meshes"])
    ob = st["meshes"][lod]
    hidden = ob.hide_get()
    ob.hide_set(False)
    scene = bpy.context.scene
    out = {}
    for name, act in st["actions"].items():
        CA.set_action(arm, act)
        n = st["animator"].clips[name]["n"]
        lows = []
        for f in list(range(0, n + 1, step)) + [n]:
            scene.frame_set(f)
            dg = bpy.context.evaluated_depsgraph_get()
            ev = ob.evaluated_get(dg)
            me = ev.to_mesh()
            co = np.empty(len(me.vertices) * 3, np.float32)
            me.vertices.foreach_get("co", co)
            zz = co.reshape(-1, 3)[:, 2]
            i = int(np.argmin(zz))
            lows.append((float(zz[i]), f, i))
            ev.to_mesh_clear()
        z, f, i = min(lows)
        v = ob.data.vertices[i]
        bone = max(v.groups, key=lambda g: g.weight).group if len(v.groups) else -1
        out[name] = dict(min_z_m=round(z, 3), frame=f, part=ob.vertex_groups[bone].name if bone >= 0 else None)
    CA.set_action(arm, None)
    scene.frame_set(0)
    ob.hide_set(hidden)
    return out


def tri_counts(st):
    return {f"LOD{k}": int(ob["tris"]) for k, ob in st["meshes"].items()}


def write_manifest(st, files, checks, extra):
    P = st["P"]
    rel = "art/export/Characters/GiantCrab"
    clips = CA.clip_records(st, rel + "/Anims")
    tips = []
    for s in CK.SIDES:
        for n in CK.PAIRS:
            b = f"leg_{s}{n}_dactyl"
            tips.append(dict(bone=b, socket=f"SOCKET_LegTip_{s.upper()}{n}",
                             tip_bone_space_cm=[0.0, round(st["Lay"].bones[b]["length"] * 100, 1), 0.0],
                             contact_height_cm=0.0,
                             notify=f"AN_Step_{b}"))
    textures = []
    for slot, name in enumerate(CK.MAT_SLOTS):
        base = CB.TEX[slot]
        for tag, kind in (("BC", "color"), ("ORM", "masks"), ("N", "normal_directx")) + ((("E", "mask"),) if slot == CK.SLOT_EYE else ()):
            textures.append(dict(asset=f"{base}_{tag}", file=f"{rel}/Textures/{base}_{tag}.png", kind=kind,
                                 srgb=kind == "color", size=CK.V2["eye_px"] if slot == CK.SLOT_EYE else CK.V2["atlas_px"]))
    man = dict(
        format="giant-crab-manifest/2", version="2.0.0", task="T-260", fps=CA.FPS, units="m (Blender) / cm (Unreal)",
        unreal_root=UE_ROOT,
        paths=dict(meshes=UE_ROOT + "/Meshes", anims=UE_ROOT + "/Anims", materials=UE_ROOT + "/Materials",
                   textures=UE_ROOT + "/Textures", notifies="/Game/MonkeIsland/Characters/_Shared/Notifies"),
        export_settings="FBX : armature 'Armature', FBX_SCALE_NONE + apply_unit_scale, -Y avant / Z haut, axe d'os "
                        "primaire Y / secondaire X, sans os de bout, os déformants seulement ; une action par fichier",
        skeleton=dict(blender="SKEL_GiantCrab", count=len(st["Lay"].deform_bones()),
                      deform_bones=st["Lay"].deform_bones(), unreal=UE_ROOT + "/Meshes/SK_GiantCrab_Skeleton",
                      note="identique à la v1 (noms, hiérarchie, repères de repos)"),
        meshes=dict(lod0=f"{rel}/SK_GiantCrab.fbx", lod0_tris=int(st["meshes"][0]["tris"]),
                    lods=[dict(lod=k, file=f"{rel}/SK_GiantCrab_LOD{k}.fbx", tris=int(st["meshes"][k]["tris"]))
                          for k in sorted(st["meshes"]) if k > 0]),
        material_slots=CK.MAT_SLOTS,
        material_instances=CK.UE_MI,
        materials_ue={
            "MI_GiantCrab_Body": dict(slot="MI_GiantCrab_Chitin",
                                      parent="/Game/MonkeIsland/Characters/_Shared/Materials/M_Character_Baked",
                                      textures=dict(BaseColor="T_GiantCrab_Body_BC", ORM="T_GiantCrab_Body_ORM",
                                                    Normal="T_GiantCrab_Body_N"), scalars=dict(EmissiveStrength=0.0)),
            "MI_GiantCrab_Legs": dict(slot="MI_GiantCrab_Legs",
                                      parent="/Game/MonkeIsland/Characters/_Shared/Materials/M_Character_Baked",
                                      textures=dict(BaseColor="T_GiantCrab_Legs_BC", ORM="T_GiantCrab_Legs_ORM",
                                                    Normal="T_GiantCrab_Legs_N"), scalars=dict(EmissiveStrength=0.0)),
            "MI_GiantCrab_Eyes": dict(slot="MI_GiantCrab_Eye", parent=UE_ROOT + "/Materials/M_GiantCrab_Eyes",
                                      textures=dict(BaseColor="T_GiantCrab_Eyes_BC", ORM="T_GiantCrab_Eyes_ORM",
                                                    Normal="T_GiantCrab_Eyes_N", EmissiveMask="T_GiantCrab_Eyes_E"),
                                      vectors=dict(EmissiveColor=[1.0, 0.45, 0.06, 1.0]),
                                      scalars=dict(EmissiveStrength=2.0, DeadEyes=0.0),
                                      note="émission = EmissiveMask x EmissiveColor x EmissiveStrength ; frénésie "
                                           "(T-261, UGiantCrabRules::FrenzyEyeColorParams) : EmissiveColor rouge HDR"),
        },
        textures=textures, normal_convention="DirectX (vert inversé), ne pas cocher Flip Green Channel",
        morph_targets={"DeadEyes": "0 = œil normal, 1 = voile laiteux ; courbe DeadEyes des clips Death*"},
        sockets=st["sockets"], leg_tips=tips, physics_asset_hint=rel + "/PHAT_GiantCrab_hint.json",
        leg_tips_note="os de pointe = leg_<s><n>_dactyl, pointe au bout de l'os (queue, 'tip_bone_space_cm'), "
                      "socket SOCKET_LegTip_<S><n> au même endroit ; contact au sol à 0 cm",
        clips=clips, new_clips_v2=CA.ORDER_V2, checks=checks, **extra)
    path = os.path.join(OUT, "GiantCrabManifest_v2.json")
    CR.write_json(path, man)
    return path, man


def main():
    a = args()
    log("options", a)
    scene = reset()
    with open(os.path.join(HERE, "crab_spec.json"), encoding="utf-8") as fh:
        spec = json.load(fh)
    ctx = dict(spec=spec, dir=HERE)
    coll = bpy.data.collections.new("SK_GiantCrab")
    scene.collection.children.link(coll)
    mats = CB.materials()
    st = CR.build_rig(ctx, coll, lods=a["lods"], mats=mats)
    log("maillages", tri_counts(st), "atlas", st["atlas_report"])
    arm = st["arm"]
    st["sockets"] = CR.make_sockets(arm, st["P"], st["Lay"], coll)
    report = dict(tris=tri_counts(st), atlas=st["atlas_report"], skin=CR.check_skin(st["meshes"][0], arm),
                  measures=measures(st), deform_bones=len(st["Lay"].deform_bones()))
    log("mesures", report["measures"], "peau", report["skin"])
    checks = {}
    if a["anims"]:
        an, acts = CA.build_actions(st)
        checks = an.check()
        bad = {k: v for k, v in checks.items() if v.get("ik_unreachable_frames")}
        log("clips", len(acts), "IK hors portée :", {k: (v["ik_unreachable_frames"], v["ik_unreachable_legs"])
                                                     for k, v in bad.items()})
        report["anim_checks"] = checks
        report["ground"] = ground_check(st)
        deep = {k: v for k, v in report["ground"].items() if v["min_z_m"] < -0.06}
        log("sol : pénétrations > 6 cm", deep)
    tex_ready = all(os.path.exists(os.path.join(TEX_DIR, f"{CB.TEX[i]}_{t}.png")) for i in range(3) for t in ("BC", "ORM", "N"))
    if a["bake"]:
        log("cuisson…")
        files = CB.bake(st["meshes"][0], TEX_DIR, log=log)
        report["textures"] = sorted(os.path.relpath(f, ROOT).replace("\\", "/") for f in files.values())
        tex_ready = True
    if tex_ready:
        CB.use_baked(mats, TEX_DIR)
    else:
        CB.vertex_preview(mats)
    files = {}
    if a["export"]:
        log("export FBX…")
        files["SK_GiantCrab"] = CR.export_fbx([st["meshes"][0], arm], os.path.join(OUT, "SK_GiantCrab.fbx"))
        for lod in [k for k in st["meshes"] if k > 0]:
            files[f"SK_GiantCrab_LOD{lod}"] = CR.export_fbx([st["meshes"][lod], arm],
                                                           os.path.join(OUT, f"SK_GiantCrab_LOD{lod}.fbx"))
        if a["anims"]:
            files.update({f"A_GiantCrab_{k}": v for k, v in CA.export_clips(st, ANIM_DIR, CR).items()})
        CR.write_json(os.path.join(OUT, "PHAT_GiantCrab_hint.json"), CR.phat_hint(st["P"], st["Lay"]))
        path, man = write_manifest(st, files, checks, dict(build=report))
        log("manifeste", path)
    if a["render"]:
        log("rendus…")
        RN.setup(scene)
        for lod, ob in st["meshes"].items():
            ob.hide_render = lod != 0
            ob.hide_set(lod != 0)
        CA.set_action(arm, None)
        for pb in arm.pose.bones:
            pb.location = (0, 0, 0)
            pb.rotation_quaternion = (1, 0, 0, 0)
        out = {}
        out["trois_quarts"] = RN.render(scene, "trois_quarts", PREVIEWS)
        out["face"] = RN.render(scene, "face", PREVIEWS)
        out["pattes"] = RN.render(scene, "pattes", PREVIEWS)
        RN.add_bean((0.95, -0.55, 0.0), 200.0)                       # sous le corps (ventre à 1,16 m)
        RN.add_bean((-2.0, -3.7, 0.0), 160.0, with_human=(-3.3, -3.2, 0.0))
        out["avec_bean"] = RN.render(scene, "avec_bean", PREVIEWS)
        if a["anims"] and "Roar" in st.get("actions", {}):
            for o in [o for o in bpy.data.objects if o.name.startswith(("REF_Human", "Bean_", "SKEL_Bean"))]:
                o.hide_render = True
            for c in bpy.data.collections:
                if c.name.startswith("REF-Bean"):
                    c.hide_render = True
            CA.set_action(arm, st["actions"]["Roar"])
            out["roar"] = RN.render(scene, "roar", PREVIEWS, frame=30)
            CA.set_action(arm, None)
        report["renders"] = {k: os.path.relpath(v, ROOT).replace("\\", "/") for k, v in out.items()}
        log("rendus", report["renders"])
    report["seconds"] = round(time.time() - T0, 1)
    CR.write_json(REPORT, report)
    if a["save"]:
        os.makedirs(os.path.dirname(BLEND), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=BLEND, compress=True)
        log("enregistré", BLEND)
    log("terminé")


if __name__ == "__main__":
    main()
