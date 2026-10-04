# monke_island — Unreal Engine 5.8 C++

## Cahier des charges officiel (contrat produit)
**`docs/CAHIER_DES_CHARGES.md`** — *Crownfall Expeditions* v1.2 : jeu coop 1–4 joueurs (listen server) d'extraction maritime ; pacifier des zones (drapeau + sermon immobile 20 s = +120 s), récolter, arrimer dans la barque, extraire avant la submersion, bilan quotas/taxe, hub galion.
- **Il prime** sur les specs et habitudes de l'équipe. Adoption, transposition dans ce dépôt et écarts à arbitrer (caméra, Raraka réel vs archipel fictif, fiction vs village réel) : `docs/adr/ADR-001-cahier-des-charges-crownfall.md`.
- **Lots** L0 → L4 (prototype), L5–L6 (V1), L7 (internet/VoIP), L8+, E1 (éditeur d'îles) — dans cet ordre ; rien de L7/L8/E1 avant le noyau validé. Chaque sprint vise un lot, chaque tâche cite le lot et les IDs de recette (§12, §23).
- **Honnêteté de preuve** (§2, §13) : « code écrit » ≠ validé ; une tâche passe `fait` seulement avec preuve et précise `compilé` / `testé éditeur` / `testé build` / `testé multi-machines`. Ne jamais prétendre avoir créé un `.uasset`, testé un réseau ou livré un build sans l'avoir fait.
- Toutes les valeurs du cahier (120 s, +120 s, 20 s, 60 s, 12 emplacements / 150 kg, taxe 35 %…) en DataAsset (`DA_ExpeditionRules`…), jamais en dur. Serveur autoritaire et réplication dès L1.
- Une map ou un Blueprint = un seul agent propriétaire à la fois. Pas de plugin payant, pas de publication sans accord.

## Existant
Jeu bâti sur le template First Person C++ d'UE 5.8 (variantes Horror et Shooter incluses), sur l'atoll réel de **Raraka** (Tuamotu) à l'échelle 1:1. Niveau principal : `/Game/MonkeIsland/Maps/Lvl_Raraka`. Fiche de données : `docs/reference/map/raraka/README.md`.

Personnage jouable : `BP_Islander` (C++ `AIslanderCharacter` + `UIslanderMovementComponent`), troisième personne, physique d'un adulte réel (1,80 m, 75 kg, 9,8 m/s²), sprint, pataugeage, nage. Le niveau de la mer est géré par `UIslandWaterSubsystem` (acteur tagué `IslandWaterSurface`). GameMode : `BP_IslandGameMode`. Spec : `docs/features/player-islander.md`.

Barque physique (L2, ADR-003) : `Source/monke_island/Boats/` (`ABoatActor`, `UBoatRiderComponent`, `UBoatSubsystem`, `DA_BoatRules`), spec `docs/features/boat-rowing.md`, asset `SM_Barque` (`docs/art/briefs/barque.md`, générateur `scripts/props/barque.py`) ; F s'asseoir/se lever, ZQSD ramer, console `monke.SpawnBoat`. Rôle `ue-boat-programmer`, skill `/ue-boat-physics`.

Personnage pilote du cahier (§10) : bean « soldat britannique » (`sprite/player-1.png`), 1,05 m, ragdoll actif façon Fall Guys, skins modulaires — fiche `docs/features/character-bean-soldier.md`, générateur `scripts/character/bean_soldier.py`, écarts `docs/adr/ADR-002-personnage-bean.md`. Pipeline : `/character-pipeline`.

## Arborescence
- `monke_island/` — projet Unreal (`monke_island.uproject`, moteur `C:/Program Files/Epic Games/UE_5.8`)
  - `Source/monke_island/` — module C++ unique. Code du template : racine (Character, PlayerController, GameMode, CameraManager), `Variant_Horror/`, `Variant_Shooter/` (IA StateTree, armes, UI). Nouveau code : `Source/monke_island/<Feature>/`, include `"<Feature>/X.h"`.
  - `Content/` — assets du template (ne pas réorganiser) ; nouveau contenu dans `/Game/MonkeIsland/` (voir `docs/conventions/asset-naming.md`).
- `scripts/` — `ue.ps1` (build / gen / test / editor / status / clean / profile / package), `chantier.py`, `perf_summary.py`, `mcp_image.py`, `map/` (pipeline carto), `atoll/` + `pack_terrain_layers.py` + `fetch_atoll_textures.py` (look-dev Blender et textures).
- `art/` — textures CC0 (`art/textures/`, couches de terrain `terrain_layers/`), HDRI, modèles ; crédits dans `art/SOURCES.md`. Mondes archivés et réutilisables (kit .blend + textures par type + README) : `art/archives/environnements/<monde>/`, dont `atoll_tuamotu`.
- `docs/` — `CAHIER_DES_CHARGES.md` (contrat produit), `features/` specs, `adr/`, `bugs/`, `levels/`, `templates/`, `conventions/`, `production/` (backlog, chantier, journal), `reference/map/<ile>/`.
- `.claude/factory/FACTORY.md` — charte de l'usine agentique (cellules, environnements DEV → REC → INT → PROD, ressources exclusives, vagues) ; `.claude/agents/` — l'équipe (29 rôles) ; `.claude/skills/` — les workflows (35 skills).

## Commandes
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 status    # éditeur + MCP
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 build     # UBT (éditeur fermé)
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 test -Filter monke_island.X
python scripts/chantier.py --env                                              # tableau de chantier
```
Éditeur partagé entre sessions (Claude + Codex) : **verrou obligatoire** avant fermeture, build principal, tests headless ou relance — `python scripts/editor_lock.py status | acquire --owner <session> --reason … | close-editor | release` (vérifié par `ue.ps1`, propriétaire dans `MONKE_LOCK_OWNER`).
Éditeur ouvert → compiler par **Live Coding via MCP**, seulement pour du code dans les corps de fonctions ; tout changement de header réfléchi exige l'éditeur fermé et un build.

## MCP
- `unreal` : plugin officiel UE 5.8 « Unreal MCP » sur `http://127.0.0.1:8000/mcp` (jamais `localhost` : IPv4 seulement), démarré avec l'éditeur. Mode tool search : `list_toolsets` → `describe_toolset` → `call_tool`. Toolsets, limites et astuces : skill `ue-editor`.
- `blender` : Blender MCP (addon à connecter dans Blender) ; skill `blender-asset`. **Depuis le 2026-10-03, les maquettes sont faites sur le 2e PC** (Claude Code + Blender) et récupérées dans `C:\Users\matth\OneDrive\maquette-blender` (lecture seule, fichier synchronisé avant import) ; ce PC se concentre sur l'intégration gameplay et l'orchestration.

## Équipe et chantier
- Qui fait quoi : table dans `.claude/agents/ue-producer.md` ; cellules et environnements : `.claude/factory/FACTORY.md`. Pipeline d'une feature : `/ue-feature`. Lancer une vague : `/factory wave` ; passer REC / INT / PROD : `/factory-gate`.
- **Statuts cahier → chantier** : À faire `a-faire` · En cours `en-cours` · À vérifier `review` · Validé `fait` (avec preuve) · Bloqué `bloque`.
- **Chantier** : chaque tâche est une ligne `T-NNN` de `docs/production/board.md`, gérée par `scripts/chantier.py` (`/chantier`). Les agents passent leur tâche `en-cours` au début et `fait` / `review` / `bloque` à la fin. Quand la session principale lance un agent, elle lui donne l'ID de tâche.
- Backlog (lots du cahier, puis mécaniques futures) : `docs/production/backlog.md`. Le niveau d'eau doit rester un paramètre global.

## Skills
`/factory` · `/factory-gate` · `/chantier` · `/ue-sprint` · `/ue-feature` · `/ue-new-class` · `/ue-build` · `/ue-test` · `/ue-review` · `/ue-bugfix` · `/ue-editor` · `/ue-import` · `/ue-material` · `/ue-level-blockout` · `/ue-pcg-scatter` · `/ue-ui-widget` · `/ue-ai-behavior` · `/ue-playtest` · `/ue-profile` · `/ue-package` · `/ue-vcs` · `/map-geodata` · `/map-terrain` · `/blender-asset` · `/character-pipeline` · `/character-model` · `/character-rig` · `/character-physics` · `/character-skin` · `/ue-boat-physics` · `/ue-architecture` · `/ue-optimize` · `/ue-memory-audit` · `/ue-security-review` · `/ue-error-handling`

## Conventions C++
- Style Epic : tabs, accolades à la ligne, `/** */` sur chaque UPROPERTY/UFUNCTION, `Category` obligatoire.
- Préfixes A/U/F/E/I/T, `MONKE_ISLAND_API`, `TObjectPtr` + `UPROPERTY()` pour les membres UObject.
- Valeurs de gameplay tunables (`EditDefaultsOnly` / DataAsset), jamais en dur.
- Tick désactivé par défaut. Logs : `UE_LOG(Logmonke_island, ...)`, ou `LogMonke<Domaine>` pour un domaine critique ou bavard.
- Normes d'ingénierie : `docs/conventions/` — `architecture.md`, `performance.md`, `memory.md`, `security.md` et `error-handling.md`. Chaque review cite leurs contrôles (ARC, MEM, SEC, ERR) ; elles s'appliquent dès le design.
- Tests : Automation Spec, chemin `monke_island.<Feature>.<Cas>`, flags `EditorContext | ProductFilter`.
- Langue : échanges en français, code et identifiants en anglais.

## Données réelles
- Données Te Fenua / DAF sous Licence Ouverte Etalab 2.0 (citer la source). Imagerie © Maxar via Te Fenua : référence de modélisation uniquement, jamais embarquée.
- Le village de Motutapu est réel : personnages et événements du jeu sont fictifs.
