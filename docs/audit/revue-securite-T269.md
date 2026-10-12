# Audit sécurité — RPC d'achat et d'inventaire, sauvegardes, commandes de dev, dépendances — 2026-10-10

- **Domaine** : sécurité
- **Norme** : `docs/conventions/security.md` (SEC-01 à SEC-10), STRIDE ; CWE cités au cas par cas
- **Agent** : `ue-security-engineer`
- **Tâche** : T-269, porte REC (lots L1 à L6 ; L7 pour les sessions et la voix)
- **Périmètre** : `Source/monke_island/` (36 RPC `Server`, Shop, Weapons, Run, UI/Tapao, Voice, Death, Props/LootCrate, Interaction, Player), `URunSaveGame` et `UTapaoSaveGame`, commandes et cvars `monke.*`, vol au double Espace, `Config/*.ini`, `monke_island.uproject`, `.mcp.json`, `scripts/` (12 scripts réseau), recherche de secrets sur les fichiers suivis hors `Content/`, plus les fichiers non suivis non ignorés.
- **État relu** : HEAD `2cf9598` plus l'arbre de travail du 2026-10-10 vers 23 h 45. `Run/`, `Shop/`, `Death/`, `Voice/`, `Props/LootCrate/` et une partie de `UI/Tapao/` ne sont pas encore suivis par Git. `TapaoUISubsystem.cpp/.h` ont été modifiés par une autre tâche pendant la revue. Les lignes citées ici sont celles de 23 h 45. Les notes de T-297, T-302 et T-303 citent des lignes de `TapaoUISubsystem.cpp` antérieures : se fier au nom des fonctions.
- **Méthode** :
  - lecture du code ;
  - recherches `rg` (RPC, `FAutoConsole*`, `UE_BUILD_SHIPPING`, E/S fichiers, `GetPlayerName`, motifs de secrets) et un script d'inventaire des gardes `#if` ;
  - lecture des sources du moteur UE 5.8.3 installé, pour les comportements cités : `ServerExec`, `EnableCheats`, `LoadGameFromMemory`, OSS Null, `FName` ;
  - recherche de chaînes dans les `.uasset` : `MaxStack`, `ExecuteConsoleCommand`, `IslanderInteractable`.
- **Niveau de preuve** : **lu**. Rien n'a été compilé, lancé ni testé : l'éditeur du propriétaire était ouvert, et la revue était en lecture seule par consigne. Aucun service externe n'a été interrogé.

## Synthèse
| Sévérité | Nombre | Corrigés |
|----------|--------|----------|
| CRITIQUE | 1 | 0 |
| MAJEUR | 8 | 0 |
| MINEUR | 14 | 0 |

**Verdict : non conforme** (1 CRITIQUE et 8 MAJEUR ouverts). `revue-securite=écarts` : T-289, T-290, T-291, T-296, T-297, T-298, T-302, T-303, T-307.

- **Ce qui tient** :
  - la boutique (offre, portée, trésorerie et cadence décidées par le serveur) ;
  - l'écran d'inventaire (seau de jetons, règles pures sans duplication) ;
  - le ramassage (portée et vue depuis les positions serveur) ;
  - les armes (munitions et gestes serveur) ;
  - la rame, le spectateur et la voix (bornes, `_Validate`, débit) ;
  - les paramètres (assainis) ;
  - les plugins MCP réservés à l'éditeur.
- **Ce qui ne tient pas** :
  - la portée des interactions se mesure sur un point fourni par le client (C1) ;
  - plusieurs RPC répétables n'ont aucune limite de débit (M1) ;
  - le vol de dev reste actif en Shipping (M2) ;
  - les builds diffusés sont en Development, où tout client peut prendre la console de l'hôte (M3, M5) ;
  - une partie solo s'ouvre au réseau local après l'écran de fin (M4) ;
  - les sauvegardes sont désérialisées sans contrôle (M6) ;
  - des pseudos et une IP vont dans les logs (M7) ;
  - deux dépendances d'outillage ne sont ni épinglées ni licenciées (M8).
- **Bloquant** :
  - C1 avant tout test à plusieurs machines hors des PC du propriétaire ;
  - M3, M4 et M5 avant toute diffusion d'un build.

## 1. Modèle de menace (STRIDE par système)
Contexte de la norme (§1) : l'hôte fait autorité et peut tricher (risque accepté) ; sauvegardes locales ; pas de backend ; sessions privées en LAN (OSS Null) avant le L7.

