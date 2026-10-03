# Fragments des Tuamotu : îles d'expédition découpées par la mer

| Champ | Valeur |
|---|---|
| Lots | L3 : pipeline + `L_Frag_Raraka_01` jouable · L4 : choix au hub · L8 : fragments 2 à N |
| Décision | `docs/adr/ADR-004-fragments-tuamotu.md` |
| Tâches | T-081 → T-092 (feature `fragments`) |
| Scripts | `scripts/map/fragment_candidates.py` (esquisse), puis `scripts/map/fragments/` |
| Données | `docs/reference/map/raraka/fragments/` (`fragments.json`, aperçus, georef de chaque fragment) |
| Cible UE | `/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_NN`, `/Game/MonkeIsland/Environment/Fragments/<FRG>/`, `LI_ExpeditionCore` |

## Intention
Au lieu de lâcher le joueur sur les 29 km de l'atoll, chaque expédition se joue sur un **morceau** de Raraka : quelques motu, un village par zone de pacification, un marae, de la cocoteraie, du fourré, des plages, un platier, des hoa et le lagon. Le morceau est **entouré d'eau** et le reste de l'atoll se devine à l'horizon. On ne doit jamais voir une coupure : ni motu tranché, ni mur, ni bord de terrain.

## Ce que disent les données (aperçu du 2026-10-03)
`python scripts/map/fragment_candidates.py docs/reference/map/raraka/fragments/preview_candidats.png`

Les motu sont regroupés quand moins de 40 m d'eau les séparent. Voici ce qu'on obtient :

