# 03 — Pacification : drapeau et sermon de 20 s
Domaine : A · Auditeur : designer de mécaniques

## Verdict
Le pivot du jeu, gagner du temps en pacifiant, n'est pas jouable : son code dort sur la branche `dev/T-054` et rien n'appelle `AddZoneBonus`. Sur le papier, vingt secondes immobiles sans pression sont un temps mort, pas une décision. Risque commercial : le cœur du jeu n'a jamais été joué, son plaisir est inconnu.

## Constats
- **[Bloquant commercial] Mécanique absente du jeu jouable**
  - Constat : `main` n'a aucun code de sermon. Le socle serveur est sur `dev/T-054` : 9 fichiers neufs, 61 commits de retard, `review` depuis le 2026-10-03, specs jamais exécutées. Il manque entrée joueur, Blueprints, câblage à l'horloge.
  - Preuve : `git branch --contains d1a9035` ; journal.md:132,147 ; `AddZoneBonus` sans appelant (C++, .uasset, .umap).
  - Recommandation : fusionner, exécuter les specs, câbler zone → horloge.
  - Effort : M
- **[Majeur] Aucun enjeu pendant le rituel**
  - Constat : le drapeau exige les 3 gardiens morts, jamais remplacés : la règle « ennemi désigné dans le périmètre interrompt » ne peut plus jouer. Ni embuscade au village, ni vague. Restent les noix de coco et, dès 240 s, 4 guerriers de nuit : de jour 20 s d'attente sûre, de nuit un tirage au sort.
  - Preuve : PacificationTypes.cpp:84-94 (dev/T-054) ; characters-tiki.md:139,219,223 ; day-night-cycle.md:13.
  - Recommandation : planter le drapeau déclenche une contre-attaque annoncée (cor, 8 s de préavis, 3 tiki + 1 par joueur) ; le rituel se joue pendant l'assaut.
  - Effort : M
- **[Majeur] Remise à zéro totale, fragile en réseau**
  - Constat : tout dégât, poussée de coéquipier, écart de 10 cm ou touche relâchée annule jusqu'à 19,9 s. Rien ne bloque le mouvement côté joueur (le journal ne cite que port et attaque) : une touche effleurée ou une correction réseau à 100–200 ms (recette n°10) annule tout.
  - Preuve : cahier §6.2, recette n°18 ; PacificationRules.h:48-50 ; PacificationZone.cpp:278-309 ; journal.md:132.
  - Recommandation : seuil de dégât, poussées ignorées, Basculer par défaut, politique d'interruption en DataAsset.
  - Effort : S
- **[Majeur] Une zone coûte autant qu'elle rapporte ; sermon lançable trop tard**
  - Constat : sur `fragments.json` (20 fragments, à vol d'oiseau), la tournée des 3 villages fait 943–2 439 m, médiane 200 s au sprint ou 340 s au trot, hors rame, combat, sermon : du même ordre que les 360 s gagnées en solo. Le délai initial réel est 300 s, pas 120 s. Aucun garde-fou si le sermon démarre à moins de 20 s de l'échéance : `TooLate` reste au journal, la zone passe « Pacifiée ».
  - Preuve : calcul sur fragments.json et player-islander.md:16-17 (vitesses supposées valables pour le bean) ; ADR-001:45 ; ExpeditionGameState.cpp:162-165 ; PacificationZone.cpp:330-354.
  - Recommandation : refuser et signaler si `TimeBeforeSubmersion < SermonDuration` ; chronométrer une zone en PIE avant d'équilibrer.
  - Effort : S
- **[Majeur] Coop et solo : un acteur, trois spectateurs**
  - Constat : un seul officiant, durées non cumulables, les autres « protègent » sans menace. Solo : « fenêtre réalisable » non définie ; durée et menace ignorent le nombre de joueurs.
  - Preuve : cahier §6.2 l.112-114, §24.6.
  - Recommandation : table DataAsset joueurs → durée et contre-attaque ; chœur ci-dessous.
  - Effort : M
- **[Majeur] Lisibilité : ni zone, ni jauge, ni cause d'échec**
  - Constat : seul « Zone sécurisée : +2:00 » est codé ; ni panneau de zone (§19) ni carte. Portée (2,5 m) et périmètre (15 m) invisibles.
  - Preuve : grep « zone » dans Source/UI ; PacificationRules.h:36-42.
  - Recommandation : drapeau qui monte au mât avec la jauge, anneau au sol, chant audible, cause d'échec en texte bref.
  - Effort : M
- **[Majeur] La forme du cahier n'est pas prouvée fun : trois variantes**
  - Constat : « immobile, remise à zéro, un officiant » est un contrat, pas une preuve de plaisir. Variantes en DataAsset, cahier par défaut :
    - A, versets : 4 × 5 s, chacun donne +30 s et un point de reprise ; édits absurdes en String Table, matière à clips.
    - B, chœur : jusqu'à 4 officiants, durée ÷ nombre, chacun exposé.
    - C, occupation : la jauge monte tant qu'un équipier tient le cercle, pause sans reset ; l'officiant immobile la double.
  - Preuve : cahier §6.2 ; bonus = identifiant de zone × montant fixe (ExpeditionClock.cpp:22-25, 36-63).
  - Recommandation : A et contre-attaque d'abord ; B si le playtest à 4 le demande ; amender la recette n°18 par ADR.
  - Effort : M

## Manques
1. Entrée joueur (Maintenir/Basculer), verrou mouvement/arme/portage, animation, voix.
2. `BP_PacificationZone`, `BP_RoyalFlag`, `DA_PacificationRules` ; zone posée sur le `Center` de chaque `bZone`.
3. Contre-attaque, échelle par nombre de joueurs, télémétrie des interruptions.

## Quick wins
1. Fusionner `dev/T-054`, lancer ses specs.
2. Console `monke.PacifyZone <Id>` : horloge, HUD et eau réagissent en PIE.
3. Refuser un sermon qui ne peut finir avant l'échéance.
4. `DA_PacificationRules` avec préréglages 12/20/30 s.

## Questions qui challengent l'équipe
1. Quel moment de clip le sermon crée-t-il que 20 s d'attente ne créent pas ?
2. Une zone coûte à peu près ses 120 s : investissement ou corvée ? Qui l'a chronométrée ?
3. Amenderez-vous officiant unique et remise à zéro si un joueur s'ennuie pendant que trois veulent chanter ?
