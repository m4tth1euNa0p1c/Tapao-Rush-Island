"""Import the staged T-149 galleon into isolated TestFlat assets.

Run with Unreal's Python commandlet, editor closed and editor lock owned by codex.
Does not modify the original galleon assets or load/save a map.
"""
import datetime as dt
import hashlib
import json
import os
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
# Lock holders allowed to run this T-149 step: codex, or the session named in MONKE_LOCK_OWNER
LOCK_OWNERS = {"codex", os.environ.get("MONKE_LOCK_OWNER") or "codex"}
SRC = ROOT / "art/export/Galleon_T149/20261006_1203"
# Optional export variant (owner request 2026-10-06: no cannons / pumps, open cabin door): mesh-source.json names
# its folder, its result file and its UCX count; without it, the corrected export of materialfix/
VARIANT_FILE = SRC / "mesh-source.json"
VARIANT = json.loads(VARIANT_FILE.read_text(encoding="utf-8")) if VARIANT_FILE.exists() else {}
MESH_SRC = SRC / VARIANT.get("dir", "materialfix")
DEST = "/Game/MonkeIsland/Galleon/TestFlat"
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
REPORT = json.loads((SRC / "galleon-report.json").read_text(encoding="utf-8"))
RECIPES = json.loads((SRC / "material-recipes.json").read_text(encoding="utf-8"))
RESULT = {"task": "T-149", "meshes": {}, "materials": [], "textures": []}


def check_lock():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    assert lock["owner"] in LOCK_OWNERS, "Editor lock not owned by codex or MONKE_LOCK_OWNER"
    assert dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone()


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def linear(hexa):
    values = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in values]


def import_texture(relative, kind):
    source = SRC / relative
    name = "T_Galleon_" + source.stem
    path = f"{DEST}/Textures/{name}"
    texture = load(path)
    if not texture:
        task = ue.AssetImportTask()
        task.filename = str(source)
        task.destination_path = f"{DEST}/Textures"
        task.destination_name = name
        task.automated = True
        task.save = True
        TOOLS.import_asset_tasks([task])
        texture = load(path)
    assert texture, path
    texture.set_editor_property("srgb", kind == "diff")
    if kind == "normal":
        texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", True)
    elif kind != "diff":
        texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
    save(texture)
    RESULT["textures"].append(path)
    return texture


