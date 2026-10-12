# Pistolet pirate de Bean (T-156)

**Lot :** L5, demande de l'utilisateur du 2026-10-06. Comme le mousquet, c'est une extension « armes à feu » du cahier §5, à arbitrer dans l'ADR-001.
**Maquette :** `bean-pirate-pistol` v0.1.0, produite sur le 2e PC (`OneDrive\maquette-blender\maquettes\`, lecture seule). Elle contient 14 gestes, chacun en 1re et en 3e personne, à 30 images/s. Le manifest est `pistol-manifest.json`. Le pack n'a jamais été testé dans Unreal (`runtime_tested: false`).
**Recette :** tests `monke_island.Weapons.PirateFirearms` (logique pure). Les IDs de recette du cahier pour les armes restent à créer avec l'ADR-001.

## Règles (DA_PistolRules, `UPistolRules`)
| Valeur | Défaut |
|---|---|
| Charges par joueur (`MaxCharges`) | 12 : 1 dans le canon au départ, 11 dans la poche |
| Départ chargé et armé (`bStartLoaded`, `bStartCocked`) | oui / oui |
| Dégâts / portée | 35 (T-167 : deux balles pour un villageois, trois pour un guerrier tiki au corps) / 30 m, sans atténuation ; `LimbDamageScale` 1 |
| Dispersion à la hanche / en visée | 3° / 0,8° |
| FOV de la vue du pack / FOV en visée | 78,6° (objectif de 22 mm) / 65° |
| Rechargement | 7,0 s (clip `Reload`, 210 images). La charge entre à `HammerCocked`, image 186 |
| Armement du chien | 1,4 s (clip `Cock`). Le chien se lève à `CockHammer`, image 20 |
| Cadence | un tir dure 1,4 s : le clip `Fire` part de son événement `Fire`, image 12 |

Les longueurs et les événements de chaque geste viennent du manifest. Le script `scripts/character/pirate_weapon_clips.py` les écrit dans `Weapons/PirateWeaponClips.inl`, qui remplit les valeurs par défaut de `Clips`. Chaque geste a aussi un `PlayRate` réglable dans le DA (logique et animations ensemble).

## Canons, munitions et arme vide (T-249, 2026-10-09)
- `UPistolRules::Barrels` (défaut 1) : `FirearmMath::FPistolState` compte `Loaded` (0 à `Barrels`), `bCocked`, `Reserve`, `Barrels` ; `FirearmMath::Fire` rend l'index du canon tiré. Le pistolet double (`UDoublePistolRules`, `Barrels` = 2, `SecondMuzzleSocket` = `socket_muzzle2`) est décrit dans `armes-nouvelles.md`.
- `AmmoKind` = `PistolCharge` : les paquets `DA_Ammo_PowderHorn` / `DA_Ammo_Pouch` et la ligne de munitions du marchand remplissent la poche (`AddAmmo`, borné par `MaxCharges`).
- Arme vide (rien de chargé, poche à zéro) : la gâchette joue `Discard` (ou `DryFire` sans ce clip) et l'arme est jetée au sol comme un pickup, compteurs gardés ; l'ancien retrait automatique après la dernière charge n'existe plus (`bDiscardWhenEmpty`, voir `armes-nouvelles.md` § Arme vide).

## Gâchette (serveur, `FirearmMath::OnTrigger`)
- **Chargé et armé** : le tir part tout de suite (`Fire`, ou `AimFire` en visée). Le canon se vide et le chien retombe.
- **Canon vide avec une charge en poche** : rechargement (`Reload`). Il remet une charge et arme le chien.
- **Chien abattu** : armement (`Cock`). Il faut armer avant de tirer.
- **Armé, vide, sans charge** : clic à vide (`DryFire`). Le chien retombe.
- **R** : recharge un pistolet vide, ou arme un pistolet chargé dont le chien est abattu.
- Le rechargement n'est pas interruptible par le tir. Le ranger, nager, porter un objet ou tomber coupe le geste : la charge n'est comptée que si `HammerCocked` est atteint.

## Commandes
Ce sont celles du mousquet, avec les mêmes `IMC_Musket` / `IA_Musket*` (priorité 2) :
- clic gauche : tirer ;
- clic droit maintenu : viser ;
- R : recharger ou armer.

`InspectAction` (geste `Inspect`) existe mais n'a pas de touche par défaut.

## Animations 1re / 3e personne
Le mécanisme est commun avec le fusil : `AFirearmWeapon`, voir `pirate-shotgun.md` § « Animations ». Pour le pistolet :
- l'arme se tient d'une main, la droite ;
- la main gauche n'aide qu'au rechargement ;
- en 1re personne, le bras gauche du pack est replié (os à l'échelle 0) hors du rechargement ;
- les gestes `Fire`, `AimFire` et `DryFire` finissent chien abattu, et c'est `Cock` ou `Reload` qui le relève, comme la logique.

## Assets (`/Game/MonkeIsland/Weapons/Pistol/`)
| Asset | Rôle |
|---|---|
| `SK_Bean_PiratePistol` | l'arme, avec son propre squelette : `gun_root`, `hammer`, `trigger`, `ramrod`, `charge_visual`, `socket_*` |
| `SK_Bean_Pistol_FP` | les bras 1re personne du pack, sur le squelette du bean du projet |
| `Animations/Bean/A_Bean_{TP,FP}_Pistol_*` | 28 clips du corps, sur `SK_Bean_Body_Skeleton` |
| `Animations/Weapon/A_Pistol_{TP,FP}_*` | 28 clips de l'arme |
| `Static/SM_Bean_PiratePistol` | l'objet au sol (pour le ramassage de T-157), avec collision |
| `Static/SM_Pistol_PaperToken` | la cartouche en papier (non utilisée en jeu pour l'instant) |
| `DA_PistolRules`, `BP_Pistol`, `DA_Item_Pistol` | créés par `setup_pirate_weapons.py` |

## Mise en place (éditeur fermé, verrou pris)
```powershell
python scripts/character/import_pirate_weapons.py --check   # à blanc : fichiers, sha256, unités, plan
& "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/matth/monk-island/monke_island/monke_island.uproject" -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/import_pirate_weapons.py" -unattended -nosplash -nullrhi -nosourcecontrol
& "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/matth/monk-island/monke_island/monke_island.uproject" -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/setup_pirate_weapons.py" -unattended -nosplash -nullrhi -nosourcecontrol
```
Ensuite, dans l'éditeur, mettre `DA_Item_Pistol` dans `StartingItems` de l'inventaire de `BP_IslanderBean`, ou le poser au sol avec le ramassage de T-157.

## Limites
- Rien n'est encore vérifié dans Unreal : ni l'import, ni le placement de l'arme, ni la synchronisation en PIE.
- Les sons sont provisoires, repris du mousquet (`SW_Musket_*`). La flamme est celle du mousquet, sans fumée.
- Pas de `Holster` en jeu : l'inventaire range l'objet immédiatement.
- Passer d'un clip à l'autre fait un saut, sans fondu (lecture `SingleNode`).

## v2 réaliste et pistolet double (2026-10-09, T-248) — importé dans Unreal, compilé et testé éditeur le 2026-10-09 (Test Flat, PIE : prise en main, visée, tir, rechargement ; captures 171 à 225)

**Demande du propriétaire (2026-10-09)** : armes plus réalistes, animations de tir et de rechargement type Call of Duty, plus d'armes, modèles de munitions. Ajout du coordinateur : geste `Discard`, visée sur la ligne de mire, sprint arme basse, rechargement lisible au centre.
**Statut : importé dans Unreal, compilé et testé éditeur le 2026-10-09 (Test Flat, PIE : prise en main, visée, tir, rechargement ; captures 171 à 225).** Rien n'a été fait dans l'éditeur Unreal : ni import, ni DA, ni PIE. Les packs sont vérifiés hors UE seulement (`check_pack.py`, `import_pirate_weapons.py --check`).

### Pack local (remplace la maquette v0.1 du 2e PC, qui reste intacte sur OneDrive)
- Scripts : `scripts/props/pistol_pack/` — `build.py` (entrée), `pistol_model.py` (géométrie, variantes `single` / `double`, munitions), `materials.py` (matériaux procéduraux Cycles + cuisson), `meshkit.py` (primitives, atlas UV, fusion par os), `rig_solver.py` (bras du bean TP/FP), `animation.py` (15 gestes), `export_pack.py` (FBX, manifeste, textures), `preview_model.py` (vues produit), `check_pack.py` (vérification), `pack.json`.
- Sorties : `build/packs/bean-pirate-pistol/` (`pistol-manifest.json`, format `bean-pirate-pistol/1` v2.0.0-local) et `build/packs/bean-pirate-pistol-double/` (`pistol-double-manifest.json`, format `bean-pirate-pistol-double/1`). Chaque pack : `exports/{Bases,Animations,Static}`, `textures/`, `renders/`, `fp-framing.json`, `validation/`, `<pack>.blend`.
- Commandes (Blender 5.2 en arrière-plan, un seul à la fois) :
  ```powershell
  & "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/pistol_pack/build.py -- --variant single --save
  & "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/pistol_pack/build.py -- --variant double --save
  & "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/pistol_pack/preview_model.py -- --variant single --ammo   # vues produit
  python scripts/props/pistol_pack/check_pack.py            # les deux packs
  python scripts/character/import_pirate_weapons.py --check   # registre T-249 : Pistol et DoublePistol lisent build/packs
  ```

### Modèle (pistolet à silex de marine, 0,366 m, dans le contrat v0.1 0,32–0,38 m)
Crosse pleine en noyer (fût + poignée courbe, lofts super-elliptiques), calotte de laiton à oreilles et bordure cordée, platine acier bruni avec chien col-de-cygne (mâchoires, vis à anneau, pierre dans son cuir, crête), batterie (face, couvre-bassinet, talon), bassinet et bride, ressort de batterie et ses vis, vis de platine, contre-platine serpentine en laiton à deux vis (simple), crochet de ceinture en fer (simple), écusson, pontet laiton avec finial et vis, détente, canon à culasse octogonale puis rond légèrement évasé, bouche et âme, queue de culasse et vis, hausse à cran et guidon laiton, baguette bois à embout laiton dans deux ressorts et une entrée, embout de fût laiton, clavette de canon. Double : deux canons juxtaposés (axes y = ±12,5 mm), bandes dessus/dessous, platine complète de chaque côté (`hammer2`, `frizzen2`), deux détentes, pontet allongé, pas de crochet.
- Triangles : simple 8 780 (+ 580 cartouche), double 11 236 (+ 580). Budget v0.1 (16 k / 25 k) respecté. Encombrement : 36,6 × 6,0 × 23,3 cm (double 36,6 × 9,5 × 23,3).
- Squelette (noms v0.1 conservés, un os `frizzen` ajouté) : `gun_root`, `hammer`, `frizzen`, `trigger`, `ramrod`, `charge_visual`, sockets `socket_grip_r` (0,0,0), `socket_muzzle` (0.27, 0, 0.071), `socket_load_port` (= bouche droite), `socket_hammer` (sur `hammer`), `socket_ramrod_grip` (sur `ramrod`). Double : + `hammer2`, `frizzen2`, `socket_muzzle2` (0.27, +0.0125, 0.071), `socket_hammer2`. Repos du chien = armé ; `Fire` l'abat de 50° (double : chien droit), `AimFire` idem (double : chien gauche `hammer2`, flamme à `socket_muzzle2`) ; la batterie s'ouvre de 62°.
- Textures PBR cuites (Cycles OptiX, 2048, un atlas par arme, usure des arêtes par nœud Bevel et des cavités par AO) : `textures/T_PiratePistol_{BaseColor,Normal,ORM}.png` et `T_PirateDoublePistol_*` ; nœuds Image Texture labellisés `BaseColor` (sRGB), `Normal` (tangent, **DirectX**, vert vers le bas), `ORM` (linéaire : R = AO, G = rugosité, B = métal). Matériaux d'export `M_PiratePistol` / `M_PirateDoublePistol` (un seul slot par arme), listés avec leurs textures dans `manifest.materials[].textures` par label. Munitions : `T_Ammo_Pistol_*` (1024), matériau `M_Ammo_Pistol`.

### Munitions (pack simple, `exports/Static/`, pivot bas-centre, boîte `UCX_<Nom>_00` dans le FBX, `manifest.assets` type `static_mesh` view `ammo`)
- `SM_Ammo_PistolCartridge` : cartouche papier couchée (ficelle, queue tordue, balle) ≈ 6 × 1,6 × 1,6 cm.
- `SM_Ammo_BallPouch` : bourse de cuir ouverte, cordon et nœud, six balles de plomb visibles ≈ 9 × 7,5 × 7,5 cm.
- `SM_Pistol_PaperToken` : alias de la cartouche sous le nom v0.1 (view `token`, sans UCX), pour les anciennes listes d'import.
- Ramassage : `SM_Bean_PiratePistol` / `SM_Bean_PirateDoublePistol` (fbx + glb, view `pickup`, collision auto à l'import).

### Gestes (30 i/s, TP et FP, mêmes durées et événements dans les deux vues ; noms d'événements v0.1 conservés)
| Geste | Images | s | Événements (image) | Notes CoD |
|---|---|---|---|---|
| Equip | 36 | 1,20 | DrawWeapon 6, Ready 30 | anticipation, montée rapide, dépassement, stabilisation |
| Idle | 90 | 3,00 (boucle) | — | respiration (0,55 cm, roulis 0,9°) |
| Walk | 120 | 4,00 (boucle) | — | balancement en 8 (4 cycles) |
| Sprint | 90 | 3,00 (boucle) | — | arme basse canon vers le bas, bob marqué (3 cycles) |
| AimIn / AimOut | 12 | 0,40 | — | ligne de mire (cran–guidon) sur l'axe caméra, petit dépassement |
| AimIdle | 90 | 3,00 (boucle) | — | respiration légère (0,22 cm) |
| Fire | 36 | 1,20 | Fire 2 (socket_muzzle), HammerDrop 2 | chien abattu 0→2, recul sec : canon +12°, recul 4 cm en 2 images, retour avec léger dépassement, stable à 22 |
| AimFire | 36 | 1,20 | Fire 2, HammerDrop 2 | idem en visée (+8,5°, 3 cm) ; double : chien gauche |
| Reload | 135 | **4,50** | AssistLeft 6, PaperPickup 18, PaperTransfer 58 (socket_load_port), RamrodGesture 70, RamrodReturned 112 (socket_ramrod_grip), HammerCocked 120, Ready 135 | arme tournée platine vers la caméra, au centre ; main gauche : cartouche de la bourse, mordue (31), versée sur la bouche avec deux secousses, balle poussée (58), baguette tirée en deux coups, retournée, deux coups de refoulement avec à-coups (94, 100), retournée, rentrée (112), chien armé au pouce (120, claquement), retour |
| DryFire | 24 | 0,80 | DryClick 6, HammerDrop 6 | déclic, petite secousse |
| Inspect | 150 | 5,00 | — | tourne l'arme : platine vers l'œil (26–62), puis côté gauche (92–122) tenu par la main gauche, retour |
| Holster | 36 | 1,20 | StowWeapon 30 | — |
| Cock | 30 | 1,00 | CockHammer 16 (socket_hammer) | arme roulée vers l'œil, batterie refermée, chien relevé lentement puis claque à 16 |
| **Discard** (nouveau) | 36 | 1,20 | **Empty 0, Release 24** | plus de munitions : regarde le pistolet (0–9), le secoue (11–20), le jette à droite et en bas (Release 24). Le clip arme **continue la chute jusqu'à 36** (FP : sort du cadre ; TP : atterrit au pied droit, à plat côté platine). Le clip bean finit mains vides. |

Durées v0.1 → v2 : Equip 54→36, AimIn/AimOut 30→12, Fire/AimFire 54→36 (événement 12→2), Reload 210→135, DryFire 42→24, Inspect 180→150, Holster 54→36, Cock 42→30. Idle/Walk/Sprint/AimIdle inchangés.

### Vue 1re personne
Caméra du jeu (0, 0, 0,80 m, 90° horizontal ; `camera_data` et `camera_transform` dans chaque scène FP). `fp-framing.json` / `manifest.fp_framing` : `hip_grip_location_cm` ≈ (26, 10, −15) et `hip_rotation_pyr_deg` (3, −6, −5) pour `HipGripLocation` / `HipRotation` ; `aim_muzzle_location_cm` ≈ (51, 0, −1,55) et `aim_rotation_pyr_deg` (0, 0, 0) pour `AimMuzzleLocation` / `AimRotation` (ligne de mire sur l'axe, l'âme 1,55 cm sous l'axe). Bras : droit sur la crosse ; gauche sous le cadre sauf Reload et Inspect (plus d'os à l'échelle 0 comme en v0.1).

### Intégration (à faire par l'intégrateur, éditeur fermé, verrou pris)
1. `python scripts/character/pirate_weapon_clips.py` (registre T-249 : lit `build/packs/bean-pirate-pistol` et `-double`) puis **rebuild** : les durées et les événements changent (Reload 4,5 s, Fire à l'image 2, `Discard` avec `Empty` / `Release`).
2. `MONKE_PIRATE_WEAPONS=Pistol,DoublePistol` → `import_pirate_weapons.py` puis `setup_pirate_weapons.py` (DA_PistolRules / DA_DoublePistolRules : `HipGripLocation` / `HipRotation` lus dans `fp_framing` ; `AimMuzzleLocation` / `AimRotation` à reporter à la main depuis `fp_framing`, le setup ne les lit pas encore). Double : `MuzzleSocket` `socket_muzzle` pour `Fire`, `socket_muzzle2` pour `AimFire` (visuel seulement ; la logique compte deux coups, le rechargement remet les deux).
3. Vérifier dans l'éditeur : taille de `SK_Bean_PiratePistol` ≈ 36,6 × 6,0 × 23,3 cm (double 36,6 × 9,5 × 23,3), échelle racine du bean, orientation (+X vers la bouche), textures (Normal sans flip vert), puis PIE.

### Rendus à regarder (`renders/` de chaque pack)
`product-<variant>-{right,left,side,lock}.png`, `product-ammo.png` (simple) ; `FP-Reload-{018,030,044,058,078,086,095,119}.png`, `FP-Fire-{000,003,008}.png`, `FP-AimIdle-000.png`, `FP-AimFire-003.png`, `FP-Sprint-010.png`, `FP-Inspect-{050,105}.png`, `FP-Discard-{014,027}.png`, `FP-Equip-014.png`, `FP-Idle-000.png`, `TP-Idle-000.png`, `TP-Reload-{030,086}.png`, `TP-Fire-003.png`, `TP-Discard-{020,036}.png`, `TP-Sprint-010.png`, `TP-AimIdle-000.png` ; double : sous-ensemble équivalent.

### Écarts et limites
- Durées et événements différents de la v0.1 (tableau ci-dessus) : `PirateWeaponClips.inl` à régénérer et à compiler ; la cadence après tir passe de 1,4 s à 1,13 s.
- Un os `frizzen` (et `frizzen2`) de plus que le contrat v0.1 ; les têtes d'os `hammer` / `trigger` / `ramrod` suivent la nouvelle géométrie (noms de sockets inchangés).
- La mitaine du bean (12 cm) masque la baguette pendant les coups de refoulement : c'est le mouvement de la main qui se lit ; la baguette est visible quand elle est tirée et retournée.
- Rechargement TP : les bras courts du bean obligent à rapprocher le pistolet de l'épaule gauche pendant la baguette (`reach` 0,47 m), le pistolet monte donc vers le visage.
- Les mitaines FP n'ont pas de doigts : la cartouche est tenue au bout de la mitaine ; le pouce ne « mord » pas vraiment le chien.
- Rien n'est importé ni testé dans Unreal ; `runtime_tested: false` dans les manifestes.

## Balistique (T-167, lot L5)
- La balle trouve son os comme un plomb du fusil (`FirearmDamage::FindStruckBone`, 18 cm) et part en `FFirearmDamageEvent`.
- Au corps (torse, bassin) : 35, pas un tir mortel.
- Sur un membre : 35 dépasse la santé de membre (bras et tête 25, jambes 30, `DA_TikiRules` `Limbs`) : **le membre est arraché** au premier tir bien placé.
- **Tête** : choix retenu, **la tête arrachée tue** (pas de multiplicateur ×2). Un tir à la tête est donc mortel, avec démembrement de la tête ; un tir qui frôle la tête à plus de 18 cm compte pour le corps.
- Sang `NS_Blood_MusketImpact` au point d'entrée, bouffée `NS_Firearm_Impact` sur le décor.
