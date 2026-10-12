# Caisse de transport du butin — import Unreal

Généré par `maquettes/caisse-transport/scripts/build.py` (ne pas éditer à la main).

## Fichiers

| Asset | Fichiers | Tris LOD0 / LOD1 / LOD2 |
|---|---|---|
| `SM_LootCrate` | `exports/SM_LootCrate.fbx`, `exports/SM_LootCrate_LOD1.fbx`, `exports/SM_LootCrate_LOD2.fbx` | 11276 / 7514 / 2815 |
| `SM_LootCrate_Lid` | `exports/SM_LootCrate_Lid.fbx`, `exports/SM_LootCrate_Lid_LOD1.fbx`, `exports/SM_LootCrate_Lid_LOD2.fbx` | 2688 / 2054 / 889 |
| `SM_LootCrate_Small` | `exports/SM_LootCrate_Small.fbx`, `exports/SM_LootCrate_Small_LOD1.fbx`, `exports/SM_LootCrate_Small_LOD2.fbx` | 7216 / 4046 / 1887 |

Textures (atlas commun, 2048) : `textures/T_LootCrate_BaseColor.png` (sRGB), `T_LootCrate_Normal.png` (DirectX), `T_LootCrate_ORM.png` (Linear : R = AO, G = Roughness, B = Metallic).

## Import

1. Importer `SM_LootCrate.fbx` (Static Mesh, Import Uniform Scale 1.0, **Combine Meshes OFF**, **Auto Generate Collision OFF** : les 5 `UCX_SM_LootCrate_0x` sont lus tels quels -> intérieur creux). Sockets importés automatiquement.
2. Static Mesh Editor > LOD Settings > Number of LODs 3, puis *Import LOD Level 1/2* avec `SM_LootCrate_LOD1.fbx` / `_LOD2.fbx` (screen size conseillé 0.5 / 0.2). Idem pour `_Lid` et `_Small`.
3. Matériau : `MI_LootCrate` (BaseColor, Normal, ORM ; `T_LootCrate_ORM` sans sRGB).
4. Animations : importer `exports/anims/A_Bean_Crate_*.fbx` sur le squelette **SK_Bean_Body** (Import Animations ON, Mesh OFF). Ajouter les notifies listés ci-dessous.

## Capacité

