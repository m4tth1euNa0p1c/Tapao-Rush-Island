# 40 — Triche et sécurité
Domaine : D · Auditeur : ingénieur sécurité (anti-triche)

## Verdict
Armes, poings, rames et options d'URL sont bien gardés côté serveur, et les commandes `monke.*` sont toutes sous `#if !UE_BUILD_SHIPPING` (lu, rien reproduit). Mais `security.md` n'a jamais été appliquée (zéro `_Validate`, zéro limite de débit, aucune revue SEC) ; les vrais trous sont le vol laissé actif, des contrôles posés sur des valeurs du client et un solo qui ouvre un serveur d'écoute. Aucun paquet n'a été testé : les garanties Shipping restent théoriques.

## Constats

- **[Majeur] Portée contrôlée sur le point du client, pas sur la cible**
  - Constat : `ServerInteract` et `ServerGrab` mesurent la distance à `Point`, fourni par le client, et ne refusent que si le tir touche un autre acteur : un point dans le vide passe. `AConsumablePickupActor::CanInteract` ne teste aucune distance : un client modifié consomme un soin lointain. Seul le grab est relâché au-delà de 4 m.
  - Preuve : IslanderGrabComponent.cpp:237-247, 288-298, 526 ; ConsumablePickupActor.cpp:62-74 ; lu.
  - Recommandation : distance à la boîte englobante de la cible, tir qui doit la toucher, délai minimal par joueur, test de refus. Palan, barre et Balance passeront par là.
  - Effort : S

- **[Majeur] Vol (double Espace) actif pour tous, accepté par le serveur**
  - Constat : outil de dev T-021 resté en jeu ; le drapeau client `FLAG_Custom_2` est appliqué sans condition : on survole la mer et on contourne la barque, cœur de la boucle (le 13 l'a vu côté onboarding).
  - Preuve : IslanderCharacter.cpp:465-469 ; IslanderMovementComponent.cpp:178, 462-466 ; player-movement-lc.md:17.
  - Recommandation : `IsFlightAllowed()` (DataAsset, faux par défaut, `#if !UE_BUILD_SHIPPING`) testé dans `UpdateCharacterStateBeforeMovement`, donc aussi côté serveur.
  - Effort : S

- **[Majeur] Norme écrite, jamais appliquée**
  - Constat : 21 RPC `Server`, aucun `WithValidation` ; les nombres sont bornés dans les corps (`SetRowInput`), donc rien d'exploitable aujourd'hui. Mais ni limite de débit (SEC-03), ni `threat-model.md`, ni tâche au chantier, ni test de refus. Le moteur laisse aussi `bMovementTimeDiscrepancyDetection` à faux : l'accélération par horodatage reste possible (supposé).
  - Preuve : grep `_Validate` = 0 ; BoatActor.cpp:291-298 ; board.md (T-032 seule) ; Engine BaseGame.ini:40-41.
  - Recommandation : `/ue-security-review code` et `threat-model` avant le L5 ; `_Validate` sur les RPC à flottants ; compteur de débit commun ; activer détection et résolution de dérive (`GameNetworkManager`).
  - Effort : M

- **[Majeur] Le solo ouvre un serveur d'écoute, 16 places, sans contrôle d'accès**
  - Constat : JOUER ajoute `?listen` même seul : invite de pare-feu au premier lancement, UDP ouvert au réseau local. Ni `PreLogin`, ni `MaxPlayers` (16 par défaut), ni expulsion.
  - Preuve : TapaoUISkin.h:121 (DA non surchargé) ; TapaoUISubsystem.cpp:644 ; Engine BaseGame.ini:68.
  - Recommandation : solo sans `listen` (déjà prévu par l'ADR-005) ; `MaxPlayers=4` ; `PreLogin` fermé hors lobby ; expulsion par l'hôte.
  - Effort : S à M

- **[Majeur] Sauvegarde sans intégrité ; SG_Run reprendrait le défaut**
  - Constat : `UTapaoSaveGame` est un `.sav` binaire : `Version` jamais relue, `Gold` non borné, aucune somme, `SaveSlot` lu dans `Game.ini` (SEC-04). `SG_Run` (trésorerie, cale) : l'ADR-005 prévoit l'écriture atomique, sans somme (`security.md` l'exige) ; éditer le fichier gonfle la trésorerie.
  - Preuve : TapaoSaveGame.h:22 ; TapaoUISubsystem.h:67, .cpp:658, 690 ; ADR-005 §10 ; security.md:30.
  - Recommandation : en-tête (version, CRC32) et bornes ; stocker `ItemId` et durabilité, pas la valeur, recalculée et plafonnée à `BaseValue` ; au chargement, `Treasury` doit égaler le chaînage de `ResolvedRounds` (RunTypes.h:203-225). Édition par l'hôte acceptée.
  - Effort : M

- **[Mineur] Hôte tricheur accepté, mais incompatible avec classements et seeds partagés**
  - Constat : l'hôte choisit île et graine par `?Island=` et `?Seed=`, Shipping compris ; les options sont assainies (alphanumérique et `_`, 64 caractères, test d'injection). Le 19 propose classements et défis hebdomadaires.
  - Preuve : BoatStartSubsystem.cpp:184-197 ; FragmentsSpec.cpp:230 ; security.md:36.
  - Recommandation : coop privé « non classé », aucun score tiré de valeurs de l'hôte, sinon serveur dédié (L8+).
  - Effort : S

- **[Mineur] Hygiène de paquet non vérifiée**
  - Constat : `ModelContextProtocol` (modules Runtime, `NoRedist: true`) n'a pas de `TargetAllowList` Editor : il part dans le jeu, et `-ModelContextProtocolStartServer` y ouvrirait 127.0.0.1:8000 (l'autostart par ini n'existe que dans l'éditeur). `ue.ps1 package` accepte `Development` (console et `monke.*` chez les testeurs).
  - Preuve : monke_island.uproject:34-37 ; ModelContextProtocol.uplugin:8, 20-29 ; Engine ConfigCacheIni.cpp:6648 ; ue.ps1:22, 138-143.
  - Recommandation : `TargetAllowList` Editor ; script de gate refusant un paquet hors Shipping ou contenant le plugin.
  - Effort : S

## Manques
1. Expulsion, bannissement et silence par l'hôte.
2. Signature Authenticode de l'exe : Smart App Control a bloqué les DLL non signées le 2026-10-03 (mémoire `smart-app-control-dll-block`) ; risque identique chez un joueur (supposé).
3. SteamSockets (IP masquées) avant tout lobby avec des inconnus.

## Quick wins
1. `DefaultGame.ini` : `MaxPlayers=4` et les deux drapeaux de dérive.
2. Vol interdit en Shipping.
3. `PlayOptions` vide en solo.
4. `TargetAllowList: ["Editor"]` pour `ModelContextProtocol`.
5. Distance à la cible dans `ServerInteract` et `CanInteract` des soins.

## Questions qui challengent l'équipe
1. Voulez-vous classements, succès Steam ou seeds partagés ? Quelle donnée croyez-vous venir d'un hôte non vérifié ?
2. Le vol est-il une fonction du jeu final ? Sinon, pourquoi existe-t-il alors que la barque est la boucle ?
3. Qui expulse un grief en lobby public, et qui répond quand un streamer voit son IP exposée ?
