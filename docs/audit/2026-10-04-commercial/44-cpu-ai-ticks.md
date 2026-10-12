# 44 — Coût CPU : IA, ticks, physique
Domaine : E · Auditeur : ingénieur perf CPU

## Verdict
Aucun chiffre CPU hors PIE (seule mesure : ~40 i/s en PIE solo avec 4 110 butins, T-108) et aucun niveau de détail : créatures, butins et cadavres tiquent, simulent et se répliquent à pleine cadence, même loin des joueurs. Supposé : 1–2 ms d'IA au repos, 3–5 ms en mêlée, 3–9 ms de réplication d'hôte, pour 8 ms de thread de jeu. Risque : un hôte de coop sous 60 i/s dès 4 joueurs.

## Constats

- **[Majeur] IA tiki : jamais endormie, des rayons à chaque image**
  - Constat : chaque créature (CMC, 2 meshes) tique à pleine cadence sur chaque machine (AlwaysTickPose, URO coupé, ni TickInterval ni dormance). 36–54 villageois (12 par village) et 9 gardiens sont créés sans condition de distance. Le pilotage direct (`MoveToward`) lance 2 rayons et 2 évaluations de houle par image, un guerrier engagé 4 ; les gardiens ne s'arrêtent jamais : ≥ 18 rayons/image. Supposé : 25–35 µs/créature au repos.
  - Preuve : TikiCreature.cpp:34,108,154–182 ; TikiWarrior.cpp:111,117,154 ; moteur 5.8 Character.cpp:125 ; T-064 « coût en temps d'image non mesuré ».
  - Recommandation : villageois créés à l'approche (200 m), retirés à 350 m ; eau en cache 0,2 s ; gardiens en pause ; URO ; ≤ 30 rayons IA/image.
  - Effort : M

- **[Bloquant commercial] Hôte : 5 000 butins, 80 cadavres permanents, physique en sous-pas**
  - Constat : chaque butin est un acteur simulé, répliqué à 100 Hz sans dormance : le moteur en considère ~60 % par image (≈ 3 000, supposé) ; en rafale le vent les tient éveillés à < 50 m. `bSubstepping` à 1/120 s = 2–3 pas à 60 i/s, 4–5 à 30. La mesure à 40 i/s n'a pas de référence sans butin.
  - Preuve : DefaultEngine.ini:118–122 ; moteur PhysicsSolverBase.cpp:607–617, NetDriver.cpp:5315–5443 ; PhysicsPropActor.cpp:12 ; TikiRules.h:163,187.
  - Recommandation : butins créés par rayon (< 150 m, pool), ≤ 800 corps éveillés ; dormance et 10 Hz (voir 34) ; cadavres figés après repos, plafond 30 ; sous-pas 1/90 à mesurer.
  - Effort : M

- **[Majeur] Vent : rendu recréé à chaque image, pics en phase**
  - Constat : (a) `BatchUpdateInstancesTransforms(…, true, …)` : en 5.8 les instances sont déjà marquées ; le `true` recrée en plus le proxy de rendu de chaque ISM à chaque image (feuilles, ambiance de village), même par calme. (b) `GetShelterFactor` part de Time = −1e6 : tous les acteurs recalculent dans la même image, toutes les 0,5 s ; 200–300 balayages d'un coup (supposé 5–10 ms) à chaque rafale.
  - Preuve : WindLeavesActor.cpp:258 ; VillageAmbienceActor.cpp:198,217 ; moteur ISMInstanceDataManager.cpp:853, ActorComponent.cpp:2667 ; WindStormActor.cpp:134,338–360, .h:22.
  - Recommandation : `false` ; décalage aléatoire, plafond de 16 balayages/image ; dérive et abri à < 100 m d'un joueur.
  - Effort : S

- **[Mineur] Parcours linéaires cachés**
  - Constat : chaque guerrier sorti, embuscade tentée et cadavre relance un `TActorIterator<AActor>` (≈ 0,1–0,4 ms, supposé) : un meurtre = 7 sorties = 3–5 ms en une image. Un ISM simple balaie toutes ses instances dans `GetInstancesOverlappingSphere` : ≈ 1 ms sur 39 k palmiers par noix (3/s en tempête) et par camp ou marae au lancement. En éditeur tout `TActorIterator` est O(acteurs) : le PIE surestime de 1–2 ms/image (supposé).
  - Preuve : TikiDirector.cpp:798,1142 ; TikiCorpse.cpp:205,354 ; CoconutFallDirector.cpp:142 ; SiteClearing.cpp:23,37 ; moteur InstancedStaticMesh.cpp:4683, EngineUtils.h:187.
  - Recommandation : registres (zones calmes, cadavres) ; grille spatiale des palmiers, comme `BushCells`.
  - Effort : S

- **[Mineur] Lancement : 150–300 ms synchrones en jeu, refaits par chaque client**
  - Constat : villages 29–30 ms mesurés, embuscades 12 ms, `CacheBushes` (tous les ISM, 1–2 passes, durée jamais relevée, payée par le client au premier tiki répliqué), butin par lots de 80 acteurs/0,05 s pendant 3 s (coût non mesuré).
  - Preuve : journal T-102, T-072, T-108 ; LootScatterSpawner.cpp:196,203 ; TikiDirector.cpp:890 ; MobHealthBarComponent.cpp:78.
  - Recommandation : budget en ms par image (comme `MeshBudgetMs` du Flood), travail sous l'écran de chargement, lots de 12.
  - Effort : M

- **[Majeur] Aucun budget par système**
  - Constat : `docs/perf/` n'existe pas ; les 8 ms de thread de jeu ne sont pas répartis ; aucun build packagé.
  - Preuve : performance.md §1–2.
  - Recommandation : budgets proposés (hôte, 4 joueurs, village, mêlée de 12, 4 000 butins) : IA ≤ 2 ms, Chaos ≤ 2, réplication ≤ 1,5, vent ≤ 0,3, directeur ≤ 0,2 (pic ≤ 2), lancement ≤ 100 ms en jeu.
  - Effort : S

## Manques
1. Marqueurs CSV/Insights (directeur, vent, IA) et mesure en build packagé.
2. Significance Manager, budget d'animation, sommeil des villages.
3. Création par rayon ; paliers de scalabilité (densité PNJ, butins) dans les réglages §18.

## Quick wins
1. `false` sur les `BatchUpdateInstancesTransforms` (S).
2. Décalage des balayages d'abri (S).
3. Registre pour `FindQuietZones` et `KeepCount` (S).
4. Gardiens : pause et cache de profondeur (S).
5. Build Development packagé, trois captures (S).

## Questions qui challengent l'équipe
1. Quel FPS l'hôte tient-il en build packagé, 4 joueurs, dans un village ?
2. Un village à 400 m de tout joueur doit-il exister (tick, physique, réplication) ?
3. 5 000 butins et 80 cadavres permanents : piliers de design ou coûts ? Quelle machine minimale ?
