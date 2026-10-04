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
| Endurance par utilisation (coup, parade encaissée) | 11 % du maximum, aucune attaque en dessous |
| Taille droite / taille revers / estoc / coup chargé | 35 / 35 / 45 / 70 |
| Portée / arc | 85 cm depuis le centre de Bean (+ rayon de la cible) ; arc de 110° pour les tailles, 40° pour l'estoc, 60° pour le coup chargé |
| Combo | taille droite → revers → estoc ; un clic dans les 0,45 s après la fin d'un coup enchaîne |
| Coup chargé | clic gauche maintenu 0,4 s |
| Parade (clic droit maintenu) | annule 80 % d'un coup de face et coûte 11 % d'endurance ; sans endurance, le coup passe entier |
| Fil | −1 % par touche ; sous 50 %, dégâts ×0,8 |
| Dégainer / rengainer | 0,5 s / 0,6 s |
| Tir ami | désactivé |

Un Coco Warrior ou un villageois (80 PV) tombe en 2 tailles + 1 estoc, ou en 1 taille + 1 coup chargé.

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
  - à l'instant de la frappe (timeline du 2e PC), il balaie l'arc devant Bean ;
  - il applique `ApplyPointDamage` (PvE : tikis), avec recul et impulsion ;
  - le porteur prédit son coup ; un refus du serveur le ramène en garde.
- **Parade :** `AInventoryItemActor::ModifyIncomingDamage`. Elle est appelée par `UIslanderHealthComponent::HandleTakeAnyDamage` à travers `UIslanderInventoryComponent`.
- **Poses :**
  - la poignée est clé en espace du mesh Bean (+Y avant, −X droite, cm) et suit le bassin du pantin ;
  - **3e personne :** la lame est attachée au pantin. Le bras droit vise la clé par IK à deux os (`upperarm_r`, `lowerarm_r`, `hand_r`), puis la lame est placée là où se trouve réellement la moufle, à `GripReachCm` le long de l'avant-bras. Elle reste donc en main même quand la clé est hors de portée du bras court du Bean. Le 2026-10-03, le défaut `grip_r` désignait un os absent : l'IK ne tournait pas et la lame flottait ;
  - **1re personne :** les mêmes clés passent dans le repère caméra (œil `FirstPersonEyeLocation`, plus `FirstPersonOffset`), et le bras FP pointe vers la poignée ;
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

## Reste à faire
- **Animations du corps exportées :** `bean-cutlass-anim` n'a pas d'export FBX, contrairement à `bean-musket-anim`. Il faut demander au 2e PC `A_Bean_Cutlass_*.fbx`, la torsion du buste et `fp_poses.json`. Le sabre suit pour l'instant les clés de poignée validées, sans torsion du buste.
- **Mob tranché en deux :** la maquette prévoit une découpe du mesh et deux moitiés physiques. Il faut le module `ProceduralMeshComponent` dans `Build.cs` : Smart App Control le bloquait, il est désactivé depuis le 2026-10-03. Autre voie : une variante par matériau de découpe. La session 78 fait les cadavres coupés en deux et les bras coupés ; elle reconnaît une coupe via `Cast<ACutlassWeapon>(DamageCauser)->GetLocalMove()`. Aujourd'hui, une mort par taille affiche « TRANCHÉ ! » et joue la mort du tiki.
- Coup à la tête ×1,5 (`headshot_multiplier`).
- Recette PIE hôte + client par l'utilisateur.

## Recette
- `monke_island.Weapons.Cutlass` : 9 tests.
  - Couverts : 11 % et 9 coups par barre, ordre du combo, dégâts de la maquette, fil émoussé, parade, arc et portée, pistes triées, côté du fourreau, IK à deux os.
  - Testé headless le 2026-10-03 : 17/17 avec le mousquet.
- PIE solo, testé éditeur le 2026-10-03 : BP_Cutlass apparaît dans l'inventaire, le fourreau et la lame rengainée sont à la hanche gauche du pantin, 0 erreur au journal.
  - Le dégainé, les coups et la parade n'ont pas été testés en jeu : aucun outil MCP ne simule les touches.
