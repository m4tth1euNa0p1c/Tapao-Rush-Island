# Galion de l'équipage (T-110)

Cahier §24 (parcours en manches), décision : `docs/adr/ADR-005-partie-en-manches.md`. Maquette : 2e PC, `maquettes/galion-anglais` (copie des exports dans `art/export/Galleon/`).

## Intention
Le galion est le point de départ et de retour de chaque île : l'équipage apparaît sur le pont, la barque est amarrée au flanc, on descend par l'échelle de coupée, on revient déposer le butin à bord.

## V1 livrée (lot L5, base de T-111 à T-113)
- **Acteur `AGalleonActor`** (`Source/monke_island/Galleon/`), répliqué, cinématique :
  - suit l'eau là où il est ancré : la mer, ou l'inondation locale du fragment quand elle la dépasse (montée lissée, jusqu'à la fin de la submersion) ;
  - zone sans vent autour du navire (sphère `WindCalmZone`, lue par `AWindStormActor`) : ni les joueurs, ni la barque, ni les objets n'y sont poussés ;
  - lampe dans la cabine du capitaine ;
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
  1. prend le mouillage de rôle `Galleon` de l'île jouée (`DA_Island_*`, calculé depuis `fragments.json`) s'il existe dans le repère de la carte ; sinon cherche autour de l'arrivée, du plus près au plus loin, la première eau assez profonde sous toute la coque ;
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
| Zone sans vent | 40 m autour du navire |
| Suivi de l'eau | 60 cm/s au plus |
| Lampe de cabine | 5 cd, rayon 6,5 m, 2 300 K |
| Joueurs sur le pont au départ / barque amarrée au départ | oui / oui |

## Hors V1 (tâches suivantes)
- Voiles, palan vers la cale (T-112) ; `L_Galleon` et état de partie (T-113) ; vente et registre (T-114). La barre et le vote de départ sont faits (T-286, plus bas).
- Contrainte douce d'amarrage : la barque est posée au dock au départ, elle n'est pas retenue ensuite.
- Cale intérieure : la maquette ne la modélise pas encore (seule la cabine du capitaine est meublée) ; le pavillon n'a pas sa croix de Saint-Georges (matériau uni).
- CA-G6 à vérifier : pendant la submersion, le pont reste au-dessus de l'eau ; près du navire, un coup de vent ne pousse plus personne.
- Flottaison simulée (42 t, 10 pontons du rapport de maquette) : non retenue en V1, voir ADR-005.

## Critères d'acceptation
- CA-G1 : au lancement d'une île, le journal écrit « anchored … m off BP_Barque », les joueurs sont sur le pont et la barque est au flanc.
- CA-G2 : on marche sur le pont principal, on monte au gaillard et à la dunette par les escaliers, on entre dans la cabine.
- CA-G3 : F au pied de l'échelle monte à bord ; F en haut redescend dans la barque.
- CA-G4 : un objet de valeur posé sur le pont augmente le compteur de pièces du HUD.
- CA-G5 : hôte et client voient le galion au même endroit, avec le même balancement.

## État de preuve
Compilé (cible jeu et éditeur). CA-G1 à CA-G5 : à vérifier en PIE, puis en hôte + client.

## Évolutions du 2026-10-07 (soir, build v1.3)
- **Mouillage plus près des côtes** (« faire apparaître le galion moins loin des côtes »). Les mouillages des données d'île étaient à 325-330 m de la terre. Le spawner (`AGalleonSpawner::ApproachShore`) cherche la terre la plus proche jusqu'à 1,5 km (`ShoreSearchCm`). Le galion avance ensuite vers elle tant que toute son emprise, élargie de 3 m (`ShoreMarginCm`), garde 1,6 m d'eau (`ApproachMinDepthCm` ; la quille descend à environ 1,9 m). La proue est mise parallèle à la côte, le flanc de l'échelle tourné vers l'île. Testé sur FRG_Raraka_01 : « the galleon comes 218 m nearer the island (land was 330 m away, now 112 m) ». La barque est amarrée à son flanc.
- **Départ proue en avant** (« le bateau part sur le côté alors qu'il est orienté de l'autre sens »). La coque est longue selon son Y, proue en +Y (`BowYawDeg` = 90). Le départ (`UDayTransitionSubsystem`) navigue maintenant dans cet axe, et la caméra de départ est placée dans ce repère (X devant).
- **Gouvernail : « Terminer la journée »**. *Remplacé le 2026-10-11 par le départ à la barre ci-dessous (T-286) : un appui ne termine plus la journée.* À bord, à moins de 2,6 m du socket `helm` (`HelmReach`), l'interaction lançait `AExpeditionGameState::EndDayEarly`, départ 10 s plus tard.

## Départ à la barre (T-286, cahier §24.2 étape 7, §6.5 ; revue réseau T-268 R-01)
- **Lever l'ancre** : un joueur vivant à bord, à la barre, mains libres, **maintient** la touche de la barre (`HelmKeys`, F) `HelmHoldSeconds` (2 s). À la barre, cette touche est reprise à l'interaction par un mapping de priorité supérieure (`UGalleonCrewComponent`, sur chaque PlayerController) ; une touche déjà enfoncée en arrivant à la barre ne compte pas.
- **Tous les vivants à bord** (solo compris) : préavis court `DepartureAllAboardNoticeSeconds` (5 s).
- **Sinon, vote de l'équipage** : la voix du barreur compte ; chaque vivant, à bord ou à terre, vote avec la touche proposée par le HUD (`VoteKeys`, O). Il faut strictement plus de `DepartureMajorityFraction` (0,5) des vivants et au moins un vivant à bord. Puis préavis long `DepartureVoteNoticeSeconds` (10 s) : cloche, corne de brume, « Départ dans N », « Les joueurs restés à terre seront abandonnés », tic des dernières secondes.
- **Annulation** : à la barre, la touche annule le vote ou le préavis en cours. Vote caduc si plus aucun vivant n'est à bord, expiré sans majorité après `DepartureVoteSeconds` (30 s) ; ensuite pas de nouveau vote pendant `DepartureVoteCooldownSeconds` (10 s).
- **Fin du préavis** : `AExpeditionGameState::SailFromHelm` résout la journée comme la fin du déluge (à bord : rapportés avec la cargaison ; à terre : abandonnés, morts). La fin de la submersion fait toujours partir le galion seul.
- **Réseau** : état répliqué dans le game state (`UGalleonDepartureComponent` : étape, votants, vivants, à bord, échéances en temps serveur, cause de la dernière fin ; tenue de la barre). Requêtes : RPC fiables du composant d'équipage, limitées (`DepartureRequestMinInterval`, un relâchement n'est jamais refusé), revérifiées par le serveur (vivant, à la barre, phase). Les morts, spectateurs et joueurs encore en chargement ne votent pas.
- **Sons** (2D, toute l'équipe) : `DepartureBellSound` (`SW_Exp_Bell`), `DepartureHornSound` (pas de corne de brume dans le projet : `S_HudTimer_Evacuation` en attendant), `DepartureTickSound` (`SW_UI_Click`, le tic du compte à rebours).
- **Règles pures** : `GalleonDeparture` (`Galleon/GalleonDeparture.h`), spec `monke_island.Galleon.Departure`. `DepartureMajorityFraction` à 0 = le barreur décide seul (choix ADR-008 D12 si le propriétaire le retient).
- **Console** (hors Shipping) : `monke.EndDayAtHelm` (préavis de vote immédiat), `monke.Galleon.RaiseAnchor` (tenir la barre sans clavier, debout à la barre), `monke.Galleon.Vote`, `monke.Galleon.Status`.

## Arrivée et retardataires (T-292 ; revue réseau T-268 R-02)
- L'horloge ne démarre plus au BeginPlay du serveur (`bStartOnBeginPlay` à faux) : elle attend que chaque joueur de la carte précédente (`TakeTravelPlayerCount`) ait un pion et ait chargé l'île, sans client encore en connexion, au plus `CrewLoadTimeoutSeconds` (60 s, `DA_ExpeditionRules`). Solo : aucune attente. Carte ouverte directement (PIE en serveur d'écoute) : `CrewJoinGraceSeconds` (3 s) pour les clients en route. HUD : « En attente de l'équipage : 1 / 2 (58 s) ». Spec `monke_island.Expedition.CrewLoad`.
- Un joueur dont le pion arrive après l'ancrage est posé sur le pont (`AGalleonSpawner`, deux images après son pion, après la barque de `UBoatStartSubsystem`).
- La téléportation sur le pont et par l'échelle tourne aussi la vue du client (R-11).

État de preuve (2026-10-11) : compilé fichier par fichier (UBT `-SingleFile`, éditeur ouvert) ; specs écrites, non exécutées ; non testé en éditeur, non testé à deux.

