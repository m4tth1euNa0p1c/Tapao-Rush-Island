# Bean : template d'animation Unreal

Le template couvre les 30 animations du player Bean, toutes sur le squelette **SK_Bean_Body** (`SKEL_Bean`), à 24 i/s et sans root motion : c'est le CharacterMovement qui déplace Bean.

| Fichier | Contenu |
|---|---|
| `BeanAnimManifest.json` | Pour chaque animation : fichier FBX, plage, boucle, slot de montage, notifies (temps en secondes), blend in/out et vitesse sans glissement. S'y ajoutent les blendspaces, les sockets d'attache et la courbe des avirons. |
| `import_bean_animations.py` | Script Python de l'éditeur Unreal. Il importe les FBX, règle les boucles, crée une classe de notify par nom, place les notifies, et crée les montages et les blendspaces. |
| `../exports/anims/*.fbx` | Les 30 animations. |
| `../../bean-musket-anim/exports/A_Musket_*.fbx` | Animations du mousquet (chien, batterie, détente, baguette), synchronisées avec `A_Bean_Musket_Reload` et `A_Bean_Musket_Fire`. |
| `../../bean-musket-anim/exports/fp_poses.json` | Poses de la vue à la première personne pour le mousquet. |

## 1. Import
1. Vérifie `SKELETON` en haut du script. Par défaut : `/Game/MonkeIsland/Characters/Bean/SK_Bean_Body_Skeleton`.
2. Lance-le dans Unreal : *Tools > Execute Python Script…* (plugin *Python Editor Script Plugin* activé).
3. Résultat dans `/Game/MonkeIsland/Characters/Bean/Animations` :
   - les séquences `A_Bean_*` ;
   - les montages `AM_Bean_*` ;
   - les classes de notifies `AN_*` ;
   - les blendspaces `BS_Bean_Locomotion` et `BS_Bean_Carry`.

## 2. AnimBP `ABP_Bean`
**AnimGraph** : machine d'états `Locomotion`, puis slot `DefaultGroup.FullBody`, puis *Layered blend per bone* à partir de `spine_01` pour la couche haut du corps (slot `UpperBody.UpperBody` + poses d'arme), puis Output.

### Machine d'états `Locomotion`
| État | Animation | Entrée / sortie |
|---|---|---|
| Ground | `BS_Bean_Locomotion` (Speed en cm/s) | état par défaut |
| JumpStart | `A_Bean_Jump_Start` | `IsFalling && VelocityZ > 0` |
| JumpLoop | `A_Bean_Jump_Loop` (boucle) | fin de JumpStart, ou chute |
| JumpLand | `A_Bean_Jump_Land` | `!IsFalling`, puis retour à Ground à la fin |
| Carry | `BS_Bean_Carry` | `bIsCarrying` (coffret, baril, totem : objet tenu à 2 mains) |
| BoatSit | `A_Bean_Boat_Sit_Idle` (boucle) | `bInBoat && RowInput == 0` |
| BoatRow | `A_Bean_Boat_Row` (boucle) | `bInBoat && RowInput != 0`, vitesse de lecture = \|RowInput\| |
| Dead | `A_Bean_Death` (sans boucle, garder la dernière pose) | `bIsDead` |

**Vitesses sans glissement des pieds :**
- marche : 49 cm/s ;
- course : 112 cm/s ;
- porter : environ 45 cm/s.

Si le jeu va plus vite, augmente le *rate scale* proportionnellement : par exemple ×2 à 98 cm/s.

### Couche haut du corps (arme en main)
| Arme active | Pose haut du corps en boucle | Montages (slot UpperBody) |
|---|---|---|
| Mousquet (touche 1) | `A_Bean_Musket_Hold` ; `A_Bean_Musket_Aim` quand on vise (clic droit) | `AM_Bean_Musket_Reload`, `AM_Bean_Musket_Fire`, `AM_Bean_Musket_Lower` |
| Sabre (touche 2) | `A_Bean_Sabre_Idle` | `AM_Bean_Sabre_Draw`, `AM_Bean_Sabre_Slash_R`, `AM_Bean_Sabre_Slash_L`, `AM_Bean_Sabre_Thrust`, `AM_Bean_Sabre_Finisher`, `AM_Bean_Sabre_Block`, `AM_Bean_Sabre_Sheathe` |
| Mains libres | aucune | `AM_Bean_Interact`, `AM_Bean_Eat_Coconut` |

Pendant les montages FullBody (Pickup, Hit_React, Wave, Celebrate), coupe la couche haut du corps : poids à 0 tant que le slot FullBody joue.

