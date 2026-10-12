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
- `ATikiCreature` : personnage sans NavMesh (déplacement direct, détection d'enlisement), santé `UIslanderHealthComponent`, clips joués en nœud unique depuis la vitesse et des actions répliquées (pas d'AnimBP), arme ou panier en Leader Pose. Les actions passent par un état répliqué depuis T-306 : voir « Réseau des créatures » en fin de document.
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
  - **T-155 (lot L5, révision)** : voir « Démembrement réaliste » plus bas : les morceaux ne volent plus et les coupes sont fermées par des modules organiques.
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
  - **Armes jouables (T-161, lot L5)** : l'arme tombe en `BP_Pickup_<Arme>` (`AMobWeaponPickup`, `Weapons/MobWeaponPickup.h` = `AInventoryPickup` + `UValuableComponent`) à la place du `BP_Drop_*` (un `APhysicsPropActor`, sans invite). Invite « Ramasser <nom> » (F), puis arme de mêlée dans un emplacement d'inventaire (`DA_Item_Spear`, `DA_Item_ObsidianClub`, `DA_Item_ShellHammer` ; règles et valeurs dans `cutlass.md`, « Armes de mêlée des mobs »). On peut toujours la soulever (F maintenu) et la ranger dans la barque : la valeur et la masse des `BP_Drop_*` sont reprises (copiées par le script), la valeur baisse aux chocs mais l'arme ne disparaît jamais (`bBreakAtZero` faux). `DroppedWeaponClass` des trois `BP_Tiki*` est réglé par `scripts/editor/mob_weapons_setup_ue.py` (action `mobweapons`) **après le build** de la classe `MobWeaponPickup` ; jusque-là les `BP_Drop_*` restent. Ceux posés dans `L_Test_Flat` restent des objets physiques sans invite.
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

## Réactions de combat (T-150, lot L5) et mort propre (T-151)

