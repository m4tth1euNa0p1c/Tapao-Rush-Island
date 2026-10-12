# 37 — Gestion des erreurs, journaux et crashs
Domaine : D · Auditeur : ingénieur fiabilité

## Verdict
La logique est défensive (un seul `check`, une trentaine de replis « DA absent », erreurs typées sur les fragments), mais les coutures ne le sont pas : packaging, sauvegarde, écrans d'erreur, Live Coding. Six rapports de crash en 23 h, dont quatre dans un module patché. Risque commercial principal : un build packagé qui dégrade en silence.

## Constats
- **[Bloquant commercial] Neuf assets chargés par chemin en dur, probablement absents du cook**
  - Constat : `DA_TapaoUISkin`, `DA_FootstepRules`, `DA_ShoreAmbienceRules`, `DA_WindLeavesRules`, `DA_SettingsRules`, `DA_WaveRules`, `DA_HudTimerRules`, `DA_BoatStartRules`, `BP_Barque` ne sont référencés par aucun paquet. Sans skin, `UTapaoUISubsystem` s'arrête : ni menu, ni HUD, ni pause. Sans `BP_Barque`, acteur C++ nu.
  - Preuve : vu : 0 référence dans 2 011 paquets, aucun `DirectoriesToAlwaysCook` (`DefaultGame.ini`), `TapaoUISubsystem.cpp:84-89`, `BoatSubsystem.cpp:102-106` ; supposé : le cook les ignore, aucun build packagé.
  - Recommandation : `+DirectoriesToAlwaysCook`, spec `Boot.MandatoryAssets`, cook Development.
  - Effort : S
- **[Majeur] Crashs éditeur : quatre sur six dans un module patché par Live Coding**
  - Constat : 6 `UECC-*` du 2026-10-03 02:52 au 2026-10-04 02:05. Quatre piles contiennent `monke_island_patch_0/1` : deux destructeurs (`~ATikiCreature`, `~ATikiDirector`, à la fermeture) après des patchs « 4 classes changed » et « 3 classes new », contre la règle « header = éditeur fermé » ; un plantage au chargement du patch ; le 02:05, `CompileLiveCoding` en plein PIE (PIE ouvert depuis 3 min), le lambda Slate `STapaoHUD::MakeBar` plante aussitôt. Aucune règle ne lie Live Coding et PIE. Les deux autres : un ensure `AddDynamic` en double (`IslanderHealthComponent.cpp:22`) et un assert Slate dans un commandlet d'import.
  - Preuve : `Saved/Crashes/UECC-Windows-85EAD…` (`monke_island.log` l.2972-2985), `…7E849…`, `…90FA6…`, `…8E2DA…`.
  - Recommandation : refuser `CompileLiveCoding` si `IsPIERunning` ou si un `.h` change ; remplacer les lambdas capturant `this` des attributs Slate par des méthodes (`CreateSP`).
  - Effort : S
- **[Majeur] Aucun diagnostic de terrain en Shipping**
  - Constat : les 230 appels `UE_LOG` sont retirés par défaut, le Crash Reporter n'est pas inclus, aucune trace d'archivage des symboles, pas de résumé technique.
  - Preuve : `monke_island.Target.cs`, `DefaultGame.ini`, `journal.md`, grep `CrashContext|OnHandleSystemError` = 0 ; défauts moteur supposés.
  - Recommandation : logs Shipping en bêta, Crash Reporter ou Sentry, PDB archivés.
  - Effort : M
- **[Majeur] Sauvegarde de campagne non atomique, écrasée en silence**
  - Constat : `WriteSave` appelle `SaveGameToSlot` toutes les 30 s ; un slot illisible redevient « nouvelle campagne » puis est écrasé ; `Version` jamais lue ; aucun test. Le `USaveSubsystem` conforme (`.tmp`, `.bak`, 18 specs non exécutées) reste sur la branche `dev/T-057`.
  - Preuve : `TapaoUISubsystem.cpp:654-694`, `board.md` T-057, pas de `Save/` en principal.
  - Recommandation : fusionner T-057, y brancher TAPAO, exécuter `Save.Errors`.
  - Effort : M
