# Mitrailleuse à manivelle « Mle 1918 » (T-273, gameplay ; modèle et clips T-272)

**Lot :** L5 (armes à feu, extension du cahier §5 à arbitrer dans l'ADR-001, comme `armes-nouvelles.md`). Demande du propriétaire du 2026-10-10 : « crée et modélise une mitrailleuse à manivelle type année 1918 mitrailleuse lourde, modélise le rechargement, courir avec, les balles s'éjectent, le canon chauffe, ajouter le recul, le son, la lumière des explosions, rends la mitrailleuse ultra réaliste type Call of Duty ».
**Recette :** spec `monke_island.Weapons.MachineGun` (cadence, chaleur, surchauffe, bande, dispersion, gestes, règles, portage). Pas encore d'ID de recette du cahier (à créer avec l'ADR-001).

**Statut (2026-10-10, honnête) :** code **compilé** fichier par fichier (`-SingleFile`, cibles éditeur `monke_islandEditor` et jeu `monke_island`, Development : « Result: Succeeded » pour chaque fichier touché). Spec lancée dans l'éditeur ouvert (sa DLL de 22:04, une version antérieure des règles) : **17/18 vertes** ; l'échec venait du test lui-même (fenêtre de mesure du refroidissement), corrigé dans la source et vérifié par un portage Python de la simulation (surchauffe à 75 coups / 7,45 s, refroidie en 3,4 s, aucun coup pendant la surchauffe) : à relancer après le build. **Action `mgsetup` passée** deux fois (7,3 s puis 0,6 s, idempotente) : 28 sons et 3 cues, 4 systèmes Niagara, `DA_MachineGunRules` relu après sauvegarde, `BP_MachineGun`, `DA_Item_MachineGun`, `DA_Ammo_MGBox`, `BP_Ammo_MGBox`. Sons jamais écoutés dans Unreal. Rien n'est **testé en PIE**, **testé build** ni **testé multi-machines**. Le modèle, le rig et les clips viennent de T-272 (en cours) : tant qu'ils ne sont pas importés, l'arme n'a ni maillage ni animation et `mgsetup` ne la met ni en boutique ni sur la carte de test.

## Vue d'ensemble

| Élément | Où |
|---|---|
| Arme | `AMachineGunWeapon : AFirearmWeapon` (`Weapons/MachineGunWeapon.h/.cpp`) → `BP_MachineGun` |
| Règles | `UMachineGunRules : UFirearmRules` (`Weapons/MachineGunRules.h/.cpp`) → `DA_MachineGunRules` |
| Règles pures | `MachineGunMath` (`Weapons/MachineGunMath.h`) : cadence, manivelle, chaleur, dispersion, bande, gestes |
| Secousse caméra | `UMachineGunShakePattern` / `UMachineGunFireCameraShake` (`Weapons/MachineGunCameraShake.h/.cpp`) |
| Munitions | `EAmmoKind::MachineGunBelt` ; `DA_Ammo_MGBox` (caisse de 100 cartouches) → `BP_Ammo_MGBox` |
| Objet d'inventaire | `DA_Item_MachineGun` (« Mitrailleuse 1918 », 17 kg au sol) |
| Sons | `scripts/audio/machinegun_prepare.py` → `art/audio/weapons/machinegun/` (28 WAV, CC0 + synthèse, crédits dans `art/SOURCES.md`) |
| Mise en jeu | `scripts/editor/machinegun_setup_ue.py` (action `mgsetup` de la session du lead) |
| Tests | `Weapons/Tests/MachineGunSpec.cpp` (`monke_island.Weapons.MachineGun`) |

## Règles (toutes les valeurs dans `DA_MachineGunRules`, défauts du constructeur C++)

