# Mousquet de Bean et inventaire (T-069)

**Lot :** L5, sur demande de l'utilisateur du 2026-10-03. Écart au cahier §5 : les armes à feu y sont prévues comme une extension, l'ADR-001 doit l'arbitrer.
**Maquettes :** produites sur le 2e PC, dans `OneDrive\maquette-blender\maquettes\` : `bean-musket` v0.1.0, `bean-musket-anim`, `bean-ball-pouch` v0.2.0. Copies dans `art/export/Weapons/Musket/`.

## Règles (DA_MusketRules)
| Valeur | Défaut |
|---|---|
| Balles par joueur (`MaxBalls`) | 23 : 1 dans le canon au départ, 22 dans la sacoche |
| Rechargement complet (`ReloadSeconds`) | 6,0 s |
| Délai de la platine (`FireDelaySeconds`) | 0,12 s |
| Recul (`RecoverSeconds`) | 0,6 s |
| Dégâts / portée | 70 / 80 m |
| Dispersion à la hanche / en visée | 4° / 0,6° |
| Tir ami | désactivé |
| FOV en visée | 65° |

Chargement par la bouche : un tir vide le canon, puis il faut recharger entièrement, ce qui prend une balle dans la sacoche. Cliquer pour tirer avec le canon vide lance le rechargement s'il reste des balles.

## Commandes
- **1 à 4** (ou &, é, ", ' en AZERTY, ou le pavé numérique) : sélectionne l'emplacement. Appuyer de nouveau sur la même touche range l'objet, et l'on repasse aux mains nues (coup de poing).
- Mousquet en main : **clic gauche** tire, **clic droit maintenu** épaule, **R** recharge.
  - Le contexte `IMC_Musket` a la priorité 2 et masque le poing, la garde et le changement de vue, qui est aussi sur R.
- **V** (ou R à mains nues) : change de vue. R est pris par le rechargement quand le mousquet est en main, V marche toujours.
- Le contexte du mousquet est retiré dans plusieurs cas : objet porté (le clic gauche lance alors l'objet), nage, chute ragdoll, mort.

## Architecture
- Dossier `Source/monke_island/Weapons/` :
  - `UIslanderInventoryComponent` : emplacements, objets de départ, emplacement actif répliqué ;
  - `AInventoryItemActor` : base d'un objet équipé, attaché au porteur, avec un état équipé répliqué ;
  - `AMusketWeapon` : l'arme ;
  - `UMusketRules` : les règles ;
  - `MusketMath` : la logique pure testée.
- **Serveur autoritaire :**
  - le client envoie seulement « tirer » ;
  - le serveur vérifie les munitions et l'état, puis trace le tir depuis la caméra du porteur dans la direction de visée répliquée ;
  - il applique `ApplyPointDamage` et l'impulsion ;
  - `PouchBalls` n'est répliqué qu'au porteur.
- **1re personne :**
  - les clés (`HoldPose`, `AimPose`, `ReloadKeys`, écrites dans `MusketRules.cpp`) placent seulement le mousquet par rapport à la caméra, et disent quel socket de `SK_Bean_Musket` chaque moufle tient (`socket_grip_r`, `socket_grip_l`, `socket_pan`, `socket_barrel`, `socket_muzzle`, `socket_ramrod_tip`) ou un point libre (la giberne) ;
  - chaque bras est résolu chaque image depuis son épaule (`RightShoulder` / `LeftShoulder`) vers sa prise (`MusketMath::SolveArm`), avec un étirement de bras en caoutchouc jusqu'à `MaxArmStretch`, si bien que les moufles restent sur l'arme, recul compris ;
  - le rechargement suit les phases de la maquette (demi-armé, amorçage, crosse en bas et canon levé, poudre, balle, baguette, armé). La baguette vient de `A_Musket_Reload`, joué sur le mousquet, et la main droite suit son extrémité ;
  - les rotations passent par des quaternions, ce qui supprime les tours complets de l'ancienne interpolation d'angles ;
  - les anciennes poses `fp_poses.json` (caméra virtuelle qui regardait les mains) sont abandonnées.
- **3e personne :**
  - le mousquet est attaché à `hand_r` du pantin Bean ;
  - dans la maquette, le mousquet est un objet à part : pendant le rechargement, la main droite le lâche et la main gauche le tient par le canon. Soudé à `hand_r`, il s'envolait avec la main droite ;
  - `scripts/props/musket_tp_tracks.py` (Blender en arrière-plan, le .blend n'est que lu) cuit donc la position du mousquet par rapport à `hand_r`, image par image, pour `Hold`, `Reload` et `Fire`, dans `MusketThirdPersonTracks.inl` (`HoldMusketTrack`, `ReloadMusketTrack`, `FireMusketTrack`, 24 i/s) ;
  - `UpdateThirdPersonGrip` lit la piste du clip joué par le corps caché, au même instant ;
- **Tir :**
  - la platine claque (`LockSound`), puis après `FireDelaySeconds` vient la détonation (`FireSound`) ;
  - au même moment : flamme à la bouche (sphère étirée, `M_MusketFlash` additif), lumière brève, et fumée de poudre noire (7 bouffées, `M_MusketSmoke` translucide) qui ralentit, monte, grossit et s'efface ;
  - en 1re personne, la vue saute (`ViewKickPitch` / `ViewKickYaw`) et roule un instant (`ViewShakeRoll`), en plus du recul de l'arme et des bras.
- **Sons :** synthétisés par `scripts/props/musket_sounds.py` (aucun enregistrement, donc aucune licence) dans `art/audio/weapons/`.

## Assets Unreal (`/Game/MonkeIsland/Weapons/`)
`SK_Bean_Musket` (avec le socket `muzzle`), `DA_MusketRules`, `DA_Item_Musket`, `BP_Musket`, `Input/IA_Musket*`, `IA_Slot1-4`, `IMC_Musket`, `IMC_Inventory`. Le composant `Inventory` est sur `BP_IslanderBean`, avec le mousquet dans l'emplacement 1, équipé dès l'apparition.

## Reste à faire
- Les 5 animations, les 2 sons et les 2 matériaux s'importent avec `scripts/props/import_musket_anims.py`, éditeur fermé (commandlet), qui les branche aussi dans `DA_MusketRules`.
- Vérifier le roulis de la prise en 3e personne. Remplacer un jour la fumée en sphères par un Niagara (tech-artist).
- Recette PIE hôte + client par l'utilisateur : tir, rechargement, 23 tirs, objet porté, nage.

## Recette
- `monke_island.Weapons.Musket` : 11 tests (23 balles, cycle tir/rechargement, 23 tirs exactement, poses triées, échantillonnage, moufle sur sa cible, rotation par le plus court chemin, pistes 3e personne, cône de dispersion).
- PIE solo, testé éditeur le 2026-10-03 : le HUD affiche « 1 Mousquet » actif et « Chargé · 23 balles ».
- Le tir, le rechargement et la visée n'ont pas encore été testés en jeu : aucun outil MCP ne permet de simuler des touches.

## Munitions, arme vide, dérivés (T-249, 2026-10-09)
- `DA_PirateMusketRules.AmmoKind` = `MusketBall` : `DA_Ammo_Cartridges` / `DA_Ammo_CartridgeBox` et la ligne du marchand remplissent la giberne (`AddAmmo`, borné par `MaxBalls`).
- Mousquet vide (canon vide, giberne à zéro) : la gâchette joue `Discard` (ou `DryFire` sans ce clip) et le mousquet est jeté au sol comme un pickup, compteurs gardés ; l'ancien retrait automatique après la dernière balle (`CheckDepleted`) n'existe plus (`bDiscardWhenEmpty`).
- Dérivés : la carabine (`URifleRules`, `BP_Rifle` sur `APirateMusketWeapon`) et le tromblon (`ABlunderbussWeapon`, `GetProjectilesPerShot`) — voir `armes-nouvelles.md`.

## Mousquet pirate refait dans le dépôt (T-212, 2026-10-07)
Demandes du propriétaire : « fix la vue première personne du mousquet, ajoute plus de détails au fusil et à l'animation de chargement et synchronisation des mains », « fix les boutons de manchettes … des mains qui flottent ». Choix du propriétaire : le faire **ici**, pas sur le 2e PC (le pack `bean-musket-anim` v0.2 de OneDrive reste intact).
- **Chaîne locale** `scripts/props/musket_pack/` (copie de `bean-musket-anim` v0.2 et `bean-musket` v0.2) : `kit.py` (helpers du kit, palette du bean du dépôt), `musket_model.py` (fusil + détails), `animation.py` (24 clips TP/FP), `export_pack.py`, `build.py`. Construction : `blender -b --factory-startup --python scripts/props/musket_pack/build.py -- --save` → `build/packs/bean-musket-anim/` (v0.3.0-local, manifeste, FBX, `fp-framing.json`, rendus). Option `--clips … --views FP --no-export --render FP:GroundReload:60` pour un essai rapide.
- **Import** : éditeur fermé, `MONKE_PIRATE_WEAPONS=Musket` puis `import_pirate_weapons.py` et `setup_pirate_weapons.py` en commandlet ; ou éditeur ouvert sans partie : action `musketimport` de `scripts/editor/lead_session_t186.py`. Le setup reporte les durées et les événements du manifeste dans `DA_PirateMusketRules` et y écrit le cadrage hanche. `pirate_weapon_clips.py` lit désormais le manifeste local (table `PirateWeaponClips.inl` régénérée).
- **Vue première personne** : une seule pose de base pour tous les clips hors rechargement ; le jeu la cadre à la hanche avec `HipGripLocation` (31, 15, -18) / `HipRotation` (6, -5.5, -8), valeurs exactes de la caméra d'auteur (90°), et sur l'axe de visée avec `AimMuzzleLocation` / `AimRotation`. Les bras FP (manche, manchette, galons, boutons, poignet, mitaine) sont placés directement : centre de la mitaine sur sa cible, avant-bras venant d'un coude hors champ, sans étirement ni mise à l'échelle. Matériaux : les MI du bean (`MI_Bean_Skin` beige…).
- **Rechargement** (`GroundReload`, 6 s à 24 i/s, les deux mains synchronisées) : demi-armé au pouce, cartouche tirée de la giberne, déchirée aux dents, amorçage du bassinet, bassinet refermé, fusil dressé crosse vers le sol, poudre versée, balle et papier poussés, baguette tirée en deux fois, retournée, bourrage en deux coups, ressortie, retournée, remise dans ses porte-baguette, retour à la hanche, armé complet. La main gauche ne lâche jamais le fusil. Événements sonores recalés : HalfCockStart 3, PanPrime 31, FrizzenClose 40, StockLowered 52, PowderCharge 58, BallAndPatch 71, RamrodDraw 81, RamrodTurn 96, RamrodStroke 107 et 114, RamrodReturn 128, FullCockEnd 140. Tir : le chien tombe et la batterie s'ouvre (6-8).
- **Fusil plus détaillé** : contre-platine serpentine en laiton et vis de platine, ressort de batterie et sa vis, bride du bassinet, vis de noix, écusson du poignet, sous-garde, talon et vis de plaque de couche, embouchoir, clavettes du canon, porte-baguette d'entrée, hausse, tenon de baïonnette, bouton de baguette, cuir de silex ; cartouche de papier sur l'os `cartridge` (cachée dans la crosse au repos).
- **Manchettes et mains** (`scripts/character/bean_soldier.py`, `build_fp_arm`) : plus de Subsurf sur des tubes à deux anneaux (il rétrécissait la manchette en pilule : boutons et mitaine flottaient) ; manchette retournée tournée exactement, trois boutons à moitié enfoncés sur des galons, poignet qui entre dans la manchette, manche prolongée hors champ. `SM_Bean_FPArm` réimporté (`scripts/character/import_bean_fparm.py`).
- **Preuve** : testé éditeur, PIE solo sur `L_Test_Flat` le 2026-10-07 (hanche, tir, rechargement au ralenti `slomo 0.05` avec 18 captures, vue 3e personne) : `docs/production/captures/T-212/`. Non testé : multijoueur, visée (cadrage `AimMuzzleLocation` inchangé).
- **Reste** : `ReloadSeconds` vaut 1,5 s : les 6 s de chorégraphie passent 4 fois plus vite (choix de jeu au propriétaire, 3 s conseillé pour lire les gestes) ; en 3e personne, les bras courts du bean ne suivent pas toute la course de la baguette (le fusil descend de 18 cm au plus, la main peut rester ~25 cm sous la baguette au moment où elle ressort).

## v2 réaliste (2026-10-09, T-246) — statut : importé dans Unreal, compilé et testé éditeur le 2026-10-09 (Test Flat, PIE : prise en main, visée, tir, rechargement ; captures 171 à 225)
Demande du propriétaire : « améliorer le design de toutes les armes, créer des animations de rechargement et de tir type Call of Duty, créer des modèles de munitions, rendre les modèles des armes plus réalistes ». Même pack (`bean-musket-anim/1`, v0.4.0-local-v2), mêmes noms de clips, d'os, de sockets et d'événements ; construit par `blender -b --factory-startup --python scripts/props/musket_pack/build.py -- --save --product --render …` dans `build/packs/bean-musket-anim/` (l'ancien pack v0.3.0-local est gardé dans `build/packs/bean-musket-anim.v0.3.0-local.bak`). **Rien n'est importé dans Unreal** : import par l'intégrateur (`import_pirate_weapons.py` + `setup_pirate_weapons.py`, éditeur fermé).

### Modèle (`musket_model.py`)
Mousquet à silex type Brown Bess / 1777 à l'échelle du pack (0,92 m), mêmes pivots et lignes de repos que T-212 (`ROD_HEAD`, `MUZZLE`, `PAN`, `HAMMER_SPUR`, `FRIZZEN_TOE`, `CARTRIDGE_REST`), donc tous les clips et les sockets du jeu restent valides :
- crosse pleine en noyer loftée par sections asymétriques (`kit.section_loft`) : busc et plaque de couche, poignée fine, plats de platine, fût avec le canon logé dans une gorge demi-ronde et la baguette dans sa rainure (fonction `carve`) ;
- canon rond tourné d'un seul profil (`kit.lathe`) : renflement de culasse, ligne du bouchon, bouche ouverte avec une âme de 3 cm, queue de culasse vissée dans la poignée, tenon de baïonnette faisant guidon, hausse à encoche (`SIGHT_Z` = 2,15 cm au-dessus de l'axe du canon, guidon et encoche à la même hauteur) ;
- platine complète : plaque découpée (`kit.plate`), bassinet et sa cuvette, bride, ressort de batterie, chien col-de-cygne avec mâchoires, vis de mâchoire fendue, vis de chien, ergot, silex dans son cuir ; batterie (face, couvre-bassinet, orteil recourbé, vis) ;
- garnitures laiton : plaque de couche et son talon sur le busc, sous-garde à sangle plate (`kit.strap`) avec fleuron avant et longue queue arrière, plaque de détente, contre-platine serpentine et vis de platine, écusson du poignet, embouchoir, trois porte-baguette (celui d'entrée avec sa queue) ;
- battants fer, bretelle cuir tendue avec boucle, goupilles de fût ; baguette acier à tête en trompette (os `ramrod`), cartouche papier (os `cartridge`) ;
- `weapon_contract.sights` (hausse, guidon, hauteur au-dessus de l'axe) et `weapon_contract.triangles` dans le manifeste.

### Textures PBR cuites (`looks.py`)
Looks procéduraux (noyer veiné, fer bruni brossé et piqué, laiton patiné, cuir grainé, papier, corne, plomb, silex) avec usure par pointiness (arêtes polies) et occlusion (crasse dans les creux), dépliés par atlas (smart UV + pack) et cuits par Cycles CPU (environ 90 s) :

| Matériau (nœuds Image Texture labellisés `BaseColor` / `Normal` / `ORM`) | Atlas | Fichiers `textures/` |
|---|---|---|
| `M_Musket_Wood` | 2048 | `T_Musket_Wood_D` / `_N` / `_ORM.png` |
| `M_Musket_Iron` (fer, fer sombre, silex) | 2048 | `T_Musket_Iron_*` |
| `M_Musket_Brass` | 2048 | `T_Musket_Brass_*` |
| `M_Musket_Leather` | 1024 | `T_Musket_Leather_*` |
| `M_Musket_Paper` (papier + fil) | 512 | `T_Musket_Paper_*` |
| `M_Ammo_CartridgeBox`, `M_Ammo_PowderHorn` | 1024 | `T_Ammo_CartridgeBox_*`, `T_Ammo_PowderHorn_*` |
| `M_Ammo_MusketCartridge`, `M_Ammo_LeadBall` | 512 | `T_Ammo_Cartridge_*`, `T_Ammo_LeadBall_*` |

`_D` sRGB ; `_N` **DirectX** (vert vers le bas : importer dans UE sans inversion ; le .blend ré-inverse le vert pour son aperçu) ; `_ORM` linéaire, R = occlusion, G = rugosité, B = métal. `manifest.materials[<nom>].textures` donne les chemins par label (plus `baked`, `normal_convention`, `orm_channels`) : l'intégrateur crée les MI UE à partir de là. Les textures sont recopiées à côté des FBX `Bases/` et `Static/` (mode COPY) ; les FBX d'animation ne font que les nommer.

### Munitions (`ammo_model.py`, `exports/Static/`, `manifest.assets` type `static_mesh`, view `ammo`)
Pivot bas-centre, mètres, collision convexe `UCX_<Nom>_00` dans le FBX (plus un `.glb` d'aperçu), un atlas chacune :
- `SM_Ammo_MusketCartridge` : cartouche papier couchée (la même que celle du rechargement) ;
- `SM_Ammo_CartridgeBox` : giberne cuir ouverte (rabat relevé, badge laiton), bloc de bois à 20 trous avec 20 cartouches debout ;
- `SM_Ammo_PowderHorn` : corne à poudre couchée, bouchon de bois, anneau et bec laiton, bouchon tourné, bretelle cuir ;
- `SM_Ammo_LeadBall` : balle de plomb de 1,7 cm avec jet de coulée et couture.
La version courante de `import_pirate_weapons.py` (inchangée par T-246) lit les statiques du manifeste : son `--check` prévoit `Musket/Static/SM_Ammo_*` à côté du pickup ; à l'import, prendre la collision du fichier (UCX), pas l'auto-génération. Le pack `build/packs/ammo` (`ammo-pickups/1`, autre chantier : sac de grenades, caisse, baril) n'a aucun nom en commun ; le registre `AMMO_DEFS` de `setup_pirate_weapons.py` pourra pointer la giberne (20 cartouches), la corne et la balle.

### Clips (`animation.py`, 24 i/s, TP + FP)
- Easing « Call of Duty » (`ease_os`) : anticipation, milieu rapide, léger dépassement, stabilisation sur toutes les transitions de pose ; manipulations avec l'arme relevée et la platine tournée vers l'œil (poses `prime`, `load`, `look`, `show_barrel`).
- `Fire` : recul sec, canon +7°, arme reculée de 3,8 cm sur 2 images (8 → 10) puis retour en 10 images avec stabilisation ; `Recoil` / `ReturnToPort` cohérents. `Idle` / `Hold` : respiration ; `AimIdle` : respiration légère ; `Walk` : balancement léger ; `Sprint` : arme basse, inclinée, bien visible, bob marqué.
- `GroundReload` : **132 images (5,5 s)** au lieu de 144 ; fenêtres `HalfCock` 0-9, `PrimePan` 9-33, `CloseFrizzen` 33-38, `Powder` 38-62, `BallAndPatch` 62-72, `DrawRamrod` 72-86, `TurnRamrod` 86-93, `RamrodStroke` 93-110, `ReturnRamrod` 110-122, `Cock` 122-132 (clips de phase toujours livrés, mêmes longueurs qu'avant). Événements : HalfCockStart 3, PanPrime 27, FrizzenClose 37, StockLowered 48, PowderCharge 55, BallAndPatch 67, RamrodDraw 75, RamrodTurn 89, RamrodStroke 99 et 104, RamrodReturn 118, FullCockEnd 130. Mordre la cartouche, amorcer, poudre, balle + papier, deux coups de baguette avec à-coup net, remise de la baguette, armer.
- **Nouveau `Inspect`** (96 images, 4 s) : platine relevée vers l'œil, puis canon et bouche, retour.
- **Nouveau `Discard`** (32 images, 1,33 s, demande du 2026-10-09) : déclic du chien à vide (`Empty` image 1, socket `socket_pan`), regard sur l'arme, secousse sèche, lancer à droite et vers le bas (`Release` image 22, `socket_grip_r`). Le clip bean finit mains vides ; le clip arme **continue la chute** après `Release` (TP : l'arme se pose au sol, platine en l'air) ; le C++ la lâche en pickup à `Release`, le reste du clip arme n'est qu'un aperçu.
- Cadrage FP (`fp_framing`) : hanche `hip_grip_location_cm` / `hip_rotation_pyr_deg` (écrits par `setup_pirate_weapons.py`) ; visée **`aim_muzzle_location_cm = [95, 0, -2.15]`, `aim_rotation_pyr_deg = [0, 0, 0]`** : ligne de mire (hausse → guidon) exactement sur l'axe de la caméra, arme centrée, bouche à 95 cm (hausse à 40 cm de l'œil, crosse derrière la caméra, coupée par le plan proche de 10 cm) ; `aim_field_of_view_deg` 65 ; `sight_line` ; `gestures` = position de l'œil dans l'arme et grip / bouche / rotation dans la vue par geste (Idle, AimIdle, Sprint, GroundReload@20/60/100, Inspect@30/70, Discard@14/22). `AimIdle` garde la pose de base : le jeu mesure `Idle` et `AimIdle` image 0 et une vraie pose de visée dans le pack ferait sauter l'unique clip `Fire`, aussi joué en visée via l'alias `AimFire`. **`setup_pirate_weapons.py` n'écrit que la hanche : reporter `aim_*` à la main dans `DA_PirateMusketRules` (AimMuzzleLocation, AimRotation).**

### Rendus de contrôle (`build/packs/bean-musket-anim/renders/`)
`product-musket.png` (vue produit côté platine), `product-musket-lock.png`, `product-musket-3-4.png`, `product-ammo.png` ; FP `GroundReload` 14/28/54/99/118, `Fire` 10, `Idle` 0, `Sprint` 12, `Inspect` 30/70, `Discard` 14/22/26 ; `FPAIM-AimIdle-000` (caméra placée comme le cadrage de visée du jeu : mire sur l'axe) ; TP `Hold` 0, `Fire` 10, `GroundReload` 54/99, `Inspect` 30, `Discard` 22/32.

### Écarts et suites
- `PirateWeaponClips.inl` **n'est pas régénéré** : `GroundReload` 132 images, `Inspect` et `Discard` nouveaux, donc `pirate_weapon_clips.py --check` dit STALE ; `Discard` exige d'abord `EFirearmGesture::Discard` dans `FirearmRules.h` (gameplay), puis régénération et compilation. Le `--check` de `import_pirate_weapons.py` échoue pour cette seule raison (fichiers, sha256 et unités OK).
- 3e personne : la main droite du bean (bras courts) reste jusqu'à environ 27 cm sous la baguette au plus haut de sa course (limite connue T-212 : le fusil descend de 18 cm au plus).
- Maquette jugée sur rendus Blender seulement : non testé éditeur, non testé en jeu.

## Visée désactivée (2026-10-09, T-254) — statut : compilé (fichier seul), non testé
Demande du propriétaire : « faire en sorte qu'avec les armes je ne puisse pas viser », « comme dans Counter-Strike ». `UMusketRules.bAllowAiming` (défaut `false`, `DA_MusketRules` hérite) : `AMusketWeapon::SetAiming` et `ServerSetAiming` ignorent le clic droit quand il est faux, le mousquet reste à la hanche et sa dispersion suit la posture (voir `docs/features/armes-nouvelles.md`, « Visée désactivée »). Les valeurs de cadrage en visée (`AimMuzzleLocation`, `AimRotation`, `AimFieldOfView`) restent dans le DataAsset pour rallumer la visée en un clic.
