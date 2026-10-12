"""T-274 (lot L5) : réimport EN PLACE des Veilleurs « Outlast » (Gardien Crabe, Veilleur des Cendres, Guerrier Coco)
dans Unreal, depuis art/export/Characters/<Mob>/Horror/ (générateur scripts/character/tiki_horror.py, manifeste
horror_manifest.json).

Par mob demandé (un à la fois : requête monke_island/Saved/T274/request.json, {"mobs": ["CrabGuardian"]}) :
1. textures T_<Mob>_{Body,Weapon,Gore}_{BC,N,ORM,E} -> /Game/MonkeIsland/Characters/<Mob>/Textures (BC sRGB ;
   N normal OpenGL, vert inversé ; ORM masques linéaires ; E émissif sRGB), remplacées en place ;
2. M_Character_Baked (maître partagé) : drapeaux d'usage, et une seule fois (version T274_PULSE) la pulsation de
   l'émission : Emissive x EmissiveStrength x (1 + EmissivePulse x sin(2 pi t EmissivePulseSpeed)), EmissivePulse = 0
   par défaut (aucun changement pour les autres instances) ;
3. MI_<Mob>_Body / _Weapon / _Gore (créées ou mises à jour, parent M_Character_Baked) ;
4. SK_<Mob> réimporté en place sur SON squelette et son Physics Asset (mêmes noms d'os), LOD1 / LOD2 importés,
   MI posées d'après imported_material_slot_name, emplacements renommés d'après le matériau importé ;
5. l'arme SK_<Mob>_<Arme> (même squelette), les 7 modules de démembrement Dismember/SK_<Mob>_<Module> ;
6. le maillage statique de l'arme ramassée /Game/MonkeIsland/Items/MobWeapons/Meshes/SM_<Mob>_<Arme> (T-161).
Les Blueprints, animations, sets de démembrement et démos de déchiquetage gardent leurs références (mêmes assets).
Un import raté arrête tout (exception : la session du lead refuse ensuite les imports jusqu'au redémarrage).
Exécution : session éditeur du lead (scripts/editor/lead_session_t186.py), action « mobsvisual », sans partie en cours.
Rapport : monke_island/Saved/T274/tiki-horror-import.json (cumulé) et import-<Mob>.json.
"""
import datetime as dt
import json
import sys
import time
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T274"
REQUEST = OUT / "request.json"
CHARS = "/Game/MonkeIsland/Characters"
MASTER = CHARS + "/_Shared/Materials/M_Character_Baked"
DROPS = "/Game/MonkeIsland/Items/MobWeapons/Meshes"
MOBS = {"CrabGuardian": "ShellHammer", "AshWatcher": "ObsidianClub", "CocoWarrior": "Spear"}
MODULES = ("Head", "Torso", "Pelvis", "Arm_L", "Arm_R", "Leg_L", "Leg_R")
PULSE_TAG, PULSE_VERSION = "t274_pulse", "1"
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
MEL = ue.MaterialEditingLibrary


class ImportFailed(RuntimeError):
    """Un import FBX ou PNG n'a pas produit l'asset attendu."""


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


class Report:
    def __init__(self, mob):
        self.data = {"mob": mob, "started_at": now(), "warnings": [], "timings_s": {}}

    def warn(self, text):
        self.data["warnings"].append(text)
        ue.log_warning(f"[T274 {self.data['mob']}] {text}")

    def timed(self, name, fn, *a):
        t = time.time()
        try:
            return fn(*a)
        finally:
            self.data["timings_s"][name] = round(time.time() - t, 1)


