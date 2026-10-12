# Grenade de pirate — le grenado (T-253)

**Lot :** L5 (armes), demande du propriétaire du 2026-10-09 (« créer plus d'armes, modèles de munitions, modèles réalistes, animations type Call of Duty »). Extension « armes à feu » du cahier §5, à arbitrer dans l'ADR-001 comme le pistolet.
**Statut : importé dans Unreal, compilé et testé éditeur le 2026-10-09 (Test Flat, PIE : prise en main, visée, tir, rechargement ; captures 171 à 225).** Le pack et les munitions sont générés dans le dépôt par Blender 5.2 en arrière-plan (preuve : fichiers, rendus et `check_pack.py` ci-dessous). Rien n'a été importé dans Unreal et aucune recette n'a tourné (`runtime_tested: false`). Le C++ (`AGrenadeWeapon`, `UGrenadeRules`, `AGrenadeProjectile`, geste `Discard` dans `EFirearmGesture`) et le pipeline d'import généralisé (`import_pirate_weapons.py` avec les entrées `Grenade` et `Ammo`, `setup_pirate_weapons.py`) sont ceux de **T-249** (code écrit, non compilé au moment de cette fiche).
**Pack :** `build/packs/bean-grenade/` (`grenade-manifest.json`, format `bean-grenade/1`, même structure que `bean-pirate-pistol/1` : `exports/{Bases,Animations,Static}`, `textures/`, `renders/`, `fp-framing.json`, `validation/grips/`, `bean-grenade.blend`), produit par `scripts/props/grenade_pack/build.py`.
**Munitions partagées :** `build/packs/ammo/` (`ammo-manifest.json`, `exports/`, `textures/`, `renders/product-ammo.png`, `ammo.blend`), produit par `scripts/props/grenade_pack/ammo.py`.

```powershell
& "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/grenade_pack/build.py -- --save --product
& "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/grenade_pack/ammo.py -- --save
python scripts/props/grenade_pack/check_pack.py                                            # contrat du pack + dry run de l'importeur
$env:MONKE_PIRATE_WEAPONS = "Grenade,Ammo"; python scripts/character/import_pirate_weapons.py --check
```
Un seul Blender à la fois (RAM partagée avec Unreal) ; la construction complète prend ~4 min (cuisson 2048 comprise), les munitions ~2 min. `build/` n'est pas versionné : on régénère.

## Intention
Le grenado : sphère de fonte creuse de 8 cm (~0,9 kg), bouchon de bois cerclé de fer, mèche de chanvre. Le pirate tient la grenade dans la **main droite**, posée sur la paume de la moufle, mèche en l'air, et porte une **mèche lente** allumée dans la **main gauche** (c'est un os de l'arme, `match`, qui suit la main gauche dans les clips, comme la cartouche du mousquet suit la main droite).
- **Lancer court** (`Fire`, depuis la hanche) : la grenade est amenée au centre de la vue contre la mèche lente, la mèche prend (`Light`), anticipation en arrière, lancer sec, suivi du bras, retour à la hanche.
- **Lancer long** (`AimFire`, bras armé en arrière) : même allumage au centre, armé derrière l'épaule, lancer par-dessus, suivi, retour en garde.
- **Réserve** : un sac de grenades à la ceinture. `Reload` sort la suivante (`HammerCocked` = elle est en main), `DryFire` tape le sac vide, `Discard` (ajout du propriétaire) retourne le sac vide, le secoue et le jette de côté (`Release`) : le bean finit les mains vides.
- Style Call of Duty : anticipation (wind-up), lancer sec (6 images), suivi du bras, grenade visible en bas à droite de la vue et au centre pendant l'allumage ; `AimIdle` respire ; `Sprint` grenade basse avec un bob marqué.

## Valeurs (`DA_GrenadeRules`, `UGrenadeRules` de T-249, dérivé de `UFirearmRules`)
Les valeurs proposées par cette fiche sont celles que `UGrenadeRules` porte déjà par défaut ; elles restent à régler en PIE.
| Valeur | Champ | Défaut |
|---|---|---|
| Mèche | `FuseSeconds` | 3,0 s depuis l'événement `Light` (`LightEvent`) ; `MinFuseAfterThrow` 0,3 s |
| Rayon d'explosion | `BlastRadiusCm` / `FullDamageRadiusCm` | 400 cm / 100 cm |
| Dégâts au centre | `CenterDamage` | 90, décroissance jusqu'à 0 au bord du rayon ; `bBlastNeedsLineOfSight` |
| Grenades | `StartGrenades` / `MaxGrenades` | 3 (un sac) / 6 ; `bStartInHand` |
| Lancers | `ShortThrowSpeed` / `LongThrowSpeed` / `ThrowPitchDegrees` | 900 / 1 600 cm/s, +10° ; départ à `ThrowOffset` (40, 15, −10) cm des yeux |
| Rechargement | `ReloadLoadedEvent` | `HammerCocked` (image 30 du clip `Reload`) ; `bAutoTakeNextAfterThrow` |
| Sac vide | `bDiscardWhenEmpty`, `DiscardReleaseEvent` | `Release` (image 30 du clip `Discard`) |
| Projectile | `ProjectileRadiusCm`, `Bounciness` | 6 cm (la sphère réelle fait 4 cm), 0,35 |

## Gestes (30 images/s, chacun en 1re et 3e personne)
| Geste | Images | Durée | Événements (image → s) | Notes |
|---|---|---|---|---|
| `Equip` | 45 | 1,5 s | `DrawWeapon` 8 → 0,27 ; `Ready` 38 → 1,27 | la grenade monte de sous le cadre, la mèche lente arrive par la gauche |
| `Idle` | 90 | 3,0 s (boucle) | — | respiration légère |
| `Walk` | 120 | 4,0 s (boucle, 4 cycles) | — | sur place, les jambes du bean sont celles du jeu |
| `Sprint` | 90 | 3,0 s (boucle, 3 cycles) | — | grenade basse, bob marqué |
| `AimIn` / `AimOut` | 24 | 0,8 s | — | hanche ↔ bras armé en arrière |
| `AimIdle` | 90 | 3,0 s (boucle) | — | bras armé, grenade visible en bas à droite, respiration |
| `Fire` | 60 | 2,0 s | `Light` 14 → 0,47 (`socket_muzzle`) ; `Fire` 34 → 1,13 (`socket_body`) | lancer court : allumage au centre, anticipation 17–28, lancer 28–34, suivi 34–43, retour |
| `AimFire` | 48 | 1,6 s | `Light` 10 → 0,33 (`socket_muzzle`) ; `Fire` 25 → 0,83 (`socket_body`) | lancer long par-dessus l'épaule, retour en garde |
| `Reload` | 75 | 2,5 s | `BagReach` 14 → 0,47 ; `HammerCocked` 30 → 1,0 (`socket_body`) ; `Ready` 72 → 2,4 | la main descend au sac (hors cadre), fouille, remonte avec la grenade |
| `DryFire` | 30 | 1,0 s | `DryClick` 12 → 0,4 | tape le sac vide |
| `Inspect` | 150 | 5,0 s | — | grenade au centre, tournée, la mèche lente levée |
| `Holster` | 45 | 1,5 s | `StowWeapon` 40 → 1,33 | |
| `Discard` | 36 | 1,2 s | `Empty` 2 → 0,07 ; `Release` 30 → 1,0 (`socket_bag`) | sac vide retourné (8–16), secoué (18–26), jeté à droite (30) ; mains vides à la fin |

`Light`, `Fire`, `HammerCocked`, `DryClick`, `Empty` et `Release` sont les noms que `UGrenadeRules` / `UFirearmRules` et les sons de `setup_pirate_weapons.py` attendent. `Empty` est à l'image 2 et non 0 : `AFirearmWeapon::StartAction` ne planifie aucun timer pour un événement à l'instant de départ du clip. Les longueurs TP et FP sont identiques, les événements aussi (`check_pack.py` le vérifie). La table provisoire `GRENADE_PLACEHOLDER` de `pirate_weapon_clips.py` (13 gestes) est remplacée par le manifeste dès que le pack est présent (`Discard` est dans `EFirearmGesture`).

## Squelette de l'arme (`SK_Bean_PirateGrenade`, son propre squelette)
Repère local : +X = les doigts de la main droite (avant), +Z = le haut (la mèche), origine = le centre de la moufle droite (`socket_grip_r`). Même convention que le pistolet et le mousquet (`gun_root` avant +X, haut +Z), donc le cadrage 1re personne d'`AFirearmWeapon` s'applique tel quel.

| Os | Parent | Rôle |
|---|---|---|
| `gun_root` | — | la main : racine, cuite dans l'espace du bean (les deux racines d'acteurs au même transform, jamais une seconde attache à la main) |
| `body` | `gun_root` | la sphère, le collier et le bouchon ; **échelle 0,0001 quand aucune grenade n'est en main** |
| `fuse` | `body` | la mèche de la grenade (suit `body`) |
| `match` | `gun_root` | la mèche lente, dans la main gauche (toujours visible) |
| `bag` | `gun_root` | le sac vide, visible seulement dans `Discard` images 8–29 |
| `socket_grip_r` | `gun_root` | poignée (cadrage hanche, `GripSocket`) |
| `socket_body` | `body` | centre de la sphère (point de départ cosmétique du projectile ; le C++ part de `ThrowOffset`) |
| `socket_muzzle`, `socket_fuse` | `fuse` | bout de la mèche de la grenade, deux noms pour le même point : `MuzzleSocket` de la logique des armes à feu, et `socket_fuse` attendu par `muzzle_sockets` du registre de l'importeur ; étincelles de `Light` à `Fire` |
| `socket_match` | `match` | braise de la mèche lente (FX permanent, petite lumière) |
| `socket_bag` | `bag` | bouche du sac : spawn du pickup à `Release` |

