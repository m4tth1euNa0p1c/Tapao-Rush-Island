---
name: tidemark-bootstrap
description: Crée le squelette du plugin Tidemark dans monke_island/Plugins/Tidemark (uplugin, modules TidemarkRuntime et TidemarkEditor, Build.cs, catégorie de log, dossiers, README, CHANGELOG, Tools), l'active dans le .uproject et vérifie qu'il compile sans aucune dépendance au module du jeu. À utiliser une fois, pour TM-01, ou pour recréer un module.
argument-hint: "[create | add-module <Nom> <Runtime|Editor> | verify]"
---

# Squelette du plugin Tidemark

Arguments : `$ARGUMENTS`. Conventions : `docs/produit/tidemark/CONVENTIONS.md` §1–§3. Rôle : `tidemark-lead`.

## `create`
1. Vérifie qu'aucun `Plugins/Tidemark/` n'existe. Verrou éditeur : `python scripts/editor_lock.py status` ; un nouveau plugin exige l'éditeur fermé et un build UBT.
2. Arborescence :
   ```
   monke_island/Plugins/Tidemark/
     Tidemark.uplugin
     Source/TidemarkRuntime/TidemarkRuntime.Build.cs
     Source/TidemarkRuntime/Public/Tidemark/TidemarkModule.h      (LogTidemark, LogTidemarkNet)
     Source/TidemarkRuntime/Private/TidemarkModule.cpp
     Source/TidemarkRuntime/Private/Tests/.gitkeep
     Source/TidemarkEditor/TidemarkEditor.Build.cs
     Source/TidemarkEditor/Public/TidemarkEditor/TidemarkEditorModule.h
     Source/TidemarkEditor/Private/TidemarkEditorModule.cpp
     Content/.gitkeep
     Docs/README.md  Docs/CHANGELOG.md  Docs/LICENSES/.gitkeep
     Tools/.gitkeep
     PROVENANCE.json   ({"assets": []})
   ```
3. `Tidemark.uplugin` : `FileVersion 3`, `Version 1`, `VersionName "0.1.0"`, `FriendlyName "Tidemark"`, `Category "Simulation"`, `CanContainContent true`, modules `TidemarkRuntime` (`Runtime`, `Default`) et `TidemarkEditor` (`Editor`, `PostEngineInit`), `EnabledByDefault false`. Pas de `Plugins` dépendants.
4. `TidemarkRuntime.Build.cs` : `PCHUsage = UseExplicitOrSharedPCHs` ; Public : Core, CoreUObject, Engine ; Private : NetCore, RenderCore, RHI, Niagara. `TidemarkEditor.Build.cs` : Public : Core, CoreUObject, Engine, TidemarkRuntime ; Private : UnrealEd, Landscape, MeshDescription, Slate, SlateCore, ToolMenus, EditorFramework, Projects. **Aucune mention de `monke_island`.**
5. Macros d'export `TIDEMARKRUNTIME_API`, `TIDEMARKEDITOR_API`. Style Epic (tabs, accolades à la ligne).
6. Active le plugin dans `monke_island/monke_island.uproject` (`"Name": "Tidemark", "Enabled": true`) et ajoute `TidemarkRuntime` dans `monke_island.Build.cs` **seulement** quand `tidemark-tapao-integrator` en a besoin (S7) ; avant, le plugin compile seul avec le projet.
7. Build : `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 build`. 0 erreur, 0 warning nouveau.
8. `Docs/README.md` : une page (ce que fait le plugin, trois couches, comment lancer les specs). `CHANGELOG.md` : `0.1.0 — squelette`.

## `add-module <Nom> <Runtime|Editor>`
Même modèle que ci-dessus, module ajouté au `.uplugin` et au `Build.cs` dépendant. À n'utiliser qu'avec l'accord de `tidemark-lead` (le cahier prévoit deux modules).

## `verify`
1. `grep -rn "monke_island\|/Game/MonkeIsland" monke_island/Plugins/Tidemark/Source` → vide.
2. `grep -rn "FMath::Rand\|FRand\|RandRange\|UtcNow\|Cycles()" monke_island/Plugins/Tidemark/Source --include=*.cpp --include=*.h | grep -v /Tests/` → vide.
3. Build OK ; `scripts/ue.ps1 test -Filter Tidemark` liste les specs (même vides).
4. Rapport : arborescence, résultat des greps, sortie de build (dernières lignes), niveau de preuve `compilé`.