### Tir continu à la manivelle
- **Clic gauche maintenu** = la manivelle tourne : les coups partent un par un tant que la touche est tenue, que la bande n'est pas vide et que le canon n'a pas surchauffé.
- **Cadence** liée à la manivelle : `MinCadenceRpm` 450 coups/min aux premiers tours, montée linéaire jusqu'à `MaxCadenceRpm` 600 en `SpinUpSeconds` 0,4 s ; relâchée, la manivelle s'arrête en `SpinDownSeconds` 0,35 s (un nouvel appui avant reprend à la vitesse atteinte).
- **Premier coup immédiat** à l'appui, sauf si le précédent date de moins d'un intervalle : **taper vite ne dépasse jamais la cadence** (`BurstStartClock`).
- Horloge des coups indépendante de la fréquence d'image (même nombre de coups à 30 et 144 i/s) ; un à-coup est plafonné à 3 coups par image (`MaxRoundsPerFrame`), le reste est perdu, pas dû.
- **Pas de tir en courant** (`bCanFireWhileSprinting` faux) : la course coupe la rafale (FireEnd), le tir reprend à la marche si la touche est toujours tenue.
- Tout autre geste (sortie, rechargement, surchauffe, inspection, jet) **lâche la détente** : il faut ré-appuyer.
- Chaque coup est un `AFirearmWeapon::FireProjectiles(1)` du serveur : dégâts, os touché, impacts, bruit pour la furtivité, **traçante 1 coup sur 3** (`Tracer.EveryNth` = 3, T-276).

| Valeur | Défaut |
|---|---|
| Dégâts d'un coup (`Damage`) | 34 : villageois (60) en 2, guerrier (80) ou soldat (100) en 3 |
| Atténuation (`DamageFalloff`) | 100 % jusqu'à 40 m, 55 % à 120 m (`RangeCm` 12 000) |
| Membres (`LimbDamageScale`) | ×1,4 |
| Recul du touché / impulsion physique | 70 cm/s / 9 000 par coup |

### Bande et rechargement
- `BeltCapacity` 100 coups par bande ; `MaxRounds` 300 portés au total ; `StartRounds` 200 (bande pleine + 100).
- **R** : le rechargement long (`Reload`, étiré à `ReloadSeconds` 5 s) ; les cartouches comptent à l'événement `BeltIn` (`ReloadLoadedEvent`) : coupé avant, rien n'est pris dans les caisses ; la bande garde ses coups restants, la réserve la complète.
- Bande vide pendant la rafale : FireEnd, puis rechargement automatique (`bAutoReloadWhenBeltRunsOut`) ; clic sur une bande vide avec réserve : rechargement (`bAutoReloadOnEmptyFire`) ; bande et réserve vides : l'arme est jetée (règle commune `bDiscardWhenEmpty`).
- **Caisse ramassable** `DA_Ammo_MGBox` : 100 cartouches (`EAmmoKind::MachineGunBelt`), versées dans la réserve comme les autres munitions (`AAmmoPickup`).
- La HUD TAPAO affiche « bande · réserve » et le compte à rebours du rechargement (geste `Reload` déjà reconnu).