**Visibilité du corps (point à voir avec T-249).** Les clips mettent `body` (et donc `fuse`) à l'échelle 0,0001 à partir de l'image `Fire` des lancers, de 0 à 29 dans `Reload`, et pendant tout `DryFire` et `Discard` ; hors de ces clips la grenade est modelée en main. `AGrenadeWeapon::ShowGrenadeInHand` cache aujourd'hui **tout le mesh de l'arme** (`SetWeaponMeshVisible`) quand la main est vide : avec ce pack cela fait aussi disparaître la mèche lente de la main gauche et le sac pendant `Discard`. Il vaut mieux cacher le seul os `body` (`HideBoneByName("body", PBO_None)` sur les deux meshes d'arme) ou ne rien cacher pendant les gestes (les clips le font déjà) et ne cacher `body` qu'à l'arrêt (`Idle` après un lancer).

## Première personne (`fp-framing.json`)
Caméra d'auteur = la caméra du jeu : œil à (0, 0, 0,80 m) dans l'espace du bean, 90° horizontal, bras FP du bean (`SK_Bean_Grenade_FP`, moufles rigides sur `hand_l` / `hand_r`, sans étirement).
- Hanche (`Idle` image 0) : `HipGripLocation` = (27, 15, −11) cm, `HipRotation` = pitch 20, yaw −25, roll −10.
- Visée (`AimIdle` image 0) : `AimMuzzleLocation` = (16,5, 15,5, 1,2) cm (le « canon » est la mèche), `AimRotation` = pitch 35, yaw −40, roll −25 ; la poignée est alors à (26, 16, −11).
- Ou `bAlignFirstPersonToView = false` avec `FirstPersonViewLocation` = la caméra du pack : le cadrage d'auteur tel quel.
- Le lancer court lâche la grenade vers (40, 8, 0) cm dans la vue, le long vers (42, 4, 10) : `ThrowOffset` (40, 15, −10) est cohérent.

## Textures et matériaux (cuits dans Cycles)
Un seul matériau `M_PirateGrenade`, atlas 2048 : `textures/T_PirateGrenade_D.png` (BaseColor, sRGB), `T_PirateGrenade_N.png` (normale **DirectX**, vert vers le bas), `T_PirateGrenade_ORM.png` (R = AO, G = rugosité, B = métal, linéaire). Fonte piquée et rouillée, bois du bouchon, corde de chanvre, cuir du sac. Le manifeste les liste sous `materials.M_PirateGrenade.textures` avec les labels `BaseColor` / `Normal` / `ORM` (nœuds Image Texture du .blend) : `import_pirate_weapons.py` en fait l'instance `MI_Grenade_PirateGrenade` de `M_Character_Baked` et l'assigne par `imported_material_slot_name`. Dans le FBX, BaseColor et Normal sont en plus câblés (dossier `.fbm` à côté des bases et du statique). 5 944 triangles pour l'arme complète (sphère, mèche, mèche lente, sac), 4 624 pour la grenade seule.

## Assets livrés (`build/packs/bean-grenade/exports/`)
| Fichier | Destination (importeur) | Rôle |
|---|---|---|
| `Bases/SK_Bean_PirateGrenade.fbx` | `/Game/MonkeIsland/Weapons/Grenade/SK_Bean_PirateGrenade` | l'arme, 11 os (5 + 6 sockets) |
| `Bases/SK_Bean_Grenade_FP.fbx` | `…/Grenade/SK_Bean_Grenade_FP` sur `SK_Bean_Body_Skeleton` | bras 1re personne (20 os du bean, moufles sur `hand_l` / `hand_r`) |
| `Bases/SK_Bean_Grenade_TP.fbx` | non importé | corps TP de contrôle (en jeu, c'est la marionnette du projet) |
| `Animations/A_Bean_{TP,FP}_Grenade_<Geste>.fbx` (28) | `…/Grenade/Animations/Bean/` sur le squelette du bean | clips du corps / des bras |
| `Animations/A_Grenade_{TP,FP}_<Geste>.fbx` (28) | `…/Grenade/Animations/Weapon/` | clips de l'arme |
| `Static/SM_Bean_PirateGrenade.fbx` (+ `.glb`) | `…/Grenade/Static/SM_Bean_PirateGrenade` | la grenade seule au sol (pickup) et le mesh du projectile `BP_GrenadeProjectile` ; pivot bas-centre, collision `UCX_SM_Bean_PirateGrenade_00` dans le FBX (8,2 × 8 × 14,5 cm) |

Unités : mètres (`UnitScaleFactor` 100, `bConvertSceneUnit`, échelle d'import 1), comme les autres packs.

## Mise en place (éditeur fermé, verrou pris)
Le registre de `scripts/character/import_pirate_weapons.py` (T-249) connaît déjà `Grenade` (classe `GrenadeWeapon`, règles `GrenadeRules`, `muzzle_sockets: ["socket_fuse"]`, `projectile: True`) et `Ammo` ; `setup_pirate_weapons.py` crée `DA_GrenadeRules`, `BP_Grenade`, `BP_GrenadeProjectile`, `DA_Item_Grenade` et les `DA_Ammo_*`. Il n'y a donc **rien à ajouter** aux scripts. État au 2026-10-09 : `check_pack.py` → `CHECK OK` ; le dry run de l'importeur (`MONKE_PIRATE_WEAPONS=Grenade,Ammo … --check`) trouve les deux packs, 63 FBX en mètres, aucun fichier manquant ni hash faux, et n'échoue que sur `pirate_weapon_clips.py --check` : `PirateWeaponClips.inl` est **périmé** (il porte encore la table provisoire de la grenade). Suite, pour l'intégrateur : `python scripts/character/pirate_weapon_clips.py` (régénère le `.inl` avec les 14 gestes), compilation du C++ de T-249, puis le commandlet d'import et `setup_pirate_weapons.py` ; vérifier le cadrage FP ci-dessus dans le DA.

## Munitions ramassables (`build/packs/ammo/`, `ammo-manifest.json`)
Meshes statiques, mètres, pivot bas-centre, `UCX_<Nom>_00` dans chaque FBX, atlas 1024 (`T_Ammo_<Nom>_{D,N,ORM}` listés sous `materials.M_Ammo_<Nom>.textures`), FBX + GLB, destination `/Game/MonkeIsland/Weapons/Ammo/Meshes/`. `setup_pirate_weapons.py` les reconnaît par mot-clé (`AMMO_DEFS`) : `SM_Ammo_GrenadeBag` → `GrenadeBag` (3 grenades), `SM_Ammo_CrateMixed` → `Crate` (lots mixtes du butin du fort), `SM_Ammo_PowderKeg` → `Barrel` (« keg »).
| Asset | Contenu | Taille (m) | Triangles |
|---|---|---|---|
| `SM_Ammo_GrenadeBag` | sac de cuir à cordon, trois grenados dans le sac dont les bouchons et les mèches dépassent de la bouche | 0,168 × 0,168 × 0,204 | 15 312 (trois grenades complètes) |
| `SM_Ammo_CrateMixed` | petite caisse ouverte (planches, cornières de fer clouées), lit de toile, 4 cartouches en papier, 6 balles de plomb, 3 cartouches de fusil (papier rouge, culot de laiton) | 0,253 × 0,166 × 0,120 | 5 664 |
| `SM_Ammo_PowderKeg` | petit baril : douves de chêne (fil et joints cuits), trois cercles de fer rouillés rivetés, bonde latérale, bouchon de liège | 0,196 × 0,210 × 0,258 | 3 304 |

Les autres munitions (cartouches, giberne, corne à poudre, bourse, cartouches de fusil seules, sachet de plombs) restent dans les packs de chaque arme.

## Rendus de contrôle (`build/packs/bean-grenade/renders/`, `build/packs/ammo/renders/`)
`product-grenade.png` (vue produit), `FP-Fire-012/014/028/034/040.png` (allumage, mèche prise, anticipation, lancer, suivi), `FP-Idle-000.png`, `FP-AimIdle-000.png`, `FP-AimFire-010/025.png`, `FP-Discard-020/028.png`, `FP-Reload-045.png`, `FP-Sprint-015.png`, `FP-Walk-020.png`, `FP-Equip-024.png`, `FP-Inspect-060.png`, `FP-DryFire-012.png`, `FP-Holster-030.png`, `TP-Fire-014/034.png`, `TP-Idle/AimIdle/AimFire/Walk/Reload/Discard/Equip/Inspect-*.png` ; `product-ammo.png`.

## Limites et écarts
- Rien n'est vérifié dans Unreal : ni l'import, ni le cadrage, ni la synchronisation en PIE ; le C++ de T-249 n'est pas compilé.
- Les mains du bean sont des moufles : la grenade « s'enfonce » de 3 cm dans la moufle droite (prise stylisée), la mèche lente sort du poing gauche.
- La mèche de la grenade ne brûle pas dans les clips (longueur constante) : l'étincelle est un FX du jeu à `socket_muzzle` / `socket_fuse`.
- Le bras cuit `match` suit la main gauche du clip ; en 3e personne l'arme est posée sur la vraie `hand_r` de la marionnette, la mèche lente peut donc s'écarter légèrement de la vraie main gauche (même limite que la cartouche du mousquet). En TP `Fire`, la main gauche reste à 7 mm de sa cible à l'allumage (bras court, étirement maximal) : invisible à l'écran.
- `Discard` finit sur une pose « mains vides » qui n'est pas celle d'`Idle` : l'objet est censé quitter l'inventaire juste après.
- `ShowGrenadeInHand` cache tout le mesh (voir plus haut) : à arbitrer avec T-249 avant la recette.