| Groupes | Nombre | Usage |
|---|---|---|
| Fragments naturels (8 à 100 ha, moins de 3 km) | **6** : F1 50 ha (0,8 × 1,8 km) · F2 49 ha · F3 42 ha · F4 22 ha · F5 16 ha · F6 8 ha | directement utilisables |
| Grands motu (plus de 1 km² ou plus de 3 km) | 2 (nord-ouest et nord) | à scinder par des hoa synthétiques : on peut en tirer 3 à 5 fragments de plus |
| Secteurs bâtis (moins de 400 m d'un bâtiment) | 4, dont Motutapu | Motutapu est exclu. Les autres sont des cabanes isolées : à vérifier en T-082 |
| Îlots isolés de moins de 8 ha | 146 (chapelets au sud-ouest et au sud-est) | à regrouper en **fragments archipel** : plusieurs îlots reliés par des hoa où l'on patauge |

![candidats](../reference/map/raraka/fragments/preview_candidats.png)

## Règles d'un fragment (valeurs dans `DA_FragmentRules` et dans la config du pipeline, jamais en dur)
- **Taille jouable** : de 15 à 80 ha de terre, 1 à 2,5 km le long de l'anneau. C'est la cible pour une expédition de 2 à 8 min avec 3 zones (§6.1).
- **Contenu minimal** :
  - 3 sites de village d'au moins 4 maisons, un par zone de pacification (`place_villages.py`) ;
  - 1 site de marae (`marae_layout.py`) ;
  - 3 mouillages, dont un accessible pendant les 60 s finales (§6.4) ;
  - 1 crique d'extraction côté lagon ;
  - un départ pour l'équipe et la barque.
- **Frontière** :
  - elle ne coupe jamais la terre ;
  - elle passe par de l'eau large d'au moins 60 m (hoa naturel, lagon ou océan) ;
  - sinon, par un hoa synthétique creusé au point le plus étroit du motu : 40 à 80 m de large, -1,5 à -3 m au centre, une plage de chaque côté, une pente d'au plus 7° ;
  - le platier qui continue vers les motu exclus s'arrête par une pente douce, jamais par une marche.
- **Marge d'eau** : environ 1,5 km de mer autour de la terre. Le fond rejoint le lagon profond (-8 à -30 m) ou l'océan. Le plan d'eau dépasse l'horizon.
- **Horizon** : au-delà de la marge, le reste de l'anneau est montré par un maillage très simplifié (sable et bande de canopée), avec un seul matériau, sans collision ni ombre dynamique.
- **Fiction** : les noms des fragments sont fictifs, sans toponyme réel ; Motutapu en est exclu.

## Pipeline (hors ligne, reproductible)
1. **`find_fragments.py raraka`** (T-082). Il calcule l'axe de l'anneau (squelette terre + platier) et repère les points de coupe :
   - coupes naturelles, là où l'eau est large ;
   - cous étroits des grands motu.

   Il note ensuite chaque segment (taille, sites de village et de marae, criques, cocoteraie et fourré) et écrit `fragments.json`, avec mémoire et identifiants stables comme `villages.json`, ainsi que des aperçus. Les fragments retenus sont **gelés**.
2. **`build_fragment.py raraka FRG_Raraka_01`** (T-083). Il découpe le terrain dans la boîte du fragment, marge comprise, à partir des classes et des hauteurs de `build_terrain.py`. Il retire la terre exclue (remplacée par le profil platier → lagon ou océan), creuse les hoa synthétiques et fait descendre la marge en pente douce. Il recalcule les masques (sable et sable mouillé sur les nouvelles rives) et sort des tuiles Nanite, avec `georef.json` pour le décalage par rapport au repère de Raraka.
3. **`build_horizon.py raraka FRG_Raraka_01`** (T-084). Il produit la silhouette du reste de l'atoll vu depuis le fragment.
4. **Contenu** (T-085, T-086). On réutilise les générateurs existants : cocotiers, végétation, villages, marae, loot, totems. Ils sont **filtrés sur le polygone du fragment et recentrés**. On ajoute mouillages, crique et départs.
5. **Import** (T-089). Commandlet si l'éditeur est fermé, MCP sinon. Résultat : `L_Frag_Raraka_01` avec `LI_ExpeditionCore`.

## Côté jeu (C++, T-087 / T-088)
- `UExpeditionIslandDefinition` (`UPrimaryDataAsset`, type d'asset primaire `ExpeditionIsland`). Il contient :
  - la map en `TSoftObjectPtr<UWorld>`, l'identifiant `FRG_*` et le nom affiché (String Table) ;
  - le nombre de zones et la liste des mouillages ;
  - la taille, la difficulté indicative, les seeds de variantes et les crédits des données.
- `DA_IslandCatalog` : la liste des fragments proposés au hub (L4, tableau de contrats §7).
- Voyage : du hub au fragment par `ServerTravel` en seamless travel, puis retour au hub au bilan. Les joueurs, leur PlayerState et leurs cosmétiques sont conservés ; aucun loot de l'île précédente (recette n°9).
- `LI_ExpeditionCore` contient le plan d'eau tagué `IslandWaterSurface`, le ciel et le jour/nuit, la tempête et le vent, l'horloge d'expédition et le directeur d'événements. Le niveau d'eau reste un paramètre global (`UIslandWaterSubsystem`).
- Pas de World Partition par défaut (environ 3 × 3 km marge comprise). On ne l'active qu'après mesure (T-091).

### Design technique T-087 (lot L3)
- **Dossier** `Source/monke_island/Expedition/Islands/` : les îles sont des données du domaine Expédition (cahier §9 « Data Assets : îles »), à côté de `Expedition/Core/`. `World/` reste pour les systèmes du monde (eau, ciel). Dépendance à sens unique : `Core/` (GameState) inclut `Islands/` dans son `.cpp` ; `Islands/` n'inclut ni `Core/`, ni UI, ni autre domaine (ARC-01).
- **Classes** :
  - `UExpeditionIslandDefinition` (`UPrimaryDataAsset`, type primaire `ExpeditionIsland`) : `FragmentId` (`FRG_Raraka_01`, aussi nom du `FPrimaryAssetId`, donc stable même si l'asset est renommé), `DisplayName` (FText, String Table), `Map` (`TSoftObjectPtr<UWorld>`), `ZoneCount`, `Anchorages` (`FExpeditionAnchorage` : id, nom, rôle §6.4 départ / ressources / repli, disponible pendant la submersion, transform dans le repère de la map), `SizeHectares`, `IndicativeDifficulty`, `VariantSeeds`, `DataCredits`, `PreviewImage` (soft, bundle `Preview`). `FragmentId` et `Map` sont `AssetRegistrySearchable` : on retrouve le fragment d'une map sans rien charger.
  - Mouillages en **données** (id + transform), pas en référence d'acteur : la définition est lisible au hub sans charger la map ; les futurs `BP_MooringPoint` porteront le même `AnchorageId`.
  - `UIslandCatalog` (`UPrimaryDataAsset`, type `IslandCatalog`, asset `DA_IslandCatalog`) : liste de `FPrimaryAssetId` de définitions + `UFragmentRules` (`DA_FragmentRules`, petit, référence dure).
  - `UFragmentRules` (`UDataAsset`) : seuils runtime seulement — délai maximal de chargement (60 s, §16), mouillages minimum (3), mouillage de submersion exigé. Les valeurs du pipeline Python restent dans sa config.
  - `ExpeditionIslands::` (fonctions pures, testées) : `ValidateDefinition`, `ValidateCatalog`, `BuildTravelURL`, `ParseFragmentOption`.
  - `UExpeditionIslandSubsystem` (`UGameInstanceSubsystem`, toutes machines) : charge catalogue puis définitions **en asynchrone** via l'Asset Manager (`LoadPrimaryAssets`, branches succès / échec / annulation, délai maximal), cache des définitions, **sélection serveur** (`SelectFragment`, refusée sur un client ou hors catalogue), URL de voyage pour T-088 (`<map>?Island=FRG_…`, option partagée avec T-093), et `ResolveFragmentIdForWorld` (option d'URL, puis sélection, puis registre d'assets par la map ouverte : couvre le PIE lancé directement sur `L_Frag_Raraka_01`).
- **Erreurs** : `EFragmentError` + `GetFragmentErrorText` (textes LOCTEXT, à basculer en String Table `ST_Fragments`) ; jamais de `check` sur une donnée d'asset ; une définition invalide est écartée du catalogue avec un `Warning`, un catalogue vide rend `CatalogEmpty`. Journal `LogMonkeIslands`.
- **Réseau** : le choix appartient au serveur (hub, L4). Seul `FragmentId` est répliqué, dans `AExpeditionGameState` (fixé par le serveur au `BeginPlay`, avant le départ de l'horloge) ; chaque client charge la définition localement par son id (nom affiché, crédits). Aucun RPC ajouté ici : la requête « choisir ce fragment » passera par le PlayerController (L4) et appellera `SelectFragment` côté serveur.
- **Mémoire** : définitions légères (map et aperçu en souple), gardées par le cache du subsystem et l'Asset Manager ; pas de `LoadSynchronous`. Callbacks liés faiblement, timers et handles annulés au `Deinitialize`.
- **Config** : `DefaultGame.ini`, `+PrimaryAssetTypesToScan` `ExpeditionIsland` et `IslandCatalog` dans `/Game/MonkeIsland/Data/Islands`, `CookRule=AlwaysCook` (la map suit par sa référence souple ; à vérifier au packaging, T-088).
- **Reste** : T-088 (voyage seamless, GameMode qui appelle `GetTravelURL`, `ZoneCount` vers les règles d'horloge), T-089 (assets `DA_Island_Raraka_01`, `DA_IslandCatalog`, `DA_FragmentRules`, `LI_ExpeditionCore`).

## Critères d'acceptation
1. Aucune frontière de fragment ne traverse la terre. Un test automatique sur `fragments.json` vérifie l'intersection entre frontière et terre, hors hoa synthétique.
2. Huit captures prises depuis la côte et depuis la barque, en faisant le tour : aucun bord, mur, motu tranché ni trou de terrain visible.
3. La barque fait le tour complet du fragment sans s'échouer sur une marche ni sortir du monde.
4. Les 3 villages, le marae et les 3 mouillages sont atteignables à pied, à la nage ou en barque. Le PlayerStart et la barque sont valides.
5. Le passage hub → fragment → hub fonctionne en solo, puis en hôte + client (PIE à 2), avec les mêmes valeurs des deux côtés.
6. Mémoire et temps de chargement mesurés et comparés à `Lvl_Raraka`, avec le budget de `docs/conventions/performance.md` et `memory.md`.
7. Données citées (Etalab 2.0, IRD) dans les crédits. Aucune imagerie Maxar embarquée.

## Hors périmètre
Plusieurs fragments en streaming dans une même partie ; génération de terrain au lancement ; autres atolls (plus tard, même pipeline) ; éditeur d'îles E1.
