# 31 — Architecture du module C++
Domaine : D · Auditeur : architecte C++ Unreal

## Verdict
Noyau de règles sain (en-têtes acycliques, DataAssets) mais squelette Gameplay Framework absent (GameMode vide, aucun PlayerState), 13 dossiers sur 28 en cycle d'inclusion, aucune copie du dépôt hors machine. Risque commercial : empiler ADR-005 et le multijoueur dessus ralentira les manches et masquera les bugs réseau.

## Constats
- **[Majeur] Aucune sauvegarde hors machine, commits par lots**
  - Constat : vers 02 h 30, 88 fichiers source (14 743 lignes, 29 %, dont tout `Weapons/`) n'étaient pas versionnés ; dix minutes et neuf commits plus tard, 27 (2 696 lignes) restaient hors dépôt. Aucun remote ; 17 worktrees = 43 Go, 14 fusionnés.
  - Preuve : `git ls-files --others`, `git remote -v`, `git branch --merged`.
  - Recommandation : un commit par tâche close ; sauvegarde quotidienne (disque externe ou remote privé) ; purger les worktrees fusionnés.
  - Effort : S

- **[Majeur] Cycle de 13 dossiers, pas de socle `Core/`**
  - Constat : sur 71 arêtes entre dossiers, 13 (Player, Weapons, Character, Boats, Expedition, Storm, UI…) forment une composante fortement connexe ; ex. Expedition→Player→Storm→Expedition. `IslanderHealthComponent.h` est inclus par 20 fichiers de 10 dossiers et son .cpp appelle l'inventaire d'armes ; le modifier impose « éditeur fermé + build ».
  - Preuve : graphe recalculé hors tests ; `IslanderHealthComponent.cpp:52` ; `IslanderHUD.cpp:4-7` ; `architecture.md` §2.
  - Recommandation : créer `Core/` (santé, `IIslanderInteractable`, interface de dégâts), déplacer `IslanderHUD` dans `UI/`, contrôler le graphe en gate INT.
  - Effort : M

- **[Majeur] Règles dans le GameState et l'UI, GameMode vide**
  - Constat : `BP_IslandGameMode` hérite d'un stub du template et utilise le contrôleur du template ; aucun `APlayerState` pour « vivant, prêt, embarqué ». Le GameState coupe le respawn par réflexion sur `RespawnDelay` et déclenche le voyage de retour ; `UTapaoUISubsystem::PollGame` incrémente `Wrecks`, écrit la sauvegarde et décide la défaite d'après le pion local : deux règles de défaite.
  - Preuve : `monke_islandGameMode.cpp:7` ; `ExpeditionGameState.cpp:299-308,361` ; `TapaoUISubsystem.cpp:523-550` ; grep binaire de `BP_IslandGameMode.uasset`.
  - Recommandation : avant ADR-005, `AExpeditionGameMode`, `AIslanderPlayerState`, `AIslanderPlayerController` ; l'UI écoute le GameState ; supprimer `PollGame`.
  - Effort : M

- **[Majeur] « Quel fragment joue-t-on ? » : deux résolveurs, deux copies répliquées**
  - Constat : `UExpeditionIslandSubsystem` (URL, sélection, registre) alimente `AExpeditionGameState::FragmentId` ; `AVillageSpawner` refait sa résolution (URL, `UBoatStartSubsystem`, joueur le plus proche, minuterie) et réplique `ActiveFragment`. Arrivée tardive ambiguë ; chaque spawner ajoutera une variante.
  - Preuve : `ExpeditionIslandSubsystem.cpp:549` ; `VillageSpawner.cpp:126-170` ; `ExpeditionGameState.cpp:49`.
  - Recommandation : une seule vérité (GameState) et un événement `OnFragmentResolved` pour les spawners.
  - Effort : M

