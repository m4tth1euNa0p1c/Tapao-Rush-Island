# Armes d'époque : fusil à verrou à lunette, mitraillette, mousquet à baïonnette (T-335 ; modèles et clips T-334)

**Lot :** L5 (armes à feu, extension du cahier §5 à arbitrer dans l'ADR-001, comme `armes-nouvelles.md`). Demande du propriétaire du 2026-10-11 : « crée plein d'autres armes de cette époque, modélise toutes les actions, pompe, sniper verrou, mitraillette, fusil mousquet avec baïonnette et utilisation de baïonnette […] ajoute toutes ces features à flat test ».
**Recette :** specs `monke_island.Weapons.BoltRifle`, `monke_island.Weapons.Mitraillette`, `monke_island.Weapons.Bayonet`, et `monke_island.Weapons.PirateFirearms` (deux cas ajoutés pour le fusil à pompe). Pas encore d'ID de recette du cahier (à créer avec l'ADR-001).

**Statut (2026-10-11, honnête) :** code **compilé** fichier par fichier (UBT `-SingleFile`, cible `monke_islandEditor` Development, sans édition de liens : « Result: Succeeded », 0 erreur, 0 avertissement), y compris les fichiers qui dépendent des en-têtes modifiés hors du dossier (`TikiCreature.cpp`, `STapaoHUD.cpp`, `ShopCatalog.cpp`). Les specs sont écrites et compilées, **pas exécutées** (pas de DLL liée) ; leurs attentes chiffrées de la mitraillette ont été vérifiées par un portage Python de la simulation (22 coups en 2 s à 30, 60 et 144 i/s, tambour vidé en 4,47 s, 21 coups en tapotant à 144 i/s). Script éditeur **non lancé** (éditeur occupé par le propriétaire). Rien n'est **testé en éditeur**, **testé build** ni **testé multi-machines**. Packs de T-334 au moment de l'écriture : `BayonetMusket` construit (`status: built`), `BoltRifle` et `Mitraillette` encore provisoires (le contrat de noms, de clips et d'événements est fixé, les fichiers pas encore écrits).

## Vue d'ensemble

| Arme | Classe C++ | Règles (DA) | Munition (`EAmmoKind`) | Pack (T-334) |
|---|---|---|---|---|
| Fusil à verrou à lunette | `ABoltRifleWeapon : AFirearmWeapon` → `BP_BoltRifle` | `UBoltRifleRules` → `DA_BoltRifleRules` | `RifleCartridge` (celle de la carabine) | `art/export/Weapons/BoltRifle/pack.json` (30 i/s) |
| Mitraillette à tambour | **pas de classe** : `AMachineGunWeapon` → `BP_Mitraillette` | `USubmachineGunRules : UMachineGunRules` → `DA_MitrailletteRules` | `SmgCartridge` (nouvelle) | `art/export/Weapons/Mitraillette/pack.json` (30 i/s) |
| Mousquet à baïonnette | `ABayonetMusketWeapon : APirateMusketWeapon` → `BP_BayonetMusket` | `UBayonetMusketRules : UPirateMusketRules` → `DA_BayonetMusketRules` | `MusketBall` | `art/export/Weapons/BayonetMusket/pack.json` (24 i/s) |

Logique pure testée : `Weapons/BoltRifleMath.h`, `Weapons/BayonetMath.h`, `MachineGunMath` (mitraillette), `FirearmMath` (fusil à pompe). Toutes les valeurs sont dans les DataAssets (défauts des constructeurs C++), jamais en dur.

## Touches
Celles des armes à feu (`IMC_Musket`, priorité 2) : **clic gauche** tire, **R** recharge, **I** inspecte.
- **Fusil à verrou :** **clic droit maintenu** = lunette (exception à la règle « pas de visée » de T-254 : `bAllowAiming` vrai dans `DA_BoltRifleRules`). R sur un fusil plein dont la douille est restée dans la chambre manœuvre la culasse.
- **Mitraillette :** clic gauche **maintenu** = rafale ; pas de visée.
- **Mousquet à baïonnette :** **clic droit = coup de baïonnette** ; **en courant (Maj) + clic droit = charge**. Choix : le clic droit, libre puisque le mousquet ne vise pas (`bAllowAiming` faux) ; **V** n'a pas été pris parce qu'il sert encore à la vue et au micro (`IMC_Voice`, `IA_PushToTalk` : parler déclencherait la baïonnette). `ABayonetMusketWeapon.ThrustAction` permet une touche dédiée plus tard (vide = clic droit).

## Fusil à verrou (`DA_BoltRifleRules`)
| Valeur | Défaut |
|---|---|
| Coups dans le fusil (`Capacity`, chambre comprise) / portés (`MaxRounds`) / au départ (`StartLoadedRounds`) | 5 / 25 / 5 (le premier chambré) |
| Dégâts / portée | 150 (un soldat de 100 ou un guerrier de 80 en un coup au corps) / 300 m, sans atténuation ; `LimbDamageScale` 1,2 ; os à 12 cm |
| Dispersion : lunette et immobile (`AimSpreadDegrees`) / sans lunette, immobile (`NoScopeSpreadDegrees`) / en mouvement (`HipSpreadDegrees`, lunette ou non) | 0,05° / 1,5° / 4° |
| Lunette : champ (`AimFieldOfView`) / vitesse du cadrage / oculaire (`AimSightSocket`) / position de l'oculaire (`AimMuzzleLocation`) | 22° (≈ ×4 depuis 90°) / 9 /s / `socket_scope_eye` / 14 cm devant l'œil, sur l'axe |
| Arme cachée derrière la lunette (`bHideFirstPersonWhenAimed`, à `AimHideAlpha`) | oui, à 92 % du cadrage |
| Culasse automatique après le tir (`bAutoCycleBolt`) / la lunette se baisse pendant la culasse (`bBoltCycleLowersScope`) | oui / oui |
| Rechargement automatique après le dernier coup / au clic à vide | oui / oui |
| Recul de la vue | 4,5° / ±0,9° ; traçante à chaque coup (32 000 cm/s) |

Déroulé (serveur, `BoltRifleMath`) : un coup chambré part (`Fire`, ou `AimFire` à la lunette quand le pack l'a) ; la douille reste dans la chambre ; à la fin du clip de tir, `BoltCycle` joue (`BoltOpen`, `CaseEject` : douille éjectée de `socket_eject`, `BoltClose` : le coup suivant est chambré). **Aucun tir avant `BoltClose`** ; après, un clic coupe la fin du clip et tire aussitôt (de même après le `BoltClose` du rechargement). Fusil vide : `Reload` à la lame-chargeur (`BoltOpen`, `ClipIn`, **`RoundsIn` : les cinq coups comptent ici** (`ReloadLoadedEvent`), `ClipEject` : la lame vide part de `socket_clip`, `BoltClose`). Un geste coupé (ranger, nager) garde ce qui est fait : culasse coupée avant `BoltClose` → le clic suivant la manœuvre ; rechargement coupé avant `RoundsIn` → rien n'est pris dans la poche.

**Lunette :** le cadrage de visée de la base place soit la bouche (cadrage `aim_*` du pack, que le script reprend tel quel quand il existe : il reproduit la caméra d'auteur, œil sur l'axe de la lunette), soit l'oculaire `AimSightSocket` (nouveau champ de `UFirearmRules`) sur l'axe de la vue. Une fois la lunette à l'œil, l'arme et les bras 1re personne sont cachés (`IsScopeView()` vrai : un HUD peut y dessiner l'anneau de la lunette) et le champ se resserre ; la flamme et le souffle de bouche ne sont pas dessinés devant l'œil du tireur (les autres les voient). **Pas de voile de lunette dans le HUD** : `UI/` est hors du périmètre de T-335 ; le champ serré et l'arme cachée suffisent pour viser, le réticule du HUD sert de mire.

## Mitraillette (`DA_MitrailletteRules`, logique de la mitrailleuse)
| Valeur | Défaut |
|---|---|
| Tambour (`BeltCapacity`) / portés (`MaxRounds`) / au départ (`StartRounds`) | 50 / 250 / 150 (tambour plein + deux) |
| Cadence (`MinCadenceRpm` = `MaxCadenceRpm`, pas de manivelle à lancer) | 660 coups/min = 11/s dès le premier coup ; `FireLoop` du pack = 11 coups en 1 s (`RoundsPerCrankTurn` 11, joué à vitesse 1) |
| Dégâts / portée | 22 (cartouche de pistolet : villageois en 3, guerrier en 4, soldat en 5) ; 100 % jusqu'à 15 m, 40 % à 60 m |
| Dispersion immobile / en mouvement / floraison par coup (max) | 1,2° / 3,5° / 0,18° (3,5°), retombe à 7°/s |
| Recul de la vue par coup / secousse | 0,32° vers le haut, dérive 0,14° ; secousse de la mitrailleuse ×0,45 |
| Chaleur / ralentissement | aucune (`HeatPerRound` 0) / aucun (×1) |
| Course | pas de tir en courant (`bCanFireWhileSprinting` faux) |

Tambour non vide : `Reload` (`MagOut`, `MagIn` : les coups comptent, `Charge`) ; **tambour vide : `ReloadEmpty`** (`MagDrop` : le tambour vide tombe au sol, mesh `SM_Ammo_MitrailletteDrum`), ajout générique à `AMachineGunWeapon` (sans effet sur la mitrailleuse, qui n'a pas ce clip). Douilles `SM_Mitraillette_Casing` à chaque coup de `socket_eject` vers `socket_eject_dir`, traçante un coup sur quatre. Réseau, détente, prédiction et sécurité : ceux de la mitrailleuse (`mitrailleuse.md`).

## Mousquet à baïonnette (`DA_BayonetMusketRules`)
Le tir et le rechargement sont ceux du mousquet pirate, inchangés (le script recopie les valeurs réglées de `DA_PirateMusketRules`).

| Valeur | Coup (`Stab`, `BayonetStab`) | Charge (`Charge`, `BayonetCharge`) |
|---|---|---|
| Dégâts | 55 | 110 |
| Portée du cône (centre du porteur → point le plus proche du corps visé) / demi-angle | 150 cm / 30° | 190 cm / 25° |
| Cibles au plus | 1 | 2 |
| Endurance (part de `MaxStamina`) | 12 % | 25 % |
| Fente (root motion) | 40 cm en 0,18 s | 160 cm en 0,3 s, vitesse plafonnée à 600 cm/s à la fin |
| Recul de la cible / impulsion physique | 220 (+50 vers le haut) / 30 000 | 520 (+90) / 60 000 |
| Événement du coup | `StabHit` | `ChargeHit` |

- **Quand :** porté, en inspection ou **pendant le rechargement (coupé, la balle reste dans la poche ; `bThrustCutsReload`)** ; jamais pendant la sortie, un tir, une autre poussée ou le jet. La charge exige de courir (sprint) ou d'aller à 450 cm/s au moins (`ChargeMinSpeedCmps` ; le serveur accepte 75 % de cette vitesse, sinon il donne un coup simple).
- **Le coup (serveur, à l'événement) :** chevauchement sphérique autour du porteur (`HitObjectTypes` : Pawn, PhysicsBody), point le plus proche de chaque corps, cône horizontal ±140 cm en hauteur (`BayonetMath::ThrustScore`), les vivants d'abord puis le plus en face ; ligne libre des yeux au point (`SightChannel`, pas à travers un mur) ; **dégâts comme le sabre** : `UGameplayStatics::ApplyPointDamage` avec l'arme pour cause, l'os le plus proche de la ligne du coup (36 cm) : Veilleurs et tikis, soldats français et crabe géant le prennent. Pas de tir ami (`bFriendlyFire`).
- **Retours :** son du coup ou de la mise à mort au point (multicast non fiable ; sons du sabre `DA_CutlassRules` posés par le script), secousse de caméra du porteur (`UCutlassHitCameraShake`), sifflement à `StabStart` / `ChargeStart`.
- **Réseau :** le porteur joue son geste, sa fente et dépense son endurance tout de suite ; le serveur revérifie tout (utilisable, geste en cours, endurance, vitesse de la charge) et décide des cibles ; refus → `ClientRejectThrust` (geste et fente repris, endurance et position corrigées par le mouvement). Une seule poussée par geste : une rafale de requêtes est refusée tant qu'un geste joue.

## Fusil à pompe : vérification (`AShotgunWeapon`)
Les gestes `FirePump` (= `Fire` / `AimFire` du pack), `ReloadStart`, `ReloadInsert`, `ReloadEnd`, `ReloadEmpty` ont leurs clips et leurs événements (`InsertSlug`, `EjectSpentShell`, la pompe) ; trois manques corrigés côté logique :
1. **`ReloadEmpty` montrait trois cartouches même quand la poche en avait moins** : une arme vide qui ne peut pas en recevoir autant que le clip en montre se recharge cartouche par cartouche (`bShellByShellWhenShort`, `FirearmMath::FirstReloadPhase` avec `MinEmptyShells`).
2. **Après `ReloadEmpty` (3 cartouches), une arme qui en tient plus restait incomplète** : le rechargement continue `ReloadStart` → `ReloadInsert` → `ReloadEnd` jusqu'au plein (`bTopUpAfterEmptyReload`, `NextReloadPhase(..., bTopUpAfterEmpty)`).
3. **R pendant la pompe était perdu** : il est retenu et le rechargement suit la pompe.
Sans clip `ReloadEmpty` dans le pack, une arme vide se recharge cartouche par cartouche (avant : aucun rechargement).

## Changements de la base (seul agent sur ces fichiers dans la vague)
- `FirearmRules.h` : `EFirearmGesture` += `BoltCycle`, `BayonetStab`, `BayonetCharge` (en fin d'énumération) ; `AimSightSocket` ; `bHideFirstPersonWhenAimed`, `AimHideAlpha` ; `AimFieldOfView` borné à 5° au lieu de 20°.
- `FirearmWeapon.h/.cpp` : point de visée = `AimSightSocket` (os du pack ou socket du mesh) sinon la bouche ; vue de lunette (`SetScopeView`, `IsScopeView`) ; `CancelPredictedGesture`, `GetLocalGesture` (pour la baïonnette). Lissage et hystérésis des boucles de port de T-330 conservés tels quels.
- `AmmoTypes.h` : `EAmmoKind::SmgCartridge` ; `AmmoPickup.cpp` : « N cartouche(s) de mitraillette ».
- `MachineGunWeapon.cpp` : `ReloadEmpty` optionnel (voir mitraillette).
- **En-têtes réfléchis modifiés** : `FirearmRules.h`, `FirearmWeapon.h`, `AmmoTypes.h`, `ShotgunRules.h`, `ShotgunWeapon.h` et les nouveaux `BoltRifleRules.h`, `BoltRifleWeapon.h`, `SubmachineGunRules.h`, `BayonetMusketRules.h`, `BayonetMusketWeapon.h` : **build complet éditeur fermé** (pas de Live Coding).

## Mise en jeu : `scripts/editor/new_weapons_ue.py` (action « newweapons », à ajouter à `ACTIONS` de `scripts/editor/lead_session_t186.py`)
Prérequis : build complet fait et éditeur relancé ; packs de T-334 construits (`"status": "built"`, fichiers présents et stables depuis 20 s). Vérification hors Unreal : `python scripts/editor/new_weapons_ue.py --check` (code de sortie 1 tant qu'un pack n'est pas prêt). `MONKE_NEW_WEAPONS=BayonetMusket` limite aux armes prêtes ; sans cela, **un pack manquant ou provisoire arrête l'action avant toute modification** (message clair, rapport `monke_island/Saved/T335/new-weapons.json`).
1. **Import** (FBX, `import_pirate_weapons.py`, entrées de registre `BoltRifle`, `Mitraillette`, `BayonetMusket` résolues par `art_pack_manifest`) : **un pack par passage** (gel de l'éditeur estimé à 1 à 3 min par pack) ; relancer l'action jusqu'à ce que les trois soient importés. Un pack déjà importé (même sha256 du manifeste, base présente) n'est pas réimporté (`MONKE_NEW_WEAPONS_FORCE_IMPORT=1` force).
2. **Données** : `DA_<Arme>Rules` (meshes, quatre clips par geste avec durées, événements et boucles du manifeste, os d'alignement `root`, sockets, caméra FP, cadrages hanche et visée du pack, sons des armes existantes, flamme, souffle, traçantes, douilles, lame vide, tambour, événement des coups), `BP_<Arme>` (règles, touches des armes à feu), `DA_Item_<Arme>` (nom, mesh au sol, icône `T_Icon_<Arme>.png` de l'agent art si présente), munitions `DA_Ammo_StripperClip` (5 cartouches de fusil), `DA_Ammo_RifleClipBox` (20), `DA_Ammo_SmgDrum` (50 cartouches de mitraillette) et leurs `BP_Ammo_*`.
3. **Boutique** (`DA_ShopCatalog`, lignes ajoutées si absentes, seulement pour une arme qui a son modèle) : fusil à verrou 1 200, mitraillette 1 500, mousquet à baïonnette 650 ; lame-chargeur 50, boîte de lames 180, tambour 120 couronnes.
4. **`L_Test_Flat`** : `Pickup_BoltRifle`, `Pickup_Mitraillette`, `Pickup_BayonetMusket` au bout de la rangée des ramassages d'armes déjà posés (la rangée la plus fournie d'`InventoryPickup` des `DA_Item_*` de `/Game/MonkeIsland/Weapons`, prolongée à son propre espacement), leurs munitions une rangée derrière (`Ammo_StripperClip_1/2`, `Ammo_RifleClipBox_1`, `Ammo_SmgDrum_1/2`) ; dossier `WeaponsTest`, tag `T335_NewWeapons` ; positions écrites dans le rapport. Carte sauvegardée sauf si elle avait des modifications non sauvegardées.

## Reste à faire et limites
- **Lead** : build complet éditeur fermé, specs `monke_island.Weapons.BoltRifle|Mitraillette|Bayonet|PirateFirearms|MachineGun|Stance`, action `newweapons` (une fois par pack, puis une dernière fois pour les données), relire le rapport.
- **Tikis (hors périmètre, `Creatures/`)** : `ATikiCreature::TakeDamage` ne reconnaît que mousquet, pistolet et fusil à pompe (`Cast`) : le fusil à verrou et la mitraillette (comme la mitrailleuse) font leurs dégâts **sans réaction de combat, sans sang ni membres arrachés**. Correctif proposé côté `Creatures/` : traiter `DamageEvent.IsOfType(FFirearmDamageEvent::ClassID)` comme un tir de mousquet. La baïonnette, causée par un `APirateMusketWeapon`, prend les réactions du mousquet (un coup à la tête l'arrache comme une balle).
- **Sensibilité de la souris à la lunette** non réduite (le regard est géré dans `Player/`) : à 22° de champ, la vue tourne aussi vite qu'à 90°.
- **Voile de lunette** du HUD non fait (`UI/` hors périmètre) : `ABoltRifleWeapon::IsScopeView()` est prêt pour lui.
- Capacité du fusil : 5 coups en tout (consigne) ; le pack annonce « 5 + 1 dans la chambre », non retenu.
- Sons provisoires repris des armes existantes (fusil, pompe, mitrailleuse, sabre), jamais écoutés.

## Vérifications PIE (solo, puis hôte + client)
1. **Fusil à verrou** : tir à la hanche, culasse automatique (douille éjectée), clic pendant la culasse sans effet avant `BoltClose` puis tir immédiat ; cinq coups puis lame-chargeur (cinq coups comptés, lame vide éjectée) ; clic droit : lunette (champ serré, arme cachée, pas de flamme devant l'œil), tir à la lunette, la lunette se baisse pendant la culasse et revient si le clic droit est tenu ; dispersion : immobile à la lunette, une cible à 100 m touchée ; en marchant, dispersée.
2. **Mitraillette** : clic tenu = 11 coups/s, recul qui monte, dispersion qui s'ouvre, douilles ; tapoter ne dépasse pas la cadence ; tambour vidé → `ReloadEmpty` (tambour qui tombe), R avant la fin → `Reload` ; pas de tir en courant ; ramassage de `Ammo_SmgDrum_*`.
3. **Mousquet à baïonnette** : tir et rechargement inchangés ; clic droit : coup de baïonnette (petite fente), dégâts sur un Veilleur, un soldat français, le crabe ; Maj + clic droit en courant : charge (fente longue, deux cibles possibles) ; endurance qui baisse, refus quand elle manque ; coup de baïonnette pendant le rechargement (coupé, balle gardée) ; pas de coup à travers un mur.
4. **Fusil à pompe** : arme vide avec 2 cartouches → cartouche par cartouche ; arme vide avec 10 → `ReloadEmpty` puis une cartouche de plus jusqu'au plein ; R pendant la pompe → le rechargement suit.
5. **Hôte + client** : le client voit ses gestes tout de suite, l'hôte les voit en 3e personne ; le coup de baïonnette du client touche (log serveur « Bayonet musket … stabs ») ; fente sans correction de position visible ; la lunette du client ne cache que son arme.
