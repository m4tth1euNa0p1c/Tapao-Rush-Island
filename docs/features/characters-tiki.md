# Créatures tiki — mobs, PNJ et sniper (planches `sprite/mob-*.png`, `pnj-1.png`, `sniper-1.png`)

| Champ | Valeur |
|---|---|
| Lot | L3 (« un ennemi terrestre simple, attaque de mêlée avec préparation visible, mort ») ; PNJ et sniper préparés pour L8 (factions) : art seulement, aucun gameplay avant le noyau |
| Générateur | `scripts/character/tiki_creatures.py` (package `scripts/character/tiki/`) → `art/blender/characters/tiki_creatures.blend` |
| Ressources CC0 | `python scripts/character/fetch_character_assets.py` → `art/textures/characters/`, `art/models/characters/` ; crédits dans `art/SOURCES.md` (section Personnages) |
| Exports | `art/export/Characters/<Nom>/` : `SK_<Nom>.fbx`, `SK_<Nom>_<Arme>.fbx`, `SM_<Nom>_<Arme>.fbx`, `A_<Nom>_<Anim>.fbx` ; inscrits dans `art/export/ue_manifest.json` |
| Rendus de contrôle | `art/_previews/tiki_lineup.png`, `tiki_poses.png`, `tiki_echelle.png`, `tiki_barque.png`, `tiki_<clé>_turnaround.png` |
| Cible UE | `/Game/MonkeIsland/Characters/<Nom>/` (`Meshes`, `Anims`, `Materials`, `Physics`, `Blueprints`) |
| Statut | modèles, armes, squelettes, animations et FBX **faits dans Blender** ; FBX vérifiés (skin et courbes présents) ; **rien n'est importé dans UE** |

```
blender --background --factory-startup --python scripts/character/tiki_creatures.py -- [--only coco,crab] [--render] [--quick] [--export] [--no-refs] [--no-save]
```

## Les cinq créatures
Échelle : celle du bean joueur (1,05 m, ADR-002), donc corps plus grand pour les plus menaçants. Toutes les valeurs (fractions de H) sont dans `SPECS` (`tiki/creatures.py`).

| Clé | Identifiant | Nom | Planche | H (sans ornement) | Arme ou objet | Rôle visuel | Triangles corps / arme |
|---|---|---|---|---|---|---|---|
| `villager` | `CHR_Npc_Villager` | Villageois Inquiet | pnj-1 | 0,95 m | panier de fruits (bananes et pommes CC0) | PNJ non hostile : visage inquiet, pagne, pousse sur la tête | 44,8 k / 10,6 k |
| `coco` | `CHR_Mob_CocoWarrior` | Guerrier Coco | mob-1 | 1,00 m | lance-massue à dents de pierre (1,4 m) | mob de base : coco fibreux, masque tiki jaune, chignon et couronne crème | 43,8 k / 10,4 k |
| `crab` | `CHR_Mob_CrabGuardian` | Gardien Crabe | mob-2 | 1,15 m | marteau-coquillage (coquillage *Lambis* CC0) | mob défensif : carapace piquetée, cornes, masque turquoise, pinces | 43,5 k / 12,4 k |
| `ash` | `CHR_Mob_AshWatcher` | Veilleur des Cendres | mob-3 | 1,30 m | massue d'obsidienne | mob lourd : roche à facettes veinée de lave, masque en planches, yeux brûlants | 35,3 k / 5,6 k |
| `bramble` | `CHR_Mob_BrambleStalker` | Rôdeur des Ronces | sniper-1 | 1,25 m | sarbacane (1,1 m) + fléchettes (carquois à la hanche gauche) | tireur embusqué : manteau de ~335 feuilles, frondes de fougère, masque à bouche ronde | 51,6 k / 4,6 k |

Budget §10 respecté (30 à 60 k triangles en LOD0). Matériaux : 8 à 14 emplacements par créature, à réduire à 2 par le bake (voir « Reste à faire »).

## Matériaux (textures libres de droits)
Chaque matériau garde la couleur de la planche. Les jeux de textures CC0 apportent le détail : leur luminance passe dans une rampe entre deux couleurs de la palette, et leur hauteur pilote le relief. Ils sont projetés en boîte sur les coordonnées *Generated* en mètres, donc le détail suit la peau quand le squelette la déforme.
- **Bourre de coco** (Guerrier, Villageois) : ambientCG Bark014.
- **Carapace** (Crabe) : Poly Haven coral_ground_02.
- **Roche et lave** (Veilleur) : ambientCG Lava002, plus des fissures émissives géométriques dont l'intensité varie selon un bruit.
- **Bois** : Poly Haven weathered_planks (masque du Veilleur), fine_grained_wood (masque du Rôdeur), rough_wood et bark_willow (manches).
- **Cordes** : ambientCG Rope001.
- **Pagne** : Poly Haven hessian_230.
- **Panier** : ambientCG Wicker002.
- **Bambou** : Poly Haven bamboo_veneer.
- **Peau d'écorce moussue** (Rôdeur) : Poly Haven tree_bark_03 et ambientCG Moss002.