- **[Majeur] Neuf acteurs-services posés à la main ; `TikiDirector` classe-dieu**
  - Constat : `Lvl_Raraka.umap` (110 Mo) embarque 9 acteurs-services (TikiDirector, DayNightCycle, FloodSimulation, WindStorm, CoconutFallDirector, 4 spawners), retrouvés par des `Get()` en `TActorIterator` ; trois subsystems spawnent déjà les leurs. `TikiDirector` : 1 683 lignes (seuil ARC-07 : 1 000), neuf responsabilités (villages, routines, gardiens, nuit, embuscades, noix, totems, buissons, débogage).
  - Preuve : grep binaire du `.umap` ; `TikiDirector.h/.cpp` ; `ShoreAmbienceSubsystem.cpp:43`.
  - Recommandation : un `UExpeditionServicesSubsystem` créant les services depuis un DataAsset ; scinder le directeur sous `TikiSpec`.
  - Effort : M (registre), L (scission)

- **[Majeur] Recherches d'acteurs à chaque image (ARC-04, ARC-05)**
  - Constat : `AIslanderHUD::DrawHUD` fait deux parcours d'acteurs par image, `UFootstepSubsystem::Tick` un autre, et `FindWindCalmZones` garde un cache `static` partagé entre mondes (rebâti à chaque appel d'un autre monde, PIE hôte + client). Coût non mesuré.
  - Preuve : `IslanderHUD.cpp:79,83,93` ; `FootstepSubsystem.cpp:142` ; `WindStormActor.cpp:35-56`.
  - Recommandation : caches `TWeakObjectPtr` au BeginPlay ; registre des personnages dans un subsystem.
  - Effort : S

- **[Majeur] Template encore câblé, impossible à supprimer à l'aveugle**
  - Constat : 22 .cpp sur 144 (≈ 4 400 lignes, 9 %) viennent du template, sans inclusion depuis Crownfall ; pourtant `BP_Islander` référence `IMC_Horror`, `IA_Sprint`, `SKM_Manny_Simple`, `BP_IslandGameMode` le contrôleur template, et `GlobalDefaultGameMode` le GameMode template (`L_MainMenu` sans surcharge : pion template supposé). `Build.cs` garde 6 chemins Variant et deux modules StateTree utilisés par `Variant_Shooter` seul.
  - Preuve : grep binaire des `.uasset` ; `DefaultEngine.ini:17` ; `monke_island.Build.cs:18-19,37-42`.
  - Recommandation : repointer ces références, puis supprimer code, modules et plugins StateTree.
  - Effort : M

- **[Mineur] Dette de build et de configuration**
  - Constat : `monke_island.uproject` active `ModelContextProtocol` (expérimental, Runtime) sans `TargetAllowList` : embarqué dans un build jeu (supposé). Trois collisions de build unity en 87 commits ; 154 `TObjectPtr` vers assets lourds, 27 souples.
  - Preuve : `monke_island.uproject:35` ; commits `740eaaf`, `641605f`, `6225d1f` ; `architecture.md` §4.
  - Recommandation : `TargetAllowList: ["Editor"]` ; build `-DisableUnity` en gate INT ; `TSoftObjectPtr` pour les DataAssets lourds.
  - Effort : S

## Manques
1. `PlayerState`, `PlayerController`, `GameMode` du jeu ; `SaveSubsystem` atomique et versionné ; `SessionSubsystem`.
2. `Core/`, interfaces `Damageable` et `Carryable` (cahier §21).
3. `docs/architecture.md`, ADR « système maison contre GAS ».
4. Graphe ARC-01 et build sans unity automatisés.

## Quick wins
1. Sauvegarde externe, purge des worktrees fusionnés.
2. `TargetAllowList` sur `ModelContextProtocol`.
3. Cacher les recherches du HUD et des pas (2 h).
4. `BP_MenuGameMode` sans pion pour `L_MainMenu`.

## Questions qui challengent l'équipe
1. Si le disque de cette machine meurt ce soir, que reste-t-il du jeu ?
2. « Un fichier = un agent » coûte-t-il plus qu'il ne protège, s'il pousse à régler le pion par réflexion ?
3. En multijoueur, qui fait foi pour la campagne (or, naufrages) : l'hôte ou chaque client ?
