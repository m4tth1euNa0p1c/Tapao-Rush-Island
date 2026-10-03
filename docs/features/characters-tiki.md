# Créatures tiki — mobs, PNJ et sniper (planches `sprite/mob-*.png`, `pnj-1.png`, `sniper-1.png`)

| Champ | Valeur |
|---|---|
| Lot | L3 (« un ennemi terrestre simple, attaque de mêlée avec préparation visible, mort ») ; PNJ et sniper préparés pour L8 (factions) : art seulement, aucun gameplay avant le noyau |
| Générateur | `scripts/character/tiki_creatures.py` (package `scripts/character/tiki/`) → `art/blender/characters/tiki_creatures.blend` |
| Ressources CC0 | `python scripts/character/fetch_character_assets.py` → `art/textures/characters/`, `art/models/characters/` ; crédits dans `art/SOURCES.md` (section Personnages) |
| Exports | `art/export/Characters/<Nom>/` : `SK_<Nom>.fbx`, `SK_<Nom>_<Arme>.fbx`, `SM_<Nom>_<Arme>.fbx`, `A_<Nom>_<Anim>.fbx` ; inscrits dans `art/export/ue_manifest.json` |
| Rendus de contrôle | `art/_previews/tiki_lineup.png`, `tiki_poses.png`, `tiki_echelle.png`, `tiki_barque.png`, `tiki_<clé>_turnaround.png` |
| Cible UE | `/Game/MonkeIsland/Characters/<Nom>/` (`Meshes`, `Anims`, `Materials`, `Physics`, `Blueprints`) |
| Statut | Blender : modèles, armes, squelettes, animations, FBX. UE (2026-10-03, T-046) : Villageois, Guerrier Coco, Gardien Crabe et Veilleur des Cendres importés (SK, Skeleton, Physics Asset, arme en SK, `weapon_socket` sur `weapon_r`, 25 animations), `BP_Tiki*` jouables (T-051). Rôdeur des Ronces non importé (L8). Matériaux : couleurs unies, bake à faire |

```
blender --background --factory-startup --python scripts/character/tiki_creatures.py -- [--only coco,crab] [--render] [--quick] [--export] [--no-refs] [--no-save]
```

## Les cinq créatures
Échelle : celle du bean joueur (1,05 m, ADR-002), donc corps plus grand pour les plus menaçants. Toutes les valeurs (fractions de H) sont dans `SPECS` (`tiki/creatures.py`).

| Clé | Identifiant | Nom | Planche | H (sans ornement) | Arme ou objet | Rôle visuel | Triangles corps / arme |
|---|---|---|---|---|---|---|---|
| `villager` | `CHR_Npc_Villager` | Villageois Inquiet | pnj-1 | 0,95 m | panier de fruits (bananes et pommes CC0) | PNJ non hostile : visage inquiet, pagne, pousse sur la tête | 44,8 k / 10,6 k |
| `coco` | `CHR_Mob_CocoWarrior` | Guerrier Coco | mob-1 | 1,00 m | lance-massue à dents de pierre (1,4 m) | mob de base : coco fibreux, masque tiki jaune, chignon et couronne crème | 43,8 k / 10,4 k |
| `crab` | `CHR_Mob_CrabGuardian` | Gardien Crabe | mob-2 | 1,15 m | marteau-coquillage (coquillage *Lambis* CC0) | mob défensif : carapace piquetée, cornes, masque turquoise, pinces | 43,5 k / 12,4 k |
| `ash` | `CHR_Mob_AshWatcher` | Veilleur des Cendres | mob-3 | 1,30 m | massue d'obsidienne | mob lourd : roche à facettes veinée de lave, masque en planches, yeux brûlants | 35,3 k / 5,6 k |
| `bramble` | `CHR_Mob_BrambleStalker` | Rôdeur des Ronces | sniper-1 | 1,25 m | sarbacane (1,1 m) + fléchettes (carquois à la hanche gauche) | tireur embusqué : manteau de ~335 feuilles, frondes de fougère, masque à bouche ronde | 51,6 k / 4,6 k |

Budget §10 respecté (30 à 60 k triangles en LOD0). Matériaux : 8 à 14 emplacements par créature, à réduire à 2 par le bake (voir « Reste à faire »).

