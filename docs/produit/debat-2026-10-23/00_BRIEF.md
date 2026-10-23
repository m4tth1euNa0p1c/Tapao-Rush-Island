# Brief commun — débat pour le cahier des charges du produit « kit d'aléas côtiers »

## Contexte
- Dépôt : /home/user/Tapao-Rush-Island (jeu coop d'extraction maritime, UE 5.8, C++). Lire en priorité :
  - `docs/audit/01_REPO_MAP.md` (faits sur le dépôt, avec sources)
  - `docs/audit/02_ARCHITECTURE_PROPOSAL.md` (diagnostic, architecture en 6 couches, mécaniques, tranche verticale, risques)
  - `docs/audit/2026-10-04-commercial/SYNTHESE.md`, `81-positioning-usp.md`, `70-asset-licensing.md` (audit commercial existant)
  - `docs/CAHIER_DES_CHARGES.md` (contrat produit actuel du jeu, pour le format et le ton attendu)
- Les `.uasset` sont des pointeurs LFS : illisibles. Ne pas prétendre les avoir lus.

## Direction retenue par le propriétaire (à challenger, pas à répéter)
Un **produit dérivé de Tapao**, séparé du jeu : un plugin Unreal + un serveur MCP, « kit d'aléas côtiers et insulaires ».
- Utilisable sur **n'importe quelle map** (réelle ou inventée) : Landscape, heightmap, meshes.
- Le moteur déduit les **aléas plausibles du terrain et de l'environnement** (altitude, pente, distance à la mer, sol, végétation, exposition au vent, climat) : submersion, surcote cyclonique, houle, érosion/ensablement, passes et chenaux qui s'ouvrent ou se ferment.
- **Déterministe, seedé, rejouable, multijoueur** (même simulation sur chaque machine, serveur autoritaire).
- Pile en 6 couches (doc 02) : pipeline de données hors jeu → descripteurs d'environnement → règles d'éligibilité (DataAssets) → simulation → rendu (lit, ne calcule pas) → gameplay (les actions des joueurs modifient les descripteurs).
- Accroche : un **linter côtier dans l'éditeur** (« ton camp est submersible à +1,5 m », « ta seule route d'évacuation se coupe en premier »).
- **MCP** : `analyze_level`, `explain(point)`, `design_scenario`, `lint_level`. Principe : **l'IA explique et configure, elle ne décide jamais** (pas d'hallucination, reproductible).
- Contenu : packs de lieux « inspirés de » (Pacifique, atolls), reconstruits avec des sources libres (Copernicus DEM + ICESat-2, Sentinel-2, ESA WorldCover, OSM, côte Te Fenua Etalab). Jamais Google 3D Tiles, jamais Maxar, jamais les cocoteraies DAG.
- Deux publics : devs de jeux (Fab, volume) et serious games / risque littoral (B2B, valeur). Modèle : MCP gratuit, plugin payant, packs payants, licence B2B.
- Hors périmètre V1 : « créer ton jeu complet », volcans/feux/avalanches, cadastre (option B2B plus tard), lieux habités réels en 1:1 par défaut.
- Garde-fous : « plausible, jamais une prévision » ; pas de rejeu de catastrophes réelles avec victimes ; pas de rites/légendes réels ; pas de données personnelles (noms de propriétaires).
- Le dépôt Tapao reste le premier client et la vitrine ; le cœur doit être codé comme un module séparé, sans dépendance au gameplay de Tapao.
- Contrainte forte du propriétaire : il veut un **truc de niche, inexistant, à fort potentiel**. Pas une copie de ce qui existe. Il a aussi envie que les gens puissent choisir des lieux précis (Tuamotu, Pacifique, lieux connus).

## Ce que doit contenir chaque proposition de cahier (format imposé)
Un fichier Markdown en français, ≤ 2 500 mots, avec exactement ces sections :
1. **Nom et promesse en une phrase** (plus 3 noms alternatifs)
2. **Le problème que personne ne résout** (preuve ou raisonnement ; dire ce qui est vérifié et ce qui est supposé)
3. **Client cible n°1** (un seul, précis) et client n°2
4. **L'idée unique** : la mécanique / la capacité qui n'existe nulle part, et pourquoi elle est défendable 12 mois
5. **Périmètre V1 / V2 / jamais**
6. **Architecture technique** (classes, modules, format de données, MCP, déterminisme, perf) en s'appuyant sur ce qui existe dans le dépôt
7. **Démo de 30 secondes** qui fait comprendre le produit
8. **Modèle économique et prix** (ordres de grandeur, marqués comme estimés)
9. **Licences, éthique, données** (règles non négociables)
10. **Plan en 8 semaines** avec critère de fin vérifiable par semaine
11. **Les 5 raisons pour lesquelles ça peut échouer** et ce qu'on fait contre
12. **Ce que je rejette de la direction retenue** (au moins 2 points, argumentés)

Règles : français, direct, sans compliment. Chaque chiffre est soit cité (source) soit marqué « estimé ». Distinguer fait observé et recommandation. Pas de modification du dépôt : écrire uniquement le fichier demandé.
