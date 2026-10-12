# Inventaire : barre rapide, sac, ramasser et lâcher

- **Tâches** : T-157 (ramasser et lâcher les armes), T-166 (écran d'inventaire façon Minecraft), lot L5 ; cahier §4 (commandes, portée de 200 cm et ligne de vue vérifiées par le serveur), §9 (serveur autoritaire), §19 (HUD), recette n° 2/3 (pas de duplication).
- **Demandes** : « un inventaire pour que le joueur puisse ramasser les armes et les lâcher » (T-157) ; « répare l'inventaire, le dépôt et le remplacement ; E ouvre l'inventaire, même gestionnaire que Minecraft, dans la logique et le design ; sobre » (T-166).
- **Code** (`Source/monke_island/`) :
  - `Weapons/IslanderInventoryComponent` : cases répliquées (`FInventorySlot` : acteur, définition, quantité), barre puis sac dans un seul tableau, RPC serveur de l'écran (`ServerMove`, `ServerQuickMove`, `ServerGather`, `ServerDrop`), ramassage, lâcher, délégués `OnInventoryChanged` (Blueprint) et `OnInventoryChangedNative` (Slate) ;
  - `Weapons/InventoryGrid` : règles pures, sans monde ni acteur (déplacer, couper, empiler, échanger, transfert rapide, regrouper, placement d'un objet ramassé) ;
  - `Weapons/InventoryItemDefinition` (DA_Item_*) : `MaxStack`, `Icon`, `ShortName`, `PickupMesh`, `PickupMassKg`, `PickupClass` ;
  - `Weapons/InventoryPickup` et `Weapons/InventoryItemActor` : l'objet au sol et le transfert d'un porteur à l'autre (T-157, inchangés par T-166) ;
  - `UI/Tapao/STapaoInventory` : modèle (copie des cases, rafraîchie par l'événement), case, écran ; `UI/Tapao/STapaoHUD` : barre rapide du HUD ; `UI/Tapao/TapaoUISubsystem` : ouverture et fermeture ;
  - `Interaction/IslanderGrabComponent` : F appuyé / F maintenu.
- **Tests** : `monke_island.Weapons.InventoryGrid` (logique pure : déplacement, échange, moitié, empilement, transfert rapide, regroupement, placement, plein, 5 000 opérations aléatoires sans duplication ni perte) et `monke_island.Weapons.Inventory` (dans un monde : barre puis sac puis remplacement, déplacements serveur avec la même requête envoyée deux fois, piles qui se coupent et se rejoignent sans acteur en trop, lâcher partiel, double prise dans la même image).
- **Scripts éditeur** : `scripts/editor/setup_inventory_input.py` (G, T-157) et `scripts/editor/setup_inventory_screen_input.py` (E et F, T-166).

## Commandes
| Action | Touche | Effet |
|---|---|---|
| Ouvrir / fermer l'inventaire | **E** (Échap ferme aussi) | Écran par-dessus le jeu, qui continue (multijoueur). Le curseur apparaît ; la vue et les déplacements sont bloqués tant que l'écran est ouvert, comme dans Minecraft. |
| Choisir l'objet en main | 1 à 4 | Inchangé. La même touche range l'objet (mains nues). Seules les cases de la barre vont en main. |
| Interagir / ramasser | **F appuyé** (moins de 0,3 s) | Utilise ce qui est sous le réticule : ramasse une arme, s'assoit dans la barque, etc. Un objet physique sans interaction est soulevé, comme avant. |
| Soulever un objet physique | **F maintenu** (0,3 s, `HoldToGrabSeconds`) | Soulève l'objet visé, même une arme au sol. S'il n'y a rien à soulever, le maintien fait l'interaction. F de nouveau, ou G, lâche l'objet porté. |
| Lâcher l'objet en main | G | Inchangé (T-157) : G lâche d'abord l'objet soulevé, sinon l'arme en main. |

### Dans l'écran (gestes de Minecraft)
| Geste | Effet |
|---|---|
| Clic gauche sur une case | Prend toute la pile sur le curseur. Sur une autre case : la pose (case vide), la complète (même objet empilable) ou l'échange (autre objet ; le curseur garde alors l'autre objet). Sur sa case d'origine : la repose. |
| Clic droit | Curseur vide : prend la moitié (la plus grosse). Curseur plein : pose un seul objet. |
| Maj + clic | Transfert rapide entre la barre et le sac (complète d'abord les piles du même objet, puis la première case vide). |
| Double-clic | Regroupe sur le curseur les objets identiques des autres cases, jusqu'à la taille de pile. |
| 1 à 4 au survol d'une case | Échange cette case avec la case correspondante de la barre. |
| Clic hors du cadre | Lâche au sol ce que porte le curseur (clic droit : un seul objet). Les objets deviennent des pickups (T-157). |
| E ou Échap | Ferme. Ce que portait le curseur est simplement reposé. |

### Schéma de l'écran (1080p, cases de 64 px)
```
                 jeu visible derrière un voile léger
   +--------------------------------------------------------------+
   |  Inventaire                                                   |
   |                                                               |
   |  [    ][    ][    ][    ][    ][    ]                         |
   |  [    ][    ][ 12 ][    ][    ][    ]    sac : 18 cases, 3 x 6 |
   |  [    ][    ][    ][    ][    ][    ]                         |
   |                                                               |
   |  [1   ][2   ][3   ][4   ]              barre rapide (1 à 4)   |
   |   ====                                 case en main : cadre   |
   |                                        doré plus épais        |
   |  Clic : prendre / poser · Clic droit : moitié / un ·          |
   |  Maj + clic : barre <-> sac · Double-clic : regrouper ·       |
   |  1 à 4 sur une case : vers la barre · Clic hors du cadre :    |
   |  lâcher · E / Échap : fermer                                  |
   +--------------------------------------------------------------+
        [icône]  <- la pile portée suit la souris, quantité en bas à droite
```
- Case : carré sombre semi-transparent, bordure fine crème. Survol : bordure claire. Case en main : bordure dorée **et plus épaisse** (l'information ne passe pas que par la couleur).
- Contenu : l'`Icon` de la définition, sinon la vignette du kit TAPAO de la classe (mousquet, sabre, torche, trompette), sinon le `ShortName` (ou les 4 premières lettres du nom). Quantité en bas à droite dès 2. Numéro de touche en haut à gauche dans la barre.
- Info-bulle au survol : nom, puis l'état (mousquet : chargé ou vide et balles en réserve ; pistolet et tromblon : coups chargés et réserve ; torche : combustible ; sabre : fil), puis la quantité d'une pile.
- Textes de 18 px au moins à 1080p (titre 28 px). L'échelle DPI du viewport suit la résolution (720p compris). Il n'existe pas encore de réglage « échelle d'interface » dans le menu des réglages ; l'écran le suivra quand il existera.
- La barre du HUD (bas de l'écran) utilise les mêmes cases, sans interaction.

## Règles
### Disposition
- Un seul tableau de cases : la **barre rapide** (`SlotCount`, 4, touches 1 à 4) puis le **sac** (`BagSlotCount`, 18, affiché en `BagColumns` = 6 colonnes). Les trois valeurs se règlent sur le composant d'inventaire de `BP_IslanderBean`. Il n'existe pas de DataAsset d'inventaire, et en créer un aurait demandé un asset, ce qui était hors du périmètre de cette tâche.
- Une case contient un acteur (`AInventoryItemActor`), sa définition et une quantité. Une pile garde **un seul acteur**, quel que soit le nombre d'objets.

### Piles
- `UInventoryItemDefinition::MaxStack` : 1 par défaut (armes, outils) ; par exemple 30 pour des cartouches.
- Couper une pile crée un nouvel acteur pour la nouvelle pile. Quand deux piles se rejoignent, l'acteur de celle qui disparaît est détruit.
- **Munitions** : elles restent pour l'instant **dans les armes** (balles du mousquet, charges du pistolet, cartouches du tromblon). Les faire entrer dans l'inventaire aurait demandé de modifier `Musket*`, `Pistol*` et `Shotgun*`, que T-160 modifie en ce moment. La structure des piles est prête. Il restera à créer un `DA_Item_*` de munitions avec `MaxStack` supérieur à 1, et à faire puiser les armes dans l'inventaire.

### Dépôt (ramassage) et remplacement
- F sur une arme au sol, avec dans l'ordre :
  1. une pile du même objet qui a de la place ;
  2. sinon la première case vide de la barre ;
  3. sinon la première case vide du sac.
- L'objet passe en main si le joueur a les mains nues et que l'objet va dans la barre.
- **Tout est plein** : la logique de T-157 est gardée, car c'est la plus claire. L'arme en main est remplacée et tombe à la place de l'objet ramassé. L'invite le dit : « Ramasser X (lâche Y) ». Avec les mains nues, l'invite dit « Inventaire plein : prends en main l'arme à échanger », et rien ne se passe.
- F maintenu sur une arme au sol la soulève physiquement au lieu de la ramasser.
- Limite : l'invite (`AInventoryPickup`, fichiers de T-160) appelle encore `ChooseSlotForPickup(bReplaces)`, qui ne voit que les cases vides. Pour un objet empilable, elle peut donc annoncer « plein » alors qu'une pile peut encore le prendre. Il faut la passer à `ChooseSlotForPickup(Definition, bReplaces)` une fois T-160 fini. Les armes ne sont pas concernées, car elles ne s'empilent pas.
- Les **objets de valeur du butin** (`UValuableComponent`, quotas, cahier §5) restent physiques : on les porte jusqu'à la barque. N'entrent dans l'inventaire que les objets qui ont une `UInventoryItemDefinition` avec une `ItemClass` (armes, outils, et plus tard munitions).
- Exception (T-161) : l'arme lâchée par un guerrier tiki (`BP_Pickup_<Arme>`, `AMobWeaponPickup`) est les deux à la fois : F la ramasse dans une case (arme de mêlée, `DA_Item_Spear` / `_ObsidianClub` / `_ShellHammer`, voir `cutlass.md`), F maintenu la soulève et la barque compte sa valeur. Dans l'inventaire elle ne compte pas dans la cale, mais elle garde sa valeur courante (base de sa zone d'origine et pertes) : relâchée (G), elle la retrouve sans que le multiplicateur de la zone soit appliqué une seconde fois (T-309), et d'une île à l'autre aussi, par le chargement gardé (`FRunLoadoutItem::BaseValue` / `Value`, T-314).
- **Réseau des objets au sol (T-316).** Les armes et les munitions au sol (`AInventoryPickup`, `AMobWeaponPickup`, `AAmmoPickup`) portent un `UNetRestComponent`, comme les props : 30 envois par seconde en mouvement au lieu de 100, aucun au repos (dormance), collées au pont du galion ou à la coque de la barque sur chaque machine. Une arme prise avec F est détruite, ce que tout client reçoit, même si son canal dort. Un paquet de munitions entamé est réveillé : le reste est vu de tous. Un paquet vidé est caché par son mesh, pas par l'acteur, pour rester pertinent jusqu'à sa destruction. Spec `monke_island.Weapons.Inventory` (tests T-316).

### Serveur autoritaire et anti-duplication
- Le curseur de l'écran **ne déplace rien** : il désigne seulement une case d'origine et une quantité. Chaque geste envoie une requête serveur (`RequestMove`, `RequestQuickMove`, `RequestGather`, `RequestDrop`), avec des index de cases et une quantité, jamais un objet.
- Le serveur :
  1. reconstruit les cases ;
  2. applique la règle pure (`InventoryGrid`) ;
  3. vérifie l'invariant (`InventoryGrid::IsValid` : quantités entre 1 et `MaxStack`, chaque acteur dans une seule case) ;
  4. recrée ou détruit les acteurs (`CommitCells`).
- Si un acteur ne peut pas être créé, rien ne change. Une requête périmée (deux clics rapides, la même requête deux fois) trouve une case vide et ne fait rien.
- Limites de rythme : `MaxScreenRequestsPerSecond` (20 par seconde, en seau à jetons) ; le lâcher garde `DropCooldown` (0,25 s). Lâcher est refusé en ragdoll, et tout réarrangement est refusé une fois mort.
- Le client recale le curseur sur la réponse du serveur, via `OnInventoryChangedNative` : jamais plus que ce que contient la case d'origine.
- Assis dans la barque, un réarrangement ne met jamais un nouvel objet en main : la règle du rameur est conservée.
- Lâcher une pile : l'acteur de la pile part dans un pickup avec son état ; chaque autre unité devient un pickup de la définition, légèrement décalé.

### Écran et entrées
- `IA_InventoryScreen` (E, dans `IMC_Inventory`) diffuse `UIslanderInventoryComponent::OnScreenRequested`. `UTapaoUISubsystem` ouvre l'écran pour le joueur local, sauf en pause, dans un menu, avec un écran de fin, avec le menu des réglages ouvert, ou si le joueur est mort.
- Le code de gameplay ne dépend pas de l'interface.
- En mode d'entrée « UI seulement », les touches tenues sont relâchées à l'ouverture (`FlushPressedKeys`).
- E (et les touches de l'action, lues dans le mapping du joueur) et Échap ferment l'écran. Dans l'éditeur, Échap ferme l'inventaire sans arrêter le PIE : le pré-processeur de la pause le prend en premier.
- L'écran se ferme tout seul à la mort, à la réapparition, à l'ouverture de la pause et au changement de carte.

## Valeurs (BP_IslanderBean, composant d'inventaire)
| Propriété | Défaut | Rôle |
|---|---|---|
| `SlotCount` | 4 | cases de la barre rapide |
| `BagSlotCount` / `BagColumns` | 18 / 6 | sac (3 × 6) |
| `MaxScreenRequestsPerSecond` | 20 | limite de rythme des RPC de l'écran |
| `PickupReach` | 250 cm | contrôle serveur de la portée |
| `bEquipPickupWhenBareHanded` | vrai | les mains nues prennent l'objet ramassé (dans la barre) |
| `DropForwardDistance` / `DropHeight` | 60 / 20 cm | point de lâcher |
| `DropSpeed` / `DropUpSpeed` / `MaxDropSpeed` | 220 / 120 / 600 cm/s | impulsion bornée |
| `DropCooldown` | 0,25 s | limite de rythme du lâcher |
| `InventoryMappingPriority` | 2 | au-dessus des contextes du personnage (1) |

Composant de saisie : `HoldToGrabSeconds` = 0,3 s (0 = chaque appui interagit tout de suite, sans maintien).

Par définition (`DA_Item_*`) : `MaxStack` (1), `Icon`, `ShortName`, `PickupMesh` (sinon un bâton provisoire de 60 × 10 × 10 cm), `PickupMassKg` (3 kg), `PickupClass`.

## Reste à faire dans l'éditeur (après le build éditeur)
1. Prendre le verrou de l'éditeur, puis lancer `scripts/editor/setup_inventory_screen_input.py` avec l'éditeur compilé avec T-166. Le script :
   - crée `IA_InventoryScreen` sur E dans `IMC_Inventory` ;
   - retire E de `IA_Grab` ;
   - remappe F sans déclencheur ;
   - vide les déclencheurs de `IA_Grab` ;
   - branche l'action sur `BP_IslanderBean` ;
   - liste les autres touches E du projet.
2. Si ce n'est pas déjà fait : `scripts/editor/setup_inventory_input.py` (G, T-157).
3. Facultatif : `Icon` et `ShortName` dans `DA_Item_Musket`, `DA_Item_Cutlass`, `DA_Item_Torch` et ceux du pistolet et du tromblon (des textures `T_UI_Item_*` dans `/Game/MonkeIsland/UI/`).
4. Recette en PIE solo :
   - E ouvre, puis E et Échap ferment ;
   - chaque geste du tableau ;
   - plein, puis remplacement ;
   - F appuyé et F maintenu sur une arme au sol, puis sur un objet physique.
5. Recette en PIE hôte + client : gestes rapides du client (double-clic, clics répétés) sans duplication, et lâcher hors du cadre.

## Limites connues
- **Preuve** : seule la compilation de la cible jeu est faite (0 erreur, 0 avertissement). Les tests Automation et l'écran n'ont pas été lancés : l'éditeur est partagé, la DLL éditeur n'est pas recompilée, et ni PIE ni Live Coding n'étaient autorisés. Le script d'entrées n'a pas été exécuté.
- Les textes sont en `LOCTEXT` (espace `TapaoUI`), comme le reste de TAPAO. Le passage en String Table se fera avec les autres textes TAPAO.
- Il n'y a pas de vignette générée depuis le mesh, car un jeu packagé ne peut pas créer de miniature d'asset. L'icône vient de la définition ou du kit, sinon le nom court est affiché.
- Les autres joueurs reçoivent aussi le contenu du sac par la réplication : l'animation lit les objets portés sur les pawns distants. C'est une fuite d'information mineure.
- Au sol, l'arme reste attachée, cachée, à son ancien porteur jusqu'au ramassage (T-157). Les armes ne tombent pas à la mort du joueur : l'inventaire est détruit avec le pawn.
