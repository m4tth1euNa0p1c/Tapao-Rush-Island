# 11 — Écart entre le cahier v1.3 et l'existant
Domaine : B · Auditeur : producteur exhaustif

## Verdict
Le jeu est un bac à sable riche, pas encore une partie : rien ne la termine, la pacification est hors `main`, vente et quota n'existent que comme calcul testé. Risque n°1 : aucune manche complète à filmer ni à jouer à deux.

## Constats

Lots (F/R/A = tâches fait / review / reste) d'après `board.md` :

| Lot | F/R/A | Statut réel (vu) |
|---|---|---|
| L0 | 1/0/0 | Compile, 296 tests ; `L_GalleonHub`, `L_Island_Prototype`, README absents |
| L1 | 0/3/0 | Solo testé éditeur ; jamais sur deux machines |
| L2 | 3/13/0 | Barque compilée ; arrimage hors `main` ; §6.4 non fait |
| L3 | 2/22/8 | PvE, horloge, submersion compilés ; pacification, extraction, palan absents |
| L4 | 0/3/5 | Calculs purs testés (RUN14) ; ni lieu, ni vente, ni sauvegarde de partie |
| L5 | 1/20/0 | Endurance, nuit, torche, HUD compilés ; jamais à 4 |
| L6 | 1/5/4 | Aucun build cuit ; ragdoll actif, skins absents |
| L7, E1, L8+ | – | L7, E1 non commencés ; monde L8 en avance |

- **[Bloquant commercial] Aucune manche ne se termine**
  - Constat : seule l'inondation écoute la fin de submersion ; `NotifyVictory` n'est jamais appelée ; `FRunCalculator` et `FEconomyCalculator` ne servent qu'aux specs : on ne finit que par la mort ou le menu.
  - Preuve : `ExpeditionGameState.cpp:266`, `TapaoUISubsystem.cpp:509` ; T-112–T-116 `a-faire`.
  - Recommandation : T-112 → T-115 avant tout contenu monde.
  - Effort : L
- **[Bloquant commercial] La mécanique centrale n'est pas dans `main`**
  - Constat : zones et sermon (T-054), arrimage (T-055), sauvegarde (T-057) dorment sur trois branches non fusionnées (6 910 lignes, specs jamais exécutées) ; `AddZoneBonus` sans appelant.
  - Preuve : `git merge-base --is-ancestor` ×3 ; grep `AddZoneBonus`.
  - Recommandation : fusionner, compiler, tester, intégrer villages → zones.
  - Effort : M
- **[Bloquant commercial] Le coop n'est joignable que par console**
  - Constat : JOUER ouvre `?listen`, mais ni Rejoindre, ni lobby, ni PlayerState, ni gestion d'échec réseau ; voyages non seamless ; jamais testé sur deux machines.
  - Preuve : `TapaoUISkin.h:121`, `DefaultEngine.ini:10`, grep NetworkFailure : 0.
  - Recommandation : Rejoindre par IP, PlayerState, lobby, test hôte + client en `-game`.
  - Effort : M
- **[Majeur] Aucune recette consignée, travail hors dépôt**
  - Constat : pas de matrice ; seules les recettes n°13–16 et RUN14 ont des specs (logique pure) ; `Run/`, `Galleon/`, `UI/Tapao` hors git, sans remote ; 73 tâches sur 117 en `review`.
  - Preuve : `git status` : 271 modifiés, 159 non suivis.
  - Recommandation : committer (accord requis), créer `docs/production/recette.md`.
  - Effort : S
- **[Majeur] L6 : aucun build jouable, produit non identifiable**
  - Constat : ni cuisson ni packaging ; trois noms (`First Person Template`, « TAPAO : Rush Island », « Crownfall Expeditions ») ; `LoadingTips` jamais affichées.
  - Preuve : `DefaultGame.ini:3`, `Saved/Cooked` absent.
  - Recommandation : premier package Development, nom unique, README.
  - Effort : M
- **[Majeur] Écarts au cahier jamais tranchés**
  - Constat : E-01, E-03 à E-07, ADR-002 et ADR-003 « proposé » ; §6.4 et n°19 caducs ; F contre E, 500 kg contre 150 kg.
  - Preuve : ADR-001 §3, ADR-003 B-01.
  - Recommandation : arbitrage en séance, ADR acceptés, cahier v1.4 ; geler le contenu monde.
  - Effort : S

## Manques
P0, chemin RUN01 → RUN10 (1, 3, 7 en parallèle) :
1. Pacification câblée : fusion T-054, zones sur les 3 villages, panneau, anim et voix du sermon.
2. Barre, vote, abandon, départ automatique, palan, cale (T-112) ; modèles absents (cale, palan, balance).
3. `ItemId` et catégorie sur `UValuableComponent` (absents) ; bois, nourriture, coffre pour les commandes.
4. `L_Galleon`, `ARunGameMode`, `URunSubsystem`, `L_Transit`, manifeste de cale, fin de `L_Boot` (T-113, T-088).
5. Zone sûre : Percepteur, cotre, registre, signature, défaite, frégate, quatre écrans (T-114).
6. Sauvegarde de partie : fusion T-057, `SG_Run`, Continuer, `ExpeditionInProgress` (T-115).
7. Lobby à bord (nom, prêt, personnage, délai 60 s), Rejoindre, Nouvelle partie, Charger.

P1, V1 : boutique (hache, lanterne, cale) et arbres ; spectateur ; arrimage ou abandon par ADR ; tutoriel ; réglages (4 onglets pour 19 lignes du §18, sans remappage ni accessibilité) ; pause sans Objectifs ; HUD sans quota ni zones ; carte et pings ; ragdoll actif, skins ; audio (100 sons, ni musique, vent, pluie ni cloche) ; cartes `L_Frag_*` ; mesure de performance.

P2 : L7, E1, L8.

## Quick wins
- Fusionner T-054, T-055, T-057 en branche d'intégration, compiler, lancer leurs specs (supposé ≤ 1 j).
- Champ « Rejoindre par IP » dans le menu TAPAO, test PIE à deux.
- Écrire `recette.md` (§12, UI01–12, RUN01–14) avec le statut réel.
- Afficher `LoadingTips`, réécrire l'astuce d'arrimage (v1.3).
- Fixer le nom de travail (ProjectName, menu).

## Questions qui challengent l'équipe
1. Pourquoi 21 tâches L5 et 15 tâches de fragments avant les six de la boucle pacifier → palan → départ → vente (T-111–T-116) ?
2. Le cahier dit E, 120 s, 150 kg ; le jeu fait F, 300 s, 500 kg : lequel fait foi, et qui signe l'ADR ?
3. Seul testeur humain, qui joue le coop à deux machines cette semaine ?