class Graph:
    def __init__(self, material):
        self.material = material
        self.index = 0

    def node(self, cls, **props):
        node = MEL.create_material_expression(self.material, cls, -1000 + self.index % 5 * 200,
                                              self.index // 5 * 180)
        self.index += 1
        for name, value in props.items():
            node.set_editor_property(name, value)
        return node

    def wire(self, source, target, pin, output=""):
        assert MEL.connect_material_expressions(source, output, target, pin), pin

    def scalar(self, value):
        return self.node(ue.MaterialExpressionConstant, r=float(value))

    def color(self, value):
        return self.node(ue.MaterialExpressionConstant3Vector, constant=ue.LinearColor(*value, 1))

    def custom(self, code, output_type, **inputs):
        pins = []
        for name in inputs:
            pin = ue.CustomInput()
            pin.set_editor_property("input_name", name)
            pins.append(pin)
        node = self.node(ue.MaterialExpressionCustom, code=code, output_type=output_type,
                         inputs=pins)
        for name, source in inputs.items():
            self.wire(source, node, name)
        return node

    def out(self, node, prop, pin=""):
        assert MEL.connect_material_property(node, pin, prop), prop


def make_material(name, original, textures):
    path = f"{DEST}/Materials/{name}"
    mat = load(path)
    if mat:
        RESULT["materials"].append(path)
        return mat
    recipe = dict(original)
    if recipe["kind"] == "canvas":
        recipe.update(tile=0.6, tint=(1.04, 1.02, 0.96) if recipe.get("patch") else (1.08, 1.02, 0.9),
                      sat=0.75 if recipe.get("patch") else 0.8, rough_add=0.12)
    mat = TOOLS.create_asset(name, f"{DEST}/Materials", ue.Material, ue.MaterialFactoryNew())
    assert mat, path
    mat.set_editor_property("two_sided", recipe["kind"] in ("canvas", "flag"))
    g = Graph(mat)
    f1, f2, f3 = (ue.CustomMaterialOutputType.CMOT_FLOAT1,
                  ue.CustomMaterialOutputType.CMOT_FLOAT2, ue.CustomMaterialOutputType.CMOT_FLOAT3)
    uv = g.node(ue.MaterialExpressionTextureCoordinate)
    tile = recipe.get("tile", 1.0)
    uv_code = f"return float2(1.0-UV.y,UV.x)/{tile};" if recipe.get("rot") else f"return UV/{tile};"
    mapped_uv = g.custom(uv_code, f2, UV=uv)
    samples = {}
    for kind, relative in recipe["maps"].items():
        tex = textures[relative]
        sampler = ue.MaterialSamplerType.SAMPLERTYPE_COLOR if kind == "diff" else (
            ue.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind == "normal" else ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
        node = g.node(ue.MaterialExpressionTextureSample, texture=tex, sampler_type=sampler)
        g.wire(mapped_uv, node, "UVs")
        samples[kind] = node
    kind = recipe["kind"]
    if kind in ("pbr", "canvas") and "diff" in samples:
        tint = recipe.get("tint", (1, 1, 1))
        code = (f"float3 c=lerp(dot(Color,float3(0.2126,0.7152,0.0722)).xxx,Color,{recipe.get('sat', 1.0)});"
                f"c*=float3({tint[0]},{tint[1]},{tint[2]});")
        if "mix" in recipe:
            value, factor = recipe["mix"]
            rgb = linear(value)
            code += f"c=lerp(c,float3({rgb[0]},{rgb[1]},{rgb[2]}),{factor});"
        code += f"c*=lerp(1.0,AO,{recipe.get('ao', 0.7)});"
        if kind == "canvas":
            rgb = linear("8D7A68")
            code += ("float seam=1-step(0.05,frac(UV.x/0.41));"
                     f"c=lerp(c,float3({rgb[0]},{rgb[1]},{rgb[2]}),seam);")
        if recipe.get("tones"):
            code += "c*=Tone*1.2;"
        code += "return c;"
        base = g.custom(code, f3, Color=samples["diff"],
                        AO=samples.get("ao") or g.scalar(1), UV=uv,
                        Tone=g.node(ue.MaterialExpressionVertexColor))
    elif kind == "flag":
        white, red = linear("E9DACD"), linear("A95B4E")
        base = g.custom(
            "float cross=max(1-step(0.085,abs(UV.x-0.5)),1-step(0.13,abs(UV.y-0.5)));"
            f"return lerp(float3({','.join(map(str,white))}),float3({','.join(map(str,red))}),cross);",
            f3, UV=uv)
    else:
        color = recipe.get("color", {"gold": "F0C46A", "glass": "3D4652", "lamp": "EAD1AB"}.get(kind, "8A6A4A"))
        base = g.color([v * recipe.get("tone",1.0) for v in linear(color)])
    g.out(base, ue.MaterialProperty.MP_BASE_COLOR)
    rough_add = recipe.get("rough_add", 0.0)
    rough = (g.custom(f"return saturate(R.r+{rough_add});", f1, R=samples["rough"])
             if "rough" in samples else g.scalar(recipe.get("rough", 0.25 if kind == "gold" else 0.6)))
    g.out(rough, ue.MaterialProperty.MP_ROUGHNESS)
    metal = recipe.get("metallic", 1.0 if kind == "gold" else 0.0)
    g.out(g.scalar(metal), ue.MaterialProperty.MP_METALLIC)
    if "normal" in samples:
        g.out(samples["normal"], ue.MaterialProperty.MP_NORMAL)
    if kind == "lamp":
        g.out(g.color([v * 2.5 for v in linear("EAD1AB")]), ue.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    save(mat)
    RESULT["materials"].append(path)
    return mat


def import_mesh(name, materials, scale=1.0, retry=False):
    path = f"{DEST}/Meshes/{name}"
    mesh = load(path)
    if mesh and not retry:
        scale = mesh.get_editor_property("asset_import_data").get_editor_property("import_uniform_scale")
    if not mesh or retry:
        options = ue.FbxImportUI()
        options.import_mesh = True
        options.import_as_skeletal = False
        # Corrected FBX retains per-face assignments for the cabin and cannons.
        # Generated T-149 PBR materials replace the imported placeholders below.
        options.import_materials = True
        options.import_textures = False
        options.import_animations = False
        options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
        data = options.static_mesh_import_data
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("auto_generate_collision", False)
        data.set_editor_property("vertex_color_import_option", ue.VertexColorImportOption.REPLACE)
        data.set_editor_property("import_uniform_scale", scale)
        task = ue.AssetImportTask()
        task.filename = str(MESH_SRC / (name + ".fbx"))
        task.destination_path = f"{DEST}/Meshes"
        task.destination_name = name
        task.replace_existing = retry
        task.automated = True
        task.options = options
        task.save = True
        TOOLS.import_asset_tasks([task])
        mesh = load(path)
    assert mesh, path
    slots = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        assert slot_name in materials, f"Unmapped slot: {name} / {slot_name}"
        mesh.set_material(index, materials[slot_name])
        slots.append(slot_name)
    box = mesh.get_bounding_box()
    size = [box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z]
    if name == "SM_Galleon":
        expected_slots = set(REPORT["exports"][name]["material_names"])
        imported_slots = {str(slot.get_editor_property("material_slot_name"))
                          for slot in mesh.get_editor_property("static_materials")}
        assert expected_slots.issubset(imported_slots), (
            f"Hull material slots were collapsed: missing {sorted(expected_slots - imported_slots)}")
        assert {"M_Galleon_Iron", "M_Galleon_IronRust", "M_Galleon_OakDark"}.issubset(imported_slots), (
            "Cannon barrel, ironwork, or carriage slots are missing from the hull FBX")
        if max(size[:2]) < 400 and not retry:
            return import_mesh(name, materials, scale=100.0, retry=True)
        assert 2000 < max(size[:2]) < 4000, f"Wrong hull scale: {size}"
        mesh_editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
        convex_count = mesh_editor.get_convex_collision_count(mesh)
        assert convex_count >= VARIANT.get("ucx", REPORT["exports"][name]["ucx"]), (
            f"Missing authored UCX collision (including stair ramps): {convex_count}")
        body = mesh.get_editor_property("body_setup")
        # Preserve imported UCX convexes as simple collision. In particular,
        # the UCX stair ramps must remain walkable; complex-as-simple discards
        # those shapes and leaves only the detailed render triangles.
        body.set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
        for socket_name, coords in REPORT["sockets"].items():
            socket = mesh.find_socket(socket_name)
            assert socket, f"Missing socket: {socket_name}"
            # Authoritative delivery coordinates avoid FBX socket scale ambiguities.
            socket.set_editor_property("relative_scale", ue.Vector(1, 1, 1))
            socket.set_editor_property("relative_location", ue.Vector(coords[0]*100, -coords[1]*100, coords[2]*100))
    save(mesh)
    RESULT["meshes"][name] = {"path": path, "size_cm": size, "slots": slots, "import_scale": scale}
    if name == "SM_Galleon":
        RESULT["meshes"][name]["walkable_collision"] = "simple_and_complex; imported UCX stair ramps preserved"
        RESULT["meshes"][name]["convex_collision_count"] = convex_count
        RESULT["meshes"][name]["cannon_material_slots"] = [s for s in slots if "Iron" in s or "OakDark" in s]
    return mesh


def main():
    check_lock()
    manifest = json.loads((SRC / "source-manifest.json").read_text(encoding="utf-8"))
    for record in manifest["files"]:
        assert hashlib.sha256((SRC / record["file"]).read_bytes()).hexdigest() == record["sha256"], record["file"]
        assert hashlib.sha256(Path(record["source"]).read_bytes()).hexdigest() == record["sha256"], (
            "OneDrive delivery changed; stage the new revision before importing: " + record["file"])
    repaired = json.loads((MESH_SRC / VARIANT.get("result", "repair-result.json")).read_text(encoding="utf-8"))
    for name, info in repaired["exports"].items():
        assert hashlib.sha256((MESH_SRC / (name + ".fbx")).read_bytes()).hexdigest() == info["sha256"], name
        assert len(info["faces_by_material"]) == REPORT["exports"][name]["materials"], name
    textures = {}
    for recipe in RECIPES.values():
        for kind, relative in recipe["maps"].items():
            if relative not in textures:
                textures[relative] = import_texture(relative, kind)
    materials = {name: make_material(name, recipe, textures) for name, recipe in RECIPES.items()}
    # Reimport corrected FBX files: the source exporter reset every face to slot
    # zero while merging tone variants. The repair restores the face assignments.
    meshes = {"SM_Galleon": import_mesh("SM_Galleon", materials, retry=True)}
    scale = RESULT["meshes"]["SM_Galleon"]["import_scale"]
    for name in REPORT["exports"]:
        if name != "SM_Galleon":
            meshes[name] = import_mesh(name, materials, scale=scale, retry=True)
    # The full hull already includes this geometry. Do not place a second,
    # coincident battery over it: that would double faces and cause flickering.
    RESULT["embedded_cannons"] = repaired.get("embedded_cannon_triangles", {})
    RESULT["variant"] = VARIANT
    path = f"{DEST}/DA_GalleonRules_TestFlat"
    rules = load(path)
    if not rules:
        rules = EAL.duplicate_asset("/Game/MonkeIsland/Galleon/DA_GalleonRules", path)
    assert rules
    for prop, suffix in (("hull_mesh", ""), ("sails_mesh", "_Sails"), ("rigging_mesh", "_Rigging"),
                         ("flag_mesh", "_Flag"), ("wheel_mesh", "_Wheel"), ("rudder_mesh", "_Rudder")):
        rules.set_editor_property(prop, meshes["SM_Galleon" + suffix])
    for prop, name in (("flag_pivot", "SM_Galleon_Flag"), ("wheel_pivot", "SM_Galleon_Wheel"),
                       ("rudder_pivot", "SM_Galleon_Rudder")):
        x, y, z = REPORT["exports"][name]["pivot_in_SM_Galleon_m"]
        rules.set_editor_property(prop, ue.Vector(x*100, -y*100, z*100))
    x, y, z = REPORT["sockets"]["ladder_top"]
    rules.set_editor_property("ladder_deck_point", ue.Vector(x*100, -y*100, z*100))
    save(rules)
    RESULT["rules"] = path
    RESULT["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    output = ROOT / "monke_island/Saved/T149/import-result.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(RESULT, indent=2), encoding="utf-8")
    ue.log("T149_IMPORT_SUCCESS " + str(output))


if __name__ == "__main__":
    main()
