# Nouvelles armes : tromblon, carabine, pistolet double, grenade, munitions ramassables (T-249)

**Lot :** L5 (armes à feu, extension du cahier §5 à arbitrer dans l'ADR-001). Demande du propriétaire du 2026-10-09 : « améliorer le design de toutes les armes, animations de rechargement et de tir type Call of Duty, créer plus d'armes, créer des modèles de munitions, armes plus réalistes », puis « quand l'arme est inutilisable (plus de munitions), jeter l'arme ».
**Statut : compilé le 2026-10-09 (603/604 tests verts, grenade corrigée : FullDamageRadiusCm 150), importé et testé éditeur (6 armes : prise en main, visée, tir, rechargement ; explosion de grenade journalisée ; munitions et pickups sur Test Flat et dans le butin du fort). Pas encore vu : jet de l'arme vide, ramassage de munitions, boutique..** Rien n'est vérifié dans Unreal : ni la compilation, ni l'import des packs (ils sont construits en parallèle par les agents Blender), ni le PIE, ni le réseau. Le lead compile et intègre.
**Recette :** specs `monke_island.Weapons.NewFirearms` (tromblon, carabine, jet de l'arme vide), `monke_island.Weapons.Grenade` (sac, lancer, mèche, souffle), `monke_island.Weapons.Ammo` (place, répartition, refus, sortes), `monke_island.Weapons.PirateFirearms` (pistolet à deux canons). Les IDs de recette du cahier pour les armes restent à créer avec l'ADR-001.

## Vue d'ensemble

| Arme | Classe C++ | Règles (DA) | Pack | Munitions (`EAmmoKind`) |
|---|---|---|---|---|
| Tromblon | `ABlunderbussWeapon : APirateMusketWeapon` | `UBlunderbussRules : UPirateMusketRules` → `DA_BlunderbussRules` | `build/packs/bean-blunderbuss-anim` (24 gestes du mousquet, 24 i/s) | `Shot` (charge de plombs) |
| Carabine rayée | **pas de classe** : `APirateMusketWeapon` | `URifleRules : UPirateMusketRules` → `DA_RifleRules` | `build/packs/bean-rifle-anim` (24 gestes du mousquet, 24 i/s) | `RifleCartridge` |
| Pistolet double | `APistolWeapon` (inchangée, `Barrels` = 2) | `UDoublePistolRules : UPistolRules` → `DA_DoublePistolRules` | `build/packs/bean-pirate-pistol-double` (14 gestes, 30 i/s, sockets `muzzle` et `muzzle2`) | `PistolCharge` |
| Grenade | `AGrenadeWeapon : AFirearmWeapon` + `AGrenadeProjectile` | `UGrenadeRules : UFirearmRules` → `DA_GrenadeRules` | `build/packs/bean-grenade` (13 gestes, 30 i/s) | `Grenade` |
| Munitions | `AAmmoPickup` (`IIslanderInteractable`) | `UAmmoDefinition` → `DA_Ammo_*` | `build/packs/ammo` (`SM_Ammo_*`) | les lots du paquet |

Les quatre armes reprennent la machine à gestes de `AFirearmWeapon` (clips TP/FP synchronisés du pack, serveur autoritaire aux événements du manifeste, voir `pirate-shotgun.md` § Animations). Toutes les valeurs sont dans les DataAssets (`EditDefaultsOnly`, défauts dans les constructeurs C++).

### Carabine : pourquoi pas de classe
Tout ce qui distingue la carabine du mousquet est déjà une valeur de `UPirateMusketRules` : dégâts, portée, dispersion, temps de rechargement, sorte de munition. `URifleRules` n'est qu'un constructeur de défauts (et sa table de clips) ; `BP_Rifle` est un enfant de `APirateMusketWeapon` avec `DA_RifleRules`. Les réactions des tikis sont celles du mousquet (`Cast<APirateMusketWeapon>` dans `ATikiCreature::TakeDamage`).

## Tromblon (`DA_BlunderbussRules`)
| Valeur | Défaut |
|---|---|
| Plombs par charge (`PelletCount`) | 10 |
| Dégâts d'un plomb (`Damage`) | 14, courbe `DamageFalloff` : 100 % jusqu'à 4 m, 30 % à 8 m, 5 % à 18 m (`RangeCm`) |
| Dégâts sur un membre (`LimbDamageScale`) | ×2,5 |
| Dispersion hanche / visée | 6° / 4,5° (le double du fusil à pompe) |
| Éclatement à courte portée (`CloseBurstDistanceCm`) / membres déchiquetés | 4 m / 2 à 3 |
| Charges par joueur (`MaxBalls`, celle du canon comprise) | 10 |
| Rechargement (`ReloadSeconds`, `GroundReload` étiré) | 2,5 s (mousquet : 1,5 s) |
| Recul de la vue | 4° / ±1° |

Tir : la gâchette part à `Trigger`, la charge quitte le canon à `Shot` (`ReleaseShot` → `FireProjectiles(PelletCount)`) : chaque plomb a sa trace, sa distance, ses dégâts et son os ; les plombs d'une même cible font un seul coup (`FFirearmDamageEvent` groupé par os, T-167). `DescribeShot` porte `CloseBurstDistance` et `ShredLimbs` : un tiki tué à 4 m ou moins éclate comme avec le fusil à pompe (le tiki lit ces valeurs dans l'événement). Spec : à 3 m, 6 plombs sur 10 tuent le tiki le plus coriace ; à 8 m, tous les plombs ne tuent pas le plus faible ; égratignures au-delà de 12 m.

## Carabine (`DA_RifleRules`)
| Valeur | Défaut |
|---|---|
| Dégâts / portée | 90 (un guerrier tiki de 80 au corps en un coup) / 160 m, sans atténuation |
| Dispersion hanche / visée | 2,5° / **0,2°** |
| FOV en visée | 50° |
| Cartouches par joueur (`MaxBalls`) | 12 |
| Rechargement (`ReloadSeconds`) | 3,0 s (balle à calepin forcée dans les rayures : deux fois le mousquet) |

## Pistolet double (`DA_DoublePistolRules`, `UPistolRules::Barrels`)
- `Barrels` (défaut 1 ; 2 pour le double) : `FirearmMath::FPistolState` compte désormais `Loaded` (0 à `Barrels`), `bCocked`, `Reserve`, `Barrels`.
- Deux charges chargées au départ ; `Fire` tire le premier canon (`FirearmMath::Fire` rend l'index du canon), le clic suivant tire le second (`bAutoCockOnFire` arme et tire dans la même pression ; sinon `Cock` d'abord). La flamme, la lumière et la fumée du second coup sortent de `SecondMuzzleSocket` (`socket_muzzle2`) : `APistolWeapon::GetShotMuzzleSocket` lit `LastShotBarrel`, répliqué par le serveur et deviné par le porteur pour son tir prédit (`OnShotPredicted`).
- Le rechargement remet les deux canons (`CompleteReload` remplit les canons vides avec ce que la poche contient) ; `R` sur un pistolet à moitié chargé recharge le canon vide.
- Charges par joueur `MaxCharges` 16 ; recul 1,8°.

## Grenade (`DA_GrenadeRules`, `AGrenadeWeapon`, `AGrenadeProjectile`)
| Valeur | Défaut |
|---|---|
| Grenades par joueur (`MaxGrenades`) / au départ (`StartGrenades`) | 6 / 3 (le sac), une en main (`bStartInHand`) |
| Mèche (`FuseSeconds`), minimum après le lancer (`MinFuseAfterThrow`) | 3 s, 0,3 s |
| Lancer court (`Fire`, hanche) / long (`AimFire`, visée) | 900 / 1 600 cm/s, +10° au-dessus de la visée (`ThrowPitchDegrees`), 50 % de la vitesse du porteur |
| Rayon du souffle (`BlastRadiusCm`) / plein dégât (`FullDamageRadiusCm`) | 4 m / 1 m |
| Dégâts au centre (`CenterDamage`) | 90, linéaires jusqu'à 0 au rayon (`GrenadeMath::DamageAtDistance`) |
| Joueurs | `Knockout` (ragdoll) à `KnockoutSpeed` 450 cm/s, lanceur compris ; dégâts seulement si `bFriendlyFire` |
| Corps physiques | `AddRadialImpulse` 60 000 ; tikis et soldats : `LaunchCharacter` au prorata des dégâts |
| Éclatement (`BurstDistanceCm`) / membres | 2 m / 1 à 3 (porté par `FFirearmDamageEvent`) |
| Projectile | sphère de 6 cm, rebonds (`Bounciness` 0,35, `Friction` 0,6), gravité 1, mesh `ProjectileMesh` (le `SM` du pack), `BP_GrenadeProjectile` |
| Explosion | son `ExplosionSound` (`SC_Grenade_Explosion`, T-259 ; rebond `SC_Grenade_Bounce`), Niagara `ExplosionSystems` (flamme et fumée du fusil à pompe, pointées vers le haut, ×2,5), flash `M_MusketFlash` sur une sphère ×2,5, lumière 80 000 / 9 m pendant 0,12 s, décalque `ExplosionDecal` (à fournir : aucun matériau de décalque dans le projet), le projectile reste 2 s pour que l'explosion atteigne chaque machine |

Déroulé (serveur) : `Fire`/`AimFire` jouent **depuis le début** du clip (`FireClipStart` = 0, le moulinet compte) ; à l'événement `Light` la mèche est allumée (`LitTime`), à `Fire` le projectile part (`ThrowGrenade` : origine `ThrowOffset` devant les yeux, balayage pour ne pas traverser un mur, mèche restante = `FuseLeft`). `Reload` = grenade suivante prise à `HammerCocked` (automatique après un lancer, `bAutoTakeNextAfterThrow`). Sac vide : `DryFire`, ou le jet du sac si `bDiscardWhenEmpty`. Un lancer coupé après `Light` (nage, mise à terre) lâche la grenade allumée aux pieds (`DropSpeed`) ; coupé avant, elle revient en main. Cosmétique : la grenade en main disparaît à l'événement `Fire` et revient à `HammerCocked` (`SetWeaponMeshVisible`), l'état répliqué `bInHand` resynchronise hors gestes.
Projectile : `UProjectileMovementComponent` + `USphereComponent` (`BlockAllDynamic`, caméra ignorée, lanceur ignoré en mouvement), `bReplicates` + mouvement répliqué (`PostNetReceiveVelocity` nourrit le mouvement client), `bExploded` répliqué (`OnRep_Exploded` joue les effets, aussi sur l'hôte). Souffle : `OverlapMultiByObjectType(AllDynamicObjects)`, point le plus proche de chaque cible, ligne de vue (`bBlastNeedsLineOfSight`), un `FFirearmDamageEvent` par cible (os le plus proche de la ligne souffle → cible, `BoneHits`, `Impacts`, `HitInfo.Distance`) : les soldats du fort et le crabe le lisent comme un tir (`bShot`, distance → clip d'explosion des soldats). **Limite** : `ATikiCreature::TakeDamage` ne reconnaît que mousquet/pistolet/fusil par `Cast` (Creatures/* hors périmètre) : les tikis prennent les dégâts et meurent, sans clip de réaction ni sang pour la grenade — à ajouter côté Creatures (traiter `AGrenadeWeapon`/`AFirearmWeapon` comme `Musket`).

## Munitions ramassables (`AAmmoPickup`, `UAmmoDefinition`, `EAmmoKind`)
- `EAmmoKind` : `None, MusketBall, PistolCharge, Shell, Shot, RifleCartridge, Grenade` ; chaque règle d'arme porte `AmmoKind` (mousquet `MusketBall`, pistolets `PistolCharge`, fusil à pompe `Shell`, tromblon `Shot`, carabine `RifleCartridge`, grenade `Grenade`).
- `UAmmoDefinition` (`DA_Ammo_*`) : nom, `Lots` (`FAmmoLot` sorte + nombre ; une caisse en a plusieurs), mesh et masse du paquet au sol.
- `AAmmoPickup` : corps physique comme `AInventoryPickup` (profil PhysicsActor, pions et caméra ignorés, Visibility bloqué), `Definition` répliquée, `Remaining` par lot répliqué, `bTaken`. **F** : le serveur (`Interact`, après les contrôles de portée et de vue du grab) verse chaque lot dans les armes du porteur de cette sorte, l'arme en main d'abord (`UIslanderInventoryComponent::GiveAmmo` → `AmmoMath::Distribute` → `AFirearmWeapon::AddAmmo`) ; le reste reste dans le paquet ; vidé, il disparaît après `DestroyDelay`.
- Refus : sans arme de cette sorte, le prompt du réticule dit déjà « *Giberne : aucune arme pour ces munitions* » (c'est le message HUD court, affiché tant qu'on vise le paquet) ; poches pleines : « *Giberne : poche pleine* ». Sur F, le serveur envoie en plus `UIslanderInventoryComponent::ClientNotice` → délégué statique `OnNotice(Inventory, FText)` que le HUD TAPAO peut afficher (non branché : `UI/` hors périmètre).
- API des armes : `GetAmmoKind()`, `GetAmmo(kind)`, `GetAmmoRoom()`, `AddAmmo(kind, count)` (serveur, borné par `GetMaxRounds()` : `MaxCharges`, `MaxShells`, `MaxBalls`, `MaxGrenades`), `SetRounds`. Inventaire : `CollectFirearmsForAmmo`, `HasFirearmFor`, `GetAmmoRoom`, `GiveAmmo`, `Notice`.
- Magasin : `FShopOffer::Ammo` (`DA_Ammo_*`, `Item` vide) : `AShopkeeperActor::Sell` accepte la ligne si le joueur porte une arme de la sorte avec de la place (sinon `InventoryFull`), paie puis verse les lots (remboursé si rien n'entre). L'écran TAPAO affiche « ? » comme nom pour une ligne sans `Item` : afficher `Offer.Ammo->DisplayName` est une ligne à ajouter dans `STapaoShop.cpp` (UI/).
- Butin des forts : `BP_Ammo_*` sont des `AActor` valables pour `FLootEntry::ItemClass` ; le setup ajoute les caisses, gibernes, boîtes et cornes à `DA_FortLoot` (poids 0,3 à 0,7).

### Paquets livrés par le setup (`AMMO_DEFS` de `setup_pirate_weapons.py`, à ajuster aux `SM_Ammo_*` réels)
| `DA_Ammo_` | Nom | Lots | Prix | Butin fort |
|---|---|---|---|---|
| Cartridges | Cartouches de papier | 6 balles de mousquet | 40 | – |
| CartridgeBox | Giberne | 15 balles de mousquet | 90 | 0,7 |
| PowderHorn | Corne à poudre | 8 charges de pistolet | 50 | 0,5 |
| Pouch | Bourse de charges | 6 charges de pistolet | 40 | – |
| RifleCartridges | Cartouches de carabine | 6 cartouches | 60 | – |
| ShellBox | Boîte de cartouches | 8 cartouches de fusil à pompe | 70 | 0,5 |
| ShotBag | Sachet de plombs | 5 charges de plombs | 45 | – |
| GrenadeBag | Sac de grenades | 3 grenades | 150 | – |
| Crate | Caisse de munitions | 10 balles, 10 charges, 6 cartouches, 4 cartouches de carabine | – | 0,5 |
| Barrel | Baril de poudre | 15 balles, 15 charges, 8 plombs, 8 cartouches de carabine | – | 0,3 |

Le mesh de chaque paquet est choisi par mots-clés dans `/Game/MonkeIsland/Weapons/Ammo/Meshes` (le plus long mot-clé qui apparaît dans le nom gagne, un mesh ne sert qu'une fois) ; un paquet sans mesh garde une boîte grise et un avertissement dans le rapport.

## Arme vide : jet de l'arme (propriétaire 2026-10-09)
- Règle pure `FirearmMath::ShouldDiscard(Loaded, Reserve, bBusy, bDiscardWhenEmpty)` : sur la gâchette, rien de chargé **et** poche/sac à zéro, aucun geste en cours → l'arme est jetée (avant la logique propre à chaque arme, dans `AFirearmWeapon::ServerFire_Implementation`). `UFirearmRules::bDiscardWhenEmpty` (défaut vrai) le désactive ; **l'ancien retrait automatique de l'arme après la dernière balle (`CheckDepleted`, `DiscardDelay`) est supprimé** : l'arme vide reste en main jusqu'au clic.
- Geste `EFirearmGesture::Discard` (clip `Discard` TP/FP des packs v2, ≈ 1,2-1,3 s, événements `Empty` au début et `Release` à l'image où l'arme quitte la main). À `Release` (`DiscardReleaseEvent`), le serveur lâche l'arme : `UIslanderInventoryComponent::DropSlot(slot, tout, vitesse)` avec `FirearmMath::DiscardToss(droite, DiscardTossSpeed 350 cm/s, DiscardTossPitchDegrees 30° vers le bas)` + la vitesse du joueur ; le pickup garde l'acteur et ses compteurs (vide) et la barre passe aux mains nues comme après `RequestDrop`. Pack sans clip `Discard` : `DryFire` joue, l'arme est lâchée à sa fin (`bDiscardPending`) ; sans `DryFire` non plus, lâchée aussitôt.
- Tant que `Discard` joue, `IsBusy()` refuse tir, rechargement et inspection, et la visée est refusée (`IsDiscarding()`). Un geste coupé (ranger, nager, mise à terre) garde l'arme vide en main.
- Ramassée puis réapprovisionnée (`AAmmoPickup`), l'arme tire à nouveau.

## Pipeline (scripts, éditeur fermé, verrou pris)
1. `python scripts/character/pirate_weapon_clips.py` : table `Weapons/PirateWeaponClips.inl` (registre `WEAPONS` : Pistol, DoublePistol, Shotgun, Musket, Blunderbuss, Rifle, Grenade ; `build/packs/<pack>` d'abord, OneDrive en repli pour pistolet et fusil ; **repli** sur la table du mousquet pour tromblon/carabine, du pistolet pour le double, table provisoire pour la grenade tant que le pack n'est pas construit ; `Discard` pris s'il est dans le manifeste ; noms de gestes lus dans `FirearmRules.h`). `--check` dit quelles tables sont en repli.
2. `python scripts/character/import_pirate_weapons.py --check` puis le commandlet : bases, bras FP, clips, statiques (noms lus dans le manifeste), **matériaux PBR** : pour chaque `manifest.materials[<nom>].textures` (labels `BaseColor`, `Normal` DirectX → pas de retournement du vert, `ORM`), textures `T_<Arme>_<Mat>_{BaseColor,Normal,ORM}` et `MI_<Arme>_<Mat>` dérivées de `M_Character_Baked` (`import_coco_real_ue.master_material`), assignées à tous les meshes importés par `imported_material_slot_name` ; pack `ammo` → `Ammo/Meshes/SM_Ammo_*` + matériaux. Filtre `MONKE_PIRATE_WEAPONS=…,Ammo` ; un pack absent est sauté.
3. `python scripts/character/setup_pirate_weapons.py --check` puis le commandlet : `DA_<Asset>Rules`, `BP_<Asset>`, `DA_Item_<Asset>` (mesh de ramassage), clips branchés (gestes manquants ajoutés au DA, durées et événements réécrits depuis le manifeste pour tous les packs), caméra FP, cadrage hanche, sons provisoires, sockets de bouche (`SecondMuzzleSocket` du double), douille (fusil), `BP_GrenadeProjectile` + mesh + effets d'explosion (grenade) ; `DA_Ammo_*` + `BP_Ammo_*` ; lignes du magasin (armes et munitions, ajoutées si absentes) ; butin du fort. `MONKE_WEAPONS_NO_SHOP=1` / `MONKE_WEAPONS_NO_LOOT=1`.
4. Carte de test (éditeur ouvert, verrou, sans PIE) : `scripts/editor/weapons_test_map_ue.py` pose sur `L_Test_Flat`, devant le PlayerStart, un `AInventoryPickup` par arme (nouvelles + pistolet, fusil, mousquet) et un `BP_Ammo_*` par paquet, dossier `WeaponsTest`, tag `T249_Weapons`.

```powershell
python scripts/character/pirate_weapon_clips.py
python scripts/character/import_pirate_weapons.py --check
& "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/matth/monk-island/monke_island/monke_island.uproject" -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/import_pirate_weapons.py" -unattended -nosplash -nullrhi -nosourcecontrol
& "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/matth/monk-island/monke_island/monke_island.uproject" -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/setup_pirate_weapons.py" -unattended -nosplash -nullrhi -nosourcecontrol
```

## Fichiers
- `Weapons/AmmoTypes.h` (`EAmmoKind`, `FAmmoLot`, `AmmoMath`), `AmmoDefinition.h`, `AmmoPickup.h/.cpp`
- `Weapons/FirearmRules.h` (`Discard`, `AmmoKind`, `bDiscardWhenEmpty`, `DiscardTossSpeed`, `DiscardTossPitchDegrees`, `DiscardReleaseEvent`), `FirearmMath.h` (`ShouldDiscard`, `DiscardToss`, `FPistolState` à canons), `FirearmWeapon.h/.cpp` (jet de l'arme, munitions, `GetShotMuzzleSocket`, `OnShotPredicted`, `SetWeaponMeshVisible`)
- `Weapons/PistolRules.h/.cpp` (`Barrels`, `SecondMuzzleSocket`, `UDoublePistolRules`), `PistolWeapon.h/.cpp` (`LoadedCharges`, `LastShotBarrel`)
- `Weapons/PirateMusketWeapon.h/.cpp` (`GetProjectilesPerShot`, `GetMaxRounds`), `BlunderbussRules.h/.cpp`, `BlunderbussWeapon.h/.cpp`, `RifleRules.h/.cpp`
- `Weapons/GrenadeMath.h`, `GrenadeRules.h/.cpp`, `GrenadeProjectile.h/.cpp`, `GrenadeWeapon.h/.cpp`
- `Weapons/IslanderInventoryComponent.h/.cpp` (`FindSlotOfItem`, `DropSlot` avec vitesse, `CollectFirearmsForAmmo`, `GiveAmmo`, `Notice`/`ClientNotice`/`OnNotice`), `ShotgunWeapon`/`ShotgunRules`/`PirateMusketRules` (`AmmoKind`, `GetMaxRounds`)
- `Shop/ShopTypes.h` (`FShopOffer::Ammo`), `Shop/ShopkeeperActor.cpp` (lignes de munitions)
- Tests : `Weapons/Tests/PirateFirearmSpec.cpp` (mis à jour), `NewFirearmsSpec.cpp`, `GrenadeSpec.cpp`, `AmmoSpec.cpp` ; `PirateMusketSpec.cpp` tolère le clip `Discard`
- Scripts : `scripts/character/pirate_weapon_clips.py`, `import_pirate_weapons.py`, `setup_pirate_weapons.py`, `scripts/editor/weapons_test_map_ue.py`

## Reste à faire (éditeur, intégrateur)
- Compiler (éditeur fermé) ; relancer `pirate_weapon_clips.py` quand chaque pack arrive dans `build/packs` (les tables de repli disparaissent), puis l'import et le setup.
- Vérifier dans les `DA_*Rules` : sockets de bouche (`socket_muzzle`, `socket_muzzle2`, `socket_fuse`), cadrage FP (`HipGripLocation`, `AimMuzzleLocation`) de chaque nouvelle arme, `FirstPersonAlignBone` (os `root` ou `gun_root` selon le pack).
- Grenade : fournir un matériau de décalque de brûlure (`ExplosionDecal`) ; régler `ThrowOffset` pour que la grenade parte de la main (sons d'explosion et de rebond : T-259, section Audio).
- `STapaoShop.cpp` : nom des lignes de munitions (`Offer.Ammo->DisplayName`) ; HUD : afficher `UIslanderInventoryComponent::OnNotice`.
- `ATikiCreature::TakeDamage` : reconnaître la grenade (réactions, sang).
- PIE à vérifier : tir du tromblon (10 plombs, éclatement à 4 m), carabine (un coup à 90 à 100 m), pistolet double (deux coups, flamme au second canon, rechargement des deux), grenade (moulinet, mèche, rebonds, explosion, ragdoll des joueurs, dégâts aux soldats), munitions (ramassage, refus sans arme, poche pleine, reste dans le paquet), jet de l'arme vide (clip `Discard` ou `DryFire`, pickup au sol, mains nues, re-ramassage + munitions), magasin (ligne de munitions), butin du fort (caisses) ; hôte + client.

## Visée désactivée (owner 2026-10-09, « comme dans Counter-Strike »)
- `UFirearmRules.bAllowAiming` (défaut **faux**) : le clic droit ne vise plus avec aucune arme à feu ; `SetAiming` est ignoré, le réticule sert de mire.
- La précision vient de la posture : immobile (vitesse < `SteadySpeedCmps`, 60 cm/s) le tir utilise `AimSpreadDegrees` ; en mouvement `HipSpreadDegrees` (`FirearmMath::SpreadForStance`, spec `monke_island.Weapons.Stance`).
- Les clips `AimIn/AimIdle/AimFire/AimOut` restent importés (réactivables par le DA).

## Audio (T-259, 2026-10-09) — statut : importé et câblé dans l'éditeur (propriétés relues), non écouté en PIE
Demande du propriétaire : « audio nouveau » pour les nouvelles armes, « type Call of Duty » sur des armes à silex. Fiche son complète (événement par événement, sources, atténuations, mixage) : `docs/audio/armes-v2.md`.
- **Tirs** (`FireSound`, une `SoundCue` à 3 variantes tirées sans répétition par arme) : enregistrements CC0 de vraies armes à poudre noire (mousquet, carabine à silex, pistolet), couches empilées comme un tir moderne : claquement, corps, sub synthétique, écho du lieu ; tromblon avec grêle de plombs, fusil à pompe plus serré. Le pistolet fait entendre l'éclair du bassinet 30 ms avant la détonation ; les armes longues jouent silex et bassinet à `Trigger`, 125 ms avant `Shot`.
- **Mécanique** (`EventSounds`, tous les événements des clips ont un son) : chien (demi-armé, armé, chute avec étincelles), batterie, amorçage, poudre, balle et calepin, baguette (sortie, retournement, deux coups, retour), plombs du tromblon, boîte à calepins de la carabine, cartouche de papier des pistolets, pompe, douille, cartouches insérées (3 variantes), tir à vide, sortir/prêt/ranger, arme vide jetée (`Empty`, `Release`, chute au sol incluse).
- **Grenade** : allumage de la mèche (`Light`), lancer (`Fire`), sac, rebond (`BounceSound`, 3 variantes), explosion (`ExplosionSound`, 3 variantes, gravats et écho).
- **Données** : `EventSoundAttenuation` = `ATT_Firearm_Handling` ; tirs `ATT_Firearm_Shot` (audibles à 300 m, assourdis au loin), explosion `ATT_Grenade_Explosion` ; concurrence `SCON_Firearm_*` ; classe `SC_Effects`. Script `scripts/audio/weapons_audio_setup_ue.py` (action `weaponsaudio`), à relancer après `setup_pirate_weapons.py` (qui remet les sons provisoires du mousquet).
- **Sans champ C++** (sons prêts, non branchés) : ramassage de munitions (`SW_Ammo_Pickup`), mèche de la grenade en vol (`SW_Grenade_FuseLoop`), arme qui touche le sol ou ramassée.
- **Sabre** : `SwingSound`, `FinisherSwingSound`, `HitSound` et `KillSound` de `DA_CutlassRules` prennent les sons du propriétaire (OneDrive `epe_*`), recalés sur les fenêtres actives des coups.

## Images des armes et des munitions (2026-10-09, T-264) — statut : importé dans l'éditeur, compilé (fichier seul), non vu en jeu
Demande du propriétaire : « ajoute bien la modélisation dans inventaire et boutique des modèles de nouvelles armes ».
- **Images** : rendus des modèles mêmes du jeu (meshes de ramassage des packs, GLB avec leurs textures cuites), fond transparent, 512 × 512, ombre douce : `scripts/ui/render_weapon_icons.py` (Blender en arrière-plan) puis `scripts/ui/finish_weapon_icons.py` (ombre, planche `art/ui/icons/weapons/_sheet.png`). Armes longues en diagonale (crosse en bas à gauche, bouche en haut à droite), pistolets de profil (le double vu de plus haut : deux canons, deux platines), paquets et boîtes de trois quarts vus d'en haut (contenu visible).
- **Unreal** (action `weaponicons` de la session lead, `scripts/editor/weapon_icons_ue.py`) : textures d'interface `T_Icon_*` dans `/Game/MonkeIsland/UI/Icons/Weapons` (sans mips, groupe UI) ; `Icon` des `DA_Item_*` (mousquet, pistolet, pistolet double, fusil à pompe, tromblon, carabine, grenade) et des 10 `DA_Ammo_*` ; le mousquet de la boutique (`DA_Item_Musket`) tombe au sol avec le modèle v2 ; 7 paquets de munitions qui n'avaient pas de modèle au sol (`PickupMesh` vide : une boîte provisoire) ont maintenant le leur ; lignes de munitions du catalogue décrites en français (« Giberne : 15 balles de mousquet »).
- **Boutique** (`UI/Tapao/STapaoShop.cpp`) : une offre de munitions montre l'image et le nom du paquet (avant : « ? » sans image) ; message d'achat adapté (« les munitions sont dans tes armes ») et refus expliqué (aucune arme ne les prend, ou pleines).

## Balles traçantes (2026-10-10, T-276) — statut : compilé (fichier seul), réglages après le build, non vu en jeu
Demande du propriétaire : « crée et ajoute les projectiles des balles des différentes armes, les traces des balles comme des munitions traçantes ».
- **Principe** : les tirs restent instantanés sur le serveur ; chaque machine dessine pour chaque balle une **traçante** : la balle (la balle de plomb des packs) et une **traînée lumineuse** derrière elle, qui part de la bouche que cette machine voit (arme 1re personne pour le tireur, 3e personne pour les autres) et file jusqu'au point touché ou jusqu'au bout de la portée dessinée. `AFirearmTracer` (`Weapons/FirearmTracer.*`), réglages `FFirearmTracerLook` : `EveryNth` (une balle sur N), vitesse dessinée (plus lente qu'une vraie balle pour que l'œil la suive), longueur et épaisseur de la traînée, couleur et intensité (données primitives personnalisées : aucune instance de matériau par balle), mesh de la balle et échelle.
- **Réseau** : le serveur envoie les points d'arrivée des balles traçantes d'un tir par un multicast non fiable (`MulticastShotTracers`) ; une perte de paquet ne perd que le dessin.
- **Armes** : `UFirearmRules.Tracer` (mousquet pirate, pistolets, carabine plus rapide, plombs du fusil à pompe et du tromblon plus fins et plus nombreux) ; **soldats français** : `UFortSoldierRules.MusketTracer` (rougeâtre), du canon de leur fusil au premier obstacle sur la ligne de tir.
- **Assets** : `M_Tracer` (non éclairé, additif, double face : couleur et intensité lues dans les données primitives 0-3), traînée `/Engine/BasicShapes/Cylinder`, balle `SM_Ammo_LeadBall` ; action `tracers` (`scripts/editor/tracer_setup_ue.py`).
- **Tests** : `monke_island.Weapons.Tracer` (une balle sur N, fin du vol, traînée qui sort de la bouche).
