# Scans CC0 telecharges pour le kit artefacts (factory/assets/textures/artifacts/)

Tous CC0 1.0 (domaine public, usage commercial libre, aucune attribution requise). Resolution 2K, JPG.
Telecharges le 2026-10-06 (API Poly Haven / ambientCG, aucun contenu hors CC0).

| Dossier | Source | URL | Cartes | Usage dans le kit |
|---|---|---|---|---|
| `coral_fort_wall_01/` | Poly Haven, Coral Fort Wall 01 (1,36 m) | https://polyhaven.com/a/coral_fort_wall_01 | diff, rough, nor_gl, disp, ao | calcaire corallien crayeux gris-blanc : depots de carbonate, `MAT_Coral_Dead` (couche chalk), `MAT_Salt_Deposit` |
| `coral_fort_wall_02/` | Poly Haven, Coral Fort Wall 02 (0,99 m) | https://polyhaven.com/a/coral_fort_wall_02 | diff, rough, nor_gl, disp, ao | calcarenite / corail poreux beige : base de `MAT_Coral_Dead` + deplacement du high-poly |
| `coral_fort_wall_03/` | Poly Haven, Coral Fort Wall 03 (1,10 m) | https://polyhaven.com/a/coral_fort_wall_03 | diff, rough, nor_gl, disp, ao | basalte vacuolaire sombre a debris de corail : `MAT_Basalt_Oceanic`, galets, base de `MAT_Coral_Red` |
| `Rock040/` | ambientCG, Rock 040 | https://ambientcg.com/view?id=Rock040 | Color, Roughness, Displacement, AO, NormalGL/DX | croute claire tachetee (tres desaturee, le lichen vert est neutralise) : `MAT_Salt_Deposit` |
| `Rock042L/` | ambientCG, Rock 042 L | https://ambientcg.com/view?id=Rock042L | idem | roche claire fine : `MAT_Bone_Aged`, `MAT_Shell_Tropical` (base) |

Reutilises depuis `art/` (lecture seule, deja CC0, voir `art/SOURCES.md`) : `art/models/props/lambis_shell` (Poly Haven, coquillage
photoscanne, geometrie + texture : coquillages incrustes du Coeur du Recif), `art/textures/characters/Lava002` (obsidienne),
`weathered_planks` + `rough_wood` (bois), `Bark014` (bourre de coco), `Rope001` (corde).

Reste procedural (complement uniquement) : nacre blanche / noire (aucun scan de nacre CC0 trouve), dents de requin, taches
d'algues seches, patine ocre, depots de sel (voile), veines / fissures (masques), micro-grain.

## Ajouts (objet FishGodFragment, 2026-10-06)

| Dossier | Source | URL | Cartes | Usage |
|---|---|---|---|---|
| `Rock035_2K/` | ambientCG, Rock 035 (2K) | https://ambientcg.com/view?id=Rock035 | Color, Roughness, Displacement, AO, NormalGL/DX | basalte fin sombre (grain, micro-eclats clairs) : `MAT_FishGodFragment` (fond de basalte) |
| `rock_05/` | Poly Haven, Rock 05 (2K) | https://polyhaven.com/a/rock_05 | diff, rough, disp, ao, nor_gl | roche piquetee, tres desaturee : patine d'erosion marine / piqures du basalte (FishGodFragment) |

## Ajouts (objet EclipseStone, 2026-10-06)

| Dossier | Source | URL | Cartes | Usage |
|---|---|---|---|---|
| `rock_01/` | Poly Haven, Rock 01 (2K, CC0) | https://polyhaven.com/a/rock_01 | diff, rough, disp, ao, nor_gl | roche grise a grain fin et regulier : face haute manipulee de la Pierre de l'Eclipse (cle `basalt_fine`, assombrie a un albedo ~0,04) |

`coral_fort_wall_03` (basalte vacuolaire) est reutilise pour flancs / dessous / paroi du trou de la Pierre de l'Eclipse ; ses debris clairs
de corail sont ecretes par une rampe de luminance sombre (voir `factory/kits/artifacts/eclipse_stone.py`).

## Ajouts (objet CrownOfTheLastAriki, 2026-10-06)

| Dossier | Source | URL | Cartes | Usage |
|---|---|---|---|---|
| `ShellSet001/` | ambientCG, Shell Set 001 (2K JPG, photogrammetrie, CC0 1.0) | https://ambientcg.com/view?id=ShellSet001 | Color, Roughness, Displacement, NormalGL/DX, Opacity (atlas de 9 coquilles bivalves) | anneaux de sommet de cone perces de la Couronne du dernier Ariki (cle `shell_set` : couleur / rugosite photo, voile calcaire, creux assombris) |

Aucun scan CC0 de nacre (Pinctada margaritifera) telechargeable sans compte n'a ete trouve (les scans Sketchfab ffishAsia exigent un
compte) : la nacre noire-argent des plaquettes est procedurale (bruit distordu + lamelles + reflets vert / rose / violet faibles) sur
la geometrie ; les fibres de sennit reposent sur le scan Bark014 (`art/textures/characters`, CC0, lecture seule).

## Ajouts (tresor : or, gemmes, 2026-10-06)

| Dossier | Source | URL | Cartes | Usage |
|---|---|---|---|---|
| `Metal048C/` | ambientCG, Metal 048 C (or impur rugueux, 2K) | https://ambientcg.com/view?id=Metal048C | Color, Roughness, NormalGL/DX, Metalness, Displacement | `MAT_Gold_Aged` : variation de couleur, rugosite, micro-relief (copie `T_Gold_Aged_*` dans `textures/Shared/`) |
| `Metal048B/` | ambientCG, Metal 048 B (or sale, traces, 2K) | https://ambientcg.com/view?id=Metal048B | idem | reserve (variante d'usure de l'or) |
| `Scratches002/` | ambientCG, Scratches 002 (2K) | https://ambientcg.com/view?id=Scratches002 | Opacity, NormalGL/DX, Color | rayures de l'or et des gemmes (`T_Gold_Scratches*`) |
| `../../hdri/studio_small_09_2k.hdr` | Poly Haven, Studio Small 09 (HDRI 2K) | https://polyhaven.com/a/studio_small_09 | HDR | reflets des vues de controle en studio (or, gemmes, nacre) |
| `../../hdri/kloofendal_48d_partly_cloudy_puresky_2k.hdr` | Poly Haven, Kloofendal 48d Partly Cloudy (Pure Sky) | https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky | HDR | reserve (exterieur) |

Generees (procedural, aucun scan CC0 equivalent) : `T_Nacre_Pinctada_Color/Normal` (nacre de Pinctada), `T_Feather_Aged_Color/Normal`
(atlas de plumes, alpha) dans `maquettes/artefacts-polynesiens/textures/Shared/`.
