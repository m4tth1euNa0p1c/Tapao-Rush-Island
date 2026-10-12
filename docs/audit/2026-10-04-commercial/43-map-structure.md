# 43 — Structure des maps : monolithe ou fragments
Domaine : E · Auditeur : level tech

## Verdict
Le choix « une carte par fragment, sans World Partition ni HLOD » est correct, mais il reste un ADR : aucun `L_Frag_*` sur disque, T-089 borné à FRG_01, décision WP sans seuil. L'outillage pèse plus que le streaming : carte binaire hors OFPA, niveau ouvert global dans l'éditeur partagé, noyau d'expédition en Level Instance au chargement non garanti. Principal risque commercial : générer 20 cartes avant de tester ce noyau à deux machines, puis tout refaire (coût du monolithe : 42, 47).

## Constats
- **[Majeur] World Partition et HLOD : non, mais la décision n'est pas fermée**
  - Constat : T-015 (« WP pour Lvl_Raraka ») reste à faire, T-091 doit « décider » sans seuil, et la spec annonce ≈ 3 × 3 km alors que FRG_01 mesure 3,9 × 5,0 km (moyenne 18 km² pour 36 ha de terre, 2 %). La terre jouable (≤ 71 ha, 1,3 à 2,3 km) tient dans un seul rayon de streaming : WP chargerait tout, en permanence, et rendrait invisibles au MCP les acteurs non chargés (supposé).
  - Preuve : board.md:22,98, fragments-tuamotu.md:311, fragments.json `box_cm`.
  - Recommandation : fermer T-015. Seuils de T-091 : chargement ≤ 20 s à froid, RAM ≤ 8 Go (performance.md:32, memory.md:46), WP seulement si l'un saute. HLOD sans objet : l'horizon de 12 k triangles en tient lieu.
  - Effort : S

- **[Majeur] Migration bornée à FRG_01, sans cible ni critère de sortie**
  - Constat : T-089 ne traite que FRG_01 et ADR-004 renvoie les 19 autres à L8 : `Lvl_Raraka` reste jouable. Ni la liste des acteurs d'une carte de fragment (voir LI), ni la date de gel de `Lvl_Raraka` ne sont fixées. T-088 et T-113 se recouvrent.
  - Preuve : board.md:95-96,120, setup_fragments.py:44, ADR-004:29.
  - Recommandation : cible `L_MainMenu` → `L_Galleon` ⇄ `L_Frag_Raraka_01..20`, sans WP, en OFPA (transitions : 21, 32). Ordre : FRG_01 avec les règles ci-dessous ; sortie = PIE à 2 et `-game` sans erreur, puis T-091 packagé ; 19 cartes générées en lot ; `Lvl_Raraka` gelée et sortie du cook (repointer `DA_Island_*.Map`, `BP_BootGameMode.FallbackMap`) ; T-088 fusionné dans T-113.
  - Effort : L

- **[Majeur] Éditeur partagé : cartes du projet hors OFPA, niveau ouvert = état global**
  - Constat : `__ExternalActors__` ne couvre que le template. `Lvl_Raraka.umap` : 110 Mo, 4 versions LFS en 21 h (437 Mo). Ouvrir `L_Frag_Raraka_01` redirige les appels MCP des autres sessions ; 3 scripts seulement vérifient le niveau ; le MCP ne crée pas de niveau (duplication de `Template_Default`).
  - Preuve : pointeurs LFS (`git show`), setup_fragment_map.py:320, ue-editor/SKILL.md:41,44.
  - Recommandation : `bUseExternalActors` (moteur, Level.h:445, valable hors WP) avant le premier acteur de chaque carte neuve ; générer les cartes éditeur fermé (`-ExecutePythonScript`), jamais éditées à la main ; propriétaire par dossier de fragment.
  - Effort : M

- **[Majeur] LI_ExpeditionCore : contenu flou, ordre de chargement non garanti**
  - Constat : l'ADR met l'eau dans la LI, le script la pose dans chaque carte. `UIslandWaterSubsystem` collecte les surfaces une seule fois au BeginPlay, or une LI charge par la file `UpdateStreamingState` et `AGameModeBase::StartPlay` n'attend pas le streaming. `BP_VillageSpawner`, répliqué, est prévu dans la LI ; le `.umap` pose ≈ 10 classes de gameplay à la main (supposé).
  - Preuve : ADR-004:26, IslandWaterSubsystem.cpp:40-48 ; moteur 5.8 : LevelInstanceSubsystem.cpp:268-360, GameModeBase.cpp:204.
  - Recommandation : LI = visuel statique (ciel, soleil, brouillard). Eau, spawners, directors, acteurs répliqués : une amorce serveur unique, pas 20 cartes à patcher. Plan B : carte noyau persistante, terrain en sous-niveau chargé par `?Island=`.
  - Effort : M

- **[Majeur] Marge de mer à 4,59 m : la moitié des tuiles est du fond profond**
  - Constat : 309 tuiles, 32,5 M triangles ; 157 tuiles (13,0 M) sont entièrement sous −10 m, alors que la barque arrive à 200–350 m de la plage et le galion mouille à 324–413 m (collision : voir 42). Le script laisse la navigation active (T-018 l'avait coupée) et le PCG en `GenerateOnLoad`, qui régénère à la charge tout composant non généré.
  - Preuve : georef.json ×20 (calcul), fragments-tuamotu.md:72, setup_fragment_map.py:211,325,373, moteur PCGComponent.h:327.
  - Recommandation : décimer la marge (pas de 4 px, ≈ −37 % de triangles), navigation coupée, PCG en `GenerateOnDemand`, avant 309 imports MCP.
  - Effort : S

## Manques
- Contenu type d'une carte de fragment (acteurs posés ou créés), budget automatique (taille, instances, triangles), test de contenu : `monke_island.Fragments` ne valide que les DataAssets.

## Quick wins
1. Fermer T-015 et écrire les seuils dans T-091.
2. `bUseExternalActors` sur `L_Frag_Raraka_01` et `LI_ExpeditionCore` avant tout acteur.
3. `setup_fragment_map.py` : navigation coupée sur les tuiles, PCG en `GenerateOnDemand`.
4. Figer chemins et préfixes : spec:10 ≠ script:40 ≠ asset-naming.md:6 (`Environment/` ou `Map/`, `L_` ou `Lvl_`).

## Questions qui challengent l'équipe
1. Noyau d'expédition : posé dans 20 cartes (LI) ou créé par le serveur ? Tant que ce n'est pas tranché, chaque carte générée fige l'état actuel.
2. À quoi sert une marge fine de 1,5 km à 4,59 m si le galion mouille à 324–413 m de la plage et l'horizon commence à 1,5 km ?
3. Les tuiles OBJ viennent-elles du terrain ou des limites du MCP (pas d'outil Landscape, SKILL.md:41) ? Un Landscape par fragment allégerait-il collision et marge ?
