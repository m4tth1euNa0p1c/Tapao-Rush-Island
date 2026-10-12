# Feature : Skins réalistes des PNJ et mobs, animations GoreBox

- **Statut** : à vérifier.
  - **Compilé** (soir du 2026-10-08), **235 tests verts** (Creatures, Fort, Arena, Weapons, Tiki, Run, Character), dont `monke_island.Fort.SoldierGore` et la mort décapitée des tikis.
  - **Testé éditeur** (Test Flat, PIE, captures 149 à 157) : soldats visibles et animés de près (os racine à 100 à l'exécution), chapeau qui saute (sergent, grenadier), décapitation de l'officier à la lame (tête et bicorne au sol, journal `EFortSoldierDeath::Decapitated`), soldats de l'arène qui se battent avec les tikis et le crabe.
  - **Pas vu en PIE** : l'explosion à bout portant et le recul au mousquet (ils demandent un vrai tir d'arme à feu, pas un coup de script), la mort décapitée d'un tiki (tête arrachée par un tir).
  - **Dans le build v1.5** (zip du Bureau).
  - **Compilé.**
  - **Tests** : 72 tests ciblés verts (Creatures, Fort, Run), dont `monke_island.Creatures.HitZone`.
  - **Testé éditeur** le 2026-10-08 dans Test Flat (PIE, captures `HighresScreenshot00128` à `00131`) : Guerriers Coco, Villageois et soldats français affichés avec leurs nouveaux maillages et textures.
  - **Réactions par zone** : branchées et testées par les specs, mais pas encore vues en jeu (un coup de test n'a touché que la capsule).
  - **Pas encore dans le build empaqueté.**
- **Owner** : lead
- **Date** : 2026-10-08
- **Cahier / lots** : L5 (PvE). Maquettes du 2e PC : `mob-coco-realiste` (2026-10-08 00:26) et `bean-francais-gore` (2026-10-07).

## Demande (mots du propriétaire)
> « Exporte les nouveaux skins sur les PNJ et mobs. »
> « Charge OneDrive et ajoute et remplace les skins et nouvelles animations. »

## Ce qui est en place
**Skins remplacés en place** : mêmes assets, mêmes squelettes, mêmes physics assets. Blueprints, animations, démembrement et cartes gardent leurs références.

| Personnage | Asset | LOD0 → LOD2 (sommets) | Matériaux |
|---|---|---|---|
| Guerrier Coco (mob) | `SK_CocoWarrior` (+ `SK_CocoWarrior_Spear`, 7 modules de démembrement) | 48 314 → 27 794 → 14 885 | `MI_CocoWarrior_Body` / `_Weapon` / `_Gore` |
| Villageois (PNJ, villageois des maisons) | `SK_Villager` (+ `SK_Villager_FruitBasket`, 7 modules) | 47 242 → 28 453 → 14 530 | `MI_Villager_Body` / `_Weapon` / `_Gore` |
| 7 soldiers français | `SK_BeanFR_<Uniforme>` | ≈ 31 800 → 17 900 → 8 600 (avant : environ 90 000 triangles, sans LOD) | `MI_BeanFR_Body` (yeux rouges émissifs ×4), `MI_BeanFR_<Uniforme>_Uniform` |

- **Textures cuites du 2e PC** :
  - BaseColor en sRGB ;
  - Normal en OpenGL, canal vert inversé ;
  - ORM en masques linéaires.
  - Copies dans `art/export/Characters/CocoReal` et `art/export/Characters/BeanFRGore`. Le dossier OneDrive reste en lecture seule.
- **Matériau maître** `/Game/MonkeIsland/Characters/_Shared/Materials/M_Character_Baked` : BaseColor, ORM (AO / rugosité / métal), Normal, Emissive × `EmissiveStrength`.
- **Scripts** (actions de la session de l'éditeur) :
  - `scripts/character/import_coco_real_ue.py` (`cocoreal`) ;
  - `import_bean_fr_gore_ue.py` (`beanfrgore`) ;
  - `import_gore_anims_ue.py` (`goreanims`).
- Les scripts d'import du 2e PC n'ont pas été lancés : ils auraient créé de nouveaux squelettes, ce qui aurait cassé les animations du jeu.

**Nouvelles animations** (59, sur les squelettes du jeu, sans doublon) :
- Guerrier Coco et Villageois, dans `Characters/<Mob>/Anims/Gore` (16 chacun) :
  - coups par zone ;
  - déséquilibre lourd ;
  - recul au mousquet ;
  - morts par tir à la tête, décapitation, coupe en deux, explosion ;
  - panique sans bras ;
  - reptation sans jambes et sa mort ;
  - convulsions au sol.
  - Le mouvement racine est activé sur les clips qui en ont.
- Soldats français, dans `Characters/BeanFR/Animations/Gore` (27) :
  - coups par zone et par côté ;
  - chapeau arraché ;
  - morts ;
  - reddition ;
  - fuite ;
  - reptation.

**Réactions par zone, branchées dans le jeu** :
- **Code** : `HitZones::ZoneOfBone` lit l'os touché par un coup ponctuel (mousquet, sabre, torche) et en déduit la zone : tête, torse, bras gauche ou droit, jambe gauche ou droite.
- **Tikis** (`ATikiCreature`) :
  - un coup survécu joue le clip de sa zone (`FTikiAnimSet.HitHead`…, action répliquée `HitZone`) ;
  - un gros coup déséquilibre toujours ;
  - un coup dans le dos garde sa réaction.
  - Les réactions au mousquet de T-150 restent prioritaires.
- **Soldats français** (`AFortSoldier`) : le sursaut joue le clip de la zone touchée (`DA_FortSoldierRules.HitHead` = `Hit_Head_Front`, `HitTorso` = `Hit_Torso_Front`, bras, jambes ; zone répliquée dans l'action).
- **Clips posés** : `BP_TikiCocoWarrior`, `BP_TikiVillager` et `DA_FortSoldierRules` (`scripts/editor/owner_batch_1008_ue.py`, partie `zoneanims`).

## Soir du 2026-10-08 : « les soldats disparaissent quand je m'approche »
**Cause** (diagnostic en PIE, `scripts/editor/fort_land_pie_check.py` étapes `bonesdump` / `lodcheck`) : les maillages Blender portent l'échelle 100 de l'armature sur l'os `root` (pose de référence), et les clips de la bibliothèque aussi (`A_Bean_*` : root = 100). Les clips des maquettes GoreBox (`A_BeanFR_*`, `A_<Mob>_Hit_*`, `Death_*`…) ont un **root à 1** : dès qu'un tel clip est évalué, la pose est 100 fois trop petite et le personnage est dessiné à 1 cm (`get_delta_transform_from_ref_pose("root")` = 0,01). Les soldats, toujours sur ces clips, étaient invisibles de près et seulement visibles de loin (pose de référence, animation non évaluée) ; les mobs disparaissaient le temps d'un clip gore.

**Correctif** : `Creatures/PoseRoot.h` (`PoseRoot::KeepReferenceScale`) remet l'échelle de la pose de référence sur l'os racine à la fin de l'évaluation de `UBeanLibraryAnimInstance` et de `UTikiAnimInstance`. Les clés d'échelle des bras des clips bean (bean_fit, 0,73 à 1,15) sont conservées : elles sont voulues.

## GoreBox sur les soldats français (owner 2026-10-08 : « les animations type GoreBox ne sont pas là »)
`AFortSoldier` (serveur autoritaire, look répliqué dans `FFortSoldierActionRep` : `Flinch`, `Death`, `bHatOff`) :
- **Morts** (`EFortSoldierDeath`, choix pur `FortSoldierGore::DeathOf`, spec `monke_island.Fort.SoldierGore`) :
  - tir à moins de `ExplodeRangeCm` (120 cm) → `DeathExplode` puis, à `ExplodeRagdollTime`, le corps part en ragdoll (`ExplodeImpulse` 700 cm/s), tête arrachée, giclées ;
  - lame dans la tête, ou tir à la tête une fois sur `DecapitationChance` (40 %) → `DeathDecapitated`, os `head` caché à `DecapitationTime`, tête (`HeadMesh`) et chapeau projetés, jet de sang au cou ;
  - tir à la tête sinon → `DeathHeadshot` ; le reste → `Death`.
- **Réactions** (`EFortSoldierFlinch`) : coup sur l'os `hat` → `HatKnockedOff`, aucun dégât, os caché à `HatOffTime` (0,42 s) et `HatMesh` projeté en physique ; tir survécu → `KnockbackMusket` (tombe et se relève) ; sinon le clip de la zone touchée ou `HitReact`.
- **Sang** : `BloodRules` (= `DA_TikiRules`, catégorie Blood) via `UTikiBloodSubsystem` : giclée à chaque coup (`MulticastBloodHit`), jet pulsé au cou, morceaux et flaque.
- **Pièces** : `SM_BeanFR_<Skin>_Hat` / `_Head` (maquette bean-francais-gore, UCX), importées dans `Characters/BeanFR/Meshes/Gore`, posées sur les 7 `BP_BeanFR_*` (`HatMesh`, `HeadMesh`) par la partie `gore2` de `scripts/editor/owner_batch_1008_ue.py`.
- **Tikis** : la mort décapitée (`Combat.DecapitatedDeath` = `A_<Mob>_Death_Decapitated`) quand le coup mortel arrache la tête (`FBlow::bHeadTorn`, T-167).

## Démembrement (2026-10-09, owner : « il manque les animations de démembrement des soldats français et des nouveaux mobs »)
**Statut : code écrit, non compilé** (T-251, `ue-gameplay-programmer`) ; le lead compile, lance les parties `gore3` / `gore3_tiki` du script et teste en PIE.

### Soldats français (`AFortSoldier`, serveur autoritaire, look répliqué)
- **Santé des membres** (`DA_FortSoldierRules`, catégorie `Health|Limbs`) : `LimbHealthArm` 25, `LimbHealthLeg` 30. Chaque coup ponctuel use le membre de l'os touché (`BoneOfHit` → `HitZones` ArmL/ArmR/LegL/LegR, `FortSoldierGore::LimbOfZone`) : les plombs d'un tir par os × `FFirearmDamageEvent::LimbDamageScale` (comme les tikis, `TikiLimbs::AddDamage` réutilisé), une lame par l'os qu'elle a frappé. Santé dépassée → membre **arraché**, que le soldat survive ou non.
- **Réplication** : masque `uint8 LostLimbs` dans `FFortSoldierActionRep` (bits `FortSoldierGore::ELimbBit` : ArmL 1, ArmR 2, LegL 4, LegR 8), envoyé avec `ForceNetUpdate` + `OnRep_Action` ; un retardataire reçoit les os cachés sans pièce ni sang (`HasActorBegunPlay`), et un changement de skin après l'action les re-cache (`ReapplyHiddenBones`).
- **Look d'un membre arraché** (chaque machine, `TearLimbLook`) : os caché (`HideBoneByName` sur `ArmLBone` = `upperarm_l`, `ArmRBone`, `LegLBone` = `thigh_l`, `LegRBone`), pièce statique physique lancée depuis l'os (`LaunchPiece` avec `ArmLMesh` / `ArmRMesh` / `LegLMesh` / `LegRMesh` posés par Blueprint, offsets `ArmMeshOffset` / `LegMeshOffset`, vitesse `PieceSpeed`), jet pulsé au moignon + morceaux + flaque (`UTikiBloodSubsystem`, `BloodRules`).
- **Posture après la perte** (`FortSoldierGore::StanceOf`, spec) :
  - **bras gauche seul** (`OneArm`) : le mousquet tombe (`MusketDropMesh` = `SM_Bean_Musket` lancé en physique depuis la main, `ShotsLeft` 0), il dégaine et continue au sabre avec le coup à une main `ArmlessFightOn` (`FFortSoldierBlowRules`, clip `A_BeanFR_Armless_Fight_On`, fenêtres 0,167–0,292 / 0,583–0,708 / 1,0–1,125 s, portée/dégâts copiés de `SlashRight` par le script ; `EFortSoldierBlow::OneArm`) ;
  - **bras droit** (seul ou les deux) (`Panic`) : plus rien à tenir (le sabre en main tombe aussi), état `Flee` : il fuit sa cible de `PanicFleeDistanceCm` (900 cm) à chaque tour, tourne en rond sur `PanicRingRadiusCm` (350 cm) sans cible, plus aucun coup ni tir, jamais de rengainage ; locomotion `ArmlessPanicLoop` à l'arrêt, `FleeLoop` en course ;
  - **une jambe** (ou les deux) (`Crawl`, prioritaire sur les bras) : il tombe et rampe (`LeglessCrawlLoop`, `CrawlNoSlideSpeed` 18 cm/s, `WritheGroundLoop` à l'arrêt), vitesse `CrawlSpeed` 40 cm/s (`GroundSpeed`), capsule à `CrawlCapsuleHalfHeight` 30 cm et mesh gardé aux pieds (`ApplyStance`, `CrawlMeshOffset`), se traîne vers sa cible ou ses postes, ni coup ni tir ni sursaut ; il **se vide de son sang** après `CrawlBleedOutSeconds` 6 s ou sous `CrawlDeathHealth` 15 PV (`BleedOut`) ; sa mort joue `LeglessCrawlDeath` quel que soit le coup (sauf l'explosion à bout portant).
- **Coupe en deux** (`EFortSoldierDeath::Bisected`) : coup mortel d'une **lame** (coutelas du joueur quand son mouvement tranche — `ATikiCreature::IsCutBlow` —, ou sabre d'un autre soldat) dans la zone torse (ou sans os connu), une fois sur `BisectChance` 0,5 (`FortSoldierGore::DeathOfBlow`) : `DeathBisected`, puis à `BisectTime` (0,05 s) `CutInTwo` : `BisectBone` (`spine_02`) caché avec ses enfants, `TorsoUpperMesh` lancé avec la tête (+ chapeau) et les bras restants, jet + morceaux + flaque à la taille ; les jambes jouent la fin du clip.
- **Spec pure** `monke_island.Fort.SoldierGore` (`Fort/FortSoldierGoreMath.h`) : zones → membres, santé, bits, posture, matériel lâché, choix de la mort (rampant, coupé en deux, explosion toujours gagnante).

### Tikis (Guerrier Coco, Villageois ; `ATikiCreature`, `TikiCombat`)
- **Clips** (`FTikiAnimSet`, catégorie `Anims|Gore` : `LeglessCrawl`, `WritheGround`, `ArmlessPanic`, `ArmlessFightOn`, `CrawlSpeed` 18 ; `FTikiCombatReactions` : `LeglessCrawlDeath`, `BisectedDeath`).
- **Une jambe arrachée** (T-167 `TearLimb`, à partir d'une jambe — décision : un unijambiste ne marche pas) : locomotion `LeglessCrawl` (rate sur `CrawlSpeed`), `WritheGround` à l'arrêt, vitesse réduite existante (`LegLossSpeedScale` 0,35 / `BothLegsLossSpeedScale` 0,12 : à baisser si la reptation est trop rapide) ; la mort joue `LeglessCrawlDeath` quel que soit le coup, sauf l'éclatement à bout portant (`FBlow::bCrawling`, `TikiCombat::ChooseClip`).
- **Les deux bras arrachés** : `IsArmless` → le serveur remplace `ServerThink` par `PanicThink` : fuit le plus proche ennemi (`ArmlessFleeDistance` 900 cm, `ArmlessPanicSpeed` 380 cm/s), tourne en rond sans ennemi, crie (`Scream`), plus aucun coup ; locomotion `ArmlessPanic`. Un seul bras perdu : le coup joue `ArmlessFightOn` s'il est posé (les mobs n'ont pas ce clip : coups normaux, dégâts `MaimedDamageScale` comme avant).
- **Coupe en deux** : lame mortelle (sabre) dans le torse (ou sans os connu) et `Roll < SabreBisectChance` (0,5, `DA_TikiRules` CombatReactions) → `BisectedDeath` ; sang à la taille au contact (`SpawnWaistBlood` = `SpawnStumpBlood(HalfCutBone)`), puis `BecomeCorpse` spawne les deux moitiés (`Upper` / `Lower` à `HalfCutBone`, mécanisme T-072/T-155) même si le mouvement ne tranchait pas.
- Spec `monke_island.Tiki.CombatReactions` : Describe « Wounds » (mort rampante, coupe en deux).
- Priorités de `ChooseClip` : tête arrachée (debout) > éclatement fort/bout portant > mort rampante > décapité > tir à la tête > tirage éclatement/chute ; sabre : coupe en deux > côté/bas/haut.

### Pipeline (`scripts/editor/owner_batch_1008_ue.py`)
- `gore3` : copie depuis OneDrive (lecture seule, taille stable 1 s) de `exports/<Skin>/Dismember/SM_BeanFR_<Skin>_{Arm_L,Arm_R,Leg_L,Leg_R,Torso_Upper}.fbx` vers `art/export/Characters/BeanFRGore/<Skin>/Gore/`, import (`Interchange` FBX désactivé comme `gore2`) dans `Characters/BeanFR/Meshes/Gore`, matériaux par `imported_material_slot_name` (`MI_BeanFR_Body`, `MI_BeanFR_<Skin>_Uniform`, `MI_Villager_Gore` pour `M_BeanFR_GoreCap`), pose des 5 meshes sur les 7 `BP_BeanFR_*`, et sur `DA_FortSoldierRules` les 6 clips, `ArmlessFightOn` (fenêtres lues dans `BeanFR_GoreManifest.json`), les valeurs des membres et `MusketDropMesh`.
- `gore3_tiki` : `BP_TikiCocoWarrior` / `BP_TikiVillager` : `Anims.LeglessCrawl` / `WritheGround` / `ArmlessPanic`, `Combat.LeglessCrawlDeath` / `BisectedDeath` (= `A_<Mob>_Legless_Crawl_Loop`, `_Writhe_Ground_Loop`, `_Armless_Panic_Loop`, `_Legless_Crawl_Death`, `_Death_Bisected`).

### À vérifier en PIE
- Un tir de pistolet (35) sur un bras de soldat l'arrache : pièce qui vole, moignon qui saigne, mousquet qui tombe ; bras gauche → sabre à une main ; bras droit → fuite en `Flee_Loop`.
- Deux coups de sabre sur une jambe (30) : il tombe, capsule basse, rampe vers le joueur à 40 cm/s, meurt en 6 s (`Legless_Crawl_Death`).
- Coup de coutelas tranchant mortel au torse : une fois sur deux `Death_Bisected`, torse/tête/bras projetés, jambes qui s'écroulent.
- Tiki : une jambe arrachée → reptation + `Writhe` à l'arrêt ; deux bras → panique et cris ; sabre mortel au torse → `Death_Bisected` puis deux moitiés.
- Hôte + client : les pièces et la posture (capsule) identiques sur le client ; un client qui rejoint tard voit les os cachés sans pièce.

## Limites et suite
- **Clips importés mais pas encore joués par le jeu** : reddition (`A_BeanFR_Surrender`), explosion/recul des tikis (`A_<Mob>_Death_Explode`, `_Knockback_Musket`). Les modules de démembrement fermés des beans (`exports/<Skin>/Dismember/SK_*`, 10 par uniforme) ne sont pas importés : les moignons des soldats sont des os cachés + pièces statiques, sans section fermée.
- Démembrement des soldats : la pièce d'un membre part dans l'orientation du corps (pose de repos), pas dans celle du bras animé ; `ArmMeshOffset` / `LegMeshOffset` à régler si le pivot `GoreLimb_<Module>` ne tombe pas à l'articulation.
- **Coco Maudit** : son modèle et ses animations sont prêts sur OneDrive (`CursedCoco`), mais c'est un nouveau mob. Il n'est pas importé. Il demande un Blueprint et une place dans le directeur des tikis.
- **Distances de bascule des LOD** : celles du moteur (1 / 0,5 / 0,25), car `LODInfo` n'est pas accessible en Python 5.8. Celles des maquettes étaient 1 / 0,45 / 0,2 et 1 / 0,42 / 0,18.
- Les anciens matériaux (`M_CocoWarrior_Husk`…, `MI_BeanFR_laine_*`…) restent sur des emplacements que plus aucune section n'utilise.