## Matériaux (textures libres de droits)
Chaque matériau garde la couleur de la planche. Les jeux de textures CC0 apportent le détail : leur luminance passe dans une rampe entre deux couleurs de la palette, et leur hauteur pilote le relief. Ils sont projetés en boîte sur les coordonnées *Generated* en mètres, donc le détail suit la peau quand le squelette la déforme.
- **Bourre de coco** (Guerrier, Villageois) : ambientCG Bark014.
- **Carapace** (Crabe) : Poly Haven coral_ground_02.
- **Roche et lave** (Veilleur) : ambientCG Lava002, plus des fissures émissives géométriques dont l'intensité varie selon un bruit.
- **Bois** : Poly Haven weathered_planks (masque du Veilleur), fine_grained_wood (masque du Rôdeur), rough_wood et bark_willow (manches).
- **Cordes** : ambientCG Rope001.
- **Pagne** : Poly Haven hessian_230.
- **Panier** : ambientCG Wicker002.
- **Bambou** : Poly Haven bamboo_veneer.
- **Peau d'écorce moussue** (Rôdeur) : Poly Haven tree_bark_03 et ambientCG Moss002.

## Squelette `SKEL_<Nom>` (même hiérarchie pour les cinq)
`root` → `pelvis` → `spine_01` → `spine_02` → `head` → `crest` (ornement du sommet, mouvement secondaire).
- Sous `spine_02` : `upperarm` → `lowerarm` → `hand`, avec `weapon_r` sous `hand_r`.
- Sous `pelvis` : `thigh` → `calf` → `foot`.
- Rôdeur seulement : `prop_l` sous `hand_l`, la fléchette de rechargement, à l'échelle 0 quand elle est cachée.

19 os (20 pour le Rôdeur), mêmes noms que le mannequin UE et `SKEL_Bean`. Le personnage regarde −Y dans Blender, donc +Y dans UE. Origine aux pieds.

Skinning :
- le corps en œuf suit la colonne selon la hauteur (fondu 1D) ;
- les membres utilisent des poids par distance, limités à leur chaîne d'os ;
- l'ornement du sommet et l'arme sont rigides.

L'arme est un mesh squelettique séparé, sur le même squelette (Leader Pose, ou masquée pour désarmer). Sa version `SM_` a l'origine au point de prise et l'axe long sur +Z.