def setp(rep, obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as error:  # noqa: BLE001
        rep.warn(f"{obj.get_name()}.{prop}: {error}")
        return False


def import_task(filename, dest, name, options=None):
    task = ue.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = dest
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    if options is not None:
        task.options = options
    TOOLS.import_asset_tasks([task])
    path = f"{dest}/{name}"
    asset = EAL.load_asset(path) if EAL.does_asset_exist(path) else None
    if asset is None:
        raise ImportFailed(f"import of {Path(filename).name} into {path} produced no asset")
    return asset


def check_files(man):
    """Fichiers présents, non vides, sans fichier temporaire (export fini)."""
    base = ROOT / Path(man["meshes"]["lod0"]).parent
    partial = [str(p) for p in base.rglob("*") if p.name.endswith((".partial", ".tmp", ".blend1"))]
    assert not partial, f"export not complete: {partial}"
    files = [man["meshes"]["lod0"]] + [x["file"] for x in man["meshes"]["lods"]] + [m["file"] for m in man["modules"]]
    files += [man["meshes"][k] for k in ("weapon_sk", "weapon_sm") if man["meshes"].get(k)]
    files += [t["file"] for t in man["textures"]]
    missing = [f for f in files if not (ROOT / f).exists() or (ROOT / f).stat().st_size == 0]
    assert not missing, f"missing files: {missing}"
    return len(files)


# ---------------------------------------------------------------- textures et matériaux
def textures(rep, mob, man):
    out, done = {}, []
    for t in man["textures"]:
        tex = import_task(ROOT / t["file"], f"{CHARS}/{mob}/Textures", t["asset"])
        kind = t["kind"]
        if kind in ("color", "emissive"):
            setp(rep, tex, "srgb", True)
            setp(rep, tex, "compression_settings", ue.TextureCompressionSettings.TC_DEFAULT)
            group = "TEXTUREGROUP_CHARACTER"
        elif kind == "normal_gl":
            setp(rep, tex, "compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
            setp(rep, tex, "srgb", False)
            setp(rep, tex, "flip_green_channel", True)
            group = "TEXTUREGROUP_CHARACTER_NORMAL_MAP"
        else:
            setp(rep, tex, "compression_settings", ue.TextureCompressionSettings.TC_MASKS)
            setp(rep, tex, "srgb", False)
            group = "TEXTUREGROUP_CHARACTER_SPECULAR"
        if hasattr(ue.TextureGroup, group):
            setp(rep, tex, "lod_group", getattr(ue.TextureGroup, group))
        kitlib.save(tex)
        out[t["asset"]] = tex
        done.append([t["asset"], kind])
    rep.data["textures"] = done
    return out


def master_material(rep):
    master = kitlib.load(MASTER)
    assert master, f"{MASTER} missing (created by scripts/character/import_coco_real_ue.py)"
    flags = [f for f in ("used_with_skeletal_mesh",) if not master.get_editor_property(f)]
    for f in flags:
        master.set_editor_property(f, True)
    entry = {"path": master.get_path_name(), "usage_flags_set": flags}
    if EAL.get_metadata_tag(master, PULSE_TAG) == PULSE_VERSION:
        entry["pulse"] = "kept"
    elif not hasattr(MEL, "get_material_property_input_node"):
        entry["pulse"] = "skipped: MaterialEditingLibrary.get_material_property_input_node missing"
        rep.warn(entry["pulse"])
    else:
        try:
            src = MEL.get_material_property_input_node(master, ue.MaterialProperty.MP_EMISSIVE_COLOR)
            assert src is not None, "nothing plugged into Emissive Color"
            prop = ue.MaterialProperty
            tnode = MEL.create_material_expression(master, ue.MaterialExpressionTime, -900, 1050)
            speed = MEL.create_material_expression(master, ue.MaterialExpressionScalarParameter, -900, 1150)
            speed.set_editor_property("parameter_name", "EmissivePulseSpeed")
            speed.set_editor_property("default_value", 0.3)
            amp = MEL.create_material_expression(master, ue.MaterialExpressionScalarParameter, -900, 1250)
            amp.set_editor_property("parameter_name", "EmissivePulse")
            amp.set_editor_property("default_value", 0.0)
            ts = MEL.create_material_expression(master, ue.MaterialExpressionMultiply, -700, 1100)
            sine = MEL.create_material_expression(master, ue.MaterialExpressionSine, -550, 1100)
            sine.set_editor_property("period", 1.0)
            wave = MEL.create_material_expression(master, ue.MaterialExpressionMultiply, -400, 1150)
            one = MEL.create_material_expression(master, ue.MaterialExpressionAdd, -250, 1150)
            one.set_editor_property("const_b", 1.0)
            final = MEL.create_material_expression(master, ue.MaterialExpressionMultiply, -100, 900)
            MEL.connect_material_expressions(tnode, "", ts, "A")
            MEL.connect_material_expressions(speed, "", ts, "B")
            MEL.connect_material_expressions(ts, "", sine, "")
            MEL.connect_material_expressions(sine, "", wave, "A")
            MEL.connect_material_expressions(amp, "", wave, "B")
            MEL.connect_material_expressions(wave, "", one, "A")
            MEL.connect_material_expressions(src, "", final, "A")
            MEL.connect_material_expressions(one, "", final, "B")
            MEL.connect_material_property(final, "", prop.MP_EMISSIVE_COLOR)
            EAL.set_metadata_tag(master, PULSE_TAG, PULSE_VERSION)
            entry["pulse"] = "added (EmissivePulse, EmissivePulseSpeed)"
        except Exception as error:  # noqa: BLE001
            entry["pulse"] = f"failed: {error}"
            rep.warn(f"pulse on M_Character_Baked not added: {error} (static emission kept)")
    MEL.recompile_material(master)
    kitlib.save(master)
    names = []
    try:
        names = [str(n) for n in MEL.get_scalar_parameter_names(master)]
    except Exception:  # noqa: BLE001
        pass
    entry["scalar_parameters"] = names
    rep.data["master"] = entry
    return master, names


def instances(rep, mob, man, tex, master, scalars):
    mis, done = {}, []
    folder = f"{CHARS}/{mob}/Materials"
    for slot, spec in man["materials"].items():
        name = spec["mi"]
        path = f"{folder}/{name}"
        existed = EAL.does_asset_exist(path)
        mi = EAL.load_asset(path) if existed else TOOLS.create_asset(name, folder, ue.MaterialInstanceConstant,
                                                                      ue.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        # les setters de MaterialEditingLibrary renvoient toujours False (UE 5.8) : on relit les valeurs
        for param, asset in spec["textures"].items():
            MEL.set_material_instance_texture_parameter_value(mi, param, tex[asset])
        if "Emissive" not in spec["textures"]:
            spec["scalars"] = dict(spec.get("scalars", {}), EmissiveStrength=0.0)
        for param, value in spec.get("scalars", {}).items():
            if scalars and param not in scalars:
                rep.warn(f"{name}: scalar {param} not in the master, skipped")
                continue
            MEL.set_material_instance_scalar_parameter_value(mi, param, float(value))
        kitlib.save(mi)
        readback = {}
        for param, asset in spec["textures"].items():
            got = MEL.get_material_instance_texture_parameter_value(mi, param)
            readback[param] = got.get_name() if got else None
            if readback[param] != asset:
                rep.warn(f"{name}: {param} reads {readback[param]}, expected {asset}")
        for param, value in spec.get("scalars", {}).items():
            if scalars and param not in scalars:
                continue
            got = MEL.get_material_instance_scalar_parameter_value(mi, param)
            readback[param] = round(got, 3)
            if abs(got - float(value)) > 1e-3:
                rep.warn(f"{name}: {param} reads {got}, expected {value}")
        mis[slot] = mi
        done.append([name, "updated" if existed else "created", readback])
    rep.data["material_instances"] = done
    return mis


# ---------------------------------------------------------------- maillages
def skeletal_options(rep, skeleton, physics):
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.skeleton = skeleton
    if physics:
        options.physics_asset = physics
    options.create_physics_asset = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_SKELETAL_MESH
    data = options.skeletal_mesh_import_data
    for prop, value in (("import_uniform_scale", 1.0), ("convert_scene", True), ("force_front_x_axis", False),
                        ("convert_scene_unit", False), ("update_skeleton_reference_pose", False),
                        ("use_t0_as_ref_pose", False), ("import_meshes_in_bone_hierarchy", True),
                        ("import_morph_targets", False)):
        setp(rep, data, prop, value)
    if hasattr(ue, "FBXNormalImportMethod"):
        setp(rep, data, "normal_import_method", ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    return options


def assign(rep, mesh, mis):
    """MI d'après le matériau importé (le réimport garde l'ancienne liste d'emplacements) ; l'emplacement prend le
    nom du matériau importé (les modules retrouvent ainsi le corps par nom, cf. TikiDismember::ApplyMaterials)."""
    slots = list(mesh.get_editor_property("materials"))
    used = set()
    for index, slot in enumerate(slots):
        imported = str(slot.get_editor_property("imported_material_slot_name"))
        mi = mis.get(imported)
        if mi:
            slot.set_editor_property("material_interface", mi)
            if str(slot.get_editor_property("material_slot_name")) != imported:
                slot.set_editor_property("material_slot_name", imported)
            slots[index] = slot
            used.add(imported)
    mesh.set_editor_property("materials", slots)
    shown = []
    for index, slot in enumerate(mesh.get_editor_property("materials")):
        mat = slot.get_editor_property("material_interface")
        shown.append([index, str(slot.get_editor_property("material_slot_name")),
                      str(slot.get_editor_property("imported_material_slot_name")), mat.get_name() if mat else None])
    for imported in [k for k in mis if k not in used]:
        if any(s[2] == imported for s in shown):
            rep.warn(f"{mesh.get_name()}: slot {imported} kept another material")
    return shown


def lod_info(mesh):
    sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    count = sub.get_lod_count(mesh)
    verts = []
    for lod in range(count):
        try:
            verts.append(sub.get_num_verts(mesh, lod))
        except Exception:  # noqa: BLE001
            verts.append(None)
    return count, verts


def bounds(mesh):
    try:
        b = mesh.get_bounds()
        return [round(b.box_extent.x, 1), round(b.box_extent.y, 1), round(b.box_extent.z, 1)]
    except Exception:  # noqa: BLE001
        return None


def reimport_skeletal(rep, path, fbx, mis, skeleton=None):
    before = kitlib.load(path)
    if before is None:
        raise ImportFailed(f"{path} missing: the T-274 import only replaces existing assets")
    skel = skeleton or before.get_editor_property("skeleton")
    physics = before.get_editor_property("physics_asset")
    lods_before = lod_info(before)[0]
    folder, name = path.rsplit("/", 1)
    mesh = import_task(ROOT / fbx, folder, name, skeletal_options(rep, skel, physics))
    if mesh.get_editor_property("skeleton") != skel:
        raise ImportFailed(f"{name}: the reimport changed the skeleton")
    if physics and mesh.get_editor_property("physics_asset") != physics:
        mesh.set_editor_property("physics_asset", physics)
        rep.warn(f"{name}: physics asset set back")
    entry = {"asset": path, "skeleton": skel.get_name(), "physics_asset": physics.get_name() if physics else None,
             "lods_before": lods_before}
    return mesh, entry


def body(rep, mob, man, mis):
    path = f"{CHARS}/{mob}/Meshes/SK_{mob}"
    mesh, entry = reimport_skeletal(rep, path, man["meshes"]["lod0"], mis)
    sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    lods = []
    for lod in man["meshes"]["lods"]:
        result = sub.import_lod(mesh, lod["lod"], str(ROOT / lod["file"]))
        lods.append([lod["lod"], result])
        if result is None or (isinstance(result, int) and result < 0):
            raise ImportFailed(f"SK_{mob}: LOD{lod['lod']} import returned {result}")
    entry["lod_imports"] = lods
    entry["lod_count"], entry["verts_per_lod"] = lod_info(mesh)
    entry["slots"] = assign(rep, mesh, mis)
    entry["bounds_extent_cm"] = bounds(mesh)
    try:
        entry["weapon_socket"] = bool(mesh.find_socket("weapon_socket"))
    except Exception:  # noqa: BLE001
        entry["weapon_socket"] = None
    kitlib.save(mesh)
    rep.data["body"] = entry
    return mesh.get_editor_property("skeleton")


def weapon(rep, mob, man, mis, skeleton):
    if not man["meshes"].get("weapon_sk"):
        return
    path = f"{CHARS}/{mob}/Meshes/SK_{mob}_{MOBS[mob]}"
    mesh, entry = reimport_skeletal(rep, path, man["meshes"]["weapon_sk"], mis, skeleton)
    entry["slots"] = assign(rep, mesh, mis)
    entry["bounds_extent_cm"] = bounds(mesh)
    kitlib.save(mesh)
    rep.data["weapon"] = entry


def modules(rep, mob, man, mis, skeleton):
    out = {}
    files = {m["module"]: m["file"] for m in man["modules"]}
    for module in MODULES:
        path = f"{CHARS}/{mob}/Dismember/SK_{mob}_{module}"
        mesh, entry = reimport_skeletal(rep, path, files[module], mis, skeleton)
        entry["slots"] = assign(rep, mesh, mis)
        entry["bounds_extent_cm"] = bounds(mesh)
        kitlib.save(mesh)
        out[module] = entry
    rep.data["modules"] = out


def drop_collision(rep, mesh):
    """Le réimport hérité garde les anciens convexes et en ajoute un (1 -> 2 -> 3 au fil des réimports) : on repart
    d'une seule enveloppe K-DOP 26 ajustée à la nouvelle arme (comme l'unique convexe d'origine)."""
    try:
        sub = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
        before = sub.get_convex_collision_count(mesh)
        sub.remove_collisions(mesh)
        sub.add_simple_collisions(mesh, ue.ScriptCollisionShapeType.NDOP26)
        after = sub.get_convex_collision_count(mesh)
        if after != 1:
            rep.warn(f"{mesh.get_name()}: {after} convex hulls after the reset")
        return {"convex_before_reset": before, "convex_after_reset": after, "shape": "NDOP26"}
    except Exception as error:  # noqa: BLE001
        rep.warn(f"{mesh.get_name()}: collision reset failed ({error})")
        return {"error": str(error)[:200]}


def fix_drop_collisions(mob):
    """Mode {"mode": "drop_collision"} : collision de l'arme ramassée remise à une enveloppe, sans import."""
    rep = Report(mob)
    mesh = kitlib.load(f"{DROPS}/SM_{mob}_{MOBS[mob]}")
    assert mesh, f"SM_{mob}_{MOBS[mob]} missing"
    rep.data["collision"] = drop_collision(rep, mesh)
    rep.data["bounds_cm"] = bounds(mesh)
    kitlib.save(mesh)
    rep.data["passed"] = not rep.data["warnings"]
    rep.data["finished_at"] = now()
    return rep


def drop_mesh(rep, mob, man, mis):
    """Arme ramassée (maillage statique de T-161) : géométrie remplacée, collision existante gardée, MI de l'arme."""
    sm_file = man["meshes"].get("weapon_sm")
    path = f"{DROPS}/SM_{mob}_{MOBS[mob]}"
    before = kitlib.load(path)
    if not sm_file or before is None:
        rep.warn(f"drop mesh {path} or its FBX missing, skipped")
        return
    def convex_count(mesh):
        try:
            body_setup = mesh.get_editor_property("body_setup")
            return len(body_setup.get_editor_property("agg_geom").get_editor_property("convex_elems"))
        except Exception:  # noqa: BLE001
            return None
    entry = {"asset": path, "convex_before": convex_count(before), "bounds_before_cm": bounds(before)}
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    for prop, value in (("combine_meshes", True), ("auto_generate_collision", False), ("import_uniform_scale", 1.0),
                        ("convert_scene", True), ("convert_scene_unit", False), ("force_front_x_axis", False)):
        setp(rep, data, prop, value)
    folder, name = path.rsplit("/", 1)
    mesh = import_task(ROOT / sm_file, folder, name, options)
    entry["collision"] = drop_collision(rep, mesh)
    mi = mis.get(f"M_{mob}_Weapon")
    slots = []
    for index in range(len(mesh.get_editor_property("static_materials"))):
        if mi:
            mesh.set_material(index, mi)
        sm = mesh.get_editor_property("static_materials")[index]
        mat = sm.get_editor_property("material_interface")
        slots.append([index, str(sm.get_editor_property("material_slot_name")), mat.get_name() if mat else None])
    entry.update(slots=slots, convex_after=convex_count(mesh), bounds_after_cm=bounds(mesh))
    kitlib.save(mesh)
    rep.data["drop"] = entry


# ---------------------------------------------------------------- un mob
def verify_mob(mob):
    """Relecture seule (requête {"mode": "verify"}) : emplacements, LOD, MI et leurs paramètres, sans import."""
    rep = Report(mob)
    man = json.loads((ROOT / f"art/export/Characters/{mob}/Horror/horror_manifest.json").read_text(encoding="utf-8"))
    out = {}
    for slot, spec in man["materials"].items():
        mi = kitlib.load(f"{CHARS}/{mob}/Materials/{spec['mi']}")
        if mi is None:
            out[spec["mi"]] = None
            continue
        vals = {p: (lambda t: t.get_name() if t else None)(MEL.get_material_instance_texture_parameter_value(mi, p))
                for p in ("BaseColor", "Normal", "ORM", "Emissive")}
        for p in ("EmissiveStrength", "EmissivePulse", "EmissivePulseSpeed"):
            vals[p] = round(MEL.get_material_instance_scalar_parameter_value(mi, p), 3)
        parent = mi.get_editor_property("parent")
        vals["parent"] = parent.get_name() if parent else None
        out[spec["mi"]] = vals
    rep.data["material_instances"] = out
    meshes = {"body": f"{CHARS}/{mob}/Meshes/SK_{mob}", "weapon": f"{CHARS}/{mob}/Meshes/SK_{mob}_{MOBS[mob]}"}
    meshes.update({m: f"{CHARS}/{mob}/Dismember/SK_{mob}_{m}" for m in MODULES})
    for key, path in meshes.items():
        mesh = kitlib.load(path)
        if mesh is None:
            rep.data[key] = None
            continue
        slots = [[i, str(s.get_editor_property("material_slot_name")),
                  (lambda m: m.get_name() if m else None)(s.get_editor_property("material_interface"))]
                 for i, s in enumerate(mesh.get_editor_property("materials"))]
        count, verts = lod_info(mesh)
        rep.data[key] = {"lods": count, "verts": verts, "bounds_extent_cm": bounds(mesh), "slots": slots}
    rep.data["passed"] = True
    rep.data["finished_at"] = now()
    return rep


def do_mob(mob):
    rep = Report(mob)
    man_path = ROOT / f"art/export/Characters/{mob}/Horror/horror_manifest.json"
    man = json.loads(man_path.read_text(encoding="utf-8"))
    rep.data["manifest"] = {"version": man["version"], "built_at": man["built_at"],
                            "lod0_tris": man["meshes"]["lod0_tris"], "files": check_files(man)}
    tex = rep.timed("textures", textures, rep, mob, man)
    master, scalars = rep.timed("master", master_material, rep)
    mis = rep.timed("instances", instances, rep, mob, man, tex, master, scalars)
    skeleton = rep.timed("body", body, rep, mob, man, mis)
    rep.timed("weapon", weapon, rep, mob, man, mis, skeleton)
    rep.timed("modules", modules, rep, mob, man, mis, skeleton)
    rep.timed("drop", drop_mesh, rep, mob, man, mis)
    rep.data["finished_at"] = now()
    rep.data["passed"] = True
    return rep


def main():
    kitlib.check_lock()
    OUT.mkdir(parents=True, exist_ok=True)
    request = json.loads(REQUEST.read_text(encoding="utf-8")) if REQUEST.exists() else {}
    mobs = [m for m in request.get("mobs", []) if m in MOBS]
    summary_path = OUT / "tiki-horror-import.json"
    summary = json.loads(summary_path.read_text(encoding="utf-8")) if summary_path.exists() else {}
    if not mobs:
        summary["last"] = {"at": now(), "note": "no mob requested (Saved/T274/request.json {\"mobs\": [...]})"}
        summary_path.write_text(json.dumps(summary, indent=1), encoding="utf-8")
        return
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    failure = None
    try:
        for mob in mobs:
            t0 = time.time()
            try:
                mode = request.get("mode")
                rep = verify_mob(mob) if mode == "verify" else fix_drop_collisions(mob) if mode == "drop_collision"                     else do_mob(mob)
            except Exception as error:  # noqa: BLE001
                rep = Report(mob)
                rep.data.update(passed=False, error=traceback.format_exc()[-3000:], finished_at=now())
                failure = error
            rep.data["total_s"] = round(time.time() - t0, 1)
            name = f"{request['mode']}-{mob}.json" if request.get("mode") else f"import-{mob}.json"
            (OUT / name).write_text(json.dumps(rep.data, indent=1, default=str, ensure_ascii=False), encoding="utf-8")
            key = f"{mob}:{request['mode']}" if request.get("mode") else mob
            summary[key] = {k: rep.data.get(k) for k in ("passed", "finished_at", "total_s", "warnings", "error")}
            if failure is not None:
                break                                    # arrêt au premier échec
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
        summary["last"] = {"at": now(), "mobs": mobs}
        summary_path.write_text(json.dumps(summary, indent=1, default=str, ensure_ascii=False), encoding="utf-8")
    if failure is not None:
        raise failure


if __name__ == "__main__":
    main()
