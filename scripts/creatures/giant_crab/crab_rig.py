"""
Rig du crabe géant v2 (T-260) : squelette SKEL_GiantCrab (58 os déformants + contrôles IK), skinning rigide, IK des
pattes et des pinces (pôles), sockets / points faibles (+ pointes des pattes SOCKET_LegTip_*), LOD, export FBX Unreal.

Provenance : factory/kits/crab_rig.py du 2e PC (maquette crabe-geant v0.2.0), rapatrié le 2026-10-09. Le squelette,
l'IK, les sockets v1 et les réglages d'export sont inchangés (compatibilité avec SK_GiantCrab, son Physics Asset et
ses clips dans Unreal). v2 : maillages régénérés par crab_kit v2 (UV d'atlas communes à tous les LOD), sockets des
pointes de pattes.

Conventions d'export (anim_export.py / bean_soldier.export_fbx) : armature renommée « Armature », FBX_SCALE_NONE +
apply_unit_scale, avant -Y / haut Z, axe primaire des os Y / secondaire X, pas d'os de bout, os déformants seulement.
Écriture atomique (.partial puis os.replace).
"""

import json
import math
import os

import bpy
from mathutils import Matrix, Vector

import crab_kit as CK



# ============================================================================= squelette

def _active(ob):
    vl = bpy.context.view_layer
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    for o in vl.objects:
        o.select_set(False)
    ob.select_set(True)
    vl.objects.active = ob


