"""
materials — matériaux PARTAGÉS du galion (textures CC0 de factory/assets/textures/ship, repli sur art/textures,
matériaux procéduraux : dorure, fer, verre, lanterne, toile, pavillon). Voir DESIGN.md « materials ».

Usage dans un module :
    from galleon import materials as GM
    M = GM.mats()                      # dict clé -> bpy.types.Material (créés une seule fois, M_Galleon_*)
    b = GM.Batch("Galleon_Hull_Planks")   # = camp_kit.Batch (UV en mètres, 1 unité UV = 1 m)
    b.box(c, size, GM.tone("hull", rng), long_u=True)   # planche avec un ton aléatoire (variante teintée)
    GM.plank(b, c, size, "hull", rng)                   # idem, raccourci (chanfrein, long_u, jitter)

Conventions UV (1 unité = 1 m, Batch.box / Batch.tube / Batch.add_bm) :
  * matériaux « planches / bois » (grain == "u") : le fil du bois court le long de U -> Batch.box(..., long_u=True).
  * matériaux « espars / cordages » (grain == "v") : le fil court le long de V -> Batch.tube (v = longueur).
  * toile (canvas) : UV en mètres, u en travers de la voile (laizes tous les 0,41 m), v vers le bas.
  * pavillon (flag) : UV 0..1 sur le rectangle du pavillon (croix de Saint-Georges procédurale).
Le tuilage (`tile`, en mètres par répétition) est appliqué par le noeud Mapping du matériau : ne PAS mettre de
Mapping dans les modules, ne pas diviser les UV.

Tons par planche : `tone(key, rng)` renvoie une des 5 variantes teintées du matériau (M_Galleon_<Nom>_t0..t4),
`variants(key)` la liste. Les variantes sont de vrais matériaux (index de matériau conservé par join_parts et
l'export FBX) : aucun attribut de sommet requis (un attribut couleur manquant rend NOIR dans Blender 5.x).
"""
import json
import math
import os
import random

import bpy

from camp_kit import Batch  # noqa: F401  (réexporté : un seul Batch pour tout le paquet)

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SHIP_TEX = os.path.join(ROOT, "factory", "assets", "textures", "ship")
ART_TEX = os.path.join(ROOT, "art", "textures")
ART_SUBDIRS = ("characters", os.path.join("props", "minka"), "props", "ground", "")

PREFIX = "M_Galleon_"
TONES = (0.80, 0.90, 1.0, 1.10, 1.20)          # multiplicateurs des variantes de ton (t0..t4)
TEX_INFO = {}                                   # nom de matériau -> {"asset", "folder", "maps", "missing", "kind"}

# Palette (sRGB hex, issue des planches de référence) — pour les teintes, les matériaux plats et la doc.
PALETTE = {
    "tar": "2F2B28", "tar_joint": "1C1713", "hull": "90633F", "hull_light": "BC8D65", "hull_dark": "785133",
    "joint": "2E251F", "blue": "3C4C5E", "blue_light": "50535B", "blue_dark": "202C39", "navy": "2F3D5C",
    "gold": "BB874F", "gold_bright": "C9A054", "gold_shadow": "8D6237", "rail": "DFB683", "deck": "D2A575",
    "deck_mid": "B2885E", "deck_dark": "845F3E", "deck_joint": "57483D", "sail": "D4C1AD", "sail_light": "EFE1CF",
    "sail_dark": "B8AB9E", "spar": "8B6B4A", "spar_dark": "5A4634", "rope_tar": "2E2A28", "rope": "977752",
    "rope_light": "C9A96E", "iron": "3A3C41", "iron_dark": "1A1512", "iron_rust": "423A35", "carriage": "B28D69",
    "carriage_dark": "553A24", "brass": "AB8956", "brass_dark": "664B33", "lamp": "EAD1AB", "flame": "F9E4C2",
    "glass": "3D4652", "glass_hi": "D1C9BB", "flag_white": "E9DACD", "flag_red": "A95B4E", "cabin_wall": "90643B",
    "cabin_wall_dark": "603F26", "cabin_floor": "624E3B", "rug": "32333A", "rug_border": "685749",
    "linen": "E1CFBE", "velvet": "4A566F", "curtain": "222D41", "paper": "EFD4AD", "door": "604029",
    "ochre": "C8A070",
}


def srgb(hexa, alpha=1.0):
    """'RRGGBB' sRGB -> (r, g, b, a) linéaire."""
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c) + (alpha,)


# ----------------------------------------------------------------------------------------------------------------
# Recettes : clé -> description. kind = "pbr" (textures) | "flat" | "gold" | "glass" | "lamp" | "canvas" | "flag"
#   grain : "u" (planches : fil le long de U, pour Batch.box long_u=True) | "v" (espars / cordes : Batch.tube)
#   rot   : True si la texture a son fil/planches le long de V dans l'image et doit être tournée pour grain "u"
#   tones : variantes de ton disponibles (tone(key, rng))

