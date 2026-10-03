---
name: ue-material
description: Crée ou modifie des matériaux et instances de matériaux dans monke_island via le MCP (MaterialTools, MaterialInstanceTools) — master materials paramétrés, instances, recettes terrain d'atoll et eau de lagon Single Layer Water.
argument-hint: "<matériau à créer ou modifier>"
---

# Matériaux

Demande : $ARGUMENTS

## Méthode
1. `describe_toolset` sur `editor_toolset.toolsets.material.MaterialTools` et `editor_toolset.toolsets.material_instance.MaterialInstanceTools` (noms et paramètres exacts, ne pas deviner).
2. Chercher l'existant (`AssetTools.find_assets`) pour réutiliser un master.
3. Master (`M_`) dans `/Game/MonkeIsland/Materials/` : paramètres nommés et décrits (scalaires, vecteurs, textures) ; variantes en instances (`MI_`).
4. Textures : importées via la skill `ue-import`, avec les bons réglages (sRGB pour base color, Normal map, masques linéaires).
5. Valider : `CaptureAssetImage` sur le matériau, puis `CaptureViewport` en situation.

## Recette : terrain d'atoll (`M_Terrain_Atoll`)
- Entrées : texture de masques du pipeline carto (UV0), canaux documentés dans `terrain/georef.json` (ex. R = végétation / litière, G = sable sec, B = platier / débris coralliens, A = sable mouillé).
- Couches PBR en world-aligned (tiling en mètres) depuis `art/textures/` : `coast_sand_01`, `damp_beach_sand`, `coral_gravel`, `coral_ground_02`, `dry_decay_leaves`…
- Le fond du lagon est du **sable corallien clair** : c'est l'eau qui le teinte, pas le matériau du sol.
- Variation macro (bruit grande échelle) pour casser la répétition vue d'avion.

## Recette : eau de lagon (`M_Water_Lagoon`)
- Shading model **Single Layer Water**, blend mode Opaque, sortie `SingleLayerWaterMaterialOutput` (Scattering, Absorption, Phase G).
- Le turquoise vient de la profondeur réelle : absorption forte dans le rouge, faible dans le bleu ; diffusion faible pour une eau claire. Calibrer visuellement sur une capture au bord du lagon (fond à 1 m turquoise clair, à 20 m bleu profond).
- Normales : deux normal maps panoramiques croisées, faible amplitude dans le lagon, plus forte côté océan.
- Point de départ possible : les matériaux du plugin Water d'Epic, si le plugin est activé (PluginToolset).

## Règles
- Pas de valeur magique : chaque paramètre exposé a un nom explicite et une description.
- Coût shader vérifié avec `ue-performance-engineer` pour les masters utilisés à grande échelle.
