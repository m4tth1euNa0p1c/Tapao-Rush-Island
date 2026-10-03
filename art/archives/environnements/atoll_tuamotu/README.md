# Kit d'environnement : atoll des Tuamotu

Archive autonome du monde look-dev Blender de l'atoll : cocotiers, buissons, couvre-sol, accessoires, sols, eau et ciel.

Elle sert de base réutilisable pour d'autres îles, par exemple l'île de prototype Crownfall (ADR-001, écart E-02). Archivée le 2026-10-03, environ 300 Mo.

## Contenu
| Dossier | Contenu |
|---------|---------|
| `scene/atoll_tuamotu.blend` | Scène « Atoll » : motu procédural, 5 caméras. Scène « Lineup » : tous les assets côte à côte. Fichier compressé ; chemins de textures relatifs ; aucune image intégrée. |
| `textures/Ground` | 7 couches de sol 2K : couleur et hauteur (`CH`), normale (`N_GL`) |
| `textures/Vegetation` | Feuilles de 9 espèces et 3 écorces |
| `textures/Props` | Coquillage *Lambis* |
| `textures/Water` | Normale d'eau tuilable |
| `textures/Sky` | 3 HDRI 4K |
| `previews/` | Rendus : planches par collection, 5 caméras, couches de sol, aperçu des assets dans Unreal |
| `manifest.json` | Liste de chaque texture : taille, espace colorimétrique, source d'origine, matériaux qui l'utilisent |

Les collections `LIB_*` de la scène forment la bibliothèque de 58 meshes :
- `LIB_Palms` : 15 cocotiers ;
- `LIB_PalmsLOD` : 3 cocotiers simplifiés pour le lointain ;
- `LIB_Shrubs` : 11 arbustes ;
- `LIB_GroundCover` : 7 couvre-sols ;
- `LIB_Props` : 22 accessoires.

La fiche de chaque asset (taille, où le poser, collisions) est dans `docs/reference/assets/catalogue_atoll.md`.

## Conventions
- **Noms** : `T_<Nom>_<Carte>`, avec les cartes suivantes :
  - `D` : couleur, sRGB ;
  - `N_GL` : normale, convention OpenGL ;
  - `R` : rugosité, linéaire ;
  - `CH` : couleur sRGB, plus la hauteur en alpha (linéaire) ;
  - `ARM` : AO, rugosité, métal.
- **Normales** : au format OpenGL, celui qu'attend Blender. Pour Unreal, cocher *Flip Green Channel* à l'import. `T_Water_N` fait exception : elle est déjà au format DirectX.
- **Échelle** : 1 unité Blender = 1 m, Z vers le haut. Les assets ont leur pivot au pied, au niveau du sol.

## Réutiliser
- **Blender** :
  - ouvrir `scene/atoll_tuamotu.blend` ;
  - ou, depuis un autre fichier, *Fichier > Ajouter* (ou *Lier*), puis `atoll_tuamotu.blend` > `Collection` > `LIB_Palms` (ou un autre lot).

  Déplacer ou copier toujours le dossier entier : les textures sont liées en chemins relatifs.
- **Unreal** :
  - les 55 meshes et leurs matériaux sont déjà dans `/Game/MonkeIsland/Environment/` (normales DirectX) ;
  - pour un autre projet : clic droit sur le dossier > *Migrer* ;
  - les FBX prêts à importer sont dans `art/export/`, régénérables (voir plus bas).
- **Terrain** : les couches `CH` et `N_GL` alimentent `M_Terrain_Atoll`, avec un tiling en mètres. Dans Unreal, seules 4 couches sur 7 sont déjà importées : Feo, Litter, SandDry, SandWet.

## Textures
### Ground : couches de sol (2048², PNG)
Les sources sont des scans recolorés vers les teintes des Tuamotu : corail blanc et gris plutôt que brun.

| Couche | Rôle | Source |
|--------|------|--------|
| `T_SandDry` | Sable corallien sec : plage, arrière-plage | ambientCG Ground033 |
| `T_SandWet` | Sable mouillé : estran, bord du lagon | Poly Haven aerial_beach_01 |
| `T_Wrack` | Laisse de mer : sable compact et débris | Poly Haven shell_floor_01 |
| `T_Soil` | Terre de cocoteraie | Poly Haven forrest_sand_01 |
| `T_Litter` | Litière (bourre de coco, brindilles) | ambientCG Ground072 |
| `T_Rubble` | Gravier corallien (soupe de corail) | Poly Haven coral_gravel |
| `T_Feo` | Platier, calcaire corallien (aussi utilisé par les blocs de corail) | Poly Haven coral_ground_02 |

### Vegetation : feuilles et écorces
| Texture | Taille | Utilisée par | Source |
|---------|--------|--------------|--------|
| `T_CoconutLeaflet` | 128 × 1024 | folioles du cocotier | générée (`scripts/gen_leaf_textures.py`) |
| `T_CoconutEophyll` | 256 × 1024 | feuilles entières des jeunes cocotiers | générée |
| `T_PandanusLeaf` | 128 × 1024 | pandanus | générée |
| `T_NaupakaLeaf`, `T_TahinuLeaf` | 512² | naupaka, tahinu | générées |
| `T_MikiMikiTwig` | 512² | miki-miki (rameau détouré, alpha dans `D`) | générée |
| `T_FernLobe`, `T_IpomoeaLeaf`, `T_GrassBlade` | 256 × 512, 512², 64 × 512 | fougère, ipomée, herbe | générées |
| `T_PalmTrunk` | 2048 × 4096, JPG | tronc de cocotier, bois flotté | Poly Haven palm_tree_bark |
| `T_PalmFiber` | 2048², JPG | base des palmes, tronc du pandanus | Poly Haven palm_bark |
| `T_ShrubBark` | 2048², JPG | écorce des arbustes | Poly Haven bark_brown_02 |

### Props, Water, Sky
| Texture | Rôle | Source |
|---------|------|--------|
| `T_LambisShell_D/N_GL/ARM` (1K, JPG) | coquillage *Lambis* | Poly Haven lambis_shell |
| `T_Water_N` (2K, DirectX) | eau du lagon dans Unreal | générée (`scripts/map/make_water_normal.py`) |
| `kloofendal_48d_partly_cloudy_puresky_4k.hdr` | ciel de la scène : alizés, cumulus | Poly Haven |
| `secluded_beach_4k.hdr`, `spiaggia_di_mondello_4k.hdr` | ciels de plage de rechange | Poly Haven |

**Licences** : tout est CC0 (Poly Haven, ambientCG) ou généré par les scripts du projet. Les crédits complets sont dans `art/SOURCES.md`.

## Ce qui n'est pas dans l'archive
- **Scans bruts d'origine** (4 maps par matériau, plusieurs centaines de Mo) : ils restent dans `art/textures/` et se retéléchargent avec `python scripts/fetch_atoll_textures.py`. L'archive ne garde que les textures finales réellement utilisées.
- **FBX et textures DirectX pour Unreal** : ils restent dans `art/export/`, car ils servent de source aux assets Unreal et à `ue_manifest.json`.

## Régénérer
```powershell
blender --background --factory-startup --python scripts/blender_atoll_lookdev.py                                   # monde -> art/blender/atoll_lookdev.blend
blender --background --factory-startup art/blender/atoll_lookdev.blend --python scripts/atoll/archive_world.py       # -> cette archive
python scripts/pack_terrain_layers.py ; python scripts/gen_leaf_textures.py                                        # couches de sol, feuilles
```