RECIPES = {
    # coque
    # Albédos cibles (mesurés sur le banc par scratch measure.py : env. blanc uniforme, Cycles, pas de spéculaire) :
    #   tar ≈ #2F2B28, hull ≈ #90633F, blue ≈ #3C4C5E, wale plus noir que tar (≈ #2A2622).
    "tar": dict(kind="pbr", asset="dark_wooden_planks", tile=1.6, tint=(0.40, 0.37, 0.34), sat=0.55, rough_add=0.10,
                grain="u", rot=False, tones=True, tone_mul=(0.85, 0.93, 1.0, 1.35, 1.6),
                role="fond goudronné (œuvres vives, de la quille à z≈0,15) ; t3/t4 nettement plus clairs = bande de flottaison"),
    "hull": dict(kind="pbr", asset="brown_planks_03", tile=2.4, tint=(2.3, 1.3, 0.70), sat=1.2, rough_add=0.0, ao=0.4,
                 grain="u", rot=True, tones=True, role="bordé chêne brun (ceinture des sabords, z 0,15..1,45)"),
    "blue": dict(kind="pbr", asset="blue_painted_planks", tile=1.6, tint=(0.48, 0.58, 0.92), sat=0.75, rough_add=0.10,
                 mix=("384859", 0.50), grain="u", rot=False, tones=True,
                 role="œuvres mortes bleu nuit (pavois, châteaux, face intérieure du pavois) ; écailles atténuées par mix navy"),
    "wale": dict(kind="pbr", asset="dark_wooden_planks", tile=1.2, tint=(0.32, 0.29, 0.27), sat=0.5, rough_add=0.15,
                 grain="u", rot=False, tones=True, role="préceintes goudronnées, quille, étrave, étambot (plus noires que tar)"),
    # ponts et bois
    "deck": dict(kind="pbr", asset="plank_flooring_03", tile=2.2, tint=(1.35, 1.18, 0.95), sat=0.9, rough_add=0.05,
                 grain="u", rot=False, tones=True, role="lattes de pont (embelle, cabine : chêne miel lessivé)"),
    "deck2": dict(kind="pbr", asset="wood_planks_grey", tile=2.0, tint=(1.55, 1.40, 1.15), sat=0.8, rough_add=0.05,
                  grain="u", rot=True, tones=True, role="lattes de pont des châteaux (gaillards, dunette, hunes : plus grises)"),
    "oak": dict(kind="pbr", asset="fine_grained_wood", tile=0.9, tint=(4.0, 3.5, 3.2), sat=0.9, rough_add=-0.1,
                mix=("8E6240", 0.35), grain="u", rot=False, tones=True, role="chêne moyen ciré (≈ #855D3C) : cloisons, bittes, hiloires, meubles, montants"),
    "oak_light": dict(kind="pbr", asset="rough_wood", tile=0.7, tint=(1.55, 1.30, 0.98), sat=0.8, rough_add=0.0,
                      grain="u", rot=True, tones=True, role="chêne clair : lisses de pavois, balustres, affûts, caillebotis, roue"),
    "oak_dark": dict(kind="pbr", asset="fine_grained_wood", tile=0.8, tint=(1.05, 0.92, 0.80), sat=0.9, rough_add=0.05,
                     grain="u", rot=False, tones=True, role="chêne sombre : hunes, chouquets, potence de cloche, coffres, lit"),
    "spar": dict(kind="pbr", asset="rough_wood", tile=1.4, tint=(1.30, 1.02, 0.76), sat=0.9, rough_add=0.05,
                 grain="v", rot=False, tones=False, role="mâts, vergues, beaupré, espars (Batch.tube : fil le long de V)"),
    "cedar": dict(kind="pbr", asset="japanese_cedar_planks", tile=1.2, tint=(0.37, 0.29, 0.21), sat=0.7, rough_add=0.2,
                  grain="u", rot=True, tones=True, role="portes, mantelets de sabord, panneaux de porte (bois sombre rougeâtre ≈ #604029)"),
    "cabin_wall": dict(kind="pbr", asset="weathered_brown_planks", tile=2.6, tint=(2.3, 1.7, 1.1), sat=0.95, rough_add=0.05,
                       grain="u", rot=False, tones=True, role="lambris de la cabine (chêne à panneaux ≈ #90643B)"),
    "cabin_floor": dict(kind="pbr", asset="hinoki_planks", tile=2.0, tint=(1.05, 0.90, 0.70), sat=1.0, rough_add=0.05,
                        grain="u", rot=False, tones=True, role="plancher de la cabine (14 lattes par tuile : pas 0,143 m)"),
    "barrel": dict(kind="pbr", asset="wooden_barrel", tile=1.0, tint=(1.0, 0.95, 0.9), sat=1.0, rough_add=0.0,
                   grain="u", rot=False, tones=True, role="tonneaux (douves + cercles intégrés ; UV : u = tour, v = hauteur)"),
    # métaux
    "gold": dict(kind="gold", asset="Metal048C", tones=False, role="dorures : moulures, lion, rinceaux, cadres, médaillon, pommes"),
    "brass": dict(kind="pbr", asset="Metal048C", tile=0.5, tint=(0.95, 0.76, 0.45), sat=0.9, rough_add=0.12, metallic=1.0,
                  grain="u", rot=False, tones=False, role="laiton : cloche, moyeu de roue, montures de lanternes, compas"),
    "iron": dict(kind="pbr", asset="Metal046B", tile=0.8, tint=(0.75, 0.78, 0.86), sat=0.8, rough_add=0.08, metallic=0.9,
                 grain="u", rot=False, tones=False, role="fonte noire bleutée : tubes de canon, boulets, tourillons"),
    # rusty_metal_04 = acier peint gris clair + rouille : teinte sombre, saturation réduite et fondu vers le gris-noir
    # (#2B2623) pour obtenir un fer forgé mat ≈ #423A35 sans éclats clairs « peinture écaillée » ; normale conservée.
    "iron_rust": dict(kind="pbr", asset="rusty_metal_04", tile=0.6, tint=(0.95, 0.90, 0.82), sat=0.5, rough_add=0.2,
                      metallic=0.3, mix=("2B2623", 0.60), grain="u", rot=False, tones=False,
                      role="fer forgé gris-noir mat : ancres, ferrures, cercles, cadènes, pentures"),
    "glass": dict(kind="glass", tones=False, role="vitrage sombre réfléchissant (fenêtres de poupe, cabine, lanternes éteintes)"),
    "lamp": dict(kind="lamp", tones=False, role="verre de lanterne émissif ambre (lumières UE sur les sockets lantern_*)"),
    # cordages, toile, drapeau
    "rope": dict(kind="pbr", asset="Rope001", tile=0.12, tint=(1.0, 0.90, 0.68), sat=0.9, rough_add=0.1,
                 grain="v", rot=False, tones=False, role="cordage de chanvre clair : manœuvres, glènes, brague, liures"),
    "rope_tar": dict(kind="pbr", asset="Rope001", tile=0.12, tint=(0.40, 0.34, 0.28), sat=0.6, rough_add=0.15,
                     grain="v", rot=False, tones=False, role="cordage goudronné : haubans, étais, gammoning (gros diamètres)"),
    "rigging": dict(kind="flat", color="2E2A28", rough=0.85, tones=False, role="lignes fines (enfléchures, bras, drisses) : pas d'UV requis"),
    "canvas": dict(kind="canvas", asset="terlenka", tones=False, role="toile de voile écrue à laizes (UV en mètres)"),
    "flag": dict(kind="flag", tones=False, role="pavillon de Saint-Georges (UV 0..1)"),
    # divers
    "pitch": dict(kind="flat", color="1C1713", rough=0.55, tones=False, role="calfatage (joints de bordé et de pont) ; intérieur des sabords"),
    "dark_recess": dict(kind="flat", color="0E0D0C", rough=0.95, tones=False, role="fond noir (faux entrepont derrière les sabords)"),
    "navy": dict(kind="flat", color="2F3D5C", rough=0.55, tones=False, role="bleu nuit peint uni (piédestal de barre, petits panneaux)"),
    "ochre": dict(kind="pbr", asset="rough_wood", tile=0.7, tint=(2.6, 1.8, 1.15), sat=0.7, rough_add=0.1, ao=0.3,
                  grain="u", rot=True, tones=True, role="bois clair ocre (≈ #C8A070 éclairé, albédo ≈ #A88562) : cadres de sabords, linteaux, filets"),
    "leather": dict(kind="pbr", asset="brown_leather", tile=0.6, tint=(1.0, 0.95, 0.9), sat=1.0, rough_add=0.0,
                    grain="u", rot=False, tones=False, role="cuir : coffres, reliures, sangles"),
    # velour_velvet est rosé : saturation 0 (texture = luminance seule) puis teinte bleu nuit -> ≈ #2F3D5C
    "velvet": dict(kind="pbr", asset="velour_velvet", tile=0.8, tint=(0.085, 0.135, 0.31), sat=0.0, rough_add=0.1,
                   grain="u", rot=False, tones=False, role="velours bleu nuit (≈ #2F3D5C) : tapis, couverture, assise de chaise"),
    "curtain": dict(kind="pbr", asset="velour_velvet", tile=0.8, tint=(0.05, 0.08, 0.19), sat=0.0, rough_add=0.1,
                    grain="u", rot=False, tones=False, role="velours plus sombre (≈ #222D41) : rideaux, doublure du coffret"),
    "linen": dict(kind="flat", color="E1CFBE", rough=0.9, tones=False, role="drap, oreiller"),
    "paper": dict(kind="flat", color="EFD4AD", rough=0.8, tones=False, role="carte marine (parchemin)"),
    "hessian": dict(kind="pbr", asset="hessian_230", tile=0.5, tint=(1.0, 0.95, 0.85), sat=1.0, rough_add=0.0,
                    grain="u", rot=False, tones=False, role="sacs, bâches, hamacs (jute)"),
}


