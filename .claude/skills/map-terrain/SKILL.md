---
name: map-terrain
description: Construit le terrain jouable d'une île réelle à partir des données de référence — classes officielles (trait de côte + atlas des récifs IRD), bathymétrie estimée, masques de matériau, meshes Nanite tuilés, calque des parcelles cadastrales — puis l'importe dans Unreal via le MCP avec matériaux terrain et eau. À utiliser pour (re)générer la map d'une île.
argument-hint: "<ile>"
---

# Terrain depuis données réelles

Île : $ARGUMENTS. Données dans `docs/reference/map/<ile>/`, récupérées par la skill `map-geodata` : `raw_sat_z15.png`, `atlas_recif_z15.png` (`fetch_imagery.py ... TEFENUA:std_atlasrecif`), `rivage.geojson`, `parcelles.geojson`. Exemple complet : Raraka (`docs/reference/map/raraka/README.md`).

## 1. Générer (Python)
```bash
python scripts/map/build_terrain.py <ile>             # classes, hauteurs, masques, aperçus, tuiles OBJ, plan d'eau
python scripts/map/build_terrain.py <ile> --no-mesh   # ne régénère que masques + aperçus
python scripts/map/build_parcels.py <ile>             # calque des parcelles + parcelles_ue.json
python scripts/map/make_water_normal.py               # normal map de houle tuilable (une fois)
```
**Toujours regarder** `terrain/preview_classes.png`, `preview_relief.png` et `preview_masks.png` avant l'import.

### Méthode (pas de couleur absolue)
- Terre = contours fermés du trait de côte (règle pair-impair). Platier / tombant / lagon / pinacles / passe = atlas IRD reclassé par couleur.
- L'anticrénelage de l'atlas entoure les pinacles d'anneaux de couleurs mélangées : les petites composantes isolées dans le lagon sont reclassées en pinacles (sinon cuvettes circulaires).
- Lagon : profondeur lissée selon la distance au bord ; pinacles = pitons à flancs raides ; hauts-fonds = contraste local de l'imagerie (luminance − moyenne locale), nuages rejetés par la chromaticité.

## 2. Importer dans Unreal (MCP)
Contraintes vérifiées sur UE 5.8 :
- `StaticMeshTools.import_file` n'accepte que **FBX / OBJ** (pas de glTF). L'OBJ est lu en Z haut, en cm, avec `UE = (x, -y, z)` : le script écrit (est, nord, haut) en cm et inverse V.
- L'import crée une **enveloppe convexe** comme collision : `remove_collisions`, puis `BodySetup.CollisionTraceFlag = CTF_UseComplexAsSimple`.
- Nanite : `NaniteSettings = {bEnabled: true, fallbackTarget: PercentTriangles, fallbackPercentTriangles: 1.0}` (fallback pleine résolution, collision fidèle).
- L'import n'écrase pas un asset existant : importer sous un nouveau nom, repointer, puis supprimer l'ancien.
- Pas d'outil « nouveau niveau » : dupliquer `/Engine/Maps/Templates/Template_Default` (`AssetTools.duplicate`), le sauvegarder, le charger. Retirer `Floor` et la sphère de ciel (rayon ~6 km, plus petite que la map).

Étapes :
1. Niveau `/Game/MonkeIsland/Maps/Lvl_<Ile>`.
2. Tuiles par lots de 8 via `ProgrammaticToolset.execute_tool_script` (~13 s par tuile) : import → collision → Nanite → `add_to_scene_from_asset` à l'origine → dossier `<Ile>/Terrain` → `save_assets`.
3. Plan d'eau `SM_<Ile>_Water` : sans collision (`CTF_UseSimpleAsComplex` sans formes simples), à Z = 0. Le niveau d'eau reste réglable en déplaçant l'acteur (montée des eaux, surcote).
4. Matériaux (skill `ue-material`) :
   - `M_Terrain_Atoll` : Substrate Slab, masques en UV0, couches `art/textures/terrain_layers` (`*_ch` = couleur sRGB + hauteur, `*_n` = normale OpenGL → `bFlipGreenChannel`) en world-aligned.
   - `M_Water_Lagoon` : Substrate Single Layer Water (extinction en **1/cm**, ≈ eau pure + diffusion carbonatée) **plus** un nœud legacy `SingleLayerWaterMaterialOutput` alimenté par les mêmes paramètres, obligatoire pour la compilation quand le ray tracing est activé.
5. Parcelles : `SM_<Ile>_Parcelles` + `M_Debug_Parcels` (unlit, double face), sans collision, `bHiddenInGame`.
6. PlayerStart : trouver le sol par `SceneTools.trace_world`, puis `ActorTools.set_actor_transform`.

## 3. Valider
- `CaptureViewport` (la réponse est un PNG base64 trop gros : la sauvegarder puis `python scripts/mcp_image.py <réponse.txt> <sortie.png>`).
- Vues : aérienne (≈ 300–450 m, pitch −25°), joueur sur la plage (yeux à +1,7 m), verticale au-dessus du village pour les parcelles (≈ 120 m).
- Lire les masques à un point précis si le rendu est suspect (UV = (est + cx)/largeur, (sud + cz)/hauteur).

## Pièges rencontrés
- **Pillow redimensionne le RGBA en alpha prémultiplié** : un masque avec A = 0 perd ses canaux RGB. Redimensionner canal par canal.
- Un plan d'eau avec collision bloque les traces et le joueur.
- La normal map d'eau à petite échelle se répète vue de haut : prévoir une atténuation avec la distance.
