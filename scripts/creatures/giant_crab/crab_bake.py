"""
Matériaux et cuisson PBR du crabe géant v2 (T-260).

Trois emplacements (crab_kit.MAT_SLOTS) : MI_GiantCrab_Chitin (carapace, pinces, tête, abdomen, voile ; instance Unreal
MI_GiantCrab_Body), MI_GiantCrab_Legs (8 pattes), MI_GiantCrab_Eye (yeux composés, émissifs ; MI_GiantCrab_Eyes). Chacun
a son atlas UV (crab_kit.atlas_layout) et ses textures T_GiantCrab_Body_*, T_GiantCrab_Legs_*, T_GiantCrab_Eyes_*.

1. materials() crée les trois matériaux en version « cuisson » : réseau procédural (bruits et Voronoi en coordonnées
   objet = position de repos, attributs de sommet CrabBase / CrabMask / CrabMat / EyeVeil) qui donne la couleur, la
   rugosité, le métal, l'occlusion et le relief (granulation, pores, rayures, cicatrices, plis des membranes).
2. bake() cuit avec Cycles (CPU) : BaseColor (sRGB), ORM (R occlusion, G rugosité, B métal ; linéaire) et Normal
   (espace tangent, vert inversé = convention DirectX d'Unreal), 2048 px pour Body et Legs, 512 px pour Eyes, plus le
   masque d'émission des yeux (T_GiantCrab_Eyes_E). Marge de 8 px.
3. use_baked() remplace le réseau par les textures cuites (aperçu fidèle à Unreal pour les rendus de contrôle).
Les nœuds sont créés par type (crab_nodes.NodeBuilder).
"""
import os
import time

import bpy

import crab_kit as CK
from crab_nodes import NodeBuilder, lin

TEX = {
    0: "T_GiantCrab_Body",
    1: "T_GiantCrab_Legs",
    2: "T_GiantCrab_Eyes",
}


def _clear(m):
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    return NodeBuilder(nt)


