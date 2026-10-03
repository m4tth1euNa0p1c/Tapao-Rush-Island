# Catalogue des assets de l'atoll (Unreal)

Tous les objets du look-dev Blender sont dans Unreal, prêts à être placés. Côté Blender, ils sont archivés dans `art/archives/environnements/atoll_tuamotu/` : scène « Lineup », collections `LIB_*`, textures rangées par type (voir le README de l'archive).

Trois façons de les placer :
- à la main, en glissant depuis le Content Browser ;
- au pinceau, en mode Foliage : glisser les meshes dans la palette, ce qui crée les Foliage Types ;
- par PCG, comme `PCG_Raraka_Palms`.

## Format commun
- **Échelle et origine** : 1 unité = 1 cm. Pivot au pied de l'objet, au niveau du sol (Z = 0) ; les racines et la base des rochers descendent légèrement sous 0.
- **Orientation** : les cocotiers penchés, en sabre et couchés penchent vers **+X**. Pour les faire pencher vers l'eau, régler le yaw sur la direction de l'eau.
- **Nanite** : activé partout. Préservation de surface (Preserve Area) sur tout le feuillage, pour que les couronnes et les buissons ne s'amincissent pas au loin.
- **Collision** : `CTF_UseSimpleAsComplex`, avec des convexes `UCX_` seulement là où le joueur doit être bloqué :
  - troncs ;
  - masse des buissons denses (naupaka, miki-miki) ;
  - base des tahinu et pandanus ;
  - rochers, cocos, bois flotté.

  Sont traversables, sans aucune forme de collision :
  - l'herbe, les fougères et l'ipomée ;
  - les cocotiers sans tronc (`Juvenile`, `Sprout`) ;
  - les palmes sèches, les gravats et le coquillage.

  **Placement par PCG** : le générateur de maillage statique crée ses instances en `NoCollision`, ce qui rend les `UCX_` inopérants. Régler le `TemplateDescriptor` du sélecteur comme sur les acteurs `PCG_Raraka_*` :
  - collision `Custom`, `QueryAndPhysics`, `WorldStatic` ;
  - Block partout, sauf Camera = Ignore ;
  - `bCanEverAffectNavigation=false`.

  Passer aussi tous les canaux dans `CollisionResponses.ResponseToChannels`, pas seulement dans `ResponseArray`. Avant de modifier un graphe déjà généré, mettre `bRegenerateInEditor=false` sur son composant PCG : sinon, chaque modification relance une génération complète et fige l'éditeur.
- **Variation** : les vertex colors portent teinte/2 (RGB) et sécheresse (A), lues par les masters `M_Foliage_Leaf`, `M_Foliage_LeafCard` (masqué), `M_Vegetation_Bark` et `M_Vegetation_VertexColor`. Chaque slot de matériau porte le nom de son instance `MI_*`.
- **Régénération** : `blender --background --factory-startup --python scripts/atoll/export_ue.py -- [groupe|SM_Nom]` (groupes : `palms`, `sprouts`, `shrubs`, `groundcover`, `props`). Textures : `python scripts/atoll/stage_ue_textures.py`. Manifeste : `art/export/ue_manifest.json`.
- **Réimport** :
  - supprimer l'asset d'abord, car l'import ne remplace pas un asset existant ;
  - régler `Default__FbxStaticMeshImportData.VertexColorImportOption = Replace` (voir la fiche Raraka) ;
  - après l'import, appeler `StaticMeshTools.remove_collisions` sur chaque mesh dont `ucx` vaut 0 dans le manifeste. Sinon, l'importeur leur ajoute une boîte de collision automatique et ils deviennent bloquants.

## Conseils de pose
- Arbres et buissons : verticaux (pas d'alignement sur la pente), yaw aléatoire, échelle 0,85–1,15, enfoncés de 5 à 10 cm.
- Couvre-sol, cocos, palmes sèches, gravats, coquillages : alignés sur la normale du sol, yaw aléatoire, échelle 0,7–1,4.
- Ne pas poser de végétation sur la plage active (moins de 3 à 5 m de l'eau), sauf les cocotiers penchés en sabre et l'ipomée.

## Cocotiers — `/Game/MonkeIsland/Environment/Vegetation/Palms/Meshes`
| Asset | Hauteur | Où | Remarque |
|-------|---------|----|----------|
| `SM_CoconutPalm_Straight_A` / `_B` | 23 m / 19 m | cocoteraies, intérieur | 28 palmes, régimes de cocos, palmes sèches pendantes |
| `SM_CoconutPalm_Leaning_A` / `_B` | 19 m / 17 m | lisières, trouées | penche de 11 m / 8 m vers +X |
| `SM_CoconutPalm_Sabre_A` / `_B` | 14 m / 10 m | bord de lagon, au-dessus du sable | très incliné à la base, puis redressé |
| `SM_CoconutPalm_SCurve_A` | 18 m | intérieur, lisière océan | tronc en S (vent) |
| `SM_CoconutPalm_Fallen_A` | 12 m | haut de plage, après cyclone | couché sur 6 m vers +X puis redressé, motte racinaire |
| `SM_CoconutPalm_Young_A` / `_B` | 6 m / 8 m | sous-bois, replantation | tronc court, palmes longues |
| `SM_CoconutPalm_Juvenile_A` / `_B` | 2,5 m / 1,4 m | sous-bois (régénération) | sans tronc, traversable |
| `SM_CoconutPalm_Sprout_A` / `_B` | 0,8 m | sous-bois, à 1–7 m d'un cocotier | coco germé avec 2 ou 4 feuilles |
| `SM_CoconutPalm_DeadSnag_A` | 9 m | intérieur, rare (2 %) | chicot mort blanchi |

## Arbustes — `/Game/MonkeIsland/Environment/Vegetation/Shrubs/Meshes`
| Asset | Taille (L × l × H) | Où | Densité indicative |
|-------|--------------------|----|--------------------|
| `SM_Pandanus_A` / `_B` | 6,9 × 6,7 × 8,3 m / 4,7 × 7,1 × 7,2 m | lisières, arrière-plage, clairières | isolés ou en petits groupes |
| `SM_Pandanus_Young` | 2,4 × 2,4 × 3,3 m | lisières, sous-bois clair | — |
| `SM_Naupaka_A` / `_B` / `_C` | ~3 × 2,7 × 2 m | **haut de plage** côté lagon et océan, en haie | 1 tous les 2–3 m le long de la ligne de végétation |
| `SM_Tahinu_A` / `_B` | 4,7–5,5 m de large, 3,5 m de haut | crête de plage, isolé | 1 tous les 15–25 m |
| `SM_MikiMiki_A` / `_B` / `_C` | ~4 × 3,5 × 2,4 m | **crête de soupe de corail côté océan**, fourré serré | 1 tous les 2–3 m, en 2 rangs |

## Couvre-sol — `/Game/MonkeIsland/Environment/Vegetation/GroundCover/Meshes`
| Asset | Taille | Où |
|-------|--------|----|
| `SM_Fern_A` / `_B` | 1,1–1,4 m, 0,5 m de haut | sous les cocotiers, à l'ombre |
| `SM_Grass_Short` / `_Tuft` | 0,2–0,6 m | clairières, arrière-plage |
| `SM_Grass_Dry` | 0,4 m | sable sec, haut de plage |
| `SM_Ipomoea_A` / `_B` | tapis de 3–5 m, 13 cm de haut | **sable nu du haut de plage** (lianes rampantes, fleurs roses) |

## Accessoires — `/Game/MonkeIsland/Environment/Props/{Coconut,Coral,Beach}/Meshes`
| Asset | Taille | Où |
|-------|--------|----|
| `SM_Coconut_Brown` / `_Green` / `_Yellow` | 21–31 cm | sous les cocotiers (0–5 m du tronc), surtout bruns |
| `SM_CoconutFrond_Dry_A` / `_B` | 5 m / 3,7 m | au sol sous les cocotiers |
| `SM_CoralBlock_A` … `_F` | 0,4 → 2,2 m | platier et crête côté océan ; quelques-uns sur la plage |
| `SM_CoralRubble_A` … `_H` | 4–19 cm | crête de tempête (dense), platier, laisse de mer |
| `SM_Driftwood_A` / `_B` | 2,5–2,9 m | haut de plage, laisse de mer |
| `SM_LambisShell` | 14 cm | laisse de mer, platier (coquillage *Lambis*, Poly Haven CC0) |

## Matériaux
- Masters : `/Game/MonkeIsland/Materials/` (`M_Foliage_Leaf`, `M_Foliage_LeafCard`, `M_Vegetation_Bark`, `M_Vegetation_VertexColor`).
- Instances : dossier `Materials` de chaque catégorie.
- Paramètres utiles par instance :
  - `DryColor` : couleur des parties sèches ;
  - `SubsurfaceStrength` : lumière traversant les feuilles ;
  - `RoughnessScale` ;
  - `Tint` (écorces).
