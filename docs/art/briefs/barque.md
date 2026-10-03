# Brief asset : SM_Barque (+ SM_Barque_Oar)

- **Demandé par** : propriétaire (session lead, T-035) · **Pour** : lot **L2** (barque, sièges, arrimage, mouillages)
- **Priorité** : P0 · **Statut** : intégré (assets importés dans UE ; voir « Validation »)
- Cahier : §5 (arrimage par emplacement, 12 emplacements / 150 kg, sièges séparés), §6.4 (3 mouillages, déplacement 15 s), §6.5 (embarquer assis, pas debout sur la coque), §1 (DA stylisée, ambiance maritime inquiétante), recette L2 n°5, n°6, n°19.

## Usage en jeu
Un seul exemplaire par expédition, vu de près : on monte dedans, on y arrime le butin et on s'y assoit. Il est aussi vu de loin depuis la plage, comme point de retour, avec la lanterne la nuit. La barque est kinématique : elle se déplace sur une trajectoire prédéfinie entre mouillages, donc sans flottaison simulée. La navigation libre à la rame reste une extension, mais l'aviron existe déjà en mesh séparé.

## Intention de design
- **Lisible d'abord.** La silhouette est une chaloupe de galion : bordés à clin, tonture marquée, étrave haute avec un crochet de lanterne, tableau arrière incliné, gouvernail et barre.
- **Fonctionnel au premier coup d'œil.**
  - **Les 12 emplacements** forment une grille 3 × 4, peinte en crème sur le plancher, avec un anneau d'arrimage en fer par case. Le joueur voit où viser avec E.
  - **Les 4 sièges** sont deux bancs (avant et arrière) à coussins rouges : on sait où s'asseoir pour « embarquer ».
- **Couleurs de la Couronne** reprises du bean soldat : bleu marine `#1E2848`, rouge `#A3202A` / `#C21F2A`, or `#C99A3E`. La couronne dorée sur le tableau arrière sert la satire de la bureaucratie royale.
- **Ambiance inquiétante.**
  - bois sombre et patiné, peinture écaillée, saleté dans les creux (occlusion intégrée à la couleur) ;
  - carène goudronnée sous une ligne de flottaison crème ;
  - flamme rouge à fourche sur le mât de pavillon.

## Spécifications
| Élément | Valeur |
|---------|--------|
| Dimensions | coque 5,6 × 2,2 m ; 6,4 × 2,25 × 2,07 m hors tout (gouvernail, étrave, pavillon) |
| Échelle | pour des beans de 1,05 m : plat-bord 0,60 m au-dessus du plancher, assise 0,24 m |
| Détail | 17 954 triangles, Nanite non requis ; LOD auto à générer côté UE si besoin |
| Matériaux | **1 slot** `MI_Barque` (parent `M_Prop_Atlas`), atlas 2048 `T_Barque_D` (sRGB), `T_Barque_ORM` (AO / rugosité / métal, TC_Masks), `T_Barque_N` (DirectX, TC_Normalmap) → 1 draw call ; l'aviron partage l'atlas |
| Collision | 23 convexes `UCX_` : 6 blocs de fond (dessus = plancher), 12 murs avec lisses, proue pleine, tableau, 2 bancs, gouvernail. Aviron : aucune |
| Pivot | **axe de la barque, à la ligne de flottaison (Z = 0)** et non au sol : on pose l'acteur à la hauteur d'eau de `UIslandWaterSubsystem`. Tirant d'eau 0,30 m, plancher à Z = −0,10 m |
| Orientation | proue vers +X. Bâbord (gauche) = −Y en UE |

### Sockets (25)
| Socket | Usage |
|--------|-------|
| `Seat_0`…`Seat_3` | sièges (0 arrière bâbord, 1 arrière tribord, 2 avant bâbord, 3 avant tribord), sur le coussin |
| `Slot_00`…`Slot_11` | emplacements d'arrimage, centre de case sur le plancher (rangée = X de l'arrière vers l'avant, 3 colonnes en Y) |
| `Lantern` | crochet de lanterne sur l'étrave (nuit, L5) |
| `Flag` | haut du mât de pavillon |
| `Tiller` | main sur la barre |
| `Mooring` | anneau d'amarrage à l'étrave |
| `Oarlock_L` / `Oarlock_R` | pivots des avirons, où se place l'origine de `SM_Barque_Oar` |
| `Board_L` / `Board_R` | points d'abord latéraux (repère d'embarquement / IA) |
| `Marker` | 2,2 m au-dessus de la barque, pour le repère HUD ou la carte |

Les capacités (12, 150 kg, +4 emplacements à la boutique) restent des **données** (`DA_ExpeditionRules`). Le mesh n'offre que les places. Les 4 emplacements achetables demanderont une variante de mesh ou des sockets supplémentaires.

## Retours
- 2026-10-03 : rames jugées trop grosses par le propriétaire. Correction provisoire dans `BP_Barque` : échelle (0,5, 0,55, 0,55) sur `OarLeft` / `OarRight`, soit une rame d'environ 1,2 m, et pale plus inclinée (`DA_BoatRules.OarPitchDeg` = (−40, −12)). À la prochaine régénération : `oar_length` 1,2 et `oar_pivot` 0,39 dans `scripts/props/barque.py`, rayons × 0,7, puis échelle remise à 1.

- 2026-10-03 : barque jugée trop grosse par le propriétaire. Échelle 0,75 (proposition de monk-island-78, comparaison `art/_previews/tiki_echelle.png`) appliquée dans `BP_Barque` sur le composant `Hull` : 4,8 × 1,69 m. Rames à (0,62, 0,65, 0,65), soit environ 1,15 m. `DA_BoatRules.HullDepthCm` = 60. Vérifié en PIE : 12 cm d'immersion à vide. Si le mesh est réexporté à l'échelle, remettre `Hull` à 1.

## Livraison
- Générateur reproductible : `scripts/props/barque.py` (Blender 5.2, `--background --bake --export --render`), avec toutes les cotes dans `P` et les couleurs dans `PALETTE`.
- Source : `art/blender/props/barque.blend`.
- Exports : `art/export/Boat/SM_Barque.fbx`, `SM_Barque_Oar.fbx`, `Textures/T_Barque_{D,ORM,N}.png`. Entrées ajoutées dans `art/export/ue_manifest.json`.
- UE : `/Game/MonkeIsland/Boat/Meshes/SM_Barque`, `SM_Barque_Oar` ; `/Game/MonkeIsland/Boat/Materials/MI_Barque` ; `/Game/MonkeIsland/Boat/Textures/*` ; matériau maître réutilisable `/Game/MonkeIsland/Materials/M_Prop_Atlas`.
- Aucune ressource externe : tout est procédural, rien à ajouter à `art/SOURCES.md`.

## Validation
- Rendus Blender : `art/_previews/barque_{trois_quarts,dessus,profil,poupe,flottaison,detail}.png`, avec des silhouettes de bean et des caisses pour l'échelle.
- UE, vérifié par MCP :
  - bornes de −332…309 × ±112 × −37…170 cm (échelle et orientation correctes) ;
  - 23 convexes, 25 sockets aux bonnes positions ;
  - slot `MI_Barque` assigné ; collision de l'aviron retirée.
- **Pas encore fait** :
  - capture en situation dans `Lvl_Raraka` ;
  - placement aux mouillages (L2, `BP_Barque`) ;
  - test de marche et d'assise d'un bean en PIE.