def _chitin_network(nb):
    """Retourne dict(color, rough, metal, ao, height) : sockets du réseau de la chitine (Body et Legs)."""
    obj = nb.texcoord("Object")
    base, _ = nb.attr_color("CrabBase")
    mask, rnd = nb.attr_color("CrabMask")
    cav, wear, scar = nb.sep(mask)
    matc, gloss = nb.attr_color("CrabMat")
    memb, encr, metal = nb.sep(matc)
    veil = nb.attr_fac("EyeVeil")
    # détail
    mott = nb.noise(obj, 3.0, 4, 0.6)
    fine = nb.noise(obj, 26.0, 4, 0.55)
    cells = nb.voronoi(obj, 34.0, 1.0, "F1", "Distance")
    cellc = nb.voronoi(obj, 34.0, 1.0, "F1", "Color")
    gran = nb.voronoi(obj, 72.0, 1.0, "F1", "Distance")
    pits = nb.voronoi(obj, 120.0, 1.0, "F1", "Distance")
    grain = nb.maprange(gran, 0.0, 0.48, 1.0, 0.0)                    # granules en dôme
    pore = nb.maprange(pits, 0.0, 0.10, 1.0, 0.0)                     # petits pores
    scr = nb.wave(nb.vscale(obj, 1.0), 26.0, 16.0, 3.0)
    scratch = nb.mul(nb.maprange(scr, 0.94, 1.0), nb.maprange(nb.noise(obj, 6.0, 2, 0.5), 0.5, 0.66))
    # cicatrices : masque de sommet cassé par du bruit (bords nets et irréguliers)
    sc_n = nb.noise(obj, 18.0, 4, 0.6)
    scar_s = nb.maprange(nb.add(scar, nb.mul(nb.math("SUBTRACT", sc_n, 0.5), 0.6)), 0.38, 0.62)
    # membranes : plis en anneaux
    rings = nb.wave(obj, 34.0, 9.0, 2.0, kind="RINGS")
    folds = nb.maprange(rings, 0.15, 0.9)
    # ---- couleur
    tone = nb.add(nb.maprange(mott, 0.3, 0.7, 0.82, 1.16), nb.mul(nb.math("SUBTRACT", nb.sep(cellc)[0], 0.5), 0.14))
    col = nb.mul_col(base, nb.gray(tone))
    col = nb.mix(nb.mul(grain, 0.10), col, nb.mul_col(base, nb.gray(1.5)))           # granules plus clairs
    col = nb.mix(nb.mul(pore, 0.35), col, "#0A0406")
    grime = nb.mul(nb.maprange(cav, 0.30, 0.90), nb.maprange(fine, 0.3, 0.7, 0.5, 1.0))
    col = nb.mix(nb.mul(grime, 0.62), col, "#0C0507")
    worn = nb.math("MAXIMUM", nb.mul(nb.maprange(wear, 0.6, 1.0), nb.maprange(fine, 0.45, 0.75)), scratch)
    col = nb.mix(nb.mul(worn, 0.30), col, "#6E5862")
    scar_col = nb.mul_col(nb.mix(nb.maprange(sc_n, 0.3, 0.7), "#4E3432", "#7A5752"), nb.gray(1.0))
    col = nb.mix(nb.mul(scar_s, 0.88), col, scar_col)
    col = nb.mix(nb.mul(nb.mul(memb, folds), 0.45), col, "#1E0204")
    col = nb.mix(nb.mul(encr, 0.5), col, nb.mul_col(base, nb.gray(nb.maprange(fine, 0.2, 0.8, 0.7, 1.2))))
    col = nb.mix(veil, col, base)
    # ---- rugosité, métal, occlusion
    rough = nb.add(0.40, nb.mul(nb.math("SUBTRACT", fine, 0.5), 0.20), nb.mul(nb.math("SUBTRACT", mott, 0.5), 0.16),
                   nb.mul(grime, 0.28), nb.mul(worn, 0.14), nb.mul(scar_s, 0.30))
    # dessus sec et mat (la carapace sèche au soleil), dessous et articulations humides
    geo = nb.node("ShaderNodeNewGeometry")
    _, _, nz = nb.sep(geo.outputs["Normal"])
    rough = nb.add(rough, nb.mul(nb.maprange(nz, 0.25, 0.9), 0.20))
    rough = nb.mixf(memb, rough, nb.add(0.22, nb.mul(folds, 0.10)))
    rough = nb.mixf(encr, rough, 0.88)
    rough = nb.mixf(gloss, rough, 0.18)
    rough = nb.mixf(veil, rough, 0.40)
    rough = nb.math("MINIMUM", nb.math("MAXIMUM", rough, 0.08), 0.95)
    met = nb.mul(metal, 0.8)
    ao = nb.mul(nb.ao(0.07, 16, True), nb.math("SUBTRACT", 1.0, nb.mul(cav, 0.35)))
    ao = nb.mixf(veil, ao, 1.0)
    # ---- relief
    h = nb.add(nb.mul(grain, 0.55), nb.mul(fine, 0.30), nb.mul(pore, -0.45), nb.mul(scratch, -0.5),
               nb.mul(nb.maprange(cells, 0.0, 0.12, 0.0, 1.0), 0.12))
    h = nb.add(h, nb.mul(scar_s, nb.mul(nb.math("SUBTRACT", sc_n, 0.8), 1.2)))
    h = nb.mixf(memb, h, nb.mul(folds, 1.4))
    h = nb.mixf(encr, h, nb.add(nb.voronoi(obj, 150.0, 1.0), nb.mul(fine, 0.5)))
    h = nb.mixf(veil, h, 0.0)
    return dict(color=col, rough=rough, metal=met, ao=ao, height=h, bump_strength=0.75, bump_distance=0.003)


def _eye_network(nb):
    obj = nb.texcoord("Object")
    base, _ = nb.attr_color("CrabBase")
    matc, _ = nb.attr_color("CrabMat")
    pupil, rim, _ = nb.sep(matc)
    fac = nb.voronoi(obj, 330.0, 0.85, "F1", "Distance")
    facet = nb.maprange(fac, 0.0, 0.5, 1.0, 0.0)
    col = nb.mul_col(base, nb.gray(nb.maprange(facet, 0.0, 1.0, 0.55, 1.10)))
    col = nb.mix(nb.maprange(fac, 0.38, 0.55), col, "#1A0802")
    col = nb.mix(pupil, col, "#0C0503")
    emit = nb.mul(nb.mul(nb.math("SUBTRACT", 1.0, pupil), nb.math("SUBTRACT", 1.0, nb.mul(rim, 0.85))),
                  nb.maprange(facet, 0.0, 1.0, 0.35, 1.0))
    return dict(color=col, rough=nb.add(0.05, nb.mul(rim, 0.25)), metal=0.0, ao=1.0, height=nb.mul(facet, 0.6),
                emit=emit, bump_strength=0.35, bump_distance=0.0015)


