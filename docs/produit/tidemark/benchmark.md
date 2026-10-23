# Tidemark — banc d'essai des solutions d'eau pour Unreal (TM-06, S1)

- **Version** : 0.1, 2026-10-23. **Auteur** : `tidemark-release-engineer`. **Tâche** : T-346.
- **Objet** : positionner Tidemark (cahier Tidemark §2, §4, §8) face aux solutions d'eau existantes pour Unreal. Ce banc sert à **positionner**, pas à comparer sur la page Fab ; aucun de ces produits ne vise le même problème que Tidemark, et aucune ligne ci-dessous ne doit être reprise telle quelle dans la communication publique.
- **Convention** : 🔎 fait observé (source, date de consultation) · ⚠ estimé · « non vérifié » = non trouvé dans une source consultable. Toutes les sources ont été consultées le **2026-10-23** par WebSearch (8 recherches, extraits de résultats) ; **l'accès direct aux pages Fab, imaginaryblend.com, dev.epicgames.com et forums.unrealengine.com était bloqué par le proxy de la session**, donc les faits viennent des extraits indexés, pas d'une lecture intégrale des pages. À relire sur machine du propriétaire avant la page Fab (voir §6).
- **Rappel non négociable** (cahier §9.5) : Tidemark est une « simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation ». Le banc ne compare personne sur la fidélité physique.

---

## 1. Grille de lecture

Pour chaque produit, les mêmes neuf questions, qui sont celles du cahier §2 (« simulations GPU orientées rendu : résultat différent d'un GPU à l'autre, pas d'état faisant foi, pas de lecture gameplay bon marché » — une hypothèse ✖ à vérifier ici) :

1. Prix et licence.
2. Calcul sur GPU ou CPU.
3. Déterminisme annoncé (même résultat sur deux machines ?).
4. État faisant foi côté serveur ou non.
5. API gameplay (lecture de profondeur, vitesse, événements).
6. Multijoueur (réplication).
7. Lint ou outil éditeur (lecture du terrain avant de jouer).
8. Nombre d'avis.
9. Dernière mise à jour.

Produits retenus : **Fluid Flux** (ImaginaryBlend), **Oceanology** (Galidar), **Water** d'Epic (UE 5.8, avec « Water Advanced » / Shallow Water River), plus deux autres : **Dynamic Real Water** (Fab) et **Water Dynamics** (TimurSibatov, forum). Écartés faute de sources ou hors sujet : KeraFlow (forum, aucune fiche Fab trouvée), VaOcean (MIT mais archivé en UE 4.12), NWH Dynamic Water Physics 2 et « Water Physics » (flottabilité seulement, pas de simulation de surface ni d'inondation).

---

## 2. Fiches produit

### 2.1 Fluid Flux (ImaginaryBlend, Krystian Komisarek)

