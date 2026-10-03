---
name: ue-editor
description: Mode d'emploi du serveur MCP Unreal (plugin officiel UE 5.8 "Unreal MCP") pour piloter l'éditeur monke_island — démarrage, découverte des toolsets, appels d'outils, dépannage.
---

# MCP Unreal — mode d'emploi

## Connexion
- Serveur HTTP dans l'éditeur : `http://127.0.0.1:8000/mcp`, déclaré dans `.mcp.json` sous le nom `unreal`.
- Démarre automatiquement à l'ouverture de l'éditeur (`bAutoStartServer=True` dans `Config/DefaultEditorPerProjectUserSettings.ini`).
- Sinon, console éditeur (`~`) : `ModelContextProtocol.StartServer` (ou `ModelContextProtocol.StartServer 8001`).
- Vérification : `scripts/ue.ps1 status`.
- Outils absents dans Claude Code alors que l'éditeur tourne → `/mcp` pour reconnecter le serveur `unreal`.
- Toujours `127.0.0.1`, jamais `localhost` : le serveur UE n'écoute qu'en IPv4 et `localhost` peut résoudre en `::1` (ECONNREFUSED).

## Découverte (mode tool search)
1. `list_toolsets` → toolsets chargés.
2. `describe_toolset` sur celui dont tu as besoin → noms d'outils et schémas de paramètres.
3. `call_tool` avec `toolset_name` + `tool_name` exacts (sans préfixe) et les paramètres du schéma. Tout paramètre sans valeur par défaut est obligatoire, même s'il est « optionnel » en pratique (ex. `annotations` de `CaptureViewport`).

## Toolsets disponibles (vérifiés le 2026-10-02)
| Toolset | Usage principal |
|---------|-----------------|
| `EditorToolset.EditorAppToolset` | PIE start/stop, caméra, `CaptureViewport` / `CaptureEditorImage` / `CaptureAssetImage`, sélection, Content Browser, recherche de CVars |
| `EditorToolset.LogsToolset` | lire l'Output Log, verbosité |
| `editor_toolset.toolsets.scene.SceneTools` | charger un niveau, placer/supprimer des acteurs, dossiers d'outliner, `trace_world`, level instances, fusion d'acteurs |
| `editor_toolset.toolsets.actor.ActorTools` | transforms, labels, hiérarchie, composants |
| `editor_toolset.toolsets.asset.AssetTools` | find / duplicate / move / delete / save, dossiers, référenceurs, fichiers texte (Game/Saved) |
| `editor_toolset.toolsets.object.ObjectTools` | `list/get/set_properties` sur tout UObject (BodySetup, NaniteSettings, composants…), sous-classes |
| `editor_toolset.toolsets.static_mesh.StaticMeshTools` | `import_file` (FBX/OBJ), Nanite, LODs, collisions, matériaux, bornes |
| `editor_toolset.toolsets.texture.TextureTools` | `import_file` (images), taille |
| `editor_toolset.toolsets.material.MaterialTools` / `material_instance.MaterialInstanceTools` | graphes de matériaux (nœuds, liens, sorties, recompile), instances |
| `editor_toolset.toolsets.programmatic.ProgrammaticToolset` | script Python **sandboxé** enchaînant des appels d'outils (`get_execution_environment` d'abord ; modules : json, re, math, time, datetime, copy) |
| `blueprint`, `data_asset`, `data_table`, `curve_table`, `string_table`, `primitive` (editor_toolset) | assets correspondants |
| `editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools` | **`import_file` de skeletal meshes FBX** (skeleton existant, physics asset, matériaux), os, sockets, matériaux |
| `LiveCodingToolset`, `AutomationTestToolset`, `ConfigSettingsToolset`, `PluginToolset`, `GameplayTagsToolset`, `UMGToolSet`, `PCGToolset` (+ `PCGSpatialToolset`) | compilation, tests, config, plugins, tags, UI, PCG |
| `StateTreeTools`, `BehaviorTreeTools` | **inspection** des assets d'IA |
| `ToolsetRegistry.AgentSkillToolset` | skills côté éditeur |

## Limites connues
- Pas d'outil **Landscape**, pas de création de **niveau** (dupliquer `/Engine/Maps/Templates/Template_Default`), pas d'exécution de commande console (CVars en recherche seule ; réglages via `ConfigSettingsToolset`), pas de Python libre.
- Import : **textures**, **static meshes** et **skeletal meshes** FBX (`SkeletalMeshTools.import_file`) ; l'import n'écrase pas un asset existant. Le reste : glisser-déposer ou commandlet `ImportAssets` éditeur fermé (skill `ue-import`).
- `CaptureViewport` renvoie un PNG en base64 trop gros pour l'afficher : sauvegarder la réponse puis `python scripts/mcp_image.py <réponse.txt> <sortie.png>`.
- `load_level` échoue si le niveau courant ou cible a des modifications non sauvegardées : `AssetTools.save_assets` d'abord.

## Ajouter un toolset
Disponibles dans le moteur (`Engine/Plugins/Experimental/Toolsets/`) : AnimationAssistant, ChaosClothAsset, Conversation, DataRegistry, Dataflow, GameFeatures, GAS, MVVM, MetaHumanGenerator, Niagara, PCG, Physics, SemanticSearch, SequencerAnimMixer, SlateInspector, WorldConditions, MCPClient.
→ ajouter `{ "Name": "<X>Toolset", "Enabled": true, "TargetAllowList": ["Editor"] }` dans `monke_island.uproject`, redémarrer l'éditeur.

## Bonnes pratiques
- Lecture/inspection avant écriture.
- Sauvegarder les assets après modification.
- Opérations destructives → confirmation utilisateur.