| Système | Usurpation (S) | Falsification (T) | Répudiation (R) | Divulgation (I) | Déni de service (D) | Élévation (E) |
|---|---|---|---|---|---|---|
| Interaction : saisie, F, caisse | RPC sur le composant possédé, le pion vient du serveur (OK) | **portée crue sur le point du client (C1)** | logs serveur des saisies et rangements (OK) | — | **pas de débit (M1)** | — |
| Inventaire, boutique, trésorerie | propriétaire = `GetOwner()` (OK) | offre, prix et trésorerie serveur ; pas de duplication (OK) | log des achats avec trésorerie (OK) | `Slots` répliqué à tous (m11) | écran 20/s, achat 0,3 s (OK) ; `ServerSelectSlot` sans débit (M1) ; pile lâchée = N acteurs (m8, latent) | — |
| Armes et combat | porteur vérifié par `IsUsable` (OK) | dégâts, munitions et cadence serveur (OK) | — | — | gestes serveur (OK) | — |
| Mouvement | — | **vol accepté du client (M2)** ; détection de dérive d'horloge éteinte (m13) | — | — | — | vol = triche de dev en jeu (M2) |
| Run et sauvegardes | **clé de loadout = identifiant déclaré par le client en OSS Null (m7)** | sauvegarde éditable (accepté pour l'hôte) ; **aucune version ni borne (M6)** | — | **identifiants de machine dans `Run.sav` (M7)** | **fichier aléatoire = crash à chaque lancement (M6, lu)** | classe chargée d'après le fichier (M6) |
| Mort et spectateur | contrôleur possédé (OK) | `_Validate` ±1, mort revérifiée (OK) | — | `COND_OwnerOnly` (OK) | `MinCycleInterval` (OK) | pas de réapparition par `ServerRestartPlayer` : le pion reste possédé (OK) |
| Voix (T-263, LAN) | expéditeur non vérifié par le moteur (documenté, accepté en LAN) | « je parle » borné (OK) | — | voix relayée à tous, proximité seulement côté client (m10) | seau de jetons (OK) | — |
| Session et connexion | **pseudo par défaut = nom de machine (M7, T-297)** ; `ChangeName` sans borne (m6) | — | — | IP de l'hôte dans le log du client (M7) | **solo ouvert à 16 places après l'écran de fin (M4)** | **Development : `serverexec EnableCheats` (M3)** |
| Commandes de dev et builds | — | — | — | `.pdb` dans le package (M5) | `ServerPause` des clients en Development (M3) | console, `monke.*` et vol en Development diffusé (M3, M5) ; 9 commandes sans garde Shipping (m1) |
| Chaîne d'outils | — | scripts de téléchargement sans empreintes (m9) | — | chemins utilisateur dans 36 fichiers suivis (m12) | — | `uvx blender-mcp` non épinglé (M8) ; addon tiers (M8) |

## 2. Constats
| ID | Contrôle | Sévérité | Emplacement | Constat (preuve) | Correction | Statut |
|----|----------|----------|-------------|------------------|------------|--------|
| C1 | SEC-02 (CWE-20) | CRITIQUE | `Interaction/IslanderGrabComponent.cpp:329-359`, `:418-468` ; `Items/ConsumablePickupActor.cpp:62-91` ; `Weapons/AmmoPickup.cpp:203-225` ; `Props/LootCrate/LootCrateActor.cpp:183-213`, `:435-450` | La portée de `ServerInteract` et `ServerStoreHeld` se mesure sur le `Point` envoyé par le client (`:347`, `:438`). La ligne de vue ne refuse que si elle touche **un autre** acteur : un point dans le vide près des yeux passe. Les soins, les munitions et la caisse (vider, ranger) n'ont aucune portée propre. | Mesurer la distance des yeux à la boîte englobante de la cible et la ligne de vue vers la cible, sans tenir compte de `Point` (comme `CanPickUp`, `IslanderInventoryComponent.cpp:612-621`). Ajouter une portée propre aux trois classes, plus une spec de refus. | ouvert, T-289 |
| M1 | SEC-03 | MAJEUR | `IslanderGrabComponent.cpp:329, 418, 500, 569, 590` ; `Weapons/IslanderInventoryComponent.cpp:408-423` | `ServerInteract`, `ServerStoreHeld`, `ServerGrab`, `ServerRelease`, `ServerThrow` et `ServerSelectSlot` sont fiables et n'ont ni délai ni compteur. | Seau de jetons serveur par joueur (modèle `ConsumeScreenRequest` ou `VoiceRateLimiter`), valeurs en DataAsset. | ouvert, T-290 |
| M2 | SEC-08 / SEC-01 | MAJEUR | `Player/IslanderMovementComponent.cpp:180`, `:495-514` ; `Player/IslanderCharacter.cpp:509-515`, `:544-558` | Le serveur applique `FLAG_Custom_2` du client sans condition. Le vol T-021 est actif dans tous les builds, Shipping compris (aucun `#if`, aucune règle). | `IsFlightAllowed()` : faux en Shipping, ailleurs cvar serveur explicite, testé dans `UpdateFromCompressedFlags` et `UpdateCharacterStateBeforeMovement`. Décision T-021 / ADR-008. | ouvert, T-291 |
| M3 | SEC-08 | MAJEUR | moteur `PlayerController.cpp:1170-1178`, `:1879-1884`, `:1968-1976` ; `Player.cpp:97-158` ; `monke_islandPlayerController.h` | Hors Shipping, un client distant peut taper `serverexec EnableCheats`, qui appelle `AddCheats(true)` sur l'hôte. Il obtient alors God, Slomo, DestroyAll, Summon et SwitchLevel (`ServerTravel`) chez l'hôte, et peut mettre la partie en pause. Or les builds diffusés sont en Development (M5). | Surcharger `ServerExecRPC_Validate/_Implementation` (virtuels) et `EnableCheats` dans `Amonke_islandPlayerController` : contrôleur local seulement. Faire de même pour `ServerPause`. Activer `bRPCDoSDetection=true`. | ouvert, T-296 |
| M4 | §1 STRIDE-E/D (accès session, §16) | MAJEUR | `UI/Tapao/TapaoUISkin.h:121` ; `UI/Tapao/TapaoUISubsystem.cpp:686`, `:979-981` ; `Expedition/Islands/ExpeditionIslandSubsystem.cpp:665-673` ; `Config/DefaultGame.ini:68-70` ; moteur `BaseGame.ini:68` | « Continuer » ou « Recommencer » sur l'écran de fin appelle `TravelToPlay()` avec `PlayOptions="listen"` (le DA ne le surcharge pas). Une partie solo devient alors un serveur d'écoute LAN à 16 places, conservées au voyage. Il n'y a ni `MaxPlayers` dans `DefaultGame.ini` (T-127 non fusionnée), ni `PreLogin`, ni mot de passe, ni expulsion. | `PlayOptions` vide en solo, `MaxPlayers=4`, `PreLogin` fermé hors lobby, pseudo choisi et borné. | ouvert, T-297 |
| M5 | SEC-08, §5 | MAJEUR | `docs/releases/v1-rec-investisseurs.md:3`, `:28` ; `scripts/ue.ps1:22-23` ; `build/package/Windows/monke_island/Binaries/Win64/` (exe et `.pdb` de Development) | Les builds diffusés (V1 investisseurs, zips v1.6 et v1.7) sont en Win64 Development : console, `monke.*`, vol, `ServerExec` et symboles inclus. Le contrôle `shipping` du skill n'a pas été passé. | Shipping pour toute diffusion hors des PC du propriétaire, PDB archivés à part, checklist de release complétée. | ouvert, T-298 (complète T-127) |
| M6 | SEC-05 (CWE-502, CWE-400) | MAJEUR | `Run/RunSubsystem.cpp:125-141` ; `UI/Tapao/TapaoUISubsystem.cpp:1005-1019` ; `Run/RunSaveGame.h:20-22` ; `UI/Tapao/TapaoSaveGame.h:20-22` ; moteur `GameplayStatics.cpp:2457-2486`, `String.cpp.inl:1848-1860` | Le code appelle `LoadGameFromSlot` sans contrôle et ne relit jamais `Version`. Le moteur charge la classe nommée dans le fichier puis appelle `NewObject` (contrôle fatal en Development). Il lit chaînes et tableaux sans plafond : un `Run.sav` aléatoire peut planter **à chaque lancement**, car `URunSubsystem::Initialize` le lit au démarrage (lu, non reproduit). Aucun test sur un fichier malformé. | Contrôles avant désérialisation (en-tête, classe, taille), CRC, version future refusée, bornes après chargement, fichier mis de côté, écriture atomique (T-115), spec. | ouvert, T-302 |
| M7 | SEC-09 (RGPD) | MAJEUR | `Run/RunSubsystem.cpp:191`, `:197`, `:160-168` ; `Death/DeathSpectatorComponent.cpp:452` ; `Player/IslanderHealthComponent.cpp:167-169` ; `UI/Tapao/TapaoUISubsystem.cpp:916` ; moteur `OnlineIdentityNull.cpp:66-79`, `:300-308`, `GameModeBase.cpp:800` | Les logs contiennent des pseudos et l'IP de l'hôte rejoint. Avec OSS Null, le pseudo par défaut vaut `<nom de machine>-<LoginId>`, tronqué à 20 caractères : les autres joueurs le voient, et sa forme complète sert de clé des loadouts dans `Run.sav`. Concerne les builds Development, ceux qu'on diffuse. Le Shipping n'a pas de logs. | Écrire le `PlayerId` ou le nom d'objet, masquer l'adresse, hacher les clés avec un sel, réviser `PRIVACY.md` (voix). Le pseudo choisi relève de T-297. | ouvert, T-303 |
| M8 | SEC-07 | MAJEUR (outillage, hors build) | `.mcp.json:9-11` ; `scripts/blender_mcp_addon.py:1`, `:30-33` | `uvx blender-mcp` n'a pas de version : la dernière version PyPI s'exécute à chaque session. L'addon « MCP for Blender » 1.8 (Siddharth Ahuja, 2025) est copié dans le dépôt sans licence ni entrée dans `licenses-inventory.md`. Il a accès au réseau (Poly Haven, Sketchfab, Hyper3D, Poly Pizza, Hunyuan3D). | Épingler la version, noter provenance et licence, ou retirer la copie et installer l'addon depuis l'amont à une version fixée. | ouvert, T-307 |
| m1 | SEC-08 | MINEUR | `Run/RunSubsystem.cpp:46-100` ; `Creatures/GiantCrabEmergence.cpp:26-33` ; `Audio/ExpeditionAudioSubsystem.cpp:30-53` | 9 commandes sans `#if !UE_BUILD_SHIPPING` : `monke.Run.Status`, `.Stay`, `.Leave`, `.Money`, `.Day`, `.New`, `monke.CrabEmerge`, `monke.Audio.Play`, `.Report`. Elles restent inaccessibles en Shipping : pas de console, `-ExecCmds` coupé (`UnrealEngine.cpp:2543-2552`), aucun appel de console dans le code ni dans les `.uasset`. | Garde `#if` ; réserver les règles de « triche » au serveur. | ouvert (dans T-291) |
| m2 | SEC-04 (CWE-22) | MINEUR | `UI/Tapao/TapaoUISubsystem.h:94-95` | `SaveSlot` est en `UPROPERTY(Config)` : un `Game.ini` utilisateur peut viser un `*.sav` hors de `SaveGames` (écriture et effacement). Il faut déjà l'accès en écriture au profil. | Constante, ou nom validé (alphanumérique) et `FPaths::IsUnderDirectory`. | ouvert (dans T-302) |
| m3 | SEC-06 (CWE-798) | MINEUR | `Config/DefaultEngine.ini:101` ; `scripts/blender_mcp_addon.py:53` | Deux valeurs à ne pas afficher : (1) le jeton `SecurityToken` du plugin AndroidFileServer, inerte (pas de cible Android, `bIncludeInShipping=False`) ; (2) la clé d'API d'essai Hyper3D publiée par l'amont, qui n'est pas un secret du projet. | Supprimer la section AFS (ou désactiver le plugin, activé par défaut) et la régénérer si un build Android a existé. Ne pas copier la clé de l'amont. | ouvert (dans T-307) |
| m4 | §5 | MINEUR | `Config/` (pas de `DefaultCrypto.ini`) | Les paks ne sont pas signés. Exigé avant une diffusion publique. | *Project Settings > Crypto* au moment de la publication (PROD). | ouvert (checklist T-298) |
| m5 | SEC-02 / ERR (CWE-20) | MINEUR | `UI/Tapao/STapaoPlayMenu.cpp:93-99` ; `UI/Tapao/TapaoUISubsystem.cpp:906-917` ; moteur `UnrealNames.cpp:3271-3284` | L'adresse de REJOINDRE n'est pas validée et part dans `OpenLevel(FName(*Address))`. Au-delà de 1 023 caractères, `checkf` fait planter un build Development ; les options d'URL sont injectables (effet local seulement). | Valider `hôte[:port]` (longueur, caractères), message d'erreur §16. | ouvert (à joindre à T-297) |
| m6 | SEC-02 (§20) | MINEUR | moteur `PlayerController.cpp:1996-2009`, `GameModeBase.cpp:1401-1409` ; `UI/Tapao/TapaoDeathUISubsystem.cpp:159`, `:165` | `ServerChangeName` (Shipping compris) n'a ni borne de longueur, ni débit, ni filtre ; le nom s'affiche en entier à l'écran de spectateur. | Surcharger `ChangeName` dans le GameMode. | ouvert (dans T-297) |
| m7 | §1 STRIDE-S | MINEUR (LAN) | `Run/RunSubsystem.cpp:160-168` ; `Weapons/IslanderInventoryComponent.cpp:71-116` | En OSS Null, l'identifiant unique est déclaré par le client sans authentification. Un client modifié peut prendre celui d'un coéquipier et récupérer son équipement au voyage suivant. | Accepté en LAN entre amis ; authentification de plateforme obligatoire au L7. | accepté jusqu'au L7 |
| m8 | SEC-03 (DoS) | MINEUR (latent) | `Weapons/IslanderInventoryComponent.cpp:700-719`, `:757-761`, `:777` | Lâcher une pile crée `Count - 1` acteurs physiques (jusqu'à 998). Aujourd'hui, aucun `DA_Item_*` ne surcharge `MaxStack` (=1). | Une seule pickup qui porte un compte, ou un plafond par lâcher. | ouvert, sans ticket |
| m9 | SEC-10 / §4 (CWE-22, CWE-494) | MINEUR | `scripts/fetch_atoll_textures.py:91-95` ; `scripts/character/fetch_character_assets.py:92-96` ; `scripts/props/fetch_minka_assets.py:77-81` ; `art/SOURCES.md:7` | `out / rel` vient de l'API Poly Haven sans confinement. `inc["url"]` n'est pas contrôlé en `https` (`urllib` accepte `file://`). Aucune empreinte SHA-256 des téléchargements. Les zips, eux, sont sûrs (nom de base seul). | `resolve()` + `is_relative_to`, `https://` imposé, SHA-256 notées. | ouvert (dans T-307) |
| m10 | §6 | MINEUR (L7) | `docs/features/chat-vocal.md:111`, `:187-189` | La voix n'a pas de mute par joueur. Elle est relayée à tous et la proximité ne s'applique que chez le client ; l'expéditeur n'est pas vérifié. Ces limites sont documentées. | Mute individuel avant le L7 ; VoIP de plateforme. | ouvert (L7) |
| m11 | §2 (réplication minimale) | MINEUR | `Weapons/IslanderInventoryComponent.cpp:44` | `Slots` est répliqué à tous les clients. Le HUD ne lit que l'inventaire du pion local (`STapaoHUD.cpp:270-274`, `:587-590`). | `COND_OwnerOnly`, après avoir vérifié qu'aucun lecteur distant n'en dépend. | à voir avec T-268 |
| m12 | SEC-09 | MINEUR | 36 fichiers suivis (`.claude/agents/*`, `CLAUDE.md`, docs) | Chemin utilisateur Windows et dossier OneDrive en clair. Sans effet tant que le dépôt reste local. | Écrire un chemin relatif ou une variable avant toute publication du dépôt. | ouvert, sans ticket |
| m13 | §2 | MINEUR | moteur `BaseGame.ini:40-41` (rien dans `DefaultGame.ini`) | `bMovementTimeDiscrepancyDetection` et `bMovementTimeDiscrepancyResolution` sont faux : accélération possible par l'horodatage. Moot tant que le vol existe. | Les activer une fois M2 corrigé. | ouvert (à voir avec T-296) |
| m14 | §4 (outils) | MINEUR | `.codex/config.toml` (local, non suivi) | `approval_mode="approve"` sur `call_tool` du MCP Unreal : l'agent Codex exécute toute action éditeur sans confirmation. | Garder l'approbation manuelle pour `call_tool`. | observation |

## 3. Détail des constats CRITIQUE et MAJEURS

### C1 — Portée des interactions crue du client (SEC-02, CRITIQUE)
- **Preuve** : `ServerInteract_Implementation(Target, Point)` (`IslanderGrabComponent.cpp:329-359`) :
  - vérifie `CanInteract` ;
  - puis `FVector::Dist(Eyes, Point) > GrabRange + 50` (`:347`) sur le `Point` du client ;
  - puis une trace `Eyes → Point` qui ne refuse que si `Hit.GetActor() != Target` (`:352-357`) ;
  - `StoreHeld` refait le même schéma (`:438-445`).

  Les cibles suivantes n'ajoutent aucune portée :
  - `AConsumablePickupActor::CanInteract/Interact` (`:62-91`) ;
  - `AAmmoPickup::CanInteract/Interact` (`:203-225`) ;
  - `ALootCrateActor::Interact` : vidage accroupi, `:440-450` ;
  - `ALootCrateActor::CanStoreItem` : le paramètre `User` n'est pas utilisé, `:183-213`.

  En revanche, la barque (`BoatActor.cpp:262-266`), le galion (`GalleonActor.cpp:479-512`), le chapeau, le marchand (portée dans `Sell`) et les pickups d'arme (`CanPickUp`) vérifient la portée depuis les positions serveur.
- **Scénarios d'abus** : client modifié, `Point = Eyes + 50 cm` :
  1. il consomme tous les soins de la carte, où qu'ils soient ;
  2. il aspire les munitions de toute l'île ;
  3. il vide, accroupi, la caisse posée dans la barque en mer ;
  4. il saisit un objet de valeur à terre, puis `ServerStoreHeld(CaisseDuGalion, Point)` : l'objet est rangé dans la caisse du galion. Il compte alors dans `GetCargoAboard` et `ComputeDayValue` (`docs/features/caisse-transport.md:39-40`) sans transport. La boucle « arrimer, extraire » est contournée et le quota gonflé.

  Toute nouvelle classe interactable sans contrôle propre hérite du trou.
- **Correctif** :
  - côté serveur, ignorer `Point` ;
  - mesurer la distance des yeux à `Target->GetComponentsBoundingBox()` (`ComputeSquaredDistanceToPoint` ≤ (portée + tolérance)²) ;
  - tracer vers le centre de la cible ;
  - ajouter une portée défensive dans `CanInteract` des trois classes et dans `CanStoreItem` (distance de `User`) ;
  - spec `monke_island.Interaction.Reach` : appel serveur avec une cible à 30 m, refusé.
- **Ticket** : T-289 (`ue-gameplay-programmer`), à coordonner avec T-268.

### M1 — RPC répétables sans débit (SEC-03, MAJEUR)
- **Preuve** : voir le tableau. Aucun de ces corps ne lit d'horloge ni de compteur.
- **Scénario d'abus** : à 1 000 appels par seconde, l'hôte enchaîne :
  - des traces ;
  - des `Interact` (s'asseoir et se lever, téléportation par l'échelle du galion) ;
  - des créations et destructions de poignée physique, avec changement des réponses de collision de la cible ;
  - des équipements alternés (gestes, timers, réplication vers les 3 autres clients).

  Résultat : une baisse d'images chez tout l'équipage.
- **Correctif** :
  - un `FRequestBucket` commun, sur le modèle de `ConsumeScreenRequest` ou de `VoiceRateLimiter` ;
  - réglages dans un DataAsset ;
  - refus et `Warning` limité à une fois par seconde ;
  - test de rafale ;
  - en filet global, `bRPCDoSDetection=true` (T-296).
- **Ticket** : T-290, à enchaîner après T-289 (mêmes fichiers).

### M2 — Vol au double Espace en jeu (SEC-08, MAJEUR)
- **Preuve** :
  - `JumpPressed` passe à `ToggleFlying` sur un double appui (`IslanderCharacter.cpp:509-515`) ;
  - `SetWantsToFly` est appelé sans condition (`:557`) ;
  - côté serveur, `bWantsToFly` est lu dans les drapeaux compressés du client (`IslanderMovementComponent.cpp:180`) puis appliqué avec `SetMovementMode(MOVE_Flying)` (`:499-514`) ;
  - aucune garde `UE_BUILD_SHIPPING`, aucune règle ;
  - ADR-008 le classe « À couper » ; T-021 attend la décision du propriétaire.
- **Scénario d'abus** : n'importe quel joueur, sans modifier son client, survole la mer et le récif. Il évite la barque, les requins et la submersion, ce qui casse la boucle de la §6.
- **Correctif** : `IsFlightAllowed()` :
  - `false` en Shipping ;
  - en dev, cvar serveur `monke.Cheat.Fly`, ou règle de la carte de test, faux par défaut ;
  - testé côté serveur avant d'appliquer le drapeau ;
  - entrée ignorée côté client.

  Ajouter les gardes manquantes de m1.
- **Ticket** : T-291.

### M3 — Console de l'hôte ouverte aux clients en Development (SEC-08, MAJEUR)
- **Preuve (moteur 5.8.3)** :
  - `ServerExecRPC_Implementation` exécute `ConsoleCommand(Msg)` hors Shipping (`PlayerController.cpp:1879-1884`) ;
  - côté serveur, `UPlayer::Exec` route la commande vers les fonctions `exec` du PC, du pion, du GameMode et du CheatManager (`Player.cpp:97-158`) ;
  - `EnableCheats` (exec) appelle `AddCheats(true)`, forcé, hors Shipping (`PlayerController.cpp:1170-1178`) ;
  - `ServerPause_Validate` accepte un client distant hors Shipping (`:1968-1976`).

  Les commandes `monke.*` de l'`IConsoleManager` ne sont pas atteintes par ce chemin (pas d'appel `GEngine->Exec`). Le projet ne surcharge rien de tout cela (`monke_islandPlayerController.h`).
- **Scénario d'abus** : un ami, ou un inconnu du même réseau (voir M4), rejoint un build Development et tape :
  - `serverexec EnableCheats` ;
  - puis `serverexec God`, `serverexec Slomo 5` (pour tous), `serverexec DestroyAll IslanderCharacter`, `serverexec Summon …`, `serverexec SwitchLevel …` (`ServerTravel` de l'hôte), ou des pauses en rafale.
- **Correctif** (dans `Amonke_islandPlayerController`) :
  - `ServerExecRPC_Validate`, `ServerExecRPC_Implementation` et `EnableCheats` : refus pour un contrôleur non local, sauf cvar de dev explicite posée par l'hôte ;
  - `ServerPause_Validate` : idem ;
  - `DefaultEngine.ini` : `[GameNetDriver RPCDoSDetection] bRPCDoSDetection=true` ;
  - preuve : deux instances Development en LAN.
- **Ticket** : T-296 (`ue-network-engineer`).

### M4 — Serveur d'écoute non voulu, 16 places, sans contrôle d'accès (MAJEUR)
- **Preuve** :
  - JOUER en solo passe `""` (`StartSolo`, `TapaoUISubsystem.cpp:892-897`, correct) ;
  - mais l'écran de fin lie `OnFirst` à `TravelToPlay()` (`:686`), qui envoie `Skin->PlayOptions` (`:979-981`), soit `"listen"` par défaut (`TapaoUISkin.h:121`, valeur non surchargée dans `DA_TapaoUISkin`) ;
  - la session prend alors le `MaxPlayers` du moteur, 16 (`BaseGame.ini:68`), qu'`ExpeditionIslandSubsystem.cpp:665-673` reporte ensuite à chaque voyage ;
  - aucun `PreLogin`, `ApproveLogin`, mot de passe ni expulsion dans `Source/`.
- **Scénario d'abus** : un joueur solo finit une partie dans une école, un café ou une résidence, puis clique « Continuer ». Son jeu écoute en UDP sur le LAN (7777) et accepte jusqu'à 15 inconnus. En Development, cela ouvre la porte à M3.
- **Correctif** :
  - `PlayOptions` vide pour une partie solo : retenir le mode à l'hôte ;
  - `[/Script/Engine.GameSession] MaxPlayers=4` (T-127) ;
  - `PreLogin` fermé hors lobby ;
  - pseudo choisi (M7) et `ChangeName` borné (m6) ;
  - validation de l'adresse de REJOINDRE (m5).
- **Ticket** : T-297.

### M5 — Builds diffusés en Development, symboles inclus (SEC-08, §5, MAJEUR)
- **Preuve** :
  - `v1-rec-investisseurs.md:3` (« Win64 Development ») et `:28` (« symboles Development inclus ») ;
  - `monke_island.exe` et `monke_island.pdb` se trouvent dans `build/package`, `build/rec-T122` et `build/rec-T124` ;
  - `ue.ps1` produit du Development par défaut (`:22-23`) ;
  - T-133 prévoit le zip v1.6 pour le test à deux machines.

  Le Shipping lui-même est sain : `Target.cs` ne réactive ni la console, ni le CheatManager, ni les logs.
- **Correctif** :
  - Shipping pour tout build qui quitte les PC du propriétaire ;
  - Development réservé aux tests internes, idéalement après T-296 ;
  - PDB archivés à part (T-127) ;
  - contrôle `shipping` dans `release-checklist.md` : console absente, `monke.*` absentes, vol absent, plugins MCP absents, PDB absents, paks signés avant publication (m4).
- **Ticket** : T-298 (`ue-build-engineer`).

### M6 — Sauvegardes désérialisées sans contrôle (SEC-05, MAJEUR)
- **Preuve** :
  - `LoadRun` appelle `LoadGameFromSlot("Run")` puis copie `State`, `LastDayReport` et `Loadouts` tels quels (`RunSubsystem.cpp:125-141`), dès `Initialize` de chaque instance ;
  - `GetSave` fait de même pour `TapaoCampaign` (`TapaoUISubsystem.cpp:1005-1019`) ;
  - `Version` est écrite mais jamais relue ;
  - côté moteur (`GameplayStatics.cpp:2457-2486`), le nom de classe lu dans le fichier passe à `LoadObject<UClass>`, puis `NewObject<USaveGame>` (fatal sous `DO_CHECK` si ce n'est pas une `USaveGame`) ;
  - sans la balise `SAVG`, le fichier est relu comme « ancien format », et la longueur de chaîne est lue sans plafond (`String.cpp.inl:1848-1860`, `MaxSerializeSize` à 0 pour un `FMemoryReader`).

  Les calculs aval sont défensifs (`ZoneAt` borné, jour ≤ 0 neutre, `SetRounds` borné en bas), mais `Treasury`, `DaysPlayed` et le nombre de loadouts ne le sont pas. `RestoreRunLoadout` charge n'importe quel chemin d'asset lu dans le fichier (`IslanderInventoryComponent.cpp:96`), sans liste autorisée (§20).
- **Scénario** : un `Run.sav` corrompu (coupure pendant l'écriture, disque plein) ou échangé (« voici ma partie ») fait planter le jeu au démarrage, à chaque lancement, jusqu'à ce qu'on efface le fichier à la main. Ce scénario est lu dans le code, pas reproduit.
- **Correctif** :
  - `LoadDataFromSlot`, taille maximale (1 Mo) ;
  - balise `SAVG` et nom de classe attendu vérifiés **avant** `LoadGameFromMemory` ;
  - CRC32 dans un en-tête projet ;
  - version future refusée ;
  - bornes après chargement ;
  - `Definition` présente dans une liste autorisée ;
  - fichier renommé en `.corrupt` puis partie neuve ;
  - écriture atomique (T-115) ;
  - `SaveSlot` hors `Config` (m2) ;
  - spec `monke_island.Save.Malformed` : fichier tronqué, aléatoire, ou de classe étrangère.
- **Ticket** : T-302 (`ue-systems-programmer`).

### M7 — Données personnelles dans les logs, nom de machine exposé (SEC-09, MAJEUR)
- **Preuve** :
  - pseudos écrits en `Log` : `RunSubsystem.cpp:191`, `:197`, `DeathSpectatorComponent.cpp:452`, `IslanderHealthComponent.cpp:167-169` ;
  - adresse de l'hôte rejoint : `TapaoUISubsystem.cpp:916` ;
  - en OSS Null, connexion automatique au démarrage : l'identifiant vaut `<HostName>-<LoginId>` pour la première instance hors éditeur (`OnlineIdentityNull.cpp:66-79`), et le surnom est cet identifiant (`:300-308`) ;
  - le serveur le tronque à 20 caractères (`GameModeBase.cpp:800`), puis l'affiche aux autres (spectateur, voix) ;
  - la forme complète sert de clé des loadouts dans `Run.sav` (`RunSubsystem.cpp:160-168`).

  La norme §6 ne prévoit que le pseudo de plateforme, et `PRIVACY.md` date d'avant la voix. Les logs sont absents en Shipping, mais les builds diffusés sont en Development, et T-127 prévoit le Crash Reporter, qui joint les logs.
- **Correctif** :
  - `PlayerId` ou nom d'objet dans les logs ;
  - adresse masquée ;
  - clé de sauvegarde = hachage salé ;
  - `PRIVACY.md` révisé ;
  - pseudo saisi (T-297).
- **Ticket** : T-303 (`ue-reliability-engineer`).

### M8 — Dépendances d'outillage non épinglées ni licenciées (SEC-07, MAJEUR, hors build)
- **Preuve** :
  - `.mcp.json:9-11` : `"command": "uvx", "args": ["blender-mcp"]`, sans version ;
  - `scripts/blender_mcp_addon.py` : 270 Ko, « Code created by Siddharth Ahuja … © 2025 », version (1, 8) ;
  - absent de `licenses-inventory.md` et de `SOURCES.md` ;
  - écoute en local seulement (`localhost:9876`, `:1184`) ;
  - télémétrie désactivée par défaut (`:5723-5727`) ;
  - `exec(code)` des commandes reçues (`:2248`) : c'est sa fonction.

  Les plugins du jeu sont tous Epic (moteur 5.8.3). Aucun plugin tiers ; `Plugins/` est absent.
- **Risque** : une version compromise de `blender-mcp` publiée sur PyPI s'exécuterait sur les postes de dev à la session suivante.
- **Correctif** :
  - `uvx blender-mcp==<version>` ;
  - licence et provenance notées ;
  - copie de l'addon documentée, ou remplacée par une installation épinglée ;
  - corrections de m3 (clé de l'amont) et de m9 ;
  - création de `docs/security/threat-model.md` à partir du §1.
- **Ticket** : T-307 (`ue-security-engineer`).

## 4. Conformités relevées
- **Boutique** (`ShopkeeperActor.cpp:95-178`) :
  - offre bornée (`IsValidIndex`), prix lu dans le catalogue serveur ;
  - portée de 450 cm sur les positions serveur, trésorerie de `URunSubsystem` ;
  - place vérifiée, remboursement si la remise échoue ;
  - `BuyCooldown` de 0,3 s par acheteur ;
  - l'acheteur est le propriétaire du composant, jamais un paramètre.
- **Inventaire** :
  - `ServerMove`, `QuickMove`, `Gather` et `Drop` : seau de 20 jetons par seconde ;
  - `InventoryGrid` (indices, `Count` borné, chaque source au plus une fois) et `CommitCells` (taille, validité, atomicité) ;
  - `ServerDropEquippedItem` : 0,25 s ;
  - `CanPickUp` : portée et vue depuis les positions serveur, mort, ragdoll et assis refusés ;
  - tests de duplication et de double ramassage présents (`InventorySpec.cpp:165`, `:223`, `:242`, `:268`).
- **Armes** :
  - `IsUsable` (porteur, vivant, sans saisie, hors de l'eau) ;
  - munitions, gestes et cadence décidés par des timers serveur ;
  - `ServerAttack` du sabre validé : coup connu, enchaînement, fenêtre d'annulation, endurance, temps de charge ;
  - mitrailleuse plafonnée par `MaxRoundsPerFrame`.
- **Mouvement** : aucune option d'autorité du client (`bServerAcceptClientAuthoritativePosition`, `bIgnoreClientMovementErrorChecksAndCorrection`) dans le code.
- **Barque** : `SetRowInput` vérifie le rameur, `IsFinite` et borne les valeurs (`BoatActor.cpp:317-324`).
- **Mort et spectateur** : `ServerCycle` a le seul `_Validate` du projet, plus une cadence minimale, la mort revérifiée et `COND_OwnerOnly`.
- **Voix** : `ServerSetTalking` passe par un seau de jetons, appuyer-pour-parler est le mode par défaut, un indicateur s'affiche et aucun pseudo n'est écrit dans `LogMonkeVoice`.
- **Paramètres** : `SettingsMath::Sanitize` contrôle `IsFinite`, les bornes et les énumérations.
- **Choix de l'île** :
  - options `?Island=` et `?Seed=` assainies ;
  - greed et fin de journée décidées par l'hôte seul ;
  - pause seulement en solo.
- **Commandes de dev** : 26 commandes `monke.*` sur 35 sont sous `#if !UE_BUILD_SHIPPING`. Le moteur coupe en Shipping la console, `-ExecCmds`, `ServerExec` et le CheatManager, et `Target.cs` ne les réactive pas.
- **Outillage** :
  - plugins MCP et boîtes à outils en `TargetAllowList: Editor` (le constat de l'audit 40 du 2026-10-04 est corrigé) ;
  - le serveur MCP de l'éditeur écoute sur `localhost` (`HttpServerConfig.h:13`, aucune surcharge) et vérifie l'origine.
- **Fichiers et secrets** :
  - aucune autre E/S fichier dans le code de jeu : l'outil de réimport est `WITH_EDITOR && WITH_AUTOMATION_TESTS` ;
  - aucun `.env`, aucune clé privée, aucun jeton GitHub, AWS, OpenAI ou Anthropic, aucune clé de pak, Steam ou EOS dans les fichiers suivis.

## 5. Recherches effectuées
- **RPC** : `rg "UFUNCTION\([^)]*\b(Server|Client|NetMulticast)\b"`. On compte 61 déclarations, dont 36 `Server`, toutes relues (annexe A).
- **Commandes** : un script d'inventaire des gardes `#if` sur `FAutoConsole*` et `TAutoConsoleVariable`. On compte 42 entrées : 26 gardées, 9 commandes et 7 cvars sans garde (annexe B).
- **Secrets** : un script de motifs (affectations `key`, `secret`, `token`, `password`, `Authorization`, préfixes de clés connus, blocs `PRIVATE KEY`, réglages de chiffrement et de signature des paks, Steam et EOS, URL avec identifiants, hex ≥ 32) sur 2 117 fichiers suivis hors `Content/`, plus les fichiers non suivis non ignorés. Il y a 1 008 correspondances :
  - 994 chaînes hex : SHA-256 des manifestes et des binaires de release, identifiants publics d'items ArcGIS, `ProjectID`, et le jeton AFS ;
  - 14 affectations : le jeton AFS et 13 lectures de clé depuis les préférences de l'addon Blender.

  Une recherche ciblée (`_KEY =`) a trouvé en plus la clé d'essai de l'amont. Les deux secrets sont décrits en m3 ; aucune valeur n'est reproduite ici.
- **`.uasset`** :
  - `MaxStack` absent des 14 `DA_Item_*` ;
  - `ExecuteConsoleCommand` absent du contenu ;
  - aucun Blueprint n'implémente `IslanderInteractable`.
- **Moteur** : `PlayerController.cpp`, `Player.cpp`, `World.cpp`, `GameplayStatics.cpp`, `String.cpp.inl`, `UnrealNames.cpp`, `OnlineIdentityNull.cpp`, `GameModeBase.cpp`, `UnrealEngine.cpp`, `BaseEngine.ini` (`RPCDoSDetection` désactivé par défaut, `:1924-1926`), `BaseGame.ini`.

## 6. Risques acceptés
- **Triche de l'hôte** (norme §1) : sauvegarde éditée à la main, commandes `monke.Run.*` en Development, cvar `monke.Hostility.ScanInterval` réglable par `Engine.ini` (effet serveur seulement).
- **Voix en LAN** : expéditeur non vérifié par le moteur, relais à tous (`chat-vocal.md:111`). À revoir au L7.
- **Identifiants OSS Null non authentifiés** (m7), en LAN entre amis jusqu'au L7.
- **N'importe quel joueur à la barre peut lancer la fin de journée** (`GalleonActor.cpp:539-553`), au risque de faire mourir ceux qui sont à terre. C'est un choix de conception (vote de départ §6.5 à trancher), pas un défaut de sécurité.

## 7. Suite
**Tickets créés** (statut `a-faire`) :

| Ticket | Constat | Propriétaire |
|---|---|---|
| T-289 | C1 portée des interactions | `ue-gameplay-programmer` |
| T-290 | M1 débit des RPC | `ue-gameplay-programmer` (après T-289) |
| T-291 | M2 vol, m1 gardes Shipping | `ue-gameplay-programmer` |
| T-296 | M3 console et triche à distance en Development, `RPCDoSDetection` | `ue-network-engineer` |
| T-297 | M4 session solo, `MaxPlayers`, `PreLogin`, pseudo, m5, m6 | `ue-network-engineer` |
| T-298 | M5 builds diffusés, PDB, checklist (m4) | `ue-build-engineer` |
| T-302 | M6 sauvegardes, m2 | `ue-systems-programmer` |
| T-303 | M7 données personnelles | `ue-reliability-engineer` |
| T-307 | M8 outillage, m3, m9, `threat-model.md` | `ue-security-engineer` |

Sans ticket (mineurs) : m8, m10 (L7), m11 et m13 (à verser à T-268), m12 (avant publication du dépôt), m14.

**Reste à vérifier** (preuves à produire, rien de cela n'a été lancé) :
1. C1 : test automatisé appelant `ServerInteract_Implementation` et `StoreHeld` avec une cible à 30 m (doit être refusé après T-289).
2. M3 : deux instances Development en LAN, `serverexec EnableCheats` puis `serverexec God` depuis le client (doit être refusé après T-296).
3. M4 : solo, écran de fin, « Continuer », puis `netstat` : aucun port d'écoute après T-297.
4. M6 : `Run.sav` tronqué, aléatoire, ou d'une autre classe : pas de plantage au démarrage (spec T-302).
5. M7 : capture de l'écran spectateur dans un build pour confirmer le pseudo issu du nom de machine.
6. Chaîne d'outils : le moteur fournit le SDK FBX 2020.2 (`Engine/Source/ThirdParty/FBX/2020.2`), utilisé pour importer des FBX venus de OneDrive ou d'Internet. Sa date laisse supposer qu'il précède les correctifs publiés par Autodesk depuis. À vérifier dans les avis Autodesk, sans requête externe pendant cette revue.
7. Signature Authenticode de l'exécutable, si Smart App Control est actif chez un joueur (voir l'audit 40), et signature des paks avant publication.

## Annexe A — RPC `Server` (36)
| RPC | Implémentation | Propriétaire, cible, bornes | Débit | Verdict |
|---|---|---|---|---|
| `ServerBuy(Shop, Offer)` | `IslanderInventoryComponent.cpp:218` → `ShopkeeperActor.cpp:95` | composant possédé ; offre `IsValidIndex` ; portée serveur ; trésorerie serveur | 0,3 s | conforme |
| `ServerSelectSlot(Slot)` | `IslanderInventoryComponent.cpp:408` | possédé ; `0 ≤ Slot < barre` | aucun | M1 |
| `ServerDropEquippedItem()` | `:795` | objet en main ; pas en ragdoll | 0,25 s | conforme |
| `ServerMove`, `ServerQuickMove`, `ServerGather`, `ServerDrop` | `:1018`, `:1026`, `:1034`, `:1042` | `InventoryGrid` ; vivant ; `Count` borné | 20/s (+ 0,25 s pour `Drop`) | conforme |
| `ServerInteract(Target, Point)` | `IslanderGrabComponent.cpp:329` | **portée sur `Point` client** | aucun | C1, M1 |
| `ServerStoreHeld(Container, Point)` | `:418` | objet tenu côté serveur ; **portée sur `Point` client** | aucun | C1, M1 |
| `ServerGrab(Target, GrabPoint)` | `:500` → `Grab` `:505-551` | physique, pas un pion, `NoGrab`, masse, ligne de vue ; portée sur `GrabPoint`, mais relâché au tick au-delà de 2 × portée (`:759-763`) | aucun | M1 |
| `ServerRelease()`, `ServerThrow()` | `:569`, `:590` | objet tenu | aucun | M1 (faible coût) |
| `ServerAdjustHoldDistance(float)` | `:638` | ±3 crans puis bornes (NaN donne 3) | non fiable | conforme |
| `ServerSetRowInput(FVector2D)`, `ServerStopRowing()` | `BoatRiderComponent.cpp:218`, `:226` → `BoatActor.cpp:317` | rameur ; `IsFinite` ; bornes | non fiable | conforme |
| Armes à feu : `ServerFire`, `ServerReload`, `ServerInspect`, `ServerSetAiming` | `FirearmWeapon.cpp:363`, `:477`, `:506`, `:526` (+ sous-classes) | `IsUsable` ; munitions et gestes serveur | gestes (`IsBusy`) | conforme |
| Mousquet : `ServerFire`, `ServerReload`, `ServerSetAiming` | `MusketWeapon.cpp:269`, `:370`, `:416` | `IsUsable` ; timers | timers | conforme |
| `ServerCeaseFire()` | `MachineGunWeapon.cpp:225` | — | — | conforme |
| Sabre : `ServerBeginCharge`, `ServerAttack(ECutlassMove)`, `ServerSetParrying` | `CutlassWeapon.cpp:336`, `:349`, `:902` | `GetAttack` (énumération hors liste refusée), enchaînement, fenêtres, endurance, charge | fenêtres serveur | conforme |
| Torche : `ServerSwing`, `ServerSetRaised` | `TorchItem.cpp:246`, `:241` | `IsUsable`, durée, endurance | geste | conforme |
| `ServerPunch()`, `ServerSetGuard(bool)` | `IslanderCombatComponent.cpp:119`, `:263` | `CanPunch` ; garde visuelle seulement (le serveur lit le drapeau de mouvement) | cooldown | conforme |
| `ServerTumble()`, `ServerSetFirstPerson(bool)` | `IslanderCharacter.cpp:381`, `:90` | ragdoll, mort, nage ; vue | ragdoll | conforme |
| `ServerPlayFanfare()`, `ServerRequestCeremony(Site)` | `TrumpetEmoteComponent.cpp:146`, `:185` → `FlagCeremonySite.cpp:111-146` | `CanPlay` ; zone, phase, état | cooldown ; phase | conforme |
| `ServerCycle(int8)` | `DeathSpectatorComponent.cpp:225-257` | `_Validate` ±1 ; mort ; `CanFollow` | intervalle minimal | conforme |
| `ServerSetTalking(bool)` | `VoiceChatComponent.cpp:258` | pion et vie | 6 jetons, +3/s | conforme |
| Moteur : `ServerExecRPC`, `ServerPause` | `PlayerController.cpp:1874-1896`, `:1968-1981` | aucun contrôle hors Shipping | — | M3 |
| Moteur : `ServerChangeName` | `PlayerController.cpp:1996-2009` | non vide seulement | aucun | m6 |
| Moteur : `ServerMove` (drapeaux personnalisés) | `IslanderMovementComponent.cpp:174-181` | sprint, garde (légitimes), vol (M2) | — | M2 |

## Annexe B — Commandes et cvars `monke.*` (42)
- **Sous `#if !UE_BUILD_SHIPPING` (26)** :
  - barque : `SpawnBoat`, `BoatSeatGuardCheck` ;
  - créatures : `SpawnShark`, `TikiDefender`, `TikiKillVillager`, `TikiCutArm`, `TikiKillWarrior`, `TikiCutCorpse` ;
  - temps : `SetHour`, `SetTime`, `TimeSpeed` ;
  - partie : `Death.Kill`, `EndDayAtHelm`, `Execution.Start`, `EndDeluge`, `Island.Next`, `FloodNow`, `DropCoconut` ;
  - furtivité et météo : `Stealth.Debug`, `Stealth.Night`, `Weather.Turn`, `Weather.Clear` ;
  - interface et voix : `UI.Loading`, `Voice.FakeTalk`, `Voice.Test`, `Voice.Dump`.
- **Commandes sans garde (9, m1, T-291)** : `Run.Status`, `Run.Stay`, `Run.Leave`, `Run.Money`, `Run.Day`, `Run.New` (`RunSubsystem.cpp:46-100`), `CrabEmerge` (`GiantCrabEmergence.cpp:26`), `Audio.Play`, `Audio.Report` (`ExpeditionAudioSubsystem.cpp:30-53`). Elles sont inaccessibles en Shipping, faute de console et de `-ExecCmds`.
- **Cvars sans garde (7)** :
  - `GiantCrab.ShowBodies` : `ECVF_Cheat`, en lecture seule en Shipping ;
  - `Hostility.ScanInterval` : cadence de l'IA côté serveur, réglable par `Engine.ini` (hôte, accepté) ;
  - `ShoreAmbience`, `WindLeaves`, `WindLeaves.Strength`, `Footsteps`, `Waves` : effets locaux et cosmétiques.

## Annexe C — Dépendances
- **Plugins du `.uproject`** : tous Epic, moteur 5.8.3 (CL 58210709) ; licences dans `docs/legal/licenses-inventory.md`.
  - Runtime : Niagara, StateTree, GameplayStateTree, ProceduralMeshComponent, OnlineSubsystem, OnlineSubsystemNull, OnlineSubsystemUtils.
  - Éditeur seulement : ModelingToolsEditorMode, ModelContextProtocol et les 11 boîtes à outils MCP.
  - AndroidFileServer est activé par défaut par le moteur, module runtime autorisé sur Win64 (m3).
- **Outillage** :
  - `blender-mcp` (PyPI, non épinglé) et addon « MCP for Blender » 1.8 copié dans le dépôt (M8) ;
  - MCP Unreal sur `127.0.0.1:8000`, en local (conforme).
- **Scripts réseau (12)** :

| Script | Rôle | Note |
|---|---|---|
| `fetch_atoll_textures.py`, `character/fetch_character_assets.py`, `props/fetch_minka_assets.py` | Poly Haven, ambientCG (CC0) | HTTPS ; zips : nom de base seul ; inclusions glTF non confinées (m9) |
| `props/fetch_footstep_sounds.py` | Kenney (CC0) | HTTPS ; URL fixes ; pas d'empreinte |
| `props/creature_sounds.py` | OpenGameArt (CC0) | HTTPS ; URL fixes ; pas d'empreinte |
| `map/fetch_tefenua.py`, `map/fetch_imagery.py` | données Te Fenua (Etalab 2.0 ; imagerie en référence seulement) | HTTPS |
| `chantier.py`, `ue.ps1`, `map/fragment_villages_ue.py` | MCP local `127.0.0.1` | — |
| `map/fragments/fraglib.py`, `atoll/plants.py` | — | faux positifs (git, `curl` = courbure) |

Aucun script n'exécute `eval` ou `exec` sur des données distantes. Les licences sont tenues dans `art/SOURCES.md`. Les empreintes des téléchargements restent à compléter (`SOURCES.md:7`) ; les sons d'armes v2 ont les leurs, hors Git.