def build_armature(Lay, coll, name="SKEL_GiantCrab"):
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    coll.objects.link(arm)
    data.display_type = "STICK"
    arm.show_in_front = True
    _active(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    for bname in Lay.order:
        b = Lay.bones[bname]
        M = b["M"]
        eb = data.edit_bones.new(bname)
        eb.head = M.translation
        eb.tail = M @ Vector((0, b["length"], 0))
        eb.align_roll(M.col[2].xyz)
        eb.use_deform = b["deform"]
        if b["parent"]:
            eb.parent = data.edit_bones[b["parent"]]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    err = 0.0
    for bname in Lay.order:
        Mb = data.bones[bname].matrix_local
        Ml = Lay.bones[bname]["M"]
        err = max(err, max(abs(Mb[i][j] - Ml[i][j]) for i in range(3) for j in range(4)))
        Lay.bones[bname]["M"] = Mb.copy()          # repère exact de Blender (arrondis du roll)
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    # groupes de couleurs dans la vue (lisibilité)
    for pb in arm.pose.bones:
        if pb.name.startswith(("ik_", "pole_")):
            pb.color.palette = "THEME09"
        elif pb.name.startswith("leg_"):
            pb.color.palette = "THEME04" if pb.name[4] == "l" else "THEME03"
        elif pb.name.startswith("claw_"):
            pb.color.palette = "THEME01"
    arm["frame_error"] = err
    arm["deform_bones"] = len([b for b in data.bones if b.use_deform])
    return arm


def add_ik(arm, Lay):
    """IK des pattes (mérus, propode, dactyle -> ik_leg_*, pôle pole_leg_*) et des pinces (bras, avant-bras, main ->
    ik_claw_*, pôle pole_claw_*). Influence 0 par défaut : les clips sont écrits en FK (cuisson analytique) ; mettre
    l'influence à 1 pour animer à la main en IK, puis cuire avec anim_export.bake_pose avant l'export."""
    chains = []
    for s in CK.SIDES:
        for n in CK.PAIRS:
            chains.append((f"leg_{s}{n}_dactyl", f"ik_leg_{s}{n}", f"pole_leg_{s}{n}", 3, True))
        chains.append((f"claw_{s}_hand", f"ik_claw_{s}", f"pole_claw_{s}", 3, False))
    for bone, tgt, pole, n, use_tail in chains:
        pb = arm.pose.bones[bone]
        c = pb.constraints.new("IK")
        c.name = "IK"
        c.target = arm
        c.subtarget = tgt
        c.pole_target = arm
        c.pole_subtarget = pole
        c.chain_count = n
        c.use_tail = True
        c.use_stretch = False
        c.influence = 1.0
        c.pole_angle = _best_pole(arm, pb, c)
        c.influence = 0.0
    arm["ik_note"] = ("Contraintes IK (influence 0 par défaut) : pattes leg_*_dactyl -> ik_leg_* (pôle pole_leg_*), "
                      "pinces claw_*_hand -> ik_claw_* (pôle pole_claw_*). Clips générés en FK.")


def _chain_error(arm, pb, n):
    bpy.context.view_layer.update()
    err = 0.0
    p = pb
    for _ in range(n):
        rest = arm.data.bones[p.name].matrix_local
        err += (p.matrix.col[0].xyz - rest.col[0].xyz).length + (p.matrix.translation - rest.translation).length
        p = p.parent
    return err


def _best_pole(arm, pb, c):
    best = (1e9, 0.0)
    for deg in range(-180, 180, 10):
        c.pole_angle = math.radians(deg)
        e = _chain_error(arm, pb, c.chain_count)
        if e < best[0]:
            best = (e, deg)
    lo = best[1]
    for deg in [lo + d * 0.5 for d in range(-20, 21)]:
        c.pole_angle = math.radians(deg)
        e = _chain_error(arm, pb, c.chain_count)
        if e < best[0]:
            best = (e, deg)
    return math.radians(best[1])


# ============================================================================= maillages / LOD

def build_lod(P, Lay, pal, mats, arm, coll, lod, atlases=None, name=None):
    """Maillage d'un LOD (crab_kit v2) ; atlases = disposition UV du LOD0 (calculée au premier appel)."""
    parts = CK.build_meshes(P, Lay, pal, lod)
    report = None
    if atlases is None:
        atlases, report = CK.atlas_layout(parts)
    name = name or ("SK_GiantCrab" if lod == 0 else f"SK_GiantCrab_LOD{lod}")
    ob = CK.assemble(name, parts, coll, mats, Lay.deform_bones(), atlases)
    ob.parent = arm
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    CK.dead_eye_shape(ob)
    me = ob.data
    me.calc_loop_triangles()
    ob["tris"] = len(me.loop_triangles)
    ob["lod"] = lod
    ob["parts"] = len(parts)
    return ob, atlases, report


def check_skin(ob, arm):
    """Aucun sommet non pondéré ; somme des poids = 1 ; nombre de sommets mélangés (membranes)."""
    deform = {b.name for b in arm.data.bones if b.use_deform}
    names = {g.index: g.name for g in ob.vertex_groups}
    unweighted, bad_sum, blended = 0, 0, 0
    for v in ob.data.vertices:
        ws = [g.weight for g in v.groups if names[g.group] in deform and g.weight > 1e-5]
        if not ws:
            unweighted += 1
        elif abs(sum(ws) - 1.0) > 1e-3:
            bad_sum += 1
        if len(ws) > 1:
            blended += 1
    return dict(vertices=len(ob.data.vertices), unweighted=unweighted, weight_sum_errors=bad_sum,
                blended_membrane_vertices=blended)


# ============================================================================= sockets, points faibles, physique

def socket_table(P, Lay):
    """(nom, os, matrice monde au repos) ; l'axe Z du socket de saisie = haut de Bean quand la pince est à plat."""
    out = []
    for s in CK.SIDES:
        c = P.claw[s]
        Lp, H = c["palm"]["length_m"], c["palm"]["height_m"]
        Lf = c["fixed_finger_m"]
        hand = Lay.M(f"claw_{s}_hand")
        u = 0.6
        fz0 = -0.20 * H
        z_ff = fz0 + 0.16 * H * u ** 1.6 + 0.05 * H * math.sin(u * math.pi) + 0.30 * H * (1 - u) ** 0.75
        # repère de saisie : pince roulée (ouverture horizontale) -> Z du socket = X de la main = haut de Bean,
        # Y du socket = axe de la main (Bean regarde le crabe)
        grab = (hand @ Matrix.Translation((0.0, Lp - 0.10 + u * (Lf + 0.10), z_ff + 0.12 * H))
                @ Matrix.Rotation(math.radians(90.0 if s == "r" else -90.0), 4, "Y"))
        S = s.upper()
        out.append((f"SOCKET_ClawGrab_{S}", f"claw_{s}_hand", grab))
        out.append((f"SOCKET_ClawHit_{S}", f"claw_{s}_hand", hand @ Matrix.Translation((0, (Lp + Lf) / 2, 0))))
        fin = Lay.M(f"claw_{s}_finger")
        out.append((f"SOCKET_ClawTip_{S}", f"claw_{s}_finger",
                    fin @ Matrix.Translation((0, c["movable_finger_m"], 0))))
        if s == "r":
            out.append(("SOCKET_Throw_Release", "claw_r_hand", grab))
    mouth = Lay.M("mouth")
    out.append(("SOCKET_Mouth", "mouth", mouth @ Matrix.Translation((0, P.mouth_size[2] * 0.5, 0.06))))
    for s in CK.SIDES:
        e = Lay.M(f"eye_{s}")
        out.append((f"WeakPoint_Eye_{s.upper()}", f"eye_{s}", e @ Matrix.Translation((0, P.eye_d * 0.55, 0))))
    for s in CK.SIDES:
        out.append((f"WeakPoint_ClawJoint_{s.upper()}", f"claw_{s}_forearm", Lay.M(f"claw_{s}_forearm")))
    ab = Lay.M("abdomen")
    out.append(("WeakPoint_Belly", "abdomen", ab @ Matrix.Translation((0, P.abd_size[1] / 2, -0.05))))
    out.append(("WeakPoint_Mouth", "mouth", mouth @ Matrix.Translation((0, P.mouth_size[2] * 0.4, 0.04))))
    # v2 : pointes des pattes (bruits de pas, poussière) = queue de l'os du dactyle
    for s in CK.SIDES:
        for n in CK.PAIRS:
            b = f"leg_{s}{n}_dactyl"
            out.append((f"SOCKET_LegTip_{s.upper()}{n}", b, Lay.M(b) @ Matrix.Translation((0, Lay.bones[b]["length"], 0))))
    return out


def make_sockets(arm, P, Lay, coll):
    """Empties SOCKET_* / WeakPoint_* parentés aux os ; retourne la liste Unreal (espace os, cm, degrés)."""
    bpy.context.view_layer.update()
    entries = []
    notes = {s["name"]: s.get("note", "") for s in P.P.get("sockets", [])}
    for name, bone, Mw in socket_table(P, Lay):
        e = bpy.data.objects.new(name, None)
        e.empty_display_type = "ARROWS" if name.startswith("SOCKET") else "SPHERE"
        e.empty_display_size = 0.12 if name.startswith("SOCKET") else 0.09
        coll.objects.link(e)
        e.parent = arm
        e.parent_type = "BONE"
        e.parent_bone = bone
        bpy.context.view_layer.update()
        e.matrix_world = arm.matrix_world @ Mw
        e.hide_render = True
        rel = arm.data.bones[bone].matrix_local.inverted() @ Mw
        loc = rel.translation
        q = rel.to_quaternion()
        eul = rel.to_euler("XYZ")
        entries.append(dict(
            name=name, bone=bone, note=notes.get(name, ""),
            blender_bone_space=dict(location_m=[round(v, 4) for v in loc],
                                    rotation_quat_wxyz=[round(v, 5) for v in q]),
            unreal_relative=dict(location_cm=[round(loc.x * 100, 2), round(-loc.y * 100, 2), round(loc.z * 100, 2)],
                                 rotation_deg_rpy=[round(math.degrees(eul.x), 2), round(-math.degrees(eul.y), 2),
                                                   round(-math.degrees(eul.z), 2)]),
            component_space_rest_cm=[round(v * 100, 2) for v in (Mw.translation.x, -Mw.translation.y,
                                                                 Mw.translation.z)]))
    return entries


def phat_hint(P, Lay):
    """ue/PHAT_GiantCrab_hint.json : un corps par os principal (formes et tailles de la spec)."""
    spec = P.P["physics"]
    bodies = []

    def bone_mid(b):
        M = Lay.M(b)
        L = Lay.bones[b]["length"]
        return M @ Vector((0, L / 2, 0)), L

    for b in Lay.deform_bones():
        key = None
        for pattern in spec["bodies"]:
            import fnmatch
            if fnmatch.fnmatch(b, pattern):
                key = pattern
                break
        if key is None:
            continue
        d = spec["bodies"][key]
        mid, L = bone_mid(b)
        entry = dict(bone=b, shape=d["shape"], center_cm_component=[round(mid.x * 100, 1), round(-mid.y * 100, 1),
                                                                     round(mid.z * 100, 1)])
        if d["shape"] == "sphyl":
            entry.update(radius_cm=round(d["radius_m"] * 100, 1), length_cm=round(max(L - 2 * d["radius_m"], 0.01) * 100, 1),
                         axis="bone Y")
        elif d["shape"] == "sphere":
            entry.update(radius_cm=round(d["radius_m"] * 100, 1))
        else:
            entry.update(size_cm=[round(v * 100, 1) for v in d["size_m"]])
            if b == "body":
                entry["center_cm_component"][2] = round(d.get("center_z", 1.6) * 100, 1)
        for k in ("pawn", "weapon_trace", "phys_material"):
            if k in d:
                entry[k] = d[k]
        bodies.append(entry)
    return dict(format="phat-hint/1", skeleton="SKEL_GiantCrab", units="cm, espace composant Unreal (Y inversé)",
                capsule=spec["character_capsule"], phys_materials=spec["phys_materials"],
                ragdoll=spec["ragdoll"], bodies=bodies)


# ============================================================================= export FBX

def export_fbx(objects, path, anim=False, frames=None):
    """Mesh(es) + armature (ou armature seule si anim) ; réglages des personnages du projet ; écriture atomique."""
    scene = bpy.context.scene
    vl = bpy.context.view_layer
    arm = next(o for o in objects if o.type == "ARMATURE")
    keep_range = (scene.frame_start, scene.frame_end)
    if frames:
        scene.frame_start, scene.frame_end = frames
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    for o in vl.objects:
        o.select_set(False)
    hidden = {}
    for o in objects:
        hidden[o] = (o.hide_get(), o.hide_viewport)
        o.hide_viewport = False
        o.hide_set(False)
        o.select_set(True)
    vl.objects.active = arm
    old = arm.name
    clash = bpy.data.objects.get("Armature")
    if clash is not None and clash is not arm:
        clash.name = "Armature_hold"
    arm.name = "Armature"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path[:-4] + ".partial.fbx"
    types = {"ARMATURE"} if anim else {"ARMATURE", "MESH"}
    try:
        bpy.ops.export_scene.fbx(filepath=tmp, use_selection=True, object_types=types,
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                                 axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                                 add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
                                 use_armature_deform_only=True, armature_nodetype="NULL", bake_anim=anim,
                                 bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                                 bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                                 bake_anim_step=1.0, bake_anim_simplify_factor=0.0, use_tspace=True,
                                 path_mode="AUTO")
        os.replace(tmp, path)
    finally:
        arm.name = old
        if clash is not None and clash is not arm:
            clash.name = "Armature"
        for o, (h, hv) in hidden.items():
            o.select_set(False)
            o.hide_set(h)
            o.hide_viewport = hv
        scene.frame_start, scene.frame_end = keep_range
    return path


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".partial"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2, ensure_ascii=False)
    os.replace(tmp, path)


# ============================================================================= assemblage

def build_rig(ctx, coll, lods=(0, 1, 2, 3), mats=None):
    """Crée SKEL_GiantCrab + SK_GiantCrab (+ LOD) dans coll ; retourne un dict d'état partagé."""
    spec = ctx["spec"]
    P = CK.Params(spec["params"])
    Lay = CK.Layout(P)
    pal = CK.Pal()
    arm = build_armature(Lay, coll)
    meshes = {}
    atlases = None
    report = None
    for lod in lods:
        meshes[lod], atlases, rep = build_lod(P, Lay, pal, mats, arm, coll, lod, atlases)
        report = rep or report
        if lod:
            meshes[lod].hide_set(True)
            meshes[lod].hide_render = True
    add_ik(arm, Lay)
    return dict(P=P, Lay=Lay, pal=pal, mats=mats, arm=arm, meshes=meshes, coll=coll, atlases=atlases,
                atlas_report=report)
