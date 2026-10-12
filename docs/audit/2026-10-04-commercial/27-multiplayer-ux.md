# 27 — Inviter, rejoindre, lobby, prêt, votes, déconnexions
Domaine : C · Auditeur : designer UX multijoueur

## Verdict
Le coop est une intention écrite (§16, §24.5), pas un parcours : ni écran pour héberger, rejoindre, être prêt ou voter, ni `SessionSubsystem`. Un groupe d'amis ne peut pas lancer une partie dans un build ; en développement, le test manuel n'a jamais tourné sur deux machines. Risque n° 1 : le premier geste du coop, « inviter mes amis », n'existe pas.

## Constats
- **[Bloquant commercial] Lancer à plusieurs : impossible en build, 3 s en développement**
  - Constat : seules entrées client, `open <ip>` en console ou l'IP en ligne de commande ; le Shipping coupe les deux. `L_Boot`, sans pion ni écran, voyage 3 s après le dernier login (plafond 15 s) : aucune saisie humaine n'y tient (supposé). Correctif du « client perdu » compilé, « PIE à 2 depuis L_Boot NON retesté ».
  - Preuve : `ExpeditionBootGameMode.h:40-48,.cpp:71-99` ; `security.md:77` ; moteur `GameInstance.cpp:642` ; `fragments-tuamotu.md:386` ; board T-095.
  - Recommandation : lobby décidé par l'hôte. Budget proposé : « Héberger » → île ≤ 2 min, quatre amis déjà dans Steam.
  - Effort : M

- **[Majeur] Voyage dur sans barrière : chaque ami arrive à son heure**
  - Constat : `ServerTravel` non seamless : à chaque voyage chaque client se reconnecte seul et recharge sa carte (110 Mo sur l'île). §24.2 : horloge quand tous ont chargé (+10 s), retardataire retiré à 60 s : `PreparationTime` n'a aucun lecteur, aucun état de chargement.
  - Preuve : `ExpeditionIslandSubsystem.cpp:544` ; `DefaultEngine.ini:10` ; `RunRules.h:60-62` ; grep `SeamlessTravel` : 0.
  - Recommandation : barrière de chargement (quick win 3) ; « Chargement… » nominatif ; seamless en `-game`, repli dur assumé.
  - Effort : M

- **[Majeur] Arrivée tardive acceptée, 16 joueurs admis : l'inverse du cahier**
  - Constat : RUN03 exige le refus « partie en expédition » ; le code pose le retardataire dans la barque, sans `PreLogin`. `MaxPlayers` reste à 16 (moteur), même pour un « solo » (JOUER ouvre toujours `listen`).
  - Preuve : `BoatStartRules.h:71` ; `BoatStartSubsystem.cpp:294-307` ; grep `PreLogin` : 0 ; moteur `BaseGame.ini:68`.
  - Recommandation : `MaxPlayers=4` ; refus motivé hors lobby, dès que celui-ci existe.
  - Effort : S

- **[Majeur] Hôte ou client qui part : retour muet au menu, aucune reprise**
  - Constat : aucun traitement d'échec réseau : le moteur renvoie le client à `L_MainMenu` sans motif. §24.5 promet la reprise à « la dernière sauvegarde de zone sûre » : `SG_Run` n'existe pas (T-115).
  - Preuve : moteur `UnrealEngine.cpp:15527,15751-15755` ; `DefaultEngine.ini:14` ; grep `NetworkFailure` : 0.
  - Recommandation : messages de motif (quick win 2). Idée : `SG_Run` copié chez chaque client : n'importe quel ami reprend en hôte.
  - Effort : S (messages), M (reprise)

- **[Majeur] « Règles de déconnexion » : citées, jamais écrites**
  - Constat : §16 renvoie à des règles absentes du cahier : siège, frais de 50 c, quota (`PlayerCount` figé au lancement), retour dans la manche (interdit). Un ami qui plante à la manche 3 ignore s'il est facturé.
  - Preuve : cahier §16 l.325, §24.4 l.557 ; `RunTypes.h:117-119`.
  - Recommandation : état « absent » : ni mort ni frais pendant 120 s, retour sur galion ou barque ; quota inchangé.
  - Effort : S (spec), M (code)

- **[Majeur] Votes et « prêt » : bloqués à deux, par un passif, sans noms**
  - Constat : « majorité des vivants » (§6.5, §24.5) : à deux, un joueur à terre bloque le vote (1 sur 2, majorité stricte, supposé) ; à quatre, un passif pèse « non » ; comment vote un joueur à terre ? Signature : « l'hôte signe » (§24.2) ou « ouvre un vote » (ADR-005:69) : une défaite clôt la partie. « Prêt » bloque sur un AFK : ni délai, ni expulsion. T-112 précède tout `PlayerState` (T-028, L6).
  - Preuve : cahier §6.5 l.148, §24.2 l.505-531, §24.5 l.565-567 ; board T-028, T-112.
  - Recommandation : bulletins nominatifs visibles, silence = abstention, majorité des votants avec quorum ; défaite à l'unanimité des présents à bord ; décompte nominatif de 15 s si non-prêts ; expulsion à l'hôte.
  - Effort : M

## Manques
1. `USessionSubsystem` (Héberger, Rejoindre ; 6 erreurs du §16) : IP d'abord, Steam ensuite (voir 12).
2. `PlayerState` répliqué, RPC de contrôleur (prêt, vote) : les 21 RPC serveur sont sur le pion.
3. `L_Galleon` léger : liste, chargement, verrou, expulsion.
4. Entrée par invitation, sans splash.

## Quick wins
1. `MaxPlayers=4` ; `ProjectVersion` par build (aucune configurée : tous les builds se croient compatibles, supposé ; `NetworkVersion.cpp:90`, §16).
2. `OnNetworkFailure` et `OnTravelFailure` vers `AskConfirm` TAPAO.
3. `L_Boot` : liste `PlayerArray`, bouton « Lancer » de l'hôte (`MakeTapaoTextButton`) et `?Crew=n` ; l'horloge attend `n` pions ou 60 s.
4. Protocole `-game` sur un PC (client avant puis après le voyage, hôte qui quitte, client qui plante), consigné en recette.

## Questions qui challengent l'équipe
1. Quel ami a déjà rejoint votre partie sans aide ? Premier test à deux machines : quand ?
2. Un ami en retard ou qui plante attend 15 minutes : l'acceptez-vous, ou rejoint-il le pont du galion (§24.3) ?
3. L'hôte décide du lancement, de la signature et de la défaite : capitaine ou dictateur ?