- Volume utile : **78.0 L** (intérieur 0.744 x 0.394 x 0.266 m).
- Emplacements : **30 slots** (5 colonnes (X) x 3 rangs (Y) x 2 couches (Z)) : `SOCKET_Slot_00` … `SOCKET_Slot_29` (pivot de l'objet posé au fond de la case ; couche 1 = au-dessus de la couche 0).
- Masse à vide estimée : **12.71 kg** (+ couvercle 4.05 kg) ; charge max conseillée : **25 kg**.
- Vitesse de marche conseillée (MaxWalkSpeed x) selon la charge : {'vide': 0.85, '<= 8 kg': 0.75, '<= 16 kg': 0.65, '<= 25 kg': 0.55}.

## Sockets (SM_LootCrate)

| Socket | Usage |
|---|---|
| `Grab_L` | main gauche de Bean (centre de la mitaine) sur la corde de la face arrière (+Y, côté ventre) ; X -> vers la caisse |
| `Grab_R` | main droite de Bean (centre de la mitaine) sur la corde de la face arrière (+Y, côté ventre) ; X -> vers la caisse |
| `Carry` | point d'appui contre le ventre de Bean en portage (X -> vers le porteur) ; mode simple : attacher ici un socket squelette CrateCarry |
| `BoatSlot` | pivot de pose : à aligner sur un SOCKET_Slot_## de la barque (plancher), grand axe parallèle à la quille |
| `Lid` | pivot du couvercle SM_LootCrate_Lid (dessous du plateau) quand il est posé |
| `LootZone` | centre du volume TRG_LootZone (BoxComponent du Blueprint) |
| `Slot_00` | emplacement couche 0 rang 0 colonne 0 (pivot d'objet posé au fond de la case) |
| `Slot_29` | emplacement couche 1 rang 2 colonne 4 (pivot d'objet posé au fond de la case) |

`TRG_LootZone` : BoxComponent attaché à `SOCKET_LootZone`, extent (cm) [37.2, 19.7, 15.8], OverlapAllDynamic + Generate Overlap Events.

## Animations de Bean

| Animation | Durée | Boucle | Notifies |
|---|---|---|---|
| `A_Bean_Crate_Pickup` | 1.5 s | non | `AN_CrateAttach` @ 0.542 s |
| `A_Bean_Crate_Carry_Idle` | 2.0 s | oui | — |
| `A_Bean_Crate_Carry_Walk` | 1.792 s | oui | `AN_Footstep_L` @ 0.0 s, `AN_Footstep_R` @ 0.458 s, `AN_Footstep_L` @ 0.917 s, `AN_Footstep_R` @ 1.333 s |
| `A_Bean_Crate_PutDown` | 1.417 s | non | `AN_CrateDetach` @ 0.875 s |
| `A_Bean_Crate_LoadBoat` | 2.208 s | non | `AN_CrateDetach` @ 0.833 s, `AN_CrateToBoatSlot` @ 0.833 s |

## Recette Blueprint `BP_LootCrate`

Composants : `StaticMesh` (SM_LootCrate, Simulate Physics ON au sol, masse = vide + contenu), `LidMesh` (SM_LootCrate_Lid, optionnel), `LootZone` (BoxComponent sur `SOCKET_LootZone`). Variables : `Contents` (array d'acteurs loot), `SlotUsed` (12 bools), `TotalMass`.

1. **Déposer du butin** : `OnComponentBeginOverlap(LootZone)` -> si l'acteur est un loot et que la caisse est posée : l'ajouter à `Contents`. Deux modes : *physique libre* (le loot reste simulé, les UCX creuses le contiennent ; activer CCD sur les petits objets) ou *rangé* (premier `SOCKET_Slot_##` libre : `AttachToComponent(Mesh, Slot, SnapToTarget)`, physics OFF).
2. **Ramasser** (Bean) : placer Bean derrière la caisse (côté +Y, face à elle, centre de Bean à ~0,5 m du centre de la caisse) ; il saisit les deux cordes de la face arrière (SOCKET_Grab_L/R), entre son ventre et la caisse, jouer `A_Bean_Crate_Pickup` (montage FullBody). À `AN_CrateAttach` : `SetSimulatePhysics(false)` sur la caisse **et** sur chaque loot de `Contents`, attacher chaque loot à la caisse (KeepWorld), puis `AttachToComponent(caisse, BeanMesh, 'hand_r', KeepWorld)`. Main gauche : IK (Two Bone IK) vers `SOCKET_Grab_L` si besoin.
3. **Porter** : `Crate_Carry_Idle` / `Crate_Carry_Walk` en couche haute (Layered blend per bone sur spine_01) ; `MaxWalkSpeed = Base x multiplicateur(charge)`, saut désactivé, pas de sprint.
4. **Poser** : `A_Bean_Crate_PutDown` ; à `AN_CrateDetach` : `DetachFromActor(KeepWorld)`, `SetSimulatePhysics(true)` sur la caisse ; le loot reste attaché (ou réactiver sa physique pour qu'il se tasse).
5. **Charger dans la barque** : si une barque est à moins de 1,2 m devant Bean : `A_Bean_Crate_LoadBoat`. À `AN_CrateDetach` + `AN_CrateToBoatSlot` : détacher, puis interpoler (Timeline 0,6 s, courbe en cloche +0,1 m) la caisse vers le premier `SOCKET_Slot_##` libre de `BP_Barque` (SOCKET_BoatSlot de la caisse = pivot, grand axe parallèle à la quille), puis `AttachToComponent(BarqueMesh, Slot, SnapToTarget)` : la caisse et son contenu voyagent avec la barque (physics OFF tant que la barque bouge).

Barque (BP_Barque, x0.75) : la caisse posée au slot ne touche pas la coque (jeu min. 0.0027 m) ; 1 caisse(s) tiennent simultanément sur les SOCKET_Slot_## existants (SOCKET_Slot_04) ; en rangement libre sur l'axe : 2 en long, 3 en travers (grand axe perpendiculaire à la quille).

## Portage (mesures)

- Caisse en portage : dos à 0.304 m du centre de Bean, fond à 0.39 m, dessus à 0.69 m (bas des yeux de Bean : 0.743 m -> Bean voit par-dessus).
- Portée de bras utilisée : 0.2463 m (repos 0,322 m ; étirement IK max 12 %, comme les Carry_* de la bibliothèque).
