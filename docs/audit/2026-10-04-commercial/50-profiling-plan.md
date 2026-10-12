# 50 — Plan de profilage et métriques
Domaine : E · Auditeur : ingénieur performance (plan de profilage)

## Verdict
Aucune mesure n'existe (ni `docs/perf/`, ni `Saved/Profiling/`, ni build packagé), et `ue.ps1 profile` avec `perf_summary.py` ne peut pas valider le §12. Les outils sont installés (CSV Profiler, PerfreportTool, Insights) ; manquent machine de référence, scénarios, instrumentation et seuils. Risque commercial : découvrir au L6 que le monolithe (110 Mo, 816 000 instances, Lumen) ne tient pas 60 FPS à 4 joueurs.

## Constats

- **[Bloquant commercial] Ni machine de référence ni métriques figées**
  - Constat : le §12 exige médiane, 1 % bas, CPU/GPU/RAM et bande passante sur une machine « à préciser au L0 » ; rien n'est fixé, T-091 est `a-faire`.
  - Preuve : CAHIER §12 l.268 ; board T-091 ; WMI (vu) : Core Ultra 7 265F, RTX 5060 Ti, 31,6 Go, NVMe.
  - Recommandation : Réf = ce PC en High 1080p ; Plancher = Deck ou PC modeste (audit 26). Budgets de `performance.md` au plancher, ÷ k sur Réf (k mesuré). Métriques : p50/p95/p99, 1 % bas (moyenne des 1 % pires images), saccades/min (> 33, 100 ms), threads, passes GPU, RAM/VRAM max, Ko/s par client, chargement.
  - Effort : S

- **[Majeur] Les outils actuels ne mesurent pas le scénario du §12**
  - Constat : `profile` capture dès le démarrage, non cuit, en une passe, plafonné à 60 images/s, à heure de jeu aléatoire. `perf_summary.py` ignore 1 % bas, saccades, `_CriticalPath` (goulot), passes GPU, baseline, code de sortie.
  - Preuve : ue.ps1:126-129 ; CsvProfiler.cpp:5177 ; SettingsRules.h:62 → GameUserSettings.cpp:537 ; DayNightRules.h:51 ; perf_summary.py:11,38-51.
  - Recommandation : `-benchmark -fps=60 -novsync` (pas fixe sans attente, graines fixes, hors Shipping : UnrealEngine.cpp:3005) ; `-ExecCmds="monke.SetHour 10,CsvProfile EXITONCOMPLETION,CsvProfile FRAMES=n"` (plus de boot capture) ; `-csvCategories=ChaosPhysics,Animation,VSM,GPUMem` ; 20 s d'échauffement retirées, passe 0 jetée, 3 passes, délai maximal. Rapports par PerfreportTool (installé) ; `perf_summary.py` réduit à une porte `--baseline --budgets`.
  - Effort : M

- **[Majeur] Systèmes maison invisibles au profileur**
  - Constat : 7 points d'instrumentation dans 3 fichiers (crue, pas), aucun `CSV_*` : IA tiki, barque, cadavres, butin et vent inattribuables.
  - Preuve : grep `Source/` : FloodSimulationActor.cpp, FloodSeaRiseSubsystem.cpp, FootstepSubsystem.cpp.
  - Recommandation : catégorie CSV `Monke`, `CSV_SCOPED_TIMING_STAT` sur ~10 chemins chauds ; A/B existants : `monke.WindLeaves`, `.Footsteps`, `.ShoreAmbience`, `.Waves`.
  - Effort : S

- **[Bloquant commercial] Aucun scénario, bot ni caméra scriptée**
  - Constat : le scénario §12 (4 joueurs, 50 ressources actives, requin, ennemis des 3 zones) n'existe pas ; fragments encore dans `Lvl_Raraka` ; navigation du terrain désactivée (T-018) : bots par waypoints.
  - Preuve : aucun `Bench` ni bot dans `Source/` ; T-095.
  - Recommandation : `monke.Bench <fragment> <scénario>` : pion scripté, marche réelle sur 6 points (pont, plage, village, cocoteraie, marae, lagon), 30 s chacun, `CSV_EVENT` par point. S1 = §12 : hôte + 3 clients `-nullrhi` par waypoints, 50 props éveillés, requin, tiki des 3 zones, `monke.FloodNow`. Balayage des 20 fragments (2 points), matrice complète sur les 3 pires (lumières), refaite sur `L_Frag_*` (T-091).
  - Effort : M

- **[Majeur] Hôte, réseau, mémoire et chargement sans protocole**
  - Constat : multijoueur jamais lancé sur deux machines ; budgets réseau (16/64 Ko/s), RAM 8 Go, VRAM 6 Go, chargement 20 s, fuite ± 2 % : méthodes décrites, jamais exécutées.
  - Preuve : performance.md:31-32 ; memory.md:46-49 ; NetDriver.cpp:6126.
  - Recommandation : `-ReplicationCSVCaptureFrames=n` et Network Insights (performance.md §4), hôte sur Réf, clients sur le 2e PC, profils §12 n°10, temps réel, sans `-benchmark`. `memreport -full` à T0, fin de manche, après 3 cycles galion → île → galion ; `nvidia-smi` à 1 Hz ; chrono `ServerTravel` → dernier `BeginPlay`, à froid et à chaud.
  - Effort : M

- **[Majeur] Seuils sans garde-fou de bruit, rien d'automatisé**
  - Constat : « +5 % refuse » sans mesure du bruit ; éditeur partagé, UBT et Blender faussent les captures ; ni cadence ni historique.
  - Preuve : performance.md:49 ; ue.ps1:62.
  - Recommandation : porte = médiane de 3 passes, valide si dispersion ≤ 3 % et écart ≥ 0,3 ms ; échec (seuils proposés) : p50 +5 %, p95 +8 %, p99 +10 %, threads/GPU +5 %, RAM +3 %, réseau +10 %, saccade > 100 ms ; refus de lancer si éditeur, UBT ou Blender tournent. N1 à chaque INT (`-nullrhi`, CPU seul, ~6 min estimées) ; N2 de nuit sur Réf (20 fragments, ~2 h estimées) ; N3 à la gate PROD (build Development, `ue.ps1 package`). `stat unit` et Insights : diagnostic, jamais une porte. Baselines JSON versionnées.
  - Effort : M

## Manques
1. Build packagé Development (aucun `build/`).
2. `monke.Bench`, bots et clients headless, balayage de caméra.
3. Machine plancher ; spécifications du 2e PC.

## Quick wins
1. `ue.ps1 profile` : carte `Lvl_Raraka?Island=FRG_Raraka_01` par défaut, flags ci-dessus, délai maximal (2 h).
2. Baseline 0, éditeur fermé, 3 passes sur 3 fragments, comparatif seulement (½ j).
3. `perf_summary.py` : 1 % bas, saccades/min, `_CriticalPath` (3 h).

## Questions qui challengent l'équipe
1. Quel matériel plancher vendez-vous, et qui le possède ? Si c'est le PC du propriétaire, « 60 FPS » ne prouve rien.
2. Gèlerez-vous le contenu jusqu'à la baseline, et couperez-vous ce qui dépasse (World Partition T-015, villageois, lumières) ?
3. Un agent peut-il bloquer l'INT d'une demande du propriétaire parce qu'elle coûte 5 % d'image ?