def _wire(m, kind):
    """(Re)construit le réseau procédural et une sortie configurable : ('bsdf' | 'emit_color' | 'emit_orm' | 'emit_e')."""
    nb = _clear(m)
    net = _eye_network(nb) if kind == "eye" else _chitin_network(nb)
    bsdf = nb.node("ShaderNodeBsdfPrincipled")
    nb._set(bsdf.inputs["Base Color"], net["color"])
    nb._set(bsdf.inputs["Roughness"], net["rough"])
    nb._set(bsdf.inputs["Metallic"], net["metal"])
    nrm = nb.bump(net["height"], net["bump_strength"], net["bump_distance"])
    nb.link(nrm, bsdf.inputs["Normal"])
    out = nb.node("ShaderNodeOutputMaterial")
    out.target = "ALL"
    emis = nb.node("ShaderNodeEmission")
    emis.inputs["Strength"].default_value = 1.0
    orm = nb.combine(net["ao"], net["rough"], net["metal"])
    m["_net"] = kind
    return dict(nb=nb, bsdf=bsdf, out=out, emis=emis, color=net["color"], orm=orm, emit=net.get("emit"))


def set_pass(m, wired, which):
    nb = wired["nb"]
    nt = m.node_tree
    for l in list(wired["out"].inputs["Surface"].links):
        nt.links.remove(l)
    for l in list(wired["emis"].inputs["Color"].links):
        nt.links.remove(l)
    if which == "N":
        nb.link(wired["bsdf"].outputs["BSDF"], wired["out"].inputs["Surface"])
        return
    src = {"BC": wired["color"], "ORM": wired["orm"], "E": wired["emit"]}[which]
    if src is None:
        src = nb.gray(0.0)
    if which == "E":
        src = nb.gray(src)
    nb.link(src, wired["emis"].inputs["Color"])
    nb.link(wired["emis"].outputs["Emission"], wired["out"].inputs["Surface"])


def materials():
    """Les trois matériaux du crabe (noms = emplacements Unreal), réseau procédural de cuisson."""
    out = []
    for slot, name in enumerate(CK.MAT_SLOTS):
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        _wire(m, "eye" if slot == CK.SLOT_EYE else "chitin")
        m.diffuse_color = (0.6, 0.35, 0.05, 1) if slot == CK.SLOT_EYE else (0.04, 0.012, 0.02, 1)
        m["ue_slot"] = name
        out.append(m)
    return out


def _set_engine_cycles(scene, samples):
    try:
        scene.render.engine = "CYCLES"
    except TypeError as ex:
        raise RuntimeError(f"Cycles indisponible : {ex}")
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    try:
        scene.cycles.use_adaptive_sampling = False
    except AttributeError:
        pass
    scene.render.threads_mode = "AUTO"


def bake(ob, out_dir, px=None, eye_px=None, samples=4, log=print):
    """Cuisson des trois atlas de l'objet ob (au repos) ; écrit les PNG dans out_dir ; retourne {nom: chemin}."""
    px = px or CK.V2["atlas_px"]
    eye_px = eye_px or CK.V2["eye_px"]
    scene = bpy.context.scene
    keep_engine = scene.render.engine
    _set_engine_cycles(scene, samples)
    bk = scene.render.bake
    bk.margin = 8
    try:
        bk.margin_type = "EXTEND"
    except (TypeError, AttributeError):
        pass
    bk.use_clear = True
    bk.normal_space = "TANGENT"
    bk.normal_r, bk.normal_g, bk.normal_b = "POS_X", "NEG_Y", "POS_Z"     # DirectX (Unreal)
    vl = bpy.context.view_layer
    for o in vl.objects:
        o.select_set(False)
    ob.hide_set(False)
    ob.hide_render = False
    ob.select_set(True)
    vl.objects.active = ob
    mats = [s.material for s in ob.material_slots]
    wired = [_wire(m, "eye" if i == CK.SLOT_EYE else "chitin") for i, m in enumerate(mats)]
    os.makedirs(out_dir, exist_ok=True)
    files = {}
    passes = [("BC", "EMIT", "sRGB"), ("ORM", "EMIT", "Non-Color"), ("N", "NORMAL", "Non-Color"), ("E", "EMIT", "Non-Color")]
    for tag, btype, cs in passes:
        imgs = []
        for i, m in enumerate(mats):
            if tag == "E" and i != CK.SLOT_EYE:
                imgs.append(None)
                continue
            size = eye_px if i == CK.SLOT_EYE else px
            name = f"{TEX[i]}_{tag}"
            img = bpy.data.images.get(name)
            if img is not None:
                bpy.data.images.remove(img)
            img = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False)
            img.colorspace_settings.name = cs
            imgs.append(img)
        # chaque matériau cuit dans son image (nœud image actif) ; les autres dans une image poubelle
        dummy = bpy.data.images.get("_bake_dummy") or bpy.data.images.new("_bake_dummy", 64, 64)
        for i, m in enumerate(mats):
            set_pass(m, wired[i], tag if (tag != "E" or i == CK.SLOT_EYE) else "BC")
            nt = m.node_tree
            for n in [n for n in nt.nodes if n.type == "TEX_IMAGE" and n.get("bake_target")]:
                nt.nodes.remove(n)
            node = nt.nodes.new("ShaderNodeTexImage")
            node["bake_target"] = True
            node.image = imgs[i] if imgs[i] is not None else dummy
            for n in nt.nodes:
                n.select = False
            node.select = True
            nt.nodes.active = node
        t0 = time.time()
        bpy.ops.object.bake(type=btype, margin=8, use_clear=True)
        log(f"[bake] {tag} : {time.time() - t0:.1f} s")
        for i, img in enumerate(imgs):
            if img is None:
                continue
            path = os.path.join(out_dir, img.name + ".png")
            img.filepath_raw = path
            img.file_format = "PNG"
            img.save()
            files[img.name] = path
    for i, m in enumerate(mats):
        set_pass(m, wired[i], "N")
    try:
        scene.render.engine = keep_engine
    except TypeError:
        pass
    return files