## Animations provisoires (§10, §6.3)
À 30 i/s et sur place, sans root motion (c'est le CharacterMovement de l'IA qui déplace l'acteur). La frame 0 est la pose de garde de chaque créature, ce qui rend les fondus faciles dans l'AnimBP.

| Créature | Cycles (en boucle) | Ponctuelles |
|---|---|---|
| Guerrier Coco | Idle, Walk, Run | Attack (estoc : préparation 12 i, coup 4 i, tenue, retour ; 38 i), Hit, Death (en arrière) |
| Gardien Crabe | Idle (pinces qui claquent), Walk, Run | Attack (coup de marteau par-dessus la tête ; 42 i), Hit, Death |
| Veilleur des Cendres | Idle (souffle lourd), Walk (pas lourds), Run | Attack (balayage latéral ; 46 i), Hit, Death (en avant) |
| Rôdeur des Ronces | Idle (accroupi, scrute), Walk (furtif), Run, Aim | Shoot (souffle 14 i), Reload (fléchette prise au carquois puis glissée dans la sarbacane ; 54 i), Hit, Death |
| Villageois Inquiet | Idle (regarde partout, tremble), Walk, Run (fuite, bras en l'air), Cower (peur), Talk (offre le panier) | Hit, Death |

Les poses sont décrites dans `tiki/anims.py` : accroupi pieds à plat, rotations de la colonne, bras gauche, et objet tenu placé par IK (point de prise et direction). Dans le `.blend`, chaque armature joue une vitrine NLA de toutes ses actions. Les marqueurs de la timeline indiquent les segments : Repos, Marche, Course, Attaque / Tir / Parole, Touché, Mort.

## Scène de présentation (`Tiki_Lineup`)
Personnages sur le sable, avec pour l'échelle :
- le bean joueur ;
- une silhouette humaine de 1,80 m ;
- la barque actuelle (6,4 m) avec deux beans à bord ;
- une **proposition de barque ×0,75** (4,8 m) ;
- deux cocotiers du kit atoll (6 m et 19 m) et des noix de coco.

Caméras : `CAM_Lineup`, `CAM_Echelle`, `CAM_Barque`. Le ciel est un HDRI CC0 (kloofendal pure sky).

## Reste à faire
- [ ] Bake par créature : dépliage UV et atlas 2K D/ORM/N (normale DirectX), plus un emplacement émissif. Les FBX actuels ont des couleurs unies par emplacement, et les fruits n'emportent pas leurs textures.
- [x] Import UE (4 créatures) : SK, Skeleton `SK_<Nom>_Skeleton`, Physics Asset `SK_<Nom>_PhysicsAsset`, arme en SK sur le même squelette, `weapon_socket` sur `weapon_r`, animations sur le squelette (commandlet `scripts/character/import_tiki_anims.py`, éditeur fermé ; le MCP n'importe pas les FBX d'animation seule).
- [ ] Rôdeur des Ronces (L8).
- [ ] Feuilles du Rôdeur : simple face, prévoir un matériau à deux faces dans UE.
- [ ] Planches en pose A pour les variantes (ADR-002, P-04). Les planches actuelles ont les bras le long du corps.

## Gameplay dans UE (T-051, lot L3 ; villages T-064)
Code : `Source/monke_island/Creatures/`. Règles : `DA_TikiRules` (`/Game/MonkeIsland/Characters/Data/`). Acteur de niveau : `TikiDirector` (outliner `Raraka/Gameplay`).
- **Villages** (`FTikiVillage`, T-064, demande du propriétaire : « les PNJ uniquement dans ces villages, les mobs dans les buissons proches ») : le directeur lit au début de la partie les marqueurs posés par `scripts/map/place_villages.py` (`docs/features/villages-japonais.md`).
  - Zone : acteur tagué `PacificationZone` et `Village.<Id>`, rayon donné par la sphère.
  - Points de spawn : TargetPoints tagués `NpcSpawn`, `Village.<Id>`, `Role.<villager|elder|priest|guardian>`.
  - Sans aucun marqueur, un seul village autour du directeur (`VillageRadius`) garde l'ancien comportement, sans gardien.
- `ATikiCreature` : personnage sans NavMesh (déplacement direct, détection d'enlisement), santé `UIslanderHealthComponent`, clips joués en nœud unique depuis la vitesse et des actions multicast (pas d'AnimBP), arme ou panier en Leader Pose.
- `ATikiVillager` (`BP_TikiVillager`) :
  - `VillagerCount` par village : 12 dans `DA_TikiRules` depuis le 2026-10-03 (demande du propriétaire : « plus de PNJ dans les villages »), soit un par maison, pour 30 points de spawn et 33 points d'intérêt par village ; 4 par défaut en C++. Ils apparaissent un par seconde, tous villages confondus, et seulement sur des points de spawn libres de leur village (`SpawnPointGap`) ;
  - flâne dans son village (rayon × `VillagerWanderFraction`, 0,55) ;
  - ramasse les noix de coco, que le directeur fait tomber dans les villages jusqu'à `CoconutCount` au total ;
  - fuit le joueur qui le frappe.
  - Aucun point de flânerie ou de chute de noix sous un bâtiment du village (`BuildingMeshKeywords`) ni sur une pente (`MinGroundNormalZ`).
- **Routine** : tant qu'un joueur est à moins de `RoutineActivationRadius` (150 m) du village, ses villageois vont de point d'intérêt en point d'intérêt.
  - Les points sont des TargetPoints tagués `VillagePOI`, `Village.<Id>` et `Activity.<work|read|pray|rest|watch|chat>`, générés par `place_villages.py --extras`.
  - À chaque point, le villageois se tourne vers le décor et joue une prise de `ActivityClipDuration` :
    - `Work` (puits, porte du kura, cour) et `Chat` (autour de la place) : clip du panier ;
    - `Pray` (sanctuaire) : le panier tendu en offrande ;
    - `Rest` (devant l'engawa), `Read` (panneau) et `Watch` (tour de guet) : immobile.
  - Il fait l'activité `ActivityChance` (75 %) du temps, et flâne ou ramasse des noix de coco le reste du temps.
  - Personne près du village : les villageois restent tranquilles.
  - `Cower` reste la peur.
- **Alarme** (`RaiseAlarm`) : un joueur frappe un villageois.
  - Tout le village fuit pendant `AlarmFleeDuration` (10 s).
  - Les gardiens du village se jettent sur l'agresseur.
  - `AlarmWarriorCount` (4) tiki bondissent des buissons, sur un anneau à `AlarmRingFraction` (0,7) du rayon du village, contre l'agresseur.
  - Au plus `AlarmMaxAlivePerPlayer` (6) tiki d'alarme vivants par joueur, et une alarme par village toutes les `AlarmCooldown` (45 s).
  - Sans marqueurs de village, on retombe sur l'ancien défenseur unique.
- **Villageois tué par un joueur** (`AvengeVillager`, T-072, demande du propriétaire : « dès qu'un joueur tue un villageois, des mobs sortent des buissons près des joueurs ») :
  - le village est alarmé comme pour un coup (le coup mortel peut être le premier, par exemple au mousquet) ;
  - `KillWarriorCount` (3) tiki bondissent des buissons les plus proches de points répartis sur un anneau de `KillRingRadius` (8 m) autour du tueur (`KillBushRadius`, 12 m ; plus loin, ils sortent du sol) ;
  - chaque meurtre en fait sortir, jusqu'à `KillMaxAlivePerPlayer` (9) tiki vivants après ce joueur, alarme comprise ;
  - le tueur est le pion du joueur, ou le porteur de l'arme ou du projectile (`PawnOfCauser`).
  - Vérifié en PIE le 2026-10-03 (`DebugKillVillagers`) : un villageois de V01 tué, 11 villageois fuient, 4 tiki sortent autour du village et 3 autour du tueur.
- **Cadavres** (`ATikiCorpse`, T-072) : `CorpseDelay` (1,5 s) après la mort, le corps devient un cadavre figé dans sa pose de mort, posé sur une boîte physique.
  - Masse `CorpseMassKg` (40 kg) : un joueur le traîne, deux le soulèvent (`LiftMassKg` 25 kg de la saisie). La barque compte sa masse comme celle de tout objet physique dans son volume de chargement.
  - Il reste jusqu'à ce que l'eau le recouvre de `CorpseFloodDepth` (30 cm, vérifié toutes les 2 s) : la montée des eaux l'emporte, comme un cadavre jeté au lagon. `CorpseLifetime` 0 = pas de durée limite.
  - Au plus `CorpseMaxCount` (80) dans le niveau : au-delà, le plus ancien disparaît (garde-fou de performance).
  - Réplication : le serveur simule la boîte et réplique le mouvement ; la pose, le mesh, les matériaux et la pièce sont envoyés une fois, et chaque machine reconstruit l'apparence.
  - Vérifié en PIE le 2026-10-03 :
    - cadavres saisis par le joueur à 40 kg, moitiés à 20 kg ;
    - une moitié jetée à l'eau est emportée (142 cm d'eau) ;
    - un cadavre posé dans la barque fait passer sa charge de 0 à 40 kg (trace `BoatDiag`).
  - Non testé : la disparition par la montée des eaux elle-même, et le jeu à plusieurs machines.
- **Découpe et démembrement** (T-072) : un coup de taille du sabre (`FCutlassAttack::bCut` : taille droite, revers, coup chargé ; l'estoc ne coupe pas, lu par `IsCutBlow`).
  - Sur un cadavre entier : il est coupé en deux moitiés de `CorpseHalfMassKg` (20 kg) chacune, au niveau de `HalfCutBone` (spine_02). La moitié haute est projetée dans le sens du coup (`CutFlingSpeed`).
  - Un tiki tué par un coup de taille tombe directement en deux moitiés.
  - Un coup de taille non mortel a `LimbCutChance` (60 %) de couper un bras (`LimbBones`) : le bras tombe avec ce qu'il tient (arme ou panier, `CorpseLimbMassKg` 4 kg) et disparaît du corps sur toutes les machines. Un guerrier qui a perdu le bras de son arme frappe à `MaimedDamageScale` (50 %).
  - Vérifié en PIE le 2026-10-03, pendant le jeu de l'utilisateur : des bras coupés au sabre sur des tiki vivants (« loses upperarm_l / upperarm_r »), un tiki tranché en deux par le coup mortel, et des cadavres coupés en deux.
  - Technique (`TikiBody.h`) : une pièce garde les os qu'elle contient et écrase les autres à leur articulation (échelle 0,001). Un disque (`CutCapMesh`, `CutCapMaterial`, rayons `BodyCutRadius` / `LimbCutRadius`) ferme la coupe. Les corps en œuf sont skinnés en fondu sur la colonne : la coupe s'amincit avant le disque.
- **Embuscades dans les buissons** (`ETikiWarriorRole::Ambusher`, T-072, demande : « des monstres aléatoires dans les buissons, au moins un par zone de buissons, qui sortent intelligemment quand le joueur approche ») :
  - les buissons sont groupés par zones de `AmbushZoneSize` (104 m). Une zone d'au moins `AmbushMinBushes` (36) buissons cache 1 monstre, plus 1 par `AmbushBushesPerMonster` (240) buissons, au plus `AmbushMaxPerZone` (3). Chaque monstre a son buisson, réparti sur la zone ;
  - rien dans les villages : leurs gardiens suffisent ;
  - le monstre (classe tirée au hasard parmi les guerriers) bondit de son buisson quand un joueur passe à moins de `AmbushTriggerRadius` (25 m), mais pas s'il est déjà à moins de `AmbushMinDistance` (7 m). Au plus `AmbushMaxAlivePerPlayer` (3) autour d'un même joueur ;
  - il chasse les joueurs proches autour de son buisson (`AmbushPostRadius`, 10 m, retour au-delà de 1,5 fois), et y retourne après `AmbushIdleTime` (20 s) sans cible ;
  - mémoire par buisson : il en cache un autre `AmbushRearmDelay` (240 s) après un retour, ou `AmbushKilledDelay` (900 s) après une mort ;
  - aucun monstre n'existe avant l'approche d'un joueur : rien ne coûte loin des joueurs.
  - Rythme, ajouté après un essai de l'utilisateur où 14 monstres étaient sortis en 20 s pendant qu'il filait à environ 16 m/s :
    - au plus un monstre toutes les `AmbushPlayerInterval` (18 s) autour d'un même joueur ;
    - le plafond compte aussi les monstres qui chassent ce joueur, où qu'ils soient ;
    - rien ne sort pour un joueur au-dessus de l'eau (barque, nage) ni pour un joueur plus rapide que `AmbushMaxPlayerSpeed` (9 m/s).
  - Apparition divisée par 3 le 2026-10-03, sur demande de l'utilisateur : zones de 60 m portées à 104 m (3 fois la surface), seuils de buissons triplés (12 → 36, 80 → 240), rythme de 6 s porté à 18 s. Avant ce changement, il y avait 3 816 zones et 6 173 monstres cachés, calculés en 12 ms.
  - Vérifié en PIE le 2026-10-03 : Des monstres sont sortis entre 11 et 25 m du joueur, ont été tués au mousquet et au sabre, et sont devenus des cadavres.
- **Totems sur le sable** (T-072) : le directeur éparpille `ScatterCount` (30) objets `ScatterItemClass` (`BP_Totem`) au début de chaque partie, à au moins `ScatterMinSpacing` (300 m) les uns des autres, avec un tirage différent à chaque partie (`ScatterSeed` 0).
  - Emplacements candidats `ScatterSpots` : `scripts/map/place_totems.py` en tire 1 969 de la carte (`docs/reference/map/raraka/collectibles/`). Ce sont des points de sable sec (canal sable du masque terrain, au moins 0,8 m au-dessus de la mer), à 3 m au moins de tout buisson et 1,5 m de tout tronc, hors villages et loin de Motutapu, espacés de 60 m.
  - En jeu, chaque emplacement est revérifié (sol, eau, bâtiments, aucun buisson à moins de `ScatterBushClearance`, 2,5 m).
  - Modèle : maquette `totem-tiki` du 2e PC, 3 variantes de 55 cm (2 998 triangles, 2 collisions convexes), importées dans `/Game/MonkeIsland/Collectibles/Totem/`.
    - Textures : `art/export/Props/Totem/`, matériaux `MI_Totem_Tiki_A/B/C` sur `M_Prop_Atlas`.
    - Objets : `BP_Totem` (variante A) et ses enfants `BP_Totem_Tiki_B` et `BP_Totem_Tiki_C`. Ce sont des `APhysicsPropActor` de 6 kg, valeur 150.
    - Chaque totem est tiré parmi `ScatterItemVariants`.
  - Vérifié en PIE le 2026-10-03 : « 30 items of 3 kinds scattered on the sand (1969 spots, 0 rejected, 8 ms) ».
- **Corps plus fluides** (T-077, demande du propriétaire : « rendre la physique des PNJ et des mobs plus fluide et réaliste ») :
  - `UTikiAnimInstance` (instance native, sans AnimBP) passe d'un clip à l'autre en fondu : `LocomotionBlendTime` (0,25 s) entre repos, marche et course, `ActionBlendTime` (0,12 s) pour un coup, un choc ou une activité ;
  - la marche et la course jouent à la vitesse du corps : vitesse divisée par `WalkClipSpeed` (110) ou `RunClipSpeed` (380), bornée à [`MinClipRate`, `MaxClipRate`] (0,6 à 1,6). Les pieds glissent moins ;
  - pas et virages amortis : `CreatureAcceleration` (700 cm/s²), `CreatureBraking` (900), `CreatureTurnRate` (320 °/s) ;
  - `Rules` est maintenant répliqué une fois : les clients ont les mêmes réglages que le serveur.
  - La partie joueur est faite par 4f (Player/, Character/Bean/).
- **Bibliothèque d'animations des mobs** (maquette `mob-anim-library` du 2e PC, importée par 19 dans `/Game/MonkeIsland/Characters/<Nom>/Anims`, branchée par T-077) :
  - guerriers :
    - `IdleCombat` : garde quand ils ont une cible, `bInCombat` répliqué ;
    - `Alert` : alerte avec grognement quand ils repèrent un joueur ;
    - `Taunt` : provocation d'un joueur hors d'atteinte dans l'eau, au plus toutes les `TauntInterval` (6 s) ;
    - `AttackVariants` : un coup tiré au hasard avec `Attack` (balayage et fente pour le Coco, balayage et frappe au sol pour le Crabe, frappe d'en haut et combo pour le Veilleur) ;
    - `HitBack` : coup reçu dans le dos ;
    - `Stagger` : titubement sur une coupe ou un coup d'au moins `StaggerDamage` (35) ;
    - `Victory` : quand leur proie tombe ;
  - villageois :
    - `Startle` : sursaut avec cri, `StartleTime` (0,7 s) avant de fuir ou de rentrer chez soi ;
    - `Greet` : salut de la main à un joueur à moins de `GreetRadius` (4,5 m), toutes les `GreetCooldown` (25 s) au plus ;
    - `LookAround` : regard autour de soi dans `LookAroundChance` (30 %) des pauses ;
    - `Gestures` : gestes des discussions sur la place (donner, montrer, oui, non, applaudir) ;
    - `WorkGather` : cueillette au travail ;
  - vitesses sans glissement des clips (manifeste) : marche et course à 44 et 100 cm/s (Coco), 36 et 84 (Crabe), 28 et 64 (Veilleur), 36 et 93 (villageois), dans `WalkSpeed` / `RunSpeed` de chaque BP. Les clips accélèrent jusqu'à `MaxClipRate` (×4) pour suivre le corps : petits pas rapides, sans glissement ;
  - les morts (`Death_Forward/Backward`) ne servent pas : le corps devient mou.
- **Morts réalistes** (T-077) : à la mort, le corps devient un corps mou (ragdoll, physics asset de chaque tiki), jeté dans le sens du coup (`DeathImpulse`, 350 cm/s). Sans physics asset, le clip de mort reste.
  - Après `CorpseDelay` (2,5 s), `ATikiRagdollCorpse` prend la suite os par os : même pose, mêmes vitesses.
  - Ce cadavre est un corps mou de `CorpseMassKg` (40 kg) réparti sur ses os. On le saisit par n'importe quel membre (le profil Ragdoll est rendu visible aux traces de saisie). La barque compte sa masse.
  - Un coup de taille le coupe en deux moitiés rigides (`ATikiCorpse`). Les bras coupés avant la mort restent absents, avec leurs corps physiques.
  - Un cadavre posé dans une barque n'est pas emporté par l'eau : le fond de la coque est sous la mer extérieure.
- **Armes lâchées** (demande de l'utilisateur, 2026-10-04) : à sa mort, un guerrier lâche son arme, qu'on peut ramasser, porter et ranger dans la barque.
  - Réglages de chaque `BP_Tiki*` (`ATikiCreature`, catégorie Tiki|Death) : `DroppedWeaponClass` (objet physique avec une valeur, `BP_Drop_*` de monk-island-19 dans `/Game/MonkeIsland/Items/MobWeapons/`), `DroppedWeaponBone` (`weapon_r`), `DroppedWeaponOffset`, `DroppedWeaponThrowSpeed` (200 cm/s, dans le sens du coup fatal).
  - Affectation : CocoWarrior → `BP_Drop_Spear` (2 kg, 30 $), AshWatcher → `BP_Drop_ObsidianClub` (3 kg, 45 $), CrabGuardian → `BP_Drop_ShellHammer` (4 kg, 50 $). Les villageois gardent leur panier.
  - La prise apparaît sur l'os de prise, orientée comme l'arme tenue. Les `SM_*` sont en espace de prise, axe long +Z. L'axe est déduit de la géométrie de l'arme au repos, de la prise vers son milieu (`TikiMath::WeaponDropTransform`), et ne dépend donc pas des axes de l'os après l'import.
  - Sur toutes les machines, l'arme tenue (`HeldItem`) est cachée à la mort, et le cadavre ne la reprend pas. Une arme partie avec un bras coupé avant la mort reste sur le bras.
  - Test : `monke.TikiKillWarrior` (ou `DebugKillWarriors` sur `TikiDirector`) tue le guerrier le plus proche du joueur.
- **Voix** (`FTikiVoice` dans chaque `BP_Tiki*`, T-077) : 47 sons CC0 de rubberduck (OpenGameArt, `scripts/props/creature_sounds.py`, crédités dans `art/SOURCES.md`), importés dans `/Game/MonkeIsland/Characters/Tiki/Audio`.
  - Ils passent par la classe de son `SC_Effects` (curseur Effets du menu), avec l'atténuation `ATT_Tiki_Voice` : net à 4 m, éteint à 40 m.
  - Guerriers :
    - `Emerge` : rugissement en sortant du buisson ;
    - `Aggro` : grognement en repérant un joueur, au plus une fois par `AggroVoiceCooldown` (8 s) ;
    - `Attack` : effort du coup ;
    - `Hurt`, `Death` ;
    - `Idle` : grognements de temps en temps, à moins de 25 m de la caméra locale.
  - Hauteur de voix : Guerrier Coco 1, Gardien Crabe 0,85, Veilleur des Cendres 1,12.
  - Villageois (voix 1,15) : `Hurt`, `Flee` (cris quand le village fuit ou se cache), `Death`.
- **Villageois tué : tout le village se cache** (T-077, demande : « quand un PNJ est tué, tous les autres fuient dans leur maison ») :
  - chaque maison a un repère `VillageDoor` (36 au total). `place_villages.py` le place devant la porte du doma, avec le tag `Gate.<cm>` vers le portail de la palissade ;
  - chaque villageois reçoit une maison à son apparition ;
  - au meurtre, après l'alarme, tous courent : le portail s'ils sont dehors, puis la porte, et se cachent `HideDuration` (60 s), invisibles et sans collision sur toutes les machines. Un villageois bloqué ou trop lent (`HideReachTime`, 25 s) se cache où il est ;
  - aucun nouveau villageois n'apparaît dans un village dans la minute qui suit une alarme ;
  - ils ressortent ensuite par leur porte.
- Vérifié en PIE le 2026-10-03 (T-077) :
  - meurtre (`DebugKillVillagers`) : « village V01 alarmed », « 11 villagers of V01 run into their houses », puis 11 villageois sur 11 cachés et aucun remplaçant ;
  - le cadavre est un `TikiRagdollCorpse`, compté dans la barque (charge 100 kg : 60 kg de joueur et 40 kg de cadavre) ;
  - les villageois tournent avec `AnimClass = TikiAnimInstance` ;
  - voix posées sur les 4 Blueprints ;
  - tests headless 201/201.
  - Non vérifié : la qualité visuelle des fondus et des corps mous, les sons à l'oreille, la découpe d'un corps mou (le PIE a été arrêté avant), le multijoueur.
- **Gardiens** (`ETikiWarriorRole::Guardian`) : les menaces désignées de chaque village (cahier §6.2).
  - `GuardiansPerVillage` (3) bondissent du buisson le plus proche des points `guardian` (`GuardianBushRadius`), `GuardianSpawnDelay` après le début (la grille des buissons doit être prête).
  - Ils restent près de leur buisson (`GuardianPostRadius`, retour au-delà de 1,5 fois) et chargent les joueurs dans `AggroRadius`.
  - Ils lâchent un joueur qui les entraîne à plus de `LoseRadius` de leur poste.
  - Ils ne s'enterrent pas à l'aube et ne sont pas remplacés une fois tués.
  - Vérifié en PIE le 2026-10-03 : 3 villages lus, 4 villageois par village sur leurs points, 3 gardiens par village.
  - Passage à 12 vérifié le même jour dans un PIE en cours : V01, V02 et V03 comptaient chacun 12 `BP_TikiVillager` dans le monde de jeu. Le coût en temps d'image des 36 villageois n'a pas été mesuré.
- `ATikiWarrior` (`BP_TikiCocoWarrior`, `BP_TikiCrabGuardian`, `BP_TikiAshWatcher`) :
  - la nuit (`ADayNightCycleActor`, phase Night) : jusqu'à `NightWarriorCount` sortent des buissons entre `NightSpawnMinDistance` et `NightSpawnMaxDistance` du joueur ; ils retournent sous terre à l'aube ;
  - joueur qui frappe un villageois : un défenseur bondit du buisson le plus proche du villageois et poursuit ce joueur ;
  - attaque de mêlée : préparation visible (`AttackWindup`), balayage serveur, dégâts, recul ; mort avec clip, puis cadavre (voir **Cadavres**).
- Buissons : instances des meshes dont le nom contient `BushMeshKeywords` (MikiMiki, Naupaka, Pandanus, Tahinu), soit 366 000 sur Raraka, rangées en grille de 20 m une fois au début de la partie.
- **Zones calmes** (demande de monk-island-19 pour le Marae des Perles) : acteur tagué `TikiQuietZone`, rayon donné par sa sphère ou par un tag `Radius.<cm>`.
  - Ses buissons sont écartés de la grille : aucun tiki n'en sort, embuscade ou autre.
  - Aucun objet n'y est éparpillé.
  - `QuietZone_Marae` (15 m) est dans `Raraka/Gameplay`.
  - Les buissons posés à la main (acteurs, pas des instances) ne comptent jamais comme cachettes.
  - Relecture au moment de sortir (demande de monk-island-19 pour les campements de pêcheurs, T-096) : `SpawnWarriorFromBush` refuse tout point situé dans une zone calme. Une cachette d'embuscade située dans une zone calme est désactivée pour la partie.
    - C'est nécessaire parce que les camps apparaissent 1 s après le début et enterrent la végétation dans un rayon de 9 m. La grille de buissons peut avoir été lue avant, chauffée par la barre de vie des mobs.
    - Les totems relisent déjà les zones calmes au moment de tomber.
- Débogage : `monke.TikiDefender` (un défenseur contre le joueur local). Sur le villageois ou le cadavre le plus proche du joueur local : `monke.TikiKillVillager [cut]`, `monke.TikiCutArm` et `monke.TikiCutCorpse`. Même effet par MCP avec les propriétés `DebugKillVillagers`, `DebugCutKillVillagers`, `DebugCutArms` et `DebugCutCorpses` du directeur dans le monde PIE.
- Hors de ce dossier : la barre de vie est cachée près des buissons (4f, `MobHealthBarComponent`, `BushHideRadius`) et les bruits de pas sont gérés par T-071 (aa, `Footsteps/`).
- **Barre de vie masquée** (demande de l'utilisateur, 2026-10-04) : `MaxDistance = 0` sur le composant `HealthBar` de `BP_TikiCocoWarrior`, `BP_TikiCrabGuardian`, `BP_TikiAshWatcher`, `BP_TikiVillager` et `BP_Shark`. La barre ne s'affiche qu'à moins de `MaxDistance` de la vue locale, donc jamais. Pour la rendre, remettre 4000.
