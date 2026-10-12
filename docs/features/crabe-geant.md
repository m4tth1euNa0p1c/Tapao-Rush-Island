# Crabe géant mini-boss « Tupa Nui » (T-211)

**Lot :** L5 (PvE). Demande du propriétaire du 2026-10-07 : « récupérer le modèle Blender du crabe mini-boss, ses déplacements et toutes ses animations sont dans OneDrive ; le charger dans la zone Test Flat, cercle rouge, avec son IA d'attaque de tout ce qui est vivant ; il ne sort pas du cercle ».
**Maquette :** 2e PC, `OneDrive\maquette-blender\maquettes\crabe-geant` v0.2.0 (lecture seule) : `SK_GiantCrab` + LOD1-3, 58 os, morph `DeadEyes`, 21 clips à 30 i/s, `ue/GiantCrabManifest.json` (clips, notifies, valeurs de jeu proposées), `ue/import_giant_crab.py`. Lien prévu avec T-207 (crabes de la campagne).

## Comportement (serveur autoritaire, `AGiantCrab`)
- **Proie** : le vivant le plus proche **dans son arène** (`AArenaZone` où il apparaît ; sans arène, une laisse de `LeashRadiusCm` autour de son départ) : joueurs, villageois, guerriers tiki. Un autre crabe géant est épargné. Il garde sa proie tant qu'une autre n'est pas `SwitchMarginCm` plus proche.
- **Déplacement** : loin (`ScuttleDistanceCm`), il se met de profil et court de côté (`ScuttleSpeed`) ; près, pinces en avant, il approche et tourne autour de sa proie (`CircleShare`), lentement en avant, plus vite de côté. Jamais un pas hors du cercle (et l'arène le retient en secours, `ExtraConfinedClasses`).
- **Coups** (tirés à l'avance selon `Weight`, il s'approche à la portée du coup choisi) : balayage de pince (25), écrasement de pince avec onde de choc (40 + 15 dans 2 m), saisie (10) puis lancer (20, voir « Lancer »). Recul des frappés ; le saisi reste dans la pince jusqu'au lancer, mouvement coupé (`MOVE_None`) : ni déplacement, ni saut, ni bascule en vol.
- **Défense** : pinces levées contre un coup de face (`BlockChance`, -90 %), recul sur un gros coup (`HitReactDamage`).
- **Dégâts reçus** : un coup compte au plus `IncomingDamageCap` (150 : le mousquet fait 10 000) ; carapace ×0,1, pattes ×0,5, points faibles yeux ×3, articulations ×2, ventre ×4, bouche ×1,5.
- **Renversement** : 3 coups sur des points faibles en 6 s, un tir à l'œil ou au ventre, ou un coup au coude droit pendant que la pince est plantée après l'écrasement ; 5 s sur le dos, ventre offert (×2), puis il se relève. Il meurt sur le dos s'il est renversé.
- **Sans proie** : il claque des pinces de temps en temps.

## Lancer (owner 2026-10-08)
Demande : « quand le crabe géant m'attrape avec sa pince et me lance, ça doit me lancer TRÈS loin, en tenant compte de la gravité, avec un ragdoll pendant la chute ».
- **Relâche** : à `ThrowRelease` (0,5 s) du clip de lancer, le serveur remet le saisi en `MOVE_Falling` et le projette par-dessus la pince gauche (direction locale (0,6 ; -0,8) sur le sol, relevée de l'angle : `GiantCrabMath::ThrowVelocity`, norme = vitesse), à la vitesse de son genre.
  - **Joueur** (`AIslanderCharacter`) : `ThrowSpeedPlayer` **2400 cm/s** à `ThrowAngleDegreesPlayer` **38°** → `Knockout(Velocity)` : ragdoll sur toutes les machines (état répliqué `KnockoutRep` depuis T-287 ; un client part du point de départ du serveur, avancé de l'âge du KO), gravité réelle sur le corps, caméra sur le bassin, pas de dégâts de chute ni de noyade en ragdoll ; le serveur seul le relève une fois immobile (`RagdollMinDuration` 2,5 s, au plus `RagdollMaxDuration` 8 s), chaque machine au point du serveur. Portée balistique ≈ 55–57 m sur sol plat sans frottement (apogée ≈ 11 m, ~3 s de vol), un peu moins avec l'amortissement du Physics Asset. Le knockout précède les dégâts `ThrowDamage` (20) : un lancer mortel projette quand même le corps (un mort ne peut plus être assommé) ; un corps qui ne peut pas passer en ragdoll (pas de Physics Asset) est lancé en `LaunchCharacter` à la même vitesse.
  - **Autres** (guerriers tiki, villageois) : `ThrowSpeed` 900 cm/s à `ThrowAngleDegrees` 35°, dégâts puis `LaunchCharacter`, inchangé.
- **Dans la pince** (`UpdateAttack`, fenêtre de la saisie) : `DisableMovement()` (`MOVE_None`) au lieu de `MOVE_Flying` — le composant de mouvement de l'islander remettait en chute un vol qu'il n'avait pas demandé (`!bWantsToFly && IsFlying()`), alors qu'il respecte `MOVE_None`. Depuis T-304, le saisi est répliqué (`HeldPrey`) : la machine du joueur saisi coupe elle-même son mouvement et le tient sous la pince qu'elle voit, le serveur ne corrige pas ses mouvements pendant la prise ; ni déplacement, ni saut, ni bascule en vol sur aucune machine ; le regard, les coups et la touche F restent libres ; X (chute volontaire) n'est pas bloqué : un joueur KO dans la pince est lâché, son ragdoll garde la main. Le serveur replace le saisi sous `SOCKET_ClawGrab_R` à chaque image, les autres machines le voient par son mouvement répliqué. Un saisi que le coup de saisie vient de tuer n'est plus emporté ; un saisi qui meurt pendant la prise est lâché sans être remis en chute.
- **Interruption** : crabe renversé, frappé, mort ou retiré du jeu (`EndPlay`, T-304) avec quelqu'un dans la pince → le saisi retombe libre (`MOVE_Falling`), sauf un mort ou un KO.
- **Journal** : `GiantCrab Tupa Nui: throws BP_Islander_C_0 at 2400 cm/s (ragdoll)`.
- **Valeurs** : `DA_GiantCrabRules` et `DA_GiantCrabRules_Sand` (copie pour les crabes qui émergent du sable) prennent les défauts de classe (2400 / 38) tant qu'ils ne sont pas surchargés.
- **Statut** : **compilé** le 2026-10-08 (lead), spec `monke_island.Creatures.GiantCrab` verte (16 tests, dont Throw). Pas encore vu en PIE : pendant l'essai (joueur invulnérable devant le crabe, 25 s) le crabe n'a fait que des balayages et un coup de pince, jamais de prise. À voir par le propriétaire dans le build v1.5.

## Fichiers
- C++ : `Creatures/GiantCrab.h/.cpp`, `Creatures/GiantCrabRules.h` (`UGiantCrabRules`, toutes les valeurs), `Creatures/GiantCrabMath.h` (décisions pures), `Arena/ArenaZone` (`ExtraConfinedClasses`, `IsConfined`). Animation sans AnimBP : `UTikiAnimInstance` (fondus de clips).
- Assets `/Game/MonkeIsland/Characters/GiantCrab/` : `Meshes/SK_GiantCrab` (sockets `SOCKET_*`, `WeakPoint_*`), `Anims/A_GiantCrab_*` (+ montages et blend space du kit, non utilisés), `Materials/MI_GiantCrab_Body` / `_Legs` / `_Eyes` (v2, textures cuites `Textures/T_GiantCrab_*` ; voir « Modèle v2 » ; les MI v1 à couleur de sommet ne servent plus), `DA_GiantCrabRules`, `BP_GiantCrab`.
- Niveau : `L_Test_Flat`, acteur `GiantCrab_TupaNui` au centre de l'`ArenaZone` (dossier Arena).
- Scripts (éditeur ouvert, sans partie, `scripts/editor/lead_session_t186.py`) : action `crabv2import` (`scripts/creatures/giant_crab/import_giant_crab_v2_ue.py`, v2 construite dans le dépôt ; `crabimport` y renvoie aussi), action `crabsetup` (`scripts/character/giant_crab_setup_ue.py` : drapeaux d'usage des matériaux, DA depuis le manifeste, BP, pose dans l'arène).

## Pièges
- Les os du kit portent une **échelle de 100** (comme le bean) : un socket créé avec les centimètres du manifeste tombe 100 fois trop loin. Les décalages des sockets sont donc divisés par 100 (fait le 2026-10-07 par le MCP ; l'import v2 garde ces sockets ; Python ne sait pas créer de socket nommé en 5.8).
- Les portées des règles se mesurent entre les bords des corps (celles du kit depuis le centre du crabe) : balayage 195, écrasement 155, saisie 115 cm.
- `M_Bean_Master` et `M_Vegetation_VertexColor` ont maintenant les usages « Skeletal Mesh » et « Morph Targets » enregistrés (sinon l'éditeur les pose sans les sauver et bloque la fermeture sur une demande d'enregistrement).

## Recette
- `monke_island.Creatures.GiantCrab` : 16 tests (choix de la proie, tirage des coups, renversement, angles, approche, arène, et depuis le 2026-10-08 deux tests du lancer `ThrowVelocity` : norme, composante verticale, direction par-dessus la pince gauche qui tourne avec le crabe) ; `monke_island.Arena.Zone` : 3 tests. 17/17 le 2026-10-07 (headless) ; les 2 tests du lancer n'ont pas encore été exécutés.
- Testé éditeur, PIE solo `L_Test_Flat` le 2026-10-07 : le crabe reste dans le cercle, chasse et tue les guerriers tiki, frappe le joueur (balayages, recul), saisit et lance, abandonne une proie sortie du cercle et claque des pinces : `docs/production/captures/T-211/`.
- Non testé : multijoueur (le saisi est replacé par le serveur à chaque image : saccades possibles chez un client), renversement par tir ou coups réels du joueur, mort du crabe en jeu, sons (aucun son assigné dans `DA_GiantCrabRules`), clips du bean saisi/lancé du kit (non branchés), lancer du joueur en ragdoll et prise en `MOVE_None` du 2026-10-08 (à vérifier en PIE : distance et relevé, joueur figé dans la pince, lancer mortel, chute libre si le crabe est interrompu).

## Arène : soldats français et hostilité générale (T-240, demande du propriétaire du 2026-10-08)
> « Ajoute des soldats français dans le cercle rouge (l'arène), et toutes les entités s'attaquent entre elles. »

**Statut** : **compilé** le 2026-10-08 (lead), `monke_island.Creatures.Hostility` 6 tests verts, Arena et Fort.Garrison verts. **Testé éditeur** (Test Flat, PIE) : 4 soldats apparus dans l'arène (`SoldierClasses` posées sur `ArenaZone_0`, carte sauvegardée), tués par un Veilleur de cendres et un Gardien crabe, soldats frappant le crabe au sabre (journal). Dans le build v1.5.

**Factions** (`Creatures/Hostility.h`, module pur) : `EFaction { None, Player, Tiki, Soldier, Crab }` ; `Hostility::FactionOf` (pion d'un joueur ou `AIslanderCharacter` → Player, `ATikiCreature` → Tiki, villageois compris, `AFortSoldier` → Soldier, `AGiantCrab` → Crab, le reste → None) ; `AreFoes` : chaque camp est hostile à tous les autres, jamais à lui-même, None à personne ; `IsLivingFoe` = camps hostiles + cible vivante (`UIslanderHealthComponent` présent et non mort). `UHostilitySubsystem` : registre des personnages qui ont un camp, relu au plus toutes les `monke.Hostility.ScanInterval` s (0,5) ; soldats et guerriers y lisent leurs cibles au lieu de parcourir chacun tous les acteurs du monde.

**Qui attaque qui**
- Soldats (`AFortSoldier::IsEnemy` = `IsLivingFoe` du camp Soldier) : l'ennemi vu le plus proche, quel que soit son camp ; mousquet, baïonnette, sabre et alarme de garnison (`HearAlarm`) marchent sur tout ennemi. Sans garnison (dans l'arène), un soldat sent, poursuit et combat quand même, et reste debout en garde sinon ; `SetLeash(centre, rayon)` : ses buts, ses pas droits et ses fentes restent dans le cercle (le fort passe d'abord quand il en a un).
- Guerriers tiki : tout `IsLivingFoe` à portée d'`AggroRadius` (joueur, soldat, crabe), le plus proche (`ATikiCreature::FindNearestFoe`) ; leurs coups et leurs tirs blessent soldats et crabe ; le recul n'est appliqué qu'à ce qui n'est pas plus lourd qu'eux (le crabe de 850 kg tient). Les villageois sont inchangés (ils fuient le joueur, et ne réagissent pas encore à un soldat qui les blesse) ; l'apparition des tikis (directeur, villages, nuit) ne change pas.
- Crabe : déjà « tout ce qui est vivant sauf un autre crabe » (`IsPrey`), inchangé.

**Arène** (`AArenaZone`) : retient aussi les `AFortSoldier` (`ExtraConfinedClasses` par défaut) ; un soldat replacé au bord perd son chemin (`OnPutBackInside`) et reçoit la laisse du cercle au premier contrôle. Au BeginPlay (serveur) elle fait apparaître `SoldierCount` (4) soldats tirés au hasard dans `SoldierClasses` (BP_BeanFR_* à poser sur `ArenaZone_0` ; liste vide = aucun, pas de chargement par chemin) à des points aléatoires du disque (`SoldierSpawnMargin` 3 m du bord, sol par trace WorldStatic de ±`GroundTraceHeight`), sans garnison, journal `Logmonke_island` (« soldier X (BP) in the arena at … »).

**Recette** : `monke_island.Creatures.Hostility` (6 tests : camp par classe, table des camps dans les deux sens, cible vivante / même camp / soi-même / sans santé) ; `monke_island.Arena.Zone` et `monke_island.Creatures.GiantCrab` inchangés.

**À vérifier en PIE (`L_Test_Flat`)** : 4 soldats apparaissent dans le cercle et y restent ; soldats, tikis et crabe se battent sans le joueur ; les tikis hors cercle chassent toujours le joueur ; si `ArenaZone_0` a une valeur enregistrée d'`ExtraConfinedClasses`, y ajouter `FortSoldier` à la main. Point à arbitrer : la garnison d'un fort voisin d'un village prendra les villageois (camp Tiki) pour cible.

## Modèle v2 « terrifiant » (T-260, demande du propriétaire du 2026-10-09)
> « fais en sorte de modéliser le crabe, de le transformer en boss Dark Souls imbattable, avec des phases, modéliser donc ses pattes, rends-le plus rapide, lorsqu'on est sous lui il s'écrase contre le sol, rends le crabe ultra terrifiant »

T-260 (lot L5) : modèle, rig et clips ; le gameplay de boss (phases, vitesse, écrasement sous le corps, frénésie) est T-261.

**Statut** : construit dans Blender 5.2 (rendus de contrôle) et **importé dans l'éditeur** le 2026-10-09 (action `crabv2import`, 22 s : textures 2 s, matériaux 3 s, mesh + 3 LOD 11 s, 26 clips 6 s ; aucune erreur, deux avertissements ci-dessous). Vérifié dans l'éditeur : squelette conservé, emplacements de matériaux, 26 séquences avec leurs notifies, vignette du mesh rendue avec les textures cuites. **Pas encore vu en PIE.**

**Générateur** (rapatrié du 2e PC comme le mousquet : `crab_kit.py`, `crab_rig.py`, `crab_anims.py`, `artifacts/geo.py`, spec `crab_spec.json` de la maquette v0.2.0) : `scripts/creatures/giant_crab/`
```
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/creatures/giant_crab/build.py -- --bake --export --render
```
35 s environ (cuisson Cycles CPU 17 s). Sorties : `art/export/Characters/GiantCrab/` (`SK_GiantCrab.fbx`, `SK_GiantCrab_LOD1..3.fbx`, `Anims/A_GiantCrab_*.fbx`, `Textures/T_GiantCrab_*.png`, `GiantCrabManifest_v2.json`, `PHAT_GiantCrab_hint.json`), `art/blender/characters/giant_crab_v2.blend`, rendus `art/_previews/giant_crab_v2_{trois_quarts,face,pattes,avec_bean,roar}.png`, contrôles `build/giant_crab_v2/build_report.json`. Import : `scripts/creatures/giant_crab/import_giant_crab_v2_ue.py` (réimport en place, rejouable ; rapport `monke_island/Saved/T260/giant_crab_v2_import.json`). L'action `crabimport` appelle désormais cet import : relancer le script de la maquette v1 remettrait l'ancien crabe.

**Compatibilité** : même squelette `SK_GiantCrab_Skeleton` (58 os, mêmes noms et parents, repères de repos identiques : écart 0,0 entre les FBX v1 et v2), même échelle (dessus de la carapace 1,95 m, épines dorsales jusqu'à 2,07 m, antennes 2,34 m ; dessous de l'abdomen à 1,16 m : un bean de 1,05 m, 1,10 m avec le tricorne, passe dessous ; envergure des pointes 5,92 m), mêmes sockets `SOCKET_*` / `WeakPoint_*` (14, inchangés), mêmes noms, durées et notifies des 21 clips v1.

**Modèle** (LOD0 107 924 triangles ; LOD1 38 022, LOD2 10 592, LOD3 6 256 ; même UV à tous les LOD) :
- pattes en plaques rigides : coxa, basis-ischion, mérus, carpe, propode, dactyle (6 segments sur les 4 os de chaque patte), sections aplaties à carènes et arêtes, membranes plissées (dégradé de poids entre os), carène dorsale dentelée du mérus (13 épines), épines ventrales et latérales, carpe épineux, propode épineux, dactyles en crochets noirs, touffes de soies ; balanes en grappes, corail et griffures sur les pattes arrière ;
- pinces 22 % plus massives (sockets inchangés), rangées d'épines (mérus, carpe, paume), tubercules, dents irrégulières : molaires bosselées à droite (broyeuse), dents de scie à gauche (coupeuse), plusieurs cassées ;
- carapace hérissée sur le bord frontal (rostre, épines sus-orbitaires, 7 épines antérolatérales par côté, grandes épines latérales de 0,46 m), 10 grandes épines de lobes, une centaine de tubercules épineux, sillons creusés, griffures et fissure, balanes et coraux sur le bord postérieur ; plus d'or ni de perles (registre Tamatoa évité) ;
- yeux composés ambrés à pseudo-pupille, pièces buccales à franges de soies, mandibules dentées.

**Matériaux** (textures cuites, PBR, 2048 px ; yeux 512 px ; normales DirectX) : emplacement `MI_GiantCrab_Chitin` → `MI_GiantCrab_Body` (corps, pinces, tête), `MI_GiantCrab_Legs` → `MI_GiantCrab_Legs`, `MI_GiantCrab_Eye` → `MI_GiantCrab_Eyes` ; Body et Legs héritent de `M_Character_Baked` (drapeau d'usage « morph targets » ajouté et enregistré) ; les yeux de `M_GiantCrab_Eyes` : textures `BaseColor`, `ORM`, `Normal`, `EmissiveMask`, vecteur `EmissiveColor` (ambre 1 / 0,45 / 0,06), scalaire `EmissiveStrength` (2), scalaire `DeadEyes` (0 vivant, 1 œil laiteux et éteint). La frénésie de T-261 (`FrenzyEyeColorParams` = `EmissiveColor`, emplacement `MI_GiantCrab_Eye`) y trouve ses noms. Palette : chitine noir violacé et sang de bœuf, dessus sec et mat, articulations et pointes humides, épines os, membranes rouge sang.

**Clips neufs** (30 i/s, en place, sans root motion ; notifies sur la piste « Gameplay ») :

| Clip | Durée | Boucle | Notifies (image, temps) | Contenu |
|---|---|---|---|---|
| `A_GiantCrab_BellySlam` | 60 i, 2,0 s | non | `AN_Attack_Windup` 12 ; `AN_BellySlam_Impact` 30 (1,0 s) ; `AN_CameraShake` 30 | accroupi, se soulève (+0,35 m, 22-26), s'écrase à plat ventre (30), reste au sol pattes écartées jusqu'à 44, se relève (60) |
| `A_GiantCrab_Roar` | 75 i, 2,5 s | non | `AN_Roar` 22 (0,733 s) ; `AN_CameraShake` 23 | se cabre à 30° sur les pattes arrière, pattes avant levées, pinces grandes ouvertes et écartées, bouche et mandibules ouvertes, tremblement (22-52) |
| `A_GiantCrab_LegStomp` | 45 i, 1,5 s | non | `AN_Stomp_L` 13 (0,433 s) ; `AN_Stomp_R` 27 (0,9 s) | patte avant gauche armée à 0,8 m puis plantée, puis la droite ; points d'impact dans le manifeste (`stomp_points_clip_space_m`) |
| `A_GiantCrab_Charge` | 12 i, 0,4 s | oui | `AN_Step_*` ×8 | course latérale vers la gauche du crabe (+X Blender, comme `Walk_Sideways_L`), 450 cm/s sans glissement, corps 30 cm plus bas |
| `A_GiantCrab_Charge_R` | 12 i, 0,4 s | oui | `AN_Step_*` ×8 | la même vers la droite |

**Pas des pattes** : notifies `AN_Step_leg_<l|r><1..4>_dactyl` (piste « Steps ») au contact de chaque pointe dans `Walk_Sideways_L/R`, `Walk_Forward` (8 par cycle), `Turn_L/R` (24), `Charge`, `Charge_R` (8). Os de pointe : `leg_<s><n>_dactyl`, la pointe est au bout de l'os (84,6 cm pour la paire 1, 92 cm pour les paires 2 et 3, 78,2 cm pour la 4), contact au sol à 0 cm. Le manifeste donne pour **tous** les clips la liste des contacts (`step_contacts` : os, image, temps) et `leg_tips`. Les sockets `SOCKET_LegTip_<S><n>` sont dans le manifeste mais **pas créés** dans Unreal (voir plus bas).

**Retouches des clips v1** (noms, durées et notifies inchangés) : pièces buccales et mandibules plus vivantes ; corps renversé : genoux pliés vers le ciel au lieu de traverser le sable, carapace et grandes épines maintenues au-dessus du sol, antennes rabattues (`Stagger_Flip`, `Flipped_Loop`, `Recover`, `Death_Flipped`) ; pinces retenues à 8 cm du sable au plus (sauf la pince plantée du `Slam`).

**Contrôles** (`build/giant_crab_v2/build_report.json`) : peau sans sommet non pondéré ni somme fausse ; glissement des pointes en appui nul dans les marches et la charge ; boucles exactes ; IK hors portée seulement dans les clips v1 hérités, comme en v1 (`Throw` 7 images, `Stagger_Flip` 3, `Recover` 1) ; enfoncement dans le sable résiduel : pinces qui poussent (`Recover` 26 cm, `Death` 14 cm), une patte de `Death_Flipped` (23 cm), une pointe pendant la bascule de `Stagger_Flip` (18 cm), moins de 9 cm ailleurs.

**Avertissements de l'import** :
- Les 8 sockets `SOCKET_LegTip_*` n'ont pas pu être créés : en UE 5.8, `SocketName` d'un `SkeletalMeshSocket` est en lecture seule depuis Python (même mur que le kit v1). Valeurs prêtes dans le rapport d'import (`to_add_by_hand` : os, décalage déjà divisé par 100, rotation) pour l'éditeur ou le MCP (`ObjectTools`). Le gameplay n'en a pas besoin (notifies, ou os du dactyle).
- Le morph `DeadEyes` n'est pas importé par l'importeur FBX hérité (aucun mesh du projet n'a ses morphs dans Unreal) : le voile reste caché dans l'œil ; l'œil mort passe par le paramètre `DeadEyes` de `M_GiantCrab_Eyes`.
- `SK_GiantCrab` n'a **pas de Physics Asset** (ni avant ni après l'import : l'import v1 n'en a pas créé). Corps proposés par os : `PHAT_GiantCrab_hint.json` (pour T-261, hitbox par corps).
- Les anciens emplacements `MAT_Gold_Aged`, `MAT_Pearl_White`, `MAT_Pearl_Black` restent dans la liste du mesh (Unreal garde la liste au réimport) mais aucune section ne les utilise ; les matériaux v1 (`Meshes/MI_GiantCrab_*`, `MAT_*`, `Materials/*_VC`, `_B`, `Gold`, `Pearl*`) ne servent plus. Distances d'écran des LOD : celles du moteur (`LODInfo` hors de portée de Python).

**À vérifier en PIE (`L_Test_Flat`)** : le crabe en jeu (couleurs, normales, yeux ambrés, LOD au loin) ; marche, virages, attaques et renversement avec la nouvelle géométrie (pattes et épines contre le sol, bean sous le ventre) ; bruits de pas sur `AN_Step_*` ; les clips neufs une fois branchés par T-261 (`BellySlam`, `Roar`, `LegStomp`, `Charge`) ; yeux rouges de la frénésie (`EmissiveColor`) ; `DeadEyes` à la mort si T-261 le pilote.

## Boss v2 « Tupa Nui, Gardien du récif » (T-261, demande du propriétaire du 2026-10-09)
> « ajoute hit box physique du crabe afin que je puisse le taper partout et pas traverser ses textures » ; « ajoute aussi les bruits de pas des pattes du crabe géant » ; « fais en sorte de modéliser le crabe, de le transformer en boss Dark Souls imbattable, avec des phases, modéliser donc ses pattes, rends-le plus rapide, lorsqu'on est sous lui il s'écrase contre le sol, rends le crabe ultra terrifiant »

**Lot** L5 (« danger lisible, états cohérents sur les clients ») ; recette §12 n° 10 (réseau dégradé, pas de désaccord final) et n° 21 (parcours de combat atteignable), RUN13 (4 joueurs). Le modèle, le rig et les clips sont T-260 (section précédente) ; ici le gameplay C++.

**Statut** : **code écrit et compilé fichier par fichier** (UBT `-SingleFile`, 0 erreur, 0 avertissement) le 2026-10-09 ; sons générés et **importés dans l'éditeur** (action `crabboss`, 37 sons, 4 s) ; `DA_GiantCrabRules` réglé pour les propriétés déjà connues de l'éditeur (santé 6000, vitesses, sons simples, tiges des yeux), le reste **en attente du build complet** du lead (voir « À lancer »). **Non testé en PIE, ni en réseau ; spec `monke_island.Creatures.GiantCrab.Boss` écrite et compilée mais pas exécutée** (consigne de la vague : pas de test pendant que le lead tient l'éditeur).

### Hitbox physique (`UGiantCrabBodyComponent`, `Creatures/GiantCrabBody.*`)
- `SK_GiantCrab` n'a pas de Physics Asset (ni v1 ni v2) : 38 **capsules et boîtes créées en C++** sur chaque machine depuis `HitBodies` (valeurs du `PHAT_GiantCrab_hint.json`, identique en v2), attachées aux os : elles suivent l'animation, sans simulation ni tick. Carapace (boîte de `body`), ventre (`abdomen`), 4 yeux et tiges, pinces (bras, main, doigt : `Claw`), coudes (`Joint`), 24 segments de pattes (mérus, propode, dactyle : `Leg`). L'échelle 100 des os du kit est neutralisée (décalage relatif calculé depuis la pose de repos, échelle absolue).
- **Blocage** : requête seule, type d'objet `Pawn`, bloquent `Pawn` : on ne traverse plus pattes, pinces ni carapace ; jamais marchables (`WalkableSlope_Unwalkable`, pas de marche dessus) : personne ne monte sur le crabe. Le sol sous la carapace reste libre entre les pattes : un bean (1,10 m) passe sous le ventre (1,16 m en v2), par l'arrière ou entre les pattes 1-2 et 2-3. La capsule du crabe ignore toujours `Pawn`.
- **Coups** : le sabre trouve les corps (balayage `HitObjectTypes` = Pawn) ; chaque coup prend la **zone du corps touché**, sinon de l'os nommé par le tir (`FindStruckBone`), sinon du corps le plus proche du point (80 cm) : carapace ×0,1, pattes ×0,5, pinces ×0,25 (`ClawDamageScale`), points faibles par os ou socket (yeux ×3, coudes ×2, ventre ×4, bouche ×1,5 ; les tiges des yeux ajoutées aux points faibles des yeux par le script). Journal à chaque coup : `GiantCrab X: blow on the Leg (leg_l2_merus) x0.50 -> 17.5, 5982 left`.
- **Tirs et caméra** : les tirs tracent sur le canal `Camera` (`FirearmRules`/`MusketRules.TraceChannel`), celui de la perche de la caméra à la 3e personne. Pour que **la caméra ne saute pas contre les pattes**, seuls les corps lents (carapace, ventre, yeux, coudes) bloquent ce canal ; pattes et pinces l'ignorent tant que `bLimbsBlockShots` est faux : **les balles ne touchent pas encore les pattes** (voir « Hors périmètre »). La capsule bloque `Camera` comme avant (zone par l'os le plus proche du tir). Mort : les tirs traversent tous les corps.
- Le crabe ne recule plus sous le sabre ni les balles (`KnockbackTakenScale` 0). Console (hors Shipping) : `monke.GiantCrab.ShowBodies 1` dessine les corps des crabes qui apparaissent ensuite.

### Phases (`Phases`, répliquée : `Phase`)
| | Phase 1 (> 66 %) | Phase 2 (≤ 66 %) | Phase 3 (≤ 33 %) frénésie |
|---|---|---|---|
| Entrée | — | rugissement | rugissement + yeux rouges |
| Vitesse déplacement / rotation | ×1 / ×1 | ×1,15 / ×1,2 | ×1,3 / ×1,4 |
| Vitesse des clips de coups (armement plus court) | ×1,15 | ×1,3 | ×1,5 |
| Récupération après un coup | ×0,8 | ×0,6 | ×0,35 |
| Dégâts infligés | ×1 | ×1,2 | ×1,4 |
| Dégâts subis (résistance) / plafond d'un coup | ×1 / 150 | ×0,8 / 120 | ×0,65 / 100 |
| Pinces levées (blocage de face) | 30 % | 35 % | 20 % |
| Piétinement / charge (poids) | non | 1,5 / 1 | 1,5 / 1,5 |
| Combos (chance) | — | 55 % : balayage → écrasement → piétinement → balayage | 75 % : balayage → écrasement → saisie, saisie → saisie (lancer puis ressaisie), piétinement → saisie |
| Écrasement sous la carapace (délai mini) | 4 s | 3 s | 2 s |
- Santé 6000 (`DA_GiantCrabRules`), « quasi imbattable » mais pas invincible : `bInvulnerable` faux par défaut ; invulnérable **pendant le rugissement** seulement. Les phases ne reviennent jamais en arrière ; un gros coup peut en sauter une (un seul rugissement). Renversé (sur le dos), il rugit une fois relevé.
- **Rugissement** (`Roar`, clip `A_GiantCrab_Roar` 2,5 s, sinon le claquement) : son + longue secousse de caméra au début, à `AN_Roar` (0,733 s) une **onde de choc** de 9 m qui repousse tout (1000 cm/s, 10 dégâts) et met en ragdoll les joueurs à moins de 3,5 m.
- **Frénésie** : `EmissiveColor` rouge et `EmissiveStrength` 12 sur `MI_GiantCrab_Eye` (instance dynamique), deux lumières rouges aux sockets `WeakPoint_Eye_*` ; barre de boss qui pulse. À la mort, lumières éteintes et `DeadEyes` = 1 au notify `AN_DeadEyes`.

### Plus rapide
Marche latérale 200 cm/s (100), avant 100 (50), course latérale 480 (260), rotation 140°/s (75), accélération 3000 ; clips de marche jusqu'à ×4,5 leur vitesse (`MaxLocomotionPlayRate`) ; armement des coups raccourci par phase (ci-dessus). Un joueur sprinte à 600 cm/s : en phase 3 le crabe court à 624.

### Écrasement quand on est dessous (`BellySlam`)
Le serveur regarde 10 fois par seconde qui a les pieds sous la carapace (ellipse 110 × 125 cm centrée 25 cm devant le centre, à ±1,2 m de son sol). Une proie qui y reste **0,4 s** (`TriggerSeconds`) le fait s'écraser (sauf pendant un coup, une charge, un rugissement ou sur le dos ; délai mini par phase) : clip `A_GiantCrab_BellySlam` (impact `AN_BellySlam_Impact` à 1,0 s, à plat jusqu'à 1,47 s) — sans lui, l'écrasement de pince et 1,033 s. À l'impact : ceux dessous (empreinte élargie de leur rayon) **60 dégâts et ragdoll** projetés vers l'extérieur ; onde de 4,5 m autour (15 dégâts, recul) ; grosse secousse, son, sable. La détection est tactile : elle ne dépend pas de la vue.

### Charge, piétinement, combos
- **Charge** (phases 2-3, à une proie entre 4,5 et 26 m, toutes les 7 s) : il tourne un flanc vers elle en grattant le sable (0,9 s ÷ vitesse des coups, cri `Charge`), puis **court de côté** comme un crabe (`A_GiantCrab_Charge` / `_Charge_R`, 450 cm/s à ×1, ici 1100 cm/s × phase) à travers la proie jusqu'à 5 m au-delà, sans sortir de l'arène ; ceux qu'il croise (2 m de ses flancs, ±80°) : 45 dégâts, ragdoll et projetés de côté ; puis il reste **1,2 s essoufflé** (ouverture).
- **Piétinement** (`Stomp`, phases 2-3) : quand une proie est à 1,2 m de son bord, sans avoir à lui faire face ; `A_GiantCrab_LegStomp` : onde de 2,3 m (25 dégâts, recul vers le haut) au flanc gauche à `AN_Stomp_L` puis au droit à `AN_Stomp_R` (sans le clip : la réaction aux coups, 0,25 / 0,55 s).
- **Combos** : à la fin d'un coup, selon la chance de la phase, le coup suivant de la chaîne part sans pause si la proie est à portée (×1,4).

### Bruits de pas des pattes (`UGiantCrabFeedbackComponent`)
Sur chaque machine qui joue du son (rien sur un serveur dédié, rien de répliqué) et seulement si une caméra locale est à moins de 45 m : notifies `AN_Step_leg_<l|r><1..4>_dactyl` des clips v2 (marches, virages, charges) ; dans les autres clips, contact de la pointe de chaque dactyle (bout de l'os : 84,6 / 92 / 92 / 78,2 cm, rotation seule) avec le sable par sa hauteur (levée 6 cm, contact 2,5 cm au-dessus de son point le plus bas récent) : lecture d'os, **aucune trace**. Les pattes qui se posent ensemble font **un** pas, lourd si une patte arrière (3 ou 4) en est ; au plus 16 pas/s ; les pas lourds secouent un peu la caméra (une fois toutes les 0,3 s). Sons : 6 tacs de chitine sur le sable, 4 pas lourds (concurrence 6, le plus ancien coupé, atténuation 3 m / 35 m).

### Terrifiant (indices et sons)
Moments joués sur chaque machine à partir de l'état répliqué (mêmes temps que les dégâts du serveur) : écrasement de pince, piétinements, écrasement du ventre, rugissement et son onde, cri de charge, cri de mort ; notifies des clips : claquements de pince (`AN_Claw_Snap`), corps au sol (`AN_BodyFall`, `AN_Land`) ; coups portés (multicast non fiable). Chaque indice : son tiré parmi des variations (atténuation 8 m / 90 m pour la voix et les impacts), secousse des caméras locales selon la distance (`UGiantCrabStepShake`, `UGiantCrabImpactShake`, `UGiantCrabRoarShake`), sable (`NS_Firearm_Impact` agrandi) pour les impacts. Sifflements toutes les 4-8 s quand il chasse (un tout de suite quand il repère quelqu'un), grondements toutes les 8-15 s sinon. 37 sons synthétisés par `scripts/audio/crab_sounds.py` (numpy/scipy, graines fixes, œuvre originale du projet) : `art/audio/creatures/giant_crab/` + `manifest.json`.

### Perception (avec T-262)
`UStealthSubsystem::CanSee(crabe, proie, 30 m)` : accroupi ou de nuit, vu seulement au contact ; accroupi dans un buisson, plus près encore (règle T-207, portée par la furtivité). En plus, **il sent ce qui touche ses pattes** (1,6 m de son bord). Sans proie sentie, `FindHeardNoise` (course, voix, tirs) désigne le fauteur s'il est dans son arène. Une proie perdue de vue est encore chassée 3 s. Les proies viennent du registre `UHostilitySubsystem` (plus d'itération sur tous les personnages). Sans sous-système de furtivité : l'ancienne règle du buisson.

### Barre de boss (`UI/BossHealthBar`, `Creatures/GiantCrabBoss.h`)
`SBossHealthBar` (Slate) dans le HUD TAPAO, en bas au centre au-dessus des balles : nom « Tupa Nui, Gardien du récif » (`BossName`), barre fine de 880 px, morceau pâle qui traîne sur les derniers dégâts, repères des phases à 66 % et 33 %, rouge qui pulse en frénésie, « GRAND ENNEMI ABATTU » au centre à sa mort. Visible pour **chaque joueur local** qui est dans l'arène du crabe (ou à 30 m sans arène) pendant que le crabe se bat (`bEngaged` répliqué : une proie, ou un coup reçu il y a moins de 5 s), et 5 s après sa mort. Les boss s'inscrivent dans `UBossBarSubsystem` (interface `IBossBarSource`) : le HUD ne cherche aucun acteur. La barre au-dessus de la carapace est masquée quand `bBossBar` est vrai.

### Réseau
Serveur autoritaire : décisions, dégâts, phases, ragdolls. Répliqués : `Action` (état, coup, série, sur le dos, vitesse de lecture, heure serveur de départ), `Phase`, `bEngaged`, `HeldPrey` (le saisi), `bRising` (montée hors du sable), la santé (composant). Chaque machine en déduit clips, moments, sons, secousses, yeux et barre. Un seul RPC (multicast non fiable) pour « un coup a porté ». Non testé en hôte + client.

Correctifs de la revue T-268 (2026-10-11, `Creatures/GiantCrabNet.cpp`, règles pures `GiantCrabLag.h`) :
- **Saisie (T-304, R-23 et R-33).**
  - Le saisi est répliqué. La machine du joueur saisi coupe elle-même son mouvement et tient son pion sous la pince qu'elle voit. Le serveur ne corrige pas les mouvements qu'elle envoie pendant la prise (`bIgnoreClientMovementErrorChecksAndCorrection`, rendu au lâcher).
  - Le saisi est lâché à `EndPlay`, à la mort du crabe et à toute interruption. Il l'est aussi quand il meurt ou tombe KO pendant la prise.
  - Un mort, ou un joueur déjà KO, garde son ragdoll : son mouvement n'est jamais remis en chute.
- **Clips sur l'heure serveur (T-305, R-24 et R-32).** `Action.StartServerTime` : un client qui reçoit une action en retard (perte, retour de pertinence, arrivée en cours) joue le clip et les moments à partir de leur âge. Un crabe mort revu plus tard est couché, sans cri de mort.
- **Corps de frappe (T-305, R-24).**
  - Les corps animés restent pour les coups, les tirs et les autres personnages. Le mouvement prédit des islanders les ignore (filtre de masque `AIslanderCharacter::AnimatedHitBodyMaskFilter`).
  - Des formes fixes, posées sur la racine à la place des corps au repos (×`PawnBlockerScale`, 0,8), arrêtent les pions de la même façon sur toutes les machines. Ce sont des objets WorldDynamic, avec le masque `PawnBlockerMaskFilter`.
  - Ces formes sont coupées pendant la charge (vitesse qu'aucune prédiction ne suit) et pendant la montée hors du sable (R-31, `bRising` répliqué).
  - En solo (`NM_Standalone`), rien ne change : ni masque ni formes fixes, ce sont les corps animés qui arrêtent le joueur, comme avant.
- **Points faibles (T-305, R-25).**
  - Le serveur garde 0,3 s d'historique des sockets des points faibles (`WeakPointSampleSeconds`, 0,033 s).
  - Un tir ou un coup d'un joueur distant qui manque un point faible ici est rejugé là où ce joueur le voyait. Le recul vaut l'aller-retour + `LagViewDelaySeconds`, à ±`LagWindowSeconds`, borné par `LagCompensationMaxSeconds`. La ligne du tir s'arrête un peu après l'impact, jamais à travers tout le corps.
  - La fenêtre du coude coincé après l'écrasement compte aussi en retard pour un joueur distant. Hôte : comportement inchangé. Journal : `weak point <socket> as seen <n> ms ago`.
  - Spec `monke_island.Creatures.GiantCrab.Net`.
- **Reste hors de ce dépôt de code.** Un tir qui ne touche rien du crabe côté serveur n'est pas compensé : il faudrait une compensation côté arme (R-48). Le second regard des armes (`FirearmDamage::TraceHitBodies`) peut ignorer les formes fixes avec le masque `UGiantCrabBodyComponent::PawnBlockerMaskFilter`.

### Fichiers
- C++ : `Creatures/GiantCrab.h/.cpp` (le chasseur), `GiantCrabMoves.cpp` (phases, rugissement, charge, piétinement, écrasement, moments, dégâts, barre), `GiantCrabNet.cpp` (saisi, montée, formes fixes, âge des actions, latence des points faibles), `GiantCrabTypes.h` (états, coups, zones, indices), `GiantCrabRules.h/.cpp` (toutes les valeurs), `GiantCrabMath.h` et `GiantCrabLag.h` (règles pures), `GiantCrabInternal.h`, `GiantCrabBody.*`, `GiantCrabFeedback.*`, `GiantCrabCameraShake.*`, `GiantCrabBoss.*`, `UI/BossHealthBar.*`, accroche dans `UI/Tapao/STapaoHUD.cpp` ; tests `Creatures/Tests/GiantCrabBossSpec.cpp`, `GiantCrabNetSpec.cpp`.
- Sons : `scripts/audio/crab_sounds.py` → `art/audio/creatures/giant_crab/` ; éditeur : `scripts/editor/crab_boss_setup_ue.py` (action `crabboss`, rejouable, rapport `monke_island/Saved/T261/crab_boss_setup.json`) → `/Game/MonkeIsland/Characters/GiantCrab/Audio/` (`SW_GiantCrab_*`, `ATT_GiantCrab_Near/Far`, `CON_GiantCrab_Steps/Voice`).

### À lancer après le build complet (lead)
1. Action `crabboss` : branche les sons des 14 indices, le sable, les clips v2 (`BellySlam`, `Roar`, `LegStomp`, `Charge`, `Charge_R`) dans `DA_GiantCrabRules` (propriétés absentes avant le build : listées `pending` dans le rapport du 2026-10-09).
2. Tests : `monke_island.Creatures.GiantCrab` (16 tests T-211 inchangés + 19 tests `Boss` : seuils et transitions de phase, valeurs des phases, empreinte sous la carapace et délai de 0,4 s, zones de chaque os et de chaque corps, dégâts plafonnés puis multipliés, pas par hauteur et par notifies, vitesse des clips, flanc de la charge, fin de charge, secousses).

### Hors périmètre (à décider)
- **Armes** : pour que les balles touchent pattes et pinces sans que la caméra saute, ajouter un canal de trace dédié aux tirs (`DefaultEngine.ini` : `+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,DefaultResponse=ECR_Block,bTraceType=True,bStaticObject=False,Name="WeaponTrace")`), passer `TraceChannel` de `FirearmRules.h` et `MusketRules.h` (et des DA d'armes) à `ECC_GameTraceChannel2`, puis dans `DA_GiantCrabRules` `ShotTraceChannel` = ce canal et `bLimbsBlockShots` = vrai. Le sabre n'a besoin de rien.
- **Pas génériques** : `UFootstepSubsystem` joue aussi ses pas de créature pour le crabe ; une ligne dans sa boucle (`if (Character->Tags.Contains(TEXT("NoGenericFootsteps"))) continue;`) les coupe (le crabe porte déjà ce tag).
- `DA_GiantCrabRules_Sand` (crabes qui sortent du sable) hérite des défauts de classe : phases, barre de boss et nom « Tupa Nui » compris, avec ses 1500 PV × difficulté. À arbitrer (décocher `bBossBar` ou renommer).

### À vérifier en PIE (`L_Test_Flat`)
Pattes et carapace bloquent le joueur, le passage sous le ventre reste possible ; `monke.GiantCrab.ShowBodies 1` avant d'entrer en jeu pour voir les corps collés aux os ; un coup de sabre sur une patte, une pince, l'œil (journal de zone) ; la caméra 3e personne ne saute pas contre les pattes ; rester sous lui → écrasement en 0,4 s + ragdoll ; phase 2 à 66 % (rugissement invulnérable, onde, secousse), charges latérales, piétinements, combos ; phase 3 à 33 % (yeux rouges, lumière), barre de boss, « GRAND ENNEMI ABATTU », yeux laiteux ; pas des pattes (un tac par groupe de pattes, plus lourd à l'arrière), sons de rugissement, sifflements ; un client : mêmes clips, sons et barre. Mesurer le coût des 38 corps (stat `game`) avec 2 crabes.

### Intégration lead (2026-10-09)
- **Balles sur les pattes et les pinces** : les armes tracent toujours sur le canal de la caméra (aucun changement de canal global, pas de risque pour les autres cibles), puis `FirearmDamage::TraceHitBodies` regarde une seconde fois le long du tir sur le canal Pawn : si le premier corps bloquant est un corps de crabe (tag `WeaponHitBody`, posé sur les 38 corps) plus proche que le coup du tir, c'est lui qui est touché ; l'os (et donc la zone) vient ensuite de `FindStruckBone` comme pour les autres créatures. Mousquet historique (`AMusketWeapon`) et armes à feu v2 (`AFirearmWeapon`, chaque plomb) ; compilé fichier par fichier.
- **Pas en double** : `UFootstepSubsystem` saute les personnages tagués `NoGenericFootsteps` (le crabe joue ses propres pas de pattes).
- **Crabes du sable** (`DA_GiantCrabRules_Sand`) : gardés comme boss (phases, barre « Tupa Nui », 1 500 PV).
