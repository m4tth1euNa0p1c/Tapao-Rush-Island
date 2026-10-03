---
name: character-pipeline
description: Pipeline complet d'un personnage jouable à partir d'une planche de référence (sprite/*.png) — fiche, modèle Blender, rig, import Unreal, physique façon Fall Guys, skins modulaires façon R.E.P.O., recette §10 — en orchestrant l'équipe personnages. À utiliser pour « fais ce perso », « modélise ce personnage », « intègre ce skin/mob ».
argument-hint: "<sprite/nom.png> [nom du personnage]"
---

# Pipeline personnage

Planche : $ARGUMENTS

Contrat : `docs/CAHIER_DES_CHARGES.md` §10 ; écarts propres au bean : `docs/adr/ADR-002-personnage-bean.md`. Modèle de référence : le bean soldat (`docs/features/character-bean-soldier.md`, `scripts/character/bean_soldier.py`).

| # | Étape | Agent | Skill | Preuve attendue |
|---|---|---|---|---|
| 1 | Analyse de la planche → fiche `docs/features/character-<nom>.md` (vues, proportions, couches de tenue, palette, slots de skin, échelle) | `ue-character-artist` | `character-model` | fiche + tâches au chantier |
| 2 | Générateur Blender `scripts/character/<nom>.py` : corps, tenue, accessoires, matériaux | `ue-character-artist` | `character-model` | rendus face / profil / dos / 3/4 à côté de la planche |
| 3 | Squelette, poids, test de pose, export FBX | `ue-character-rigger` | `character-rig` | `*_pose_test.png`, FBX dans `art/export/Characters/<Perso>/` |
| 4 | Import UE (Skeleton partagé, Physics Asset, sockets), BP de test | `ue-character-rigger` (+ `ue-editor-integrator`) | `character-rig`, `ue-import` | capture UE à côté d'un humain et d'un cocotier |
| 5 | Ragdoll actif, démarche, chapeau physique, relevé, réplication | `ue-physics-animator` | `character-physics` | specs vertes + PIE + profil de 4 persos |
| 6 | Skins modulaires (corps / tenue / chapeau), lobby | `ue-cosmetics-programmer` | `character-skin` | PIE à 2 clients, skins différents |
| 7 | Revue du code et recette §10 | `ue-code-reviewer`, `ue-qa-tester` | `ue-review`, `ue-test`, `ue-playtest` | rapport de recette |

## Règles d'orchestration
- La session principale lance les agents et donne à chacun son ID `T-NNN`. Un fichier Blender, une map ou un Blueprint n'a qu'un seul agent propriétaire à la fois.
- Parallélisable : 5 et 6 dès que le squelette (étape 3) est figé, puisque les noms d'os et de sockets forment le contrat. 1 → 2 → 3 → 4 sont séquentiels.
- Avant toute modification C++ : vérifier au chantier que personne n'a de tâche `en-cours` sur les mêmes fichiers (T-021 touche `IslanderCharacter`). Le code personnage va dans `Source/monke_island/Character/`.
- Personnages suivants (mob, pnj, sniper dans `sprite/`) : réutiliser `SKEL_Bean` si la silhouette est un bean, pour partager animations, Physics Asset et skins.
- Honnêteté : « modèle Blender fait » ≠ « importé » ≠ « testé éditeur » ≠ « testé multi-machines ».
