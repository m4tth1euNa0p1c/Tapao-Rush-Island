# 91 — Scope minimal vendable (MVP)
Domaine : J · Auditeur : producteur exécutif

## Verdict
L'effort est allé au monde plutôt qu'à la boucle : 41 tâches de terrain, fragments, villages et météo contre 8 pour fermer la manche (T-109 à T-116, 5 à faire), aucune pour Steam. Le MVP vendable : quatre amis invités par Steam, un galion, 3 îles, 5 manches ; le reste est un réservoir de mises à jour, ou à couper. Risque : ajouter du décor avant d'avoir joué une manche sur deux machines (aucune preuve « testé multi-machines »).

## Constats

- **[Bloquant commercial] Le MVP est une manche complète à quatre, rejoignable par Steam**
  - Constat : la « V1 » du cahier (L5–L6) est LAN/direct-IP, donc invendable ; le build T-120 lance une expédition, pas une partie. Définition : lobby, barque, 3 zones, palan, barre, vente, quota, de 1 à 4 joueurs. Sortie : RUN01–RUN13 verts à deux machines, et 7 inconnus sur 10 finissent la manche 1 (seuil supposé).
  - Preuve : cahier §11:242 ; docs/releases/v1-rec-investisseurs.md:53.
  - Recommandation : ADR « MVP v0.1 » ; scinder L7 : sessions Steam (L7a) avant L6, voix (L7b) après.
  - Effort : L

- **[Majeur] Classement du backlog**
  - Constat :
    - **Indispensable** : boucle T-111 à T-116, fusion T-054/055/057 ; Steam L7a, lobby, PlayerState ; spectateur, ping, boussole ; 3 fragments `L_Frag_*` (un par type, 63) et leurs villages-zones ; mobs existants, requin (spawner) ; couleur et nom par joueur ; Percepteur et six sermons ; 4 modificateurs de manche ; boutique de 5 articles tirée de l'existant ; remap, anglais, contrat guidé ; paliers de qualité ; nom, capsule, licences et crédits.
    - **Après l'EA** (existant gelé) : 17 fragments (3 par mise à jour), voix de proximité, skins modulaires, contrats à 3 offres, hameaux, camps, vent d'abri, cadavres découpables (guerriers seuls), monstres et outils neufs, endgame, manette et Deck, succès, T-027 (wobble).
    - **À couper** : E1, serveurs dédiés, 8 joueurs, consoles ; marae pillable, perle de 1 500, totems ; villageois létaux ; hache ; §6.4 ; `Variant_*` ; `Lvl_Raraka` du build ; 17 rôles et 23 skills mis en sommeil.
  - Preuve : cahier §11, §14, §20 ; rapports 11, 12, 17, 59, 60, 72, 73, 80.
  - Recommandation : couper avant la page Steam ce qui coûtera plus cher après (marae, villageois, `Lvl_Raraka`) ; toute nouvelle demande en retire une autre (l'audit seul dépose 69 bloquants et 407 majeurs).
  - Effort : S

- **[Majeur] Sur-ingénierie n°1 : le monde a dépassé la boucle**
  - Constat : ADR-004 renvoyait les fragments 2 à N à L8, après le noyau ; le pipeline en a produit 20, avec 104 villages et hameaux. Le décor pèse 876 Mo des 1,1 Go de contenu ; `Creatures/`, premier dossier de code, compte 7 263 lignes, `Run/` 745 ; un tiers des lignes de scripts sert l'atoll réel (carto, fragments, look-dev).
  - Preuve : ADR-004:29 ; CLAUDE.md:6 ; board T-083, T-102, décompte par fonction ; `du`, `wc -l` (vu).
  - Recommandation : geler : 3 fragments joués, 17 en données, hameaux non chargés ; aucune tâche monde avant RUN10 vert.
  - Effort : S

- **[Majeur] Sur-ingénierie n°2 : 29 rôles, 35 skills, 4 environnements, un seul testeur**
  - Constat : 55 tâches sur 126 portent « lead » ; 9 rôles n'ont aucune ligne (intégrateur, relecteur, sécurité) : gates sur le papier. 83 tâches attendent en `review` ; Codex (6 profils) s'ajoute.
  - Preuve : FACTORY.md:6-19, 88-90 ; board.md ; rapport 39.
  - Recommandation : ~12 rôles et ~12 skills actifs ; plafond de 15 `review`.
  - Effort : S

- **[Bloquant commercial] Des conditions de vente n'ont aucune tâche**
  - Constat : 0 occurrence au board de Steam, L7, spectateur, tutoriel, licences, ping, boussole, crédits ou crash.
  - Preuve : grep board.md ; rapports 13, 28, 56, 70.
  - Recommandation : 8 tâches : sessions Steam, spectateur, ping et boussole, contrat guidé, licences et crédits, nom et capsule, paliers de qualité, recette à deux machines.
  - Effort : M

## Manques
Par priorité : boucle fermée (11) ; sessions Steam (56) ; spectateur (28) ; Percepteur et sermons (20, 71) ; paliers de qualité (45) ; anglais (79) ; nom libre (78).

## Quick wins
1. ADR « MVP v0.1 » avec les trois listes (2 h).
2. Chantier, 18 `a-faire` → 9 : reporter T-013, T-014, T-017, T-027 ; fermer T-015, T-088 (dans T-113), T-025 et T-026 (doublons supposés de T-030, T-078) ; réduire T-028 à couleur et nom ; créer les 8 tâches (2 h).
3. Spike Steam d'un jour : AppID 480, `open steam.<id>` entre deux PC, voix LAN (56).
4. `NoGrab` sur le marae, villageois non létaux (72, 80).
5. `Variant_*` (3 979 lignes) et Mannequins hors du cook (70).

## Questions qui challengent l'équipe
1. Vous avez classé L7 « secondaire » (§23) : qui joue entre amis avec une IP et un port ouvert, et que retirez-vous pour passer Steam avant L6 ?
2. Si trois éléments seulement tiennent sur la capsule (sermon, barque, ragdoll de bean, découpe, atoll 1:1, vent), lesquels ?
3. 83 tâches attendent votre jeu : combien d'heures de test par semaine, et gelez-vous le monde jusqu'à RUN10 ?