| Question | Réponse | Source |
|---|---|---|
| Prix | 🔎 « From $349.99 » sur Fab (vendeur ImaginaryBlend). Même prix annoncé au lancement Marketplace (UE 4.26–5.0), donc possiblement stable depuis 2021. | https://www.fab.com/search?tags=lake (2026-10-23) ; https://news.denfaminicogamer.jp/news/2205132f (2026-10-23) ; fiche : https://www.fab.com/listings/196c70cd-1283-4249-bf6b-c3019d1cbe11 |
| Licence | ⚠ Licence standard Fab (usage commercial). Options de licence précises : non vérifié. | https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab (2026-10-23) |
| GPU / CPU | 🔎 Simulation d'eaux peu profondes 2D sur **GPU** (Niagara, lecture par « Niagara Readback »). | https://80.lv/articles/fluid-flux-a-cool-water-simulation-system-for-unreal-engine/ (2026-10-23) ; https://forums.unrealengine.com/t/imaginaryblend-fluid-flux/2420009 (2026-10-23) |
| Déterminisme | 🔎 Le vendeur écrit que le générateur de vagues « uses render time, so it is not yet deterministic » ; la simulation dynamique est « client-side and can desynchronize ». Aucune promesse de même résultat sur deux machines. | Tableau des limitations relayé dans https://forums.unrealengine.com/t/imaginaryblend-fluid-flux/2420009 (2026-10-23) |
| État faisant foi serveur | 🔎 Non : « Niagara does not function on dedicated servers, making it impossible to process critical data from the server side. Simulations are calculated client-side only, and Niagara Readback results are automatically sent to the server » ; le vendeur qualifie cela d'« unsafe for authoritative-server designs ». | idem |
| API gameplay | 🔎 Lecture de la surface via Niagara Readback (hauteur, vitesse) côté client ; suivi des zones sèches/mouillées (« wet-dry region tracking »). Fonctions Blueprint précises, événements « cellule atteinte » : non vérifié. | https://80.lv/articles/fluid-flux-a-cool-water-simulation-system-for-unreal-engine/ ; https://forums.unrealengine.com/t/imaginaryblend-fluid-flux/2420009 (2026-10-23) |
| Multijoueur | 🔎 Partiel : vagues océaniques répliquées (le serveur envoie temps de vague et vent) ; « Coastline time replication has not been implemented yet » ; simulation dynamique non répliquée ; « synchronizing the simulation's state is particularly challenging when new players join or frame spikes occur. The system cannot recover smoothly ». Le vendeur recommande des états pré-bakés pour les arrivants tardifs. Feuille de route 3.0 : « implement multiplayer for ocean, coastline, and pre-baked simulation states ». | https://forums.unrealengine.com/t/imaginaryblend-fluid-flux/2420009 ; https://imaginaryblend.com/2024/11/21/fluid-flux-3-0-release-notes/ (2026-10-23) |
| Lint / outil éditeur | 🔎 Outils d'édition et de bake de simulation dans l'éditeur (états pré-bakés, « Fluid Flux 3.0 » : système de verre, éclairage sous-marin, décals, pluie). Aucun outil de type « quand cette route est-elle coupée » : non trouvé. Limitation déclarée : « open-world is currently not supported ». | https://80.lv/articles/fluid-flux-3-0-arrives-with-a-glass-system-underwater-lighting-decals-rain-more/ ; https://imaginaryblend.com/2021/09/26/fluid-flux/ (2026-10-23) |
| Avis | Non vérifié (la fiche Fab n'a pas pu être lue). | — |
| Dernière mise à jour | 🔎 Fluid Flux 3.0 : notes de version du 2024-11-21 ; guide tiers pour UE 5.5 et 5.6 (2025). Version C++ « Fluid Flux Plugin » annoncée en développement. | https://imaginaryblend.com/2024/11/21/fluid-flux-3-0-release-notes/ ; https://dredyson.com/complete-beginners-guide-to-imaginaryblend-fluid-flux-… (2026-10-23) |

Lecture : produit de référence pour le **rendu** d'eau dynamique (rivières, vagues côtières, inondation visuelle), très au-dessus de Tidemark en qualité d'image. Par les mots du vendeur lui-même, la simulation n'est ni déterministe ni autoritaire ; c'est précisément le trou visé par le cahier §2.

### 2.2 Oceanology (Galidar)

| Question | Réponse | Source |
|---|---|---|
| Prix | 🔎 Plusieurs éditions et prix observés : « From $189 » (Creative Bloq) ; $159.99 (iRender, 2023-09) ; $249.99 et $149.99 sur des extraits Fab sans attribution claire à une édition. Le vendeur annonce « Oceanology Legacy becomes Oceanology Lite when Oceanology Pro launches ». **Prix courant par édition : non vérifié.** | https://www.creativebloq.com/features/unreal-engine-5-plugins ; https://irendering.net/the-best-plugins-for-unreal-engine-5-in-2023/ ; https://galidar.com/oceanology-legacy (2026-10-23) |
| Licence | ⚠ Licence standard Fab. 🔎 Historique : un article 80.lv rapporte une polémique sur une mise à jour (Oceanology 6) promise gratuite puis facturée (gratuite pour achats après le 1er mai, −50 % sinon). Cité comme fait de marché, sans jugement. | https://80.lv/articles/developers-enraged-by-unreal-engine-plug-in-s-broken-promise (2026-10-23) |
| GPU / CPU | 🔎 Rendu d'océan temps réel ; éditions ciblées par GPU : Legacy « optimized for mid-range GPUs », NextGen « high-end GPUs (RTX 3080+) ». Calcul des vagues côté CPU pour la flottabilité : non vérifié. | https://www.fab.com/listings/1cd1f62e-0fa3-48bf-bc60-f0e06010fce3 ; https://galidar.com/oceanology-legacy (2026-10-23) |
| Déterminisme | Non vérifié (aucune mention trouvée). | — |
| État faisant foi serveur | Non vérifié. | — |
| API gameplay | 🔎 Deux systèmes de flottabilité, nage, rendu sous-marin (god rays, caustiques), paramètres vagues et écume. Lecture de profondeur en Blueprint : non vérifié. | https://www.fab.com/listings/1cd1f62e-0fa3-48bf-bc60-f0e06010fce3 ; https://irendering.net/the-best-plugins-for-unreal-engine-5-in-2023/ (2026-10-23) |
| Multijoueur | Non vérifié (aucune mention de réplication trouvée dans les sources consultées). | — |
| Lint / outil éditeur | 🔎 Modes « Local » (lacs, piscines) et « Infinite » (haute mer). Pas d'outil de lecture du terrain. | https://irendering.net/the-best-plugins-for-unreal-engine-5-in-2023/ (2026-10-23) |
| Avis | 🔎 Oceanology Legacy : **4,3 / 5 sur 284 notes** (Fab). Autres éditions : non vérifié. | https://www.fab.com/listings/1cd1f62e-0fa3-48bf-bc60-f0e06010fce3 (2026-10-23) |
| Dernière mise à jour | 🔎 Vendeur : « 7+ years production tested » ; Oceanology 6 annoncée (2025, date exacte non vérifiée). | https://www.fab.com/listings/1cd1f62e-0fa3-48bf-bc60-f0e06010fce3 ; https://80.lv/articles/developers-enraged-by-unreal-engine-plug-in-s-broken-promise (2026-10-23) |

Lecture : produit d'**océan** (surface, vagues, sous-marin, flottabilité). Pas de notion d'inondation du terrain ni d'état simulé par cellule. Le seul chevauchement avec Tidemark est le rendu de surface, que Tidemark ne fournit pas (pont de rendu seulement, cahier §5) ; il est donc complémentaire, pas concurrent.

### 2.3 Water d'Epic (plugin moteur, UE 5.8) et « Water Advanced » / Shallow Water River

| Question | Réponse | Source |
|---|---|---|
| Prix | 🔎 Gratuit, inclus dans le moteur, sous l'EULA Unreal. | https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/Water (2026-10-23) |
| Licence | EULA Unreal Engine. | idem |
| GPU / CPU | 🔎 Water Bodies (`AWaterBodyOcean`, `Lake`, `River`, `Custom`) avec vagues de Gerstner ; `UBuoyancyComponent::GetWaterHeight(position, …, bShouldIncludeWaves)` et `GetLastWaterSurfaceInfo` (plan d'eau, normale, profondeur, vitesse) sont des évaluations **CPU** de la surface analytique. 🔎 Depuis UE 5.6, le plugin **Water Advanced** ajoute l'acteur Shallow Water River : simulation 2D **Niagara (GPU)** pilotée par `river_sim_system`. | https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/Water/UBuoyancyComponent/GetWaterHeight ; https://docs.unrealengine.com/4.27/en-US/API/Plugins/Water/UBuoyancyComponent/GetLastWaterSurfaceInfo/index.html ; https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ShallowWaterRiverComponent?application_version=5.5 ; https://dev.epicgames.com/community/learning/tutorials/OZMa/… (2026-10-23) |
| Déterminisme | 🔎 Vagues de Gerstner : fonction analytique du temps, donc reproductible si le temps est le même (⚠ lecture technique, non annoncée par Epic comme garantie). Shallow Water River : simulation Niagara GPU, aucune promesse de déterminisme trouvée. Statut Experimental dans les docs Python 5.5. | idem |
| État faisant foi serveur | 🔎 Water Bodies : la surface analytique est évaluable sur le serveur (pas de dépendance Niagara pour `GetWaterHeight`). Shallow Water River : Niagara, donc même contrainte que Fluid Flux sur serveur dédié (⚠ déduit, non vérifié). | idem |
| API gameplay | 🔎 `GetWaterHeight`, `GetLastWaterSurfaceInfo` (profondeur, vitesse), `GetCurrentWaterBodyComponents` ; flottabilité asynchrone (`FBuoyancyManagerAsyncInput`). Pas d'événement « point inondé à t » ni de notion d'ordre d'arrivée. | https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/Water/UBuoyancyComponent/GetCurrentWaterBodyComponents ; https://docs.unrealengine.com/4.27/en-US/API/Plugins/Water/FBuoyancyManagerAsyncInput/Inputs/index.html (2026-10-23) |
| Multijoueur | 🔎 `BuoyancyComponent` expose les drapeaux de réplication standard (composant répliqué, acteur propriétaire doit répliquer). Pas de synchronisation d'un état d'eau dynamique documentée. Le niveau d'une Water Body est un acteur : réplicable par le jeu, pas par le plugin. | https://docs.unrealengine.com/5.1/en-US/PythonAPI/class/BuoyancyComponent.html (2026-10-23) |
| Lint / outil éditeur | 🔎 Water Brush (sculpte le Landscape autour des splines), Water Zone. Aucun outil de lecture « où l'eau arrive en premier ». Forum : collisions perdues sur grands splines, lacs n'interagissant pas avec la Shallow Water River, rivière bakée invisible après package (posts individuels, non confirmés). | https://forums.unrealengine.com/t/unreal-5-6-water-simulation/2626503 ; https://forums.unrealengine.com/t/shallow-water-body-advanced-water-plugin-fails-to-render/2634176 (2026-10-23) |
| Avis | Sans objet (moteur). | — |
| Dernière mise à jour | 🔎 Water Advanced introduit en UE 5.6 (2025) ; chemin du plugin toujours `/Engine/Plugins/Experimental/Water`. État exact en 5.8 : à vérifier sur la machine du propriétaire (`UE_5.8/Engine/Plugins/Experimental/Water/Water.uplugin`, champ `"Installed"`/`"IsExperimentalVersion"`). | https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/Water (2026-10-23) |

Lecture : c'est le socle gratuit que tout client de Tidemark a déjà. Tidemark doit **s'y brancher** (le pont de rendu du cahier §5 peut piloter une Water Body ou un matériau) et non le remplacer. La Shallow Water River 5.6+ est la menace la plus directe sur le « visuel » (rivière dynamique gratuite), mais pas sur l'état de jeu.

### 2.4 Dynamic Real Water (Fab)

| Question | Réponse | Source |
|---|---|---|
| Prix | 🔎 $99.99. | https://www.fab.com/listings/84495b74-94b4-45d1-a08d-2feac2ecb92f (2026-10-23) |
| Licence | ⚠ Licence standard Fab. | idem |
| GPU / CPU | Non vérifié. | — |
| Déterminisme | Non vérifié. | — |
| État faisant foi serveur | Non vérifié. | — |
| API gameplay | Non vérifié (« fully modular C++ plugin » selon la fiche). | idem |
| Multijoueur | 🔎 La fiche se présente comme « physically accurate, multiplayer-replicated water » (⚠ affirmation du vendeur, non testée ; périmètre de ce qui est répliqué non précisé dans l'extrait). | idem |
| Lint / outil éditeur | Non vérifié. | — |
| Avis | 🔎 4,7 / 5 sur 13 notes. | idem |
| Dernière mise à jour | Non vérifié. | — |

Lecture : intéressant parce que c'est le seul produit payant trouvé qui **met la réplication en première ligne** et qui est dans la gamme de prix de Tidemark. À lire intégralement avant la page Fab : si sa réplication couvre une surface dynamique par cellule, le positionnement « same tide on every machine » doit être formulé plus précisément (ordre d'arrivée + checksum + rejeu, pas seulement « répliqué »).

### 2.5 Water Dynamics (TimurSibatov, forum Epic)

| Question | Réponse | Source |
|---|---|---|
| Prix | Non vérifié (aucune fiche Fab trouvée). | — |
| Licence | Non vérifié. | — |
| GPU / CPU | 🔎 « depth-averaged shallow-water solver » le long d'un canal de spline ; GPU ou CPU : non vérifié. | https://forums.unrealengine.com/t/timursibatov-water-dynamics-interactive-rivers-waterfalls-lakes/2840244 (2026-10-23) |
| Déterminisme | Non vérifié. | — |
| État faisant foi serveur | Non vérifié. | — |
| API gameplay | Non vérifié. | — |
| Multijoueur | Non vérifié. | — |
| Lint / outil éditeur | 🔎 Périmètre déclaré : rivières, cascades, lacs ; exclut « landscape carving » et océan infini. Pas d'inondation de terrain. | idem |
| Avis | Non vérifié. | — |
| Dernière mise à jour | Non vérifié (post de forum, 2025 ⚠). | — |

Lecture : même famille que Fluid Flux (solveur d'eaux peu profondes orienté rivière). Retenu comme signal que le solveur « shallow water » est banalisé (cahier §4 : « ce qui est défendable n'est pas l'algorithme ») ; à ne pas citer tant que la fiche n'est pas lue.

---

## 3. Tableau de synthèse

| | Fluid Flux | Oceanology (Legacy) | Water d'Epic (+ Water Advanced) | Dynamic Real Water | Water Dynamics | **Tidemark V1 (cahier)** |
|---|---|---|---|---|---|---|
| Prix | $349.99 🔎 | $149.99–$249.99 selon édition (non vérifié) | gratuit 🔎 | $99.99 🔎 | non vérifié | ⚠ 89 $ |
| Calcul | GPU (Niagara) 🔎 | GPU rendu 🔎 | Gerstner CPU + rivière Niagara GPU 🔎 | non vérifié | non vérifié | CPU, ordre fixe |
| Déterminisme annoncé | non (« not yet deterministic ») 🔎 | non vérifié | non annoncé | non vérifié | non vérifié | oui, seedé, spec en CI |
| État faisant foi serveur | non (« unsafe for authoritative-server ») 🔎 | non vérifié | surface analytique : oui ; rivière Niagara : non (⚠) | non vérifié | non vérifié | oui |
| API gameplay | readback hauteur/vitesse client 🔎 | flottabilité, nage 🔎 | `GetWaterHeight`, `GetLastWaterSurfaceInfo` 🔎 | non vérifié | non vérifié | `TimeToFlood`, `FloodOrder`, `IsPassable`, `SampleWater` |
| Multijoueur | partiel, feuille de route 🔎 | non vérifié | drapeaux de réplication standard 🔎 | « multiplayer-replicated » (vendeur) 🔎 | non vérifié | seed + horloge + journal + checksum + snapshot |
| Lint éditeur | bake d'états, pas de lint | non | Water Brush, pas de lint | non vérifié | non | calque first-wet, curseur, 4 règles |
| Éditions du terrain en jeu | non trouvé | non | non | non vérifié | non | `Dig`, `Fill`, `Raise` répliquées |
| Rendu de surface | excellent 🔎 | excellent 🔎 | bon, gratuit 🔎 | non vérifié | non vérifié | **minimal** (texture + MPC + matériau d'exemple) |
| Avis | non vérifié | 4,3 / 284 🔎 | s.o. | 4,7 / 13 🔎 | non vérifié | 0 |
| Dernière MAJ | 3.0, 2024-11 ; guides 5.6 🔎 | « 7+ ans », v6 2025 🔎 | 5.6 (Water Advanced) 🔎 | non vérifié | non vérifié | — |

---

## 4. Positionnement

### 4.1 Ce que Tidemark fait que personne (dans ce banc) ne fait

1. **Un état d'inondation qui fait foi sur le serveur**, calculé sur CPU en ordre fixe : aucun produit consulté ne l'annonce ; Fluid Flux écrit explicitement l'inverse pour lui-même.
2. **Même résultat sur deux machines, prouvé par somme de contrôle par bloc** et rejeu depuis (seed, grille, échéancier, journal). Aucun produit n'annonce de déterminisme ; Dynamic Real Water annonce une réplication, pas un déterminisme.
3. **Lecture du terrain avant de jouer** : `TimeToFlood` / `FloodOrder` par point, calque first-wet, curseur de niveau de mer, lint « Refuge A : 6:20 ». Aucun outil éditeur de ce type trouvé.
4. **Le joueur change la réponse** : `Dig`, `Fill`, `Raise` répliquées en journal. Non trouvé ailleurs.
5. Format ouvert `.tmk` + lecteur Python + `explain` par cellule (cahier §5, V1/V1.1).

### 4.2 Ce que les autres font mieux (et que Tidemark ne doit pas prétendre faire)

1. **Rendu** : Fluid Flux et Oceanology sont des produits de rendu d'eau de haut niveau ; Tidemark livre un pont (texture R16 + MPC) et un matériau d'exemple. La page Fab doit le dire sans ambiguïté, sinon remboursements (cahier §3 : « demande remboursement si ça ne marche pas »).
2. **Vagues, écume, sous-marin, flottabilité** : Oceanology et Water d'Epic. Tidemark n'en fait pas ; il doit cohabiter avec Water d'Epic (gratuit, déjà installé chez tout client).
3. **Rivières dynamiques gratuites** : Water Advanced 5.6+ réduit l'intérêt d'un solveur « shallow water » en soi ; la valeur de Tidemark est l'intégration prouvée (déterminisme, réseau, lint), pas le solveur.
4. **Maturité et avis** : Oceanology 284 notes, Fluid Flux 3 versions majeures depuis 2021. Tidemark part de zéro ; les 8 entretiens de S1–S2 et la vidéo de 30 s portent toute la crédibilité.
5. **Open world** : Fluid Flux le déclare non supporté ; Tidemark non plus (grille unique par map, cahier §6). Pas un avantage, à documenter.

### 4.3 Hypothèse du cahier §2, verdict

« Les plugins d'eau vendus sont des simulations GPU orientées rendu : résultat différent d'un GPU à l'autre, pas d'état faisant foi, pas de lecture gameplay bon marché. »
- **GPU orienté rendu** : 🔎 confirmé pour Fluid Flux et Oceanology, et pour la Shallow Water River d'Epic.
- **Pas d'état faisant foi** : 🔎 confirmé par le vendeur pour Fluid Flux ; non vérifié pour Oceanology et Dynamic Real Water ; faux pour la surface analytique de Water d'Epic (évaluable sur serveur, mais ce n'est pas une inondation du terrain).
- **Pas de lecture gameplay bon marché** : ⚠ nuancé. Water d'Epic et Fluid Flux exposent hauteur et vitesse en un point ; ce qui manque partout est **l'instant et l'ordre d'arrivée** et la possibilité de l'interroger avant de jouer. Reformuler le pitch en conséquence : non pas « ils n'ont pas d'API », mais « aucun ne répond à *quand* ».

---

## 5. Ce que ça change pour le prix 89 $ (⚠ tout estimé)

- **Plafond** : Fluid Flux à $349.99 fixe la référence haute d'un produit d'eau « sérieux » ; Oceanology entre $150 et $250 ; Dynamic Real Water à $99.99 avec réplication en première ligne. 89 $ place Tidemark **sous** le seul produit qui parle de multijoueur et bien sous les deux leaders : cohérent avec un produit qui ne livre pas de rendu.
- **Plancher** : Water d'Epic est gratuit et couvre surface, vagues et flottabilité. Tidemark ne peut pas se vendre comme « de l'eau », seulement comme « l'état de jeu de l'inondation ». Si les entretiens montrent que les devs coop recodent un plan qui monte en Z en une journée (cahier §2 : les trois jeux Steam le font), 89 $ est trop haut et 59 $ (position du contradicteur) redevient défendable.
- **Argument de prix qui tient** : le coût évité n'est pas le solveur (gratuit en 5.6) mais la **recette multijoueur** (spec de déterminisme, checksum, snapshot correctif) et le **lint**. Chiffrer dans les entretiens : combien de jours un studio de 1–8 personnes passe à prouver qu'un état d'eau est identique chez l'hôte et les clients.
- **Risque de formulation** : « multiplayer-replicated » existe déjà à $99.99 ; la page Fab doit dire « same tide on every machine, checksummed, replayable », pas seulement « replicated ».
- **Ce qui ferait monter le prix** (V1.1/V2, cahier §8) : export GeoTIFF, rapport `explain`, générateur d'atoll fictif. Rien de cela en V1 : 89 $ reste un pari, à tester par les 8 entretiens, et la décision appartient au propriétaire.
- **Ce qui ferait baisser le prix** : échec de la porte S4 (pas de preuve deux PC) : alors le produit n'est plus qu'un lint éditeur et vaut ⚠ 29–49 $, ou reste interne (backlog, porte S4).

---

## 6. Ce qui reste à vérifier avant la page Fab (bloquant, machine du propriétaire)

| À faire | Qui | Pourquoi |
|---|---|---|
| Lire intégralement la fiche Fab de Fluid Flux (avis, prix courant, options de licence) et la page « Limitations » d'imaginaryblend.com | tidemark-release-engineer (accès web direct requis) | Les citations ci-dessus viennent d'extraits de recherche |
| Lire les fiches Fab d'Oceanology (toutes éditions) : prix, réplication | idem | Multijoueur non vérifié |
| Lire la fiche Dynamic Real Water : périmètre exact de « multiplayer-replicated », GPU/CPU | idem | Concurrent le plus proche en prix et en promesse |
| Vérifier le statut de `Water` et `WaterAdvanced` dans `UE_5.8/Engine/Plugins/Experimental/` et tester `GetWaterHeight` sur serveur dédié | tidemark-lead ou tidemark-network-engineer | Décide si le pont de rendu cible une Water Body |
| Confirmer « Fab reverse 88 % » (cahier §8) sur la doc Fab | tidemark-release-engineer | Hypothèse de revenu |

Rien dans ce document n'est un avis juridique ni un avis sur la qualité des produits cités ; les citations des vendeurs sont reproduites pour positionner Tidemark et ne doivent pas être reprises sur la page Fab, dans la vidéo ni sur les forums (règle « pas de dénigrement », fiche de rôle).