Les 8 clips de réaction livrés par le 2e PC (`combat-dismemberment` + `combat-followups`, 30 i/s, contact à l'image 12) sont dans `Anims.Combat` de chaque BP_Tiki* (`FTikiCombatReactions`, `Creatures/TikiCombat.h`), remplis par `scripts/character/set_combat_reactions.py` depuis `/Game/MonkeIsland/Characters/<Mob>/Anims/Combat/A_<Mob>_<Clip>`.

**Choix du clip (serveur, `TikiCombat::ChooseClip`)** — l'arme vient du `DamageCauser` (AMusketWeapon / ACutlassWeapon), le caractère mortel de la mort dans `HandleDeath` :
- mousquet non mortel : `Musket_StaggerRecover`, ou `Musket_Impact` si le mob est déjà en réaction ; les décisions (ServerThink) sont suspendues jusqu'à la fin du clip (ou `ReactionAISuspendTime`) ;
- mousquet mortel : `Musket_Burst` si le tir est fort (dégâts ≥ `MusketBurstDamage`, ou distance ≤ `MusketBurstDistance`) ou au tirage `MusketBurstChance`, sinon `Musket_ArticulatedFall` ; le Burst arrache `MusketBurstLimbCount` membre(s) au contact ;
- sabre mortel : sens de la lame d'après le coup du combo (SlashRight balaie vers la gauche du bean, SlashLeft vers sa droite) projeté sur la droite de la victime → `Sabre_LeftToRight` / `Sabre_RightToLeft` ; Finisher, estoc ou sens douteux → `Sabre_Down` ; visée plus basse que `SabreLowAimPitch` → `Sabre_Low`. Si le coup tranche (bCut), le membre du clip (`Sabre*Bone`) se détache au contact, par le démembrement de T-072 ;
- sabre non mortel, autres armes, clip absent : comportement d'avant (Hit/Stagger/HitBack, ragdoll immédiat). Repli : l'autre clip du mousquet, `Sabre_Down`, puis rien.

**Synchronisation** : le clip démarre à `ReactionStartTime` (0,4 s = image 12) au moment de l'impact ; état répliqué `CombatReaction` (clip + numéro + heure serveur), joué sur chaque machine, pas rejoué pour un joueur qui arrive après. L'arme est lâchée au contact, ou à `ArticulatedFallDropTime` (image 27) pour la chute articulée.

**Fin du clip mortel (T-151, catégorie `Corpse`)** : `CorpseSettleMode` = `FrozenPose` (défaut : la dernière pose du clip est gardée, posée au sol, endormie, réveillée quand on l'attrape) ou `CalmRagdoll` (ragdoll sans poussée). Dans tous les cas : vitesses bornées à `CorpseMaxSpeed` (poussée de mort, vitesses transmises au corps, coup de mousquet sur un cadavre), amortissement `CorpseLinearDamping` / `CorpseAngularDamping`, sortie lente des interpénétrations (`CorpseMaxDepenetrationSpeed`), rotation bornée, articulations verrouillées en translation avec projection (`bCorpseLockJoints`), puis après `CorpseSettleTime` le serveur recale le corps au sol (trace verticale sous le corps le plus bas, `CorpseGroundTolerance`), l'endort et réplique la pose finale à tous les clients. Un cadavre endormi au-dessus du sol est recalé au contrôle périodique.
**Corps figé (T-151, révision du 2026-10-06)** : en `FrozenPose` (défaut), la mort joue le clip de réaction mortel, sinon `Anims.Death`, jusqu'au sol ; la dernière image est tenue `CorpseFreezeDelay`, puis le cadavre (`ATikiCorpse`, rigide, plus de ragdoll) copie cette pose. Sa collision (`UTikiCorpseCollisionComponent`) est faite des corps du Physics Asset placés dans la pose (une boîte autour des os seulement sans Physics Asset) : elle épouse le mesh. Il est posé au sol par son point de collision le plus bas, à `CorpseRestClearance` près (trace qui ignore les plans d'eau), puis la physique fait le reste, sans mise en sommeil forcée. Joueurs et créatures le traversent sauf `bCorpseBlocksPawns` ; la saisie (trace Visibility) le trouve. Eau : la profondeur se mesure depuis le point le plus bas de la collision, jamais sous le sol trouvé sous le corps (`TikiMath::CorpseWaterDepth`) : une mer plus basse que le sol sec n'emporte plus les corps (faux « washed away » de L_Test_Flat). Une instance posée dans un niveau avec `Anims.Combat` vide reprend les clips de sa classe au BeginPlay. Tests : `monke_island.Tiki.CorpseGround.*`.

**Physics Asset (à vérifier dans l'éditeur)** : chaque body (sauf la racine) doit avoir une contrainte vers son parent — le jeu journalise `has no constraint to its parent` sinon ; contraintes en translation `Locked`, limites angulaires réalistes (cou/épaules ≤ 45°, coudes/genoux une seule charnière) ; corps de collision qui ne se chevauchent pas au repos (ou collision désactivée entre voisins) ; pas de body minuscule sur les os de doigts/armes.

Tests : `monke_island.Tiki.CombatReactions.*`, `monke_island.Tiki.CorpseSettle.*`.
Limites : BrambleStalker n'a ni squelette ni BP dans le projet (aucun clip assigné) ; le Villageois tient un panier (`prop_l`), pas d'arme ; le sens gauche/droite reste à confirmer à l'œil (`bMirrorSabreSides` l'inverse).

## Sang (T-153, lot L5)

Purement cosmétique et local : chaque machine crée le sang à partir de ce qu'elle reçoit déjà, un serveur dédié jamais (`UTikiBloodSubsystem`, `Creatures/TikiBlood.h`). Valeurs et assets dans la catégorie `Blood` de `DA_TikiRules` (`bBloodEnabled`, systèmes, materials de decal, `BloodColor` : le sang par défaut, rouge sombre des humains et des beans, tailles, durées, plafonds) ; teinte par espèce dans `DA_BloodSpecies` (T-162, ci-dessous).
- **Coup de mousquet ou de sabre (mortel ou non)** : le serveur (`TakeDamage`, dégâts ponctuels d'AMusketWeapon / ACutlassWeapon) envoie `MulticastBloodHit` (non fiable : un paquet perdu ne perd qu'une gerbe) avec le point d'impact et la direction du tir → `MusketImpactSystem` orienté (+X = direction) et une tache au sol (trace verticale, rien sur l'eau ni sous l'eau).
- **Membre arraché (découpe, Musket_Burst)** : chaque nouvel os de `CutBones` (serveur dans `DetachLimb`, clients dans `OnRep_CutBones`, sauf à la réplication initiale d'un joueur qui arrive après) → `SliceSpraySystem` attaché à l'os parent au moignon pendant `SliceSprayDuration`, `ChunksSystem`, une tache.
- **Morceaux du cadavre** (`ATikiCorpse`, `SpawnServerTime` répliqué une fois) : un morceau frais (< `BloodFreshWindow`) gicle par sa coupe (membre, moitiés ; la moitié basse s'accroche à l'os parent), la moitié haute ajoute morceaux et tache ; tout corps entier ou moitié laisse une flaque (`PoolDecalMaterial`) qui grandit dans le material (`GrowStartTime`, `GrowDuration`, sans tick) après `PoolGrowDelay`, puis s'estompe (`PoolLifetime`, `DecalFadeDuration`). Le mode `CalmRagdoll` (`ATikiRagdollCorpse`) n'a pas de flaque.
- **Garde-fous** : `MaxBloodSpawnsPerFrame`, `MaxActiveDecals` (le plus ancien part), `BloodCullDistance` depuis la caméra locale, assets chargés en asynchrone au BeginPlay (pas de chargement synchrone ; un asset pas encore chargé est sauté).
- **Paramètres lus dans les assets** : Niagara (paramètres utilisateur) `BloodColor` (LinearColor), `Direction` (Vector, monde), `Duration` (float, jet) ; materials de decal `BloodColor`, `GrowStartTime`, `GrowDuration`.
- **Assets** (`scripts/vfx/build_blood_assets_ue.py`, via le MCP) : `/Game/MonkeIsland/VFX/Blood/` — `M_Blood_Decal` + `MI_Blood_Splat_A/B/C`, `MI_Blood_Pool` ; `NS_Blood_*` copies du template stateless `FountainLightweight` (modules réglés par ObjectTools : rafale/jet pulsé, durée de vie, cône de vitesse sur +X local, gravité, traînée) rendus par `M_Blood_Particle` : la couleur vient du material (`BloodColor` 0,35/0/0 × Particle Color en multiplicateur de luminosité : 1,3 gouttes fines, 1,0 jet, 0,55 morceaux), jamais blanc même si le module garde le blanc du template.
- **Limites** : les copies n'ont pas de paramètres utilisateur (le MCP ne sait pas en ajouter) : la couleur des particules vient du matériau, et donc de copies par espèce (T-162, ci-dessous), pas de `User.BloodColor` ; pas de « nuage fin » séparé dans l'impact (un seul émetteur par système) ; pas de flaque en mode `CalmRagdoll`.
- Masques générés par `scripts/vfx/gen_blood_textures.py` → `art/textures/vfx/`.

### Sang par espèce (T-162, lot L5)
**Statut : code écrit et compilé fichier par fichier (UBT `-SingleFile`, 2026-10-10) ; assets à construire par l'action éditeur `bloodtint` ; couleurs à vérifier en PIE. Pas testé en build ni à plusieurs machines.**

| Espèce | Squelette | Sang | Gouttes (`BloodColor` du MI) | Taches et flaques |
|---|---|---|---|---|
| Bean : joueurs, soldats français, marchand | `SK_Bean_Body_Skeleton`, `SK_BeanMerchant_Skeleton` | rouge sombre (systèmes rouges de `DA_TikiRules`) | `M_Blood_Particle` 0,35/0/0 | 0,30/0,005/0,004 |
| Villageois (PNJ) | `SK_Villager_Skeleton` | rouge sombre, un peu plus vif : ce sont des gens | `M_Blood_Particle` | 0,32/0,008/0,006 |
| Guerrier Coco | `SK_CocoWarrior_Skeleton` | sève ambrée de la bourre de coco (« sève ambrée » de l'audit de classification, pas de rouge organique) | `MI_Blood_Particle_Sap` 0,42/0,17/0,02 | 0,30/0,11/0,012 |
| Gardien Crabe | `SK_CrabGuardian_Skeleton` | hémolymphe bleue (hémocyanine, le sang des vrais crabes) | `MI_Blood_Particle_Hemolymph` 0,03/0,15/0,45 | 0,015/0,09/0,30 |
| Veilleur des Cendres | `SK_AshWatcher_Skeleton` | lave en fusion de ses veines, qui luit (`NightReadability` 3) | `MI_Blood_Particle_Lava` 1,0/0,20/0,02 | 0,60/0,13/0,012 |

Le crabe géant n'est pas dans la table : il n'appartient pas à cette tâche.

- **Pourquoi des copies** : les `NS_Blood_*` n'ont pas de paramètre utilisateur, et ce n'est pas faisable par script dans cet éditeur. Le MCP ne sait pas en ajouter. Le greffon expérimental `NiagaraToolsets` d'UE 5.8 (outils `AddUserVariables`, `SetStackInputData`) le pourrait, mais il n'est pas activé.
  - Chaque sang a donc ses copies des trois systèmes, `NS_Blood_<MusketImpact|SliceSpray|Chunks>_<Sap|Hemolymph|Lava>`, dessinées par une instance de `M_Blood_Particle` à sa couleur.
  - Les decals prennent la couleur de l'espèce par leur paramètre `BloodColor`.
  - Le jeu écrit toujours `User.BloodColor` : le jour où les systèmes l'auront, il servira tel quel.
- **Choix de l'espèce** (`UTikiBloodSubsystem`, local) : le squelette du maillage qui saigne.
  - Ordre de recherche : le composant qui saigne, sinon le maillage du personnage, sinon le premier maillage skinné de l'acteur dont le squelette est listé. Pour un cadavre, c'est son corps, pas son arme.
  - Une table pour toutes les sources : créature, cadavre, morceau coupé, soldat. Rien n'est ajouté dans leur code.
  - Une gerbe sans corps (`SpawnChunks` sans source) prend l'espèce de la blessure saignée au même endroit dans la même image (`SameWoundRadius`, 300 cm). Sinon, elle prend le sang des règles, c'est-à-dire le rouge.
  - Un système d'espèce encore en chargement est sauté : jamais la mauvaise couleur.
- **Données** : `DA_BloodSpecies` (`UTikiBloodSpeciesRules`, `Creatures/TikiBloodSpecies.h`), chargé en asynchrone par chemin (`DefaultGame.ini` `[/Script/monke_island.TikiBloodSubsystem] SpeciesRulesPath`, sinon `/Game/MonkeIsland/VFX/Blood/DA_BloodSpecies`). `/Game/MonkeIsland/VFX/Blood` est cuit en entier.
- **Script** : `scripts/editor/blood_tint_ue.py` (action `bloodtint`, rapport `Saved/T162/blood-tint-result.json`). Avant le build du lead, il fait les MI et les copies. Après, il crée aussi `DA_BloodSpecies`.
- **Fait le 2026-10-10 (02:33)** : action `bloodtint` passée dans l'éditeur, encore sans le build. Elle a créé et enregistré :
  - 3 MI, relus : Sap 0,42/0,17/0,02, Hemolymph 0,03/0,15/0,45, Lava 1,0/0,20/0,02 lueur 3 ;
  - 9 copies de systèmes, dont le moteur de rendu dessine avec leur MI (relu).
  - Vignettes (`scripts/editor/weather_blood_captures.py`) : `docs/production/captures/T-162/` (ambre, bleu, orange lumineux, rouge par défaut). L'outil de capture ne sait pas faire les systèmes Niagara.
  - `DA_BloodSpecies` attend le build.
- **Spec** : `monke_island.Tiki.BloodSpecies` (espèce par squelette, absence d'espèce, couleurs, blessure de la même image).
- **À vérifier en PIE** (L_Test_Flat, lead) :
  - mousquet ou sabre sur un guerrier Coco (ambre), un gardien Crabe (bleu), un veilleur des Cendres (orange lumineux), un villageois (rouge) et un soldat français (rouge) ;
  - les gouttes, le jet au moignon, les morceaux, les taches et la flaque du cadavre ;
  - le journal doit afficher « Tiki blood: 5 species (DA_BloodSpecies), 9 systems of their own ».

## Démembrement réaliste (T-155, lot L5)

Demande du propriétaire : « je ne veux pas que les corps des mobs et PNJ volent quand je les coupe ; du réalisme dans les parties déchirées, avec du sang » (L_Test_Flat, sabre et Musket_Burst).

**Pas d'envol, garanti par le code** (catégorie `Dismember` de `DA_TikiRules`, défauts C++) :
- un morceau coupé (membre, moitié) part **à plat** dans le sens du coup, à `CutPieceMaxSpeed` (40 cm/s) au plus, sans vitesse héritée du ragdoll ni du tir, puis tombe par son poids au pied du corps (`TikiMath::CutPieceVelocity`, appliqué dans `ATikiCorpse::SpawnPart` quel que soit l'appelant). `CutFlingSpeed` garde son sens (vitesse le long du coup) mais n'a plus de composante verticale et est borné par `CutPieceMaxSpeed` ;
- un morceau n'est jamais « figé » : il tombe toujours ; seul un corps entier sans vitesse reste figé (T-151) ;
- un coup sur un cadavre ou un morceau au sol (sabre 25 000, mousquet 40 000 kg·cm/s, coup de poing) le pousse à `CorpseBlowMaxSpeed` (40 cm/s) au plus, sans rotation (`UTikiCorpseCollisionComponent::AddImpulse(AtLocation)`, `TikiMath::BoundedBlowVelocity`) ; un lancer de joueur (changement de vitesse) n'est pas borné ;
- un cadavre frais traverse les autres corps physiques (`ECC_PhysicsBody` ignoré : les autres morceaux, le corps d'où il vient, un ragdoll) jusqu'à ne plus en chevaucher aucun, contrôlé toutes les `CorpseCheckInterval` (0,2 s) : les morceaux d'une coupe ne se repoussent jamais. Il collisionne ensuite normalement (barque, autres cadavres) ;
- un morceau immobile (`CutPieceRestSpeed` 8 cm/s, après `CutPieceMinFallTime` 0,6 s) est endormi au sol comme un cadavre figé ; les contrôles s'arrêtent après `CutPieceSettleTimeout` (4 s).

**Coupes fermées (modules organiques du pack `combat-dismemberment` du 2e PC)** : 7 modules `SK_<Mob>_<Head|Torso|Pelvis|Arm_L|Arm_R|Leg_L|Leg_R>` par mob, sur le squelette du mob, faces de coupe modélisées (muscle, fascia, os, moelle).
- Solution retenue : le corps vivant reste le SK entier tant qu'il est intact. À la première coupe, il est remplacé visuellement par les 7 modules en **leader pose** (le SK entier reste l'animation et la physique, invisible, `AlwaysTickPoseAndRefreshBones`) et le module de l'os coupé est masqué : le moignon montre la face de coupe du module voisin. Le morceau (`ATikiCorpse`) dessine ses modules (`UPoseableMeshComponent`) dans la pose figée complète, sans écraser d'os : le membre a sa propre face de coupe. Plus de disque ni de `HideBoneByName` dans ce cas.
- Pourquoi : aucune géométrie à couper au moment du jeu, aucune échelle d'os à zéro (les modules sont fermés par construction), même mécanisme pour le corps vivant, les moitiés et les membres, et réplication inchangée (les os coupés et la pose répliqués suffisent ; le set est répliqué une fois sur le cadavre).
- Données : `UTikiDismemberSet` (`Creatures/TikiDismember.h`, `DA_<Mob>_DismemberSet` dans `Characters/<Mob>/Dismember/`), champ `DismemberSet` de `ATikiCreature` (catégorie `Tiki|Dismember` des BP). Matériaux : les emplacements extérieurs des modules reprennent les matériaux du corps (par nom d'emplacement ou par asset, donc les variantes) ; les faces de coupe ont `M_<Mob>_Cut_<Muscle|Fascia|Bone|Marrow>`.
- **Repli (comportement T-072)** : sans set, set incomplet (module manquant), coupe à un os qui ne commence pas un module (avant-bras…), module sur un autre squelette : os masqués + disque `CutCapMesh` comme avant (`TikiDismember::SelectModules`).
- Les coupes de moitié se font à `HalfCutBone` (spine_02) = premier os du module Torso : haut = Torso, Head, bras ; bas = Pelvis, jambes. `LimbBones` (upperarm_l/r) = premiers os des modules Arm_L/Arm_R.

**Sang** (crochets T-153 réutilisés) : jet pulsé `SliceSpraySystem` (`SliceSprayDuration` 1,3 s) au moignon (`SpawnStumpBlood`) et sur le morceau frais ; gouttes (`ChunksSystem`) au moignon ; une tache au sol au pied du moignon et, nouveau, sous le membre tombé (une flaque sous chaque moitié, inchangé).

**Import et remplissage** (session principale, éditeur fermé, verrou pris, après le build éditeur) :
```
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/matth/monk-island/monke_island/monke_island.uproject" -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/import_dismember_parts.py" -unattended -nosplash -nullrhi -nosourcecontrol
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/matth/monk-island/monke_island/monke_island.uproject" -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/set_dismember_sets.py" -unattended -nosplash -nullrhi -nosourcecontrol
```
Essai à sec : `python scripts/character/import_dismember_parts.py --check` (28 modules à importer pour CocoWarrior, CrabGuardian, AshWatcher, Villager ; BrambleStalker sauté, pas de squelette ; FBX en cm, UnitScaleFactor 1). Rapport : `Saved/MonkeTools/dismember_parts_report.json` (comparer `extent_cm` des modules à `body_extent_cm`).

Tests : `monke_island.Tiki.Dismember.*` (borne de vitesse, coup borné, mise en sommeil, choix des modules selon l'os coupé, repli).
Limites : le mode `CalmRagdoll` (`ATikiRagdollCorpse`) garde les os masqués (pas de modules) ; les modules ne sont pas encore importés ni vus en jeu (à vérifier en PIE : jointures des modules au repos, matériaux, coupe au moignon) ; un morceau saisi pendant ses 4 premières secondes peut retrouver sa collision avec les corps physiques pendant qu'il est porté.

## Santé des membres et démembrement par os (T-167, lot L5)

Les tirs (pistolet, chaque plomb du fusil, mousquet) remplissent une **santé de membre**, à part de la santé du corps (catégorie `LimbHealth` de `DA_TikiRules`, struct `FTikiLimb`, règles pures `Creatures/TikiLimbs.h`).

| Membre (`Limbs`) | Premier os | Santé | Perte |
|---|---|---|---|
| Head | head | 25 | **tue** : le coup est porté au moins à la santé restante (réaction mortelle normale) |
| Arm_L / Arm_R | upperarm_l / upperarm_r | 25 | le bras qui tient l'arme la **lâche** en pickup (`DroppedWeaponClass`, `bArmLossDropsWeapon`) ; coups affaiblis (`MaimedDamageScale`, `IsMaimed`) |
| Leg_L / Leg_R | thigh_l / thigh_r | 30 | le mob **boite** : vitesse ×0,35 (`LegLossSpeedScale`), ×0,12 sans jambes (`BothLegsLossSpeedScale`), appliqué dans `MoveToward` |

- Un os appartient au membre dont le premier os est lui-même ou son plus proche ancêtre (`TikiLimbs::LimbOfBone`) ; torse et bassin = corps, sans santé de membre.
- Dégâts de membre = dégâts du groupe d'os × `LimbDamageScale` de l'arme (fusil 2,5, pistolet 1). Quand le cumul franchit la santé, le membre est arraché par le système T-155 (`DetachLimb` : modules, sang au moignon, morceau qui tombe), **que le mob survive ou non**, avant que la santé du corps ne prenne le coup.
- Fusil à bout portant (`FFirearmDamageEvent::ShredLimbs`, 2 à 3) : si le tir tue, des membres encore attachés sont arrachés jusqu'au compte (ceux des plombs comptent) ; l'éclatement `MusketBurst` n'en ajoute pas (`bReactionShredded`).
- Mousquet et autres `FPointDamageEvent` : l'os est cherché par le mob lui-même depuis la trace (`ResolveHitBone`, `LimbHitRadius` 18 cm).
- Sang : `MulticastBloodHits` envoie le point d'entrée de chaque plomb (une gerbe par plomb, une tache au sol par tir) ; `MaxBloodSpawnsPerFrame` passe à 16.
- Serveur autoritaire : santé des membres et jambes perdues côté serveur seulement ; les clients voient les os coupés (`CutBones` répliqués) et le mouvement répliqué.
- Tests : `monke_island.Tiki.Limbs.*`, `monke_island.Firearm.Ballistics.*`.
- Limites : la pose des os sur le serveur n'est fiable que si le maillage s'anime sur l'hôte (listen server : oui à l'écran ; un mob hors vue de l'hôte peut garder une pose ancienne, l'os choisi reste plausible) ; pas encore de clip de ramper (le mob se traîne au ralenti) ; le mob sans arme garde ses coups affaiblis plutôt qu'un comportement de fuite.

## Démos de déchiquetage cuites (T-168, lot L5)

Retour du propriétaire (2026-10-06) : « les animations de déchiquetage existant dans les maquettes ne sont pas appliquées ». À la mort par `Musket_Burst` ou par un coup de sabre mortel, le mob est remplacé par la démonstration cuite du pack `combat-dismemberment` v0.2 du 2e PC : fragments, éclatement et découpe rejoués exactement comme dans Blender, avec du sang.

**Quand** (serveur, `TikiShredDemo::Choose`, valeurs de `DA_TikiRules`, catégorie `ShredDemo`) :
- réaction mortelle `Musket_Burst` (tir fort, proche ou tiré au sort, T-150), ou tir de fusil à `CloseBurstDistance` (5 m) ou moins quelle que soit la réaction (`bCloseShotBurstDemo`) ;
- coup de sabre mortel : `Sabre_LeftToRight`, `Sabre_RightToLeft`, `Sabre_Down`, `Sabre_Low` (si `bShredDemoSabreNeedsCut`, seulement un coup qui tranche) ;
- jamais : corps déjà amputé (`CutBones` non vide), règles coupées (`bShredDemos`), mob sans set ou sans la démo du clip, arme sans tranchant (lance, marteau-coquillage ramassés, T-161 : mort simple). Une démo n'est jouée que si le mob a aussi le clip de réaction choisi (`Anims.Combat`) : les quatre `BP_Tiki*` ont les 8 clips de T-150 (CrabGuardian et AshWatcher n'ont pas les morts gore ; rapport `Saved/T168/shred-demo.json`, `blueprints`).

**Déroulé** (`ATikiShredDemo`, `Creatures/TikiShredDemo.h`) : purement cosmétique et local, sans collision ni tick (minuteries). Le serveur réplique la réaction (`FTikiCombatReactionState::bShredDemo` + `DemoYaw`) ; chaque machine (jamais un serveur dédié) cache le mob et fait apparaître sa démo au sol sous ses pieds, tournée vers l'origine du coup, à l'échelle du mesh du mob, l'animation partant du contact (image 12, 0,4 s) plus l'âge de la réaction (un client en retard démarre plus loin, sans le sang du contact au-delà de `BloodFreshWindow`). Le serveur détruit le mob après `CorpseDelay`, sans cadavre. La dernière image reste `ShredDemoHoldTime` (8 s), puis les fragments s'enfoncent de `ShredDemoSinkDepth` en `ShredDemoFadeTime` (2 s) — ou restent en morceaux (`ShredDemoEnd = Keep`, `ShredDemoKeepMax` 8 au plus, le plus ancien s'efface).
- **Sang** (crochets T-153) : au contact, un jet (`SliceSpraySystem`) sort de chaque fragment détaché, des morceaux et une tache au centre ; à la fin du clip, une tache sous chaque fragment détaché. L'éclatement du tir (`ShredDemoBurstSystem`) est vide dans `DA_TikiRules` : seul le sang des fragments marque le Burst.
- **Arme** : le fragment `frag_Weapon` est caché quand le mob lâche son arme en pickup à cette mort (`UTikiShredDemoSet::WeaponFragmentBone`, `TikiShredDemo::HiddenFragment`) : l'arme n'apparaît pas deux fois. Le panier du villageois vole avec le corps.

**Données** : `DA_<Mob>_ShredDemoSet` (`UTikiShredDemoSet`) dans `/Game/MonkeIsland/Characters/<Mob>/ShredDemos/`, branché sur `ShredDemoSet` (catégorie `Tiki|Dismember`) des `BP_Tiki*`. Cinq démos par mob (`Musket_Burst` et les quatre coups) : `SK_Demo_<Mob>_<Clip>` (8 fragments, squelette propre `root` + `frag_<Partie>`), `A_Demo_<Mob>_<Clip>` (3 s, 30 i/s), contact 0,4 s, `SeveredBones` = `frag_<Partie>` des `detached_parts` du manifeste (Burst : les 7 ; LeftToRight : Arm_L ; RightToLeft : Arm_R ; Down : Head ; Low : Leg_L). Les démos `Musket_Impact` (non mortelles) sont importées mais hors du set (pas préchargées). Matériaux : ceux du projet (`M_<Mob>_<Nom>` des `Meshes/`, faces de coupe `M_<Mob>_Cut_<Organe>` de T-155).

**Import** : `scripts/editor/shred_demo_setup_ue.py`, action `shreddemo` de la session du lead (ou commandlet `UnrealEditor-Cmd -run=pythonscript`, éditeur fermé : commande dans l'en-tête du script). Essai à sec hors Unreal : `python scripts/editor/shred_demo_setup_ue.py --check` (24 FBX synchronisés, UnitScaleFactor 1, os `frag_*`, matériaux du projet trouvés : 0 problème le 2026-10-10). Requête `Saved/T168/shred-demo-request.json` (`mobs`, `clips`, `steps`, `force`, `tangents`). Le script s'arrête au premier import raté (rien d'autre importé, ni set ni Blueprint écrit). Une démo de l'AshWatcher prend ~4 min (≈ 2 min à l'import, 2 min au build du mesh, même avec le générateur de tangentes `"tangents": "builtin"` : sa roche, 117 k triangles aux normales nulles, est en cause) et gèle l'éditeur partagé : les importer par commandlet, éditeur fermé (`MONKE_SHRED_REQUEST=monke_island/Saved/T168/shred-demo-commandlet.json`, commande dans l'en-tête du script), ou un clip par appel avec une attente assez longue (`run_action_locked.sh shreddemo 1800`).
- **Incident du 2026-10-10 02:01** : l'attente de 600 s de `run_action.sh` a expiré pendant l'import AshWatcher ; la requête suivante (Villager) a été exécutée en réentrance pendant le SlowTask de cet import (même image 891), l'import imbriqué de `SK_Demo_Villager_Musket_Impact` a échoué (importeur FBX occupé) et l'importeur partagé, réinitialisé, a fait planter l'import extérieur (violation d'accès dans `libfbxsdk.dll`). Le fichier n'était pas en cause (8 250 748 octets, synchronisé, lu entièrement par `--check`). Le lead a ajouté depuis une garde (une action par image, pause, refus après un import raté) dans `lead_session_t186.py`.

**État (2026-10-10)** : C++ compilé fichier par fichier (UBT `-SingleFile`), pas encore de build complet ; 22 démos sur 24 importées et vérifiées par le rapport (3,0 s, 9 os `root` + `frag_*`, emprise comparable au corps, tous les emplacements sur des matériaux du projet) : CocoWarrior, CrabGuardian et Villager 6/6, AshWatcher 4/6 (`Sabre_Down`, `Sabre_Low` restent à importer par commandlet) ; les quatre `DA_<Mob>_ShredDemoSet` et le `ShredDemoSet` des quatre `BP_Tiki*` sont écrits (AshWatcher : Burst et les deux coups horizontaux pour l'instant). `SK_Demo_Villager_Musket_Impact`, raté pendant l'incident, s'importe seul en 4,7 s. **Non testé en PIE** : orientation face au tireur, posé au sol, sang, disparition, client en retard.
- Limites : la géométrie des démos est celle du pack stylisé du 2026-10-04 (le Coco et le villageois réalistes du 2026-10-08 n'ont pas de démo : leurs fragments ont l'ancien aspect) ; pas d'effet d'éclatement dédié ; un mob mort hors de la vue de l'hôte joue sa démo quand même (coût de quelques secondes d'un mesh de 80 à 117 k triangles) ; mémoire : chaque set précharge ses 5 démos dès qu'un mob de l'espèce entre en jeu (5 meshes de 80 à 117 k triangles par espèce présente, ~15 à 21 Mo par `.uasset` dans l'éditeur), à mesurer pour la porte PROD (`/ue-memory-audit`).
- Tests : `monke_island.Tiki.ShredDemo.*` (choix, set, chronologie, orientation), `monke_island.Weapons.MobMelee` (fragment d'arme caché, pas de démo pour une arme sans tranchant).

## Refonte Outlast (T-274, lot L5)

Demande du propriétaire (2026-10-10) : « rends ces mobs : crab guardian, ashwatcher, cocowarrior, améliorer leurs design et texture, fais un design comme Outlast, prends les meilleurs modèles et ajuste-les sur notre thème ». Ici, le visuel seulement ; les mécaniques sont refaites en parallèle par T-275.

**Statut (2026-10-10, 22 h 30)** : modèles, textures et matériaux réimportés en place dans l'éditeur et vérifiés par relecture (rapports `monke_island/Saved/T274/`) et par les vignettes rendues par l'éditeur (`docs/production/captures/T-274/`). **Pas encore vus en PIE**, ni de jour ni de nuit.

| | |
|---|---|
| Générateur | `scripts/character/tiki_horror.py` (package `scripts/character/horror/`) → `art/blender/characters/tiki_horror_<crab\|ash\|coco>.blend` |
| Exports | `art/export/Characters/<Mob>/Horror/` : `SK_<Mob>.fbx`, `LOD/SK_<Mob>_LOD1-2.fbx`, `SK_` et `SM_<Mob>_<Arme>.fbx`, `Dismember/SK_<Mob>_<Module>.fbx` (7), `Textures/`, `horror_manifest.json` |
| Rendus (matériaux cuits) | `art/_previews/horror_<crab\|ash\|coco>_{face,trois_quarts,nuit}.png`, `horror_lineup.png` et `horror_lineup_nuit.png` (avec le bean et l'humain de 1,80 m) |
| Import Unreal | `scripts/character/tiki_horror_import_ue.py`, action `mobsvisual` de la session du lead ; requête `Saved/T274/request.json` (`{"mobs": [...]}`, modes `verify` et `drop_collision`) |
| Vignettes | `python scripts/editor/tiki_horror_captures.py [Mob ...]` (MCP, lecture seule, prend le verrou d'action) |

```
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/character/tiki_horror.py -- --only crab [--quick --look] [--no-bake] [--no-render] [--no-save]
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/character/tiki_horror.py -- --lineup
```
Un mob par processus Blender (environ 40 s : construction, découpe, cuisson OptiX, FBX, rendus). `--quick --look` donne un essai de forme rapide avec les matériaux procéduraux (`build/tiki_horror/look_*.png`).

### Meilleurs modèles de départ
- **Guerrier Coco** : le modèle réaliste du 2e PC (maquette `mob-coco-realiste`, intégré le 2026-10-08).
- **Gardien Crabe** et **Veilleur des Cendres** : ils étaient encore les tiki stylisés du 2026-10-03. Ils sont refaits sur la même base réaliste.
- Les kits du 2e PC (`coco_real_*`, NB et matériaux de `cursedcoco_kit`) sont rapatriés dans `horror/real_*.py`. Ils sont copiés tels quels, à trois changements près :
  - le poids de budget `budget_weight` ;
  - la liste de modules `only` ;
  - des profils de couches de coupe extensibles.
- Les couches « horreur » sont dans `horror_mats.py` et `horror_geo.py`.
- Les SPECS tiki ne changent pas : H, articulations, œuf et masque. Les têtes d'os des nouveaux FBX ont été comparées à celles des FBX déjà importés : elles sont identiques au dixième de millimètre pour les trois mobs.

### Direction artistique
Réalisme sale et inquiétant, sans gore gratuit sur les vivants. Motifs inventés uniquement (traits, chevrons, points), aucun motif sacré réel.

**Gardien Crabe**
- Carapace bordeaux sombre bleuie, granuleuse, avec des cicatrices de balanes arrachées, des arêtes usées, des algues noires en coulures, de la vase brun-noir sur le bas et du sel blanchi sur le dessus sec.
- Balanes en colonies, corail incrusté et lanières d'algues qui pendent des cornes, du dos et des ligatures.
- Masque en bois flotté gris au bord rongé, avec des incisions sombres où reste du sel et un pigment bleu-vert délavé dans les creux.
- Dents en éclats de coquillage, dont deux manquent. Yeux laiteux (cataracte) qui luisent faiblement.
- Pattes de chitine maigres et segmentées, à épines. Pinces dentelées, dont la droite serre le manche.
- Marteau : bois flotté tordu, coquillage encroûté de balanes et de corail, algue.
- Coupe (démembrement) : chitine, chair de crabe rosée et hémolymphe bleue, cohérente avec le sang de T-162.

**Veilleur des Cendres**
- Croûte de charbon en plaques, poudre de cendre sur les dessus.
- Fines failles de braise : elles suivent les grandes failles (attribut `Fissure`), et la moitié seulement couvent.
- Éclats d'obsidienne sur le crâne et les épaules.
- Masque de quatre planches calcinées, en une seule plaque :
  - joints creusés et clous d'obsidienne ;
  - bord rongé par le feu ;
  - yeux de braise au fond des orbites, bouche à dents d'obsidienne ;
  - chevrons de cendre délavés.
- Membres maigres de chair brûlée, avec des braises sous la peau, des griffes vitreuses et des bandes de tissu roussi.
- Massue de basalte à failles de braise, hérissée d'obsidienne.
- Coupe : croûte, cendre rouge et cœur de braise lumineux, cohérent avec le sang de lave de T-162.

**Guerrier Coco**
- Bourre pourrissante, moisie, avec des plaies tracées dans le shader : trois griffures sur le flanc avant gauche, une entaille dans le dos et trois coupures à droite. On y voit une chair de coco fibreuse crème, bordée de sève ambrée qui coule en filets (le « sang » des cocos de T-162).
- Mèches de bourre arrachées.
- Masque gris-brun gorgé d'eau, avec une fente du haut jusqu'à l'orbite droite et un coin ébréché. L'ocre est presque partie, les traînées de sève partent des orbites.
- Chaux sale ; une dent manquante, une cassée.
- Yeux injectés, membres plus maigres (muscle 0,78 au lieu de 1,15).
- Bandages souillés sur l'avant-bras gauche, le tibia droit et le genou gauche. Pagne déchiqueté.

### Données techniques

| Mob | LOD0 / LOD1 / LOD2 (tris) | Arme | Modules (Head, Torso, Pelvis, bras, jambes) | Boîte (m) |
|---|---|---|---|---|
| Gardien Crabe | 53 897 / 26 946 / 10 778 | 5 996 | 28,2 k / 22,2 k / 5,7 k / 2,9-2,7 k / 2,2-2,3 k | 1,13 × 0,79 × 1,26 |
| Veilleur des Cendres | 54 043 / 27 020 / 10 808 | 3 984 | 27,1 k / 20,8 k / 14,2 k / 2,4-1,9 k / 1,8 k | 1,21 × 0,85 × 1,44 |
| Guerrier Coco | 53 721 / 26 852 / 10 741 | 6 000 | 20,2 k / 31,4 k / 11,9 k / 3,9-3,5 k / 4,0-4,6 k | 0,88 × 0,64 × 1,15 |

**Textures cuites** : Cycles, 8 échantillons, AO 48. Couche UV `UVBake` avec îlots pondérés (visage ×2, fibres ×0,3).
- `T_<Mob>_Body_{BC,N,ORM,E}` : 2048.
- `T_<Mob>_Weapon_*` : 1024 (E pour le Veilleur seulement).
- `T_<Mob>_Gore_*` : 1024, atlas 4 × 4 des couches de coupe (E pour le Veilleur).

Formats : BC en sRGB ; N en normale OpenGL, *Flip Green Channel* posé à l'import ; ORM = AO / rugosité / métal ; E émissif normalisé, l'intensité vient du MI.

**Matériaux** : `MI_<Mob>_{Body,Weapon,Gore}`, parent `M_Character_Baked`.

| MI | EmissiveStrength | EmissivePulse | EmissivePulseSpeed |
|---|---|---|---|
| Coco et Crabe, Body | 1,0 (yeux) | — | — |
| Veilleur, Body | 5,0 | 0,35 | 0,32 |
| Veilleur, Weapon | 3,0 | 0,3 | 0,27 |
| Veilleur, Gore | 4,0 | 0,25 | 0,4 |

`M_Character_Baked` a reçu une pulsation, une seule fois (balise `t274_pulse` = 1) : émissif × (1 + `EmissivePulse` × sin(2π t `EmissivePulseSpeed`)). `EmissivePulse` vaut 0 par défaut, donc les autres instances (Villageois, crabe géant…) ne changent pas.

**Correspondance des emplacements** : le réimport hérité garde l'ancienne liste d'emplacements.
- Corps : l'emplacement 0 devient `M_<Mob>_Body` → `MI_<Mob>_Body`. Les anciens emplacements (`M_<Mob>_Shell`, `_Mask`…) restent, inutilisés.
- Arme : emplacement 0 = `M_<Mob>_Weapon`.
- Modules : `M_<Mob>_Body` (extérieur) et `M_<Mob>_Gore` (coupes), sur les emplacements 0 et 1.
- Les emplacements sont renommés d'après le matériau importé : `TikiDismember::ApplyMaterials` retrouve donc le corps par nom comme par asset.

**Réimport (2026-10-10)** : action `mobsvisual`, un mob à la fois, aucun échec.
- Coco : 19,5 s, Crabe : 28,3 s, Veilleur : 20,6 s.
- Squelette, Physics Asset et `weapon_socket` conservés ; 3 LOD par corps.
- Les Blueprints, clips, `DA_<Mob>_DismemberSet`, démos `SK_Demo_*` et sangs par espèce gardent leurs références : mêmes assets.
- L'arme ramassée (`Items/MobWeapons/Meshes/SM_<Mob>_<Arme>`, T-161) est réimportée, puis sa collision est remise à une seule enveloppe K-DOP 26. Le réimport hérité gardait les anciens convexes et en ajoutait un à chaque passage.

### Pièges rencontrés (Blender 5.2)
- **Booléen EXACT** sur un masque solidifié posé sur l'œuf : il vide la plaque (16 faces au lieu de 6 000), parce que la plaque se recoupe légèrement.
  - FLOAT oublie parfois un trou (bouche du Veilleur).
  - `HorrorMixin.check_booleans` essaie donc EXACT avec auto-intersection (`use_self`), puis FLOAT, puis MANIFOLD. Il garde le premier résultat plausible : au moins 70 % des faces, un changement réel, et aucun sommet hors de la boîte.
- **Masque plus large que l'œuf** à sa hauteur (Veilleur) : la plaque se replie sur le flanc, et le *even offset* du Solidify crée des sommets à 60 m. Correctifs : demi-largeur du contour bornée à 88 % du rayon du corps, *even offset* coupé.
- **Facettes de la croûte** : elles sont aplanies autour du plan moyen de chaque cellule (centre rentré, bords sortis). Aplanies vers le plan tangent, elles traversaient le masque posé 4,4 cm devant.
- **Unreal** : `MaterialEditingLibrary.set_material_instance_*_parameter_value` renvoie toujours `False` en 5.8 (code source). Le script relit les valeurs avec `get_material_instance_*_parameter_value`.

### Limites et suites
- **Démos de déchiquetage** (`SK_Demo_<Mob>_*`, T-168) : elles gardent l'ancien aspect stylisé (fragments cuits du 2e PC). Elles sont à refaire à partir des nouveaux modèles.
- **Icônes d'inventaire** des armes ramassées (`T_Icon_<Arme>`, T-161) : elles montrent encore les anciennes armes. Il faut relancer `scripts/ui/render_mob_weapon_icons.py` sur `Horror/SM_<Mob>_<Arme>.fbx`.
- **Postures** : elles viennent des clips existants. Les rendus utilisent une garde plus menaçante (accroupi, buste en avant, tête basse), pour l'image seulement. Une posture inquiétante en jeu relève des animations (T-275).
- **Pas de morph** `DeadEyes` (l'import hérité ne les prend pas). La pulsation suppose `M_Character_Baked` à jour : si la balise manque, l'émission reste fixe.

### À vérifier en PIE
- **Jour** :
  - lecture des trois silhouettes à 10-20 m ;
  - le masque gris du crabe et celui du Veilleur ressortent sur les corps sombres ;
  - les plaies du coco se voient de profil ;
  - rien de trop brillant (rugosité des membres) ;
  - les yeux du crabe et du coco ne doivent pas « éclairer » en plein jour.
- **Nuit** (cycle jour/nuit) :
  - yeux laiteux du crabe, sclère du coco et braises du Veilleur visibles sans torche ;
  - pulsation lente des braises (environ 3 s), pas de clignotement ;
  - reflets mouillés sous la torche.
- **Combat** :
  - coupes au sabre et éclatement au mousquet : les modules s'alignent au repos, les faces de coupe ont les bonnes couleurs (bleu pour le crabe, braise pour le Veilleur) ;
  - l'arme ramassée tombe et se pose sur sa nouvelle collision.

## Refonte des mécaniques (T-275, lot L5)

Demande du propriétaire (2026-10-10) : « rends ces mobs : crab guardian, ashwatcher, cocowarrior, améliorer leurs design et texture, fais un design comme Outlast […] rework tous les mécanismes ». Cette section couvre les **mécaniques**. Le visuel relève de T-274 (section précédente), avec les mêmes squelettes et les mêmes clips. Cahier : §6.2 et §6.3 (menaces désignées, mêlée avec préparation visible, validation serveur, cooldown). Recette liée : §12 n°21 (un parcours combat/collecte reste atteignable en solo, à deux et à quatre).

**Statut honnête** :
- **Fait** :
  - C++ écrit et compilé fichier par fichier (UBT `-SingleFile`), cibles éditeur et jeu, 0 erreur.
  - Spec `monke_island.Tiki.Tactics` écrite et compilée.
  - 34 sons générés par script.
  - Action éditeur `mobsmech` écrite.
- **Pas encore fait** :
  - le build complet ;
  - l'exécution des specs ;
  - l'action `mobsmech`, qui attend le build ;
  - le PIE solo et à plusieurs ;
  - le build packagé.
- **Aucun `.uasset`** n'a été créé ni modifié par cette tâche à ce jour.
- **Aucun Blueprint n'est touché.** Le rôle d'un mob se lit dans le nom de sa classe.

### État trouvé
- Une seule classe pour les trois mobs : `ATikiWarrior`.
  - Comportement identique : repérer, grogner, courir droit au joueur, puis frapper après `AttackWindup` (0,45 s).
  - Valeurs communes : `WarriorHealth` 80, `WarriorRunSpeed`, `AttackDamage` 15…
  - Seules différences entre les mobs : clips, voix, arme lâchée et sang.
- Déjà en place, et conservé :
  - furtivité (T-262) ;
  - embuscades et alarmes du `TikiDirector` ;
  - mousquet des tiki armés (T-214) ;
  - réactions, démembrement et démos (T-150 à T-168).
- La bibliothèque d'animations des mobs (`art/export/Characters/MobAnimManifest.json`) contenait des clips non branchés :
  - `A_<Mob>_Block` (parade de 0,87 s ; pour le crabe : « carapace : −90 % de dégâts de face, vulnérable de dos ») ;
  - `_Strafe_L/R` et `_Walk_Back` (boucles) ;
  - `_Taunt` (le Veilleur y rugit).

### Les trois rôles
| Mob | Rôle (`ETikiArchetype`) | En une phrase |
|---|---|---|
| Guerrier Coco | `Stalker` (traqueur) | il tourne autour de sa proie dans l'ombre, s'approche quand on ne le regarde pas, feinte puis charge avec un coup lourd annoncé, s'enfuit seul et blessé |
| Gardien Crabe | `Bulwark` (rempart) | il avance de biais derrière une pince qui bloque tout coup de face, saisit et projette, se met entre le joueur et un allié fragile |
| Veilleur des Cendres | `Watcher` (veilleur) | il reste à distance derrière un buisson, aveugle avec un nuage de cendres, recule en semant des braises, hurle pour appeler les autres |

Une classe dont le nom ne contient aucun mot-clé de `Pack.Archetypes` (`CocoWarrior`, `CrabGuardian`, `AshWatcher`) reste `Brawler`, c'est-à-dire la mêlée d'avant. Une instance de niveau peut forcer son rôle (`Archetype`).

### Guerrier Coco : le traqueur (`DA_TikiRules.Stalker`)
- **Traque** :
  - il tourne autour de sa cible entre `StalkRadiusMin` et `StalkRadiusMax` (7 à 11 m), face à elle, en pas chassés ;
  - il change de sens toutes les 2,5 à 5 s ;
  - quand la caméra du joueur le regarde (`WatchedHalfAngle`, 35°), il recule au rayon maximal ;
  - quand on ne le regarde pas, il se rapproche au rayon minimal, à `CreepSpeed` ;
  - s'il reste `UnwatchedRushSeconds` (2,5 s) dans le dos, assez près, il charge par derrière ;
  - pendant la traque, on entend sa respiration derrière le masque et des cliquetis de fibre (sons `Stalk`, à moins de 25 m de la caméra), qui le trahissent ;
  - la traque dure de `StalkSecondsMin` à `StalkSecondsMax` (4 à 8 s).
- **Encerclement** : à plusieurs traqueurs sur la même proie, chacun prend une place sur un cercle de `EncircleRadius` (5,5 m).
  - La première place est dans le dos du joueur, les autres se répartissent à partir de là.
  - L'ordre est stable : chaque traqueur garde son rang.
  - Ils attaquent quand ils sont en place, ou au bout de `EncircleMaxSeconds` (6 s).
- **Feinte puis charge** :
  - la feinte (`FeintChance`, 60 %) : un sifflement, une fausse ruée et un pas en arrière (0,35 s + 0,6 s) ;
  - puis la vraie charge, à `ChargeSpeed` (5,4 m/s), annoncée par un rugissement.
- **Coup lourd annoncé** :
  - à `HeavyRange` (2,3 m), il s'arrête face à sa cible ;
  - grognement montant et craquement de bois (son `Windup`) ;
  - fente ralentie (`A_CocoWarrior_Attack_Lunge`, vitesse réglée pour que l'impact tombe à la fin de `HeavyWindup`, 0,85 s) ;
  - le coup part dans la direction visée : un pas de côté l'esquive ;
  - dégâts `HeavyDamage` 30, recul 6,5 m/s ;
  - il reste ensuite ouvert `RecoverSeconds` (1,2 s), puis recule dans l'ombre et reprend la traque ;
  - frappé pendant sa préparation ou sa feinte, il perd son coup.
- **Coup rapide** : si le joueur se jette sur lui, il donne le coup classique, puis s'éclipse.
- **Fuite** : sous `FleeHealthFraction` (35 %) de vie, sans autre guerrier à moins de `FleeAllyRadius` (15 m), il s'enfuit en gémissant vers un buisson éloigné du joueur.
  - Un guerrier de nuit, d'embuscade ou de défense s'y cache pour de bon.
  - Un gardien de village y attend `CowerSeconds` (12 s), puis revient : une menace désignée ne disparaît jamais (§6.2).
  - Rattrapé à `CorneredDistance` (2 m), il se retourne et charge.
  - Il ne refuit pas avant `FleeCooldown` (25 s).
- **Torche** : une torche allumée dans la main du joueur le tient à `TorchFearRadius` (4 m), 6 m si elle est levée.
  - La peur dure `TorchCourageSeconds` (10 s), ou cède dès que 3 traqueurs chassent ensemble.

### Gardien Crabe : le rempart (`DA_TikiRules.Bulwark`)
- **Tank** : vie ×`HealthScale` 1,6 (128 au lieu de 80), coup de pince ×1,2.
- **Garde de pince** : face au joueur, la garde arrête tout coup de face, dans un demi-angle de `GuardHalfArc` (70°).
  - Coups arrêtés : sabre, lance, massue, torche, poing et tirs.
  - Il ne prend alors aucun dégât, ne saigne pas et ne perd aucun membre.
  - Chaque coup arrêté joue la parade (`A_CrabGuardian_Block`), un « clang » de chitine et des étincelles.
  - Un attaquant au corps à corps est repoussé (3 m/s).
  - Un coup de côté ou de dos passe.
  - Une explosion ou une brûlure passe aussi.
- **Garde brisée** : la garde casse si les coups arrêtés atteignent `GuardBreakDamage` (70) en `GuardBreakWindow` (4 s), ou sous un seul coup d'au moins `GuardBreakBlow` (40).
  - Exemples : deux coups de sabre, deux balles de pistolet, une balle de mousquet.
  - Il titube alors, avec un craquement de carapace, et reste ouvert `GuardBrokenSeconds` (3 s).
  - Le mousquet tue d'un coup (10 000 de dégâts). Une part de dégâts de face ne voudrait donc rien dire : la garde les arrête tous (`GuardShotDamageScale` 0). Le mousquet ouvre la garde ; il faut finir au sabre ou au pistolet, ou le prendre à revers.
- **Avance de biais** : à moins de `GuardDistance` (9 m), il avance en crabe, garde levée.
  - Lent : `GuardMoveSpeed` 1,2 m/s.
  - De biais : `SideApproachAngle` 40°, côté changé toutes les 2,5 s.
  - Il tourne lentement (`GuardTurnRate` 110°/s) : courir autour de lui atteint son flanc.
  - Plus loin, il court, garde baissée.
- **Saisie et projection** (code propre, sans rien du crabe géant) :
  - à `GrabRange` (2,1 m), sa pince s'ouvre : claquements de plus en plus rapides, balayage ralenti (`A_CrabGuardian_Attack_Sweep`), pendant `GrabWindup` (0,8 s) ;
  - elle se referme droit devant : `GrabReach` 2,4 m, demi-angle 50° ;
  - le joueur pris subit `GrabDamage` 14, puis il est projeté sur le côté, en ragdoll (`AIslanderCharacter::Knockout`) ;
  - la projection l'écarte de l'allié que le crabe protège (`ThrowSpeed` 7,5 m/s, `ThrowUpSpeed` 4,2 m/s) ;
  - un pas de côté pendant l'ouverture l'évite ;
  - cooldown 7 s ;
  - un coup fort reçu pendant l'ouverture (au moins `StaggerDamage`) la coupe.
- **Protection** : il se place entre le joueur et un allié, à `InterposeDistance` (2,5 m) de cet allié.
  - Allié concerné : un Veilleur, ou un allié sous 50 % de vie que le joueur approche à moins de `WardThreatRadius` (9 m).

### Veilleur des Cendres : le veilleur (`DA_TikiRules.Watcher`)
- **Hurlement** : quand il prend une cible, il s'arrête et hurle (`A_AshWatcher_Taunt`, son `Howl` entendu de loin), au plus une fois par `HowlCooldown` (30 s).
  - Les guerriers sans cible à moins de `HowlRadius` (40 m) accourent à l'endroit où il a vu le joueur.
  - Jusqu'à `HowlWakeAmbushers` (1) monstre caché dans un buisson proche en bondit (`ATikiDirector::WakeAmbushers`), dans la limite de monstres autour du joueur.
  - Ils n'attaquent que s'ils voient le joueur une fois sur place (furtivité respectée).
- **Distance et couvert** : il garde le joueur entre `KeepDistanceMin` et `KeepDistanceMax` (9 à 16 m).
  - Il se place derrière le buisson le plus proche de ce point, s'il y en a un (`CoverSearchRadius` 8 m).
  - Il replace sa position toutes les 2 s.
- **Nuage de cendres** : entre 4 et 18 m, il inspire la cendre (souffle grave et grésillement, coup par-dessus la tête ralenti, `CastWindup` 0,9 s), puis lance une motte.
  - La motte vise où sera le joueur dans 0,35 s, avec une avance d'au plus 3 m.
  - Elle suit un arc visible (au moins 0,4 s de vol) et tombe là où elle était visée : jamais de tête chercheuse.
  - Elle éclate en un nuage de `CloudRadius` (3,2 m) qui reste `CloudSeconds` (6 s).
  - Un joueur dedans est aveuglé `BlindSeconds` (2,5 s) : un voile de cendre à 85 % sur son écran seulement (le HUD reste lisible), avec un sifflement d'oreilles.
  - Il est aveuglé de nouveau, au plus toutes les 1,5 s, tant qu'il reste dans le nuage.
  - Cooldown 9 s.
- **Repli et braises** : un joueur à moins de `RetreatDistance` (6 m) le fait reculer face à lui, à 1,2 m/s.
  - Toutes les `EmberCooldown` (7 s), il laisse entre eux une plaque de braises de 1,8 m de rayon, pendant 5 s.
  - Les braises brûlent tout ennemi des tiki qui s'y tient : 6 points de vie par seconde.
  - Si le joueur s'approche à moins de 3,8 m, il se retourne et court.
  - Bloqué par un mur ou la mer, il s'écarte de côté.
  - Rattrapé au contact, il frappe de sa massue (le coup classique).

### Communs
- **Meute** : jamais plus de `Pack.MaxAttackersPerFoe` (2) attaquants à la fois sur une même cible.
  - Comptent comme attaques : coup, feinte, charge, saisie, lancer.
  - Les autres attendent à leur place.
- **Joueur à terre épargné** (`bSpareDownedFoes`) : aucune attaque ne démarre sur un joueur en ragdoll (projeté, assommé).
- **Annonces** : toute grosse attaque a une préparation d'au moins 0,6 s, un son et un clip dédiés. Les specs vérifient ces valeurs par défaut (« Fairness »).
- **Furtivité respectée** (T-262) :
  - les tactiques ne tournent que sur une cible que le mob voit ;
  - hors de vue, il va au dernier endroit vu ou entendu ;
  - le regard du joueur ne sert qu'à la traque d'une cible déjà vue ;
  - le hurlement donne un lieu, jamais une position suivie.
- **Torche la nuit** (`Pack.bTorchRevealsAtNight`, proposition ouverte dans `furtivite.md`, à arbitrer) :
  - un joueur debout avec une torche allumée est vu des guerriers jusqu'à `TorchNightSightCm` (25 m), même la nuit ;
  - il est aussi vu des monstres cachés dans les buissons ;
  - accroupi, il reste caché ;
  - le jour, rien ne change.
- **Sons générés** :
  - script `scripts/audio/tiki_tactics_sounds.py` : numpy/scipy, œuvre originale, déterministe ;
  - 34 fichiers dans `art/audio/creatures/tiki_tactics/` ;
  - Coco : respiration de traque, alerte à claquements, feinte sifflée, grognement de préparation, gémissement de fuite ;
  - Crabe : claquements d'alerte, ouverture de pince, clang de parade, craquement de garde, saisie ;
  - Veilleur : hurlement, inspiration de cendre, grondement ;
  - cendre : éclatement du nuage, braises, sifflement d'oreilles en 2D.
  - Contrôle fait : niveaux et spectrogrammes. Ils n'ont pas encore été écoutés en jeu.

### Architecture
- `UTikiTactics` (`Creatures/TikiTactics.h`) est le cerveau serveur du rôle.
  - C'est un composant sans tick, créé à `BeginPlay` par `ATikiWarrior` selon son archétype, sur le serveur seulement.
  - Sous-classes : `UTikiStalkerTactics`, `UTikiBulwarkTactics`, `UTikiWatcherTactics`.
  - Le guerrier garde la cible, la furtivité, le bond hors du buisson et le mousquet.
  - Le guerrier appelle les tactiques ainsi :
    - `ThinkBusy` : les coups lancés vont au bout ;
    - `OnEngaged` ;
    - `ThinkEngaged` : comment combattre ;
    - `FilterDamage` : la garde ;
    - `OnDamaged`.
- Pas de StateTree ni de NavMesh : on reste dans la logique serveur écrite à la main des tiki, cohérente avec l'atoll au 1:1 sans NavMesh.
- **Réseau** (serveur autoritaire) :
  - **Répliqué** :
    - `FTikiTacticRep` (état, déplacement face à la cible, garde levée) : clips de pas chassés et de marche arrière, sons de traque ;
    - les actions multicast (`Feint`, `HeavyAttack`, `Grab`, `GuardBreak`, `Howl`, `Cast`, `Flee`) ;
    - les signaux sonores ponctuels (`MulticastTacticCue`, non fiable : parade, saisie).
  - **`ATikiAshBurst`** :
    - lancer répliqué une fois, puis `bBurst` ;
    - chaque machine calcule le même arc ;
    - aveuglement par `MulticastBlind` fiable : seule la machine du joueur touché pose le voile (`UTikiVeilSubsystem`, fondu de caméra, jamais par-dessus un fondu d'un autre système).
  - Dégâts, garde, saisie et braises sont décidés sur le serveur.
- **Données** : tout est dans `DA_TikiRules`, catégorie `Tactics` (`Pack`, `Stalker`, `Bulwark`, `Watcher`, et `Watcher.Ash` pour l'apparence).
  - Les clips et les sons de chaque rôle y sont aussi : un rôle correspond à un mob.
  - Les BP restent intacts, et T-274 refait leur visuel en parallèle.
  - Un clip d'un autre squelette n'est jamais joué (`IsClipForBody`).
  - Sans `mobsmech`, les tactiques tournent quand même, avec les clips des BP (marche, `Taunt`, `Attack`) et sans sons nouveaux.
- **Modifications de l'existant** :
  - `ATikiCreature` :
    - `PlayLocalAction` devient virtuelle et protégée ;
    - `CanSpot` devient virtuelle ;
    - nouveaux points d'accroche `GetDirectionalClip` et `IsLocomotionClip` ;
    - `PlayOneShot` ;
    - vitesse de lecture des clips.
  - `ATikiWarrior` :
    - le coup classique passe par `BeginStrike` et `StrikeWith`, sans changement de valeurs ;
    - l'archétype est lu depuis le nom de classe ;
    - la vie dépend du rôle.
  - `ATikiDirector` : `WakeAmbushers`, et la torche déclenche aussi les embuscades la nuit.
- Fichiers ajoutés dans `Source/monke_island/Creatures/` :
  - `TikiTacticsTypes.h`, `TikiTacticsMath.h` ;
  - `TikiTactics.h/.cpp`, `TikiStalkerTactics.h/.cpp`, `TikiBulwarkTactics.h/.cpp`, `TikiWatcherTactics.h/.cpp` ;
  - `TikiAshBurst.h/.cpp`, `TikiVeil.h/.cpp` ;
  - `Tests/TikiTacticsSpec.cpp`.

### Spec `monke_island.Tiki.Tactics`
Elle couvre :
- le rôle par nom de classe ;
- la meute : jetons d'attaque, rang, places du cercle ;
- le traqueur : regard, rayons, orbite, fuite, peur de la torche ;
- le rempart : type de coup, arc de garde, dégâts filtrés, garde brisée, interposition, urgence de protection, approche de biais, projection ;
- le veilleur : couvert, distance, avance de visée, arc, durée de vol, zone du nuage ;
- la torche la nuit ;
- le choix des clips de pas chassés ;
- l'équité des valeurs par défaut.

Elle n'est pas encore exécutée. Les specs existantes (`Tiki.Rules`, `Tiki.CombatReactions`, `Hostility`…) ne sont pas modifiées : aucune de leurs règles ne change.

### Action éditeur `mobsmech` (`scripts/editor/tiki_mechanics_setup_ue.py`)
- Elle est idempotente et ne touche aucun Blueprint.
- Elle crée :
  - les 34 sons dans `/Game/MonkeIsland/Characters/Tiki/Audio/Tactics/` ;
  - deux atténuations : `ATT_Tiki_Tactics_Near` et `_Far` (hurlement) ;
  - `CON_Tiki_Stalk`, qui limite à 2 les respirations simultanées ;
  - la classe `SC_Effects` sur ces sons ;
  - `M_Tiki_AshCloud`, matériau translucide pour le nuage.
- Elle remplit `DA_TikiRules` :
  - les clips de chaque rôle ;
  - les vitesses sans glissement du manifeste ;
  - la vitesse du clip signature, calculée pour que l'impact tombe à la fin de la préparation ;
  - les sons et les atténuations ;
  - l'apparence de la cendre : sphère et cylindre du moteur, flamme du mousquet, fumée et étincelles du fusil ;
  - les étincelles de la garde.
- Avant le build, elle prépare les sons et le matériau, puis échoue proprement (« build the editor with the T-275 code, then run mobsmech again »).
- Rapport : `monke_island/Saved/T275/tiki-mechanics-setup.json`.

### À vérifier en PIE (L_Test_Flat, après le build et `mobsmech`)
Le journal de chaque mob doit afficher « … fights as Stalker / Bulwark / Watcher » (`LogMonkeTactics`). Puis, par mob :

| ID | Situation | Attendu |
|---|---|---|
| TAC-01 | Coco | Il tourne autour du joueur, recule quand on le regarde et se rapproche dans le dos ; respiration audible. |
| TAC-02 | Coco | Feinte, puis charge et coup lourd annoncé ; un pas de côté l'esquive. |
| TAC-03 | Deux Coco | L'un se place dans le dos. |
| TAC-04 | Coco blessé et seul | Il fuit vers un buisson. |
| TAC-05 | Coco, torche levée | Il garde ses distances environ 10 s. |
| TAC-06 | Crabe de face | Sabre et mousquet arrêtés (clang, étincelles, parade, pas de sang). |
| TAC-07 | Crabe | Deux coups de sabre brisent la garde (titube 3 s) ; de dos, les coups passent. |
| TAC-08 | Crabe | Saisie annoncée, puis projection latérale. Personne ne frappe le joueur à terre. |
| TAC-09 | Veilleur | Hurlement, puis des guerriers viennent voir. |
| TAC-10 | Veilleur | Motte en arc, nuage, voile à l'écran, le HUD reste visible. |
| TAC-11 | Veilleur | Recul avec braises qui brûlent. |
| TAC-12 | Hôte + client | Mêmes clips, sons et voile chez le client touché. |

Mesures à prendre : temps serveur des pensées avec 10 guerriers, nombre de nuages et de plaques actifs.

### Limites connues
- **Valeurs** : non réglées en jeu (vitesses, rayons, délais).
- **Clips** :
  - la feinte réutilise `Taunt` ;
  - le lancer réutilise `Attack_Overhead` ;
  - la saisie réutilise `Attack_Sweep` ;
  - les pas chassés ont des foulées courtes (17 à 21 cm/s) : léger glissement au-delà de `MaxClipRate`.
- **Visuel du nuage** : premier jet (sphère translucide et fumée du fusil répétée), à revoir par le tech artist. Pas d'effet dédié aux braises : étincelles du fusil.
- **Regard du joueur** : c'est celui de sa caméra (rotation de contrôle).
- **Voile** : c'est un fondu de caméra. Il ne marche pas si un autre fondu est en cours.
- **Torche la nuit** : règle à arbitrer par le propriétaire. Pour la couper : `Pack.bTorchRevealsAtNight`.
- **Pas fait** : les soldats et les villageois ne réagissent pas à la torche.
- **Non testé** : multijoueur, réseau dégradé, coût des braises sur le crabe géant.

## Réseau des créatures (T-306, T-312, lot L4 ; revue réseau T-268 R-27, R-28, R-34)

### Télégraphes en état répliqué (T-306, R-27)
- **Avant.** Les actions ponctuelles (coup, coup lourd, saisie, jet de cendre, feinte, hurlement, alerte avant le tir de mousquet) partaient par un multicast non fiable, sans état. À 1 à 3 % de perte, ou pour un mob qui devenait pertinent pendant sa préparation, le client prenait le coup sans avoir vu la préparation (cahier §5).
- **Maintenant.** Le serveur écrit l'action dans un état répliqué `FTikiActionRep` : action, variante, numéro, heure serveur de départ. Il force un envoi.
  - Chaque client la joue une fois, décalée de son âge (`TikiMath::LateActionStart`). Le clip démarre aussi loin que celui du serveur et finit avec lui : le coup tombe à l'écran quand le serveur l'applique.
  - Une action déjà finie n'est jamais rejouée (mob revenu en pertinence, joueur arrivé en cours).
  - Les sons d'une action reçue plus de `ActionLateSoundSeconds` (0,5 s) après son départ sont omis.
  - Le non fiable reste pour le sang (`MulticastBloodHit(s)`) et les indices des tactiques (`MulticastTacticCue` : parade, pince qui se ferme).
- Spec : `monke_island.Tiki.Net` (cas `LateAction`).

### Fréquences réseau (T-312, R-28)
Valeurs dans `DA_TikiRules`, catégorie `Network`, appliquées par le serveur au `BeginPlay` :

| Valeur | Défaut | Note |
|---|---|---|
| WarriorNet | 30 Hz, 2 Hz min, 150 m | guerriers : coups et traque à suivre de près |
| VillagerNet | 15 Hz, 2 Hz min, 100 m | villageois : un joueur loin n'en a pas besoin |
| CorpseNetUpdateFrequency | 20 Hz | cadavre ou morceau en mouvement (chute, traîné, porté) |
| ActionLateSoundSeconds | 0,5 s | voir T-306 |

- Le minimum ne sert qu'avec `net.UseAdaptiveNetUpdateFrequency 1` (0 par défaut dans le moteur). C'est une décision projet, laissée à l'intégrateur.
- **Cadavres et totems au repos** (`UNetRestComponent`, `Items/NetRestComponent.h`) :
  - un cadavre (`ATikiCorpse`, `ATikiRagdollCorpse`) ou un totem (`BP_Totem*`, props) endormi ne se réplique plus (dormance) ;
  - il se réveille dès qu'il bouge (événement de réveil physique, ou contrôle toutes les 0,25 s), qu'on le saisit, qu'il est porté, caché, attaché ou que sa valeur change ;
  - le ragdoll se réveille aussi à chaque coup reçu.
- Une relique prise ou reposée par un villageois réveille son réseau à l'instant même (`ATikiVillager::AttachRelic`).
- **Requin** (`DA_SharkRules`, catégorie `Shark|Network`) :
  - 20 envois par seconde au lieu de 100 (`NetUpdateFrequency`) ;
  - le client prolonge le requin par sa vitesse répliquée entre deux envois, pendant 0,25 s au plus (`ClientExtrapolateTime`) ;
  - il fait glisser le corps sur chaque correction en 0,12 s (`ClientSmoothTime`), au lieu du saut de chaque mise à jour (R-34) ; au-delà de 4 m, le corps saute (`ClientMaxSmoothDistance`) ;
  - la queue bat enfin à la vitesse réelle chez le client.
- Spec : `monke_island.Tiki.Net` (cas `Rates`).

### Statut honnête (2026-10-11, `ue-network-engineer`)
- `compilé` : UBT, fichier par fichier, éditeur ouvert. En-têtes réfléchis modifiés (`ATikiCreature`, `UTikiRules`, `ATikiCorpse`, `ATikiRagdollCorpse`, `USharkRules`, `ASharkActor`) : il faut un build complet, éditeur fermé.
- Non fait : PIE, specs non exécutées, mesures non prises.
- Essais à mener (`docs/audit/revue-reseau-T268.md`) :
  - n°15 : à 3 % de perte, compter les coups reçus sans préparation visible ; attendu, aucun ;
  - n°19 : `stat net` sur l'hôte avec 3 clients, en village de nuit, avec cadavres ; relever les octets sortants par connexion, contre 16 Ko/s par client ; Networking Insights (`-trace=default,net -NetTrace=1`).
