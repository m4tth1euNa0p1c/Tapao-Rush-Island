# 47 — Temps de chargement
Domaine : E · Auditeur : ingénieur chargement

## Verdict
Rien n'a été mesuré hors éditeur : le seul chiffre (LoadMap 1,6 s) vient d'un PIE qui copie un monde déjà en mémoire. Le goulot n'est pas la construction (villages, dégagement : 25–40 ms) mais la map monolithique (1 062 composants d'instances), ~730 Mo de contenu partagé, l'absence de préchargement et de PSO embarqués. Risque : premier lancement à froid sur disque dur bien au-dessus de 30 s (supposé).

## Constats
- **[Bloquant commercial] Chaque expédition charge l'atoll entier**
  - Constat : les 20 `DA_Island_Raraka_NN` ouvrent `Lvl_Raraka` (110 Mo, 816 000 instances PCG sérialisées, 97 % du fichier en 1 062 composants ISM) ; FRG_01 n'en garde que 36 766 (4,5 %). Aucune `L_Frag_*` sur disque.
  - Preuve : en-têtes `.uasset`/`.umap` ; `vegetation.json` de FRG_01 ; ADR-004 §1, §7.
  - Recommandation : ne livrer que des `L_Frag_*` (catalogue limité aux fragments coupés), retirer `Lvl_Raraka` du cuit ; T-015 sans objet.
  - Effort : L
- **[Majeur] La scission ne réduit la fermeture que de ~20 %**
  - Constat : références dures de la map : 1 047 paquets, 918 Mo non cuits (borne haute). Sans map (110) ni terrain (76), restent ≈ 730 Mo partagés : végétation 181, camps 86, sols 79, villages 74, marae 54. Les 122 Mo de DataTables PCG (souples) restent cuits sans être lus (supposé).
  - Preuve : imports des `.uasset`.
  - Recommandation : budget de fermeture par map contrôlé par script (≤ 800 Mo, ≤ 1 000 paquets) ; `DirectoriesToNeverCook` pour les DataTables.
  - Effort : M
- **[Majeur] Aucune mesure hors éditeur**
  - Constat : le PIE duplique le monde (0,34 s) puis le met en jeu (1,05–1,18 s), sans lecture disque ; `ue.ps1 package` ne passe ni `-compressed` ni ordre d'ouverture.
  - Preuve : `monke_island-backup-2026.10.04-00.39.34.log:4017-4065` ; `scripts/ue.ps1:140-141`.
  - Recommandation : package Development, 10 lancements à froid (SSD, HDD USB), Unreal Insights `loadtime`, `LogMonkeLoad` ; base de T-091.
  - Effort : S
- **[Majeur] 22 chargements synchrones, dont celui des villages**
  - Constat : 22 `LoadSynchronous`/`LoadObject` dans 16 fichiers ; l'asynchrone ne sert qu'au catalogue et à un widget. Pour les villages, le chrono des 25–40 ms démarre après le chargement (`:203` après `:196`), ≤ 3 ms d'après les horodatages car le kit est résident (la map l'importe). Dans une `L_Frag` sans import, le DA tire 169 paquets, 80 Mo, sur le thread jeu et chez chaque client à la réplication d'`ActiveFragment`.
  - Preuve : grep `Source/monke_island` ; `VillageSpawner.cpp:117-124,196,203` ; MEM-06.
  - Recommandation : DA en référence dure de la map (un fragment par map) ; bundle « Monde » dans `UExpeditionIslandDefinition`, chargé au tirage, poignée gardée jusqu'au voyage ; vérifier que `LoadMap` réutilise le paquet.
  - Effort : M
- **[Mineur] Construction et dégagement : ne pas optimiser**
  - Constat : villages 25–40 ms (5 mesures, 527–622 instances), TikiDirector 41 ms : ≈ 0,1 s, soit 1 % de 15 s.
  - Preuve : `monke_island-backup-2026.10.04-00.39.34.log:4050-4059` ; `VillageSpawner.cpp:224-310`.
  - Recommandation : cuire le dégagement par fragment (`--apply-clearing` existe).
  - Effort : M
- **[Majeur] Shaders et PSO : précaching moteur seul**
  - Constat : aucun cache embarqué (0 `*.upipelinecache`) ; SM6 et SM5 (D3D11) compilés ensemble, `r.RayTracing`, Substrate, VSM ; 103 maîtres de matériaux ; premier lancement : compilation en jeu (supposé).
  - Preuve : `DefaultEngine.ini:32-36,47-51` ; `monke_island.log:546,1090,1142`.
  - Recommandation : `-logPSO` en recette, fusion, embarquement ; précompiler derrière le menu et le rideau ; écran « Préparation des shaders » au 1er lancement (≤ 60 s) ; retirer SM5/D3D11 ; RT optionnel.
  - Effort : M
- **[Majeur] Disque dur : ≈ 1 050 paquets, sans ordre d'ouverture ni test**
  - Constat : sur HDD le coût suit les recherches (≈ 10 ms chacune) et le streaming Nanite, pas les octets (supposé) ; à 4 joueurs le plus lent fixe le départ.
  - Preuve : fermeture ci-dessus.
  - Recommandation : IoStore compressé, `FileOpenOrder`, machine HDD de recette, « SSD recommandé » sur Steam.
  - Effort : M

**Cible et plan** (net hors voile, P95 de 10 lancements à froid ; `performance.md:32` visait 20 s SSD) : SSD < 15 s, HDD < 30 s. SSD / HDD : du clic JOUER au pont (hub léger) 3,5 / 7,5 s ; de l'ancre levée à l'île jouable 11 / 22 s, soit lecture 4 / 12, mise en jeu et BeginPlay 1 / 1,6, PSO et échauffement sous le rideau (Nanite, textures, Lumen) 3 / 5, réseau et marge 3 / 3,4. Ordre : mesurer, fragments, précharger (hub derrière le menu, île suivante en zone sûre : jambe ≈ 5 s ; hub pendant la traversée), PSO, HDD.

## Manques
1. Journal de chargement, budget de fermeture.
2. Pipeline PSO, écran de premier lancement.
3. Préchargement, « monde prêt » (voir 21), machine HDD.

## Quick wins
1. Chrono avant `LoadSynchronous` (`VillageSpawner.cpp:203`), `LogMonkeLoad`.
2. Retirer D3D11/SM5 (`DefaultEngine.ini:50-51`).
3. `ClientAddTextureStreamingLoc` à l'arrivée, sous le rideau.
4. Premier package Development (`-compressed`, `ue.ps1:141`) : chiffre à froid.

## Questions qui challengent l'équipe
1. Combien de secondes pour une première partie, packagé, disque dur, 1er lancement compris ? « SSD requis » au-delà de 30 s ?
2. Pourquoi cuire `Lvl_Raraka` et 122 Mo de DataTables, l'ADR-004 §7 n'y jouant plus ?
3. Pourquoi un chargement de map par manche, plutôt qu'un océan persistant et l'île en sous-niveau streamé ?
