# Normes de performance et d'optimisation

Le cahier §12 fixe la cible : 60 FPS en 1080p sur une machine de référence à préciser au L0. Le scénario de mesure comprend 4 joueurs, 50 ressources actives, un requin et les ennemis des trois zones. On y rapporte :
- la médiane et le 1 % bas ;
- la configuration graphique ;
- le CPU, le GPU et la RAM ;
- la bande passante.

L'objectif n'est jamais déclaré atteint avant d'avoir été mesuré.

**Références** :
- documentation Epic *Performance and Profiling* : Unreal Insights, commandes `stat`, CSV Profiler, GPU Visualizer ;
- outils GPU : RenderDoc, PIX, NVIDIA Nsight ;
- méthode : mesure, hypothèse, un seul changement, nouvelle mesure.

## 1. Budgets
Ce sont des valeurs de départ, à recalibrer au L0 sur la machine de référence.

| Poste | Budget | Remarque |
|-------|--------|----------|
| Image (médiane) | ≤ 16,6 ms (60 FPS) | scénario du §12 |
| Image (1 % bas) | ≤ 22 ms (≥ 45 FPS) | proposé, à valider avec l'utilisateur |
| Thread de jeu | ≤ 8 ms | gameplay, IA, animation, physique côté jeu |
| Thread de rendu | ≤ 8 ms | visibilité, appels de dessin |
| GPU | ≤ 14 ms | garde 2,6 ms de marge pour les pics |
| GPU : Lumen | ≤ 4 ms | GI et réflexions, eau comprise |
| GPU : Nanite et passe de base | ≤ 4 ms | |
| GPU : ombres (VSM) | ≤ 3 ms | |
| GPU : eau et translucides | ≤ 1,5 ms | un seul plan Single Layer Water |
| GPU : post-traitement et TSR | ≤ 1,5 ms | |
| Réseau, par client | ≤ 16 Ko/s en moyenne, ≤ 64 Ko/s en pic | à mesurer au L1 |
| Chargement du hub vers une expédition | ≤ 20 s visé, 60 s au maximum (§16) | sur SSD |
| Personnage (§10) | 30 000 à 60 000 triangles en LOD0, textures 2K, 2 à 4 matériaux | |

Les budgets mémoire sont dans `memory.md`.

## 2. Méthode obligatoire
1. **Mesurer d'abord.** Prendre une référence de départ (même map, même point de vue, même configuration, même durée), avec `/ue-profile`.
   - Les chiffres de référence viennent d'un build packagé en Development ou en Test.
   - Les mesures en `-game` non cooké ne servent qu'à comparer.
   - Faire 3 captures et retenir les médianes.
2. **Trouver le goulot** (jeu, rendu, GPU ou RHI) :
   - `stat unit`, `stat unitgraph` ;
   - Unreal Insights avec `-trace=default` (ajouter `-statnamedevents` pour plus de détail) ;
   - `ProfileGPU`, `stat gpu`, `stat Nanite`, `stat SceneRendering`, `stat Streaming` ;
   - au besoin, RenderDoc, PIX ou Nsight.
3. **Une hypothèse, un changement, une nouvelle mesure.** On ne garde le changement que si le gain dépasse le bruit entre les captures.
4. **Rapport** dans `docs/perf/<date>-<sujet>.md`, avec le modèle `docs/templates/perf-report.md`.
5. **Régressions** : une tâche qui dégrade de plus de 5 % la médiane ou le p95 de l'image sur le scénario de référence est refusée en REC ou INT, sauf ADR.

## 3. Leviers selon le goulot
- **Thread de jeu** :
  - tick désactivé ou espacé (`PrimaryActorTick.TickInterval`), timers ou événements ;
  - Significance Manager, Animation Budget Allocator, URO sur les maillages squelettiques ;
  - références de composants mises en cache, pool d'acteurs ;
  - aucune allocation à chaque image (`memory.md`) ;
  - collisions simples, corps physiques endormis.
- **Thread de rendu** :
  - Nanite pour le statique ;
  - instanciation (ISM, HISM, PCG) pour ce qui se répète ;
  - moins de permutations de matériaux ;
  - World Partition avec HLOD sur les grandes maps.
- **GPU** :
  - qualité Lumen par niveau de scalabilité ;
  - VSM : peu de lumières dynamiques, pas d'ombre sur les petits objets ;
  - surcharge de translucides (vue *Shader Complexity*) ;
  - distances de culling et `MaxDrawDistance` sur les petits props ;
  - densité PCG plafonnée ;
  - baisse de l'échelle de rendu (TSR) en dernier recours.
- **Chargement et streaming** : chargement asynchrone, références souples, pool de streaming des textures (`memory.md`).
- **Réseau** :
  - pertinence, `NetCullDistanceSquared`, `NetUpdateFrequency`, dormance ;
  - quantification des propriétés répliquées ;
  - jamais de RPC fiable à chaque image (§9).
- **Contenu** : LOD et repli Nanite, textures de 2K au maximum pour un prop (au-delà, ADR), nombre d'instances plafonné par acteur PCG.

## 4. Outils du projet
- `scripts/ue.ps1 profile -Map <map> -Frames <n>` produit un CSV, résumé par `scripts/perf_summary.py` : moyenne, p50, p95, p99, maximum.
- Les courbes s'obtiennent avec `Engine/Binaries/DotNET/CsvTools/CSVToSVG`.
- Pour la mémoire, voir `memory.md`. Pour le réseau, utiliser Networking Insights (`-trace=default,net -NetTrace=1`).

## 5. Définition de fini en performance
- Chiffres réels pour le scénario de référence :
  - médiane, p95, p99 et 1 % bas ;
  - configuration ;
  - CPU, GPU et RAM ;
  - bande passante en multijoueur.
- Budget tenu, ou dépassement inscrit dans les problèmes connus avec un plan de correction.
- Aucune optimisation n'est livrée sans mesure avant et après.
