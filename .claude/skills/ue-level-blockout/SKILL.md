---
name: ue-level-blockout
description: Blockout d'une zone jouable dans l'éditeur via le MCP — volumes gris, chemins, couverts, repères, PlayerStart — à partir d'un doc de zone, puis validation en PIE et par captures.
argument-hint: "<zone>"
---

# Blockout

Zone : $ARGUMENTS

1. Doc de zone `docs/levels/<zone>.md` (template `docs/templates/level-design.md`) : s'il n'existe pas, le créer d'abord avec `ue-level-designer`.
2. MCP : `describe_toolset` sur `editor_toolset.toolsets.scene.SceneTools` et `editor_toolset.toolsets.actor.ActorTools`.
3. Charger le niveau cible (`/Game/MonkeIsland/Maps/Lvl_<...>`), puis cadrer la zone (`SetCameraTransform`).
4. Placer les éléments avec les meshes de `/Game/LevelPrototyping/Meshes` (et `/Game/LevelPrototyping/Interactable` : portes, jump pads, cibles) :
   - labels explicites (`BO_<Zone>_<Élément>`) ;
   - dossier d'outliner `Blockout/<Zone>` ;
   - en batch via `ProgrammaticToolset.execute_tool_script` pour plus de ~10 acteurs (après `get_execution_environment`).
5. `PlayerStart` au point d'entrée de la zone.
6. Capture annotée (`CaptureViewport` avec grille) en vue de dessus et à hauteur d'yeux (≈ 1,7 m), puis PIE pour tester le parcours.
7. Mettre à jour le doc de zone (captures, problèmes relevés) et le chantier.

## Règles
- Gris et lisible : pas de matériaux finaux pendant le blockout.
- Hauteurs, sauts et passages validés en PIE, pas supposés.
- Ne pas toucher aux niveaux du template sans demande.