def _name(key, tone_idx=None):
    n = PREFIX + "".join(w.capitalize() for w in key.split("_"))
    return n if tone_idx is None else f"{n}_t{tone_idx}"


# ----------------------------------------------------------------------------------------------------------------
# Textures

def find_folder(asset):
    """Dossier absolu de l'asset : factory/assets/textures/ship/<asset>, sinon art/textures/<sous-dossier>/<asset>."""
    p = os.path.join(SHIP_TEX, asset)
    if os.path.isdir(p):
        return p
    for sub in ART_SUBDIRS:
        p = os.path.join(ART_TEX, sub, asset) if sub else os.path.join(ART_TEX, asset)
        if os.path.isdir(p):
            return p
    return None


MAP_KEYS = {"diff": ("_diff", "_color", "basecolor", "albedo"), "rough": ("_rough", "roughness"),
            "normal": ("nor_gl", "normalgl"), "ao": ("_ao", "ambientocclusion"), "metal": ("_metal", "metalness")}


def find_map(folder, kind):
    if not folder or not os.path.isdir(folder):
        return None
    for f in sorted(os.listdir(folder), reverse=True):      # 2k avant 1k
        low = f.lower()
        if low.endswith((".jpg", ".png")) and any(k in low for k in MAP_KEYS[kind]) and "_dx" not in low:
            return os.path.join(folder, f)
    return None


