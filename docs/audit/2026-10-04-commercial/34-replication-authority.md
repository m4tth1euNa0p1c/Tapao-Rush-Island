# 34 — Autorité serveur et réplication
Domaine : D · Auditeur : ingénieur réseau UE (réplication)

## Verdict
Le noyau d'autorité est sain : dégâts, munitions, rame, sièges, IA et valeurs sont décidés par le serveur, et aucun des 30 RPC (22 fiables) ne part à chaque image. Mais barque, galion, ragdoll et butin sont simulés séparément par chaque machine, sans recette hôte + client. Risque commercial : une coop qui rubber-bande ou diverge dès la première partie à deux.

## Constats

- **[Bloquant commercial] Bases mobiles simulées séparément sous des joueurs en CharacterMovement**
  - Constat : la coque simule « avec les mêmes forces » sur chaque machine et le serveur corrige ; rivage et retenue de cargaison ne tournent que sur le serveur, qui téléporte la cargaison arrimée à chaque image. Le galion, sans mouvement répliqué, est recalculé par chaque machine (eau, crue, balancement).
  - Preuve : BoatActor.h:24, BoatActor.cpp:401-409, 477 ; GalleonActor.cpp:32, 117-151 ; ADR-003:25 (revue réseau prévue, aucune trace au chantier). Effet supposé.
  - Recommandation : PIE à 2 avec `Net PktLag=120`, debout sur barque et pont ; forces de coque côté serveur seul, clients interpolés ; cargaison attachée à la coque.
  - Effort : M

- **[Majeur] La valeur d'un objet est écrasée au BeginPlay**
  - Constat : `Value = BaseValue` et `Health = MaxHealth` s'exécutent aussi chez le client, après l'état initial (moteur : BeginPlay suit les propriétés). Le butin n'étant pertinent qu'à 80 m, chaque objet est recréé à l'approche : un objet abîmé s'affiche au prix neuf. Sans setter, le manifeste de cale d'ADR-005 ne pourra pas restituer la valeur courante.
  - Preuve : ValuableComponent.cpp:27 ; IslanderHealthComponent.cpp:19 ; IslanderHUD.cpp:137 ; moteur DataChannel.cpp:3528.
  - Recommandation : initialiser sur autorité seule ; `InitFromManifest(ItemId, Valeur)`.
  - Effort : S

- **[Majeur] Ragdoll des joueurs non autoritaire**
  - Constat : tumble et mort sont un multicast fiable unique ; chaque machine simule son ragdoll et décide seule du relevé, la capsule serveur reste sur place, `bIsRagdoll` n'est pas répliqué : positions divergentes, mort invisible pour les arrivants tardifs.
  - Preuve : IslanderCharacter.h:287-291, 318 ; .cpp:401, 539, 641.
  - Recommandation : état répliqué (OnRep), capsule suivant le bassin sur le serveur, relevé décidé par lui.
  - Effort : M

- **[Majeur] Départ de manche sans barrière « tous prêts »**
  - Constat : par défaut l'horloge démarre au BeginPlay serveur (BP non lu) ; le pont ne place que les joueurs connectés à cet instant ; L_Boot n'attend que la connexion (3 s, 15 s max), pas le chargement de 110 Mo.
  - Preuve : ExpeditionGameState.h:46, .cpp:67 ; GalleonSpawner.cpp:136-147 ; ExpeditionBootGameMode.h:44, 48.
  - Recommandation : `bStartOnBeginPlay=false`, démarrer quand chaque contrôleur a un pion chargé, avec délai maximal.
  - Effort : S

- **[Majeur] Coût de réplication du butin, des tiki et des cadavres**
  - Constat : 4 110 props (5 000 max), 36–54 villageois et 80 cadavres sont répliqués sans dormance ni fréquence réglée dans le C++ (défaut 100 Hz) ; seule la noix est `DORM_Initial`.
  - Preuve : PhysicsPropActor.cpp:12 ; ConsumablePickupActor.cpp:19 ; T-108 (~40 i/s en PIE solo, avant réseau).
  - Recommandation : 20–30 Hz et fréquence adaptative, dormance au repos, mesure `stat net` à 4 joueurs.
  - Effort : M

- **[Majeur] Portée et ligne de vue validées sur un point fourni par le client**
  - Constat : `ServerGrab` et `ServerInteract` mesurent la distance à `GrabPoint`/`Point`, pas à la cible, et acceptent un tracé qui ne touche rien : un client modifié saisit un objet à travers un mur ou boit une noix hors de portée. Zéro `WithValidation`, aucun délai minimal.
  - Preuve : IslanderGrabComponent.cpp:237-247, 288-298 ; grep `_Validate` : 0.
  - Recommandation : distance à la boîte de la cible, tracé qui doit la toucher, `_Validate`, délai par joueur.
  - Effort : S

- **[Majeur] Endurance hors réplication et hors prédiction**
  - Constat : elle tourne séparément chez le serveur et le propriétaire ; seul le serveur tue (noyade immédiate à 0), le HUD montre la valeur locale, et un coup refusé n'est jamais remboursé.
  - Preuve : IslanderMovementComponent.cpp:181-217, .h:120 ; CutlassWeapon.cpp:353.
  - Recommandation : répliquer au propriétaire avec correction périodique, ou l'intégrer au mouvement prédit.
  - Effort : M

## Manques
- Aucun test réseau (PIE à 2 avec latence, spec Automation) ni budget mesuré.
- Pacification et vente sans modèle serveur : `AddZoneBonus` sans appelant, pas d'`ItemId` stable.
- Villages construits par chaque client (VillageSpawner.cpp:117-124), sans contrôle de cohérence.
- Télégraphes des tiki en multicast non fiable (TikiCreature.h:381) ; eau embarquée répliquée à chaque image tant qu'elle varie (BoatActor.cpp:659-683).
- Choc des valeurs lu sur la vitesse post-solveur, non l'impulsion (ValuableComponent.cpp:57) : perte sous-estimée, à tester (supposé).
- Pas de reprise sur reconnexion.

## Quick wins
1. Garde d'autorité sur `Value = BaseValue` et `Health = MaxHealth`.
2. Distance à la cible et tracé obligatoire dans `ServerGrab`/`ServerInteract`.
3. `bStartOnBeginPlay=false` + barrière « tous prêts ».
4. `SetNetUpdateFrequency` 20–30 Hz sur props et tiki, `net.UseAdaptiveNetUpdateFrequency 1`.
5. Session PIE à 2 avec `Net PktLag` : siège, pont, tumble, saisie, échelle.

## Questions qui challengent l'équipe
1. Quand jouez-vous à deux machines ? 296 tests verts ne couvrent pas votre mode principal.
2. Si la barque physique ne tient pas à 150 ms, la rendrez-vous cinématique, comme le galion ?
3. Quel budget (ms par image, Ko/s) pour l'hôte à 4 joueurs, et qui le mesure ?
