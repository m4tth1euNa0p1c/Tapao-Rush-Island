# 03 — Cahier des charges « SURCOTE », version marché

Sources : lecture seule du dépôt (`docs/audit/01_REPO_MAP.md`, `02_ARCHITECTURE_PROPOSAL.md`, audit commercial 2026-10-04 : `SYNTHESE.md`, 70, 81, 85, 90). Six recherches web en résumés de moteur (pages non ouvrables, DNS bloqué) : tout chiffre web est « recherche, non relu à la source ». Le reste est **estimé**.

**Verdict** : les devs Unreal paient la vitrine, pas le loyer ; le vrai client est le bureau d'études littoral qui anime des ateliers pour des collectivités, et il n'achète pas un plugin, il achète un atelier qui marche.

---

## 1. Nom et promesse

**SURCOTE** — « Sur n'importe quel relief, dis-moi où l'eau arrive en premier, pourquoi, et montre-le à dix personnes dans la même pièce. »

Alternatives : **Hoa** (le chenal d'atoll qui s'ouvre et se ferme), **Trait de côte**, **Submersible**.

Vérifier INPI/EUIPO avant tout logo (l'audit 78 a perdu « Crownfall » ainsi).

## 2. Le problème que personne ne résout

**Vérifié dans le dépôt** (01_REPO_MAP §3) : aléas pilotés par le temps et des scripts, relief inventé (`build_terrain.py:46-59`), données en partie non diffusables (Maxar, DAG). Le moteur « aléas = terrain » n'existe pas encore.

**Vérifié par recherche** :
- Devs Unreal : les plugins d'eau vendent du **rendu**. Fluid Flux « from 349,99 $ », 160 avis ; Oceanology 189,99–249,99 $ (recherche : https://www.fab.com/search?tags=lake). Aucun ne dit « ton camp est submersible », aucun ne lit un DEM libre pour en déduire une exposition.
- Risque littoral : les jeux sérieux existent, **publics, 2D, académiques** : LittoSIM (La Rochelle, élus et agents, Oléron puis d'autres territoires), LittoWag (Occitanie) (recherche : https://innovationhub.eu-conexus.eu/innovation/3-LITTOSIM ; https://www.documentation.eauetbiodiversite.fr/fr/notice/littowag-a-serious-game-to-explore-adaptation-scenarios-in-front-of-coastal-risks-35861). En moteur de jeu, un prototype par territoire : CLIVE (Île-du-Prince-Édouard), MAGE (Morecambe) (recherche : https://richmond-news.com/pei-news/immersive-video-game-by-pei-researchers-reveals-vulnerability-to-flooding-erosion-9088963 ; https://earthlab.uw.edu/project/from-risk-to-resilience-connecting-communities-to-coastal-hazards-through-interactive-and-immersive-design/). Pacifique : plateforme web Coastal Risk Vanuatu, LiDAR financé par l'Australie, 190 personnes formées (recherche : https://geospatialworld.net/article/the-world-is-a-lot-bigger-than-spatial-industry-but-we-have-a-real-role-to-play). HORA 3D : 4,7/5 d'utilité sur 87 répondants (recherche : https://diglib.eg.org/handle/10.1111/cgf15110).

**Le trou (supposé, cohérent avec ces sources)** : chaque projet est un prototype à usage unique refait par un labo. Il n'existe pas de **moteur réutilisable sous licence, qu'un bureau d'études achète pour rejouer l'atelier sur la commune suivante en deux semaines**, avec une trace « pourquoi cette cellule » lisible par un élu. La niche est petite, mais vide.

**Non vérifié** : budgets réels des ateliers PAPI, part sous-traitée au privé, appétit des assureurs (sources académiques seulement).

## 3. Client cible

**Client n°1 (unique)** : le **chef de projet « concertation et risques côtiers » d'un bureau d'études littoral français** (type Créocéan, BRL, Artelia, Egis, indépendant GEMAPI), qui répond aux marchés des collectivités (PAPI, stratégie locale du trait de côte, PPRL) et anime des ateliers avec élus et habitants. Budget de prestation, pas de temps pour Unreal, besoin de crédibilité (« d'où vient ce chiffre ? »). Il paie **par projet**. Joignable : LinkedIn, Assises de la mer, journées ANEL, réseau LittoSIM.

Pas la commune : 6 à 18 mois d'achat, personne pour faire tourner un moteur. Pas le labo : il ne paie pas, il co-écrit un article ; c'est un **partenaire de légitimité**.

**Pacifique** : meilleur terrain de démonstration (atolls à +2 m, Te Fenua maîtrisé), mais payeur public et régional (CPS, SPREP, DIREN) à cycles longs : **client n°1 bis en année 2**.

**Client n°2** : le **dev indé Unreal** qui veut que sa map décide du danger. Il achète sur Fab, 99 à 199 $ ; c'est la **vitrine**, ses captures et avis font la preuve sociale pour le client n°1.

## 4. L'idée unique

**Le linter côtier expliqué** : pour toute heightmap, une carte d'exposition déterministe (submersion, surcote, coupure des routes) **et, par cellule, une chaîne de justification lisible** (« altitude 1,4 m, Copernicus calé ICESat-2 ; distance mer 38 m ; sable nu ; exposition NE 0,8 ; surcote 1,5 m ⇒ submergée à t+4 min »). Le même objet sert au game designer (« ta route s'évacue en dernier ») et à l'élu (« pourquoi ma rue est rouge »).

Défendable 12 mois parce que : (1) le **cœur est écrit et testé** : solveur d'eaux peu profondes pur et déterministe (`S/Flood/FloodGrid.h`), horloge pure, vent seedé, 107 specs (02 §A.1) ; (2) la **pipeline de données libres** (02 §B.2) est un travail ingrat que personne ne fait pour vendre 149 $ ; (3) la **trace de justification** exige la couche de descripteurs (02 §B.2-2), que les plugins d'eau n'ont aucune raison de construire ; (4) la **réputation de méthode** (plausible, sources par cellule) se gagne en ateliers, fossé lent mais réel.

À ne pas vendre comme unique : rendu d'eau, multijoueur en soi, « île réelle 1:1 » (audit 81 : invisible pour le joueur, risqué).

## 5. Périmètre

**V1 (8 semaines, pilote vendable)** : heightmap 16 bits géoréférencée **ou** Landscape UE, pas de mesh arbitraire ; un aléa, la submersion par surcote (hauteur, durée, direction) avec coupure des chemins ; linter éditeur, carte d'exposition, `explain(point)`, **rapport PDF/PNG** sourcé avec mention « plausible, non prévision » ; MCP `analyze_level`, `explain`, `lint_level` (pas `design_scenario`) ; déterministe et rejouable **en mono-machine**, réseau architecturé mais non promis ; un pack de démonstration « inspiré des Tuamotu », sources libres, motu renommés.

**V2 (mois 3 à 9)** : multijoueur validé sur deux PC ; érosion/ensablement ; houle et vent (ERA5) ; `design_scenario` ; mode « atelier » (vote d'aménagement, rejeu comparé) ; packs Polynésie et Manche/Atlantique.

**Jamais** : prévision ou calcul réglementaire (PPRL), cadastre, lieux habités réels nommés par défaut, catastrophes réelles avec victimes, Google 3D Tiles, Maxar, DAG, volcans/feux/avalanches, « crée ton jeu complet ».

## 6. Architecture technique

Plugin séparé `Plugins/Surcote` (puis dépôt propre), quatre modules, dépendances vers le bas (`docs/conventions/architecture.md`) :

| Module | Contenu | Origine dans le dépôt |
|---|---|---|
| `SurcoteCore` (sans UObject, testable hors moteur) | `FShallowWaterSolver` (`FFloodGrid` déplacé + rugosité), `FEnvDescriptorGrid` (SoA : élévation int16 cm, pente, distance mer, sol, végétation, rugosité, exposition[8], flags), `FHazardEligibilityCalculator` (pur), `FSurcoteSeed`, `FExplainTrace` | `S/Flood/FloodGrid.*`, `ExpeditionClock.cpp`, `WindMath.h` ; descripteurs à créer (02 §B.2-B.3) |
| `SurcoteRuntime` | `USurcoteDescriptorAsset` (crédits, hash des sources), `USurcoteSimulationSubsystem` (pas fixe 0,05 s, ≤ 4 pas/image), `USurcoteRuleSet` DataAssets, `USurcoteRenderBridge` (texture R16F à 5 Hz) | `AFloodSimulationActor` scindé, `U*Rules` |
| `SurcoteEditor` | Linter (surcouche, messages), importeur `.envgrid`, export rapport | nouveau |
| `SurcoteMCP` (Python) | Lit `.envgrid` et journal exportés en JSON ; `analyze_level`, `explain`, `lint_level`. **Ne calcule rien** | plugin MCP officiel UE 5.8 (`monke_island.uproject`) |

Données : `scripts/envgrid/build_envgrid.py` (GDAL, rasterio, pyproj, absents aujourd'hui) produit `.envgrid` + `CREDITS.json`. Format ouvert, versionné : l'actif B2B.

Déterminisme : CPU, pas fixe, descripteurs entiers, flottants en ordre fixe, CRC d'état exposé ; pas de GPU pour l'état qui fait foi (02 §B.2-4). Perf : 512² cellules ≤ 1 ms moyen (02 §B.5, **estimé**, mesure semaine 2).

Isolation : zéro include vers `Expedition/`, `Run/`, `Player/` ; Tapao consomme le plugin comme un tiers.

## 7. Démo de 30 secondes

Un Landscape quelconque. 0–5 s : clic « Lint ». 5–12 s : cellules rouges, bulle « Camp B submersible à +1,2 m ; la seule piste vers la crête se coupe à t+4 min ». 12–20 s : curseur de surcote 0 → 2 m, l'eau entre par le chenal, pas par la plage. 20–26 s : clic sur une cellule, fiche « pourquoi » sourcée. 26–30 s : « Exporter », PDF marqué « plausible, non prévision ». Même seed, même résultat.

## 8. Modèle économique et prix (tout estimé sauf mention)

**Marché devs Unreal** : Fab 2025, 420 000 listings, > 20 000 vendeurs, 24 M$ versés aux créateurs (recherche : https://www.unrealengine.com/news/fab-2025-year-in-review), soit ≈ 1 200 $ par vendeur et par an, très inégalement répartis. Un plugin de niche sans rendu : **50 à 200 ventes/an à 149 $ → 7 à 30 k$ brut**, part Fab 12 % (à vérifier). **Pas un business : une vitrine et un flux de leads.** Fluid Flux (350 $, 160 avis) plafonne le segment en vendant du spectaculaire ; nous vendons de l'invisible.

**Grille** :
| Offre | Prix | Rôle |
|---|---|---|
| MCP + linter en lecture seule | 0 | acquisition, preuve de méthode |
| Plugin `Surcote` sur Fab | 149 $ (lancement 99 $) | vitrine, avis, captures |
| Pack de lieu | 29–49 $ | marginal, démonstration |
| **Pilote bureau d'études** | 6–10 k€ (licence + 3 jours d'adaptation + atelier) | **premier chèque** |
| Licence annuelle BE, territoires illimités | 12–20 k€/an | récurrence |
| Territoire ajouté, via le BE | 2–4 k€ | volume |
| Éducation | gratuit avec attribution | légitimité |

**Ordres de grandeur** : année 1, 1 à 2 pilotes (10–20 k€) + Fab (5–10 k€) ; année 3, 5 à 10 clients BE, 80–150 k€/an. Un **studio d'outil à une ou deux personnes qui vit**, pas une fureur. La seule voie vers un « fort potentiel » est hors Unreal : une version web du linter (même `.envgrid`, même `explain`) pour agences d'urbanisme et assureurs ; HORA 3D et GeoFlood Studio montrent l'appétit (recherche : https://engineering.nyu.edu/news/nyu-tandon-researchers-launch-interactive-3d-flood-map-help-new-yorkers-visualize-climate). Pas la V1, mais la raison de construire le format de données proprement.

**Pourquoi maintenant** : Fab stabilisé (2025) ; plugin MCP officiel livré avec UE 5.8 (fait, dépôt) ; Copernicus DEM, WorldCover, ICESat-2 libres avec attribution (02 §B.2, juriste à consulter) ; loi Climat et résilience : liste des communes exposées au recul du trait de côte (≈ 300, **à vérifier**) et cartes locales obligatoires, donc des ateliers financés. L'audit 85 chiffre l'alternative : Tapao rapporte 14 k$ en an 1 (médian). Un seul pilote B2B rapporte autant.

## 9. Licences, éthique, données (non négociable)

1. Chaque couche a une licence écrite compatible usage commercial : Copernicus (attribution), ICESat-2 et IBTrACS (domaine public), WorldCover (CC BY 4.0), OSM (ODbL, partage des dérivés à cadrer), Te Fenua (Etalab 2.0 : producteur + date). **Interdits** : Maxar et dérivés (`build_terrain.py:238-284`), cocoteraies DAG, raster IRD, Google 3D Tiles (MAP §7, audit 70).
2. `CREDITS.json` par couche, affiché dans l'éditeur, le rapport et tout jeu qui embarque un pack.
3. Mention non désactivable : « Simulation plausible à visée pédagogique, fondée sur des données publiques ; ni prévision ni document réglementaire ». C'est aussi notre protection juridique.
4. Lieux « inspirés de », toponymes renommés ; lieu habité réel nommé seulement sous contrat avec la collectivité concernée, sans cadastre ni noms de personnes.
5. Pas de rejeu de catastrophes réelles avec victimes, pas de rites ni légendes réels.
6. Registre de provenance (outil, date, licence, SHA-256), code d'agents compris ; avocat PI avant la première facture.

## 10. Plan en 8 semaines

Pré-requis : rien n'est extrait, aucun lot du jeu validé (02 §B.8) ; **une personne à plein temps**, Tapao gelé.

| Sem. | Livrable | Critère de fin vérifiable |
|---|---|---|
| 1 | `scripts/envgrid/` : `.envgrid` + `CREDITS.json` depuis Copernicus, ICESat-2, WorldCover, OSM, côte Etalab, sur FRG_Raraka_01 renommé | Test Python : terre ≥ 95 % conforme à la côte Etalab ; altitude médiane des motu 1–4 m ; 0 entrée Maxar/DAG. **Testé hors moteur** |
| 2 | Plugin créé, `SurcoteCore` avec `FFloodGrid` déplacé + rugosité, `FSurcoteSeed` | Spec `Surcote.Determinism` : deux runs de 600 s, même seed, même CRC ; ≤ 1 ms moyen sur 512². **Compilé, testé éditeur**, CSV joint |
| 3 | Descripteurs, calculateur d'éligibilité, `explain(cell)` | Spec : 20 cellules de référence avec trace attendue ; même règle sur deux heightmaps ⇒ deux ordres d'inondation |
| 4 | Linter éditeur : surcouche, « submersible à +X m », « chemin coupé à t » | Rapport sans erreur sur 3 Landscapes de test, dont un non insulaire ; captures datées |
| 5 | Export PDF/PNG avec crédits et mention ; MCP `analyze_level`, `explain`, `lint_level` sur le JSON exporté | Démo de 30 s enregistrée ; MCP identique sur 10 appels |
| 6 | **Entretiens** : 8 chefs de projet BE/agences/labos (LittoSIM compris) voient la démo | 8 comptes rendus ; ≥ 3 « je paierais X pour un atelier » avec X écrit ; sinon pivot documenté en semaine 7 |
| 7 | Pack de démonstration, page Fab en brouillon, dossier pilote (devis 6–10 k€, mention légale) | Page Fab soumise ; devis envoyé à ≥ 2 prospects |
| 8 | Tapao consomme le plugin pour sa submersion ; perf ; build Development | 0 include du plugin vers le jeu ; PIE solo 60 FPS médian en submersion ; **testé build**. Go/no-go sur une réponse de pilote |

## 11. Cinq raisons d'échouer, et la parade

1. **Le relief d'atoll est sous le bruit des DEM mondiaux** (≈ 3 m de relief pour 2–4 m d'erreur, 02 §B.8-1) : un expert voit que la carte est arbitraire. Parade : calage ICESat-2, incertitude affichée par cellule dans `explain`, et premiers pilotes sur des côtes métropolitaines couvertes par Litto3D.
2. **Personne ne paie** : les ateliers restent faits par des labos sur fonds publics. Parade : couperet en semaine 6 ; sous 3 intentions chiffrées, Surcote redevient un plugin Fab et une brique de Tapao, sans société.
3. **Dispersion** : aucun lot du jeu validé, éditeur partagé, une seule personne. Parade : propriétaire unique du plugin, Tapao gelé 8 semaines, un seul aléa en V1.
4. **Licences** : un dérivé Maxar ou DAG traîne dans un pack vendu. Parade : purge testée en semaine 1, registre de provenance, revue juridique avant la première vente, aucun pack de lieu habité.
5. **Le « plausible » est pris pour une prévision** par un élu ou un journal. Parade : mention non désactivable, pas de cadastre, pas de lieu réel sans contrat, FAQ de crise écrite avant la première démo publique.

## 12. Ce que je rejette de la direction retenue

1. **« Plugin payant + packs payants = modèle »**. Rejeté comme revenu : 24 M$ répartis sur 20 000 vendeurs, plafond segment à 350 $ pour du rendu spectaculaire, et notre produit est invisible à l'écran. Le plugin est une vitrine à 149 $ ; le modèle est la licence par projet au bureau d'études. Si le propriétaire refuse le B2B, Surcote est un module interne de Tapao et un plugin d'appoint, pas un produit.
2. **« N'importe quelle map, meshes compris » en V1**. Rejeté : le mesh arbitraire oblige à garder les 16 000 lancers de rayons que l'architecture veut supprimer (02 §A.2-3), et aucun client n'en a besoin le premier jour. V1 = heightmap géoréférencée ou Landscape.
3. **Multijoueur promis en V1**. Rejeté : le client n°1 anime une salle avec un écran ; le dev indé veut d'abord un linter qui marche. L'architecture (entrées répliquées, CRC) est gardée, la preuve à deux machines est V2. Le dépôt n'a jamais prouvé une session à deux PC (SYNTHESE, bloquant 3).
4. **Lieux précis et connus comme argument grand public**. Rejeté : le 1:1 réel est sans effet sur le joueur (audit 81) et dangereux en réputation (72–73). Un lieu réel nommé ne vaut que signé par la collectivité concernée : livrable B2B, pas pack à 29 $.
5. **« Fort potentiel » tel que formulé**. Corrigé : le potentiel honnête est un studio d'outil à 1–2 personnes et une méthode reconnue dans un petit milieu. Le grand potentiel, s'il existe, est la version web du moteur de données pour urbanistes et assureurs, après deux pilotes ; d'où `.envgrid` et `explain` conçus dès la semaine 1 pour survivre sans Unreal.

Gardé : le linter comme accroche, « l'IA explique, ne décide jamais », le déterminisme CPU, les sources libres, Tapao comme premier client.
