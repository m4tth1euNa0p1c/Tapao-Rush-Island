# Textures du galion — sources (toutes CC0)

Téléchargées par `factory/tools/fetch_ship_textures.py`. Nommage `<asset>_{diff,nor_gl,rough,ao[,metal]}_<res>.jpg`
lu par `marae_kit.pbr` (`_find`). Licence CC0 1.0 (Poly Haven, ambientCG) : aucune attribution requise, créditée quand même.

## Poly Haven (https://polyhaven.com/license) — 2k coque/pont/voile, 1k le reste

- `brown_planks_03` (2k) — bordé de coque : planches de bateau brun-gris patinées (teinter en brun chaud) — https://polyhaven.com/a/brown_planks_03
- `wood_planks_grey` (2k) — pont : planches claires lessivées gris-blond (holystoned), bord de mer — https://polyhaven.com/a/wood_planks_grey
- `plank_flooring_03` (2k) — pont (variante plus chaude) : planches blondes usées, clous apparents ; gaillards, passavants — https://polyhaven.com/a/plank_flooring_03
- `terlenka` (2k) — toile de voile écrue à laizes (tissage fin crème ; teinter / rayer les laizes dans le shader) — https://polyhaven.com/a/terlenka
- `dark_wooden_planks` (1k) — bois sombre : quille, préceintes, affûts de canon, lisses, cabestan, bittes — https://polyhaven.com/a/dark_wooden_planks
- `blue_painted_planks` (1k) — planches peintes bleu usé (pavois, galeries de poupe ; tint bleu nuit pour assombrir) — https://polyhaven.com/a/blue_painted_planks
- `rusty_metal_04` (1k) — fer forgé noir / rouillé : ancres, ferrures, cercles de tonneaux, chaînes, gonds de mantelets — https://polyhaven.com/a/rusty_metal_04
- `brown_leather` (1k) — cuir : brague et sangles, coffres, reliures de la cabine — https://polyhaven.com/a/brown_leather
- `velour_velvet` (1k) — velours rouge : tapis et rideaux de la grande cabine, coussins — https://polyhaven.com/a/velour_velvet
- `wooden_barrel` (1k) — tonneaux / barriques du pont et de la cale (douves + cercles) — https://polyhaven.com/a/wooden_barrel

## ambientCG (https://docs.ambientcg.com/license/) — zip 1K-JPG renommé

- `Metal046B` (1K) — fonte noire sale : tubes de canon, boulets, bouton de culasse — https://ambientcg.com/view?id=Metal046B
- `Metal048C` (1K) — laiton / bronze patiné : cloche, dorures du lion de proue et de la poupe, lanternes, fanal — https://ambientcg.com/view?id=Metal048C

## Réutilisées depuis `art/textures` (référence importée, ne pas modifier)

- `art/textures/characters/rough_wood` — mâts, vergues, beaupré, espars (bois brut) — https://polyhaven.com/a/rough_wood
- `art/textures/characters/fine_grained_wood` — lion de proue, sculptures, rambardes polies — https://polyhaven.com/a/fine_grained_wood
- `art/textures/characters/weathered_planks` — variante de bordé très usé (coque sous la flottaison) — https://polyhaven.com/a/weathered_planks
- `art/textures/characters/Rope001` — cordages, haubans, enfléchures, brague — https://ambientcg.com/view?id=Rope001
- `art/textures/characters/hessian_230` — sacs, bâches, hamacs (jute) — https://polyhaven.com/a/hessian_230
- `art/textures/props/minka/weathered_brown_planks` — cloisons intérieures, écoutilles (brun-rouge peint écaillé) — https://polyhaven.com/a/weathered_brown_planks
- `art/textures/props/minka/hinoki_planks` — plancher clair de la grande cabine (2k) — https://polyhaven.com/a/hinoki_planks
- `art/textures/props/minka/japanese_cedar_planks` — portes, panneaux de cabine, mantelets de sabord — https://polyhaven.com/a/japanese_cedar_planks

## Non texturé

- verre (fanaux, fenêtres de poupe) : matériau procédural (Principled BSDF transmission 1.0, IOR 1.5, roughness 0.05, teinte vert bouteille)
- corde : `Rope001` ci-dessus ; dorures : `Metal048C` ; fer : `Metal046B` (canons) et `rusty_metal_04` (ferrures)
