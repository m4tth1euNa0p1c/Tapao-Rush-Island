# 13 — Tutoriel et apprentissage
Domaine : B · Auditeur : designer d'apprentissage

## Verdict
L'apprentissage est à zéro : les 10 premières minutes sont un menu, un chargement sans astuce, un pont de galion sur une île tirée au hasard et l'invite « F » ; palan, barre, vente et quota ne sont pas codés, le sermon dort dans une branche. La base est saine (invites contextuelles). Risque commercial : une quinzaine de commandes et cinq mécaniques enchaînées, jamais jouées par un novice, mènent au remboursement Steam sous 2 h (supposé).

## Constats

- **[Bloquant commercial] Aucun tutoriel, aucune astuce, aucune tâche**
  - Constat : le cahier promet tutoriel contextuel, conseils de chargement, onglet Aide ; rien n'existe. `LoadingTips` n'est jamais affiché, 0 tâche sur 116, 0 recette d'apprentissage. Barque : une invite ; sermon : hors main, sans interface ; palan, vente, quota : non codés (T-112 à T-116).
  - Preuve : CAHIER §16 l.317/327, §18 l.362, `TapaoUISkin.h:133`, `STapaoScreens.cpp:314-362`.
  - Recommandation : « Contrat d'essai », manche 1 guidée (durées supposées, à valider avec cinq novices). 0:00 pont, briefing du Percepteur de 20 s passable, « F Descendre dans la barque ». 1:00 barque vide, balise sur la plage, toast « Z ramer, Q/D virer ». 2:30 trois objets, un choc affiche « −N $ », arrimer. 4:00 zone d'essai à un gardien, drapeau, sermon avec arc 0–20 s (voir 03), sans interruption au premier essai, « +2:00 » explique l'horloge. 7:00 échelle, palan (refus expliqué : « barque en mouvement »), « seul ce qui est à bord compte ». 8:30 barre puis Balance : brut, taxe, net. Moteur : un coach abonné aux événements §21, une astuce une fois par joueur (profil local, §16), réaffichable.
  - Effort : L (coach seul : M)

- **[Majeur] Premier contrat tiré au hasard**
  - Constat : l'île vient de l'horloge système ; `FRG_Raraka_01`, « île d'apprentissage », n'est jamais forcée. Départ possible à 18 h, au crépuscule (supposé). `URunRules` n'a aucun champ d'essai.
  - Preuve : `ExpeditionBootGameMode.cpp:56`, `islands.md:90`, `DayNightRules.h:51-55`, `RunRules.h:30`.
  - Recommandation : première partie sur FRG_01, départ à 9 h, quota d'essai non éliminatoire.
  - Effort : S

- **[Majeur] Aucun objectif à l'écran**
  - Constat : le HUD montre jauges, compteurs et barre d'objets, mais ni contrat, ni quota, ni zones restantes (`GetRemainingZoneCount` existe, non lu). La pause n'a pas d'Objectifs (§16). Le joueur ignore « que faire maintenant ».
  - Preuve : `STapaoHUD.cpp:105-168`, `hud-flood-timer.md:89-91`, `STapaoScreens.cpp:297-300`.
  - Recommandation : une ligne d'objectif sous l'horloge, pilotée par la phase (plage, zone, galion, vente).
  - Effort : M

- **[Majeur] Invites provisoires, commandes introuvables**
  - Constat : invite canvas avec « F » en dur (ni remappable, ni manette) ; `WBP_Prompt` et toast du kit non câblés ; horloge réduite au chiffre ; pas d'onglet Commandes ; 11 glyphes seulement (ni Z, S, D, G, X, Maj, clics).
  - Preuve : `IslanderHUD.cpp:192`, `FloodTimerWidget.cpp:169-174`, `SSettingsMenu.cpp:80-83`, `ui_manifest.json`.
  - Recommandation : `WBP_Prompt` lisant la touche liée, page Commandes, libellé et sablier sur l'horloge.
  - Effort : M

- **[Majeur] Barque : ni consigne de rame, ni jauge de charge**
  - Constat : assis, rien n'indique que ZQSD rament (coups de 1,2 s). Surcharge à 500 kg = naufrage, tout perdu, annoncé après coup par un message de debug (muet en Shipping, supposé) ; aucune jauge au HUD.
  - Preuve : `BoatActor.cpp:308,945`, `BoatRules.h:41`.
  - Recommandation : toast d'assise ; jauge sur la ligne de flottaison, alerte à 85%.
  - Effort : M

- **[Majeur] Vente et quota : trois nombres, deux vocabulaires**
  - Constat : quota jugé sur le brut, avant taxe (35%) et frais (50 par mort) ; HUD en « $ », cahier en « couronnes » ; « montez dans la barque » n'est plus la bonne consigne avec le galion (§24.2).
  - Preuve : CAHIER §24.2 étape 10, §24.4, `STapaoHUD.cpp:154`, `FloodTimerWidget.cpp:178`.
  - Recommandation : écrire le script du Percepteur avant l'interface (« Il vous manque 40 ») ; une seule monnaie.
  - Effort : S

- **[Majeur] Commandes piégées : vol, V et R**
  - Constat : deux appuis sur Espace en 0,3 s activent le vol, sans garde ni indication HUD : on survole mer et tutoriel. V est trompette et vue ; R recharge ou change la vue selon l'arme (conflit réel supposé).
  - Preuve : `IslanderCharacter.cpp:465-469`, `musket.md:24`, `TrumpetEmoteComponent.h:25`.
  - Recommandation : vol derrière une variable console ; une touche, un sens.
  - Effort : S

## Manques
1. Contrat d'essai non éliminatoire, coach contextuel, réglage Aide.
2. Ligne d'objectif, prompts dynamiques, page Commandes.
3. Fumée des villages (spec islands.md, non codée) ; boussole : voir 12.
4. Voix et textes du Percepteur (bible narrative absente), astuces de chargement.
5. Jalons chronométrés (première rame, premier sermon, première vente).

## Quick wins
1. Afficher `LoadingTips` : 8 astuces, panneau parchemin du kit.
2. Libellé et sablier sur l'horloge ; corriger `hud-flood-timer.md`.
3. Toast d'assise « Z ramer, Q/D virer, F se lever », touche lue dans l'input.
4. Brancher `OnValueChanged` (aucun abonné) : popup « −N $ ».

## Questions qui challengent l'équipe
1. Qui, hors vous, a joué dix minutes sans que vous parliez ?
2. La première manche est-elle éliminatoire ? Un novice qui rate le sermon perd-il sans comprendre ?
3. À quatre, qui apprend aux trois autres, la V1 n'ayant pas de VoIP (§8) ?
