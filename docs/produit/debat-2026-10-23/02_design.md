# 02 — Cahier des charges, version design : « la mer lit ton terrain »

Sources : `docs/audit/01_REPO_MAP.md` (MAP), `02_ARCHITECTURE_PROPOSAL.md` (ARCH), audit commercial 2026-10-04 (rapports 70, 81, SYNTHESE), cahier, bible narrative. Les `.uasset` sont des pointeurs LFS : rien n'a été ouvert ni joué. Tout chiffre non sourcé est **estimé**.

## 1. Nom et promesse

**HIGHWATER** — *Le plugin Unreal qui fait de n'importe quel terrain un adversaire : la mer arrive là où ton relief le permet, dans l'ordre qu'il dicte, et tout le monde la voit venir.*

Alternatives : **Tideline**, **Hoa**, **Rempart**.

Trois verbes : **lire** (où ça inonde), **jouer** (la même marée sur 4 machines), **payer** (chaque action sur le terrain change le risque).

## 2. Le problème que personne ne résout

**Fait observé.** Dans le dépôt, la montée des eaux est un chronomètre identique sur les 20 fragments ; le terrain n'intervient qu'en aval (ARCH A.2-1, `ExpeditionClock.cpp:22-25`). Et elle a trois quasi-jumeaux sur Steam : *Flood With Friends*, *Rapid Raccoons*, *Are We Sinking Yet?* (rapport 81). « L'eau monte » est banal ; **aucun de ces jeux n'explique pourquoi elle monte ici d'abord**, et aucun ne laisse le joueur changer la réponse.

**Ce que les références n'ont pas** (mémoire de joueur, non revérifié : **supposé**) :
- *Cities: Skylines* : eau simulée, tsunamis ; aucune lecture *avant* l'événement, pas de déterminisme multijoueur, aucun lien entre ce qu'on a bâti et la vitesse de l'eau.
- *Timberborn* : l'eau suit le relief, on bâtit des digues ; pas de mer, solo, la sécheresse est un calendrier.
- *Frostpunk* : la pression vient du temps, jamais de l'espace.
- *Valheim*, *Raft*, *Stranded Deep* : la mer est une surface, jamais un système qui lit la terre.
- *Flood With Friends* : l'île coule à vitesse uniforme (supposé d'après la page).

**Le trou** : aucun outil ne transforme un terrain quelconque en **risque lisible, ordonné et modifiable**, identique pour 4 joueurs. Chaque studio recode un solveur, une UI d'alerte et une synchronisation, et le risque reste un compteur en haut de l'écran, pas une chose qu'on voit sur le sol.

**Vérifié** : un solveur d'eaux peu profondes pur et déterministe existe (`S/Flood/FloodGrid.h:7-14`), 1 158 tests passent (MAP §6), le réseau réplique l'horloge, pas l'état (`FloodSimulationActor.cpp:163-169`). **Supposé** : qu'un développeur paiera pour ne pas le refaire.

## 3. Clients

**Client n°1 (le seul qui compte en V1)** : le studio indé de 2 à 8 personnes qui fait un jeu coop (survie, extraction, « friendslop ») sur UE5, qui a déjà une map et veut une menace spectaculaire et honnête en moins d'une semaine d'intégration. Il achète sur Fab, juge en 30 secondes de vidéo et **montre le plugin dans son propre trailer** ; sinon il ne l'achète pas.

**Client n°2** : le studio de serious game qui sensibilise au risque littoral pour une collectivité. Même produit, plus un importateur d'emprise réelle et un rapport de plausibilité. **Rien de spécifique en V1** : on lui vend une licence, pas du développement.

## 4. L'idée unique

Quatre choses, par ordre d'importance.

**a) La marée annoncée : le risque est un objet du monde, pas un compteur.** Chaque point du terrain porte `TimeToFlood` (secondes avant l'eau dans le scénario en cours) et `FloodOrder` (rang d'inondation). Le développeur l'affiche sur ce qu'il veut : sol qui s'assombrit, ligne de varech, chiffre au-dessus d'une caisse, route qui clignote. **Le joueur lit le risque sur le sol avant de lire l'horloge.** Aucun jeu cité ne fournit cette valeur par point.

**b) Ce que tu prends, la mer le prend.** Toute action sur le terrain (couper la végétation de rivage, arracher le corail, creuser, poser une fascine, replanter) est une écriture dans les descripteurs, validée par le serveur, qui **change l'ordre et la vitesse d'arrivée de l'eau là où l'équipe se trouve**. ARCH B.6 note la piste 5/5 en originalité : aucun jeu cité ne lie l'extraction au risque. C'est le clip : « on a tout pris, et la mer est entrée par là où on a tout pris ».

