# 81 — Positionnement et USP face au genre
Domaine : I · Auditeur : stratège produit

## Verdict
Le jeu n'a pas de « pourquoi nous » : le codé emprunte au genre, le distinctif (sermon, vente, Percepteur) est absent de `main`. Barque physique et montée des eaux ont déjà trois voisins directs en 2026. Risque : arriver tard avec des ingrédients communs et une fiction invisible, seul fossé défendable.

## Constats
- **[Bloquant commercial] Le projet se décrit en « façon X », sans « pourquoi nous »**
  - Constat : le chantier nomme ses briques « façon Lethal Company / R.E.P.O. / Content Warning / Fall Guys » ; le cahier §1 décrit le jeu, pas ce qui le distingue. Le parcours compte 11 étapes (§24.2) ; le genre se dit « easy to learn ».
  - Preuve : board.md:34,35,38,40,41 ; `AddZoneBonus` sans appelant hors tests (ExpeditionGameState.cpp:122) ; aucune classe Percepteur, sermon, barre, palan (01, 03, 12) ; recherche « friendslop » (extraits) : https://knowyourmeme.com/editorials/guides/whats-the-term-for-games-like-lethal-company-and-repo-people-calling-co-op-games-friendslop-explained, https://exploringthegames.substack.com/p/what-is-friendslop-lethal-company-co-op-games.
  - Recommandation : **« Le coop de pillage où la cupidité se pèse, se noie et se facture »** ; pitch en trois verbes, piller, ramer, vendre ; raccourci externe : « Lethal Company en plein jour, à la rame, sous une Couronne absurde ».
  - Effort : S

- **[Majeur] Barque, île qui coule, quotas : trois voisins directs en 2026**
  - Constat : *Flood With Friends* (« the island goes under », 1–8 joueurs, 5,19 €), *Rapid Raccoons* (« only what reaches the hold counts », quotas croissants, accès anticipé 1er déc.), *Are We Sinking Yet?* (sorti 16 juil., 8,15 €, 3 avis). Aucun ne montre fiction coloniale, galion, rituel ni succès visible (pages lues) ; ailleurs, absence non prouvée (6 requêtes).
  - Preuve : https://store.steampowered.com/app/5005150/ ; https://store.steampowered.com/app/4896950/Rapids ; https://store.steampowered.com/app/4466930/Are_We_Sinking_Yet/.
  - Recommandation : ne plus vendre « physique » ni « montée des eaux » comme unicité ; vendre la recette et la fiction ; playtest public dès la tranche verticale (84).
  - Effort : S

- **[Majeur] Triage : une idée neuve, deux contestées, le reste copié ou invisible**
  - Constat : neuve mais injouable, le sermon qui achète du temps (supposé neuf, non recherché). Contestées : barque surchargée (meilleur faiseur de clips, naufrage sans son ni effet), montée des eaux (meilleur spectacle). Copies : quota, acheteur-autorité, élimination (Lethal Company), valeur aux chocs (R.E.P.O.), bras en 1re personne (Content Warning), bean (Fall Guys ; ragdoll actif non codé). Sans effet pour le joueur : atoll 1:1. Sea of Thieves (navigation libre), Raft (construction), Dredge (solo) : comparaisons perdantes, galion immobile.
  - Preuve : BoatActor.cpp:956-959 (message de debug) ; board.md:34 ; cahier §24.3 ; https://www.timeextension.com/games/pc/lethal-company (extrait).
  - Recommandation : barque et marée en spectacle, Couronne en preuve ; « un atoll qui coule », jamais « Raraka » (72) ; tags Co-op, Physics, Comedy, Satire, pas « pirates » ni « survie » (supposé).
  - Effort : S

- **[Majeur] Le fossé est la fiction ; le sermon en est la scène-signature**
  - Constat : physique et décor s'imitent (supposé), quota et autorité sont ceux de Lethal Company ; reste l'habillage colonial, plus lent à imiter. Or la satire n'a ni écran (74), ni Percepteur (76), ni UI propre (78). Le sermon relie fiction et horloge, mais « 20 s immobile » se lit comme une attente (03) et « pacifier » sonne conquête sans ironie (72).
  - Preuve : docs/narrative/ST_Islands.csv:10-66 (accroches du Bureau : 8 îles sur 20, 74) ; board.md:121.
  - Recommandation : budgéter la fiction comme une fonctionnalité : Percepteur à la Balance, « Lecture de l'Édit » (un bean lit, tout s'effondre autour), frégate ; 30 répliques avant tout fragment neuf.
  - Effort : M

- **[Majeur] Trois piliers, aucun n'a de scène montrable**
  - Constat : plat principal, la comédie de la cupidité ; la peur nocturne n'est qu'un assaisonnement (cf. 74, Q3). Naufrage jamais capturé (69) ; vente et Percepteur absents.
  - Preuve : 69 ; board.md:121.
  - Recommandation : ordre du trailer. (1) **Se pèse, « Un objet de trop »** : la barque redevient un choix (quoi laisser), pas une navette (ADR-005) ; butin fragile. (2) **Se noie, « La mer est l'horloge »** : submersion lisible, +2:00 par Édit. (3) **Se facture, « Au nom de la Couronne »** : Percepteur, registre, frégate.
  - Effort : M

## Manques
1. Percepteur et trois scènes Couronne à l'écran.
2. Piliers déclinés en modificateurs de contrat (poids, marée, édits) : marketing et rejouabilité (10).
3. Tranche verticale menu → vente, base d'un trailer honnête (01, 11).

## Quick wins
1. Écrire phrase, piliers et mots interdits en tête du cahier §1.
2. Test des 5 secondes : capture de la barque au ras de l'eau devant 5 joueurs du genre.
3. Confronter « Sinking Fund » et « Drowned Assets » (78) à la phrase.

## Questions qui challengent l'équipe
1. Flood With Friends propose radeau physique et île qui coule pour 5,19 € : qu'apporte la Couronne qu'un inconnu voit en 5 s ?
2. Sermon : signature ou dogme ? Si le playtest l'ennuie, retirez-vous la « mécanique centrale » (cahier §8) ?
3. Friendslop à 10 € en trois verbes, ou jeu de campagne en onze étapes : pas les deux.
