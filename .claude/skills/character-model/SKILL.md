---
name: character-model
description: Modélise un personnage dans Blender à partir d'une planche de référence — mesure des proportions, corps, vêtements en couches (coques décalées), accessoires, matériaux, collections par slot de skin — avec un générateur Python reproductible et des rendus de contrôle.
argument-hint: "<sprite/nom.png>"
---

# Modèle de personnage (Blender)

Planche : $ARGUMENTS — exemple complet : `scripts/character/bean_soldier.py`.

## 1. Lire et mesurer
- Read sur la planche. Noter chaque vue (face, profil, dos, 3/4, accessoires) et vérifier que la tenue est identique partout.
- Mesurer en pixels sur la vue de face (sol, haut du corps, visière, col, ceinture, ourlet, bottes, largeur du corps, envergure du chapeau), puis convertir en **fractions de la hauteur** et en mètres. Bean : 1,05 m sans chapeau (ADR-002).
- Lister les couches de la tenue du dessous vers le dessus, et la palette en hex sRGB.
- Rédiger ou compléter `docs/features/character-<nom>.md`.

## 2. Générer
- Copier la structure de `bean_soldier.py` : `P` (dimensions), `PALETTE`, puis un `build_*` par slot (corps, tenue, chapeau).
- Outils du générateur :
  - `profile()` / `surf()` pour la surface du corps ;
  - `shell()` pour les bandes de tissu (avec `keep` pour les ouvertures et `flare` pour l'évasement) ;
  - `ellipse_patch()` pour la visière et les yeux ;
  - `ribbon()` pour les sangles et les revers ;
  - `tube()` pour les membres ;
  - `blob()` et `box()` pour les pièces dures ;
  - `button()` pour les petites pièces orientées.
- Les vêtements sont des coques à 1–5 cm de la surface, avec Solidify. Ordre des décalages, de l'intérieur vers l'extérieur : gilet < habit < revers < sangles < ceinture.
- Lancer en arrière-plan : `"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --factory-startup --python scripts/character/<nom>.py -- --render`.

## 3. Contrôler
- Assembler les 4 rendus (`art/_previews/<nom>_turnaround.png`) et les comparer à la planche : silhouette, couleurs, placement des couches.
- Défauts typiques :
  - couche qui transperce : augmenter son décalage ;
  - corps visible par une ouverture : prolonger la couche du dessous ;
  - revers trop larges : réduire la largeur du ruban.
- Itérer en modifiant `P`, jamais à la main dans le .blend.

## 4. Ouvrir dans le GUI
- Si `bpy.data.is_dirty` : ne rien écraser, demander.
- Sinon, rouvrir via `bpy.app.timers.register(lambda: bpy.ops.wm.open_mainfile(filepath=...))`.
- Dire à l'utilisateur d'appuyer sur `.` (pavé numérique) pour cadrer le personnage.

## 5. Passer la main
Fiche à jour (chemins, défauts connus) → `ue-character-rigger` (skill `character-rig`).
