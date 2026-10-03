# Noix de coco consommables (+PV) et textures des noix de coco

| Champ | Valeur |
|---|---|
| Tâche | T-048, demande du propriétaire (2026-10-03). Hors cahier : §4 garde la faim pour plus tard. Un soin consommable est un ajout, à valider en playtest. |
| Code | `Items/ConsumableDefinition.h` (DataAsset), `Items/ConsumablePickupActor.*` (acteur interactif), `Player/IslanderHealthComponent::ApplyHeal` / `ComputeHeal` |
| Tests | `monke_island.Items.Consumable` (6 tests) ; `monke_island.Player.Movement` (21) relancés, verts |
| Assets UE | `/Game/MonkeIsland/Items/Data/DA_Consumable_Coconut` (+25 PV, mesh vert), `DA_Consumable_CoconutYellow` (+20 PV, mesh jaune), `/Game/MonkeIsland/Items/Blueprints/BP_CoconutPickup` |
| Textures | `scripts/props/coconut_textures.py` → `T_Coconut_{Brown,Green,Yellow}_{D,N,ORM}` (1K, normale DirectX), `MI_Coconut_*` (parent `M_Prop_Atlas`) affectés aux `SM_Coconut_*` |

## Règles
- **Visée.** On vise une noix de coco fraîche (verte ou jaune) à moins de 200 cm, en ligne de vue. Le HUD affiche « F  Boire la noix de coco (+25 PV) ».
- **Blessure obligatoire.** Le prompt n'apparaît que si le joueur est blessé : la noix reste en place pour plus tard. Le réglage `bUsableAtFullHealth` du DataAsset permet de changer ce comportement.
- **Côté serveur.** F déclenche la suite suivante :
  1. le serveur revérifie la portée et la ligne de vue (composant de saisie), puis `CanInteract` ;
  2. il soigne via `ApplyHeal`, plafonné à `MaxHealth` et refusé si le joueur est mort ;
  3. il marque la noix consommée (variable répliquée) ;
  4. il la cache sur tous les postes, puis la détruit 0,5 s plus tard.
  
  Une noix ne soigne qu'une fois, même si deux joueurs appuient en même temps.
- **Coût réseau.** Les noix posées dans la map sont en dormance réseau : aucun coût tant que personne ne les boit.
- **Distinction visuelle.** Les noix **brunes et sèches** restent du décor, dispersé par PCG et non interactif. Seules les fraîches se boivent.

## Valeurs (DataAsset, jamais dans le code)
| DataAsset | Soin | Mesh | Texte |
|---|---|---|---|
| `DA_Consumable_Coconut` | 25 PV | `SM_Coconut_Green` | Boire la noix de coco |
| `DA_Consumable_CoconutYellow` | 20 PV | `SM_Coconut_Yellow` | Boire la noix de coco |

## Reste à faire
- [ ] PCG : faire produire au décor uniquement des noix brunes, et répartir les `BP_CoconutPickup` (vertes et jaunes) près des cocotiers. Le graphe et les blocs PCG appartiennent à l'environnement (T-036) : à coordonner.
- [ ] Son de dégustation (`UseSound`) et petite animation de bras.
- [ ] Texte dans une String Table quand l'UI passera en tables (§16).
- [ ] Test PIE hôte + client : un seul soin pour deux appuis simultanés.
