# 36 — Qualité des tests et stratégie QA
Domaine : D · Auditeur : ingénieur QA et tests automatisés

## Verdict
La logique pure est bien testée : 306 `It()` rapides et déterministes. Mais aucun test ne touche un acteur, un RPC, une carte, un build packagé ou la performance ; 5 des 52 critères de recette (§12, §23, §24.9) sont automatisés (n°13 à 16, RUN14). Risque commercial : un jeu coop dont la réplication n'a jamais été exercée automatiquement.

## Constats

- **[Bloquant commercial] Multijoueur : zéro test, un essai manuel**
  - Constat : 30 RPC, 27 classes répliquées, 77 `DOREPLIFETIME` ; aucune spec ne crée monde, acteur ni client. Le seul essai à deux documenté (PIE, T-095) a perdu le client au démarrage ; le correctif reste « NON retesté ». Seamless travel limité en PIE.
  - Preuve : 0 occurrence de `NM_|HasAuthority|SpawnActor|CreateWorld` dans `*/Tests/` ; journal.md:253-255 ; ADR-005:50.
  - Recommandation : `scripts/nettest.ps1` : hôte + 2 clients `-game -nullrhi` ; scénarios n°2, n°3, voyage ×3, n°14, puis émulation réseau 200 ms / 2 % de pertes (n°10). Détail : 58.
  - Effort : M

- **[Majeur] Six dossiers sans test, ratio faible ailleurs**
  - Constat : lignes de test / code = 0,095 (4 443 / 46 673). `Interaction` (prise/dépôt), `Galleon`, `Camps`, `Character`, `Emotes`, `Marae` : 5 988 lignes, 14 % du code propre, 0 test. Creatures, Weapons, UI : 0,06–0,07. `TikiDirector.cpp` (1 344 lignes) : fonctions pures seulement. Menus TAPAO (UI01–UI12) : rien.
  - Preuve : comptage par dossier ; `IslanderGrabComponent.cpp` 567 lignes, 5 RPC.
  - Recommandation : specs d'acteur en monde transitoire (`UWorld::CreateWorld`), d'abord prise/dépôt ; ARC-09 appliqué en revue.
  - Effort : M

- **[Majeur] Tests verts, fonctions non branchées ; vrais tests d'acteur hors main**
  - Constat : `FRunCalculator` et `FEconomyCalculator` (29 tests) n'ont aucun appelant hors `Run/` ; EconomySpec teste surtout le modèle bois/nourriture rendu facultatif par l'ADR-005. Les seules specs d'acteur (T-054, T-055) vivent hors main, avec celles de T-057 : 69 specs, 1 671 lignes, « NON exécutées ». La sauvegarde livrée (`UTapaoSaveGame`) n'a aucun test.
  - Preuve : `git branch --no-merged main` ; board T-054/055/057 ; TapaoUISubsystem.cpp:658-690.
  - Recommandation : exécuter ces branches sur une branche d'intégration (voir 11) ; migrer EconomySpec vers le quota en valeur.
  - Effort : M

- **[Majeur] « 296/296 » est un texte, pas un artefact**
  - Constat : le board cite 296/296 (00:33) ; le code compte 306 `It()` (Run 9 et Tiki 1 lancés isolément). Le seul rapport conservé couvre 9 tests ; chaque run l'écrase (supposé).
  - Preuve : board.md:107-111 ; `Saved/Automation/Reports/index.json`.
  - Recommandation : `ue.ps1 test -Full` archive `index.json` + sha ; échec si réussis < nombre de `It(` du code.
  - Effort : S

- **[Majeur] Pas de CI : l'INT est manuel et bloqué par l'éditeur**
  - Constat : `worktree.py merge` fusionne sans build ni test ; l'INT exige l'éditeur fermé (mutex Live Coding), d'où trois branches en attente. Ni remote, ni `.github` ; hooks Git = LFS.
  - Preuve : worktree.py:117,127-141 ; FACTORY.md:27.
  - Recommandation : `scripts/ci.ps1` dans un clone complet (`BoatPhysicsSpec.cpp:214` charge `SM_Barque`) : build éditeur, suite complète (≈ 30 s estimées), Planificateur de tâches la nuit ; `merge` appelle une version rapide.
  - Effort : M

- **[Majeur] Les données livrées ne sont jamais testées**
  - Constat : les specs lisent les défauts C++ (`GetDefault`, `NewObject`), jamais les 76 `DA_*.uasset` où vivent les vraies valeurs ; les 20 `DA_Island_Raraka_NN` ne sont jamais validés.
  - Preuve : FragmentsSpec.cpp:13 ; un seul `LoadObject` dans les tests.
  - Recommandation : spec `Data.Validation` : registre d'assets, invariants du cahier (quotas, 20 fragments valides, cartes existantes).
  - Effort : S

- **[Majeur] Performance et packaging jamais mesurés**
  - Constat : `ue.ps1 profile` vise `Lvl_FirstPerson` ; aucun `Saved/Profiling` ; aucun test n'asserte un budget. Pas de `build/`, pas de section de packaging dans DefaultGame.ini, 0 tâche sur 117 « testé build ».
  - Preuve : ue.ps1:27 ; DefaultGame.ini.
  - Recommandation : `profile` sur `Lvl_Raraka`, 3 fragments, échec si médiane > 16,6 ms ; premier `package -Config Development` + smoke `monke_island.exe -nullrhi`.
  - Effort : M

- **[Mineur] Hygiène des specs**
  - Constat : 30 specs sur 30 en `EditorContext | ProductFilter` (ni `SmokeFilter`, ni `ClientContext`) ; 0 chemin `.Errors.*` ; recette citée dans 5 fichiers sur 29 ; un point coupe le nom (« 9 (RUN14) »).
  - Preuve : RunSpec.cpp:77 ; `index.json`.
  - Recommandation : préfixes `[RUN14]`, `SmokeFilter`, matrice de recette générée depuis `index.json`.
  - Effort : S

## Manques
Pyramide cible :
1. N0 specs pures (306) : conserver.
2. N1 acteurs en monde transitoire (modèle déjà dans SermonSpec, CargoLedgerSpec).
3. N2 `AFunctionalTest` sur une carte `L_TestBed` : barque, sermon, tiki.
4. N3 hôte + 2 clients `-game`.
5. N4 build packagé ; Gauntlet (plugin expérimental du moteur) et AutomatedPerfTesting après un packaging stable.

Aussi : AutomationDriver (UI01–UI05), couverture mesurée (OpenCppCoverage).

## Quick wins
1. Garde-fou du nombre de tests, archivage de `index.json`.
2. `ue.ps1:27` : carte par défaut `Lvl_Raraka`.
3. Spec `Data.Validation` sur les 20 îles.
4. `SmokeFilter` et préfixes `[RUNnn]`.
5. Lancer les 69 specs des trois branches.

## Questions qui challengent l'équipe
1. Qui lance la suite complète avant chaque fusion quand l'éditeur l'en empêche ?
2. Quelle preuve de réplication fait foi avant L7, sans seconde machine ?
3. À quelle date le premier build packagé est-il joué ?
