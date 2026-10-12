# 32 — Architecture de la partie en manches (ADR-005)
Domaine : D · Auditeur : architecte gameplay Unreal

## Verdict
ADR-005 tient sur le papier (état hors des maps, calculs purs testés), mais sa couche réseau repose sur des préconditions absentes : seamless travel non configuré, ni PlayerState ni porte de session. Chaque manche coûte deux voyages ; un seul qui perd un joueur, le butin ou la valeur d'un objet tue la partie. Risque : le découvrir à la première session à deux.

## Constats
- **[Bloquant commercial] Voyage : aucun chemin configuré ni validé**
  - Constat : aucun GameMode n'active `bUseSeamlessTravel` (C++ et Blueprints : 0), `TransitionMap` est vide. Moteur 5.8 : en PIE le voyage redevient dur sauf `net.AllowPIESeamlessTravel=1` ; l'URL `?SeamlessTravel` force le mode sans toucher aux Blueprints ; une `TransitionMap` vide crée un monde factice (`L_Transit` superflu) ; les clients sont prévenus avant la vérification de la carte (absente : clients partis, hôte resté) ; `PostLogin` n'est pas appelé après un seamless.
  - Preuve : DefaultEngine.ini:10 ; BoatStartSubsystem.cpp:55 ; moteur GameModeBase.cpp:494,503-511,515-522,607-642, World.cpp:8291,8367.
  - Recommandation : `ServerTravel` + `?SeamlessTravel` partout (cvar PIE à 1), `DoesPackageExist` avant l'appel, arrivée signalée depuis `HandleStartingNewPlayer`, `L_Transit` supprimé.
  - Effort : M
- **[Majeur] PlayerState : contrat d'identité à fixer (absence : voir 11, 14)**
  - Constat : le seamless crée le PlayerState de la classe du GameMode d'arrivée, `Reset()` puis `CopyProperties` (propriétés de base seulement) : deux classes différentes perdent tout. Le repli dur recrée tout, donc « rien n'est perdu » (ADR §4) ne vaut que pour l'hôte.
  - Preuve : PlayerController.cpp:3652-3663 (moteur).
  - Recommandation : un `ACrownPlayerState` unique aux deux modes : `PlayerKey` (GUID du profil via `?Key=`, validé en `PreLogin`), `SlotIndex` (place sur le pont, aujourd'hui l'ordre des PC), `bReady`, `CopyProperties` et `OverrideWith` ; morts et ventes indexés par `PlayerKey`.
  - Effort : M
- **[Majeur] Sources de vérité multiples, DTO non répliquable**
  - Constat : `FRunProgress` et `FRunState` (ADR §3) dupliquent trésorerie et clôture ; `UExpeditionIslandSubsystem` garde déjà choix et tirage ; `ExpeditionId` est un `NewGuid()` alors que le cahier §24.1 veut `RoundId`. `FRunProgress` contient un `TMap`, non répliquable. `HasSelectionAuthority()` vaut vrai quand le monde est nul : fausse autorité client entre deux cartes.
  - Preuve : RunTypes.h:203-226 ; ExpeditionIslandSubsystem.cpp:389-394 ; ExpeditionGameState.cpp:110.
  - Recommandation : `FRunState` contient `FRunProgress` ; `URunSubsystem` seul écrivain (`bIsHost` explicite) et seul tireur de fragment ; `StartExpedition(RoundId)` ; URL = `?Island=` seul ; répliquer `FRunRoundResult` (déjà plat) recalculé après chaque vente, jamais `Sales`.
  - Effort : M
- **[Majeur] Cale : valeur non restaurée, destruction avant validation**
  - Constat : `BeginPlay` fait `Value = BaseValue` : un objet réapparu est réparé (RUN11 échoue, la cale devient un atelier). `FHoldEntry` perd l'état propre (cadavre, arme : supposé). L'ADR détruit avant que le voyage soit acquis, or il peut échouer.
  - Preuve : ValuableComponent.cpp:27 ; ADR §5.
  - Recommandation : capturer sans détruire (le voyage détruit le monde) puis voyager ; `SpawnActorDeferred` + `InitializeValue`. Mieux : cale = 40 emplacements en données (`FFastArraySerializer`), proxys côté client, acteur physique créé à la « prise ».
  - Effort : M
- **[Majeur] Voyage : déclencheurs concurrents, aucune phase**
  - Constat : vote, fin de submersion, équipage mort, hôte, retardataire peuvent tous voyager. Le moteur ignore un second `ServerTravel` mais renvoie `true` : `TravelToMapURL` croit réussir. `FRunState` n'a pas de phase. L'horloge démarre au `BeginPlay` serveur, avant les clients lents ; ni barrière de 60 s, ni `PreLogin` (RUN03) ; `MaxPlayers` vaut 16.
  - Preuve : World.cpp:9540-9557, BaseGame.ini:68 (moteur) ; ExpeditionIslandSubsystem.cpp:544 ; ExpeditionGameState.cpp:67-70,349 ; 0 `PreLogin` dans Source.
  - Recommandation : `FRunPhase` et table de transitions pures testées ; `RequestTravel(Raison)` à passage unique ; interactions verrouillées entre capture et bascule ; barrière de chargement ; `PreLogin` lisant la phase ; `MaxPlayers=4` en config.
  - Effort : M
- **[Majeur] Actions « maintenir » sans protocole**
  - Constat : `Interact` est ponctuel, or barre 2 s, palan 3 s, sermon 20 s sont tenus ; un composant du galion ne reçoit pas de RPC client (aucun propriétaire).
  - Preuve : IslanderInteractable.h:37-39 ; ADR §6-7.
  - Recommandation : `UChanneledActionComponent` sur le personnage (RPC début/annulation, minuteur et revalidation serveur), partagé par barre, palan, sermon ; votes répliqués avec échéance serveur.
  - Effort : M
- **[Mineur] ADR en retard sur le code**
  - Constat : `IsMooredTo()` existe déjà (`IsBoatMoored`) ; `UGalleonStartSubsystem` contredit `AGalleonSpawner` livré ; le cahier nomme `ARunGameState`, l'ADR un composant sur des GameState dont les Blueprints appartiennent à d'autres.
  - Preuve : GalleonActor.h:58-61 ; GalleonSpawner.cpp:43.
  - Recommandation : amender l'ADR avant T-113 : un orchestrateur de départ ; réplique `AInfo` posée par un `UWorldSubsystem`.
  - Effort : S

## Manques
1. Point de reprise à l'arrivée en zone sûre : sinon un crash en vente efface la manche (§24.7 n'exclut que l'expédition).
2. Test automatique à deux processus `-game`.

## Quick wins
1. `TravelToMapURL` : `DoesPackageExist`, `?SeamlessTravel`, refus pendant un voyage.
2. `DefaultGame.ini` : `[/Script/Engine.GameSession] MaxPlayers=4`.
3. `UValuableComponent::InitializeValue` et spec RUN11.
4. `FRunPhase` pure et sa spec (patron `ExpeditionClock`).
5. `ue.ps1 duo` : hôte et client en `-game`.

## Questions qui challengent l'équipe
1. Quel chemin de voyage livrez-vous ? Le PIE ne teste que le dur par défaut.
2. Les 40 objets de la cale doivent-ils être des corps physiques, ou un inventaire matérialisé à la prise suffit-il ?
3. Qui possède `ACrownPlayerState` : T-113, T-028 ou le profil (14) ?
