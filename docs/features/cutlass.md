# Sabre de Bean (T-070)

**Lot :** L5, sur demande de l'utilisateur du 2026-10-03. Comme le mousquet (T-069), c'est un écart au cahier §5, à arbitrer dans l'ADR-001.

**Maquettes :** produites sur le 2e PC, dans `OneDrive\maquette-blender\maquettes\` :
- `bean-cutlass` v0.1.0 (validated) : mesh et bloc `gameplay` ;
- `bean-scabbard` v0.1.0 (validated) : fourreau à pendant pivotant ;
- `bean-blade-test` v0.1.0 (validated) : placements de la lame validés pour chaque coup ;
- `bean-cutlass-anim` v0.1.0 (built) : timeline du combat.

Les copies sont dans `art/export/Weapons/Cutlass/`.

## Règles (DA_CutlassRules, valeurs de la maquette)
| Valeur | Défaut |
|---|---|
| Endurance par coup | 8 % du maximum, soit 12 coups par barre ; 11 % avant T-152. Aucune attaque en dessous |
| Endurance par parade encaissée | 11 % (`ParryStaminaCostFraction`) |
| Taille droite / taille revers / estoc / coup chargé | 35 / 35 / 45 / 70 |
| Détection | balayage de la lame pendant les images actives (voir « Fluidité »). Filet de sécurité à l'instant de la frappe : arc de 110° pour les tailles, 40° pour l'estoc, 60° pour le coup chargé, portée de 100 cm depuis le centre de Bean (85 avant T-152) plus le rayon de la cible |
| Combo | taille droite → revers → estoc ; un clic dans les 0,45 s après la fin d'un coup enchaîne |
| Coup chargé | clic gauche maintenu 0,4 s |
| Parade (clic droit maintenu) | annule 80 % d'un coup de face et coûte 11 % d'endurance ; sans endurance, le coup passe entier |
| Fil | −1 % par touche ; sous 50 %, dégâts ×0,8 |
| Dégainer / rengainer | 0,5 s / 0,6 s |
| Tir ami | désactivé |

Un Coco Warrior ou un villageois (80 PV) tombe en 2 tailles + 1 estoc, ou en 1 taille + 1 coup chargé.

## Fluidité (T-152, lot L5)
Toutes les valeurs sont dans DA_CutlassRules. Les défauts C++ sont entre parenthèses.

- **Images actives.** Chaque coup a une fenêtre `ActiveStartTime` → `ActiveEndTime` placée autour de sa frappe, là où la lame passe devant Bean dans `CutlassPoses.inl` :
  - taille droite 0,17–0,32 s ;
  - revers 0,14–0,29 s ;
  - estoc 0,16–0,28 s ;
  - coup chargé 0,49–0,62 s.
- **Récupération plus courte.** Après la dernière image active, la piste est jouée plus vite (`RecoveryTimeScale` 0,7, soit −30 %). La taille droite finit à 0,474 s au lieu de 0,54 s, le coup chargé à 0,93 s au lieu de 1,06 s.
- **Fenêtres d'annulation.**
  - Le coup suivant peut partir `AttackCancelDelaySeconds` (0,08 s) après la dernière image active, soit 0,40 s pour la taille droite, contre 0,54 s avant.
  - Une parade, un saut ou un pas annulent la fin du coup dès la dernière image active (`DefensiveCancelDelaySeconds` 0 s).
  - Une parade demandée pendant les images actives attend la fenêtre : on ne pare plus au milieu de son propre coup.
  - Le jeu n'a pas d'esquive. Le saut et le déplacement en tiennent lieu.
- **Tampon d'entrée.** Un clic pendant un coup est gardé `InputBufferSeconds` (0,25 s) et part dès l'ouverture de la fenêtre. Un clic arrivé plus tôt est oublié. Le coup chargé passe aussi par le tampon.
- **Mouvement.** Du début du coup à la dernière image active, la vitesse de marche est multipliée par `AttackMoveSpeedMultiplier` (0,45). Ensuite, elle revient à la normale.
- **Lunge.** Au début de chaque taille, Bean avance de `LungeDistanceCm` (30 cm) en `LungeSeconds` (0,12 s). C'est une root motion source additive, appliquée par le porteur et par le serveur.
  - Pas de lunge si un ennemi devant est à moins de `LungeBlockDistanceCm` (110 cm, de centre à centre).
  - Le lunge ne rapproche jamais Bean à moins de cette distance.
  - L'estoc et le coup chargé n'ont pas de lunge.
- **Direction du coup.**
  - Au début du coup, Bean se tourne vers sa visée (la caméra) à `BodyTurnRateDegPerSec` (1080°/s) jusqu'à la dernière image active. En 3e personne, c'est le corps qui tourne.
  - `bFaceAimOnAttack` = faux : Bean frappe dans la direction où il fait face.
- **Aide à la visée** (`bAimAssist`).
  - Au début du coup, la direction passe sur l'ennemi vivant le plus proche dans un cône de 60° (`AimAssistConeDegrees`) et à moins de 2,5 m (`AimAssistRangeCm`). Les joueurs ne sont jamais ciblés.
  - En 1re personne, la caméra tourne vers l'ennemi à 360°/s au plus (`AimAssistTurnRateDegPerSec`), puis la souris retrouve la main.
- **Balayage de la lame** (serveur).
  - À chaque image du serveur pendant les images actives, `BladeSamples` (3) sphères de `BladeSweepRadiusCm` (15 cm) sont balayées le long de la lame, de la garde (6,2 cm) à la pointe (43,1 cm, sockets `guard` et `tip` de la maquette).
  - Chaque sphère va de la position précédente à la position actuelle de la piste, par sous-pas de `BladeSubstepSeconds` (1/60 s) au plus.
  - Le repère est le composant du pantin, sans le balancement du bassin.
  - L'arc ci-dessus reste un filet de sécurité à l'instant de la frappe.
  - Chaque cible n'est touchée qu'une fois par coup. Le coup en cours finit son balayage avant que le suivant ne commence : `GetLocalMove()` reste le coup qui frappe pour les réactions des tikis (T-150).
- **Retour d'impact** (« juice »), sur la machine du porteur seulement et à la confirmation du serveur.
  - Hit-stop : la piste de la lame (et le clip du corps qui la suit) se fige 0,045 s, ou 0,06 s pour un coup mortel (`HitStopSeconds`, `KillHitStopSeconds`).
  - Les ennemis touchés passent à `CustomTimeDilation` 0,05 pendant ce temps. Ce n'est jamais une pause globale.
  - Le pion du porteur n'est pas ralenti : son mouvement est prédit, le ralentir provoquerait des corrections.
  - Secousse caméra `UCutlassHitCameraShake` (`HitCameraShake`), échelle 1 sur une touche, 1,6 sur une mort.
  - Les sons d'impact restent les `SW_Cutlass_Hit` / `Kill` existants, multicast.

## Vue 1re personne (T-239, demande du propriétaire du 2026-10-08)
**Statut : compilé le 2026-10-08 (lead), `monke_island.Weapons.Cutlass` 14 tests verts (dont les 2 de la vue FP). Testé éditeur** : garde du sabre visible à la 1re personne dans Test Flat (capture 157, bras FP activés pour le sabre dans `BeanAnimDriverComponent`) ; coups, parade et balancement de course à juger par le propriétaire dans le build v1.5.

### Principe
- Le sabre et les bras FP ont leurs **propres clés en espace caméra** (X avant, Y droite, Z haut, cm ; lame le long de +X, fil vers −Z à roll 0), dans `DA_CutlassRules` : `FirstPersonGuardKey`, `FirstPersonParryKey`, `FirstPersonDrawKeys`, `FirstPersonSheatheKeys`, et `ViewKeys` dans chaque attaque (`SlashRight`, `SlashLeft`, `Thrust`, `Finisher`). Même axe de temps que les clés corps : **les fenêtres de dégâts, les `HitTime` et les images actives de T-152 sont inchangés**, le serveur balaie toujours les clés corps.
- Chaque coup est **un seul segment lissé** (smoothstep) de part et d'autre de sa frappe : la lame est au plus vite et **au centre de la vue à `HitTime`**. Taille droite : armée au bord droit, balaie jusqu'au bord gauche, fil en avant. Revers : la main croise au bord gauche, la lame balaie vers la droite. Estoc : poignée ramenée à la hanche, pointe poussée au centre à 93 cm. Coup long : la charge du clic tenu monte la lame au-dessus de la vue (c'est le début de sa piste), la tenue continue de la monter lentement jusqu'à 0,483 s, puis la lame **traverse toute la vue de haut en bas** (frappe à 0,57 s, milieu de la chute), reste basse (0,82 s) et revient.
- Au départ d'un coup, la piste est fondue depuis la pose affichée sur `ViewMoveBlendInSeconds` : le coup long part de la charge sans retomber, un coup tamponné part de la récupération du précédent sans saut (les coups depuis la garde ne sont pas affectés, leur piste y commence).
- La garde est en bas à droite (poignée à (30, 17, −15) cm, pointe vers le centre-droit), avec une **respiration** (sinus lent) au repos et en marche. Hors coup, la pose se mélange vers la garde / parade / charge à `PoseBlendSpeed`.
- **Parade** (clic droit tenu) : lame levée en travers de la vue (poignée en bas à droite, pointe qui sort en haut à gauche) ; la **main gauche** vient sur le plat de la lame (`bParryLeftHand`, point à `ParryLeftHandAlongBladeCm` de la poignée, `ParryLeftHandOffset`), mélangée à `LeftHandBlendSpeed`.
- **Coup bloqué** : `ACutlassWeapon::ModifyIncomingDamage` (serveur, via `UIslanderHealthComponent` → inventaire) appelle `PlayBlockFeel` sur l'hôte ou le RPC client `ClientParryBlocked` : recul de tout l'ensemble vers le visage (`BlockRecoilLocation` / `BlockRecoilRotation`, enveloppe sinus sur `BlockRecoilSeconds`) et secousse `BlockCameraShake` (`UCutlassHitCameraShake`, échelle `BlockShakeScale`). Le son Parry reste le multicast existant.
- **Course** : bob de l'ensemble sabre + bras selon la vitesse au sol : phase de foulée qui avance avec la vitesse (`BobFrequencyHz` à `BobReferenceSpeed`), poids = vitesse / vitesse de référence (0 en l'air, dans l'eau ou à l'arrêt) lissé à `BobDampingSpeed` ; un creux par pas (`BobVerticalCm`), un balancement et un roulis par foulée (`BobLateralCm`, `BobRollDegrees`), réduit à `BobAttackScale` pendant un coup.
- **Near clip** (5 cm dans le projet) : `CutlassMath::KeepAheadOfNearPlane` pousse la pose vers l'avant si un point du sabre (pommeau `FirstPersonHiltCm` → pointe `BladeTipCm`) passe sous `FirstPersonNearClipCm`. Les pistes par défaut restent à ≥ 10 cm sans correction (vérifié par la spec, échantillonnage à 120 Hz).
- **Bras** : `MusketMath::SolveArm` (bras caoutchouc depuis `FirstPersonShoulder` / `FirstPersonLeftShoulder`, moufle à `FirstPersonArmHandPoint`, étirement ≤ `FirstPersonArmMaxStretch`), comme le mousquet ; la main droite suit la poignée après balancement.
- Le rengainé se joue aussi en FP (la lame descend vers la hanche, hors de la vue) ; avant T-239, la lame disparaissait d'un coup.
- **Mode « bras du corps »** : `UBeanAnimDriverComponent::bBodyArmsInFirstPerson` (vrai par défaut) montre les bras animés du corps avec le sabre en main et **masque cette vue FP** (`ShowsBodyArmsInFirstPerson`, lu par le sabre). `ACutlassWeapon::UsesFirstPersonViewModel()` (`bFirstPersonViewModel`, vrai par défaut) est exposé pour que le driver laisse les bras FP au sabre : il reste à changer la ligne 444 de `BeanAnimDriverComponent.cpp` (`InHand->IsA<ACutlassWeapon>()` → sabre sans view model), hors du périmètre de T-239 ; à défaut, décocher `Body Arms In First Person` sur le composant `BeanAnimDriver` de `BP_IslanderBean`. Le sabre journalise une fois « first person view model not shown » quand le driver le masque.

### Valeurs (DA_CutlassRules, catégorie First Person ; défauts C++)
| Valeur | Défaut |
|---|---|
| `bFirstPersonViewModel` | vrai |
| `FirstPersonGuardKey` | (30, 17, −15) cm, (P 24, Y −6, R 35)° |
| `FirstPersonParryKey` | (27, 13, −3) cm, (P 38, Y −76, R −30)° |
| `SlashRight.ViewKeys` | 0 garde · 0,15 (22, 22, −8) (30, 72, 90) · 0,33 (30, −16, −10) (−8, −72, 90) · 0,42 (31, 8, −14) (12, −30, 60) · 0,54 garde |
| `SlashLeft.ViewKeys` | 0 garde · 0,11 (22, −14, −4) (28, −74, −90) · 0,29 (30, 22, −12) (−6, 70, −90) · 0,39 (31, 20, −14) (14, 14, 0) · 0,51 garde |
| `Thrust.ViewKeys` | 0 garde · 0,12 (23, 16, −11) (8, −12, 0) · 0,24 (50, 5, −5) (3, −3, 0) · 0,44 garde |
| `Finisher.ViewKeys` | 0 (22, 10, 13) (72, 12, 0) armé · 0,483 (20, 10, 15) (78, 14, −4) · 0,66 (30, 8, −17) (−58, −4, 0) · 0,82 (29, 11, −16) (−44, −8, 20) · 1,06 garde |
| `ViewMoveBlendInSeconds` | 0,12 s |
| `FirstPersonDrawKeys` | 0 (16, −14, −34) (40, −30, 0) · 0,18 (20, −8, −22) (55, −35, 0) · 0,42 (27, 4, −2) (50, −20, 40) · 0,58 garde |
| `FirstPersonSheatheKeys` | 0 garde · 0,20 (26, 2, −4) (48, −24, 20) · 0,44 (19, −9, −24) (50, −34, 0) · 0,64 (16, −14, −34) (40, −30, 0) |
| `FirstPersonShoulder` / `FirstPersonLeftShoulder` | (−6, 18, −24) / (−6, −18, −24) cm |
| `FirstPersonArmHandPoint` / `FirstPersonArmMaxStretch` | (36, 0, −1,5) cm / 1,6 |
| `FirstPersonNearClipCm` / `FirstPersonHiltCm` | 8 / 9 cm |
| Respiration `BreathFrequencyHz` / `BreathAmplitudeCm` / `BreathPitchDegrees` | 0,22 Hz / 0,5 cm / 0,5° |
| Course `BobReferenceSpeed` / `BobFrequencyHz` | 600 cm/s / 1,8 foulées/s |
| Course `BobVerticalCm` / `BobLateralCm` / `BobRollDegrees` | 1,4 cm / 0,9 cm / 1,5° |
| Course `BobDampingSpeed` / `BobAttackScale` | 6 /s / 0,35 |
| Parade `bParryLeftHand` / `ParryLeftHandAlongBladeCm` / `ParryLeftHandOffset` / `LeftHandBlendSpeed` | vrai / 16 cm / (−3, 0, −1) cm / 12 /s |
| Blocage `BlockRecoilSeconds` / `BlockRecoilLocation` / `BlockRecoilRotation` | 0,2 s / (−5, 1,5, 2,5) cm / (P 8, Y 6, R −10)° |
| Blocage `BlockCameraShake` / `BlockShakeScale` | `UCutlassHitCameraShake` / 0,5 |

Propriétés retirées des règles : `FirstPersonOffset` et `FirstPersonArmLength` (leurs valeurs sauvegardées dans `DA_CutlassRules` sont ignorées au chargement). Les nouvelles propriétés prennent les défauts C++ tant que l'asset ne les enregistre pas.

### Vérifications PIE (à faire par le propriétaire)
1. **Mode d'affichage** : au PIE en 1re personne avec le sabre (touche **2**), si le journal dit « first person view model not shown », appliquer le changement de `BeanAnimDriverComponent.cpp:444` ou décocher `Body Arms In First Person` dans `BP_IslanderBean`.
2. **Garde** : sabre en bas à droite, lame vers le centre-droit, respiration lente visible à l'arrêt ; en marche le bob est léger, en sprint (Maj) net, amorti en moins d'une seconde à l'arrêt ; aucun bob en saut ni dans l'eau.
3. **Coups** (clic gauche ×3) : taille droite de droite à gauche, revers de gauche à droite, estoc vers le centre ; la lame traverse le centre à chaque frappe ; pas de trou de near clip sur la poignée.
4. **Coup long** (clic gauche tenu 0,4 s) : la charge monte la lame au-dessus de la vue ; au départ, armé lent, descente qui traverse toute la vue, suivi bas puis retour.
5. **Parade** (clic droit tenu) : lame en travers de la vue, main gauche visible sur le plat ; relâcher ramène la garde et la main gauche hors vue. Se faire frapper de face par un tiki : recul de la lame, secousse légère, son Parry ; de dos ou sans endurance : rien.
6. **Dégainé / rengainé** (2) : la lame monte depuis la hanche gauche avec un petit moulinet ; rengainer la fait redescendre hors de la vue.
7. **Hit-stop** : à une touche, la lame se fige 0,045 s comme avant.
8. Hôte + client : le recul de parade arrive sur le client (RPC) ; les autres joueurs voient la pose corps inchangée.

## Commandes
- **2** : dégaine le sabre depuis le fourreau à la hanche gauche. Appuyer de nouveau sur **2** le rengaine, et l'on repasse aux mains nues.
- **Clic gauche** : combo. **Clic gauche maintenu** : coup chargé. **Clic droit maintenu** : parade.
- Le contexte `IMC_Cutlass` a la priorité 2. Il est retiré dans plusieurs cas : objet porté, nage, chute ragdoll, mort.

## Architecture
- Fichiers dans `Source/monke_island/Weapons/` :
  - `ACutlassWeapon` : l'arme, un `AInventoryItemActor` ;
  - `UCutlassRules` : les règles ;
  - `CutlassMath` : la logique pure testée ;
  - `CutlassPoses.inl` : les poses, générées par `scripts/props/cutlass_poses.py` depuis `blade-report.json` et `timeline.json`.
- **Serveur autoritaire :**
  - le client envoie « coup » (combo ou coup chargé) ;
  - le serveur vérifie l'état, l'endurance, l'enchaînement du combo et la durée de charge ;
  - pendant les images actives, il balaie la lame le long de sa piste, et à l'instant de la frappe il contrôle aussi l'arc devant Bean (T-152) ;
  - il applique `ApplyPointDamage` (PvE : tikis), avec recul et impulsion ;
  - le porteur prédit son coup ; un refus du serveur le ramène en garde.
- **Parade :** `AInventoryItemActor::ModifyIncomingDamage`. Elle est appelée par `UIslanderHealthComponent::HandleTakeAnyDamage` à travers `UIslanderInventoryComponent`.
- **Poses :**
  - la poignée est clé en espace du mesh Bean (+Y avant, −X droite, cm) et suit le bassin du pantin ;
  - **3e personne :** la lame est attachée au pantin. Le bras droit vise la clé par IK à deux os (`upperarm_r`, `lowerarm_r`, `hand_r`), puis la lame est placée là où se trouve réellement la moufle, à `GripReachCm` le long de l'avant-bras. Elle reste donc en main même quand la clé est hors de portée du bras court du Bean. Le 2026-10-03, le défaut `grip_r` désignait un os absent : l'IK ne tournait pas et la lame flottait ;
  - **1re personne :** depuis T-239, des clés propres en espace caméra (voir « Vue 1re personne ») ; avant, les clés corps passaient dans le repère caméra avec un simple décalage ;
  - le fourreau pivote sur son pendant pendant le dégainé.
- **HUD :** « Sabre / Parade · fil N % », la jauge de charge du coup chargé et les dégâts flottants (« −35 », « −70 TRANCHÉ ! ») pour l'auteur du coup.

## Sons
Les sons sont synthétisés par `scripts/props/cutlass_sounds.py` dans `art/audio/weapons/SW_Cutlass_*.wav`, sans enregistrement ni licence. `scripts/props/import_cutlass_sounds.py` les importe par commandlet dans `Cutlass/Audio/`, avec `ATT_Cutlass` (net à 3 m, éteint à 25 m).

| Son | Quand | Joué sur |
|---|---|---|
| Swing / FinisherSwing | début d'un coup | chaque machine (`PlayMove`) |
| Hit / Kill | touche ou mort d'un ennemi | tous (`MulticastBladeSound`, serveur) |
| Parry | coup paré | tous (multicast) |
| Draw / Sheathe | dégainer / rengainer | chaque machine |

Chaque son reçoit une hauteur aléatoire de ±`SoundPitchJitter` (8 %).

## Assets Unreal (`/Game/MonkeIsland/Weapons/`)
- `Cutlass/` : `SM_Bean_Cutlass`, `SM_Bean_Scabbard`, `DA_CutlassRules`, `DA_Item_Cutlass`, `BP_Cutlass`.
- `Input/` : `IA_CutlassAttack`, `IA_CutlassParry`, `IMC_Cutlass`.
- `BP_IslanderBean` : le composant `Inventory` a `StartingItems[1]` = `DA_Item_Cutlass`.

## Armes de mêlée des mobs (T-161, lot L5)
Un guerrier tiki tué lâche son arme en pickup (`BP_Pickup_<Arme>`, voir `characters-tiki.md`, « Armes lâchées ») : invite « Ramasser <nom> » (F), puis l'arme va dans un emplacement d'inventaire et se joue avec les commandes du sabre (clic gauche : combo ; maintenu : coup long ; clic droit maintenu : parade ; touche de l'emplacement : sortir / ranger). Chaque arme est un `ACutlassWeapon` avec ses propres règles : `BP_MobWeapon_<Arme>` est un enfant de `BP_Cutlass` (mêmes touches) dont `Rules` = `DA_<Arme>Rules`, une copie de `DA_CutlassRules` (mêmes pistes de coups 3e et 1re personne, même ressenti) avec le mesh, les dégâts, la portée et les sons de l'arme.

**Nouvelles règles de `UCutlassRules`** (défauts = le sabre inchangé) :
- `MeshInGrip` (catégorie `Meshes`) : placement du mesh sur la poignée (lame le long de +X). Identité pour le sabre ; les `SM_<Mob>_<Arme>` sont en espace de prise, axe long sur +Z : tangage −90.
- `bShowWhenPutAway` : rangée, l'arme reste visible sur le Bean (le sabre au fourreau). Faux pour les armes des mobs (pas de fourreau) : elles disparaissent une fois rangées (`CutlassMath::ShowsThirdPersonMesh`).
- `bEdged` (catégorie `Attacks`) : l'arme a un tranchant. Faux : aucun coup ne coupe de membre (quel que soit `bCut`), un mob tué meurt simplement (`TikiCombat::EWeapon::Blunt` : pas de réaction de sabre ni de démo cuite de T-168), mais il saigne à l'impact ; toute mise à mort compte comme telle (son `Kill`, « − N » de mise à mort, `CutlassMath::IsKillBlow`).

**Valeurs** (`DA_<Arme>Rules`, écrites par `scripts/editor/mob_weapons_setup_ue.py` ; sabre : 35 / 35 / 45 / 70, portée 100 cm, 8 % d'endurance par coup ; un Guerrier Coco a 80 PV) :

| Arme (mob) | Tranchant | Taille droite / revers / estoc / coup long | Portée | Endurance par coup | Recul | Masse au sol, valeur |
|---|---|---|---|---|---|---|
| Lance à dents de pierre (CocoWarrior) | non | 28 / 28 / 50 (fente 45 cm, arc 30°) / 60 | 150 cm | 8 % | 220 cm/s | celles de `BP_Drop_Spear` |
| Massue d'obsidienne (AshWatcher) | oui : tailles et coup long coupent, l'obsidienne s'ébrèche (2 % de fil par coup) | 38 / 38 / 25 / 75 | 115 cm | 10 % | 260 cm/s | celles de `BP_Drop_ObsidianClub` |
| Marteau-coquillage (CrabGuardian) | non | 40 / 40 / 22 / 85 | 105 cm | 11 % | 320 cm/s | celles de `BP_Drop_ShellHammer` |

- Lame balayée (`BladeStartCm` → `BladeTipCm`) mesurée sur le mesh : de la tête (35 à 45 % de la longueur depuis le bout) au bout ; rayon 14 à 18 cm ; `FirstPersonHiltCm` = partie derrière la poignée + 2 cm. Parade : 60 % (lance), 70 % (massue), 50 % (marteau).
- **Sons** synthétisés (bois et pierre, pas d'acier) par `scripts/props/mob_weapon_sounds.py` → `art/audio/weapons/SW_MobWeapon_*.wav` (Swing, HeavySwing, Hit, Kill, Parry, Draw, Sheathe), communs aux trois armes, classe `SC_Effects`, atténuation `ATT_Cutlass`.
- **Icônes** : rendues depuis les meshes du sol par `scripts/ui/render_mob_weapon_icons.py` (Blender en arrière-plan, couleurs de la palette tiki) puis `finish_mob_weapon_icons.py` (ombre douce) → `art/ui/icons/mob_weapons/T_Icon_<Arme>.png`.
- **Assets** (`/Game/MonkeIsland/Weapons/MobWeapons/`) : `DA_<Arme>Rules`, `BP_MobWeapon_<Arme>`, `DA_Item_<Arme>` (nom, nom court, icône, masse du `BP_Drop_*`, `PickupClass`), `BP_Pickup_<Arme>` (après le build), `Audio/`, `Icons/`. Action `mobweapons` de la session du lead, idempotente : avant le build, elle saute ce qui demande la classe `MobWeaponPickup` ou les nouvelles propriétés et le liste (`needs_build` de `Saved/T161/mob-weapons.json`) ; la relancer après le build.
- Tests : `monke_island.Weapons.MobMelee` (règles du sabre inchangées, coupe et mise à mort selon le tranchant, mort simple sans démo pour une arme sans tranchant, mesh posé sur la poignée, arme rangée cachée, pickup qui ne disparaît pas, fragment d'arme caché dans la démo).
- **État (2026-10-10)** : C++ compilé fichier par fichier (UBT `-SingleFile` : `CutlassWeapon.cpp`, `MobWeaponPickup.cpp`, `TikiCreature.cpp`, `TikiShredDemo.cpp`, `Tests/MobMeleeSpec.cpp`, specs voisines), pas encore de build complet, tests non lancés ; action `mobweapons` passée avant le build (7 sons, 3 icônes, `DA_SpearRules` / `DA_ObsidianClubRules` / `DA_ShellHammerRules`, 3 `BP_MobWeapon_*`, 3 `DA_Item_*` ; emprises mesurées : lance −30 → 108 cm, massue −22 → 78 cm, marteau −20 → 58 cm depuis la poignée). Restent après le build (même action) : `BP_Pickup_*`, `MeshInGrip` / `bEdged` / `bShowWhenPutAway`, `DroppedWeaponClass` des `BP_Tiki*`. **Non testé en PIE.**
- **Limites** : le Bean joue les animations du sabre (pas de clips de lance) et la lance d'un mètre dépasse largement du champ en 1re personne (pistes du sabre) ; le HUD affiche encore « Sabre · fil N % » et l'info-bulle « Fil » (code UI hors de T-161) ; une arme ramassée puis relâchée retrouve sa pleine valeur ; la lance est l'ancien modèle stylisé (le Coco réaliste du 2026-10-08 tient un nouveau `SK_CocoWarrior_Spear` sans version statique).

## Reste à faire
- **Animations du corps exportées :** `bean-cutlass-anim` n'a pas d'export FBX, contrairement à `bean-musket-anim`. Il faut demander au 2e PC `A_Bean_Cutlass_*.fbx`, la torsion du buste et `fp_poses.json`. Le sabre suit pour l'instant les clés de poignée validées, sans torsion du buste.
- **Mob tranché en deux :** la maquette prévoit une découpe du mesh et deux moitiés physiques. Il faut le module `ProceduralMeshComponent` dans `Build.cs` : Smart App Control le bloquait, il est désactivé depuis le 2026-10-03. Autre voie : une variante par matériau de découpe. La session 78 fait les cadavres coupés en deux et les bras coupés ; elle reconnaît une coupe via `Cast<ACutlassWeapon>(DamageCauser)->GetLocalMove()`. Aujourd'hui, une mort par taille affiche « TRANCHÉ ! » et joue la mort du tiki.
- Coup à la tête ×1,5 (`headshot_multiplier`).
- **Limites de T-152.**
  - Le ralentissement en attaque change `MaxWalkSpeed` chez le porteur et sur le serveur, chacun avec son horloge. Le lunge, une root motion source, s'applique lui aussi des deux côtés. Pour un client distant, attendre de petites corrections de position. À vérifier en hôte + client.
  - Le juice d'un client distant arrive avec un aller-retour de retard (le serveur confirme la touche). Sur l'hôte, il est immédiat.
  - Sur l'hôte, ralentir un tiki ralentit l'acteur autoritaire pendant 0,05 s.
  - La régénération plus rapide hors combat relève de `UIslanderMovementComponent` (`StaminaRegenDelay` 0,8 s, `IdleStaminaRegen` 20/s, `WalkStaminaRegen` 10/s), hors du périmètre de T-152. Proposition : `StaminaRegenDelay` 0,5 s et `WalkStaminaRegen` 15/s dans BP_Islander.
- Recette PIE hôte + client par l'utilisateur.

## Recette
- T-239 : 2 tests ajoutés (pistes FP triées, sabre devant le near clip à 120 Hz, lame au centre à la frappe et sens de chaque coup, filet de sécurité ; bob nul à l'arrêt, périodique, deux creux par foulée, respiration et recul en boucle). Compilés et verts le 2026-10-08 (lead).
- T-152 : 5 tests ajoutés, ce qui fait 14.
  - Couverts : tampon d'entrée, fenêtres d'annulation et récupération à −30 %, sélection de la cible d'aide à la visée et rotation bornée, lunge, balayage de la lame.
  - Compilé en cible jeu le 2026-10-06. Pas encore lancés.
  - Reste à tester en jeu : PIE solo dans `L_Test_Flat`, puis hôte + client.
- `monke_island.Weapons.Cutlass` : 9 tests avant T-152.
  - Couverts : 11 % et 9 coups par barre, ordre du combo, dégâts de la maquette, fil émoussé, parade, arc et portée, pistes triées, côté du fourreau, IK à deux os.
  - Testé headless le 2026-10-03 : 17/17 avec le mousquet.
- PIE solo, testé éditeur le 2026-10-03 : BP_Cutlass apparaît dans l'inventaire, le fourreau et la lame rengainée sont à la hanche gauche du pantin, 0 erreur au journal.
  - Le dégainé, les coups et la parade n'ont pas été testés en jeu : aucun outil MCP ne simule les touches.
