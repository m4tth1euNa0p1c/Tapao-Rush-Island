# Revue réseau T-268 : systèmes serveur jamais revus (porte REC)

Domaine : réseau et physique répliquée · Auditeur : ue-network-engineer · 2026-10-10 · Lot : L4 (prépare T-088 et T-172) · Recette visée : §12 n°2, 3, 8, 10, 12, 15

**Portée de la preuve.** Lecture seule du code (HEAD `2cf9598` + copie de travail du 2026-10-10), sans éditeur, sans PIE, sans build. **Rien ici n'est « testé réseau »** : chaque constat dit s'il est *lu dans le code* ou *supposé* (à confirmer en PIE hôte + client). Chemins relatifs à `monke_island/Source/monke_island/`. Valeurs des DataAssets : un nom de propriété absent du paquet `.uasset` signifie que la valeur par défaut du C++ s'applique (vérifié pour `BP_ExpeditionGameState`, `DA_GalleonRules`, `DA_FlagCeremonyRules`, `DA_ExpeditionRules`).

## Verdict

**Revue réseau non signée.** Ces systèmes ne passent pas la porte REC multijoueur en l'état : **3 bloquants, 17 majeurs, 28 mineurs**.

**L'autorité est saine.**
- Aucun client ne décide de dégâts, de butin, de temps, de valeur ni d'une validation de zone.
- L'horloge commune et le bonus de zone sont serveur et idempotents par (`ExpeditionId`, `ZoneId`). L'interface dérive du temps serveur.
- Le ramassage accorde une seule prise. La boutique, la cérémonie, la voix et la mitrailleuse ont un bon modèle réseau.
- Aucun RPC fiable n'est envoyé à chaque image.

**Trois défauts cassent pourtant une partie à deux en usage normal :**
- **R-01** : un joueur seul, au gouvernail, termine la journée de tous et tue ceux restés à terre ;
- **R-22** : un joueur mis KO ou lancé est simulé et relevé séparément par chaque machine, alors que le serveur le croit resté au point de départ ;
- **R-37** : l'ancien porteur d'une arme la fait tirer après qu'un autre l'a ramassée, et l'hôte le fait côté serveur.

**Les majeurs se rangent en quatre familles :**
- **voyage et arrivée** (préalables de T-088) : horloge lancée avant le chargement des clients, client qui lit sa propre partie, inventaire dupliqué par une déconnexion ;
- **bases mobiles** (barque, pont du galion) : jamais testées à deux, simulées avec des entrées locales ;
- **bande passante** : props, mobs, cadavres et totems sans fréquence ni dormance. L'estimation dépasse le budget de 16 Ko/s par client (`docs/conventions/performance.md:31`) dès un village de nuit ou un pont chargé ;
- **état écrasé ou désynchronisé chez le client** : valeur et santé écrasées au BeginPlay, visée, télégraphes, crabe.

**Rien n'est « testé réseau ».** La campagne PIE de 28 essais ci-dessous, à 0, 100 et 200 ms puis 1 à 3 % de perte, est la condition de toute signature.

## Synthèse par système

Débits : ordres de grandeur par client distant, à mesurer. L'hôte d'écoute réplique à chaque image (60 i/s), et la fréquence par défaut d'un acteur est de 100 Hz.

| Système | Autorité | Réplication | RPC | Arrivée en cours | Débit | Constats |
|---|---|---|---|---|---|---|
| Horloge, fin de jour, partie perdue (`Expedition/Core`) | serveur, minuteries | `ClockState`, `Phase`, temps de fin, OnRep | aucun | correcte | négligeable | R-02, R-13, R-14, R-21 |
| Partie et cupidité (`Run/`) | serveur (GameInstance de l'hôte) | `URunStateComponent` sur le game state | aucun (cupidité : interface de l'hôte) | correcte | négligeable | R-03, R-15, R-41 |
| Galion et gouvernail | serveur ; pose recalculée par chaque machine sur l'horloge serveur | `Rules` envoyé une fois, sans mouvement | via `ServerInteract` | correcte | environ 3 Ko/s par objet posé sur le pont | **R-01**, R-04, R-11, R-17 |
| Barque (ADR-003) | serveur (sièges, charge, eau) ; physique sur chaque machine | sièges, rameur, entrée, charge, eau, naufrage ; mouvement physique | rame non fiable (10/s au plus), arrêt fiable | correcte (garde de siège) | 2 à 3 Ko/s, plus environ 3 Ko/s par objet arrimé | R-05, R-09, R-12, R-19 |
| Saisie et interaction | serveur (poignée physique) | `HeldComponent`, `HeldMass` (propriétaire seul) | 5 fiables + 1 non fiable, sans limite | correcte | l'objet tenu à pleine fréquence | R-06, R-16 |
| Inventaire, ramassage, boutique | serveur | `Slots`, `ActiveSlot`, `bClaimed`, trésorerie | fiables, bornés, limités (écran, lâcher, achat) | correcte | faible | R-39, R-41, R-06 |
| Armes à feu, mitrailleuse, traçantes | serveur (munitions, cadence, dispersion, dégâts) | geste avec heure serveur, état compact | 1 fiable par clic ; mitrailleuse : 2 fiables par rafale ; impacts et traçantes non fiables | correcte | mitrailleuse 0,4 à 0,8 Ko/s | **R-37**, R-38, R-42 à R-44, R-48 |
| Sabre, torche, grenades | serveur | `MoveState`, `Fuel`, `bExploded`… | fiables, bornés par les gestes | gestes rejoués | torche 0,6 à 1,2 Ko/s | R-40, R-44 à R-47 |
| Portes du village | serveur + ouverture prédite pour son propre pion | `OpenDoorways`, `UnsealedVillages` | aucun | correcte | négligeable | R-08 |
| Cérémonie du drapeau | serveur | `State` (2 Hz + ForceNetUpdate), toujours pertinent | requête par la trompette | correcte | négligeable | R-10 |
| Voix | serveur (relais, mute des morts) | aucune | booléen, seau à jetons | correcte | VoIP du moteur | conforme |
| Caisse de transport | serveur | `Contents` (OnRep) | via l'interaction ; couvercle non fiable | correcte, contenu non pertinent | faible | R-06, R-07, R-18 |
| Furtivité | serveur | aucune | aucun | sans objet | aucun | conforme |
| Crabe géant et corps de frappe | serveur | `Action`, `Phase`, `bEngaged` | indices non fiables | phase correcte, action qui repart à 0 | un seul boss | **R-22**, R-23 à R-25, R-31 à R-33 |
| Tiki, tactiques, cendre | serveur | `CutBones`, `CombatReaction`, `Tactic`… | actions non fiables ; `MulticastBlind` fiable | correcte sauf télégraphes | 1 à 2 Ko/s par mob en mouvement | **R-22**, R-27 à R-30 |
| Cadavres, requin, Director | serveur | pose envoyée une fois, `SettledBodies` | morsure non fiable | correcte | cadavres sans dormance ni fin | R-28, R-34, R-35 |
| Soldats et sites du fort | serveur | `Action`, `Skin`, `Rules` ; même instance de niveau partout | non fiables | membres corrects, clips rejoués | à mesurer | R-26, R-32, R-36 |

## Constats bloquants

