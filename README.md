<p align="center">
  <img src="docs/images/logo.png" alt="TAPAO : Rush Island" width="440">
</p>

<p align="center">
  <b>Jeu coopératif d'extraction maritime, de 1 à 4 joueurs, sous Unreal Engine 5.8</b><br>
  <a href="https://github.com/m4tth1euNa0p1c/Tapao-Rush-Island/releases/latest">Dernière version</a> ·
  <a href="docs/CAHIER_DES_CHARGES.md">Cahier des charges</a> ·
  <a href="docs/narrative/BIBLE.md">Bible de l'univers</a> ·
  <a href="docs/production/board.md">Chantier</a> ·
  <a href="https://github.com/m4tth1euNa0p1c/Tapao-Rush-Island/issues">Tickets</a>
</p>

---

## Le jeu

> Une Couronne d'opérette envoie des équipages de haricots en habit rouge vider les îles de l'Anneau avant que la mer ne les reprenne.

Chaque jour, l'équipage quitte le galion, rame jusqu'à l'île du jour, hisse les couleurs dans les villages, pille marae, temples et forts français, puis rapporte tout à bord avant le déluge. Une campagne dure dix jours, découpés en trois contrats à quota. Si un quota est manqué, l'équipage est déclaré traître et remis au peloton français.

- **Époque** : XVIIIe siècle de fantaisie : habits rouges, tricornes, mousquets, galion.
- **Ton** : aventure, peur la nuit, chaos coopératif et satire de la bureaucratie royale.
- **Décor** : un atoll des Tuamotu à l'échelle 1:1, découpé en 20 îles jouables.

<p align="center">
  <img src="docs/images/galion.jpg" alt="Le galion au mouillage" width="49%">
  <img src="docs/images/drapeau.jpg" alt="La cérémonie des couleurs dans un village" width="49%">
  <img src="docs/images/bean.jpg" alt="Le bean soldat britannique" width="24%">
  <img src="docs/images/veilleurs.jpg" alt="Les Veilleurs, la nuit" width="74%">
</p>

## État au 12/10/2026

