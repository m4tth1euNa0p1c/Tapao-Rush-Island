# Galion de l'équipage (T-110)

Cahier §24 (parcours en manches), décision : `docs/adr/ADR-005-partie-en-manches.md`. Maquette : 2e PC, `maquettes/galion-anglais` (copie des exports dans `art/export/Galleon/`).

## Intention
Le galion est le point de départ et de retour de chaque île : l'équipage apparaît sur le pont, la barque est amarrée au flanc, on descend par l'échelle de coupée, on revient déposer le butin à bord.

## V1 livrée (lot L5, base de T-111 à T-113)
- **Acteur `AGalleonActor`** (`Source/monke_island/Galleon/`), répliqué, cinématique :
  - suit le niveau de la mer (montée des eaux comprise) ;
  - balancement cosmétique identique sur toutes les machines (horloge serveur) : roulis, tangage, pilonnement ;
  - ponts, escaliers, pavois et cabine praticables : collision complexe de `SM_Galleon` ;
  - voiles, gréement, pavillon, barre et gouvernail en pièces séparées, sans collision.
- **Échelle de coupée** (touche d'interaction F, `IIslanderInteractable`) :
  - depuis l'eau ou la barque, près du pied de l'échelle : « Monter à bord » ;
  - depuis le pont, près du haut de l'échelle : « Descendre dans la barque » si elle est amarrée, sinon « Descendre à l'eau » ;
  - l'objet porté suit le joueur.
- **Contrat pour la suite** (barre, palan, départ, vente) :
  - `IsActorAboard(Actor)` : volume des joueurs à bord ;
  - `GetCargoAboard(Out)` et `GetCargoValue()` : objets de valeur posés à bord (`UValuableComponent` dans le volume de cargaison) ;
  - `GetDockTransform()`, `IsBoatMoored(Boat)`, `FindMooredBoat()`, `MoorBoat(Boat)` : barque au flanc ;
  - `PlaceOnDeck(Character, Index)` : poser un joueur sur le pont ;
  - sockets de la coque : `helm`, `cargo_hatch`, `boarding_ladder`, `barque_dock`, `barque_davit`, `bean_spawn`, `captain_cabin`, `anchor_*`, `bell`, `flag`, `rudder_hinge`.
- **Début de partie `AGalleonSpawner`** (serveur, posé dans `Lvl_Raraka`) : 2 s après le départ, quand la barque attend à l'arrivée du fragment, il
  1. cherche autour de l'arrivée, du plus près au plus loin, la première eau assez profonde sous toute la coque ;
  2. ancre le galion, flanc de l'échelle tourné vers l'arrivée ;
  3. pose l'équipage sur le pont (les joueurs quittent leur siège de barque) ;
  4. amarre la barque au dock.
- **HUD** : le compteur de pièces du HUD TAPAO affiche la valeur de la cargaison à bord du galion.

## Valeurs (`DA_GalleonRules`, aucune en dur)
| Valeur | Défaut |
|---|---|
| Balancement : roulis / tangage / pilonnement | 1,0° / 0,4° / 6 cm, périodes 7 / 5,3 / 6,1 s |
| Profondeur minimale sous la coque | 4 m |
| Recherche du mouillage | de 25 m à 300 m de l'arrivée, pas de 5 m, 24 directions |
| Délai avant l'ancrage | 2 s |
| Portée de l'échelle | 4,2 m |
| Barque amarrée | à moins de 4,5 m du dock et à moins de 40 cm/s |
| Joueurs sur le pont au départ / barque amarrée au départ | oui / oui |

## Hors V1 (tâches suivantes)
- Barre, voiles, vote de départ, palan vers la cale (T-112) ; `L_Galleon` et état de partie (T-113) ; vente et registre (T-114).
- Contrainte douce d'amarrage : la barque est posée au dock au départ, elle n'est pas retenue ensuite.
- Cale intérieure : la maquette ne la modélise pas encore (seule la cabine du capitaine est meublée) ; le pavillon n'a pas sa croix de Saint-Georges (matériau uni).
- Flottaison simulée (42 t, 10 pontons du rapport de maquette) : non retenue en V1, voir ADR-005.

## Critères d'acceptation
- CA-G1 : au lancement d'une île, le journal écrit « anchored … m off BP_Barque », les joueurs sont sur le pont et la barque est au flanc.
- CA-G2 : on marche sur le pont principal, on monte au gaillard et à la dunette par les escaliers, on entre dans la cabine.
- CA-G3 : F au pied de l'échelle monte à bord ; F en haut redescend dans la barque.
- CA-G4 : un objet de valeur posé sur le pont augmente le compteur de pièces du HUD.
- CA-G5 : hôte et client voient le galion au même endroit, avec le même balancement.

## État de preuve
Compilé (cible jeu et éditeur). CA-G1 à CA-G5 : à vérifier en PIE, puis en hôte + client.
