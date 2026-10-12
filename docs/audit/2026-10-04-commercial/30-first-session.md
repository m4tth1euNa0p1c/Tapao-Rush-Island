# 30 — Première session d'un novice
Domaine : C · Auditeur : chercheur UX (playtests)

## Verdict
Aucun novice n'a joué ce parcours : pas de build packagé, clics des menus non testés (T-106), première manche sans issue heureuse ni explication de ses échecs. Le plaisir actif arrive après 1 à 5 min de transit muet, et le parcours simulé compte quatre ruptures avant la 12e minute (lancement, départ, mort, fin de manche). Risque commercial : remboursement Steam avant 2 h, la page promettant sermon, galion et vente que la première heure ne montre pas.

## Constats
T = minutes depuis le lancement, durées calculées (01, 06) ; abandon = estimation qualitative.

- **[Bloquant commercial] T+0 : le premier lancement est une inconnue**
  - Constat : aucun build packagé lancé ; T-106 n'a testé ni JOUER ni le chargement. Smart App Control a bloqué sur ce PC des binaires neufs non signés (erreur 4551) : chez un novice le menu peut ne jamais apparaître, à chaque patch (supposé). Qualité Épic, pare-feu, jauge factice : 23. Abandon : élevé.
  - Preuve : board.md:113 ; mémoire projet smart-app-control-dll-block.
  - Recommandation : build Development sur trois PC neufs (Smart App Control actif, GPU d'entrée de gamme) ; chiffrer la signature de code (98).
  - Effort : M

- **[Majeur] T+2 : quatre gestes non guidés, sous l'horloge, avant la première rame**
  - Constat : sur le pont, aucune consigne ; l'invite n'apparaît qu'à portée de l'échelle (4,2 m), puis F (échelle), F (« Prendre les rames »), Z maintenu, horloge déjà lancée (BeginPlay). Le code assoit pourtant l'équipage aux rames (T-093) avant de le poser sur le pont : supprimer ces gestes vaut mieux que les expliquer (13). Abandon : moyen.
  - Preuve : GalleonActor.cpp:290-308 ; BoatActor.cpp:305-309 ; GalleonSpawner.cpp:136-150 ; ExpeditionGameState.h:46.
  - Recommandation : manche d'essai en départ assis (`bPlayersStartOnDeck` et `bMoorBoatAtStart` à faux), échelle apprise au retour ; horloge au premier coup de rame.
  - Effort : S

- **[Bloquant commercial] T+3 et T+8 : mort ou fin de manche, le même écran sans bilan**
  - Constat : l'eau tue en 25 s de nage (09) ; à la fin de la submersion le code journalise seulement, l'horloge dit « montez dans la barque ! » et `NotifyVictory` n'a aucun appelant. On n'en sort que par le menu ou NAUFRAGÉ, qui affiche des compteurs de campagne, non ceux de la manche (maquette 07) ; « Recommencer » tire une autre île, à une autre heure (13) : ni réessai ni progrès visible. Abandon : très élevé.
  - Preuve : TapaoUISubsystem.cpp:493-495, 509 ; FloodSimulationActor.cpp:335-343 ; FloodTimerWidget.cpp:178 ; cahier §19, §24.2.
  - Recommandation : bilan de manche à toute fin (durée, mobs, `GetCargoValue()`, cause) ; « Réessayer cette île » (`GetLastPickedFragmentId()` et `TravelToSelectedIsland()` existent).
  - Effort : M

- **[Majeur] T+3 à T+10 : des plaisirs probables, isolés, tardifs et muets**
  - Constat (supposé, aucun testeur tiers) : seul le plaisir visuel est immédiat (galion sur le lagon) ; les actifs (rames, tangage, « TRANCHÉ ! », trompette, ragdoll, mer qui monte) suivent après 1 à 5 min de transit (06) ; pas de musique (11), moments comiques silencieux (18) ; le butin (1 à 6 $) se compare à un quota jamais affiché (01, 22).
  - Preuve : docs/features/galleon_pie.png ; boat-rowing.md:17-34 ; IslanderHUD.cpp:315 ; ExpeditionRules.h:37-41.
  - Recommandation : première rame < 60 s après JOUER, premier butin à bord < 6 min, première réussite < 15 min ; jalon sonore et « +N $ » à chaque étape.
  - Effort : M

- **[Majeur] T+30 min à 2 h : la curiosité s'épuise avant le remboursement**
  - Constat (supposé) : à 30 min, trois à cinq îles et une seule boucle ; à 1 h, rien de nouveau (ni boutique ni déblocage, 14) ; à 2 h, ni sermon (hors `main`, 03), ni vente (non codée), ni coéquipier (Rejoindre absent, 23).
  - Preuve : 03, 11, 14, 23 ; STapaoScreens.cpp:143-148.
  - Recommandation : trailer limité à ce qui se joue en 10 min ; démo « contrat d'essai » avant le build complet (84).
  - Effort : M

## Manques
1. Rejoindre / inviter (§16) : sans lui, la première session d'un groupe, cœur de cible, est impossible (11, 12, 23).
2. Une réussite observable en moins de 20 min (contrat d'essai non éliminatoire, 13) et un bilan de manche avec cause de mort (maquette 07 non branchée).
3. Balise « échelle, plage, barque » : `compass_bar` existe sans widget (22).
4. Lobby-école sans horloge (§24.2) pour apprendre sans compte à rebours.

## Quick wins
1. Deux cases de `DA_GalleonRules` : départ assis aux rames pour la manche d'essai.
2. NAUFRAGÉ : statistiques de la manche (maquette 07) au lieu des compteurs de campagne.
3. Bouton « Réessayer cette île ».
4. Protocole d'essai prêt avant le premier build : trois inconnus, 20 min, chronométrer première rame, premier butin, première mort.

## Questions qui challengent l'équipe
1. Que voit un inconnu pendant ses 90 premières secondes sur un PC neuf ? Qui l'a vérifié ?
2. À quelle minute un novice réussit-il quelque chose pour la première fois, un écran de réussite et non un compteur ? Aujourd'hui : jamais.
3. Quelle part de novices doit atteindre le premier butin à bord en 10 min pour ouvrir la page Steam, et avec quel instrument (aucune télémétrie, 95) ?
