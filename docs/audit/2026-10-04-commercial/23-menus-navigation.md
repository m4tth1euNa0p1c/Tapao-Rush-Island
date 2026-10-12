# 23 — Menus TAPAO, lobby à bord et paramètres
Domaine : C · Auditeur : designer UI (menus PC)

## Verdict
Le squelette TAPAO ne couvre que 5 entrées sur 7 du §16, 6 rubriques sur 19 du §18 et 1 recette UI sur 12 ; le lobby n'existe pas et T-106 n'a vu aucun clic. Risque commercial : impossible de rejoindre un ami ou de remapper ses touches, et la campagne unique se perd en deux clics.

## Constats

- **[Bloquant commercial] Navigation du §16 aux deux tiers**
  - Constat : pas de Charger ni de Rejoindre ; formulaire de création 0/7 champs ; pause 4 boutons sur 6 (ni Objectifs ni Équipage). Recette §23 : UI07 satisfaite ; UI01, UI09 partielles ; UI04, 05, 06, 08, 10, 11 non couvertes ; UI02, 03, 12 non vérifiées.
  - Preuve : `STapaoScreens.cpp:143-148, 297-300` ; cahier §16, §23.
  - Recommandation : Rejoindre par IP, Solo/Héberger, Charger à 3 emplacements, puis lobby.
  - Effort : M

- **[Majeur] Campagne unique détruite en deux clics**
  - Constat : NOUVELLE PARTIE demande « Écraser ? » sans nom ni bilan, puis `DeleteGameInSlot` sans copie. Si la lecture échoue, une campagne vierge est écrite par-dessus sans message joueur ; `Version` n'est jamais lue ; JOUER double CONTINUER.
  - Preuve : `TapaoUISubsystem.cpp:213-217, 574-592, 649-669`.
  - Recommandation : copie `.bak`, confirmation nominative, modale « sauvegarde illisible », JOUER masqué si sauvegarde.
  - Effort : S

- **[Majeur] Clavier : aucun focus initial ni visible**
  - Constat : chaque écran focalise sa racine, pas un bouton (Entrée n'agit qu'après Tab ou flèche ; vu dans le code, non joué). `SButton` ne dessine aucun état focus : le « focus visible » du §16 n'est pas tenu (maquette : cadre doré sur le 1er bouton).
  - Preuve : `TapaoUISubsystem.cpp:259-264`, `TapaoStyle.cpp:73-86`, moteur `SButton.cpp:221-239`.
  - Recommandation : focus du 1er bouton actif (ANNULER dans les confirmations), cadre doré (sprite `Hover`) autour du bouton focalisé.
  - Effort : S

- **[Majeur] Réglages hors charte et sans filet**
  - Constat : 4 onglets en style moteur, polices 11 à 15, alors que le kit livre l'écran Options. Échap jette les modifications sans modale (§18) ; résolution appliquée sans retour à 15 s (UI06). Absents : remappage (ZQSD en AZERTY), luminosité, sous-titres, daltonisme, échelle, maintenir/basculer (UI11).
  - Preuve : `SSettingsMenu.cpp:30-33, 80-83, 137`, `SettingsSubsystem.cpp:43, 220-237, 339-346`, `05_options.png`.
  - Recommandation : refaire dans le kit (AUDIO, VIDÉO, COMMANDES d'abord) ; `STapaoConfirm` à 3 choix (Appliquer/Abandonner/Rester) ; `RevertVideoMode` à 15 s.
  - Effort : L

- **[Majeur] Qualité au premier lancement : Épic probable, pas Moyen**
  - Constat : `Load()` écrase le défaut Moyen par le niveau du moteur, Épic au départ ; ni `DefaultGameUserSettings.ini` ni benchmark : première partie probablement au maximum (déduit, non mesuré).
  - Preuve : `SettingsSubsystem.cpp:137, 145-148`, moteur `Scalability.cpp:1038-1043`.
  - Recommandation : défaut Moyen, `RunHardwareBenchmark` au premier lancement.
  - Effort : S

- **[Majeur] Pause et confirmations fausses en coop**
  - Constat : en multi, la bannière « PAUSE » s'affiche alors que le monde tourne. « La partie est sauvegardée » s'affiche aussi chez le client, qui ne sauvegarde pas ; l'hôte n'est pas prévenu que la session se ferme ni de l'abandon d'expédition (§16, §17). QUITTER est confirmé depuis le menu principal : friction.
  - Preuve : `TapaoUISubsystem.cpp:425-431, 602-622`, `STapaoScreens.cpp:276-310`.
  - Recommandation : ligne « La partie continue » dès 2 joueurs, textes selon `NetMode` et phase, confirmation retirée du menu.
  - Effort : S

- **[Majeur] Ni pile d'écrans ni modale d'erreur avant le lobby**
  - Constat : 8 écrans à z-ordre manuel (100 à 1 100), focus codé en dur, deux sous-systèmes qui changent le mode d'entrée ; T-114 et le spectateur en ajoutent cinq : curseur ou focus coincés probables. Aucun échec affiché, aucune garde anti double-clic (UI03, UI04).
  - Preuve : `TapaoUISubsystem.h:142-148`, `SettingsSubsystem.cpp:351-359`, cahier §21.
  - Recommandation : `UIFlowSubsystem` minimal et `STapaoError` avant T-114.
  - Effort : M

- **[Mineur] Détails du menu et du chargement**
  - Constat : CONTINUER grisé sans explication ; fond statique de case, sans galion (§16) ; version « v0.1 » figée dans le DA ; jauge de chargement factice (`1-e^(-t/4)`) ; pas de pause solo à la perte de focus : l'horloge court pendant un Alt+Tab (supposé).
  - Preuve : `STapaoScreens.cpp:374-380`, `TapaoUISkin.h:75-77`, grep `ActivationState`, `LostFocus` : 0.
  - Recommandation : voir Quick wins.
  - Effort : S

## Manques
1. Rejoindre (IP, LAN) avec états et erreurs du §16 ; Solo sans `listen` (toute partie ouvre un port, alerte pare-feu probable, supposé).
2. Lobby : 4 places (nom, personnage, Prêt, chargement), refus listant les non-prêts, délai 60 s.
3. Charger, Commandes, luminosité, accessibilité (voir 25), pause avec Objectifs et Équipage, indicateur de sauvegarde.

## Quick wins
- Focus initial et focus visible partout.
- `.bak` et confirmation nominative de NOUVELLE PARTIE.
- « La partie continue » et confirmations selon `NetMode`.
- Qualité Moyen par défaut, benchmark au premier lancement.
- Pause solo à la perte de focus ; verrou anti double-clic dans `TravelToPlay`.

## Questions qui challengent l'équipe
1. Qui fait foi : la maquette du 2e PC (JOUER, 3 onglets) ou le cahier §16 ?
2. T-106 est en review sans clic testé : qui joue les menus au clavier seul avant tout nouvel écran ?
3. Un joueur solo doit-il ouvrir un port réseau ? Sinon, quand sépare-t-on Solo et Héberger ?
