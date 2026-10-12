# Armes longues : tromblon et carabine rayée (T-250)

**Lot :** L5, demande du propriétaire du 2026-10-09 (« créer plus d'armes, modèles réalistes, animations de tir et de rechargement type Call of Duty, modèles de munitions »). Comme le mousquet, c'est une extension « armes à feu » du cahier §5, à arbitrer dans l'ADR-001.
**Statut : importé dans Unreal, compilé et testé éditeur le 2026-10-09 (Test Flat, PIE : prise en main, visée, tir, rechargement ; captures 171 à 225).** Les deux packs sont produits et vérifiés hors Unreal (rendus Blender, `check.py`) ; rien n'est importé ni testé dans l'éditeur (consigne de la tâche : pas d'éditeur, pas de MCP). Recette à faire par l'intégrateur après import.
**Chaîne de production :** `scripts/props/blunderbuss_pack/`, `scripts/props/rifle_pack/` et le code commun `scripts/props/longarm_common/`, qui réutilisent **par import** le pack du mousquet (`scripts/props/musket_pack/`, T-212 et sa v2 T-246) : source du bean, solveurs des bras (FP placés par la caméra du jeu, TP à deux segments), poses `look` / `show_barrel` / `throw` d'`Inspect` et `Discard`, export FBX. Les packs ne touchent à aucun fichier du mousquet.

## Les deux armes
| | Tromblon (`Blunderbuss`) | Carabine rayée (`Rifle`) |
|---|---|---|
| Modèle | tromblon naval : canon court en laiton à bouche évasée (lathe creux), crosse en noyer, platine à silex, baguette courte | type Baker : canon bruni à culasse octogonale, hausse à feuillet et guidon, barrette de sabre-baïonnette, crosse à joue, garnitures en laiton, sous-garde à crosse (scroll), **boîte à calepins** à couvercle articulé dans la crosse, bretelle |
| Longueur | 0,75 m (bbox 76 × 6,8 × 15,8 cm) | 1,15 m (bbox 116 × 7 × 22,5 cm) |
| Munition | `SM_Ammo_ShotBag` : sachet de toile de plombs, lanière de cuir, plombs répandus au pied (5,8 × 4,2 × 6,2 cm, pivot au sol) | `SM_Ammo_RifleCartridge` : cartouche de papier (poudre + balle calepinée), couchée (7 × 1,8 × 1,8 cm, pivot au sol) |
| Rechargement | 5,0 s (120 images) | 7,0 s (168 images) |
| Visée | le dessus du canon (haut de la culasse → lèvre de la bouche), œil à 28 cm | hausse → guidon, œil à 30 cm |
| Recul (clip) | +10° de relèvement, 6,5 cm de recul, roulis 3° | +6°, 4 cm, roulis 1,5° |
| Masse (volumes × densités, indicatif) | 5,5 kg | 7,7 kg |

Les deux armes gardent **le contrat du mousquet** : os `root` (racine, = `FirstPersonAlignBone` du mousquet), `hammer`, `frizzen`, `trigger`, `ramrod`, `cartridge`, sockets `socket_grip_r`, `socket_grip_l`, `socket_barrel`, `socket_muzzle`, `socket_pan`, `socket_butt`, `socket_sling`, `socket_ramrod_tip`, `socket_ramrod_end`, `socket_flint`, plus `socket_sight_rear` / `socket_sight_front` (ligne de mire). La carabine ajoute les os `patchbox_lid` (couvercle) et `patch` (calepin + balle) et le socket `socket_patchbox` ; ils sont animés dans les clips et ignorés par le code.

## Valeurs de gameplay proposées (DataAsset, jamais en dur)
Classe d'arme : **`APirateMusketWeapon`** avec un `UPirateMusketRules` propre à chaque arme (`DA_PirateBlunderbussRules`, `DA_PirateRifleRules`), même logique que le mousquet (une charge dans le canon, le reste dans la giberne, rechargement complet `GroundReload` étiré sur `ReloadSeconds`).

| Valeur | Tromblon | Carabine |
|---|---|---|
| Dégâts / projectile (`Damage`) | 14 × **10 plombs** | 90 (une balle) |
| Portée (`RangeCm`) | 1 200 (12 m) ; `DamageFalloff` : 100 % à 4 m, 40 % à 8 m, 10 % à 12 m | 16 000 (160 m) ; `FalloffStartCm` 8 000, `FalloffMinScale` 0,6 |
| Dispersion hanche / visée (`HipSpreadDegrees` / `AimSpreadDegrees`) | 9° / 7° | 2,5° / **0,2°** |
| Munitions par joueur (`MaxBalls`, `bStartLoaded`) | 8 sachets, chargé | 12 cartouches, chargée |
| Rechargement (`ReloadSeconds`) | **5,0** (= le clip, cadence 1) | **7,0** (= le clip, cadence 1) |
| FOV en visée (`AimFieldOfView`) | 70° | 58° |
| Secousse de vue (`ViewKickPitch` / `ViewKickYaw`) | 4° / 1° | 2° / 0,4° |
| Recul des cibles (`KnockbackSpeed`) | 350 | 250 |
| Cadrage FP hanche (`HipGripLocation` / `HipRotation`, `fp-framing.json`) | (31, 14, −19) / (6, −6, −12) | (31, 14, −19) / (6, −6, −12) |
| Cadrage FP visée (`AimMuzzleLocation` / `AimRotation`, `fp-framing.json`) | (72,04, 0, −3,08) / (−1,465, 0, 0) | (94,12, 0, −2,27) / (0,43, 0, 0) |
| Événements | `FireEvent` = `Shot`, `TriggerEvent` = `Trigger` | idem |

Pré-requis code (programmeur gameplay) : `UPirateMusketRules` n'a pas de nombre de plombs ; il faut un `PelletCount` (ou une sous-classe `UBlunderbussRules`) pour que `APirateMusketWeapon` tire 10 traces dans le cône, comme `AShotgunWeapon`.

## Packs livrés (`build/packs/`, format `bean-musket-anim/1`, 24 i/s, mètres : UnitScaleFactor 100 sur les 113 FBX)
- `build/packs/bean-blunderbuss-anim/` (`blunderbuss-manifest.json`, 133 Mo) et `build/packs/bean-rifle-anim/` (`rifle-manifest.json`, 136 Mo) : `exports/Bases/` (`SK_Bean_Pirate<Arme>.fbx` squelette propre, `SK_Bean_<Arme>_FP.fbx` bras FP sur le squelette du bean, `SK_Bean_<Arme>_TP.fbx` non importé), `exports/Animations/` (108 FBX : `A_Bean_<Arme>_{TP,FP}_<Geste>` et `A_<Arme>_{TP,FP}_<Geste>`), `exports/Static/` (`SM_Bean_Pirate<Arme>` pickup à pivot bas-centre + `SM_Ammo_*`, FBX et GLB), `textures/`, `renders/`, `fp-framing.json`, `validation/grips/`, `<arme>-clips.inl.txt`, `bean-<arme>-anim.blend`.
- **27 gestes** par vue : les 24 du mousquet (`Equip`, `Idle`, `Walk`, `Sprint`, `AimIn`, `AimIdle`, `AimOut`, `HalfCock`, `PrimePan`, `CloseFrizzen`, `GroundReload`, `Powder`, `BallAndPatch`, `DrawRamrod`, `TurnRamrod`, `RamrodStroke`, `ReturnRamrod`, `Cock`, `Present`, `Fire`, `Recoil`, `Hold`, `ReturnToPort`, `Holster`) + **`AimFire`** (tir depuis la pose de visée : plus d'alias de `Fire`), **`Inspect`** (96 images) et **`Discard`** (32 images, demande du propriétaire). Les sous-gestes du rechargement sont des fenêtres de `GroundReload` rejouées sur leur propre longueur (`reload_sequence.windows_frames`).
- **Événements du rechargement** (images, `reload_sequence.events`) — tromblon : HalfCockStart 2, PanPrime 26, FrizzenClose 33, StockLowered 44, PowderCharge 50, **ShotPour 60** (le sachet versé dans la bouche), BallAndPatch 66 (papier bourré), RamrodDraw 74, RamrodTurn 86, RamrodStroke 100 et 104, RamrodReturn 108, FullCockEnd 117 ; carabine : HalfCockStart 3, PanPrime 31, FrizzenClose 40, StockLowered 52, PowderCharge 58, **PatchBox 78** (couvercle ouvert, socket `socket_patchbox`), BallAndPatch 93 (calepin et balle enfoncés au pouce), RamrodDraw 104, RamrodTurn 121, RamrodStroke 137 et 144 (deux coups appuyés), RamrodReturn 153, FullCockEnd 164.
- **Tir** (`Fire` / `AimFire`) : Trigger 5, PanFlash / MuzzleFlash / Shot / Recoil 8. **`Discard`** : `Empty` image 1 (le chien tombe sur le bassinet vide), `Release` image 22 (l'arme quitte la main droite, socket `socket_grip_r`) ; le clip bean finit les mains vides, le clip arme continue : l'arme vole à droite et vers le bas, tourne, et repose au sol à la fin du clip (TP). `Equip` : DrawWeapon 8, Ready 34 ; `Holster` : StowWeapon 36.
- **Textures PBR cuites** (Cycles OptiX, matériaux procéduraux : noyer à veines, laiton patiné, acier bruni ou gris strié, cuir, toile, papier, plomb ; usure des arêtes par nœud Bevel + pointiness, salissure par occlusion) : `T_Bean_Pirate<Arme>_D` (sRGB), `_N` (tangent, **DirectX : vert vers le bas**), `_ORM` (R = AO, G = rugosité, B = métal, linéaire), 2048² ; munitions `T_Ammo_*` en 1024². Un seul matériau par mesh, `M_Bean_Pirate<Arme>` / `M_Ammo_*`, nœuds Image Texture labellisés `BaseColor` / `Normal` / `ORM` et listés dans `manifest.materials[<matériau>].textures` (chemin, taille, espace colorimétrique). L'import FBX d'Unreal ne branche que BaseColor et Normal : brancher l'ORM à la main dans le matériau.
- **Rendus** (`renders/`) : FP `GroundReload` à quatre instants (tromblon 26 amorçage, 50 poudre, 61 plombs versés, 97 bourrage ; carabine 31, 60, 82 boîte à calepins, 136 bourrage), FP `Fire` 10, `AimIdle` 0, `Idle` 0, `Sprint` 12, `Discard` 14 et 22, `Inspect` 30 et 70, TP `Hold` 0, `GroundReload`, `Discard` 31 (arme au sol), `AimIdle` 0, `product.png` (arme + munition) et `product-lock.png`.

### Animations façon Call of Duty (`scripts/props/longarm_common/choreo.py`)
Anticipation, dépassement et stabilisation sur les arrivées (easing `ease_os` du mousquet v2, rebonds amortis `settle`) ; tir au recul sec (relèvement en 2 images, recul le long du canon, roulis, retour avec dépassement, stable en 16 images), le même en visée à 80 % ; `Idle` / `Hold` respiration (2 cycles) ; `Walk` balancement en huit ; `Sprint` arme basse inclinée (−14°, −40°, −50°) avec bob marqué (×2,2) ; `Inspect` platine à l'œil puis dessus du canon ; `Discard` regard, secousse, lancer. **Visée** : `AimIn` / `AimIdle` / `AimFire` / `AimOut` sont écrits avec la ligne de mire sur l'axe de la caméra FP, arme centrée, respiration légère en `AimIdle` ; `fp-framing.json` donne `aim_muzzle_location_cm` / `aim_rotation_pyr_deg` à copier dans le DataAsset pour que le recadrage du jeu soit nul (le mousquet v2 garde une seule pose et laisse le jeu cadrer : les deux approches coexistent, le DA fait la différence). Le rechargement est au centre de la vue (poses `prime` / `load` / `box`).

### Troisième personne
Les bras du bean (0,32 m) sont courts pour ces armes : le tromblon se charge debout comme le mousquet (crosse au sol entre les pieds) ; la carabine (plus longue que le bean) se charge crosse au sol **à droite du bean**, bouche devant le menton, les deux mains à la bouche, baguette travaillée **main sur main** (quatre tractions, bourrage en poussées avec reprises) ; pendant la pose `box` la crosse remonte à la hanche. Portée résiduelle : carabine, main droite jusqu'à 17 cm sous la baguette au retournement (images 156-159) ; tromblon, main gauche ≤ 7,6 cm pendant le bourrage (le mousquet accepte 25 cm). Les sabots qui plongeraient sous le sol sont relevés par la logique du mousquet.

## Mapping pour l'intégrateur (`/ue-import`, éditeur fermé, verrou tenu)
Mêmes commandlets que le mousquet, avec ces ajouts (aucun fichier de script n'a été modifié par T-250) :
1. `scripts/character/import_pirate_weapons.py` : `FPS["Blunderbuss"] = FPS["Rifle"] = 24` et, dans `WEAPONS` :
   `"Blunderbuss": {"pack": "bean-blunderbuss-anim", "manifest": "blunderbuss-manifest.json", "base": "SK_Bean_PirateBlunderbuss", "fp": "SK_Bean_Blunderbuss_FP", "static": ["SM_Bean_PirateBlunderbuss", "SM_Ammo_ShotBag"], "pickup": "SM_Bean_PirateBlunderbuss", "idle": "A_Bean_Blunderbuss_TP_Hold", "local_pack": True}` ;
   `"Rifle": {"pack": "bean-rifle-anim", "manifest": "rifle-manifest.json", "base": "SK_Bean_PirateRifle", "fp": "SK_Bean_Rifle_FP", "static": ["SM_Bean_PirateRifle", "SM_Ammo_RifleCartridge"], "pickup": "SM_Bean_PirateRifle", "idle": "A_Bean_Rifle_TP_Hold", "local_pack": True}` ;
   `MONKE_PIRATE_WEAPONS=Blunderbuss,Rifle`. Destination `/Game/MonkeIsland/Weapons/<Arme>/` : `SK_Bean_Pirate<Arme>` (squelette propre), `SK_Bean_<Arme>_FP` (sur `SK_Bean_Body_Skeleton`), `Animations/Bean/A_Bean_<Arme>_{TP,FP}_<Geste>`, `Animations/Weapon/A_<Arme>_{TP,FP}_<Geste>` (noms du manifeste gardés, comme le mousquet), `Static/SM_*`. Le rapport vérifie l'échelle racine du bean sur `A_Bean_<Arme>_TP_Hold` et la taille du mesh contre `weapon_contract.bounds.size`.
2. `scripts/character/setup_pirate_weapons.py` : `ASSET_NAME` `PirateBlunderbuss` / `PirateRifle`, `DISPLAY` « Tromblon » / « Carabine », `CLIP_NAME` vide (pas d'alias : `AimFire` existe), `clip_sources` sur le motif du mousquet (`A_Bean_<Arme>_TP_<Clip>`), classes `UPirateMusketRules` / `APirateMusketWeapon`, `EVENT_SOUNDS` du mousquet + `ShotPour → SW_Musket_Powder`, `PatchBox → SW_Musket_Paper`, `Empty → SW_Musket_Lock` (provisoires). Le setup n'écrit que le cadrage hanche : copier `aim_*` de `fp-framing.json` à la main.
3. `scripts/character/pirate_weapon_clips.py` : ajouter les deux manifestes à `WEAPONS` **après** avoir ajouté `Discard` à `EFirearmGesture` (`Weapons/FirearmRules.h`, éditeur fermé + build), sinon le `.inl` généré ne compile pas ; la table `Add<Arme>Clips` est pré-générée dans `build/packs/<pack>/<arme>-clips.inl.txt`.
4. Gameplay : jouer `Discard` à la dernière munition (`bDiscardWhenEmpty`, `DiscardDelay` ≈ 1,33 s = le clip) et lâcher le pickup à l'événement `Release` ; `PelletCount` pour le tromblon (ci-dessus).
5. Vérification hors Unreal : `python scripts/props/longarm_common/check.py build/packs/bean-blunderbuss-anim build/packs/bean-rifle-anim` (fichiers, sha256, unités, 27 gestes × 2 vues, événements, os et sockets, textures, cadrage) — `CHECK OK` pour les deux le 2026-10-09.

## Reconstruire
```
blender -b --factory-startup --python scripts/props/blunderbuss_pack/build.py -- --save --product --render FP:GroundReload:61,TP:Hold:0
blender -b --factory-startup --python scripts/props/rifle_pack/build.py -- --save --product
```
(Blender 5.2 ; options `--clips`, `--views`, `--no-bake`, `--no-export` pour un essai rapide, 10 s sans cuisson.) Cuisson ≈ 25 s sur GPU, construction complète ≈ 2 min par arme. `musket_ref.py` vérifie les attributs du pack du mousquet et s'arrête avec un message clair si le mousquet change (T-246 retravaille `animation.py` en parallèle : reconstruire et relancer `check.py` quand T-246 sera posé).

## Écarts et reste
- Rien n'est importé dans Unreal ni testé éditeur (consigne) ; l'orientation après import (avant vers +X) reste à vérifier, comme pour le mousquet.
- 27 gestes au lieu de 24 : `AimFire`, `Inspect` et `Discard` ajoutés (`Discard` demandé par le propriétaire ; il manque à `EFirearmGesture`).
- Visée écrite dans les clips (demande du propriétaire) : différent du mousquet v2 ; sans les valeurs `aim_*` dans le DA, `AFirearmWeapon` recadrerait la visée autrement.
- Textures des munitions en 1024 (objets de 5 à 7 cm) ; les rendus Blender lisent la normale DirectX telle quelle (relief fin inversé à l'écran, correct dans Unreal).
- Aucun asset externe : modèles, matériaux et textures sont procéduraux (`art/SOURCES.md` inchangé).
