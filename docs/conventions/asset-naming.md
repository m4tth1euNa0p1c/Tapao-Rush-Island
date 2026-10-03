# Conventions d'assets — monke_island

## Dossiers
| Contenu | Dossier |
|---------|---------|
| Niveaux | `/Game/MonkeIsland/Maps/` (`Lvl_`) |
| Données carto d'une île | `/Game/MonkeIsland/Map/<Ile>/` (`Meshes`, `Textures`) |
| Matériaux maîtres | `/Game/MonkeIsland/Materials/` |
| Environnement (sols, végétation, roches, props) | `/Game/MonkeIsland/Environment/<Catégorie>/` |
| Feature de gameplay | `/Game/MonkeIsland/<Feature>/` (`Blueprints`, `Data`, `Meshes`, `Materials`, `Textures`, `Audio`) |
| UI | `/Game/MonkeIsland/UI/` |
| Contenu du template | `/Game/FirstPerson`, `/Game/Variant_*`, `/Game/Weapons`, `/Game/LevelPrototyping`… (ne pas réorganiser) |

## Préfixes
| Type | Préfixe | Exemple |
|------|---------|---------|
| Niveau | `Lvl_` | `Lvl_Raraka` |
| Blueprint | `BP_` | `BP_CoconutCrab` |
| Widget | `WBP_` | `WBP_HUD` |
| Static mesh | `SM_` | `SM_Raraka_Terrain_2_0` |
| Skeletal mesh | `SK_` | `SK_Shark` |
| Squelette / Physics Asset / AnimBP | `SKEL_` / `PA_` / `ABP_` | `SKEL_CocoWarrior`, `PA_Bean`, `ABP_Bean` |
| Animation (AnimSequence) | `A_<Perso>_<Action>` | `A_CocoWarrior_Attack` |
| Matériau / instance / fonction | `M_` / `MI_` / `MF_` | `M_Water_Lagoon`, `MI_Water_Pass` |
| Texture | `T_<Nom>_<Suffixe>` | `T_SandDry_CH`, `T_Water_N` |
| Data asset / table | `DA_` / `DT_` | `DA_WeatherCyclone` |
| StateTree / Behavior Tree | `ST_` / `BT_` | `ST_Shark` |
| Input action / mapping | `IA_` / `IMC_` | `IA_Swim` |
| Niagara | `NS_` | `NS_ReefFoam` |
| Son | `MS_` (MetaSound), `SC_` (cue), `ATT_` (atténuation) | `MS_ReefSurf` |
| PCG | `PCG_` | `PCG_Cocoteraie` |
| String table | `ST_Text_` | `ST_Text_Quests` |

## Suffixes de texture
`_D` base color (sRGB) · `_N` normale (TC_Normalmap, convention DirectX) · `_ORM` AO / roughness / metal (linéaire) · `_R`, `_AO`, `_H` canaux isolés · `_CH` couleur + hauteur en alpha (couches de terrain) · `_Masks` masques linéaires (TC_Masks).

## Échelle et repère
1 unité UE = 1 cm. Repère des îles : X = est, Y = sud, Z = haut, Z = 0 au niveau moyen de la mer (`docs/reference/map/<ile>/terrain/georef.json`).
