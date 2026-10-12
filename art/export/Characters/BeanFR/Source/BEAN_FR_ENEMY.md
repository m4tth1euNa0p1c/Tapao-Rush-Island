# Beans français : brancher les ennemis dans Unreal

Toutes les données sont dans `BeanFR_EnemyManifest.json` : skins, fichiers, matériaux, polycount, sockets, animations avec leurs notifies, et les réglages d'IA suggérés.

## 1. Squelette et skins
- Les 7 skins utilisent le **même squelette** que le player (`SK_Bean_Body_Skeleton`). À l'import FBX, choisis ce squelette existant au lieu d'en créer un nouveau.
- Chaque skin a ses fichiers dans `exports/<Nom>/` :
  - `SK_BeanFR_<Nom>.fbx` : personnage complet (corps, tenue et coiffure). C'est l'option recommandée pour un ennemi, un seul skeletal mesh.
  - `SK_BeanFR_<Nom>_Body.fbx`, `_Outfit.fbx` et `SM_BeanFR_<Nom>_Hat.fbx` : version modulaire, avec les mêmes conventions que le player. Le chapeau est un static mesh, avec son pivot au contact de la tête, et s'attache au socket `hat_socket` de l'os `hat`.
  - `SK_BeanFR_<Nom>.glb` : aperçu web.
- Les noms de matériaux sont `MAT-*` (laines, cuirs, galons, `MAT-BeanFR_Skin`, `MAT-BeanFR_Eye_Red`, `MAT-BeanFR_Hair`). Les couleurs de base sont exportées. L'usure procédurale ne sert qu'aux rendus Blender.
- Le polycount va de 84 000 à 97 000 triangles par skin (subdivision appliquée), comme le player. Prévois des LOD (réduction automatique Unreal : 50 %, puis 25 %).

## 2. Armes et sockets (à créer sur `SK_Bean_Body_Skeleton`)
| Socket | Os | Usage |
|---|---|---|
| `musket_grip` | hand_r | `SK_Bean_Musket` en main, pour la garde, la patrouille, la baïonnette et la visée |
| `musket_back` | spine_02 | mousquet dans le dos quand le sabre est en main |
| `sabre_grip` | hand_r | `SM_Bean_Cutlass` en main |
| `sabre_sheathed` | pelvis | sabre au fourreau |
| `scabbard` | pelvis | `SM_Bean_Scabbard`, toujours présent |

Les valeurs (cm et roll/pitch/yaw) sont dans `sockets.*.unreal_socket_guess`. Vérifie le roll dans l'aperçu de socket : la convention d'axes d'os FBX peut demander ±90°.

Pour la baïonnette, attache `exports/Common/SM_Bean_Bayonet.fbx` au socket `socket_muzzle` du mousquet, sans décalage (son pivot est déjà à la bouche).

## 3. Animations
- Les clips du player sont réutilisés tels quels (voir `anims[].file`, dans `bean-anim-library`).
- Clips nouveaux dans `exports/anims/` :
  - `A_BeanFR_Idle_Guard` : en boucle ;
  - `A_BeanFR_Patrol` : en boucle, 49 cm/s ;
  - `A_BeanFR_Bayonet_Thrust` : montage FullBody, coup entre 0,375 et 0,625 s ;
  - `A_BeanFR_Sabre_Combo` : montage UpperBody, trois fenêtres de coup.
- Rien n'a de root motion : le CharacterMovement déplace le personnage.
- Mapping de l'AnimBP ennemi `ABP_BeanFR` :
  - Locomotion : blendspace Idle_Guard (0) / Patrol (49) / Run (112) ;
  - montages : Musket_Aim, Musket_Fire, Musket_Reload, Bayonet_Thrust, Sabre_Draw, Sabre_Slash_R/L, Sabre_Combo, Sabre_Sheathe, Hit_React, Death.
- Changement d'arme : sur `AN_Draw_AttachToHand` (Sabre_Draw), passe le mousquet sur `musket_back` et le sabre sur `sabre_grip`. Sur `AN_Sheathe_AttachToScabbard`, fais l'inverse.

## 4. IA suggérée (Behavior Tree)
Les portées, dégâts et temps de recharge sont dans `ai` (en cm).
- **Patrouille** : `Patrol` entre les points du fort, `Idle_Guard` aux postes.
- **Distance (300 à 1 500 cm, idéal 900)** : visée (`Musket_Aim`), tir (`Musket_Fire`, trace au moment de `AN_FireShot`), puis `Musket_Reload` (6,6 s). Pendant le rechargement, l'ennemi est vulnérable.
- **Contact (moins de 140 cm)** : les fusiliers et les grenadiers font `Bayonet_Thrust` (portée de 60 à 115 cm). Le sergent et l'officier tirent le sabre (`Sabre_Draw`), puis enchaînent Slash ou `Sabre_Combo` (portée de 40 à 85 cm).
- **Rôles** :
  - fr4, tirailleur : garde ses distances ;
  - fr5, grenadier : charge à la baïonnette ;
  - fr6, sergent : sabre, commande la patrouille ;
  - fr7, officier : mini-boss, alterne tir et sabre, avec plus de points de vie.
