# Fusil à pompe pirate de Bean (T-156)

**Lot :** L5, demande de l'utilisateur du 2026-10-06. Comme le mousquet, c'est une extension « armes à feu » du cahier §5, à arbitrer dans l'ADR-001. Le fusil à pompe est un anachronisme assumé : c'est de la fiction.
**Maquette :** `bean-pirate-shotgun` v0.1.0, produite sur le 2e PC (`OneDrive\maquette-blender\maquettes\`, lecture seule). Elle contient 16 gestes, chacun en 1re et en 3e personne, à 30 images/s. Le manifest est `shotgun-manifest.json`. Le pack n'a jamais été testé dans Unreal (`runtime_tested: false`).
**Recette :** tests `monke_island.Weapons.PirateFirearms`.

## Règles (DA_ShotgunRules, `UShotgunRules`)
| Valeur | Défaut |
|---|---|
| Capacité de l'arme (`Capacity`) | 4 cartouches (tube et chambre) |
| Cartouches par joueur (`MaxShells`) | 24, dont 4 dans l'arme au départ (`StartLoadedShells`) |
| Plombs par tir (`PelletCount`) | 8 |
| Dégâts d'un plomb | 16, courbe `DamageFalloff` (T-167) : 100 % jusqu'à 5 m, 35 % à 10 m, 8 % à 25 m (`RangeCm`), linéaire entre les points |
| Dégâts sur un membre (`LimbDamageScale`) | ×2,5 : un membre de tiki se déchire plus vite que le corps |
| Rayon de touche d'un os (`BoneHitRadiusCm`) | 18 cm |
| Dispersion du cône à la hanche / en visée | 2,5° / 1,8° (≈ 17 cm à 4 m, 44 cm à 10 m) |
| Déchiquetage à bout portant (`CloseShredLimbsMin/Max`) | 2 à 3 membres |
| Éclatement des tiki à courte portée (`CloseBurstDistanceCm`) | 5 m |
| FOV de la vue du pack / FOV en visée | 90° (objectif de 18 mm) / 75° |
| Tir et pompe | 2,0 s (`FirePump`, 72 images, joué à partir de `Fire`, image 12). La douille sort à `EjectSpentShell`, image 30 |
| Recharge | `ReloadStart` 1,4 s, puis `ReloadInsert` 1,8 s par cartouche (comptée à `InsertSlug`, image 36), puis `ReloadEnd` 1,4 s |
| Recharge à vide | `ReloadEmpty`, 8,0 s : trois cartouches aux images 68, 123 et 178, puis la pompe |

## Munitions et arme vide (T-249, 2026-10-09)
- `AmmoKind` = `Shell` : `DA_Ammo_ShellBox` et la ligne du marchand remplissent la poche (`AddAmmo`, borné par `MaxShells`).
- Fusil vide (rien dans l'arme, poche à zéro) : la gâchette joue `Discard` (ou `DryFire` sans ce clip) et le fusil est jeté au sol comme un pickup, compteurs gardés ; l'ancien retrait automatique après la dernière cartouche n'existe plus (`bDiscardWhenEmpty`, voir `armes-nouvelles.md`). Le tromblon (`ABlunderbussWeapon`) reprend le tir à plombs de ce fusil sur les gestes du mousquet.

## Tir (serveur)
- **Plombs** : chaque plomb est un tracé depuis les yeux du porteur, dans le cône (`FirearmMath::SpreadDirections`).
  - Chaque plomb a sa trace, sa distance, ses dégâts (courbe) et **son os** (T-167) : l'os de la touche si le mesh collisionne, sinon l'os dont le plomb passe le plus près (`FirearmDamage::FindStruckBone`, la capsule prend la trace), à `BoneHitRadiusCm` au plus.
  - Les plombs d'une même cible font **un seul coup** (`FFirearmDamageEvent`, sous-classe de `FPointDamageEvent`, `TakeDamage` direct) : le total pour la santé (une réaction, une mort), les groupes **par os** (`BoneHits`), le point d'entrée de chaque plomb (`Impacts`, sang), `LimbDamageScale`, `ShredLimbs` et `CloseBurstDistance` (`AShotgunWeapon::DescribeShot`). `HitInfo.BoneName` = l'os du groupe le plus fort. Une cible qui ne connaît pas l'événement lit un `FPointDamageEvent`.
  - Le recul est au prorata des plombs qui touchent.
  - L'impulsion physique est donnée plomb par plomb.
- **Tiki** (T-150) : `TikiCombat::EWeapon::Shotgun` prend les réactions du mousquet. Un tir mortel à 5 m ou moins fait toujours éclater le tiki (`FBlow::CloseBurstDistance`).
  - Le pistolet prend les mêmes réactions (`EWeapon::Pistol`).

## Recharge cartouche par cartouche (`FirearmMath`, serveur)
- Le serveur enchaîne les gestes :
  - arme vide → `ReloadEmpty` (au plus trois cartouches, puis la pompe ; non interruptible) ;
  - sinon `ReloadStart` → `ReloadInsert` × N → `ReloadEnd`.
- N s'arrête quand l'arme est pleine ou que la poche est vide.
- **Interruption** : un clic de tir pendant `ReloadStart`, `ReloadInsert` ou `ReloadEnd`, avec au moins une cartouche dans l'arme, coupe la recharge et tire tout de suite. Les cartouches déjà entrées restent.
- Une cartouche n'est comptée qu'à l'événement `InsertSlug`. Une insertion coupée avant cet événement ne compte pas.
- Clic sans cartouche dans l'arme : recharge s'il en reste dans la poche (`bAutoReloadOnEmptyFire`), sinon `DryFire`.

## Animations 1re / 3e personne (`AFirearmWeapon`, commun au pistolet)
Pour chaque geste, quatre clips jouent **ensemble, depuis le même instant** (`ApplyClipToMeshes`) : le corps du bean et l'arme, en 1re et en 3e personne. Conformément aux READMEs du pack, l'arme n'est jamais attachée une seconde fois à la main : sa racine est posée sur la racine du bean, car son mouvement est déjà calculé dans l'espace du bean.
- **1re personne** (porteur seulement) :
  - un `FirstPersonRig` est attaché à la caméra avec l'inverse de la caméra FP du pack, calculée depuis le manifest et convertie aux axes d'Unreal (`FirstPersonViewLocation` / `Rotation`) ;
  - les bras du pack (`SK_Bean_Shotgun_FP`, sur le squelette du bean du projet) et l'arme y sont posés à l'identité ;
  - le FOV passe à celui du pack, pour que le cadrage soit celui des rendus ;
  - les bras procéduraux du bean sont masqués (`UBeanFirstPersonArmsComponent::SetHiddenByItem`, seul ajout dans `Character/Bean/`).
- **3e personne** :
  - comme le mousquet, un corps de bean caché joue le clip TP, et ses os du haut du corps (`UpperBodyBones`) sont copiés sur la marionnette (`SetBoneOverrideSource`) ; les jambes gardent la marche du bean ;
  - l'arme garde, par rapport à l'os réel `AnchorBone` (`hand_r`) de la marionnette, la place qu'elle a par rapport à ce même os dans le clip : `inverse(os du clip) × os réel`. Les échelles 100 des os s'annulent.
- **Qui joue quoi** :
  - le serveur décide des gestes qui touchent aux munitions et les réplique (`FFirearmAction` : geste, numéro, début du clip, heure serveur) ;
  - chaque machine les joue là où ils en sont : un joueur qui arrive en retard reprend en cours de route ;
  - la logique tourne sur des timers aux événements du manifest ;
  - les boucles (`Idle`, `Walk`, `Sprint`, `AimIdle`) et les transitions `AimIn` / `AimOut` sont locales ;
  - le porteur voit son tir tout de suite (prédiction), et le serveur décide de la touche.
- **Effets** (sur chaque machine, aux événements du clip joué) :
  - flamme, lumière et son du tir à `socket_muzzle` ;
  - sons provisoires du mousquet aux autres événements ;
  - douille `SM_Shotgun_Spent` éjectée à `EjectSpentShell` (pool de 4, chute simple avec un tracé par image).

## Assets (`/Game/MonkeIsland/Weapons/Shotgun/`)
| Asset | Rôle |
|---|---|
| `SK_Bean_PirateShotgun` | l'arme, avec son propre squelette : `gun_root`, `pump`, `bolt`, `trigger`, `shell_load`, `shell_eject`, `socket_*` |
| `SK_Bean_Shotgun_FP` | les bras 1re personne, sur le squelette du bean du projet |
| `Animations/Bean/A_Bean_{TP,FP}_Shotgun_*`, `Animations/Weapon/A_Shotgun_{TP,FP}_*` | 32 + 32 clips |
| `Static/SM_Bean_PirateShotgun` | l'objet au sol, avec collision |
| `Static/SM_Shotgun_Full`, `SM_Shotgun_Spent`, `SM_Shotgun_Slug` | cartouche pleine, douille vide, balle |
| `DA_ShotgunRules`, `BP_Shotgun`, `DA_Item_Shotgun` | créés par `setup_pirate_weapons.py` |

Les commandes de mise en place sont dans `pirate-pistol.md` : le même import et le même setup traitent les deux armes (filtre `MONKE_PIRATE_WEAPONS`).

## Limites
- Rien n'est encore vérifié dans Unreal : ni l'import, ni le cadrage en 1re personne, ni la synchronisation en 3e personne, ni le réseau.
- **Échelle de la racine** : l'import contrôle que la racine des clips du pack a la même échelle (100) que les clips du bean du projet (`pirate_weapons_report.json`). Si elle diffère, le bean change de taille pendant les gestes, et il faudra corriger l'import.
- La recharge n'est pas prédite chez le porteur : elle part au retour du serveur.
- Pas de `Holster` en jeu. Les changements de clip font un saut, sans fondu.
- La flamme est celle du mousquet, orientée selon la visée. Pas de fumée.
- Les douilles traversent les objets fins, faute de vraie physique.

## Balistique (T-167, lot L5)

| Distance | Effet sur un tiki (60 / 80 PV) |
|---|---|
| 0 à 5 m | 8 × 16 = 128 si tous les plombs touchent (5 suffisent pour 80) : **mort en un tir**, éclatement (`MusketBurst`) et **déchiquetage** de 2 à 3 membres (y compris ceux déjà arrachés par les plombs). Chaque plomb sur un membre lui fait 40 : il est arraché. |
| ~10 m | 5,6 par plomb, environ la moitié du cône sur le corps : 22 à 45, **jamais mortel seul**. Un bras ou la tête touché par 2 plombs (2 × 14 > 25), une jambe par 3 plombs, est arraché. |
| 15 à 25 m | 4 à 1,3 par plomb : égratignures. |

Effets : son `SW_Shotgun_Fire` (généré, `scripts/props/shotgun_shot_sound.py`, SC_Effects), explosion de bouche `MuzzleBlastSystems` = `NS_Shotgun_MuzzleFlame` + `NS_Shotgun_MuzzleSmoke` + `NS_Shotgun_Sparks` (la gerbe d'étincelles dessine le cône), bouffée de sable `ImpactSystem` = `NS_Firearm_Impact` à chaque plomb qui touche le décor (`MulticastShotImpacts`), sang `NS_Blood_MusketImpact` orienté à chaque plomb qui touche un tiki (`ATikiCreature::MulticastBloodHits`, une tache au sol par tir). Création : `scripts/props/import_shotgun_sounds.py` (console Python de l'éditeur) puis `python scripts/vfx/build_firearm_vfx_ue.py` (MCP, verrou pris, sans PIE).
Tests : `monke_island.Firearm.Ballistics.*` (courbe, mortel à 4 m, non mortel à 10 m, arrachement par membre, regroupement par os, os le plus proche).

## v2 réaliste (2026-10-09) — statut : importé dans Unreal, compilé et testé éditeur le 2026-10-09 (Test Flat, PIE : prise en main, visée, tir, rechargement ; captures 171 à 225)

**Demande du propriétaire :** « améliorer le design de toutes les armes, animations de rechargement et de tir type Call of Duty, modèles de munitions, armes plus réalistes », puis (même jour) le geste `Discard`, la visée au guidon et un sprint marqué. **Tâche :** T-247 (lot L5, `ue-blender-artist`). **Rien n'est importé ni testé dans Unreal** : pack construit et vérifié hors éditeur seulement.

**Pack local** (construit dans le dépôt, comme le mousquet depuis T-212) : `scripts/props/shotgun_pack/` (`build.py`, `shotgun_model.py`, `textures.py`, `animation.py`, `export_pack.py`, `check_pack.py`, `pack.json`), sortie `build/packs/bean-pirate-shotgun/` (`shotgun-manifest.json` au format `bean-pirate-shotgun/1` v0.2.0-local, `exports/{Bases,Animations,Static}`, `textures/`, `renders/`, `fp-framing.json`, `validation/grips/`, `bean-pirate-shotgun.blend`). Le kit du mousquet (`scripts/props/musket_pack/kit.py`) et les solveurs / la caméra FP de son `animation.py` sont réutilisés tels quels.

```powershell
& "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/shotgun_pack/build.py -- --product --save --render FP:FirePump:14,FP:ReloadInsert:30,TP:Idle:0
python scripts/props/shotgun_pack/check_pack.py        # contrôle hors Unreal : CHECK OK le 2026-10-09
```

**Contrat conservé (le C++ et l'import ne changent pas) :** mêmes 16 gestes, mêmes longueurs, mêmes événements et mêmes images que la v0.1 (vérifié ligne à ligne contre `PirateWeaponClips.inl` par `check_pack.py`), mêmes 13 os (`gun_root`, `pump`, `bolt`, `trigger`, `shell_load`, `shell_eject`, 7 `socket_*`) aux mêmes positions, mêmes noms de fichiers (`SK_Bean_PirateShotgun`, `SK_Bean_Shotgun_{TP,FP}`, `A_Bean_{TP,FP}_Shotgun_<Geste>`, `A_Shotgun_{TP,FP}_<Geste>`, `SM_Bean_PirateShotgun`, `SM_Shotgun_{Full,Spent,Slug}`), mètres (UnitScaleFactor 100), armature exportée sous le nom `Armature`, 30 i/s. Longueur 0,728 m, 23 908 triangles, un seul matériau `M_Shotgun`.

**Modèle :** fusil à pompe type Winchester 1897 à l'échelle de Bean : crosse et fût (pompe nervurée à 12 anneaux, viroles laiton) en noyer veiné, boîtier et canon en acier bleui avec usure des arêtes et rayures, fenêtre d'éjection réelle (découpe booléenne) avec culasse visible qui recule avec la pompe (os `bolt`, 0,9 × course), fenêtre de chargement dessous avec élévateur laiton, chien externe armé, tube magasin et bouchon vissé, collier de canon, barres d'action, guidon laiton (`SIGHT` = (0,412 ; 0 ; 0,1123) m), détente animée.
**Textures PBR cuites** (Cycles, 2048 pour l'arme, 1024 pour les munitions ; dépliage Smart UV Project unique) : `T_Shotgun_BaseColor.png` (sRGB), `T_Shotgun_Normal.png` (tangent, **convention DirectX, vert vers le bas** : ne pas inverser le vert à l'import), `T_Shotgun_ORM.png` (linéaire : R occlusion, G rugosité, B métal). Nœuds Image Texture labellisés `BaseColor` / `Normal` / `ORM` ; `manifest.materials[].textures` les liste par label ; même jeu pour `M_Shotgun_Spent`, `M_Ammo_Shell`, `M_Ammo_ShellBox`. La couleur de base est cuite par émission (un bake DIFFUSE/COLOR rend les métaux noirs). L'ORM n'est pas câblé par l'import FBX d'Unreal : à brancher dans le matériau après import.
**Munitions** (`exports/Static/`, pivot bas-centre, un UCX convexe chacune, listées `type: static_mesh`, `view: ammo`) : `SM_Shotgun_Spent` (douille vide laiton + papier roussi, même rôle qu'aujourd'hui : `ShotgunRules.SpentShellMesh` et douille des clips), `SM_Ammo_Shell` (cartouche pleine, sertissage en étoile, bande imprimée), `SM_Ammo_ShellBox` (boîte kraft ouverte de 12 cartouches, étiquette couronne-ancre), plus `SM_Shotgun_Full` (= `SM_Ammo_Shell` sous son nom v0.1) et `SM_Shotgun_Slug` pour la liste d'import inchangée. La cartouche tenue pendant `ReloadInsert` reste sur l'os `shell_load`, la douille éjectée sur `shell_eject` (v0.1).

**Animations (type Call of Duty), FP écrites dans la caméra du jeu** (œil à (0, 0, 0,80) m, 90° ; bras FP posés sans étirement, 3e personne par solveur deux segments comme le mousquet) :
- `FirePump` / `AimFirePump` : recul sec en 2 images (canon +8°, arme reculée 4 cm à l'image 13 pour un tir à 12), retour amorti, pompe nette (21 → 33, retour 38 → 47), douille balistique dès `EjectSpentShell` (30), épaules qui encaissent en TP ;
- `ReloadInsert` : arme couchée en travers, bouche en haut à gauche, fenêtre de chargement vers l'œil au centre ; cartouche visible au bout de la moufle gauche de `ShellPickup` (17) à `InsertSlug` (36), poussée dans le tube (sertissage en avant) ; `ReloadEmpty` enchaîne trois insertions puis la pompe ; `ReloadStart` / `ReloadEnd` amènent et ramènent l'arme ;
- `Idle` respiration (deux sinus), `Walk` balancement, `Sprint` arme basse à la hanche, inclinée, bob marqué (×2,2) ; `AimIn` / `AimOut` en 26 images avec un petit tassement, `AimIdle` respiration légère ; `Inspect` roulis −55° / +50° et demi-course de pompe ; `DryFire`, `Equip`, `Holster` ;
- **visée** : `AimIdle` image 0 = guidon laiton exactement sur l'axe de la caméra à 70 cm, canon à +2,2° pour que l'arrière du boîtier et le chien passent 1,6 cm sous la ligne de mire ;
- **`Discard`** (nouveau, 40 images = 1,33 s, TP + FP) : le bean regarde le fusil (boîtier au centre), actionne la pompe à vide d'un coup sec (événement `Empty` à l'image 9), le lance à droite et en bas (événement `Release` à l'image 24) ; le clip bean finit mains vides (bras baissés hors champ en FP, le long du corps en TP) ; **le clip arme continue la chute** après `Release` (vol balistique, atterrissage et repos au sol vers l'image 36 : à droite du bean en TP, hors champ en bas à droite en FP) ; le jeu peut l'arrêter à `Release`.

**Cadrage FP à copier dans `DA_ShotgunRules`** (`fp-framing.json`, reproduit exactement la caméra du pack) : `HipGripLocation` = (30, 14, −17) cm, `HipRotation` = (6,37 ; −8,17 ; −7,96), `AimMuzzleLocation` = (71,37 ; 0 ; −1,88) cm, `AimRotation` = (2,18 ; 0 ; 0) ; `FirstPersonViewLocation` / `FirstPersonFieldOfView` comme pour le mousquet (caméra à 80 cm, 90°).

**Rendus** (`build/packs/bean-pirate-shotgun/renders/`) : `FP-FirePump-012/014/034`, `FP-ReloadInsert-024/030/036`, `TP-Idle-000`, `FP-Idle-000`, `FP-AimIdle-000`, `FP-Sprint-020`, `FP-Discard-008/022`, `TP-Discard-020`, `TP-FirePump-034`, `TP-ReloadInsert-028`, `FP-Inspect-072`, `FP-ReloadEmpty-120` (EEVEE), `product-weapon`, `product-side` (vue de dessus, fenêtre d'éjection), `product-ammo` (Cycles).

**Intégration (intégrateur, éditeur fermé, verrou pris) :** dans `scripts/character/import_pirate_weapons.py`, `WEAPONS["Shotgun"]["local_pack"] = True` (le pack est alors lu dans `build/packs/bean-pirate-shotgun/`) et ajouter `SM_Ammo_Shell`, `SM_Ammo_ShellBox` à sa liste `static` ; dans `scripts/character/pirate_weapon_clips.py`, pointer `WEAPONS["Shotgun"]` sur `build/packs/bean-pirate-shotgun/shotgun-manifest.json` ; puis le commandlet d'import, `setup_pirate_weapons.py` et les valeurs de cadrage ci-dessus.

**Écarts et limites :**
- `Discard` est un 17e geste : `pirate_weapon_clips.py` émettrait `EFirearmGesture::Discard`, qui n'existe pas dans `FirearmRules.h` : ajouter la valeur (et la logique « plus de cartouches → Discard ») côté C++, ou ignorer les gestes inconnus dans le générateur. Rien du C++ n'a été touché.
- `ReloadInsert` garde ses 54 images (1,8 s) pour ne pas changer la table C++ ; le mouvement utile (prise → insertion) dure 0,63 s. Pour « ≤ 1,2 s par cartouche », régler `PlayRate = 1,5` sur le clip dans `DA_ShotgunRules`.
- La douille cuite sur `shell_eject` vole dès `EjectSpentShell` (comportement v0.1) et le jeu lance aussi sa propre `SM_Shotgun_Spent` : vider `SpentShellMesh` dans le DA si elle apparaît en double.
- La posture TP reste celle de la v0.1 (arme en travers, bras courts de Bean, étirement ≤ 1,13 sauf `Discard` 1,32) ; en jeu seuls les os du haut du corps sont copiés.
- Masse `physics.mass_kg` du manifeste (8,2 kg) calculée sur des volumes pleins : indicative, pas une donnée de jeu.
- Non vérifié dans Unreal : import, cadrage FP réel, synchronisation TP, réseau.