**c) Les passes vivantes.** Les hoa et chenaux s'ouvrent et se ferment ; une barque passe là où il n'y avait pas de passage trois minutes avant, un chemin à pied disparaît. Le dépôt a déjà une barque physique (`S/Boats/`) et des fragments découpés par les hoa (ADR-004).

**d) Les signes calculés, jamais scriptés.** L'eau dans les creux, la houle qui change de période, les oiseaux qui quittent les motu exposés : des **sorties de la simulation**, donc toujours vraies. Bible : phénomènes naturels, aucun rite ni savoir inventé.

**Défendable 12 mois** : a) et b) exigent ensemble une grille de descripteurs lue par la simulation, un solveur déterministe CPU, une synchronisation par somme de contrôle et la preuve multi-machines. Chaque brique se copie ; l'ensemble coûte à un concurrent le temps qu'il nous faut pour sortir trois packs et une communauté qui montre ses clips.

**Jargon rejeté** : « exposition au vent par secteur », « rugosité de Manning », « climat ERA5 » sont des entrées internes. Le joueur voit trois choses : **où**, **quand**, **à cause de quoi**. Une couche qui ne change pas l'une de ces réponses à l'écran n'est pas en V1.

## 5. Périmètre

**V1 (8 semaines) — l'eau seulement, mais toute l'eau.**
- Submersion par surcote seedée ; l'eau entre par le relief.
- `TimeToFlood`, `FloodOrder`, passes ouvertes/fermées, 3 signes calculés.
- Édition de descripteurs par le gameplay : `Clear`, `Dig`, `Fill`, `Plant`, à effet mesurable.
- Linter d'éditeur avec **curseur de niveau de mer** : glisser de +0 à +3 m et voir l'île s'inonder dans l'ordre, camp, route et points d'intérêt annotés.
- Déterminisme multijoueur prouvé sur deux PC.
- Entrée : Landscape, heightmap PNG/R16 ou meshes de collision (cuits une fois en grille).
- Un pack « Atoll » inspiré des Tuamotu.
- MCP : `analyze_level`, `explain(point)`, `lint_level`. Pas `design_scenario` (§12).

**V2.** Érosion et ensablement persistants ; vent et embruns ; cyclone avec trajectoire ; packs « Barrière », « Delta », « Fjord » ; importateur d'emprise réelle (WGS84 → grille) pour le client n°2 ; `design_scenario`.

**Jamais.** Volcans, feux, avalanches ; « crée ton jeu complet » ; cadastre ; lieux habités réels en 1:1 par défaut ; rejeu de catastrophes réelles ; Google 3D Tiles, Maxar, cocoteraies DAG ; une prévision.

## 6. Architecture technique

Deux modules Unreal et un serveur, sans dépendance au gameplay de Tapao (ARCH B.2–B.5). ★ = existe déjà, déplacé et non réécrit.

| Couche | Élément | Origine |
|---|---|---|
| Descripteurs | `UEnvGridAsset` (`UPrimaryDataAsset`, SoA, pas 4 m : altitude, sol, couverture, drapeaux) ; `UEnvGridSubsystem` (`Sample`, `Query`, `Apply(FDescriptorEdit)`, `Version`) | Nouveau. Landscape → grille ; meshes → **cuisson unique** à l'import (remplace les 16 000 rayons par image, `FloodRules.h:132-140`) |
| Règles | `UHazardRuleSet` (DataAsset), `FHazardEligibility` (pure) → `FExposureMap` | Nouveau, modèle `FRunContractCalculator` |
| Simulation | `FShallowWaterSolver` ★ (ex-`FFloodGrid` + frottement par cellule), `UHazardSimSubsystem` à pas fixe 0,05 s, `FHazardSeed` (une seed, sous-flux nommés) | `S/Flood/FloodGrid.*`, `S/Storm/WindMath.h:25-29` |
| Lecture gameplay | `UHazardQueryLibrary` (Blueprint) : `TimeToFlood(FVector)`, `FloodOrder`, `IsPassable(A, B, t)`, `GetSigns()` ; tags `Hazard.Flood.Reached`, `Hazard.Pass.Opened` | Nouveau : **c'est l'API vendue** |
| Rendu | `UHazardRenderBridge` : texture R16F profondeur + ordre, 5 Hz, MPC ; `AFloodSurfaceActor` ★ purement visuel | `FloodSimulationActor.cpp:237-361` |
| Réseau | `UHazardSyncComponent` : CRC32 toutes les 5 s, journal des `FDescriptorEdit` en `FFastArraySerializer`, instantané correctif | Nouveau |
| Éditeur | `HighwaterEditor` : panneau linter, curseur de niveau de mer, `HazardAnnotationActor` (camp, route, POI), rapport JSON | Nouveau |
| MCP | Serveur Python stdio, **gratuit**, qui n'appelle que les commandes de l'éditeur : il formule, il ne calcule rien | Modèle : plugin MCP du `.uproject` |