def maps_for(asset):
    folder = find_folder(asset)
    return folder, {k: find_map(folder, k) for k in MAP_KEYS}


# ----------------------------------------------------------------------------------------------------------------
# Constructeurs

def _bsdf(mat):
    return next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")


def _set(bsdf, key, value):
    if key in bsdf.inputs:
        bsdf.inputs[key].default_value = value


def flat(name, color_hex, rough=0.6, metal=0.0, emit=0.0):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = _bsdf(m)
    col = srgb(color_hex)
    b.inputs["Base Color"].default_value = col
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit:
        _set(b, "Emission Color", col)
        _set(b, "Emission Strength", emit)
    m.diffuse_color = col
    return m


def pbr(name, asset, tile=1.0, tint=(1, 1, 1), sat=1.0, rough_add=0.0, normal_strength=1.0, metallic=None,
        rot=False, ao_strength=0.7, fallback_hex="8A6A4A", mix=None):
    """Matériau PBR texturé : diff (teinté, saturation), rough (+rough_add), normale GL, AO (multiplié dans la
    couleur), metal (carte ou constante). `rot` tourne les UV de 90° (fil de la texture ramené le long de U).
    `mix` = ("RRGGBB", fac) : après le Multiply, fondu vers une couleur plate (atténue écailles / stries trop
    marquées de la texture, la normale est conservée).
    Si l'asset manque : couleur plate `fallback_hex` (jamais de magenta), signalé dans TEX_INFO."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    folder, maps = maps_for(asset)
    info = {"asset": asset, "folder": folder, "maps": {k: v for k, v in maps.items() if v}, "missing": [], "kind": "pbr"}
    TEX_INFO[name] = info
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = _bsdf(m)
    m["galleon_asset"] = asset
    m["galleon_tile_m"] = tile
    m["galleon_tint"] = list(tint)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1 / tile, 1 / tile, 1 / tile)
    if rot:
        mp.inputs["Rotation"].default_value = (0, 0, math.pi / 2)
    nt.links.new(tc.outputs["UV"], mp.inputs["Vector"])

    def img(path, color):
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = bpy.data.images.load(path, check_existing=True)
        n.image.colorspace_settings.name = "sRGB" if color else "Non-Color"
        n.interpolation = "Linear"
        nt.links.new(mp.outputs["Vector"], n.inputs["Vector"])
        return n

    if not folder:
        info["missing"].append("folder")
    if maps["diff"]:
        d = img(maps["diff"], True)
        hs = nt.nodes.new("ShaderNodeHueSaturation")
        hs.inputs["Saturation"].default_value = sat
        nt.links.new(d.outputs["Color"], hs.inputs["Color"])
        mul = nt.nodes.new("ShaderNodeMixRGB")
        mul.blend_type = "MULTIPLY"
        mul.inputs["Fac"].default_value = 1.0
        mul.inputs["Color2"].default_value = (*tint, 1)
        nt.links.new(hs.outputs["Color"], mul.inputs["Color1"])
        last = mul.outputs["Color"]
        if mix:
            mx = nt.nodes.new("ShaderNodeMixRGB")
            mx.inputs["Fac"].default_value = mix[1]
            mx.inputs["Color2"].default_value = srgb(mix[0])
            nt.links.new(last, mx.inputs["Color1"])
            last = mx.outputs["Color"]
        if maps["ao"] and ao_strength > 0:
            a = img(maps["ao"], False)
            am = nt.nodes.new("ShaderNodeMixRGB")
            am.blend_type = "MULTIPLY"
            am.inputs["Fac"].default_value = ao_strength
            nt.links.new(last, am.inputs["Color1"])
            nt.links.new(a.outputs["Color"], am.inputs["Color2"])
            last = am.outputs["Color"]
        nt.links.new(last, b.inputs["Base Color"])
        m.diffuse_color = (min(1, 0.55 * tint[0]), min(1, 0.45 * tint[1]), min(1, 0.35 * tint[2]), 1)
    else:
        info["missing"].append("diff")
        b.inputs["Base Color"].default_value = srgb(fallback_hex)
        m.diffuse_color = srgb(fallback_hex)
    if maps["rough"]:
        r = img(maps["rough"], False)
        add = nt.nodes.new("ShaderNodeMath")
        add.operation = "ADD"
        add.use_clamp = True
        add.inputs[1].default_value = rough_add
        nt.links.new(r.outputs["Color"], add.inputs[0])
        nt.links.new(add.outputs[0], b.inputs["Roughness"])
    else:
        info["missing"].append("rough")
        b.inputs["Roughness"].default_value = min(1.0, 0.6 + rough_add)
    if maps["normal"]:
        nn = img(maps["normal"], False)
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nm.inputs["Strength"].default_value = normal_strength
        nt.links.new(nn.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
    else:
        info["missing"].append("normal")
    if metallic is not None:
        if maps["metal"]:
            mt = img(maps["metal"], False)
            sc = nt.nodes.new("ShaderNodeMath")
            sc.operation = "MULTIPLY"
            sc.inputs[1].default_value = metallic
            nt.links.new(mt.outputs["Color"], sc.inputs[0])
            nt.links.new(sc.outputs[0], b.inputs["Metallic"])
        else:
            b.inputs["Metallic"].default_value = metallic
    return m


def gold_mat(name, asset="Metal048C", tone=1.0):
    """Dorure : métal (metallic 1), or brillant quasi uni (bruit très fin, scale 90, ramp 0,12-0,38 : seules de
    rares piqûres tirent vers l'or sombre), rugosité 0,18-0,32, léger Coat pour les reflets, micro-rayures par la
    normale de Metal048C si présente."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    folder, maps = maps_for(asset)
    TEX_INFO[name] = {"asset": asset, "folder": folder, "maps": {k: v for k, v in maps.items() if v and k == "normal"},
                      "missing": [] if maps["normal"] else ["normal"], "kind": "gold"}
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = _bsdf(m)
    b.inputs["Metallic"].default_value = 1.0
    _set(b, "Coat Weight", 0.3)
    _set(b, "Coat Roughness", 0.1)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 90.0
    noise.inputs["Detail"].default_value = 3.0
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    els = ramp.color_ramp.elements
    els[0].position, els[0].color = 0.12, srgb("A8783F")
    els[1].position, els[1].color = 0.38, tuple(min(1.0, c * tone) for c in srgb("F0C46A")[:3]) + (1.0,)
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["To Min"].default_value = 0.18
    mr.inputs["To Max"].default_value = 0.32
    nt.links.new(noise.outputs["Fac"], mr.inputs["Value"])
    nt.links.new(mr.outputs["Result"], b.inputs["Roughness"])
    if maps["normal"]:
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (2.0, 2.0, 2.0)
        nt.links.new(tc.outputs["UV"], mp.inputs["Vector"])
        nn = nt.nodes.new("ShaderNodeTexImage")
        nn.image = bpy.data.images.load(maps["normal"], check_existing=True)
        nn.image.colorspace_settings.name = "Non-Color"
        nt.links.new(mp.outputs["Vector"], nn.inputs["Vector"])
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nm.inputs["Strength"].default_value = 0.35
        nt.links.new(nn.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
    m.diffuse_color = srgb("C9A054")
    return m


def glass_mat(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = flat(name, "3D4652", rough=0.06, metal=0.35)
    b = _bsdf(m)
    _set(b, "Specular IOR Level", 1.0)
    _set(b, "Coat Weight", 0.6)
    _set(b, "Coat Roughness", 0.05)
    TEX_INFO[name] = {"asset": None, "folder": None, "maps": {}, "missing": [], "kind": "glass"}
    return m


def lamp_mat(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = flat(name, "F2C27A", rough=0.4, emit=4.0)
    _set(_bsdf(m), "Emission Color", srgb("FFB85C"))
    TEX_INFO[name] = {"asset": None, "folder": None, "maps": {}, "missing": [], "kind": "lamp"}
    return m


def canvas_mat(name, asset="terlenka"):
    """Toile écrue (terlenka) : laizes de 0,41 m (lignes de couture sombres) et variation de teinte par laize.
    UV en mètres (u en travers). Pas de pièces procédurales : les vraies pièces sont maillées (sails_parts.patches)."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = pbr(name, asset, tile=0.6, tint=(1.08, 1.02, 0.90), sat=0.8, rough_add=0.12, normal_strength=0.6,
            fallback_hex="D4C1AD")
    TEX_INFO[name]["kind"] = "canvas"
    nt = m.node_tree
    b = _bsdf(m)
    uv = nt.nodes.new("ShaderNodeUVMap")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uv.outputs["UV"], sep.inputs[0])
    # laizes : fract(u / 0,41) -> ligne de couture étroite
    div = nt.nodes.new("ShaderNodeMath")
    div.operation = "DIVIDE"
    div.inputs[1].default_value = 0.41
    nt.links.new(sep.outputs["X"], div.inputs[0])
    fr = nt.nodes.new("ShaderNodeMath")
    fr.operation = "FRACT"
    nt.links.new(div.outputs[0], fr.inputs[0])
    seam = nt.nodes.new("ShaderNodeMath")
    seam.operation = "LESS_THAN"
    seam.inputs[1].default_value = 0.05
    nt.links.new(fr.outputs[0], seam.inputs[0])
    # ton par laize (bruit sur floor(u/0,41))
    fl = nt.nodes.new("ShaderNodeMath")
    fl.operation = "FLOOR"
    nt.links.new(div.outputs[0], fl.inputs[0])
    comb = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(fl.outputs[0], comb.inputs["X"])
    n1 = nt.nodes.new("ShaderNodeTexNoise")
    n1.inputs["Scale"].default_value = 0.9
    nt.links.new(comb.outputs[0], n1.inputs["Vector"])
    tone = nt.nodes.new("ShaderNodeMapRange")
    tone.inputs["To Min"].default_value = 0.90
    tone.inputs["To Max"].default_value = 1.06
    nt.links.new(n1.outputs["Fac"], tone.inputs["Value"])
    base_in = b.inputs["Base Color"].links[0].from_socket if b.inputs["Base Color"].links else None
    m1 = nt.nodes.new("ShaderNodeMixRGB")
    m1.blend_type = "MULTIPLY"
    m1.inputs["Fac"].default_value = 1.0
    if base_in:
        nt.links.new(base_in, m1.inputs["Color1"])
    else:
        m1.inputs["Color1"].default_value = srgb("D4C1AD")
    tv = nt.nodes.new("ShaderNodeCombineColor")
    for k in ("Red", "Green", "Blue"):
        nt.links.new(tone.outputs["Result"], tv.inputs[k])
    nt.links.new(tv.outputs[0], m1.inputs["Color2"])
    m3 = nt.nodes.new("ShaderNodeMixRGB")
    m3.inputs["Color2"].default_value = srgb("8D7A68")
    nt.links.new(seam.outputs[0], m3.inputs["Fac"])
    nt.links.new(m1.outputs["Color"], m3.inputs["Color1"])
    nt.links.new(m3.outputs["Color"], b.inputs["Base Color"])
    _set(b, "Sheen Weight", 0.15)
    # toile épaisse : faible part translucide (0,11) pour que la face ombrée reste beige (pas olive) SANS laisser
    # voir les voiles / mâts situés derrière ; couleur translucide = toile x 0,85 (face arrière un peu plus sombre)
    tr = nt.nodes.new("ShaderNodeBsdfTranslucent")
    trc = nt.nodes.new("ShaderNodeMixRGB")
    trc.blend_type = "MULTIPLY"
    trc.inputs["Fac"].default_value = 1.0
    trc.inputs["Color2"].default_value = (0.85, 0.85, 0.85, 1)
    nt.links.new(m3.outputs["Color"], trc.inputs["Color1"])
    nt.links.new(trc.outputs["Color"], tr.inputs["Color"])
    mixs = nt.nodes.new("ShaderNodeMixShader")
    mixs.inputs[0].default_value = 0.11
    nt.links.new(b.outputs[0], mixs.inputs[1])
    nt.links.new(tr.outputs[0], mixs.inputs[2])
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    nt.links.new(mixs.outputs[0], out.inputs["Surface"])
    m.diffuse_color = srgb("D4C1AD")
    return m


def flag_mat(name):
    """Croix de Saint-Georges : rouge si |u-0,5| < 0,085 ou |v-0,5| < 0,13 (UV 0..1), toile légèrement bruitée."""
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = flat(name, "E9DACD", rough=0.85)
    TEX_INFO[name] = {"asset": None, "folder": None, "maps": {}, "missing": [], "kind": "flag"}
    nt = m.node_tree
    b = _bsdf(m)
    uv = nt.nodes.new("ShaderNodeUVMap")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uv.outputs["UV"], sep.inputs[0])
    masks = []
    for ax, half in (("X", 0.085), ("Y", 0.13)):
        s = nt.nodes.new("ShaderNodeMath")
        s.operation = "SUBTRACT"
        s.inputs[1].default_value = 0.5
        nt.links.new(sep.outputs[ax], s.inputs[0])
        a = nt.nodes.new("ShaderNodeMath")
        a.operation = "ABSOLUTE"
        nt.links.new(s.outputs[0], a.inputs[0])
        lt = nt.nodes.new("ShaderNodeMath")
        lt.operation = "LESS_THAN"
        lt.inputs[1].default_value = half
        nt.links.new(a.outputs[0], lt.inputs[0])
        masks.append(lt)
    mx = nt.nodes.new("ShaderNodeMath")
    mx.operation = "MAXIMUM"
    nt.links.new(masks[0].outputs[0], mx.inputs[0])
    nt.links.new(masks[1].outputs[0], mx.inputs[1])
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 40.0
    nt.links.new(uv.outputs["UV"], noise.inputs["Vector"])
    nr = nt.nodes.new("ShaderNodeMapRange")
    nr.inputs["To Min"].default_value = 0.85
    nr.inputs["To Max"].default_value = 1.05
    nt.links.new(noise.outputs["Fac"], nr.inputs["Value"])
    mix = nt.nodes.new("ShaderNodeMixRGB")
    mix.inputs["Color1"].default_value = srgb("E9DACD")
    mix.inputs["Color2"].default_value = srgb("A95B4E")
    nt.links.new(mx.outputs[0], mix.inputs["Fac"])
    mul = nt.nodes.new("ShaderNodeMixRGB")
    mul.blend_type = "MULTIPLY"
    mul.inputs["Fac"].default_value = 1.0
    nt.links.new(mix.outputs["Color"], mul.inputs["Color1"])
    cc = nt.nodes.new("ShaderNodeCombineColor")
    for k in ("Red", "Green", "Blue"):
        nt.links.new(nr.outputs["Result"], cc.inputs[k])
    nt.links.new(cc.outputs[0], mul.inputs["Color2"])
    nt.links.new(mul.outputs["Color"], b.inputs["Base Color"])
    # toile fine : part translucide (le motif se lit aussi à contre-jour / sur la face ombrée)
    tr = nt.nodes.new("ShaderNodeBsdfTranslucent")
    nt.links.new(mul.outputs["Color"], tr.inputs["Color"])
    mixs = nt.nodes.new("ShaderNodeMixShader")
    mixs.inputs[0].default_value = 0.35
    nt.links.new(b.outputs[0], mixs.inputs[1])
    nt.links.new(tr.outputs[0], mixs.inputs[2])
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    nt.links.new(mixs.outputs[0], out.inputs["Surface"])
    return m


def _build(key, tone_idx=None):
    r = RECIPES[key]
    name = _name(key, tone_idx)
    k = r.get("tone_mul", TONES)[tone_idx] if tone_idx is not None else 1.0
    kind = r["kind"]
    if kind == "pbr":
        tint = tuple(c * k for c in r["tint"])
        mix = r.get("mix")
        if mix and tone_idx is not None:        # la couleur de fondu suit le ton (sinon les variantes se confondent)
            mix = ("".join(f"{min(255, int(round(int(mix[0][i:i + 2], 16) * k))):02X}" for i in (0, 2, 4)), mix[1])
        m = pbr(name, r["asset"], tile=r["tile"], tint=tint, sat=r.get("sat", 1.0), rough_add=r.get("rough_add", 0.0),
                normal_strength=r.get("normal", 1.0), metallic=r.get("metallic"), rot=r.get("rot", False),
                ao_strength=r.get("ao", 0.7), fallback_hex=PALETTE.get(key, "8A6A4A"), mix=mix)
    elif kind == "flat":
        m = flat(name, r["color"], rough=r.get("rough", 0.6), metal=r.get("metal", 0.0), emit=r.get("emit", 0.0))
        TEX_INFO[name] = {"asset": None, "folder": None, "maps": {}, "missing": [], "kind": "flat"}
    elif kind == "gold":
        m = gold_mat(name, r.get("asset", "Metal048C"), tone=k)
    elif kind == "glass":
        m = glass_mat(name)
    elif kind == "lamp":
        m = lamp_mat(name)
    elif kind == "canvas":
        m = canvas_mat(name, r.get("asset", "terlenka"))
    elif kind == "flag":
        m = flag_mat(name)
    else:
        raise ValueError(kind)
    m["galleon_key"] = key
    m["galleon_grain"] = r.get("grain", "u")
    return m


_CACHE = {}


def mats():
    """Dict clé -> matériau de base (ton médian). Clés : voir RECIPES (tar, hull, blue, wale, deck, deck2, oak,
    oak_light, oak_dark, spar, cedar, cabin_wall, cabin_floor, barrel, gold, brass, iron, iron_rust, glass, lamp,
    rope, rope_tar, rigging, canvas, flag, pitch, dark_recess, navy, ochre, leather, velvet, curtain, linen, paper,
    hessian)."""
    out = {}
    for key in RECIPES:
        out[key] = _build(key)
    return out


def variants(key):
    """Les 5 variantes de ton (t0 sombre .. t4 clair) d'un matériau à tons ; [base] si la recette n'en a pas."""
    if not RECIPES[key].get("tones"):
        return [_build(key)]
    return [_build(key, i) for i in range(len(TONES))]


def tone(key, rng=None, bias=0.0):
    """Une variante de ton au hasard (rng : random.Random ; bias -1..+1 décale vers sombre / clair)."""
    vs = variants(key)
    if len(vs) == 1:
        return vs[0]
    rng = rng or random
    i = rng.choice((0, 1, 1, 2, 2, 2, 3, 3, 4))
    i = min(len(vs) - 1, max(0, int(round(i + bias * 2))))
    return vs[i]


def plank(batch, c, size, key, rng, bevel=0.006, jitter=0.0, long_u=True, R=None, uv_size=None, uv_offset=None):
    """Planche : boîte biseautée avec un ton aléatoire et un décalage UV aléatoire (casse la répétition)."""
    mat = tone(key, rng)
    n0 = len(batch.bm.faces)
    batch.box(c, size, mat, R=R, bevel=bevel, jitter=jitter, rng=rng if jitter else None, long_u=long_u, uv_size=uv_size)
    off = uv_offset if uv_offset is not None else (rng.uniform(0, 4.0), rng.uniform(0, 4.0))
    batch.bm.faces.ensure_lookup_table()
    for i in range(n0, len(batch.bm.faces)):
        for lp in batch.bm.faces[i].loops:
            lp[batch.uv].uv = (lp[batch.uv].uv[0] + off[0], lp[batch.uv].uv[1] + off[1])
    return mat


def check():
    """Vérifie les textures de tous les matériaux construits : renvoie {"ok": bool, "materials": {...}}."""
    report = {}
    ok = True
    for name, info in TEX_INFO.items():
        entry = dict(info)
        imgs = []
        m = bpy.data.materials.get(name)
        if m and m.use_nodes:
            for n in m.node_tree.nodes:
                if n.type == "TEX_IMAGE":
                    good = n.image is not None and n.image.size[0] > 0
                    imgs.append({"image": n.image.filepath if n.image else None, "loaded": good})
                    ok = ok and good
        entry["images"] = imgs
        if info["kind"] in ("pbr", "canvas") and ("folder" in info["missing"] or "diff" in info["missing"]):
            ok = False
        report[name] = entry
    return {"ok": ok, "materials": report}


# ----------------------------------------------------------------------------------------------------------------
# Banc d'essai : planche de dalles (1,2 x 1,2 m, UV en mètres) + sphères (métaux) + variantes de ton + étiquettes

def _label(text, loc, coll, size=0.14):
    cu = bpy.data.curves.new("TXT-" + text, type="FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    ob = bpy.data.objects.new("TXT-" + text, cu)
    ob.location = loc
    coll.objects.link(ob)
    return ob


def build_test(coll):
    import bmesh
    rng = random.Random(3)
    M = mats()
    keys = list(RECIPES)
    cols = 6
    pitch = 1.5
    b = Batch("Galleon_Materials_Sheet")
    labels = []
    for i, key in enumerate(keys):
        cx, cy = (i % cols) * pitch, -(i // cols) * pitch
        c = (cx, cy, 0.0)
        r = RECIPES[key]
        if r["kind"] in ("gold", "glass", "lamp") or r.get("metallic"):
            tmp = bmesh.new()
            bmesh.ops.create_uvsphere(tmp, u_segments=32, v_segments=16, radius=0.45)
            bmesh.ops.translate(tmp, verts=tmp.verts, vec=(cx, cy, 0.5))
            b.add_bm(tmp, M[key], smooth=True, uv_size=1.0)
            b.box((cx, cy, 0.03), (1.2, 1.2, 0.06), M[key], bevel=0.01, long_u=True)
        elif key in ("rope", "rope_tar", "spar", "rigging"):
            b.tube([(cx - 0.6, cy, 0.12), (cx + 0.6, cy, 0.12)], 0.12 if key != "spar" else 0.2, M[key], segs=16)
            b.box((cx, cy, 0.03), (1.2, 1.2, 0.06), M[key], bevel=0.0, long_u=True)
        elif key == "flag":
            b.box((cx, cy, 0.03), (1.2, 0.8, 0.02), M[key], bevel=0.0, uv_size=1.0)
            # UV 0..1 sur le rectangle : remappe la face du dessus
            b.bm.faces.ensure_lookup_table()
            for f in b.bm.faces[-6:]:
                if f.normal.z > 0.5:
                    for lp in f.loops:
                        lp[b.uv].uv = ((lp.vert.co.x - cx) / 1.2 + 0.5, (lp.vert.co.y - cy) / 0.8 + 0.5)
        else:
            b.box((cx, cy, 0.03), (1.2, 1.2, 0.06), M[key], bevel=0.01, long_u=True)
            if r.get("tones"):
                for k, vm in enumerate(variants(key)):
                    b.box((cx - 0.5 + k * 0.25, cy - 0.72, 0.03), (0.22, 0.2, 0.06), vm, bevel=0.0, long_u=True)
        labels.append(_label(key, (cx, cy + 0.68, 0.07), coll))
    sheet = b.finish(coll)
    # une vraie « planche » avec plank() (ton + décalage UV) pour contrôler le raccourci
    pb = Batch("Galleon_Materials_Planks")
    for k in range(8):
        plank(pb, (-1.6, -k * 0.2 - 0.2, 0.05), (1.2, 0.17, 0.045), "hull", rng)
        plank(pb, (-3.0, -k * 0.2 - 0.2, 0.05), (1.2, 0.17, 0.045), "deck", rng)
    planks = pb.finish(coll)
    rep = check()
    print("MATS_CHECK " + json.dumps({"ok": rep["ok"], "count": len(rep["materials"]),
                                      "missing": {n: e["missing"] for n, e in rep["materials"].items() if e["missing"]},
                                      "unloaded": [n for n, e in rep["materials"].items() if any(not i["loaded"] for i in e["images"])]},
                                     ensure_ascii=False))
    return [sheet, planks] + labels
