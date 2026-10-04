# Mobs tiki et villageois : template d'animation Unreal

Le template couvre **95 animations** pour 5 personnages, chacun sur son propre squelette `SKEL_<Nom>`. Les hiérarchies d'os sont identiques, ce qui permet le retarget. Toutes les animations sont à 30 i/s, en place : c'est le CharacterMovement de l'IA qui déplace l'acteur.

| Personnage | Fiche | Taille | Arme (os `weapon_r`) | Clips |
|---|---|---|---|---|
| Guerrier Coco `CocoWarrior` | mob-1 | 1,00 m | lance (`Spear`) | 19 |
| Gardien Crabe `CrabGuardian` | mob-2 | 1,15 m | marteau-coquillage (`ShellHammer`) | 19 |
| Veilleur des Cendres `AshWatcher` | mob-3 | 1,30 m | massue d'obsidienne (`ObsidianClub`) | 19 |
| Rôdeur des Ronces `BrambleStalker` | sniper-1 | 1,25 m | sarbacane (`Blowgun`) + fléchette (`prop_l`) | 20 |
| Villageois Inquiet `Villager` | pnj-1 | 0,95 m | panier de fruits (`FruitBasket`) | 18 |

| Fichier | Contenu |
|---|---|
| `MobAnimManifest.json` | Par personnage : squelette, chemins Unreal, meshes, blendspaces. Par animation : FBX, durée, boucle, slot, notifies (en secondes), blend, vitesse sans glissement, indication gameplay. |
| `import_mob_animations.py` | Script Python de l'éditeur Unreal : import, boucles, notifies, montages `AM_*`, blendspaces. |
| `../exports/<Nom>/` | `SK_<Nom>.fbx` (corps et squelette), `SK_/SM_<Nom>_<Arme>.fbx`, et une FBX `A_<Nom>_<Clip>.fbx` par animation. |
| `../mob-anims.json` | Données brutes de Blender : images clés, notifies en images, instant « pic » de chaque clip. |
| `../renders/contact_<Nom>.png`, `../renders/mob_anim_library.mp4` | Planches de poses et vidéo de la vitrine. |

## 1. Import
1. Importe `SK_<Nom>.fbx` dans `/Game/MonkeIsland/Characters/<Nom>/Meshes`. Unreal crée `SK_<Nom>_Skeleton`. Si ces meshes ont déjà été importés depuis `art/export/Characters`, c'est le même squelette : rien à refaire.
2. Si le chemin du squelette diffère, renseigne `SKELETON_OVERRIDE` en haut du script. Pour n'importer qu'un personnage, utilise `ONLY`.
3. Lance *Tools > Execute Python Script… > import_mob_animations.py*. Résultat pour chaque personnage :
   - `…/<Nom>/Anims` : les séquences `A_*` ;
   - `…/Anims/Montages` : les montages `AM_*` ;
   - `…/Anims/BlendSpaces` : `BS_<Nom>_Locomotion` (1D) et `BS_<Nom>_Combat` (2D) ;
   - `/Game/MonkeIsland/Characters/_Shared/Notifies` : les classes `AN_*`, partagées par tous les mobs.
4. **Arme** : `SK_<Nom>_<Arme>` en *Leader Pose* sur le corps, avec le même squelette (cache-le pour désarmer). Variante : `SM_<Nom>_<Arme>` attaché à l'os `weapon_r` ; l'origine est la poignée.

## 2. AnimBP : un template partagé, un enfant par mob
- **`ABP_Tiki_Template`** (*Animation Blueprint Template*, sans squelette) contient toute la logique ci-dessous. Les animations sont des **variables d'asset** : `Seq_Idle`, `BS_Locomotion`, `BS_Combat`…
- **`ABP_<Nom>`** est un enfant par personnage. Dans *Asset Override*, on y branche les `A_<Nom>_*` / `BS_<Nom>_*`.
- **AnimGraph** : machine d'états `Main`, puis slot `DefaultGroup.FullBody` (attaques, réactions, émotes), puis Output.

