# 54 — Voyages, sessions et perte de l'hôte
Domaine : F · Auditeur : ingénieur sessions et voyages

## Verdict
Aucun voyage multijoueur n'est éprouvé (voir 27, 36), et le jeu ne branche aucun crochet d'échec du moteur : hôte parti, client lent ou carte absente finissent au menu, sans un mot. Le « plafond de 60 s » du cahier existe déjà, mais comme délai moteur muet. Risque commercial : deux voyages par manche, donc deux occasions de perdre un ami sans explication.

## Constats
- **[Bloquant commercial] Le « plafond de 60 s » est un délai moteur muet**
  - Constat : en voyage dur, le client charge sa carte en bloc entre `Welcome` et `Join`, sans émettre (supposé) ; le serveur le coupe après 60 s de silence et ignore les connexions pendant sa bascule. Disque dur à froid : plus de 30 s (47, supposé). Boot : connexions seules (27, 34). `LoadTimeoutSeconds` ne garde que le catalogue ; l'horloge part au `BeginPlay`.
  - Preuve : BaseEngine.ini:1855-1856 ; NetConnection.cpp:4757 ; UnrealEngine.cpp:16037 ; World.cpp:7089 ; FragmentRules.h:23 ; ExpeditionGameState.h:46.
  - Recommandation : `TravelToMapURL` fige la liste des attendus ; l'horloge attend leurs `PostLogin` 60 s, puis `KickPlayer` motivé ; `InitialConnectTimeout=90` pour que le jeu coupe d'abord.
  - Effort : M
- **[Majeur] Deux chemins de voyage : « Recommencer » largue l'équipe**
  - Constat : TAPAO voyage par `OpenLevel`, local, qui n'emmène aucun client ; le GameState passe par `ServerTravel`, qui les emmène. Moins de 2 s après sa mort, l'hôte voit « Recommencer » : un clic renvoie ses amis vivants au menu (effet déduit). « Menu » ne dit pas que la session s'arrête pour tous (§16).
  - Preuve : TapaoUISubsystem.cpp:497-505,604,633-645 ; TapaoUISubsystem.h:79 ; GameplayStatics.cpp:1006 ; GameModeBase.cpp:393-435.
  - Recommandation : un seul service de voyage, hôte seul, `ServerTravel` ; « Recommencer » seulement équipage entier mort ; avertissement avant « Menu ».
  - Effort : S
- **[Majeur] Perte de l'hôte : 45 à 60 s de gel, motif jeté trois fois**
  - Constat : plantage ou coupure de l'hôte : « instable » à 45 s, coupure à 60 s, menu muet (27, 37). Le moteur prévoit un motif : `ReturnToMainMenuHost` (jamais appelé) fabrique « Host has left », que le client ignore ; `ClientWasKicked` est vide ; les événements d'erreur de la `GameInstance` brute restent vides.
  - Preuve : NetDriver.cpp:1612,1656 ; GameSession.cpp:391-405 ; PlayerController.cpp:1915,3932 ; GameInstance.h:262-268 ; DefaultEngine.ini:14.
  - Recommandation : lier `OnNetworkFailure`/`OnTravelFailure`, surcharger `ReturnToMainMenu`, `ClientWasKicked` et `ClientReturnToMainMenuWithTextReason` ; `FText` affiché au menu, « Réessayer » (§16) ; bandeau sur `OnNetworkLagStateChanged`.
  - Effort : S (messages), M (écran)
- **[Majeur] Un vrai échec de voyage ramène au menu ; les replis sont du code mort**
  - Constat : `TravelToMapURL` annonce le succès dès que `ServerTravel` répond `true`, soit toujours hors caractère illégal. Une carte absente échoue plus tard (`ServerTravelFailure`) : l'hôte retombe sur `L_MainMenu`, ses clients déjà partis expirent à 60 s. `Fail()` → `FallbackMap` et `RestartFromBoot` ne servent pas ici.
  - Preuve : ExpeditionIslandSubsystem.cpp:544 ; GameModeBase.cpp:437-474 ; UnrealEngine.cpp:15209-15227,15974 ; ExpeditionBootGameMode.cpp:125-137.
  - Recommandation : pré-vol de la carte avant l'ordre (voir 32), motif mémorisé par `OnTravelFailure`.
  - Effort : S
- **[Majeur] Reconnexion : le moteur sait faire, le projet l'a coupée**
  - Constat : la reprise du `PlayerState` (300 s, par identifiant ou adresse et nom) n'existe que dans `AGameMode` ; nos modes sont `AGameModeBase`, le GameState `AGameStateBase`. Un ami qui revient est un nouveau joueur, assis vivant dans la barque : la mort définitive se contourne (supposé).
  - Preuve : GameMode.cpp:46,601-744 ; ExpeditionBootGameMode.h:20 ; ExpeditionGameState.h:27 ; BoatStartSubsystem.cpp:301.
  - Recommandation : `ARunGameMode` sur `AGameMode`, GameState sur `AGameState`, `PlayerKey` en `?Key=` ; état « absent » (voir 27).
  - Effort : M
- **[Majeur] Seamless ou dur, jamais « sans voyage » : la fiabilité se multiplie**
  - Constat : 2 voyages par manche ; à 2 % d'échec par client et voyage (illustratif), trois amis sur 8 voyages perdent quelqu'un 38 % du temps. T-088 et T-113 possèdent le même voyage seamless.
  - Preuve : ADR-005:46-51 ; board.md:95,120.
  - Recommandation : spike de 2 j sur un océan persistant, île en sous-niveau streamé (ni `PlayerState` copié, ni reconnexion, ni `L_Transit`) ; sinon fusionner T-088 dans T-113 et mesurer les voyages.
  - Effort : S (spike), L (adoption)

## Manques
1. Écran « Connexion perdue / Retiré / Hôte parti » avec Réessayer.
2. Barrière nominative (« Alice charge… ») et état « absent ».
3. `LogMonkeTravel` : ordre, arrivée de chaque client, durée (voir 47).
4. Reprise par un ami (copie de `SG_Run`), hors V1.

## Quick wins
1. « Recommencer » : hôte seul, équipage mort, via `TravelToMapURL` ; avertissement avant « Menu ».
2. Lier `OnNetworkFailure`/`OnTravelFailure`, surcharger `ReturnToMainMenu` : un `FText` affiché au menu.
3. `InitialConnectTimeout=90` ; `ConnectionTimeout=60` jusqu'à mesure (SteamSockets : 80/120).
4. `LogMonkeTravel` et deux fenêtres `-game` : distribution des arrivées.

## Questions qui challengent l'équipe
1. L'hôte plante à la manche 4 : trois amis, un menu muet, rien à reprendre. Acceptable pour un coop payant « entre amis » ?
2. Pourquoi voyager ? Un océan persistant supprimerait deux voyages par manche (47, Q3) : l'avez-vous instruit avant T-113 ?
3. Quel ami lent attendez-vous : 30 s, 60 s, jamais ? Qui tranche entre T-088 et T-113 ?