- **R-01 [Bloquant] Le gouvernail : un seul joueur termine la journée de tout l'équipage, et ceux restés à terre meurent 10 s plus tard**
  - Constat : n'importe quel joueur vivant à moins de 260 cm de la barre déclenche, d'une pression de F (la touche d'interaction commune), `EndDayEarly(User, HelmEndDaySeconds)`. 10 s plus tard (`DA_GalleonRules` ne surcharge pas la valeur), `ResolveDelugeEnd` tue tout joueur hors du volume du pont. Ni vote, ni confirmation, ni annulation ; les autres ne voient qu'un compte à rebours.
  - Preuve : `Galleon/GalleonActor.cpp:547-552` ; `Galleon/GalleonRules.h:228` ; `Expedition/Core/ExpeditionGameState.cpp:553-566` (départ), `:484-503` (mort de ceux hors du pont) ; `UI/Tapao/STapaoHUD.cpp:211-213` (seul avertissement). Lu dans le code.
  - Scénario (hôte + client) : le client rentre seul au galion et appuie sur F près de la barre (marchand et caisse sont à côté). L'hôte, à 300 m dans un village, meurt « emporté par le déluge » 10 s plus tard, et il repart avec le kit de base : `StoreLoadouts` retire l'équipement des morts.
  - Correctif : un **vote de départ** (cahier §6, votes de départ). La barre ouvre un vote répliqué dans le game state (votants, échéance en temps serveur). Le galion part quand tous les joueurs vivants sont à bord, ou à l'unanimité des vivants. Sinon, un délai long réglable (DataAsset, par exemple 60 s) avec une alerte sonore et visuelle pour tous. La requête passe par un RPC du PlayerController, et le serveur revérifie : vivant, à bord, phase. La même brique servira à « PRÊTS POUR LE DÉPART » (T-172) et à la question de cupidité (ADR-007).
  - Rôle : ue-network-engineer, règle validée par ue-game-designer et le propriétaire (sa demande du 2026-10-07 visait le solo). Effort : M.

- **R-22 [Bloquant] Un joueur mis KO ou lancé par un mob est simulé et relevé séparément par chaque machine, tandis que sa capsule serveur reste au point de départ**
  - Constat : `Knockout` part du serveur, puis un multicast fiable lance `StartRagdoll` sur chaque machine. Ensuite, chaque machine simule *son* ragdoll et décide *seule* du relevé : minuterie locale, puis téléportation au bassin local. Côté serveur, la capsule reste sans collision et le mouvement coupé à l'endroit du KO. `bIsRagdoll` n'est pas répliqué. Le combat du crabe en fait un usage normal (saisie puis lancer à 24 m/s, rugissement, charge, écrasement), comme le lancer du Gardien Crabe.
  - Preuve : `Player/IslanderCharacter.cpp:394-410`, `:560-605`, `:652-692` ; `Player/IslanderCharacter.h:327` ; déclencheurs `Creatures/GiantCrab.cpp:762`, `:787`, `:813`, `Creatures/GiantCrabMoves.cpp:48`, `:82`, `Creatures/TikiBulwarkTactics.cpp:217`. Mécanisme lu dans le code. L'audit 34 l'avait classé Majeur, sans ticket ; il devient bloquant avec le crabe.
  - Scénario (hôte + client) : le crabe lance le client. Le client atterrit en A et se relève quand son ragdoll se pose. Ses mouvements sont alors refusés par le serveur, qui le rappelle à la pince (capsule figée en `MOVE_None`). Il est ensuite téléporté en B, l'atterrissage calculé par le serveur. Pendant tout le KO (2,5 à 8 s), le serveur situe le joueur à la pince : ondes de choc, tirs de soldats et poursuites frappent là où le corps n'est plus.
  - Correctif :
    - le relevé est décidé par le serveur seul ;
    - le point et le cap du relevé sont répliqués (`FKnockoutRep` {bActive, Serial, RecoverLocation, RecoverYaw} avec OnRep) ;
    - le client se place sur ce point avant de rendre le mouvement ;
    - pendant le KO, la capsule serveur suit le bassin (environ 10 Hz, sans collision) ;
    - `bIsRagdoll` est répliqué pour les arrivants.
  - Rôle : ue-physics-animator, avec revue de ue-network-engineer. Effort : M.

- **R-37 [Bloquant] Les liaisons d'entrée d'une arme ne sont jamais retirées : l'ancien porteur pilote l'arme ramassée par un autre**
  - Constat : une arme lie ses actions (`IA_MusketFire`, `IA_MusketAim`, `IA_MusketReload`, `IA_FirearmInspect`, partagées par toutes les armes à feu dans `IMC_Musket`) sur l'`InputComponent` de son porteur local, une fois pour toutes. Les handlers ne vérifient pas que le porteur est toujours local. Or l'acteur de l'arme est **réutilisé** quand on la lâche puis qu'un autre la ramasse : il attend, caché, dans le pickup, puis `SetCarrier` le donne au nouveau porteur. Quand l'hôte est l'ancien porteur et qu'il équipe une autre arme à feu, `IMC_Musket` redevient actif chez lui. Ses clics passent par `RequestFire` → `HasAuthority()` → `ServerFire_Implementation()` de l'arme **du client** : un vrai tir serveur, depuis les yeux et dans la visée du client, avec ses munitions.
  - Preuve : `Weapons/FirearmWeapon.cpp:273-300` (liaison), `:302-305`, `:329-347` (requête, tir direct sur l'hôte à `:337`), `:1151-1153` ; même schéma dans `Weapons/MachineGunWeapon.cpp:168-223`, `Weapons/CutlassWeapon.cpp:236-333`, `Weapons/TorchItem.cpp:198-233` ; `Weapons/InventoryItemActor.cpp:40-44`, `:74-86` (changement de porteur sans retrait) ; `Weapons/InventoryPickup.cpp:140-165` (même acteur rendu). Lu dans le code et les assets.
  - Scénario (hôte + client) : l'hôte jette son pistolet vide, ou le lâche. Le client le ramasse, le recharge et l'équipe. L'hôte prend son mousquet et tire : le pistolet du client part aussi, côté serveur. Le clic droit de l'hôte fait viser le client. La mitrailleuse du client tire tant que l'hôte tient la gâchette. Le sabre et la torche ont le même défaut. Dans l'autre sens (le client était l'ancien porteur), il voit un geste fantôme, et LogNet montre le RPC rejeté.
  - Correctif :
    - garde `IsCarrierLocallyControlled()` en tête de chaque handler d'entrée ;
    - `AInventoryItemActor` garde l'`InputComponent` lié et les handles de liaison ;
    - il les retire (`RemoveBindingByHandle`) dans `OnRep_Owner`, `SetCarrier` et `EndPlay`, et remet `bInputBound` à false.

    Ajouter une spec, puis l'essai PIE n°20.
  - Rôle : ue-gameplay-programmer, avec revue de ue-network-engineer. Effort : S.

## Constats majeurs

- **R-02 [Majeur] L'horloge démarre au BeginPlay du serveur, avant que les clients aient chargé l'île, et le pont n'accueille que les joueurs déjà présents**
  - Constat : `bStartOnBeginPlay = true`, et `BP_ExpeditionGameState` ne le surcharge pas. L'expédition démarre donc dès le BeginPlay du game state serveur. `L_Boot` n'attend que la connexion sur la carte vide, pas le chargement de l'île. `AGalleonSpawner` ne place sur le pont que les pions qui existent au moment de l'ancrage. Un retardataire est assis dans la barque au quai (`UBoatStartSubsystem`), et sa scène d'arrivée abandonne au bout de 12 s.
  - Preuve : `Expedition/Core/ExpeditionGameState.h:58`, `.cpp:98-101` ; `Galleon/GalleonSpawner.cpp:288-297` ; `Expedition/Islands/ExpeditionBootGameMode.cpp:142-167` ; `Expedition/Core/DayTransitionSubsystem.cpp:270-275`. Déjà relevé par l'audit 34 du 2026-10-04 (« départ de manche sans barrière tous prêts »), sans ticket ni correctif. Lu dans le code.
  - Scénario : à deux en `-game`, le client charge l'île 20 s après l'hôte. Il perd 20 s d'horloge, apparaît dans la barque au lieu du pont, sans scène d'arrivée.
  - Correctif : passer `bStartOnBeginPlay` à false. Le serveur démarre l'horloge quand chaque joueur attendu (`TakeTravelPlayerCount`) a un pion et `HasClientLoadedCurrentWorld()`, avec un délai maximal (DataAsset). Un écouteur PostLogin / OnNewPawn place aussi les retardataires sur le pont. Ce correctif est un préalable de T-088.
  - Rôle : ue-network-engineer. Effort : S.

- **R-03 [Majeur] Le client lit sa propre partie (`URunSubsystem` chargé de son disque) au lieu de la partie répliquée**
  - Constat : `URunSubsystem` vit sur la GameInstance de chaque machine et charge le slot `Run` du disque local à son `Initialize`. Seule la copie du serveur compte, et la partie répliquée est `URunStateComponent`. Pourtant, l'enchaînement de fin de jour côté client teste `Run->GetState().bActive` sur la copie locale. Un client sans partie solo en cours voit donc l'écran de fin « ÎLE CONQUISE » (seul « Menu principal » est actif, et il déconnecte) par-dessus la scène de départ et le résumé du jour, à chaque fin de jour. Un client qui a une partie solo sur son disque voit la bonne scène, par hasard. Le HUD choisit de même d'afficher le jour du cycle d'après la partie locale.
  - Preuve : `Run/RunSubsystem.cpp:115-123` ; `UI/Tapao/TapaoUISubsystem.cpp:724-744` ; `UI/Tapao/TapaoUISubsystem.cpp:617-660` (boutons de l'écran) ; `Player/IslanderHUD.cpp:92`. Lu dans le code ; l'effet à l'écran reste à confirmer en PIE.
  - Scénario : PIE à deux, client sans fichier `Run`. À la fin du jour 1, l'hôte voit le départ et « JOUR 1 TERMINÉ », le client voit « ÎLE CONQUISE ».
  - Correctif : règle « un client ne lit jamais `URunSubsystem` ». Les lectures d'interface passent par `URunStateComponent` (ajouter un champ répliqué `bRunActive` si besoin). Sur un client, `URunSubsystem::Initialize` ne charge pas le slot `Run` : il peut tester le NetMode au premier `Get`, ou marquer sa copie « non autoritaire ». Ajouter un test Automation.
  - Rôle : ue-systems-programmer, avec ue-ui-developer. Effort : S.

- **R-04 [Majeur] Le pont du galion roule sur une coque cinématique : les objets posés dessus ne dorment jamais et les joueurs debout sont corrigés**
  - Constat : chaque machine bouge la coque (`Hull`, collision « BlockAll ») à chaque image, d'après l'horloge serveur : roulis 1°, tangage 0,4°, pilonnement de 6 cm. Tout corps physique posé sur le pont est donc entraîné et reste éveillé : le butin déposé, la caisse T-258, les objets lâchés. Son mouvement répliqué change à chaque image et part à chaque mise à jour réseau (fréquence par défaut 100 Hz, plafonnée par l'image de l'hôte ; aucune dormance sur `APhysicsPropActor`). L'horloge serveur estimée par le client a un retard d'un demi-aller-retour, plus des à-coups de 0,1 s : la pose du pont diffère donc de quelques millimètres à quelques centimètres entre machines. Le mouvement basé des joueurs debout sera corrigé.
  - Preuve : `Galleon/GalleonActor.cpp:327-332` (pose par image), `:140` (mouvement non répliqué) ; `Items/PhysicsPropActor.cpp:12-17` (100 Hz par défaut, sans dormance) ; plafond `MaxClientRate=100000` octets/s (`BaseEngine.ini` d'UE 5.8, non surchargé dans `Config/DefaultEngine.ini`). Lu dans le code ; débit supposé.
  - Ordre de grandeur, à mesurer : environ 40 à 60 octets par envoi de mouvement, 60 envois/s, soit environ 3 Ko/s par objet et par client. Avec 20 à 30 objets à bord, on atteint 60 à 90 Ko/s par client, au bord du plafond de 100 Ko/s. Le budget du projet est de 16 Ko/s en moyenne et 64 Ko/s en pic par client (`docs/conventions/performance.md:31`) : il est dépassé dès que le pont porte une vingtaine d'objets, et le reste du jeu (personnages, mobs) passe en retard.
  - Correctif : rendre le roulis purement visuel : coque de collision fixe, mesh visuel qui roule (le joueur peut suivre par la caméra). À défaut, ranger le butin du pont comme le fait la barque, mais en attachant les objets (physique coupée, comme le contenu de la caisse T-258). Dans tous les cas : `SetNetUpdateFrequency` de 20 à 30 Hz et `net.UseAdaptiveNetUpdateFrequency 1` pour les props, dormance au repos.
  - Rôle : ue-network-engineer, avec ue-gameplay-programmer. Effort : M.

- **R-05 [Majeur] Barque : la physique répliquée n'a pas de mode choisi, la houle est locale, et la cargaison arrimée est téléportée par le serveur seul (ADR-003)**
  - Constat : la coque simule sur toutes les machines avec `SetReplicateMovement(true)` et le mode de réplication physique par défaut (aucun `SetPhysicsReplicationMode`, aucun réglage de prédiction dans les `.ini`). Les forces sont « les mêmes partout », mais trois entrées sont locales :
    - la houle, qui suit `SwellTime`, le temps écoulé depuis le BeginPlay de chaque machine, et se décale donc d'une machine à l'autre ;
    - le poids des passagers, lu par les recouvrements locaux ;
    - la surface de l'eau, lue dans la simulation d'inondation locale.

    Le client dérive donc en permanence et il est rappelé par la correction. Les joueurs debout, qui s'appuient sur la coque, et le rameur sont attachés à cette coque corrigée. En plus, à chaque image, le serveur seul téléporte la cargaison arrimée sur la coque ; le client simule ces objets librement : chez lui, ils glissent puis reviennent. La barque tangue toujours à cause de la houle, donc chaque objet arrimé repart à chaque mise à jour, soit environ 3 Ko/s par objet et par client. Avec 12 objets, cela fait environ 36 Ko/s par client, plus du double du budget moyen de 16 Ko/s.
  - Preuve : `Boats/BoatActor.cpp:48-55`, `:412`, `:421-433`, `:551-557`, `:692` ; `docs/adr/ADR-003-barque-physique.md` (Conséquences : revue réseau demandée) ; T-039 « non testé multi-machines ». Lu dans le code ; l'effet visible reste à confirmer en PIE.
  - Correctif :
    - houle sur l'horloge serveur, comme le galion ;
    - `SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation)` sur la coque, choisi puis testé sur UE 5.8 (référence : networked-physics-overview) ;
    - cargaison arrimée **attachée** à la coque, physique coupée, comme le contenu de la caisse : aucun octet de mouvement tant qu'elle est arrimée ;
    - 30 à 60 Hz pour la barque.

    Si la barque ne tient pas à 150 ms, la rendre cinématique côté client.
  - Rôle : ue-boat-programmer, avec revue de ue-network-engineer. Effort : M.

- **R-06 [Majeur] `ServerInteract` et `ServerGrab` mesurent la portée et la ligne de vue sur un point fourni par le client, et plusieurs cibles n'ont aucun contrôle de portée**
  - Constat : la distance est mesurée des yeux au point `Point` / `GrabPoint` envoyé par le client (250 cm), pas à la cible. Le tracé de ligne de vue passe s'il **ne touche rien**. Un client modifié envoie un point dans le vide devant ses yeux et n'importe quelle cible de la carte. `Interact` est alors exécuté si la cible ne revérifie pas la portée. C'est le cas de la caisse (`CanInteract` : seulement `IsValid`, et accroupi elle se **vide à distance**), des consommables (soin à distance), des munitions (`AAmmoPickup`, ramassées à distance, à travers un mur ou assis aux rames) et du marchand (écran seulement : l'achat revérifie `BuyReach`). `AInventoryPickup` revérifie bien la portée sur la boîte de la cible. La saisie à distance est bornée par le relâchement à 400 cm au tick suivant, mais la saisie à travers un mur reste possible. Aucun débit limité sur ces RPC fiables. Déjà relevé par l'audit 34, non corrigé.
  - Preuve : `Interaction/IslanderGrabComponent.cpp:329-358`, `:500-531`, `:760` ; `Props/LootCrate/LootCrateActor.cpp:435-450` ; `Items/ConsumablePickupActor.cpp:62-74` ; `Weapons/AmmoPickup.cpp:203-225` ; `Shop/ShopkeeperActor.cpp:81-92` ; modèle à suivre : `Weapons/IslanderInventoryComponent.cpp:612-621`. Lu dans le code ; exige un client modifié.
  - Correctif : distance des yeux à la **boîte de collision de la cible** (le point le plus proche), et `Point` refusé s'il est à plus de quelques centimètres de cette boîte. Le tracé doit **toucher la cible**. Limite de débit par joueur (par exemple 8 requêtes/s). Optionnel : un `_Validate` pour les nombres non finis. Coordonner avec T-269 (revue sécurité en cours) pour un seul correctif.
  - Rôle : ue-gameplay-programmer. Effort : S.

- **R-07 [Majeur] La valeur d'un objet est écrasée au BeginPlay chez le client (non corrigé depuis l'audit 34)**
  - Constat : `Value = BaseValue` s'exécute sur toutes les machines, après réception de l'état initial. Chez le client, `BaseValue` n'a pas reçu le multiplicateur de zone ni de cupidité (calcul serveur seul), et le serveur ne renvoie pas `Value` tant qu'elle ne change pas chez lui. Le client affiche donc la valeur de base, non multipliée et non abîmée. Cela touche chaque objet de la zone 2 et au-delà, chaque jour de cupidité, et chaque objet recréé chez le client : sortie de pertinence (150 m), ou objets rangés dans la caisse (cachés et sans collision, donc non pertinents, puis détruits et recréés au vidage, voir R-18).
  - Preuve : `Items/ValuableComponent.cpp:28-36` ; moteur `ActorReplication.cpp:406-409` (un acteur caché sans collision n'est pas pertinent). Lu dans le code.
  - Correctif : `Value = BaseValue` seulement avec l'autorité. Test : objet d'une zone avec un multiplicateur de 1,5, client = serveur.
  - Rôle : ue-gameplay-programmer. Effort : S.

- **R-08 [Majeur, à confirmer] Portes du village : au sprint, le serveur ouvre trop tard pour un pion client, qui est corrigé contre un panneau encore fermé**
  - Constat : le client ouvre tout de suite les portes près de son pion (bonne idée). Le serveur, lui, ne voit le pion qu'avec un demi-aller-retour de retard, il ne vérifie que toutes les 0,1 s, et le panneau met 0,35 s à glisser (moitié ouvert vers 0,18 s). Le déclencheur est à 150 cm du seuil, soit 0,25 s au sprint (600 cm/s). En LAN, le serveur ouvre en 0,18 à 0,28 s : c'est déjà juste. À 100 ms de latence, il lui faut 0,28 à 0,38 s : la capsule du client touche le panneau côté serveur, le mouvement est rejeté et le client recule.
  - Preuve : `Village/VillageDoorsComponent.cpp:194-250` (contrôle serveur), `:329-346` (glissement) ; `Village/VillageHouseKit.h:154`, `:162`, `:170` ; `Village/VillageDoorMath.h:13-20` ; `Player/IslanderMovementComponent.h:31`. Calcul tiré du code ; à confirmer en PIE avec PktLag=100.
  - Correctif : côté serveur, anticiper selon la vitesse. `MarkNear` élargit la profondeur pour les pions distants : `TriggerDepth + v·(CheckInterval + 0,5·SlideSeconds + 0,15 s)`. C'est une fonction pure dans `VillageDoorMath`, avec un test.
  - Rôle : ue-gameplay-programmer. Effort : S.

Créatures, fort et furtivité. Relu par une passe parallèle ; les citations clés ont été recontrôlées. `Creatures/TikiCreature.cpp` a été modifié par un autre agent à 23:29 pendant la revue, et les numéros ont été revérifiés après.

- **R-23 [Majeur] Saisie par le crabe : le client tenu n'est déplacé que par des corrections.** `DisableMovement` n'est appelé que sur le serveur, alors que le commentaire dit « on every machine ». Le serveur téléporte ensuite le pion sur la pince à chaque image. Le client continue de bouger localement jusqu'à la première correction, puis suit la pince par à-coups (environ 0,5 s), décalé de la pince qu'il voit. Le KO de R-22 part alors d'une position déjà périmée. Preuve : `Creatures/GiantCrab.cpp:167-173`, `:640-647`, `:805`. Correctif : attacher le pion au socket de la pince (attachement répliqué, comme les sièges de la barque), ou au minimum un RPC client qui coupe le mouvement au moment de la saisie. Rôle : ue-gameplay-programmer. Lu dans le code ; à voir en PIE avec PktLag=100.
- **R-24 [Majeur, supposé] Les corps de frappe du crabe bloquent les pions, mais ils sont animés différemment sur chaque machine.** Chaque machine les construit localement, attachés aux os et non répliqués. Le clip démarre à la réception de `Serial`, sans heure de départ serveur. Le déplacement prédit du client heurte des pattes qui ne sont pas au même endroit côté serveur : corrections en mêlée (sabre) et pendant la charge. Preuve : `Creatures/GiantCrabBody.cpp:99-110` ; `Creatures/GiantCrabRules.h:543` (`bHitBodiesBlockPawns`) ; `Creatures/GiantCrab.cpp:115`, `:978-986`. Correctif : seules des formes non animées, posées sur la racine, bloquent les pions ; les corps animés restent pour les requêtes. Ajouter `StartServerTime` à l'action répliquée et avancer le clip client de l'âge de l'action, comme `CombatReaction` des tiki. Rôle : ue-gameplay-programmer.
- **R-25 [Majeur, supposé] Les points faibles du crabe sont résolus sur la pose serveur, sans compensation de latence.** L'œil, et le coude coincé après un écrasement, sont testés sur les sockets et la `ClipTime` du serveur. À 100–200 ms, un client qui vise l'œil voit son tir compté sur la carapace : retourner le boss devient presque réservé à l'hôte. Preuve : `Creatures/GiantCrabMoves.cpp:451-469` ; tracé serveur `Weapons/FirearmWeapon.cpp:705-707`. Correctif : aligner les clips (R-24), puis ajouter une tolérance proportionnelle au ping sur le rayon des points faibles et sur la fenêtre `SlamStuck`, ou garder sur le serveur 0,5 s d'historique des transforms des corps de frappe. Rôle : ue-gameplay-programmer, avec ue-network-engineer.
- **R-26 [Majeur] Un mob mort reçu avant son BeginPlay reste « vivant » chez le client.** La mort est émise par `OnRep_Health` avant que le mob ait lié `OnDeath` (la liaison se fait dans son BeginPlay). De plus, `Health = MaxHealth` s'exécute sur toutes les machines. Cas concerné : un soldat ou un crabe mort qui revient en pertinence (au-delà de 150 m) ou un client qui arrive en cours. Côté client, la capsule debout bloque encore les pions et la caméra (corrections en traversant le fort), et la barre de vie d'un blessé revient pleine. Preuve : `Player/IslanderHealthComponent.cpp:25`, `:236-243` ; liaisons dans `Fort/FortSoldier.cpp:208`, `Creatures/GiantCrab.cpp:108`, `Creatures/TikiCreature.cpp:106`, `Creatures/SharkActor.cpp:61`. Correctif : `Health = MaxHealth` seulement avec l'autorité. Dans le BeginPlay de chaque mob, après la liaison, appeler la mort en version silencieuse si le mob est déjà mort. Même famille que R-07. Rôle : ue-gameplay-programmer.
- **R-27 [Majeur] Les télégraphes des tiki passent par un multicast non fiable, sans état de clip répliqué.** Avec PktLoss de 1 à 3 %, ou un mob qui devient pertinent pendant sa préparation, le client prend une attaque, une saisie ou un coup lourd sans avoir vu la préparation (cahier §5). Preuve : `Creatures/TikiCreature.h:539-541` ; `Creatures/TikiCreature.cpp:280-286` ; `Creatures/TikiWarrior.cpp:489` ; `Creatures/TikiBulwarkTactics.cpp:155` ; `Creatures/TikiStalkerTactics.cpp:90` ; `Creatures/TikiWatcherTactics.cpp:88`. Correctif : un état d'action répliqué {Action, Variant, Serial, StartServerTime}, sur le modèle du crabe et des soldats ; le non fiable reste pour le son et le sang. Rôle : ue-ai-programmer.
- **R-28 [Majeur] Mobs, soldats, cadavres, totems et requin n'ont ni fréquence réseau ni dormance.** Les cadavres ont une durée de vie infinie (`CorpseLifetime = 0`), jusqu'à 80 à la fois. 120 totems sont en physique, sans dormance. La fréquence par défaut de 100 Hz s'applique partout. Ordre de grandeur, à mesurer : 1 à 2 Ko/s par mob en mouvement et par client. Un village de nuit avec 20 à 40 mobs pertinents coûte 30 à 80 Ko/s par client, soit 90 à 240 Ko/s en émission pour l'hôte avec 3 clients, en plus des props (R-04, R-05). C'est 2 à 5 fois le budget moyen de 16 Ko/s par client (`docs/conventions/performance.md:31`). Preuve : `Creatures/TikiCreature.cpp:47-84` ; `Creatures/TikiCorpse.cpp:82-83` ; `Creatures/TikiRagdollCorpse.cpp:47-48` ; `Creatures/TikiRules.h:336`, `:360` ; `Creatures/TikiDirector.cpp:1437-1438` ; `Creatures/SharkActor.cpp:26-27`. Correctif :
  - mobs à 20–30 Hz, minimum 2 Hz, `net.UseAdaptiveNetUpdateFrequency 1` ;
  - cadavres et totems à 5–10 Hz, `DORM_DormantAll` une fois endormis, `FlushNetDormancy` quand on les saisit ;
  - durée de vie limitée pour les soldats morts ;
  - `NetCullDistance` réduite pour les villageois ;
  - mesure `stat net` à 4 joueurs.

  Rôle : ue-network-engineer, mesure par ue-performance-engineer.

Armes, inventaire et ramassage. Relu par une passe parallèle ; les citations clés ont été recontrôlées.

- **R-38 [Majeur] La visée est désynchronisée entre le porteur et le serveur.** `bAiming` est répliqué en `COND_SkipOwner`. Le serveur force la visée à faux pendant un rechargement (y compris l'auto-rechargement au tir à vide), un abandon d'arme ou une inspection, sans prévenir le propriétaire. Le client garde le clic droit, reste en visée chez lui (FOV, tir visé prédit), alors que le serveur tire avec la dispersion de hanche et que les autres le voient tirer à la hanche. Preuve : `Weapons/FirearmWeapon.cpp:103`, `:387`, `:510` ; `Weapons/PistolWeapon.cpp:130` ; `Weapons/ShotgunWeapon.cpp:182`. Le contre-exemple correct est `Weapons/PirateMusketWeapon.cpp:51-58`. Correctif : le porteur applique la même règle à partir du geste serveur répliqué, et renvoie `ServerSetAiming(true)` si le clic droit est encore tenu à la fin du geste. Rôle : ue-gameplay-programmer.
- **R-39 [Majeur] Une arme de mob ramassée puis lâchée revient à neuf.** Le pickup est détruit avec sa valeur, et le lâcher recrée un `BP_Pickup_*` dont la valeur repart de `BaseValue`, multipliée par la zone **courante**. Une lance abîmée retrouve sa pleine valeur, et une arme gardée d'une île à l'autre prend le multiplicateur de l'île où on la lâche. Preuve : `Weapons/InventoryPickup.cpp:140-165` ; `Weapons/IslanderInventoryComponent.cpp:689-698` ; `Items/ValuableComponent.cpp:28-36`. Correctif : l'objet porte sa valeur courante, rendue au pickup par une apparition différée avant son BeginPlay, sans réappliquer le multiplicateur. Rôle : ue-systems-programmer.
- **R-40 [Majeur] Le carburant de la torche est répliqué à chaque image.** `Fuel` est répliqué sans condition et décrémenté à chaque tick serveur, sur un acteur à 100 Hz. Ordre de grandeur, à mesurer : 0,6 à 1,2 Ko/s par torche allumée et par client, soit 2,5 à 5 Ko/s pour 4 torches de nuit (15 à 30 % du budget moyen), pour une valeur qui se calcule. Preuve : `Weapons/TorchItem.cpp:56`, `:352`. Correctif : répliquer l'heure serveur d'allumage et le carburant de départ, puis calculer `Fuel` localement. Baisser `NetUpdateFrequency` des objets portés et appeler `ForceNetUpdate` aux changements. Rôle : ue-gameplay-programmer.
- **R-41 [Majeur] L'inventaire gardé entre les îles (`FRunLoadout`) n'est jamais consommé : duplication par déconnexion.** `StoreLoadouts` ne parcourt que les joueurs présents, donc l'entrée d'un absent reste celle de la veille. La restauration la lit sans la retirer. Scénario : A donne son mousquet à B, se déconnecte et revient (même jour ou île suivante) : il retrouve son mousquet. Il y a alors deux mousquets, et les munitions dépensées sont rendues. Preuve : `Run/RunSubsystem.cpp:177-197` ; `Weapons/IslanderInventoryComponent.cpp:71-116`. Correctif : consommer l'entrée à la restauration (`TakeLoadout`) et enregistrer l'inventaire d'un joueur qui part au Logout. Rôle : ue-systems-programmer, lié à T-088.
- Le ramassage de munitions (`AAmmoPickup`) n'a pas de revérification serveur de la portée, de la vue et du siège. C'est un cas de **R-06**, traité avec lui. Preuve : `Weapons/AmmoPickup.cpp:203-225` ; à comparer avec `Weapons/IslanderInventoryComponent.cpp:612-621`.

## Constats mineurs

Barque, galion, saisie, partie, cérémonie (lus dans le code sauf mention).

- **R-09 [Mineur] Rames : la commande part en non fiable, seulement quand elle change.** Si le paquet se perd (PktLoss de 1 à 3 %), le serveur garde l'ancienne commande tant que le rameur ne change pas de touche : la barque reste immobile alors que Z est enfoncé. Preuve : `Boats/BoatRiderComponent.cpp:203-215`, `.h:71`. Correctif : renvoyer la commande non nulle toutes les 0,25 s, même si elle est inchangée.
- **R-10 [Mineur, à confirmer] Cérémonie : l'immobilité (10 cm) est mesurée sur la position serveur d'un pion client.** Le point de départ (`GatherCenter`) est la position serveur au moment de la requête. Le client n'est pas immobilisé : un appui sur F en fin de freinage (environ 30 cm depuis 350 cm/s), ou un dernier mouvement encore en vol, suffit à interrompre le rituel (« Moved »). Une poussée d'un villageois du cercle sur le serveur aussi. Preuve : `Ceremony/FlagCeremonySite.cpp:146`, `:190-194` ; `Ceremony/FlagCeremonyRules.h:64`. Correctif : n'envoyer la requête qu'à l'arrêt (vitesse 2D quasi nulle, aussi dans `CanStart`), et prendre `GatherCenter` après une courte grâce (0,3 s, DataAsset).
- **R-11 [Mineur] Galion : la téléportation sur le pont ou par l'échelle ne règle la rotation de contrôle que sur le serveur.** Le client distant garde sa direction de regard (pas de `ClientSetRotation`, contrairement au départ en barque). Preuve : `Galleon/GalleonActor.cpp:453` ; à comparer avec `Boats/BoatStartSubsystem.cpp:416-423`.
- **R-12 [Mineur] Naufrage : `LaunchCharacter` est appelé côté serveur sur des pions clients.** Le client ne le prédit pas : il est rattrapé par une correction (saut net). Preuve : `Boats/BoatActor.cpp:1027`. Correctif : un RPC client « éjecté » qui lance aussi localement.
- **R-13 [Mineur] Fin de jour non indexée par `ExpeditionId` dans la partie.** `URunSubsystem::EndDay` compte un jour à chaque appel ; l'idempotence (cahier §6) ne repose que sur `IsExpeditionOver()` du game state. Preuve : `Run/RunSubsystem.cpp:292-314` ; `Expedition/Core/ExpeditionGameState.cpp:508`. Correctif : mémoriser le dernier `ExpeditionId` résolu dans l'état de la partie (sauvegardé) et refuser un second `EndDay` avec le même identifiant, avec un test.
- **R-14 [Mineur] Voyage raté = impasse.** Si `RestartFromBoot` échoue, un log `Error` est écrit et les joueurs restent sur la carte : pions cachés et entrées coupées par la scène de départ sur chaque machine, sans nouvel essai ni message. Preuve : `Expedition/Core/ExpeditionGameState.cpp:703-708` ; `Expedition/Core/DayTransitionSubsystem.cpp:322-339`. Correctif : nouvel essai avec repli (`FallbackMap`) et message au joueur (`error-handling.md`, ERR-04).
- **R-15 [Mineur] La question de cupidité est tranchée par l'hôte seul.** Les clients lisent « le capitaine décide » ; l'ADR-007 demande un vote de l'équipage. Preuve : `UI/Tapao/TapaoDayUISubsystem.cpp:155-167`. À traiter avec la brique de vote de R-01 (T-172).
- **R-16 [Mineur] Objet tenu : pas de prédiction locale.**
  - L'objet suit le ressort du serveur : chez le porteur, il a un aller-retour de retard quand il tourne vite.
  - Le lancer part dans la direction de contrôle connue du serveur (un demi-aller-retour plus ancienne).
  - `CountHolders` parcourt tous les personnages à chaque image de chaque porteur (ARC-05).

  Preuve : `Interaction/IslanderGrabComponent.cpp:118-129`, `:613-619`, `:746-801`. À mesurer en PIE avec PktLag=100 avant de choisir une prédiction.
- **R-17 [Mineur] Coûts par image du galion.**
  - `TActorIterator<AFloodSimulationActor>` à chaque image (ARC-05) : `Galleon/GalleonActor.cpp:313`.
  - Le marchand attaché au pont réplique son mouvement à 100 Hz sans dormance alors qu'il ne bouge pas : `Shop/ShopkeeperActor.cpp:21-22`.
- **R-18 [Mineur] Caisse : son contenu sort de la pertinence.** Un objet rangé est caché et sans collision : il n'est plus pertinent, il est détruit chez les clients après environ 5 s, puis recréé au vidage. Ce n'est pas un défaut en soi (bande passante économisée), mais il hérite de R-07 (valeur fausse à la recréation). Preuve : `Props/LootCrate/LootCrateActor.cpp:322-347` ; moteur `ActorReplication.cpp:406-409`.
- **R-19 [Mineur] Arrivée en cours : un client qui rejoint après un naufrage voit le message « La barque a coulé ».** `OnRep_Sunk` le rejoue à l'état initial. Preuve : `Boats/BoatActor.cpp:1038-1054`.
- **R-20 [Mineur, hors périmètre, à mesurer] Inondation : chaque machine fait avancer la simulation d'eau peu profonde avec son propre pas.** Les surfaces divergent au front d'eau : un client se voit marcher là où le serveur le fait nager (corrections). La noyade est décidée sur l'eau du serveur. Le galion, mouillé en eau profonde, ne devrait pas être touché. Preuve : `Flood/FloodSimulationActor.cpp:556`, `:566-591`. À mesurer en PIE à deux pendant la submersion avant tout ticket.
- **R-21 [Mineur] `EarlyEndTimer` n'est pas effacé dans `EndPlay`** (sans effet pratique, les minuteries meurent avec le monde). Preuve : `Expedition/Core/ExpeditionGameState.cpp:104-112`.

Créatures, fort et furtivité :

- **R-29 [Mineur] Les reculs infligés par les mobs ne sont appliqués que par le serveur sur un pion joueur.** Le joueur ne voit le recul qu'avec la correction, en un à-coup. Preuve : `Creatures/TikiWarrior.cpp:996`, `Creatures/TikiBulwarkTactics.cpp:302`, `Creatures/SharkActor.cpp:312`, `Creatures/GiantCrab.cpp:732`. Correctif : un RPC client non fiable qui applique la même poussée chez le propriétaire, ou un Root Motion Source.
- **R-30 [Mineur] `MulticastBlind` est fiable et part vers toutes les connexions** alors qu'un seul joueur est aveuglé. Preuve : `Creatures/TikiAshBurst.h:124-126`. Correctif : un RPC client fiable sur le PlayerController de la victime, ou `BlindUntil` répliqué en `COND_OwnerOnly`.
- **R-31 [Mineur] Émergence du crabe : la collision n'est coupée que sur le serveur** (`bActorEnableCollision` n'est pas répliqué). Pendant la montée, les corps de frappe bloquent les pions côté client. Preuve : `Creatures/GiantCrabEmergence.cpp:214-215`. Correctif : un état « en montée » répliqué.
- **R-32 [Mineur] Les clips d'état repartent de zéro à l'arrivée ou au retour de pertinence** : un crabe ou un soldat mort rejoue sa mort et ses sons. Preuve : `Creatures/GiantCrab.cpp:978-986`, `Fort/FortSoldier.cpp:1770-1776`. Correctif : `StartServerTime`, et pose finale directe pour les morts.
- **R-33 [Mineur] La libération du joueur saisi est incomplète.**
  - `EndPlay` du crabe ne libère pas `Caught` : le pion reste en `MOVE_None`.
  - Un joueur tué pendant la saisie continue d'être téléporté, puis tombe en `MOVE_Falling` avec une capsule sans collision.

  Preuve : `Creatures/GiantCrab.cpp:151-158`, `:167-173`, `:805`. Correctif : libérer dans `EndPlay`, revérifier la proie à chaque image, ne pas changer le mode de mouvement d'un mort.
- **R-34 [Mineur] Le requin saccade chez le client** : acteur cinématique téléporté à chaque mise à jour, sans interpolation. Preuve : `Creatures/SharkActor.cpp:26-27`, `:252`.
- **R-35 [Mineur, supposé] Un cadavre figé tombe chez le client** : le client simule la physique pendant que le serveur fait descendre le corps sans physique. Preuve : `Creatures/TikiCorpse.cpp:88`, `:251-260`.
- **R-36 [Mineur, hors réseau] « LaunchCharacter ignoré après la mort » chez les soldats.** Le garde a sa raison : `HandleDeath` laisse un soldat tué en l'air en `MOVE_Falling` pour qu'il retombe, et les reculs d'armes appliqués après le coup mortel lanceraient sinon le cadavre. Il est appelé sur le serveur seul, et les clients suivent le mouvement répliqué : **aucun effet réseau**. Seule réserve : le lancer du crabe appelle `Fling` avant `HurtPrey`, ce qui annule l'amortissement ×0,3 du corps. Preuve : `Fort/FortSoldier.cpp:1482-1491`, `:1502-1509` ; `Creatures/GiantCrab.cpp:813-814`.

Armes :

- **R-42 [Mineur] Des balles d'une même image sont perdues par le moteur.** Le moteur jette au-delà de 2 appels d'un même multicast non fiable par mise à jour réseau (`net.MaxRPCPerNetUpdate` = 2, `DataReplication.cpp:38-42`). Or chaque tir envoie deux multicasts (impacts et traçantes), et la mitrailleuse peut tirer 3 balles dans une même image. La 3e traçante ou le 3e impact disparaissent, surtout quand l'hôte tombe sous 20 i/s. Preuve : `Weapons/FirearmWeapon.cpp:750-757` ; `Weapons/MachineGunWeapon.cpp:30`, `:376`. Correctif : regrouper les balles d'une image en un seul multicast. Le reste du modèle de la mitrailleuse est bon (voir « Ce qui tient »).
- **R-43 [Mineur] Débits accessoires.**
  - Le refroidissement de la mitrailleuse est répliqué environ 46 fois par seconde pendant environ 5 s après chaque rafale : `Weapons/MachineGunWeapon.cpp:295`, `:444-452`.
  - Les objets portés et les pickups n'ont ni fréquence ni dormance : 100 Hz par défaut.
  - Le mode de réplication physique des pickups n'est ni choisi ni testé : `Weapons/InventoryPickup.cpp:21-22`.
- **R-44 [Mineur] Prédiction sans rattrapage.**
  - Un tir prédit refusé par le serveur n'est jamais corrigé (flash, son et recul fantômes) : `Weapons/FirearmWeapon.cpp:341-347`.
  - L'endurance d'un coup de sabre refusé n'est pas rendue : `Weapons/CutlassWeapon.cpp:331` (relevé par l'audit 34, toujours ouvert).
  - Des vitesses sont changées hors de la prédiction du mouvement : `Weapons/MachineGunWeapon.cpp:136-151`, `Weapons/CutlassWeapon.cpp:809`, `:855`. Les corrections sont à mesurer.
- **R-45 [Mineur] Gestes rejoués à l'arrivée ou au retour de pertinence**, y compris un coup de feu fantôme du mousquet : `Weapons/CutlassWeapon.cpp:1025-1028`, `Weapons/TorchItem.cpp:185-188`, `Weapons/MusketWeapon.cpp:434-441`. Correctif : ajouter un instant serveur, comme le fait déjà `FFirearmAction`.
- **R-46 [Mineur] Le hit-stop d'un coup de l'hôte fige les créatures pour tout le monde.** `CustomTimeDilation` est appliqué sur les acteurs autoritaires : `Weapons/CutlassWeapon.cpp:681`.
- **R-47 [Mineur] Robustesse.**
  - Des minuteries lambda capturent `this` sans être annulées : accès possible à une torche détruite si le porteur se déconnecte pendant une brûlure (`Weapons/TorchItem.cpp:153`, `:310` ; utiliser `CreateWeakLambda`).
  - `SetRounds` n'est pas borné à la restauration d'une sauvegarde (SEC-05, à reprendre par T-269) : `Weapons/ShotgunWeapon.h:34`, `Weapons/PirateMusketWeapon.h:49`, `Weapons/GrenadeWeapon.h:40`, `Weapons/MachineGunWeapon.cpp:101`.
  - Des RPC fiables n'ont pas de limite de débit, mais elles sont bornées par les gestes (SEC-03).
  - Côté client, les règles du projectile de grenade viennent de la classe par défaut : `Weapons/GrenadeProjectile.h:33-35`.
  - `AMusketWeapon` n'est plus référencé (`DA_Item_Musket` pointe vers le mousquet pirate) : c'est du code mort qui garde la désynchronisation de visée de R-38.
- **R-48 [Mineur, choix à acter] Le hitscan n'a pas de compensation de latence** (`Weapons/FirearmWeapon.cpp:683-705`). À 150 ms, un client doit viser devant une cible mobile. C'est un choix de jeu à écrire, lié à R-25.

## Ce qui tient

Points relus et conformes (cahier §4, §5, §6.1, §9) :

- **Horloge et phases** : l'échéance du déluge est recalculée depuis le départ, donc sans dérive. Le bonus de zone est accordé une seule fois par couple (`ExpeditionId`, `ZoneId`), sur l'heure serveur lue sur place et jamais fournie par l'appelant ; l'égalité de temps revient au déluge. Les phases avancent par minuteries serveur, sans tick. Côté client, l'eau reste à 0 tant que la phase répliquée n'a pas atteint « Submerging ». Les comptes à rebours affichés (déluge, gouvernail, départ, cérémonie) dérivent tous de `GetServerWorldTimeSeconds()`. Références : `Expedition/Core/ExpeditionGameState.cpp:178-243`, `:262-300`.
- **Fin de jour et partie perdue** : décidées par le serveur seul, avec des temps répliqués (`TeamWipedTime`, `DayEndedTime`, `DayEndTravelWait`, `EarlyEndTime`, `bExecution`) envoyés dans le même paquet que la phase. Un joueur encore en chargement compte « en attente » : pas de partie perdue pendant le chargement d'un client. Un départ est recompté au tick suivant. Pendant le départ, chaque machine fait partir le galion localement (son mouvement n'est pas répliqué), comme prévu : personne ne joue plus.
- **Barque, sièges et rames** : le serveur attribue siège et rameur, et revérifie la portée (`SeatReach` + 100 cm), la mort et le ragdoll. Le RPC de rame est non fiable, limité à 10/s, avec un arrêt fiable au relâchement ; l'entrée est finie et bornée à [-1, 1], et le rameur est vérifié. La garde de siège (T-182) réconcilie chaque machine. Le composant cavalier est ajouté par le serveur et répliqué (`Boats/BoatSubsystem.cpp`, `EnsureRider`).
- **Boutique** : l'achat est jugé par le serveur : offre, portée `BuyReach`, trésorerie commune, place libre, délai par acheteur. Le paiement passe avant le don, avec remboursement en cas d'échec. Deux achats simultanés sont traités l'un après l'autre : pas de double dépense.
- **Portes** : le serveur fait autorité, et le client ouvre localement pour son propre pion, ce qui évite les corrections au pas. L'état est un tableau d'indices compact (1 Hz + `ForceNetUpdate`). Les maisons sont construites dans le même ordre sur chaque machine à partir du même asset, donc les indices de porte concordent.
- **Cérémonie** : le départ est revérifié par le serveur (zone, vivant, debout, à terre, pas assis). Immobilité, coup, mort et départ de l'officiant sont jugés par le serveur. L'état est complet et toujours pertinent (2 Hz + `ForceNetUpdate`), donc un joueur qui arrive en cours voit le drapeau levé. Le bonus de zone et la prime sont versés une fois.
- **Voix** : composant répliqué du PlayerController, RPC booléen avec seau à jetons. Le bruit de la voix est décidé par le serveur (furtivité). Un mort est rendu muet par `GameplayMutePlayer` (le serveur ne relaie plus sa voix), y compris pour un joueur qui arrive pendant qu'il est mort. Avis : **revue réseau OK pour T-263**, sous réserve d'un PIE à deux.
- **Partie (Run)** : l'état répliqué tient dans `URunStateComponent`, sous-objet du game state, écrit par le serveur seul. Les inventaires sont gardés par identifiant unique, stable d'un voyage à l'autre dans un même processus.
- **Galion** : mouvement non répliqué (cinématique, pose calculée sur l'horloge serveur), `Rules` envoyé une fois, toujours pertinent. Le marchand et la caisse sont créés par le serveur.
- **Caisse** : entrées, sorties, contenu et masse sont décidés par le serveur. L'état « rangé » est réappliqué au `OnRep`, car le moteur ne réplique pas la collision. La saisie de la caisse passe par le chemin de saisie revérifié.

- **Créatures et fort** : IA, coups, dégâts, apparitions et tirs des soldats sont serveur seul :
  - `ServerThink` est sous autorité ;
  - `OnTakeAnyDamage` n'est lié que sur le serveur ;
  - le Director n'est pas répliqué ;
  - les tactiques ne sont créées que sur le serveur ;
  - les soldats tirent après une ligne de vue tracée sur le serveur et un tirage de chance.

  Aucun RPC Server dans `Creatures/`, `Fort/` et `Stealth/` : aucun client n'y décide de dégâts. Pas d'effet en double chez l'hôte, car les OnRep y sont appelés explicitement. L'arrivée en cours est correcte pour la phase du crabe, les membres coupés, la cendre, les cadavres (pose envoyée une fois, `SettledBodies`) et les membres des soldats.
- **Cendre (`ATikiAshBurst`)** : la trajectoire est recalculée sur chaque machine à partir de valeurs envoyées une fois et de l'heure serveur, sans mouvement répliqué. L'explosion est un OnRep. Le schéma est bon.
- **Site du fort** : chaque machine charge la même instance de niveau sous le même nom, à partir de valeurs envoyées une fois : les soldats placés dans le niveau sont reconnus côté client. `FortSiteSubsystem`, la garnison et les douves sont serveur seul.
- **Furtivité** : `ReportNoise` est ignoré sur un client (`Stealth/StealthSubsystem.cpp:198`). Les entrées viennent d'états fiables côté serveur : accroupi tiré des drapeaux de mouvement, sprint compressé, nuit de l'horloge répliquée. Aucun constat.

- **Inventaire** : le serveur décide. Toutes les requêtes passent par des RPC Server du pion possédé, avec des indices bornés côté serveur. Les limites de débit sont en place : écran (seau à jetons, 20/s), lâcher (0,25 s), achat. Un joueur qui arrive en cours retrouve l'état correct par la réplication de `Slots`, du propriétaire, de `bEquipped` et de l'attache.
- **Ramassage** : le verrou est bon, une seule prise est accordée. `Claim` pose `bClaimed` puis détruit le pickup dans la même image, et une seconde requête est refusée. Les munitions sont prises par lots partiels, l'un après l'autre. Il n'y a pas de duplication entre l'inventaire et le monde. `AInventoryPickup` revérifie la portée et la ligne de vue sur la boîte de la cible.
- **Armes à feu** : le client n'envoie que « tirer » (un RPC par clic). Le serveur décide des munitions, de la cadence, de l'origine (caméra du porteur), de la direction (rotation de contrôle), de la dispersion (tirage serveur), des cibles et des dégâts. **Aucun client ne choisit ses dégâts.** Le geste répliqué est rejoué depuis l'heure serveur, et le tir prédit chez le porteur n'est pas joué en double.
- **Mitrailleuse** : le modèle est bon. Il n'y a que 2 RPC fiables par rafale (appui, relâché). Le serveur tient gâchette, manivelle, bande et chaleur, et tire chaque balle dans son tick. L'état répliqué est compact et la réserve n'est envoyée qu'au propriétaire. Ordre de grandeur : 0,4 à 0,8 Ko/s par mitrailleuse qui tire et par client, soit environ 2 à 3 Ko/s par client avec 4 mitrailleuses.
- **Traçantes** : ce sont des acteurs locaux, non répliqués, lancés par le multicast non fiable depuis la bouche visible, sans coût propre. Une traçante perdue est acceptable (sauf la limite moteur de R-42).
- **Sabre, grenades et torche** : coups, souffle, KO et brûlure sont décidés par le serveur. Le coup de sabre est prédit puis validé par le serveur (fenêtres, endurance, combo), qui envoie un refus explicite. La grenade est un projectile répliqué avec correction de vitesse.

## À vérifier en PIE hôte + client

Réglages communs à tous les essais :

| Élément | Valeur |
|---|---|
| Mode | `Play As Listen Server`, 2 joueurs (puis 4 pour les débits) |
| Profils | PktLag=0, puis 100, puis 200 ; PktLoss=1 puis 3 |
| Commandes | `p.NetShowCorrections 1`, `stat net`, `net.Profile` sur l'hôte |
| Journaux | `LogNet`, `Logmonke_island`, `LogMonkeBoat` |
| Rapport | scénario, nombre de clients, profil, résultat par client, extrait de journal |

Barque, galion, partie, monde :

1. **Gouvernail (R-01)** : le client à la barre, l'hôte à 200 m. Aujourd'hui, l'hôte meurt 10 s plus tard. Après correctif : vote ou délai long, et alerte chez tous.
2. **Arrivée sur une île (R-02)** : deux joueurs en `-game`, client ralenti (autre machine, ou charge disque). Relever l'heure de départ de l'horloge chez l'hôte et le client, la place de chacun (pont ou barque), et la scène d'arrivée.
3. **Fin de jour chez un client sans fichier `Run` local (R-03)** : écran vu par le client (attendu : départ et « JOUR N TERMINÉ », pas « ÎLE CONQUISE »).
4. **Barque (R-05, R-09, R-12)** : client rameur et hôte debout dans la barque, puis l'inverse. Lag 100/200 et perte 3 %. Compter les corrections, les à-coups des joueurs debout, et la cargaison arrimée (12 objets) vue par le client. Mesurer les octets par seconde. Faire couler la barque.
5. **Pont du galion (R-04)** : 20 objets et la caisse posés sur le pont. Mesurer avec `stat net` et `net.Profile`, puis deux joueurs debout pendant deux minutes : compter les corrections.
6. **Portes (R-08)** : le client sprinte vers une porte à 0, 100 et 200 ms. Une correction est-elle visible au seuil ?
7. **Cérémonie (R-10)** : le client appuie sur F en fin de freinage à 100 ms. Le rituel est-il interrompu en « Moved » ? Faire de même avec les villageois qui se rassemblent.
8. **Caisse et valeurs (R-07, R-18)** : objets d'une zone dont le multiplicateur dépasse 1. Les ranger, s'éloigner plus de 10 s, revenir, vider la caisse. La valeur affichée chez le client doit égaler celle du serveur.
9. **Saisie (R-06, R-16)** : porter un objet en tournant vite à 100 ms, puis le lancer. Mesurer le retard visible et la direction du lancer.
10. **Voix (T-263)** : un joueur meurt (il n'est plus entendu), un troisième rejoint pendant qu'il est mort, appuyer pour parler et activation vocale à 3 % de perte.
11. **Inondation (R-20)** : deux joueurs au front d'eau pendant la submersion. Comparer nage et marche chez le client et chez le serveur.

Créatures et fort :

12. **KO (R-22, R-23)** : le client est saisi puis lancé par le crabe (`monke.CrabEmerge`), et lancé par le Gardien Crabe. Relever l'atterrissage chez le client et chez l'hôte, le rappel à la pince et sa durée, la téléportation finale, et les coups reçus au point de départ pendant le KO.
13. **Crabe (R-24, R-25)** : le client combat au sabre contre les pattes, puis se met sur la trajectoire de la charge : compter les corrections. Il tire sur l'œil et sur le coude coincé à 100 puis 200 ms : comparer avec la zone notée par le serveur (`shot on the <zone>`).
14. **Morts hors pertinence (R-26, R-32)** : tuer un soldat ajouté par `AdjustGarrison`, s'éloigner à plus de 150 m, revenir et traverser sa position. Bloque-t-il ? Rejoue-t-il sa mort ? Même essai avec le crabe mort.
15. **Télégraphes (R-27)** : à 3 % de perte, compter les coups de tiki reçus sans préparation visible.
16. **Cendre (R-30)** : seul le joueur visé est voilé.
17. **Émergence (R-31)** : le crabe sort à moins de 5 m du client. Le client est-il bloqué ?
18. **Fort** : le client voit les soldats placés et ajoutés, sans soldat fantôme (soldats en surnombre détruits avant le chargement du niveau chez le client).
19. **Débit (R-28)** : `stat net` sur l'hôte avec 3 clients, en village de nuit, avec fort et cadavres. Relever les octets sortants par connexion et le temps de réplication de l'hôte.

Armes :

20. **Liaisons d'entrée (R-37)** : l'hôte lâche un pistolet, le client le ramasse et l'équipe, puis l'hôte tire, vise et recharge avec un mousquet. Refaire dans l'autre sens, puis avec le sabre et la torche. Attendu après correctif : aucun effet sur l'arme de l'autre, et rien dans LogNet.
21. **Visée (R-38)** : le client tient le clic droit, vide le fusil à pompe puis tire. Comparer son FOV, la mention serveur « (aimed) » et ce que voit l'hôte.
22. **Mitrailleuse (R-42, R-43)** à 2 puis 4 joueurs, lag 100/200 et perte 1 à 3 % :
    - balles serveur comparées aux traçantes reçues ;
    - balles en trop après le relâché ;
    - bande et surchauffe identiques sur les deux machines ;
    - octets/s par client (Networking Insights) ;
    - « Too many calls » dans `LogRep` (Verbose) quand l'hôte perd des images.
23. **Prise simultanée** : deux joueurs appuient sur F en même temps sur un même pickup d'arme, puis sur un même paquet de munitions, à 0 puis 200 ms.
24. **Valeur des armes (R-39)** : lance abîmée, F puis G, plusieurs fois.
25. **Torches (R-40)** : de nuit, 2 à 4 torches allumées. Mesurer le débit, puis faire arriver un joueur pendant qu'une torche brûle.
26. **Retour de pertinence** : partir à plus de 150 m puis revenir. Vérifier arme équipée, visée, mitrailleuse en tir, torche, sabre fantôme et pickups déjà pris.
27. **Fin de jour (T-088)** :
    - munitions gardées par l'hôte et par le client ;
    - kit de base pour un joueur mort ;
    - déconnexion puis reconnexion, sans duplication (R-41).
28. **Hit-stop (R-46)** : un coup de l'hôte fige-t-il les tiki chez le client ?

## Préparation de T-088 et T-172

**T-088, voyage d'île en île à deux.** La chaîne relue :

1. `EndDay` ;
2. attente de `DayEndTravelWait` (`DayEndTravelDelay`, plus la cupidité) ;
3. `RestartFromBoot` : `StoreLoadouts`, `SaveRun`, `ServerTravel` absolu vers `L_Boot?listen?MaxPlayers=4` ;
4. `L_Boot` attend les joueurs attendus (grâce et délai maximal) ;
5. `ServerTravel` vers l'île `?Island=` ;
6. au BeginPlay de l'île : `BeginDay`, puis l'horloge.

Le schéma est sain : le serveur décide du voyage, la partie est gardée sur la GameInstance de l'hôte, les inventaires sont indexés par identifiant unique (stable dans un même processus) et `MaxPlayers` est conservé. Avant la recette à deux :

- **R-02** : barrière « tous chargés » avant l'horloge, et place sur le pont pour les retardataires.
- **R-03** : un client ne lit jamais sa propre partie, sans quoi il voit un écran de fin à chaque jour.
- **R-14** : nouvel essai ou repli si le voyage échoue, sinon tout le monde reste figé, caché et sans commandes.
- **R-22** (et R-23) : un joueur relevé ailleurs sur le serveur peut mourir ou rester « à terre » au moment du déluge.
- **À noter** : avec l'OSS Null, l'identifiant unique est aléatoire en PIE et pour une seconde instance sur la même machine (hôte et client sur le même PC). Les inventaires survivent au voyage, pas à une nouvelle session : à tester sur deux machines.

**T-172, lobby à bord.** Rien n'existe encore côté réseau. À prévoir dès la conception :

- **PlayerState du projet** : aucun n'existe. Il portera `bReady`, `bAboard` et la couleur ou le nom (T-028), avec `CopyProperties` si le voyage devient seamless.
- **Requêtes** : RPC Server sur le PlayerController (`ServerSetReady`, `ServerVoteDepart`), avec délai minimal et vérification de la phase et de l'état.
- **Compte à rebours** : échéance en temps serveur répliquée dans le game state, comme `EarlyEndTime`, et une interface qui ne fait que la lire.
- **Refus en expédition** : un joueur qui rejoint pendant une expédition est aujourd'hui accepté sans règle. Il faut un `PreLogin` qui refuse avec un message clair dès que la phase atteint « Exploring », ou une admission en spectateur (RUN02, RUN03).
- **Brique de vote commune** : la même brique sert au départ (R-01), à « PRÊTS POUR LE DÉPART » et à la cupidité (R-15).

## Tickets créés

Une ligne `a-faire` par correctif bloquant ou majeur (`docs/production/board.md`). Les constats qui partagent un correctif partagent un ticket.

| Ticket | Constats | Rôle | Sévérité |
|---|---|---|---|
| T-286 | R-01 gouvernail : vote de départ | ue-network-engineer | Bloquant |
| T-287 | R-22 KO et lancer : relevé serveur, point répliqué | ue-physics-animator | Bloquant |
| T-288 | R-37 liaisons d'entrée retirées au changement de porteur | ue-gameplay-programmer | Bloquant |
| T-292 | R-02 barrière « tous chargés », retardataires sur le pont | ue-network-engineer | Majeur |
| T-293 | R-03 le client lit la partie répliquée | ue-systems-programmer | Majeur |
| T-294 | R-04 roulis visuel, pont au repos | ue-network-engineer | Majeur |
| T-295 | R-05 barque (et R-09, R-12) | ue-boat-programmer | Majeur |
| T-299 | R-06, **doublon partiel de T-289 et T-290** (T-269, créés quelques secondes avant). Seul ajout : `ServerGrab`, à traiter dans T-289 | ue-gameplay-programmer | Majeur |
| T-300 | R-07 et R-26 : BeginPlay client qui écrase `Value` et `Health`, mort silencieuse | ue-gameplay-programmer | Majeur |
| T-301 | R-08 portes anticipées selon la vitesse | ue-gameplay-programmer | Majeur |
| T-304 | R-23 et R-33 : saisie du crabe attachée, libération propre | ue-gameplay-programmer | Majeur |
| T-305 | R-24 et R-25 : clips du crabe sur l'heure serveur, corps de frappe, points faibles | ue-gameplay-programmer | Majeur |
| T-306 | R-27 télégraphes des tiki en état répliqué | ue-ai-programmer | Majeur |
| T-312 | R-28, R-43 et le débit de R-04 : fréquences et dormance, mesure à 4 joueurs | ue-network-engineer | Majeur |
| T-308 | R-38 visée resynchronisée | ue-gameplay-programmer | Majeur |
| T-309 | R-39 valeur courante portée par l'objet | ue-systems-programmer | Majeur |
| T-310 | R-40 carburant de la torche calculé | ue-gameplay-programmer | Majeur |
| T-311 | R-41 `FRunLoadout` consommé, enregistré au départ | ue-systems-programmer | Majeur |

Note de chantier : une première T-307 (fréquences et dormance) a été écrasée par une écriture concurrente de T-269. Elle a été recréée en T-312, et le journal garde les deux créations. Les mineurs n'ont pas de ticket : ils sont à reprendre dans les tickets voisins (par exemple R-09 et R-12 dans T-295) ou au tri.

## Avis par tâche relue

Avis de revue réseau pour les tâches dont le code a été relu. Les notes de ces tâches n'ont pas été modifiées : `chantier.py set` remplacerait leur note. À reporter par le lead.

| Tâche | Avis réseau |
|---|---|
| T-039 barque | écarts R-05, R-09, R-12, R-19 (T-295) : revue ADR-003 non signée, jamais testée à deux |
| T-228 gouvernail | écart bloquant R-01 (T-286) |
| T-142 fin du déluge | logique serveur conforme ; dépend de R-02 (joueur en chargement) et de R-04 (position sur le pont) |
| T-171 et T-214 partie, boucle des jours | écarts R-03 (T-293), R-41 (T-311) ; mineurs R-13, R-14, R-15 |
| T-186 portes du village | écart R-08 (T-301), à confirmer en PIE |
| T-194 et T-218 cérémonie | conforme, sauf le mineur R-10 |
| T-177 boutique | conforme |
| T-258 caisse de transport | écarts R-06 (T-289, T-290, T-299) et R-07 (T-300) ; mineur R-18 |
| T-263 voix | **revue-reseau=OK**, sous réserve de l'essai PIE n°10 |
| T-157 et T-161 ramassage, armes de mob | verrou de prise conforme ; écarts R-37 (T-288), R-39 (T-309) |
| T-273 mitrailleuse | modèle réseau conforme ; mineurs R-42, R-43 ; dépend de R-37 (T-288) |
| T-276 traçantes | conforme (acteur local, sans coût propre) ; mineur R-42 |
| T-243, T-260, T-261 crabe géant | écarts R-22 à R-25 (T-287, T-304, T-305) ; mineurs R-31 à R-33 |
| T-275 tactiques du Gardien Crabe, du Veilleur, du Guerrier Coco | écarts R-22 (lancer du Gardien) et R-27 (T-306) ; mineurs R-29, R-30 |
| T-213, T-231 soldats et forts | conforme côté autorité ; écart R-26 (T-300) ; mineurs R-32, R-36 |