## Squelette `SKEL_<Nom>` (même hiérarchie pour les cinq)
`root` → `pelvis` → `spine_01` → `spine_02` → `head` → `crest` (ornement du sommet, mouvement secondaire).
- Sous `spine_02` : `upperarm` → `lowerarm` → `hand`, avec `weapon_r` sous `hand_r`.
- Sous `pelvis` : `thigh` → `calf` → `foot`.
- Rôdeur seulement : `prop_l` sous `hand_l`, la fléchette de rechargement, à l'échelle 0 quand elle est cachée.

19 os (20 pour le Rôdeur), mêmes noms que le mannequin UE et `SKEL_Bean`. Le personnage regarde −Y dans Blender, donc +Y dans UE. Origine aux pieds.

Skinning :
- le corps en œuf suit la colonne selon la hauteur (fondu 1D) ;
- les membres utilisent des poids par distance, limités à leur chaîne d'os ;
- l'ornement du sommet et l'arme sont rigides.

L'arme est un mesh squelettique séparé, sur le même squelette (Leader Pose, ou masquée pour désarmer). Sa version `SM_` a l'origine au point de prise et l'axe long sur +Z.

## Animations provisoires (§10, §6.3)
À 30 i/s et sur place, sans root motion (c'est le CharacterMovement de l'IA qui déplace l'acteur). La frame 0 est la pose de garde de chaque créature, ce qui rend les fondus faciles dans l'AnimBP.

| Créature | Cycles (en boucle) | Ponctuelles |
|---|---|---|
| Guerrier Coco | Idle, Walk, Run | Attack (estoc : préparation 12 i, coup 4 i, tenue, retour ; 38 i), Hit, Death (en arrière) |
| Gardien Crabe | Idle (pinces qui claquent), Walk, Run | Attack (coup de marteau par-dessus la tête ; 42 i), Hit, Death |
| Veilleur des Cendres | Idle (souffle lourd), Walk (pas lourds), Run | Attack (balayage latéral ; 46 i), Hit, Death (en avant) |
| Rôdeur des Ronces | Idle (accroupi, scrute), Walk (furtif), Run, Aim | Shoot (souffle 14 i), Reload (fléchette prise au carquois puis glissée dans la sarbacane ; 54 i), Hit, Death |
| Villageois Inquiet | Idle (regarde partout, tremble), Walk, Run (fuite, bras en l'air), Cower (peur), Talk (offre le panier) | Hit, Death |

Les poses sont décrites dans `tiki/anims.py` : accroupi pieds à plat, rotations de la colonne, bras gauche, et objet tenu placé par IK (point de prise et direction). Dans le `.blend`, chaque armature joue une vitrine NLA de toutes ses actions. Les marqueurs de la timeline indiquent les segments : Repos, Marche, Course, Attaque / Tir / Parole, Touché, Mort.

## Scène de présentation (`Tiki_Lineup`)
Personnages sur le sable, avec pour l'échelle :
- le bean joueur ;
- une silhouette humaine de 1,80 m ;
- la barque actuelle (6,4 m) avec deux beans à bord ;
- une **proposition de barque ×0,75** (4,8 m) ;
- deux cocotiers du kit atoll (6 m et 19 m) et des noix de coco.

Caméras : `CAM_Lineup`, `CAM_Echelle`, `CAM_Barque`. Le ciel est un HDRI CC0 (kloofendal pure sky).

## Reste à faire
- [ ] Bake par créature : dépliage UV et atlas 2K D/ORM/N (normale DirectX), plus un emplacement émissif. Les FBX actuels ont des couleurs unies par emplacement, et les fruits n'emportent pas leurs textures.
- [ ] Import UE : SK, Skeleton, Physics Asset `PA_<Nom>`, sockets `weapon_socket` (sur `weapon_r`), animations sur `SKEL_<Nom>`, BP de test à côté d'un PlayerStart et d'un cocotier (recette §10).
- [ ] Feuilles du Rôdeur : simple face, prévoir un matériau à deux faces dans UE.
- [ ] Planches en pose A pour les variantes (ADR-002, P-04). Les planches actuelles ont les bras le long du corps.