**Déterminisme** : CPU pour l'état qui fait foi, flottants en ordre fixe ; GPU réservé au visuel ; jamais l'état complet répliqué (ARCH B.2-4).

**Performance (estimée, mesurée S2)** : 512² à 6 m, ≤ 1 ms, ≤ 40 Mo, ≤ 1 ko/s (ARCH B.5).

**Formats** : `.envgrid` (en-tête JSON + couches binaires), `CREDITS.json` par pack, `scenario.json` (seed, courbe de surcote, règles, journal d'édits) : un scénario se rejoue depuis ce fichier seul.

**Tapao reste le premier client** : `S/Flood/` et `S/Storm/` consomment le plugin ; le cahier v1.4 garde son contrat de temps (déluge à 9 min), le plugin décide *où* et *dans quel ordre*.

## 7. La démo de 30 secondes

Une prise, sans voix.

- **0–8 s, éditeur.** Une île vue de haut, un camp, une route. La souris glisse le curseur « Sea level » de +0,0 à +2,4 m. L'eau entre par les creux, remonte le hoa, coupe la route, cerne le camp. Étiquette : *« Camp floods at +1.5 m. Your only exit cuts first, at +1.2 m. »*
- **8–12 s.** Bouton *« Same scenario, 4 players »*.
- **12–26 s, jeu.** Quatre beans sur la plage, lignes de varech qui remontent. Un joueur coupe une haie de rivage pour passer avec une caisse ; la ligne saute de deux mètres vers le camp ; un coéquipier écrit *« WHY DID YOU CUT THAT »*. L'eau arrive, la barque passe par le chenal qui vient de s'ouvrir, un bean reste sur la route coupée.
- **26–30 s.** Carton : **« HIGHWATER — your terrain is the enemy. Same tide on every machine. »** Logo Fab.

Critère : un développeur comprend sans lire ; un joueur partage le clip du bean abandonné.

## 8. Modèle économique et prix (tout est estimé)

| Offre | Prix estimé | Rôle |
|---|---|---|
| MCP + linter en lecture seule | Gratuit | Acquisition |
| Plugin HIGHWATER (runtime + éditeur) | 99 à 149 € HT sur Fab, par siège | Revenu volume |
| Packs « Atoll », puis « Barrière », « Delta » | 19 à 29 € chacun | Marge et nouveaux clips |
| Licence B2B (importateur, rapport de plausibilité, support) | 3 000 à 8 000 € HT par an et par projet | Valeur ; V2 |

Hypothèses : commission Fab ≈ 12 % (à revérifier) ; 150 à 400 ventes la première année (**estimé**, aucune donnée Fab consultée). Au plancher ≈ 15 k€ : pas un salaire, le financement du pack suivant et la preuve de marché pour le B2B.

## 9. Licences, éthique, données (non négociables)

1. Sources libres seulement : Copernicus DEM, ICESat-2, Sentinel-2, ESA WorldCover (CC BY 4.0), OSM (ODbL, attribution), côte Te Fenua (Etalab 2.0, producteur et date). **Jamais** Google 3D Tiles, Maxar, cocoteraies DAG (rapport 70 ; MAP §7).
2. `CREDITS.json` généré par le pipeline, affiché par un widget fourni, livré avec `Licenses/`.
3. Chaque scénario porte « plausible, pas une prévision » ; aucune fonction ne renvoie une période de retour.
4. Packs nommés par **archétype**, pas par toponyme habité ; aucun village réel en 1:1 par défaut ; l'importateur (V2) refuse une emprise à bâti dense sans la case « usage interne, non distribué ».
5. Pas de rejeu de catastrophe réelle, pas de rite ni de légende réels, pas de données personnelles.
6. Registre de provenance pour tout asset, origine IA comprise (rapport 70).
7. Avis de juriste avant la première vente ; ce document n'en est pas un.

## 10. Plan en 8 semaines

| Semaine | Livrable | Critère de fin vérifiable |
|---|---|---|
| S1 | Module `Highwater` compilé dans un projet UE 5.8 **neuf** ; `FShallowWaterSolver` et `FHazardSeed` déplacés avec leurs tests | Les specs de `FloodGrid` passent dans le projet neuf. `testé éditeur` |
| S2 | `UEnvGridAsset` + importateur Landscape et heightmap ; zéro rayon par image | Deux runs de 600 s, même seed, même CRC. `stat` : ≤ 1 ms moyen sur 512². `testé éditeur` |
| S3 | `TimeToFlood`, `FloodOrder`, `IsPassable`, tags ; widget ligne au sol | Heightmap de test à 3 bassins : l'ordre prédit par `FloodOrder` est l'ordre observé en PIE, 10 runs sur 10 |
| S4 | Linter : curseur, annotations, rapport JSON ; MCP `analyze_level`, `lint_level`, `explain` | Le linter annonce « route coupée à +1,2 m » et la valeur est retrouvée en PIE à ± 0,1 m |
| S5 | `FDescriptorEdit` serveur : `Clear`, `Dig`, `Fill`, `Plant` | Dégager 200 cellules de rivage avance l'eau au camp d'au moins 20 s (DataAsset) ; `Plant` la retarde |
| S6 | `UHazardSyncComponent` ; hôte + client sur **deux PC** | 0 écart de CRC sur 3 scénarios de 10 min avec 50 édits côté client. `testé multi-machines` |
| S7 | Pack « Atoll » : `scripts/envgrid/` sur sources libres, `CREDITS.json`, 1 île ; 3 signes calculés | Terre de la grille recouvre la côte Etalab à ≥ 95 % ; motu entre 1 et 4 m ; playtest à 3 : 2 sur 3 prédisent la première zone inondée |
| S8 | Démo de 30 s tournée, page Fab, `S/Flood/` de Tapao consomme le plugin | Clip terminé ; Tapao compile et ses tests passent ; package Development. `testé build` |

Rien n'est « fait » sans la preuve exigée par le cahier (§2, §13).

## 11. Cinq raisons d'échouer, et la parade

1. **Le relief d'un atoll est sous le bruit des DEM mondiaux** (≈ 3 m de relief pour 2 à 4 m d'erreur, ARCH B.8-1). Parade : caler sur ICESat-2, contraindre par la côte et le récif, et **ne pas vendre le pack Atoll comme preuve** : le plugin se vend sur une heightmap de test lisible.
2. **Le développeur ne voit pas de différence avec un volume d'eau animé.** Parade : la démo commence par le curseur ; `TimeToFlood` est la première ligne de la page Fab ; projet d'exemple avec route coupée.
3. **Désynchronisation réseau.** Parade : CPU, pas fixe, CRC toutes les 5 s, instantané correctif, critère S6 avant toute vente.
4. **Dispersion** : six aléas, deux publics, un MCP, des packs, pendant que Tapao n'a pas de boucle fermée (SYNTHESE) et 147 tickets en review (ARCH B.8-5). Parade : V1 = l'eau seule ; branche séparée ; Tapao ne reçoit le plugin qu'en S8.
5. **Un plugin que personne ne montre.** Parade : dès S4, chaque semaine finit par une capture de 10 s publiable. Si S4 ne produit pas un GIF qu'on a envie d'envoyer, on corrige la lisibilité avant de continuer.

