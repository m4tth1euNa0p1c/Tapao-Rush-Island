"""Import staged T-179 relics and temple modules from the full Unreal editor."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import sys
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
STAGE = ROOT / "art/export/OneDriveShowcase/20261006"
OUT = ROOT / "monke_island/Saved/T179"
TEMPLE = "/Game/MonkeIsland/Environment/TempleReliquary"
RELICS = "/Game/MonkeIsland/Items/PolynesianArtifacts"
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
MP = ue.MaterialProperty
sys.path.insert(0, str(ROOT / "scripts/props"))
from import_galleon_t149 import Graph, linear, check_lock

PREFLIGHT = json.loads((STAGE / "preflight-result.json").read_text(encoding="utf-8"))["meshes"]
ART = json.loads((STAGE / "artefacts-polynesiens/artifacts-report.json").read_text(encoding="utf-8"))["objects"]
TR = json.loads((STAGE / "temple-reliquaire/temple-report.json").read_text(encoding="utf-8"))
RESULT = {"task": "T-179", "meshes": {}, "materials": [], "textures": []}
TEXTURES = {}


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(obj):
    assert EAL.save_loaded_asset(obj, only_if_is_dirty=False), obj.get_path_name()


def texture(relative, kind, dest, flip=False):
    key = (relative, dest)
    if key in TEXTURES:
        return TEXTURES[key]
    source = STAGE / relative
    name = source.stem if source.stem.startswith("T_") else "T_Temple_" + source.stem
    path = dest + "/Textures/" + name
    tex = load(path)
    if not tex:
        task = ue.AssetImportTask()
        task.filename = str(source)
        task.destination_path = dest + "/Textures"
        task.destination_name = name
        task.automated = True
        task.save = True
        TOOLS.import_asset_tasks([task])
        tex = load(path)
    assert tex, path
    tex.set_editor_property("srgb", kind in ("diff", "emissive"))
    tex.set_editor_property("max_texture_size", 2048)
    if kind == "normal":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("flip_green_channel", flip)
    elif kind not in ("diff", "emissive"):
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
    save(tex)
    TEXTURES[key] = tex
    RESULT["textures"].append(path)
    return tex


def make_material(slot, recipe, dest):
    name = "M_" + slot.removeprefix("MAT-").removeprefix("MAT_").removeprefix("M_")
    path = dest + "/Materials/" + name
    mat = load(path)
    if not mat:
        mat = TOOLS.create_asset(name, dest + "/Materials", ue.Material, ue.MaterialFactoryNew())
    assert mat
    MEL.delete_all_material_expressions(mat)
    kind = recipe.get("kind", "pbr")
    g = Graph(mat)
    f1, f2, f3 = (ue.CustomMaterialOutputType.CMOT_FLOAT1,
                  ue.CustomMaterialOutputType.CMOT_FLOAT2, ue.CustomMaterialOutputType.CMOT_FLOAT3)
    uv = g.node(ue.MaterialExpressionTextureCoordinate)
    mapped = g.custom(f"return UV/{recipe.get('tile', 1.0)};", f2, UV=uv)
    samples = {}
    for channel, relative in recipe.get("maps", {}).items():
        sampler = (ue.MaterialSamplerType.SAMPLERTYPE_COLOR if channel in ("diff", "emissive") else
                   ue.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel == "normal" else ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
        sample = g.node(ue.MaterialExpressionTextureSample,
                        texture=texture(relative, channel, dest, recipe.get("normal_gl", False)), sampler_type=sampler)
        g.wire(mapped, sample, "UVs")
        samples[channel] = sample
    vertex = g.node(ue.MaterialExpressionVertexColor)
    mask = g.custom("return Mask;", f3, Mask=vertex)
    base = samples.get("diff") or g.color(linear(recipe.get("color", "B9B2A4")))
    rough = samples.get("rough") or g.scalar(recipe.get("roughness", 0.7))
    metal = samples.get("metal") or g.scalar(recipe.get("metallic", 0.0))
    if "orm" in samples:
        g.out(samples["orm"], MP.MP_AMBIENT_OCCLUSION, "R")
        rough = g.custom("return T.g;", f1, T=samples["orm"])
        metal = g.custom("return T.b;", f1, T=samples["orm"])
    elif "ao" in samples:
        g.out(samples["ao"], MP.MP_AMBIENT_OCCLUSION, "R")
    if dest == TEMPLE:
        tint = recipe.get("tint", [1, 1, 1])
        base = g.custom(f"return lerp(dot(C,float3(.2126,.7152,.0722)).xxx,C,{recipe.get('sat',1)})"
                        f"*float3({tint[0]},{tint[1]},{tint[2]});", f3, C=base)
    if kind == "gold":
        gold, tarnish, polish, green = [linear(c) for c in ("E7C07A", "3A2A18", "F3D58E", "4E6B55")]
        vec = lambda x: "float3(" + ",".join(map(str,x)) + ")"
        base = g.custom(f"float3 c={vec(gold)}*(.85+.3*dot(T,float3(.2126,.7152,.0722)));"
                        f"c=lerp(c,{vec(tarnish)},Mask.r*.85);c=lerp(c,{vec(polish)},Mask.g);"
                        f"return lerp(c,{vec(green)},Mask.b*.35);", f3, T=base, Mask=mask)
        rough = g.custom("return saturate(.24+.30*(R.r-.3)+.35*Mask.r-.10*Mask.g);", f1, R=rough, Mask=mask)
        metal = g.custom("return saturate(1-.55*Mask.r-.35*Mask.b);", f1, Mask=mask)
    if kind == "gem":
        fresnel = g.node(ue.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.04)
        base = g.custom("return C*(.6+.4*F);", f3, C=base, F=fresnel)
        rough = g.custom("return .04+.4*Mask.g;", f1, Mask=mask)
        g.out(g.scalar(0.9), MP.MP_SPECULAR)
    if kind in ("pearl", "nacre"):
        mat.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_DEFAULT_LIT)
        g.out(g.scalar(.8), MP.MP_SPECULAR)
        fresnel = g.node(ue.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=.04)
        base = g.custom("return C + float3(.018,.033,.038)*F;", f3, C=base, F=fresnel)
        if kind == "nacre":
            rough = g.custom("return .16+.45*Mask.g;", f1, Mask=mask)
        elif not recipe.get("maps"):
            rough = g.custom("return .22+.33*Mask.g;", f1, Mask=mask)
    if kind == "feather":
        mat.set_editor_property("blend_mode", ue.BlendMode.BLEND_MASKED)
        mat.set_editor_property("two_sided", True)
        mat.set_editor_property("opacity_mask_clip_value", 0.45)
        mat.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        g.out(samples["diff"], MP.MP_OPACITY_MASK, "A")
        g.out(g.custom("return C*.35;", f3, C=base), MP.MP_SUBSURFACE_COLOR)
    if "normal" in samples:
        g.out(samples["normal"], MP.MP_NORMAL, "RGB")
    if kind == "flame":
        g.out(g.custom(f"return C*{recipe.get('intensity',3.0)};", f3, C=base), MP.MP_EMISSIVE_COLOR)
    # Dormant relics keep their source emissive maps available without a glow.
    g.out(base, MP.MP_BASE_COLOR)
    g.out(rough, MP.MP_ROUGHNESS)
    g.out(metal, MP.MP_METALLIC)
    MEL.recompile_material(mat)
    save(mat)
    RESULT["materials"].append(path)
    return mat


def artifact_recipes():
    recipes = {}
    for name, info in ART.items():
        source = info["textures"]
        maps = {target: "artefacts-polynesiens/" + source[k] for target, k in
                (("diff", "BaseColor"), ("normal", "Normal_DX"), ("orm", "ORM"))}
        recipes["MAT_" + name] = {"maps": maps, "kind": "pearl" if "Pearl" in name else "pbr", "coat": 0.75}
    shared = "artefacts-polynesiens/textures/Shared/"
    recipes["MAT_Gold_Aged"] = {"kind": "gold", "normal_gl": True, "maps": {
        "diff": shared+"T_Gold_Aged_Color.jpg", "rough": shared+"T_Gold_Aged_Roughness.jpg", "normal": shared+"T_Gold_Aged_Normal.jpg"}}
    for name, color in {"Ruby": "5A0710", "Sapphire": "0A1A55", "Emerald": "08401E"}.items():
        recipes["MAT_Gem_" + name] = {"kind": "gem", "color": color}
    for name, color in {"White": "CFC8BA", "Black": "0E1113"}.items():
        recipes["MAT_Pearl_" + name] = {"kind": "pearl", "color": color}
    recipes["MAT_Nacre_Pinctada"] = {"kind": "nacre", "normal_gl": True, "maps": {
        "diff": shared+"T_Nacre_Pinctada_Color.png", "normal": shared+"T_Nacre_Pinctada_Normal.png"}}
    recipes["MAT_Feather_Aged"] = {"kind": "feather", "roughness": 0.72, "normal_gl": True, "maps": {
        "diff": shared+"T_Feather_Aged_Color.png", "normal": shared+"T_Feather_Aged_Normal.png"}}
    return recipes


def import_mesh(name, dest, materials, info, lods=False):
    mesh_editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    assert mesh_editor, "Full editor required for static mesh import checks"
    path = dest + "/Meshes/" + name
    source_name = name + "_LOD0" if lods else name
    preflight = PREFLIGHT[source_name]
    mesh = load(path)
    if not mesh or mesh_editor.get_convex_collision_count(mesh) < preflight["ucx_count"]:
        task = ue.AssetImportTask()
        source = STAGE / preflight["source"]
        repaired = STAGE / "repaired_collisions" / source.name
        task.filename = str(repaired if lods and repaired.exists() else source)
        task.destination_path = dest + "/Meshes"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        options = ue.FbxImportUI()
        options.import_as_skeletal = False
        options.import_mesh = True
        options.import_materials = False
        options.import_textures = False
        options.import_animations = False
        options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
        data = options.static_mesh_import_data
        for prop, value in {"combine_meshes": True, "auto_generate_collision": False,
                            "import_uniform_scale": 1.0, "convert_scene": True, "convert_scene_unit": True,
                            "vertex_color_import_option": ue.VertexColorImportOption.REPLACE}.items():
            data.set_editor_property(prop, value)
        task.options = options
        TOOLS.import_asset_tasks([task])
        mesh = load(path)
    assert mesh, path
    before_lod_collision = mesh_editor.get_convex_collision_count(mesh)
    assert before_lod_collision >= preflight["ucx_count"], ("before_lods", name, before_lod_collision)
    if lods and mesh_editor.get_lod_count(mesh) < 3:
        for lod in (1, 2):
            file = STAGE / PREFLIGHT[f"{name}_LOD{lod}"]["source"]
            assert mesh_editor.import_lod(mesh, lod, str(file)) == lod, file
    slots = list(mesh.get_editor_property("static_materials"))
    assigned = []
    for slot in slots:
        slot_name = str(slot.get_editor_property("material_slot_name"))
        assert slot_name in materials, (name, slot_name)
        slot.set_editor_property("material_interface", materials[slot_name])
        assigned.append(slot_name)
    assert set(preflight["faces_by_material"]).issubset(assigned), f"Collapsed material slots: {name}"
    # Set the array once to avoid rebuilding the mesh for every slot.
    mesh.set_editor_property("static_materials", slots)
    bounds = mesh.get_bounding_box()
    dims = sorted([bounds.max.x-bounds.min.x, bounds.max.y-bounds.min.y, bounds.max.z-bounds.min.z])
    assert all(abs(a-b) < max(1.0, b*.03) for a,b in zip(dims, sorted(preflight["size_cm"]))), (name,dims,preflight["size_cm"])
    convex = mesh_editor.get_convex_collision_count(mesh)
    assert convex >= preflight["ucx_count"] > 0, (name, convex, preflight["ucx_count"])
    mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    points = ({k:v["location_m"] for k,v in info["points"].items()} if lods else
              {k.removeprefix("SOCKET_"):v for k,v in info["sockets"].items()})
    for socket_name, pos in points.items():
        socket = mesh.find_socket(socket_name)
        if not socket:
            # Some delivered empties lose their parent on FBX LOD reimport.
            # Recreate the explicit authored attachment point from the delivery report.
            socket = ue.new_object(ue.StaticMeshSocket, outer=mesh)
            socket.set_editor_property("socket_name", socket_name)
            mesh.add_socket(socket)
        socket.set_editor_property("relative_scale", ue.Vector(1,1,1))
        socket.set_editor_property("relative_location", ue.Vector(pos[0]*100,-pos[1]*100,pos[2]*100))
    save(mesh)
    RESULT["meshes"][name] = {"path": path, "size_cm": dims, "material_slots": assigned,
                              "convex_count": convex, "lods": mesh_editor.get_lod_count(mesh)}


def main():
    check_lock()
    manifest = json.loads((STAGE / "source-manifest.json").read_text(encoding="utf-8"))
    for rel, info in manifest["files"].items():
        assert hashlib.sha256((STAGE / rel).read_bytes()).hexdigest() == info["sha256"], rel
        source = Path(info["source"])
        assert source.stat().st_size == info["bytes"] and source.stat().st_mtime_ns == info["mtime_ns"], f"OneDrive revision changed: {rel}"
    temple_recipes = json.loads((STAGE / "temple-material-recipes.json").read_text(encoding="utf-8"))
    for recipe in temple_recipes.values():
        recipe["normal_gl"] = True
    temple_recipes["MAT-Temple_ShowcaseGround"] = dict(temple_recipes["MAT-Temple_CoralGravel"])
    temple_recipes["MAT-Temple_ShowcaseGround"]["tile"] /= 32.0
    temple_recipes.update({"M_Marae_Flame": {"kind": "flame", "color": "FF950D", "intensity": 3},
                           "M_Marae_FlameCore": {"kind": "flame", "color": "FFE1A0", "intensity": 8}})
    for dest, recipes, objects, use_lods in ((TEMPLE, temple_recipes, TR["modules"], False),
                                            (RELICS, artifact_recipes(), ART, True)):
        mats = {k:make_material(k,v,dest) for k,v in recipes.items()}
        for name, info in objects.items():
            import_mesh("SM_" + name if use_lods else name, dest, mats, info, use_lods)
    RESULT["source_staged_at"] = manifest["staged_at"]
    RESULT["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "import-result.json").write_text(json.dumps(RESULT, indent=2), encoding="utf-8")
    ue.log("T179_IMPORT_SUCCESS")


if __name__ == "__main__":
    # Interchange converts auto_generate_collision=False into Collision=None,
    # which also discards the UCX meshes. The authored kit requires legacy FBX.
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        main()
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