### Chaleur et surchauffe
- Chaque coup ajoute `HeatPerRound` 0,0135 (1 = surchauffe) ; **pas de refroidissement pendant la rafale** (`CoolingDelaySeconds` 0,35 s après le dernier coup), puis `CoolingPerSecond` 0,18/s, y compris arme rangée (le temps écoulé est rattrapé).
- Tenue sans lâcher : **surchauffe vers 75 coups (≈ 7,5 s), avant la fin de la bande** ; en rafales de 2 s et pauses de 1 s, une bande entière passe sans surchauffe (spec).
- À `OverheatHeat` 1 : tir coupé, geste `Overheat` (événement `SteamVent` : jet de vapeur au bouchon et le long du manchon), son de surchauffe ; l'arme ne retire qu'une fois redescendue à `OverheatRecoverHeat` 0,45 (≈ 3 à 3,5 s).
- **Le canon rougeoie** : au-delà de `GlowStartHeat` 0,45, le paramètre `EmissiveStrength` (`HeatParameterName`) des emplacements de matériau dont le nom contient « Jacket » (`HeatMaterialSlots` : seul le manchon a le masque de lueur de T-272 ; les autres pièces, sans masque, rougiraient en gris) monte jusqu'à `HeatParameterMax` 25 ; une petite lumière orange (`HeatLightIntensity` 2 500, 1,4 m) éclaire les mains.
- **Vapeur** : volutes au bouchon (`SteamCapSocket`) au-dessus de `SteamStartHeat` 0,6 (`NS_MG_Steam` relancé tant que c'est chaud) ; frémissement du manchon (`SteamLoopSound`, volume avec la chaleur).

### Recul et dispersion (« type Call of Duty »)
- **Vue** du porteur : monte de `RecoilPitchPerRound` 0,17° par coup et dérive de côté (marche aléatoire, `RecoilYawPerRound` 0,09°) : le joueur compense ; ×`MovingRecoilScale` 1,6 en mouvement. Le petit coup de vue de la base est coupé (`ViewKickPitch` 0).
- **Secousse caméra** à chaque coup (`UMachineGunFireCameraShake` : saut de 0,35°, roulis, recul de 0,7 cm en 0,09 s), ×1,5 en mouvement.
- **Dispersion** (serveur) : posture (`AimSpreadDegrees` 0,9° immobile, `HipSpreadDegrees` 3° en mouvement) + **floraison** de la rafale (`BloomPerRound` 0,1° par coup jusqu'à `MaxBloomDegrees` 2,5°, ×1,8 en mouvement), qui retombe à 5°/s sans tirer. Le premier coup d'une rafale part avec la dispersion de la posture seule.
- Pas de visée (`bAllowAiming` faux, règle commune « comme dans Counter-Strike »).

### Lumière, éclair, douilles
- Chaque coup : éclair de bouche et lumière ponctuelle de la base (`FlashLightIntensity` 55 000, 7,5 m, `FlashSeconds` 0,045 s) : à 10 coups/s **la lumière papillote** et éclaire les alentours la nuit ; souffle `NS_MG_MuzzleFlame` + `NS_MG_MuzzleSmoke` (légers : un par coup).
- **Douilles** (`SM_MG_Casing`) et **maillons** (`SM_MG_BeltLink`, facultatif) : composants statiques en réserve (24 douilles, 12 maillons par arme, la plus vieille réutilisée), éjectés du socket `socket_eject` vers `socket_eject_dir` (cône de 16°, 260 cm/s + la vitesse du porteur), une trace courte par image et par douille en vol, un rebond, couchées au sol, disparues après 2,5 s ; tintement au premier contact (un toutes les 0,06 s au plus, concurrence `SCON_MG_Brass` 6 voix). Rien au-delà de `EffectsCullDistance`.
- **Caisse lâchée** au rechargement (événement `BoxOut`) : `SM_Ammo_MGBox` tombe du socket `socket_box` et reste 8 s au sol (cosmétique, locale).
- Bande vide : les poches chargées (`LoadedBeltBones` belt_01..16) sont cachées hors rechargement.

### Porter : arme lourde
- En main, le porteur marche à ×0,72 et court à ×0,68 de ses vitesses (`WalkSpeedFactor`, `SprintSpeedFactor`), via `UIslanderMovementComponent::SetHeldItemSpeedFactors` (posé sur toutes les machines : la prédiction du mouvement reste d'accord) ; rendu en rangeant, en jetant ou à la destruction de l'arme. La course est permise (clip `Sprint`), pas le tir en courant.

## Animations (contrat T-272, `scripts/props/machinegun_pack/animation.py`, 30 i/s)
Os `gun_root`, `jacket`, `crank`, `feed_cover`, `cocking_handle`, `steam_cap`, `ammo_box`, `tripod`, `belt_01..16`, `belt_out_01..10` ; sockets en os `socket_*` (`socket_muzzle`, `socket_eject`, `socket_eject_dir`, `socket_steam`, `socket_jacket`, `socket_box`, `socket_grip_r` sur la poignée de manivelle…).

| Geste (`EFirearmGesture`) | Clip | Rôle côté code |
|---|---|---|
| `FireStart` (nouveau) | 17 im., `CrankTick` 0, `Fire` 1/9/13 | joué par le serveur au début de la rafale ; **prédit** par le client du porteur |
| `FireLoop` (nouveau) | 16 im. en boucle, `Fire` 0/4/8/12 | **boucle de port pendant le tir** (`DesiredLoop`), vitesse = 4 coups à la cadence max (×1,33 à 600 coups/min) |
| `FireEnd` (nouveau) | 10 im., `CrankTick` 8 | fin de rafale (prédit par le porteur) ; la manivelle « saute » à sa butée si la rafale s'arrête en cours de tour |
| `Reload` | 150 im., `CoverOpen` 16, `BeltOut` 34, `BoxOut` 64, `BoxIn` 92, `BeltIn` 116, `CoverClose` 126, `Charge` 136 | `BeltIn` charge la bande |
| `Overheat` (nouveau) | 60 im., `SteamVent` 6 | surchauffe |
| `Idle`, `Walk`, `Sprint`, `Equip`, `Holster`, `Inspect`, `DryFire`, `Discard` | longueurs du pack | comme les autres armes |

Les coups visibles (éclair, son, douille, recul) suivent la **cadence**, pas les événements `Fire` des clips (ignorés : la manivelle et la cadence varient) ; l'écart de phase entre le coup de recul animé et l'éclair reste sous 50 ms.

## Réseau (serveur autoritaire)
- Le serveur possède la détente (`ServerFire` existant à l'appui, `ServerCeaseFire` fiable au relâcher), la manivelle, l'horloge des coups, la bande, la réserve, la chaleur et chaque touché.
- État répliqué **compact** : `FMachineGunState` { `BeltRounds` uint16, `Heat` uint8 (0-255), `bFiring`, `bOverheated` } ; `ReserveRounds` au seul porteur ; les gestes passent par l'`Action` répliquée de la base.
- Effets **cosmétiques locaux** : le serveur d'écoute dessine ses vrais coups (`FireRound`) ; les autres machines simulent la même cadence depuis `bFiring` ; le client du porteur tire **tout de suite** à l'appui (sa propre détente, sa manivelle prédite) et s'arrête si le serveur n'a pas confirmé la rafale en 0,75 s. Impacts (multicast non fiable de la base) et traçantes (1 sur 3, multicast de T-276) viennent du serveur.
- Sécurité : la cadence est imposée par le serveur (spammer `ServerFire` ne tire pas plus vite) ; `ServerCeaseFire` ne fait que lâcher la détente.

## Crochets ajoutés à la base (modification minimale, après les traçantes du lead)
- `FirearmRules.h` : `EFirearmGesture` += `FireStart`, `FireLoop`, `FireEnd`, `Overheat` (en fin d'énumération).
- `FirearmWeapon.h/.cpp` : `DesiredLoop()` devient `protected virtual` ; `virtual GetShotSpreadDegrees(bMoving)` (même calcul qu'avant par défaut, utilisé par `FireProjectiles`) ; `PredictGesture(Gesture, ClipStart)` (le bloc de prédiction de `RequestFire` l'appelle, comportement inchangé).
- `AmmoTypes.h` : `EAmmoKind::MachineGunBelt` ; `AmmoPickup.cpp` : « N cartouche(s) de mitrailleuse ».
- `Player/IslanderMovementComponent.h/.cpp` : `SetHeldItemSpeedFactors` / `GetHeldItemSpeedFactor` (multiplicateur dans `GetMaxSpeed`, 1 par défaut).
- Headers modifiés : **build complet éditeur fermé** nécessaire (pas de Live Coding).

## Sons (`scripts/audio/machinegun_prepare.py`, 48 kHz PCM16 mono)
Construits coup par coup pour suivre la manivelle quelle que soit la cadence (pas de boucle de tir figée) :
- `SW_MG_Round_01..04` → `SC_MG_Round` (`FireSound`, volume 0,6 : dix coups par seconde se superposent) : détonation de cartouche de fusil (enregistrements Mosin, SKS, CZ coupés avant leur écho), claquement supersonique, sub, culasse 18 ms après, petite réflexion ;
- `SW_MG_MechanismLoop` (`MechanismLoopSound`, boucle sans couture de 2 s = 20 coups à 600/min : cliquet d'alimentation à chaque coup, rochet de la manivelle, machine, bande) dont la hauteur suit la cadence (0,75 à 450/min) ;
- `SW_MG_BurstTail_01..03` → `SC_MG_BurstTail` (`BurstTailSound` : écho extérieur synthétique, roulement grave, manivelle qui s'arrête), `SW_MG_BurstStart` (embrayage de la manivelle), `SW_MG_CrankTick` (cliquet en butée) ;
- `SW_MG_CasingLand_01..06` → `SC_MG_CasingLand` (douille qui tinte, rebond) ;
- rechargement : `SW_MG_CoverOpen`, `BeltOut`, `BoxOut`, `BoxIn`, `BeltIn`, `CoverClose`, `Charge` ; `SW_MG_Draw` (arme lourde), `SW_MG_DiscardRelease` (jetée) ;
- chaleur : `SW_MG_Overheat` (acier qui tinte, premier sifflement), `SW_MG_SteamVent` (jet de vapeur), `SW_MG_SteamLoop` (frémissement, boucle de 3 s).
Rafale simulée hors moteur (30 coups + boucle + queue) : crête +3,4 dBFS avant le volume 0,6 du cue → ≈ −1 dBFS ; ‑7,9 LUFS. **Jamais écouté dans Unreal.**

## Mise en jeu (`mgsetup`, idempotent, ~25 s de travail au plus par passage)
1. Sons → `/Game/MonkeIsland/Weapons/MachineGun/Audio` (classe `SC_Effects`, atténuations et concurrences de T-259 + `ATT_MG_Mechanism`, `ATT_MG_Brass`, `SCON_MG_Brass`), cues aléatoires.
2. Effets `NS_MG_MuzzleFlame`, `NS_MG_MuzzleSmoke`, `NS_MG_Steam`, `NS_MG_SteamVent` (copies de FountainLightweight comme ceux du fusil ; repli sur ceux du fusil en cas d'échec).
3. `DA_MachineGunRules` (pack T-272 s'il est importé : maillage, bras FP, quatre clips par geste, durées et événements du manifeste, caméra et cadrage FP ; sons, effets, traçantes, douille, maillon, caisse), `BP_MachineGun` (règles, touches du mousquet `IMC_Musket`), `DA_Item_MachineGun`, `DA_Ammo_MGBox`, `BP_Ammo_MGBox`, images si les rendus existent.
4. **Une fois le modèle de T-272 présent seulement** (jamais une arme invisible) : boutique (`DA_ShopCatalog` : mitrailleuse 2 500 couronnes, caisse 250) et `L_Test_Flat` (`Pickup_MachineGun`, `Ammo_MGBox_1/2` à côté des armes de T-249, dossier `WeaponsTest`, tag `T273_MachineGun`).
Rapport : `monke_island/Saved/T273/mgsetup.json`. Images : `render_weapon_icons.py` (Blender en arrière-plan) prend le `SM_Bean_MachineGun.glb` et `SM_Ammo_MGBox.glb` du pack dès qu'ils existent, puis `mgsetup` (ou `weaponicons`) les pose.

## Reste à faire
- **Lead** : build complet éditeur fermé (headers modifiés), lancer la spec `monke_island.Weapons.MachineGun`, puis, après l'import du pack T-272 (`mgimport`), relancer `mgsetup` (complète boutique et carte de test) et, si les GLB existent, `render_weapon_icons.py --only MachineGun,SM_Ammo_MGBox` puis `mgsetup`.
- **PIE (solo puis hôte + client)** : tenir le clic (montée de cadence audible et visible, manivelle FireStart → FireLoop → FireEnd), lâcher (queue d'écho), taper vite (pas plus vite que la cadence), vider une bande (rechargement auto, caisse qui tombe, 5 s, HUD), tenir jusqu'à la surchauffe (≈ 7,5 s : vapeur, sifflement, lueur du manchon, attente ≈ 3 s), recul et dispersion en marche, course (pas de tir), vitesse réduite en main, douilles et tintements, lumière qui papillote de nuit, traçantes 1/3, ramassage de `BP_Ammo_MGBox`, achat en boutique, jet de l'arme vide.
- **Matériau** : vérifier que l'emplacement du manchon contient « Jacket » et que `EmissiveStrength` 25 donne une lueur crédible (régler `HeatParameterMax`).
- **HUD** (UI/, hors périmètre) : jauge de chaleur possible via `AMachineGunWeapon::GetHeat()` / `IsOverheated()`.
- **Base** : la ligne de journal « fires N projectile(s) » de `FireProjectiles` est en `Log` : 10 lignes/s en rafale ; à passer en `Verbose` si gênant (fichier du lead).
