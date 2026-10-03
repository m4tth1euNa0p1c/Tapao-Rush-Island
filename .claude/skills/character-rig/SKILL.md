---
name: character-rig
description: Rig, skinning, test de pose, export FBX skeletal et import Unreal d'un personnage modulaire (squelette partagé, Physics Asset, sockets, animations provisoires). À utiliser après character-model ou pour corriger des déformations.
argument-hint: "<personnage>"
---

# Rig et import d'un personnage

Personnage : $ARGUMENTS — exemple : `scripts/character/bean_soldier.py` (`bone_table`, `allowed_bones`, `weight_and_bind`, `export_fbx`).

## 1. Squelette et poids (Blender)
- `bone_table()` : noms façon UE, `root` à l'origine, `pelvis` au bassin. Tous les os déforment sauf `root`. Os d'accessoire : `hat` sous `head`, `coattail_l/r` sous `pelvis`.
- `allowed_bones()` : restreindre chaque pièce à ses os (bras, jambes, tronc, chapeau 100 % `hat`). La visière et les yeux suivent les **mêmes os que le corps**, sinon ils se décollent.
- Test de pose obligatoire (script de pose : bras levés, buste penché, pas en avant, tête tournée), rendu dans `art/_previews/<nom>_pose_test.png`. Vérifier épaules, coudes, genoux, col et chapeau.

## 2. Export FBX
- `--export` du générateur : `SK_<Perso>_Body.fbx`, `SK_<Perso>_Outfit_<Tenue>.fbx` (avec armature), `SM_<Perso>_Hat_<Nom>.fbx` (sans armature, pivot au contact).
- Vérifier la présence de skin : `grep -a -c Cluster <fbx>` doit être > 0 pour les SK et égal à 0 pour le SM.

## 3. Import Unreal
- SK : `SkeletalMeshTools.import_file` (MCP) — corps avec `create_physics_asset=true` et `import_materials=true`, pièces suivantes avec `skeleton` = `<SK_Corps>_Skeleton`.
  - chapeau : `StaticMeshTools.import_file`.
  - L'armature doit s'appeler « Armature » dans le FBX (fait par `export_fbx`), sinon UE ajoute un os racine.
- Destination : `/Game/MonkeIsland/Characters/<Perso>/Meshes`. Le Physics Asset est généré à l'import du corps : le renommer `PA_<Perso>` et le déplacer dans `Physics/`.
- Sockets via `ObjectTools` sur le Skeleton : `hand_r_socket`, `hand_l_socket`, `back_socket`, `hat_socket`.
- Contrôle : `CaptureAssetImage` puis BP de test placé à côté d'un PlayerStart et d'un cocotier. Hauteur attendue : 105 cm pour le bean, pieds au sol, avant sur +X dans le BP (mesh tourné de −90° en yaw comme le mannequin).

## 4. Animations provisoires (§10)
Repos, marche, course, saut, nage, portage, mort. Clés procédurales simples dans Blender, exportées sur le même squelette. Le dandinement vient de l'AnimBP et de la physique (`character-physics`).

## 5. Chantier
Note : exporté / importé / testé éditeur, avec captures et défauts restants.