- **[Majeur] Pas d'écran d'erreur ni de traitement des échecs réseau et de voyage**
  - Constat : les textes français existent (`GetErrorText`), aucun écran ne les affiche ; `Fail()` n'émet qu'un message de debug hors Shipping. Aucun `OnNetworkFailure`/`OnTravelFailure`, `GameInstance` brut : un hôte qui part renvoie au menu sans un mot (supposé) ; `RestartFromBoot` échoué laisse « les joueurs morts sur cette map ».
  - Preuve : `ExpeditionBootGameMode.cpp:112-138`, `ExpeditionGameState.cpp:361-376`, `DefaultEngine.ini:14`.
  - Recommandation : `UMonkeGameInstance` reliant ces délégués à un écran « Connexion perdue ».
  - Effort : M
- **[Majeur] DataAssets non validés : 2 sur 35 ont `IsDataValid`**
  - Constat : `ClampMin` ne protège que l'interface, or MCP et Python écrivent les DA. `AGalleonSpawner::FindAnchorage` boucle sur `Distance += SearchStep` : un pas ≤ 0 peut geler le jeu. La gate INT ne compte que Error et ensure : les replis « DA absent » (Warning) laissent passer un jeu sans villages, butin ni galion.
  - Preuve : `GalleonSpawner.cpp:70`, `BoatStartSubsystem.cpp:270` (borne son pas), `IslandCatalog.cpp:28`.
  - Recommandation : `IsDataValid` sur les DA vitaux, `ensureMsgf` pour `DA_TikiRules`, `DA_VillageRules`, `DA_GalleonRules`, `DA_LootRules`.
  - Effort : M
- **[Majeur] `AVillageSpawner::Build` : `LoadSynchronous` sans délai ni reprise**
  - Constat : chaque machine charge `DA_Villages_<FRG>` (meshes en références dures) sur le thread de jeu. Échec : log `Error`, `OnVillagesBuilt` jamais émis, ni reprise ni message : île sans villages. 15 `LoadSynchronous` dans 10 fichiers, que `ExpeditionIslandSubsystem.h` s'interdit.
  - Preuve : `VillageSpawner.cpp:196-201`, `VillageLayout.h:47`, `ExpeditionIslandSubsystem.h:28`.
  - Recommandation : chargement asynchrone borné par `LoadTimeoutSeconds`, avec statut.
  - Effort : M
- **[Majeur] Chemins d'échec asynchrones et d'interface non testés**
  - Constat : aucun groupe `*.Errors.*`, un seul `AddExpectedMessage` ; délais, annulations et double résolution de `ExpeditionIslandSubsystem` non testés ; `TapaoUISubsystem` et `VillageSpawner` sans spec d'échec.
  - Preuve : grep `AddExpected` (`EconomySpec.cpp:229`).
  - Recommandation : `FStreamableManager` simulé, une famille `Errors.*` par domaine.
  - Effort : M

## Manques
- Crash Reporter, symboles, `WBP_ErrorDialog`, `GameInstance` du projet.
- `IsDataValid` généralisé, test de packaging, catégories de log par domaine.

## Quick wins
1. `DirectoriesToAlwaysCook` pour les dossiers des neuf assets, puis cook de contrôle.
2. `ue-build/SKILL.md` : PIE actif ou `.h` modifié = pas de Live Coding.
3. `AddUniqueDynamic` partout.
4. Borner `SearchStep`, `IsDataValid` sur `UGalleonRules`.
5. `bUseLoggingInShipping` et Crash Reporter pour les builds de test.

## Questions qui challengent l'équipe
1. Avez-vous déjà cuit le jeu ? Que perd-on sans bruit (menu, barque…) ?
2. Qui peut patcher par Live Coding quand le PIE d'un pair tourne, et combien de crashs par jour acceptez-vous ?
3. Si le disque tronque la sauvegarde pendant l'autosave de 30 s, que voit le joueur ?