| État | Animation | Condition |
|---|---|---|
| Relax | `BS_<Nom>_Locomotion` (Speed cm/s) | `!bInCombat` (patrouille, errance) |
| Combat | `BS_<Nom>_Combat` (Direction, Speed) | `bInCombat` : face au joueur, recul et pas chassés |
| Ambush (Rôdeur) | `A_BrambleStalker_Ambush_Idle` | `bHidden` (embuscade dans les fourrés) |
| Aim (Rôdeur) | `A_BrambleStalker_Aim` | `bAiming` (remplace Combat à l'arrêt) |
| Cower (villageois) | `A_Villager_Cower` | `bScared` (combat proche) |
| Work (villageois) | `A_Villager_Work_Gather` | `bWorking` (point de cueillette) |
| Talk (villageois) | `A_Villager_Talk` | `bTalking` (dialogue ouvert) |
| Dead | `AM_*_Death*`, puis on garde la dernière pose (ragdoll en option) | `bIsDead` |

**Vitesses sans glissement des pieds** (cm/s, à mettre en *MaxWalkSpeed* ou à compenser par le *rate scale*) :

| | Marche | Course | Recul | Pas chassé |
|---|---|---|---|---|
| CocoWarrior | 44 | 100 | 35 | 21 |
| CrabGuardian | 36 | 84 | 29 | 20 |
| AshWatcher | 28 | 64 | 22 | 17 |
| BrambleStalker | 39 | 110 | 31 | 20 |
| Villager | 36 | 93 | – | – |

Ces valeurs viennent d'une estimation géométrique. Si l'IA va plus vite, augmente le rate scale proportionnellement : ×2 pour une vitesse ×2.

## 3. IA → animations (Behavior Tree)
| Tâche IA | Coco | Crabe | Cendres | Ronces | Villageois |
|---|---|---|---|---|---|
| Repérer le joueur | `AM_Alert` | `AM_Alert` | `AM_Alert` | `AM_Alert` | `AM_Startle` puis Cower |
| Provoquer (1 fois sur 3, à 4–8 m) | `AM_Taunt` | `AM_Taunt` (pinces) | `AM_Taunt` (rugissement) | `AM_Taunt` (sifflement) | – |
| Attaque de base | `AM_Attack` (estoc) | `AM_Attack` (coup vertical) | `AM_Attack` (balayage) | `AM_Shoot` + `AM_Reload` | – |
| Attaque 2 | `AM_Attack_Sweep` | `AM_Attack_Sweep` | `AM_Attack_Overhead` | `AM_Melee_Swipe` (< 1,5 m) | – |
| Attaque spéciale | `AM_Attack_Lunge` (2–4 m) | `AM_Attack_GroundPound` (zone) | `AM_Attack_Combo` | – | – |
| Défense | `AM_Block` | `AM_Block` (carapace) | `AM_Block` | `AM_Dodge_Back` | – |
| Coup reçu (de face / de dos) | `AM_Hit` / `AM_Hit_Back` | idem | idem | idem | idem |
| Gros coup (tir de mousquet, coup final du sabre) | `AM_Stagger` | `AM_Stagger` | `AM_Stagger` | `AM_Stagger` | `AM_Hit` |
| Mort | `AM_Death` / `AM_Death_Forward` | idem | `AM_Death` / `AM_Death_Backward` | idem | idem |
| Joueur mort | `A_Victory` (boucle) | idem | idem | idem | `AM_Cheer` |
| Interaction PNJ | – | – | – | – | `AM_Greet_Wave`, `AM_Talk`, `AM_Give`, `AM_Point`, `AM_Nod_Yes`, `AM_Shake_No`, `AM_Look_Around` |

**Mort coupée en deux** (coup final du sabre, `AN_Dismember_Check` côté Bean) : il n'y a pas d'animation dédiée. Le gameplay remplace l'acteur par les deux moitiés du mesh dans la pose courante, puis lance la physique. Les clips `Death*` servent pour toutes les autres morts.

## 4. Notifies (événements gameplay)
| Notify | Clips | Effet attendu |
|---|---|---|
| `AN_Footstep_L` / `AN_Footstep_R` | Walk, Run, Walk_Back, Strafe | son de pas, poussière |
| `AN_Alert_Shout`, `AN_EnterCombat` | Alert | cri ; l'IA passe en combat (`bInCombat`) |
| `AN_Taunt_Shout`, `AN_Taunt_Stomp`, `AN_Claw_Snap`, `AN_Roar`, `AN_Roar_CameraShake`, `AN_Hiss` | Taunt | sons ; légère secousse caméra pour le rugissement |
| `AN_Attack_Windup` | toutes les attaques | télégraphe : son et petit flash. Le joueur peut parer ou esquiver à partir d'ici. |
| `AN_HitWindow_Begin` / `AN_HitWindow_End` | attaques | trace de l'arme (os `weapon_r`, capsule le long de l'arme). Un seul coup par cible par fenêtre. Le combo a deux fenêtres. |
| `AN_Lunge_Dash`, `AN_Land` | Attack_Lunge | `LaunchCharacter` vers l'avant (~1,5 m) |
| `AN_Jump`, `AN_GroundPound_Impact` | Attack_GroundPound | onde de choc de 2 m : dégâts et renversement du joueur, poussière, secousse |
| `AN_GroundImpact` | Attack (crabe), Attack_Overhead | poussière, son |
| `AN_FireDart`, `AN_Recoil` | Shoot | spawn du projectile depuis la bouche de la sarbacane |
| `AN_Reload_Grab`, `AN_Reload_Insert`, `AN_Reload_Complete` | Reload | la fléchette (`prop_l`) apparaît, puis est chargée |
| `AN_Dodge_Impulse`, `AN_Land` | Dodge_Back | `LaunchCharacter` 350 cm/s vers l'arrière ; invulnérable entre les deux |
| `AN_BlockWindow_Begin` / `End`, `AN_BlockImpact` | Block | parade active : −70 % de dégâts de face (−90 % pour la carapace du crabe) |
| `AN_HitFlash`, `AN_Stagger_Stumble`, `AN_StaggerEnd` | Hit, Hit_Back, Stagger | flash ; l'IA reprend la main sur `AN_StaggerEnd` |
| `AN_DeathStart`, `AN_BodyFall`, `AN_DeathEnd` | Death* | désactive la collision et l'IA, son de chute, loot sur `AN_DeathEnd` |
| `AN_Victory_Shout` | Victory | cri |
| `AN_Greet`, `AN_Point`, `AN_Cheer` | villageois | voix |
| `AN_Give_Item` | Give | l'objet ou la récompense passe au joueur |
| `AN_Gather_Pickup` | Work_Gather | son de cueillette |
| `AN_Scream` | Startle | cri, puis état Cower |

Les champs `gameplay` du manifeste donnent des **propositions** de dégâts et de portée pour chaque attaque. Elles sont à équilibrer dans les DataAssets du jeu.

## 5. Régénérer
```
uv run factory/tools/maquette.py build mob-anim-library --headless --force
python factory/tools/make_mob_ue_template.py
blender -b maquettes/mob-anim-library/mob-anim-library.blend --python factory/blender/mob_pose_tiles.py -- <abs>/maquettes/mob-anim-library/mob-anims.json <abs>/maquettes/mob-anim-library/renders/tiles
uv run factory/tools/mob_contact_sheets.py maquettes/mob-anim-library
```
Les clips sont écrits dans `factory/kits/mob_anims.py` (classe `MobAnimator`). Ils réutilisent sans les modifier les créatures, les squelettes et le solveur de poses de `scripts/character/tiki`.
