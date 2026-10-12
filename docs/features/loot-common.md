# Butin commun (T-076)

**Lot :** L5 (objets). Demande de l'utilisateur du 2026-10-03.

**Maquette :** `loot-butin-commun` v0.1.0 (validated) du 2e PC, dans `OneDrive\maquette-blender\maquettes\`. Ce sont 12 objets du quotidien à l'échelle du Bean.

## Objets (`/Game/MonkeIsland/Items/Loot/`)
| Blueprint | Objet | Valeur | Masse | Poids de tirage |
|---|---|---|---|---|
| `BP_Loot_CoconutBowl` | bol en coco | 2 $ | 0,15 kg | 1 |
| `BP_Loot_Basket` | panier usé | 4 $ | 0,35 kg | 0,6 |
| `BP_Loot_RopeCoil` | corde en fibres | 3 $ | 0,4 kg | 1 |
| `BP_Loot_TiedShells` | coquillages liés | 5 $ | 0,08 kg | 0,8 |
| `BP_Loot_WoodenSpoon` | cuillère en bois | 1 $ | 0,05 kg | 1 |
| `BP_Loot_Calabash` | petite calebasse | 3 $ | 0,25 kg | 1 |
| `BP_Loot_LineSpool` | bobine de ligne | 3 $ | 0,12 kg | 1 |
| `BP_Loot_RolledMat` | natte roulée | 4 $ | 0,6 kg | 0,6 |
| `BP_Loot_SmoothStone` | galet lissé | 1 $ | 0,25 kg | 1 |
| `BP_Loot_WoodenComb` | peigne en bois | 2 $ | 0,03 kg | 1 |
| `BP_Loot_SeedNecklace` | collier de graines | 4 $ | 0,04 kg | 0,7 |
| `BP_Loot_SmallChest` | petit coffret | 6 $ | 1,2 kg | 0,35 |

- **Type d'objet :** chaque Blueprint est un `APhysicsPropActor`, saisissable avec F, porté, lancé ou arrimé comme les autres objets de valeur. Sa valeur est dans `UValuableComponent`.
- **Chocs :** un choc sous 6 m/s ne coûte rien, et l'objet ne se brise jamais (`bBreakAtZero = false`).
- **Usages non implémentés :** la maquette prévoit un ramassage en inventaire (E, 0,3 s), l'artisanat et le coffret qui s'ouvre.

## Apparition au sol
`ALootScatterSpawner` (`Items/`) est posé dans Lvl_Raraka (dossier `Raraka/Gameplay`, `Rules` = `DA_LootRules`). Au début de chaque partie, le serveur :
- tire 300 objets selon les poids, sur des emplacements à 1,5 km au plus d'un PlayerStart et à 8 m au moins ;
- garde 15 m entre deux objets et évite les zones calmes (tag `TikiQuietZone` : le marae) ;
- pose chaque objet sur le sol trouvé par un tracé : la végétation est traversée, mais un emplacement sous un bâtiment de village, sur une pente, sur la plage mouillée ou dans l'eau est refusé.

**Emplacements :** `scripts/map/place_loot.py` écrit `collectibles/loot_spots.json` (65 578 points de terre sèche espacés de 9 m). Le vrai village de Motutapu est exclu sur 120 m autour de son centre.

## Pipeline
1. `scripts/props/loot_textures.py` copie les FBX et prépare les textures (`T_Loot_*`) : photo CC0 cuites avec la teinte de la maquette.
2. Import des textures et des meshes par MCP.
3. `scripts/props/import_loot.py`, commandlet éditeur fermé, crée :
   - les MI de `M_Marae_Surface` pour les textures photo ;
   - `M_Loot_Speckle` (deux couleurs + bruit en espace local) et ses MI pour les matériaux procéduraux ;
   - les 12 Blueprints et `DA_LootRules`.

## Recette
- `monke_island.Items.Loot` : 4 tests (tirage pondéré, espacement, anneau autour du départ, défauts) : 4/4 dans l’éditeur le 2026-10-03.
- PIE solo du 2026-10-03 :
  - journal : `300 common loot items scattered`. Le 1er essai à 250 objets avait 0 emplacement rejeté sur le niveau, en 64 ms ;
  - 47 objets à moins de 300 m du départ, le plus proche à 52 m ;
  - vignettes texturées.
- **Non vérifié :** l'aspect au sol en jeu (la capture de la vue éditeur ne montre pas le monde du PIE), la saisie avec F, le coût réseau de 300 objets répliqués.

## Réseau des objets libres (T-312, T-294, T-295, T-316 ; revue réseau T-268 R-04, R-05, R-28, R-43)
Tout `APhysicsPropActor` porte un `UNetRestComponent` (`Items/NetRestComponent.h`) : butin, artefacts, totems, reliques, objets des camps et des marae. Depuis T-316, les armes et munitions au sol (`AInventoryPickup`, `AMobWeaponPickup`, `AAmmoPickup`) et la caisse de transport (`ALootCrateActor`) le portent aussi. Le serveur simule l'objet et en réplique le mouvement.
- **En mouvement** : `AwakeNetUpdateFrequency` envois par seconde (30), au lieu de 100 par défaut dans le moteur.
- **Au repos** :
  - endormi, l'objet ne se réplique plus du tout (dormance `DORM_DormantAll`) ;
  - il se réveille dès qu'il bouge (événement de réveil physique, ou contrôle toutes les `CheckInterval`, 0,25 s), qu'un joueur le saisit, qu'un PNJ le porte ou qu'il est rangé ;
  - un changement sans mouvement (caché, attaché, collision, valeur) est envoyé une fois ;
  - `UNetRestComponent::WakeActorNet(Actor)` réveille un objet à la demande (serveur).
  - Règle (T-316) : tout changement d'état répliqué d'un objet qui peut dormir l'appelle **avant** un multicast et avant de cacher l'objet. Un multicast ne part que vers les clients pour qui l'acteur est pertinent, et un acteur caché sans collision ne l'est pour aucun : on cache alors son mesh, pas l'acteur, s'il doit encore envoyer un état. Une destruction n'a pas besoin de réveil : le moteur l'envoie aussi aux clients dont le canal dort.
- **Sur une base qui bouge** (T-294 : pont du galion, qui roule à chaque image) :
  - l'objet ne s'endort jamais, et la revue estimait environ 3 Ko/s par objet et par client ;
  - une fois immobile sur le pont pendant `GlueDelay` (0,75 s), le serveur le colle au pont ;
  - chaque machine le porte alors avec son propre pont. Son mouvement n'est plus envoyé ; l'objet reste éveillé, mais le serveur ne le regarde que `GluedNetUpdateFrequency` fois par seconde (2), et rien ne part tant que rien ne change. Il n'est pas dormant : un client qui recrée le pont ou la barque le recollerait à rien ;
  - il continue de simuler : la saisie, le compte de la cale du galion (recouvrement du volume) et le transport le trouvent comme avant ;
  - un joueur qui le prend le libère partout.
- **Dans la barque** (T-295) : la barque colle à sa coque, de la même façon, la cargaison qu'elle arrime.
- Valeurs : propriétés du composant, réglables dans chaque Blueprint (`Network`, `Network|Moving base`).
- Spec : `monke_island.Items.NetRest`.
- **Statut** : `compilé` le 2026-10-11 ; PIE non fait.
  - Essai n°5 de `docs/audit/revue-reseau-T268.md` : 20 objets et la caisse sur le pont, `stat net` et `net.Profile`, `LogMonkeNetRest Verbose`.
  - Attendu : les objets collés ne coûtent plus rien au repos, la caisse T-258 non plus depuis T-316.
- **T-316** (pickups et caisse) : `compilé` le 2026-10-11 (fichier par fichier) ; specs `monke_island.Weapons.Inventory`, `monke_island.LootCrate`, `monke_island.Items.Consumable` non exécutées ; PIE non fait.
