# 04 — L'avocat du diable : la version qui survit à ses propres faiblesses

Fondateur grillé × juriste données × géomorphologue côtier. Lecture seule (HEAD `45437d7`, assets LFS illisibles). Légende : 🔎 observé (fichier ou URL) · ⚠ estimé · ✖ supposé.

## 0. Démolition préalable

1. **Le relief d'un atoll est sous le bruit des sources libres.** Les motu de Raraka font 2–4 m, valeurs inventées dans `build_terrain.py:46-59`. Copernicus GLO-30 est un modèle de **surface** radar à 30 m, calé sur EGM2008 et non sur le niveau moyen local ; son masque d'erreur va de 0,09 à 43 m par pixel, et FABDEM, dérivé de GLO-30, sous-estime le terrain île par île aux Philippines (🔎 https://developers.google.com/earth-engine/datasets/catalog/COPERNICUS_DEM_GLO30_2024_1, https://www.gim-international.com/content/article/deltadtm-mapping-coastal-terrain-elevation). Sous cocoteraie, le radar mesure la canopée ; un motu de 300 m fait 10 pixels. ICESat-2 est précis mais clairsemé (traces espacées, nuages) ; validations trouvées sur glace et au Brésil, pas sur atoll (🔎 https://www.earthdata.nasa.gov/fr/data/catalog/nsidc-cprd-atl03-005). La bathymétrie Sentinel-2 (10 m/px) ne voit ni un hoa de 20 m ni une crête de 2 m. Litto3D (Etalab) couvre Moorea, Bora Bora, une partie de Tahiti ; **aucun produit Tuamotu trouvé** (🔎 https://services.data.shom.fr/geonetwork/1/api/records/BATHYMETRIE_L3D_LIDAR_POLYNESIE_MOO_2015.xml). ⚠ Conclusion : la **forme en plan** d'un atoll est réelle (côte Etalab), le **relief vertical** ne l'est pas et ne le sera pas. Un « pack de lieux réels » vend une précision illusoire, que l'acheteur ne voit même pas (audit 73, Q3).
2. **Les licences ne sont pas qu'une attribution.** Copernicus DEM impose crédit DLR/Airbus et un déni de responsabilité à redistribuer (🔎 même fiche). WorldCover est CC BY 4.0. **OSM est ODbL** : une grille de descripteurs calculée depuis OSM est une *base dérivée* ; vendue en fichier, elle tombe sous le partage à l'identique (⚠ lecture ODbL §4.4, pas un avis juridique). Le dépôt embarque encore des dérivés Maxar et des cocoteraies DAG « recherche/éducation » (🔎 `01_REPO_MAP.md` §7, audit 70) : on ne monte pas un produit « données » sur un dépôt qui n'a pas purgé les siennes.
3. **Fab : 420 000 fiches, 20 000 vendeurs** (🔎 chiffres Epic 2025 relayés par https://www.strayspark.studio/blog/fab-marketplace-12-month-retrospective-seller-2026). ⚠ Un plugin « aléas côtiers » y vend quelques dizaines d'unités par an. « Volume » est faux.
4. **Le B2B risque littoral n'achète pas un plugin Unreal pour calculer.** Il a SFINCS, XBeach, TELEMAC, Delft3D (✖ de mémoire), et la cartographie réglementaire du risque est à l'État (PPR). Si une image du village de Motutapu sous l'eau circule avec le nom du produit, « plausible, jamais une prévision » ne protège personne : le risque est réputationnel avant d'être juridique (audit 73).
5. **MCP et « l'IA explique mais ne décide jamais »** : une règle déterministe s'explique par une info-bulle. Un serveur MCP gratuit autour d'un plugin est une démo, pas un produit.
6. **Le dépôt n'a pas validé son jeu.** 147 tickets en `review`, `main` figé au 2026-10-06 alors que les builds v1.6–v2.0 vivent dans l'arbre de travail, aucun test multi-machines reconnu, CI sans compilation (🔎 `docs/production/recette-2026-10-11.md`, `01_REPO_MAP.md` §6). Huit des dix outils que j'ai vus mourir sont nés ainsi.
7. **Six couches** (doc 02) : plan correct pour quinze personnes, posé sur un développeur seul. Chaque couche de plus est une semaine sans utilisateur.

Ce qui survit est petit ; c'est pour ça que c'est vendable.

## 1. Nom et promesse

**Tidemark** — *« Une montée des eaux qui suit ton terrain, identique sur quatre machines, et qui te dit dans l'éditeur ce qui coule en premier. »*

Alternatives : **FirstWet**, **Rimline**, **Hoa** (terme géographique tuamotu, à valider avant usage).

## 2. Le problème que personne ne résout

- 🔎 L'inondation du dépôt est un solveur d'eaux peu profondes CPU, sans UObject, déterministe, à pas fixe, qui ne réplique que l'horloge et le centre de grille (`S/Flood/FloodGrid.h:4-10`, `FloodSimulationActor.cpp:163-169,401-424`). Chaque client refait la même eau. Les plugins d'eau UE (Fluid Flux, Oceanology, Water d'Epic) sont GPU et visuels : ni même résultat sur deux GPU, ni lecture gameplay bon marché (⚠ à vérifier semaine 1).
- ⚠ Besoin réel : « je veux que l'eau monte *pour de vrai* sur mon Landscape, que le serveur fasse foi, que ça coûte < 1 ms et ne désynchronise pas ». Aujourd'hui les devs font monter un plan en Z (Flood With Friends, Are We Sinking Yet?, 🔎 audit 81) faute d'alternative abordable.
- ✖ Non prouvé : le « linter côtier » comme achat principal. C'est une démo de 30 s et un garde-fou de level design, pas la raison d'ouvrir le portefeuille.
- Vérifié : le solveur existe et a des specs (`Flood/Tests/`, 4 fichiers). Supposé : qu'un studio paie.

## 3. Clients

**N°1** : le dev solo ou duo d'un coop « friendslop » sur UE 5.4+, qui a une île et veut une submersion jouable en multi sans écrire solveur ni netcode. Il teste en 10 min et demande remboursement si ça ne marche pas sur Landscape.

**N°2** : l'enseignant ou médiateur scientifique (musée, lycée, association littorale) qui veut une maquette « si la mer monte de 1 m, où passe-t-elle en premier » sur un terrain **inventé ou anonymisé**. Il paie peu, apporte crédibilité et retours. **Pas** la commune, **pas** le bureau d'études.

## 4. L'idée unique

**« First-wet map » : pour toute grille de hauteurs, Tidemark calcule l'ordre et l'instant d'arrivée de l'eau en chaque cellule, de façon déterministe et seedée, et l'expose trois fois** : au level designer (calque éditeur, lint « ta sortie se coupe à 4:10 »), au gameplay (`SampleWater`, `TimeToWet`), au réseau (seed + horloge → même eau partout, somme de contrôle).

Ça n'existe pas parce que les plugins d'eau optimisent le rendu, pas la **prédictibilité**, et que les solveurs scientifiques ne tournent ni dans une frame ni en listen server. Défendable 12 mois parce que le fossé n'est pas l'algorithme (virtual pipes, 2007) mais l'intégration prouvée : tests de déterminisme, réplication par seed, budget CPU mesuré, Landscape et meshes, API stable. ⚠ Un concurrent GPU copiera le rendu avant le déterminisme réseau.

Ce que l'idée **n'est pas** : une reconstitution de lieux réels, une prévision, un moteur d'IA.

## 5. Périmètre

| V1 (8 semaines) | V2 (si ≥ 150 ventes ou 3 studios utilisateurs, ⚠) | Jamais |
|---|---|---|
| Plugin UE, modules runtime + éditeur, sans dépendance à Tapao | Rugosité par cellule et édition runtime : « couper le rivage fait monter l'eau plus vite » (mécanique 5/5 du doc 02) | Packs de lieux réels nommés |
| Entrée : Landscape, heightmap PNG/EXR ou meshes (bake par rayons à l'import) | Surcote et vent seedés, forçage simple (pas de cyclone IBTrACS) | Prévision, réglementaire, aide à la décision |
| Solveur CPU déterministe, seed, horloge, checksum, snapshot correctif | Générateur d'atoll **fictif** paramétrique (crête, platier, motu, hoa, lagon) | Volcans, feux, avalanches |
| Calque first-wet + lint (refuge, route coupée, zone de spawn) | Importeur NetCDF SFINCS/XBeach → texture, pour la médiation | Serveur MCP comme produit |
| Pont de rendu : texture R16F + MPC, matériau d'exemple | Script Python MIT `heightmap_from_open_data.py` : **l'utilisateur** assemble Copernicus/ICESat-2/Te Fenua sous sa responsabilité, avec `CREDITS.json` | Cadastre, bâti nominatif, données personnelles |
| Map de démo fictive, docs, vidéo 30 s | | Google 3D Tiles, Maxar, cocoteraies DAG |

## 6. Architecture technique

Trois couches, pas six. Tout vient du dépôt.

- **Plugin `Tidemark`** (`Plugins/Tidemark/`), modules `TidemarkRuntime` et `TidemarkEditor`. Dépendances : Core, Engine, Landscape (lecture), ProceduralMeshComponent (optionnel). Aucune vers `monke_island` : c'est Tapao qui dépend du plugin (`docs/conventions/architecture.md`).
- **A — Terrain** : `FTidemarkHeightField` (SoA int16 cm, pas, origine). Importeurs `FromLandscape`, `FromTexture`, `FromCollision` (rayons **une fois à l'import**, contre 16 000 par image dans `FloodSimulationActor.cpp:500`). `UTidemarkTerrainAsset : UPrimaryDataAsset` avec hash des sources et `Credits`.
- **B — Simulation** : `FTidemarkSolver` = `FFloodGrid` déplacé tel quel (pas 0,05 s, ≤ 4 pas de rattrapage), rugosité en V2. `UTidemarkSubsystem : UTickableWorldSubsystem` possède le solveur, `FTidemarkSeed` (une seed, sous-flux nommés ; remplace `FMath::Rand`/`UtcNow`/`Cycles`, 🔎 MAP §3.4) et `FTidemarkClock` (fonctions pures d'`ExpeditionClock.cpp`). `UTidemarkRules : UDataAsset` porte toutes les valeurs. Réseau : seed, départ et `FTidemarkEdit` répliqués (`FFastArraySerializer`) ; `FTidemarkChecksum` CRC32 toutes les 5 s, snapshot compressé si écart. Déterminisme : CPU, même binaire, ordre d'itération fixe, pas de réduction `ParallelFor` non ordonnée (à vérifier, `FloodGrid.cpp:46-87`).
- **C — Lecture** : `UTidemarkRenderBridge` (texture R16F 5 Hz, MPC), `UTidemarkQuery` (BlueprintCallable : `SampleDepth`, `TimeToWet`, `IsWetAt`), côté éditeur `FTidemarkFirstWetBake` (simulation hors ligne jusqu'au niveau max → texture d'instants) et `FTidemarkLint` (règles en DataAsset, Message Log avec focus acteur).
- **Format** : `.tmk`, en-tête JSON (version, origine, pas, dimensions, hash, crédits) + corps binaire int16, lisible en Python sans Unreal.
- **MCP** : rien dans le plugin ; au plus un commandlet de lint. L'« explication » d'un point est la lecture des règles en clair.
- **Perf** (cibles, non mesurées) : 512² cellules à 6 m, ≤ 1 ms moyen / 2 ms pic, ≤ 40 Mo, ≤ 1 ko/s réseau (doc 02 §B.5), mesurées semaine 3.
- **Tests** : `Tidemark.Solver.*`, `Tidemark.Determinism` (deux runs de 600 s, même CRC), `Tidemark.Import.Landscape`, `Tidemark.Lint.*`. CI : compilation UBT et specs, ce que la CI actuelle ne fait pas (🔎 `.github/workflows/verifications.yml`).

## 7. Démo de 30 secondes

Un Landscape du starter content. 0–8 s : « Bake first-wet », un calque bleu → rouge se peint, trois panneaux : « Refuge A : 6:20 », « Pont : coupé à 3:40 », « Spawn : sous l'eau à 2:10 ». 8–20 s : PIE hôte/client côte à côte, l'eau remplit les creux puis franchit la crête ; CRC vert. 20–30 s : le designer rehausse le pont de 1 m, rebake : « Pont : 7:05 ». Texte : « Deterministic. Replicated by seed. Reads your terrain. »

## 8. Modèle économique (tout ⚠ estimé)

- Fab, 88 % reversés (🔎 https://www.unrealengine.com/blog/fab-content-marketplace-launches-in-october-publishing-portal-opens-today). Prix **59 $**, licence standard, pas de pack payant en V1.
- Bas : 40 ventes an 1 ≈ 2 k$. Médian : 150 ≈ 7,8 k$. Haut : 400 avec un jeu connu qui l'utilise ≈ 21 k$. Rien ne paie un salaire. **Produit d'appoint et preuve de compétence, pas une entreprise.** Le dire maintenant évite une année perdue.
- Valeur ailleurs : l'usage dans Tapao (« utilisé en production ») ; en V2, licence site 300–800 € pour la médiation, volume faible.
- Pas de B2B « risque » en V1 : zéro contrat, zéro responsabilité.

## 9. Licences, éthique, données — non négociables

1. Le plugin V1 **ne contient aucune donnée géographique** ; il lit le terrain de l'utilisateur.
2. Le script d'assemblage (V2) est MIT, hors Fab, et écrit les crédits exigés : Copernicus (« © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH » + déni), WorldCover CC BY 4.0, Te Fenua Etalab 2.0 avec producteur **et date**, ICESat-2 domaine public. **OSM exclu** des grilles redistribuées tant qu'un juriste n'a pas tranché la question ODbL.
3. Jamais Maxar, Google 3D Tiles, cocoteraies DAG, raster IRD sans licence. Tapao purge ses dérivés **avant** le branchement du plugin, sinon le plugin hérite de la contamination.
4. Toute sortie porte « simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation ». Pas de commande « charger un lieu réel » ; aucun toponyme dans le `.tmk`.
5. Pas de rejeu de catastrophes réelles, pas de bâti réel identifiable submergé, aucune donnée personnelle. Démo fictive.
6. `PROVENANCE.json` (SHA-256) pour chaque asset de la démo ; EULA Epic relue ; une heure de juriste PI/données avant la première vente.

## 10. Plan en 8 semaines

| Sem. | Livrable | Critère de fin vérifiable (niveau de preuve) |
|---|---|---|
| 1 | **Go/no-go** : benchmark Fluid Flux / Oceanology / Water sur déterminisme et API gameplay ; 5 entretiens de devs coop UE ; squelette du plugin | Tableau sourcé ; ≥ 3 devs sur 5 « je paierais 59 $ », sinon **arrêt** ; `compilé` |
| 2 | `FTidemarkSolver` extrait de `FFloodGrid`, seed, horloge ; Tapao compile contre le plugin | Specs `Tidemark.Solver.*` vertes ; Determinism 600 s, CRC identique × 10 ; `testé éditeur` |
| 3 | Importeurs Landscape / texture / collision ; `UTidemarkRules` ; profil | Landscape 2 km importé en < 5 s ; ≤ 1 ms moyen mesuré par `ue.ps1 profile` ; `testé éditeur` |
| 4 | Réplication : seed, départ, edits, checksum, snapshot | Hôte + client sur **deux PC**, 3 runs de 10 min, 0 écart CRC ; `testé multi-machines` |
| 5 | Bake first-wet, 3 règles de lint, Message Log | `Tidemark.Lint.*` vertes ; le lint annonce la route coupée avec < 5 % d'écart sur l'heure réelle en PIE ; `testé éditeur` |
| 6 | Pont de rendu, matériau, requêtes Blueprint ; map de démo fictive | Package Development ; 60 FPS médian 1080p pendant la montée ; `testé build` |
| 7 | Docs, vidéo, page Fab, EULA, licences, `PROVENANCE.json` ; juriste 1 h | Page soumise ; checklist licences signée ; 2 testeurs externes installent seuls en < 15 min |
| 8 | Publication ; `Flood/` de Tapao remplacé par le plugin sur branche séparée | Fab en ligne ; specs Tapao vertes ; décision écrite V2 à J+30 selon ventes |

## 11. Cinq raisons d'échouer et parades

1. **Personne n'en veut** (⚠ 50 %). Parade : go/no-go réel en semaine 1, arrêt écrit sous 3 devs sur 5.
2. **Fluid Flux ou Epic sortent un mode déterministe.** Parade : le fossé est netcode + lint + API ; publier vite, prix bas. Si Epic le fait, Tidemark redevient outil interne et on cesse de vendre.
3. **Désynchronisation en production** (flottants, `ParallelFor`, rattrapage). Parade : spec de déterminisme à chaque commit, checksum et snapshot livrés, jamais de GPU dans l'état qui fait foi.
4. **Le plugin siphonne le jeu, aucun des deux ne sort.** Parade : le plugin **est** le refactor de `Flood/` que doc 02 recommande ; 8 semaines plafonnées ; pas de V2 sans ventes ; L1–L4 prioritaire sur l'éditeur partagé.
5. **Une image d'un lieu réel submergé circule avec le nom du produit.** Parade : aucune donnée réelle dans le plugin, démo fictive, mention d'exclusion, silence sur « Tuamotu » tant que Tapao n'a ni licences assainies ni contact local.

## 12. Ce que je rejette de la direction retenue

1. **Les packs « inspirés de » bâtis sur Copernicus + ICESat-2 + Sentinel-2.** Rejeté V1 et V2 : relief vertical impossible à honorer (§0.1), chaîne de licences plus lourde que le plugin (ODbL, déni Copernicus, Etalab daté), et l'envie de « lieux précis » est le risque réputationnel n°1. Forme acceptable : un script libre exécuté par l'utilisateur, sous sa responsabilité.
2. **Le double marché devs + B2B risque.** Rejeté : clients incompatibles (fun déterministe contre validation), deux responsabilités. Le second ne calcule pas dans UE ; au mieux il visualise ses résultats (V2, médiation, pas décision).
3. **MCP comme accroche et « l'IA explique ».** Rejeté comme produit : aucune valeur payante ni défendabilité, et il attire la méfiance qu'il prétend conjurer.
4. **Six couches et un catalogue d'aléas.** Rejeté en V1 : un seul aléa, la submersion, est codé, testé et unique par son déterminisme. L'érosion sur Nanite non déformable est un chantier entier (doc 02 §B.6, 2/5) ; chaque aléa de plus est un mois sans client.
5. **Commencer maintenant comme produit séparé.** Rejeté dans cet ordre : d'abord sortir `Flood/` en plugin **parce que Tapao en a besoin** (dépendance `Flood → ExpeditionGameState`, rayons par image, seeds dispersées), puis décider de vendre après le go/no-go. Sinon, un seul produit ; les deux à la fois est la seule issue dont je suis sûr qu'elle échoue.

**Forme qui doit exister** : un plugin à 59 $ qui fait une seule chose mieux que tout le monde — une montée des eaux déterministe, répliquée et lisible par le level designer — nourri par le terrain de l'utilisateur, sans un seul octet de lieu réel.
