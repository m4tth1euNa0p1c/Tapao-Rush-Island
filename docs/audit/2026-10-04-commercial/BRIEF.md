# Audit commercial — brief commun des auditeurs (2026-10-04)

Demande du propriétaire : « lance un audit optimisation flux gameplay pour optimiser un max la place etc. pour que cela devienne un jeu commercial ; crée plein d'agents (100) qui challengent notre technique, design, logique, gameplay, marketing, etc. pour constamment améliorer l'idée et résoudre les trucs de jeu manquants ».

Tu es **un des 100 auditeurs**. Chacun a un angle précis (ta mission est dans ton message). Sois **critique, concret et utile** : on veut transformer un prototype en jeu commercial vendable sur Steam.

## Le jeu en 10 lignes

- **Crownfall Expeditions** (nom provisoire), Unreal Engine 5.8, C++ + Blueprints, Windows, clavier/souris, interface française.
- Coop 1–4 joueurs (listen server), **extraction maritime**. Un équipage mandaté par une Couronne fictive (satire bureaucratique) va piller des îles condamnées. Le personnage est un « bean » soldat britannique façon Fall Guys, avec un ragdoll actif.
- **Cahier v1.3 (§24)**, partie en manches autour du galion. Le parcours :
  - menu → galion (lobby à bord) → chargement → galion ancré au large d'un fragment d'atoll tiré au hasard ;
  - barque physique à rames vers l'île : pacifier 3 zones (drapeau + sermon immobile de 20 s = +120 s), récolter, combattre les tiki, ramener le butin en barque ;
  - palan vers la cale → départ à la barre avant la submersion (120 s + 120 s par zone, puis 60 s de montée finale) ;
  - traversée → zone sûre (galion seul en mer) : vente objet par objet au Percepteur → quota atteint, manche suivante (quota 250, 400, 610, 880, 1 210… ×1,25 par joueur en plus) ; quota manqué, défaite.
- **Monde** : l'atoll réel de **Raraka** (Tuamotu, Polynésie française), à l'échelle 1:1 (29 × 26 km), découpé en 20 fragments séparés par la mer (ADR-004). Contenu fictif : villages japonais (60 villages de zone, 44 hameaux), marae, camps de pêcheurs, tiki (5 créatures : 3 guerriers, villageois, sarbacanier), requin. Le village de Motutapu est réel ; ses habitants ne sont pas représentés.
- **Systèmes** :
  - horloge commune, montée des eaux, tempête finale, cycle jour/nuit, vent ;
  - armes : sabre, mousquet, torche ;
  - cadavres portables découpables, armes de mobs lâchées, objets physiques avec une valeur qui baisse aux chocs (façon R.E.P.O.), butin commun ;
  - interface TAPAO (menu, HUD) ;
  - galion importé du 2e PC.

## État réel (ne pas supposer plus)

- 144 .cpp / 158 .h dans `monke_island/Source/monke_island/` : Ambience, Boats, Camps, Character, Creatures, DayNight, Economy, Emotes, Expedition, Flood, Foliage, Footsteps, Galleon, Hazards, Interaction, Items, Marae, Player, Run, Settings, Storm, Tools, UI, Village, Water, Weapons, plus `Variant_Horror` et `Variant_Shooter` (template).
- Tests Automation : 296 qui passent (headless), dont Run 9, Tiki 27, Village 13. **Multijoueur jamais testé sur deux machines.** Aucun build packagé joué hors éditeur.
- Tailles :
  - `Content` 1,2 Go ;
  - `Lvl_Raraka.umap` **110 Mo**, map monolithique non World Partition, plus de 816 000 instances de végétation PCG ;
  - `art/` 2,0 Go, `docs/` 2,2 Go.
- Flux réel : `L_MainMenu` (TAPAO) → `L_Boot` (tirage du fragment) → `Lvl_Raraka?Island=FRG_Raraka_NN`. La barque part au point d'arrivée ; le galion est ancré à 25 m, joueurs sur le pont.
  - Zone sûre, vente, manches : **pas encore codées**. Seul le calcul pur `FRunCalculator` existe (T-109).
  - Rien n'est fait pour la sauvegarde de partie, les sessions internet ou la VoIP.
- Organisation : plusieurs sessions Claude partagent un éditeur Unreal, un dépôt Git local et un tableau de chantier (`docs/production/board.md`, tâches T-NNN). Le propriétaire est seul humain : il teste et décide.

## Documents à lire selon ta mission

- **Toujours** : `docs/CAHIER_DES_CHARGES.md` (au moins §1, §3, §11, §24), `docs/adr/ADR-005-partie-en-manches.md`, `docs/production/backlog.md`.
- Selon le besoin :
  - `docs/adr/ADR-001` à `ADR-004` ;
  - `docs/features/*.md` (19 fiches) ;
  - `docs/production/board.md` ;
  - `docs/conventions/*.md` ;
  - `CLAUDE.md` ;
  - `art/SOURCES.md` (licences) ;
  - le code dans `monke_island/Source/monke_island/<Dossier>/` ;
  - les scripts dans `scripts/`.
- Des rapports d'autres auditeurs peuvent apparaître dans ce dossier : tu peux les lire, mais ne les répète pas.

## Règles strictes

1. **Lecture seule**, sauf ton unique rapport. Ne modifie aucun autre fichier.
2. **Interdit** : MCP Unreal (`mcp__unreal__*`), Blender, builds (`Build.bat`, `ue.ps1`), toute commande git qui change l'état (`add`, `commit`, `checkout`…), lancer l'éditeur ou un jeu, SendMessage aux autres sessions. L'éditeur est partagé et le propriétaire est peut-être en train de jouer.
3. **Recherche web** : autorisée seulement si ta mission porte sur le marché, la concurrence, le juridique ou les plateformes. 6 recherches au plus. Cite tes sources (URL).
4. **Honnêteté** : distingue ce que tu as **vu** (fichier, ligne, chiffre) de ce que tu **supposes**. Pas d'invention de fonctionnalités existantes.
5. **Rapport en français**, 900 mots maximum, dans `docs/audit/2026-10-04-commercial/<NN>-<slug>.md`, au format ci-dessous.
6. **Réponse finale** (ton dernier message) : 120 mots au plus, avec ton verdict et tes 3 recommandations prioritaires.

## Format du rapport

```markdown
# <NN> — <Titre de la mission>
Domaine : <A–J> · Auditeur : <persona>

## Verdict
3 phrases au plus : où en est le jeu sur ton angle, et le principal risque commercial.

## Constats
Un bloc par constat (5 à 10) :
- **[Bloquant commercial | Majeur | Mineur] Titre court**
  - Constat : …
  - Preuve : fichier:ligne, doc §, chiffre, ou « supposé »
  - Recommandation : action concrète
  - Effort : S (≤ 1 j) / M (≤ 1 sem.) / L (> 1 sem.)

## Manques
Fonctionnalités ou contenus absents qu'un jeu commercial de ce genre devrait avoir (liste courte, priorisée).

## Quick wins
3 à 5 actions de moins d'un jour, à fort impact.

## Questions qui challengent l'équipe
3 questions dérangeantes auxquelles le propriétaire doit répondre.
```