def use_baked(mats, tex_dir, eye_emit=(1.0, 0.45, 0.06), eye_strength=2.5):
    """Matériaux d'aperçu à partir des textures cuites (comme M_Character_Baked / M_GiantCrab_Eyes dans Unreal)."""
    for i, m in enumerate(mats):
        nb = _clear(m)
        nt = m.node_tree
        uvn = nb.node("ShaderNodeUVMap")
        uvn.uv_map = "UVMap"

        def img(tag, cs):
            path = os.path.join(tex_dir, f"{TEX[i]}_{tag}.png")
            im = bpy.data.images.load(path, check_existing=False)
            im.colorspace_settings.name = cs
            n = nb.node("ShaderNodeTexImage", interpolation="Linear")
            n.image = im
            nb.link(uvn.outputs["UV"], n.inputs["Vector"])
            return n
        bc = img("BC", "sRGB")
        orm = img("ORM", "Non-Color")
        nm = img("N", "Non-Color")
        bsdf = nb.node("ShaderNodeBsdfPrincipled")
        ao, rough, metal = nb.sep(orm.outputs["Color"])
        nb.link(nb.mul_col(bc.outputs["Color"], nb.combine(ao, ao, ao)), bsdf.inputs["Base Color"])
        nb.link(rough, bsdf.inputs["Roughness"])
        nb.link(metal, bsdf.inputs["Metallic"])
        r, g, b = nb.sep(nm.outputs["Color"])
        ogl = nb.combine(r, nb.math("SUBTRACT", 1.0, g), b)                 # DirectX -> OpenGL pour Blender
        nmap = nb.node("ShaderNodeNormalMap", space="TANGENT", uv_map="UVMap")
        nb.link(ogl, nmap.inputs["Color"])
        nb.link(nmap.outputs["Normal"], bsdf.inputs["Normal"])
        if i == CK.SLOT_EYE:
            e = img("E", "Non-Color")
            em = nb.mul_col(nb.gray(e.outputs["Color"]), (*eye_emit, 1.0))
            nb.link(em, bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = eye_strength
            bsdf.inputs["Specular IOR Level"].default_value = 0.8
        out = nb.node("ShaderNodeOutputMaterial")
        nb.link(bsdf.outputs["BSDF"], out.inputs["Surface"])
        m["_net"] = "baked"
    return mats


def vertex_preview(mats):
    """Aperçu rapide sans cuisson : couleur de sommet CrabBase, rugosité moyenne."""
    for i, m in enumerate(mats):
        nb = _clear(m)
        base, _ = nb.attr_color("CrabBase")
        bsdf = nb.node("ShaderNodeBsdfPrincipled")
        nb.link(base, bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.32
        if i == CK.SLOT_EYE:
            nb.link(base, bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = 1.5
        out = nb.node("ShaderNodeOutputMaterial")
        nb.link(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mats