| Élément | État |
|---|---|
| Dernier exécutable jouable | **v2.0** du 11/10, Windows 64 bits, testé en solo ([release](https://github.com/m4tth1euNa0p1c/Tapao-Rush-Island/releases/latest)) |
| Code source | **v2.2.0** : build éditeur réussi, 1 085 tests automatiques sans échec |
| Boucle complète en solo | Vérifiée en build : galion, barque, île, drapeau, pillage, déluge, bilan, jour suivant |
| Multijoueur | Codé (hôte et jusqu'à 3 clients par IP), **jamais testé sur deux PC** |
| Recette du cahier | Aucun lot validé, voir [la recette du 11/10](docs/production/recette-2026-10-11.md) |

Les tickets marqués **preuve: acceptation sans test** ont été validés sans recette dédiée : ils restent à vérifier en jeu.

## Jouer

1. Téléchargez `Taopao-Rush-Island-v2.0.zip` depuis la [dernière release](https://github.com/m4tth1euNa0p1c/Tapao-Rush-Island/releases/latest), puis extrayez-le.
2. Lancez `monke_island.exe`.
3. **Multijoueur** : l'hôte choisit *Créer une partie*, les autres choisissent *Rejoindre* avec son adresse IP. Hors réseau local, l'hôte doit rediriger le port UDP 7777.

| Touche | Action |
|---|---|
| ZQSD, souris | Se déplacer, regarder ; ramer dans la barque |
| Espace · Maj · Ctrl | Sauter · courir · s'accroupir (plonger dans l'eau) |
| Clic gauche · clic droit | Frapper, tirer, lancer · parer, lunette, baïonnette |
| F (appui ou maintien) | Interagir · soulever un objet · menotter · lever l'ancre (2 s) |
| E · 1 à 6 · G | Inventaire · cases · lâcher |
| R · A · O · T | Recharger · trompette · voter le départ · parler |

## Développer

**Prérequis** : Windows 10 ou 11, Unreal Engine 5.8.3, Visual Studio 2022 avec C++, Git et Git LFS, Python 3.12, environ 25 Go libres.

```powershell
git lfs install
git clone git@github.com:m4tth1euNa0p1c/Tapao-Rush-Island.git
cd Tapao-Rush-Island
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 build              # compile (éditeur fermé)
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 editor             # ouvre l'éditeur
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 test -Filter monke_island
python scripts/chantier.py                                                            # tableau de chantier
```

> [!WARNING]
> Le dépôt pèse environ 8,5 Gio en Git LFS. Le quota gratuit de GitHub est de 10 Gio de téléchargement LFS par mois : un clone complet en consomme l'essentiel. Pour seulement jouer, prenez le zip de la release.

Le projet Unreal est `monke_island/monke_island.uproject`, et la carte principale est `/Game/MonkeIsland/Maps/Lvl_Raraka`. Les conventions de code et de contenu sont dans [`CLAUDE.md`](CLAUDE.md) et [`docs/conventions/`](docs/conventions).

## Organisation du dépôt

```text
monke_island/              projet Unreal (monke_island.uproject)
  Source/monke_island/     module C++, un dossier par feature (Run, Galleon, Boats, Weapons, Capture…)
  Content/MonkeIsland/     contenu du jeu (cartes, personnages, armes, UI, audio)
  Config/                  configuration du projet
docs/                      cahier des charges, ADR, specs des features, narration, production, QA, audits
art/                       sources Blender, exports FBX, aperçus, textures et HDRI CC0, interface
scripts/                   outillage : build, chantier, carto, props, éditeur, UI
sprite/                    planches de référence des personnages
.claude/                   usine agentique : agents, skills, charte (factory/FACTORY.md)
.codex/                    agents Codex
```

## Branches, versions et tickets

| Référence | Rôle |
|---|---|
| `main` | Développement intégré : toute modification passe par une pull request |
| `ga` | Dernière version publiée, avancée à chaque release |
| `dev/T-NNN` | Branches de travail de l'usine, une par ticket |
| `pr/NN-domaine` | Lots d'intégration par domaine du 12/10/2026 |
| `vX.Y.Z` | Tags de version ; l'exécutable Windows est joint à la release |

- **Tickets** : l'issue **#N** correspond au ticket **T-N** du [chantier](docs/production/board.md). Les étiquettes donnent le statut, le lot du cahier et le domaine.
- **Vérifications** : GitHub Actions contrôle chaque pull request (fichiers binaires sous LFS, scripts Python, chantier, messages de commit).
- **Historique** : il a été réécrit le 12/10/2026 avec un auteur unique et sans co-auteur IA. Les anciens SHA cités dans les documents se retrouvent dans [`docs/production/sha-map.md`](docs/production/sha-map.md).

## Règles de contribution

- Les commits et les pull requests sont au nom de **Matthieu Barraque**, sans ligne de co-auteur IA. Le CI refuse les lignes `Co-Authored-By: Claude`.
- Format des messages : `domaine: description (T-NNN)`.
- Une tâche ne passe à « fait » qu'avec une preuve : `compilé`, `testé éditeur`, `testé build` ou `testé multi-machines` (cahier §2 et §13).

## Données, licences et crédits

- **Atoll de Raraka** : données Te Fenua / DAF, sous Licence Ouverte Etalab 2.0.
- **Imagerie © Maxar via Te Fenua** (`docs/reference/map/raraka/*sat*`) : référence de modélisation uniquement, jamais embarquée dans le jeu. Le dépôt est privé ; ces fichiers doivent être retirés de l'historique avant toute ouverture publique.
- **Couche des cocoteraies (DAG)** : réservée à la recherche et à l'éducation, à régulariser avant toute diffusion (T-130).
- **Textures et HDRI** : CC0 (Poly Haven, ambientCG), sources dans [`art/SOURCES.md`](art/SOURCES.md).
- **Polices** : Pirata One et Alegreya SC, sous licence OFL.
- Le village de Motutapu est réel ; les personnages et les événements du jeu sont fictifs.
- Aucune licence de diffusion n'est accordée sur le code et les assets originaux : tous droits réservés.
