---
name: blender-asset
description: Produit un asset 3D dans Blender via le MCP Blender (modélisation, récupération Poly Haven / Sketchfab / Poly Pizza, génération IA) et l'exporte vers Unreal avec les bonnes conventions (échelle, pivot, collisions, nommage), puis l'importe.
argument-hint: "<asset> [références]"
---

# Asset Blender → Unreal

Asset : $ARGUMENTS

## 1. Préparer
- Charger les outils : ToolSearch `select:mcp__blender__get_addon_status,mcp__blender__get_scene_info,mcp__blender__execute_blender_code,mcp__blender__get_viewport_screenshot,mcp__blender__get_object_info`.
- `get_addon_status` (version de Blender, intégrations actives), puis `get_scene_info`. Blender injoignable → demander d'ouvrir Blender et de cliquer « Connect » dans le panneau BlenderMCP.
- Brief : `docs/art/briefs/<asset>.md` (template `docs/templates/art-brief.md`), avec dimensions réelles et références.

## 2. Produire (par ordre de préférence)
1. Asset existant sous licence compatible (Poly Haven CC0, Sketchfab téléchargeable, Poly Pizza) → crédit dans `art/SOURCES.md`.
2. Modélisation par script (`execute_blender_code`) pour les formes simples ou architecturales (fare, quai, citerne).
3. Génération IA (Hyper3D / Hunyuan3D / Tripo) pour un objet isolé ; dupliquer plutôt que régénérer.
- Nœuds de shader : les chercher par type, jamais par nom (UI localisée).

## 3. Contrôler
- Dimensions réelles (`get_object_info`, `world_bounding_box`) ; pivot au centre de la base, posé en Z = 0 ; transformations appliquées.
- Polygones raisonnables (Nanite côté UE pour le haut détail) ; UV propres ; matériaux nommés `M_<Nom>`.
- Collision : `UCX_SM_<Nom>_00` (convexes) ou génération côté UE.
- `get_viewport_screenshot` pour vérifier.

## 4. Exporter
- `art/export/<Feature>/SM_<Nom>.fbx` (ou `.glb`), mesh sélectionné seulement, modificateurs appliqués, unités en mètres.

## 5. Importer dans Unreal
- `StaticMeshTools.import_file` → `/Game/MonkeIsland/<Feature>/Meshes`, puis Nanite et collisions si besoin (skill `ue-import`).
- `CaptureAssetImage` : vérifier l'échelle (comparer à un mesh connu du template) et l'orientation (l'avant pointe vers +X). Corriger dans Blender et réexporter plutôt que de compenser dans UE.