## 3. Entrées → animations
| Entrée | Action | Animation |
|---|---|---|
| ZQSD / stick | se déplacer | blendspace Locomotion (Carry si on porte) |
| Espace | sauter | JumpStart → JumpLoop → JumpLand |
| E | ramasser (objet au sol) / interagir | `AM_Bean_Pickup` / `AM_Bean_Interact` |
| F | manger une noix de coco | `AM_Bean_Eat_Coconut` (soin sur `AN_Heal`) |
| 1 | mousquet | `AM_Bean_Musket_Lower` pour ranger |
| 2 | sabre dégainé / rengainé | `AM_Bean_Sabre_Draw` / `AM_Bean_Sabre_Sheathe` |
| Clic gauche | tir / combo de sabre | `AM_Bean_Musket_Fire` ; combo Slash_R → Slash_L → Thrust (`AN_ComboWindow`) |
| Clic gauche maintenu 0,4 s | coup vertical au sabre | `AM_Bean_Sabre_Finisher` |
| Clic droit | viser / parer | `A_Bean_Musket_Aim` / `AM_Bean_Sabre_Block` |
| R | recharger le mousquet (6,0 s) | `AM_Bean_Musket_Reload` + `A_Musket_Reload` sur SK_Bean_Musket |
| ZQSD dans la barque | ramer | BoatRow (+ courbe des avirons) |
| T / G | saluer / célébrer | `AM_Bean_Wave` / `AM_Bean_Celebrate` |

## 4. Notifies (événements gameplay)
| Notify | Animation | Effet attendu |
|---|---|---|
| `AN_Footstep_L` / `AN_Footstep_R` | Walk, Run, Carry_Walk | son de pas, poussière |
| `AN_JumpTakeoff`, `AN_Land` | Jump | son, poussière, petite secousse caméra |
| `AN_PickupAttach`, `AN_PickupStore` | Pickup | l'objet suit la main droite, puis va dans l'inventaire |
| `AN_Interact` | Interact | déclenche l'interaction |
| `AN_Drink_Gulp` ×3, `AN_Heal`, `AN_Coconut_Throw` | Eat_Coconut | sons, soin, la noix est jetée ou détruite |
| `AN_RowStroke_Catch`, `AN_RowStroke_Splash` | Boat_Row | impulsion de la barque, éclaboussure |
| `AN_HitFlash`, `AN_BodyFall` | Hit_React, Death | flash, son de chute |
| `AN_Reload_HalfCock` … `AN_Reload_Complete` | Musket_Reload | étapes ; si on interrompt avant `Complete`, l'arme n'est pas chargée |
| `AN_FireTrigger`, `AN_PanFlash`, `AN_FireShot`, `AN_Recoil` | Musket_Fire | départ 0,12 s après la détente : projectile et effets sur `AN_FireShot` |
| `AN_Draw_Grab`, `AN_Draw_AttachToHand`, `AN_Draw_Complete` | Sabre_Draw | le sabre passe du socket du fourreau à la main |
| `AN_Stamina_Consume` | chaque coup de sabre | −11 % d'endurance (refusé en dessous de 11 %) |
| `AN_HitWindow_Begin` / `AN_HitWindow_End` | coups de sabre | balayage de la lame (sockets grip → tip, capsule de 3 cm) |
| `AN_Dismember_Check` | Finisher | si le coup tue la cible : découpe du mob selon le plan de la lame |
| `AN_BlockWindow_Begin` / `End`, `AN_BlockImpact` | Sabre_Block | parade active (−80 % de dégâts, −11 % d'endurance) |
| `AN_Sheathe_Insert`, `AN_Sheathe_AttachToScabbard` | Sabre_Sheathe | le sabre retourne au fourreau |

## 5. Sockets à créer sur SK_Bean_Body
Les valeurs sont dans `BeanAnimManifest.json > attach_sockets`. Elles ont été calculées dans Blender : vérifie le roll dans l'aperçu de socket.

| Socket (os) | Objet |
|---|---|
| `musket_grip` (hand_r) | SK_Bean_Musket (pivot grip_r) ; main gauche en IK sur son socket `grip_l` |
| `sabre_grip` (hand_r) | SM_Bean_Cutlass |
| `scabbard` (pelvis) | SM_Bean_Scabbard (hanche gauche, pendant pivotant) |
| `sabre_sheathed` (pelvis) | SM_Bean_Cutlass au fourreau |
| `coconut` (hand_r) | noix de coco pendant `Eat_Coconut` |

## 6. Barque
- **Position de Bean** : assis sur une caisse de nage 0,35 m en arrière des dames de nage, regard vers la poupe. La transform est dans le manifeste (`boat`).
- **Avirons** : `SM_Barque_Oar` ×2. À chaque image, applique `theta_deg` et `phi_deg` de `boat.oar_curve_per_frame` en suivant la position de lecture de `A_Bean_Boat_Row` :
  - yaw bâbord = 90 − θ ;
  - yaw tribord = −90 + θ ;
  - pitch = −φ.

## 7. Première personne
- **Mousquet** : `fp_poses.json`, dans `bean-musket-anim/exports`.
- **Sabre** : pas encore de poses FPS. À ajouter si la vue par défaut reste à la première personne.

## 8. Régénérer
Dans Blender (dépôt `maquette-blender`) :
```
uv run factory/tools/maquette.py build bean-anim-library --headless
blender -b maquettes/bean-musket-anim/bean-musket-anim.blend --python factory/blender/export_anim_ranges.py -- maquettes/bean-anim-library/ranges_musket.json maquettes/bean-anim-library/exports/anims
blender -b maquettes/bean-cutlass-anim/bean-cutlass-anim.blend --python factory/blender/export_anim_ranges.py -- maquettes/bean-anim-library/ranges_sabre.json maquettes/bean-anim-library/exports/anims
python factory/tools/make_ue_template.py
```