## 12. Ce que je rejette de la direction retenue

1. **Six aléas en V1 (submersion, surcote, houle, érosion, ensablement, passes).** Non. Érosion et ensablement sont invisibles en 10 minutes et notés 2/5 en faisabilité (ARCH B.6). La houle n'est qu'un signe. V1 = **une** menace, l'eau, avec trois lectures : quand, où, à cause de quoi. Six aléas moyens ne font pas un clip ; une menace parfaite en fait dix.
2. **Le MCP comme accroche.** Un serveur MCP ne se montre pas dans une vidéo et ne se vend pas sur Fab. Il reste gratuit et utile ; l'accroche est **le curseur de niveau de mer**, visuel, sans IA, compris en huit secondes. `design_scenario` sort de la V1 : une IA qui configure un scénario sans playtest livre des scénarios que personne n'a joués.
3. **« Choisir des lieux précis » comme argument.** Le joueur ne distingue pas réel et plausible (ARCH B.6, faisabilité 2), et le réel coûte en licences, consultation et réputation (rapport 70, précédent Motutapu). V1 vend des **archétypes** et un importateur libre pour qui veut son emprise ; les packs nommés viennent après un avis juridique et un premier revenu.
4. **Les deux publics à égalité.** Le B2B tire vers la prévision et le cadastre, ce que les garde-fous interdisent. En V1 il achète ce que le client n°1 achète ; pas une ligne de code pour lui avant V2.
5. **Vent et climat comme descripteurs d'entrée.** Deux couches à sourcer, expliquer et tester pour un effet que le joueur ne distingue pas de la surcote. Retour en V2 avec le cyclone, seul aléa où le vent se voit.
