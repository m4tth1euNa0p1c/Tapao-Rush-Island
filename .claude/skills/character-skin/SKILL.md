---
name: character-skin
description: Ajoute ou modifie un skin / cosmétique de personnage (couleur du corps, tenue, chapeau) dans le système modulaire façon R.E.P.O. — modèle Blender de la pièce, DataAsset DA_<Perso>Skin_*, application et réplication, vérification à 2 clients. À utiliser pour « nouveau skin », « nouvelle tenue », « nouveau chapeau », « autre couleur ».
argument-hint: "<slot: body|outfit|hat> <nom> [planche ou couleur]"
---

# Skin / cosmétique

Demande : $ARGUMENTS — système : `docs/features/character-bean-soldier.md` §3 ; agent : `ue-cosmetics-programmer` (code) et `ue-character-artist` (pièce 3D).

## 1. Quel type de skin
| Slot | Ce qui change | Travail |
|---|---|---|
| body | couleur, motif | seulement un DataAsset (paramètres de matériau) |
| outfit | tenue complète | une collection `SK_<Perso>_Outfit_<Nom>` dans le générateur (même squelette), export, import sur le Skeleton existant |
| hat | chapeau | une collection `SM_<Perso>_Hat_<Nom>` avec pivot au contact, collision simple (corps physique), import en static mesh via le MCP |

## 2. Produire la pièce (outfit / hat)
- Ajouter un `build_outfit_<nom>()` ou `build_hat_<nom>()` au générateur du personnage. Réutiliser `shell`, `ribbon`, `tube`.
- Rendu de contrôle, puis test de pose pour une tenue (skill `character-rig` §1).
- Le chapeau doit rester lisible à 20 m. Il faut aussi vérifier son envergure par rapport à la capsule, pour qu'il ne se coince pas dans les portes.

## 3. Déclarer le skin
- `DA_<Perso>Skin_<Nom>` (`UBeanSkinDefinition`) : slot, mesh (soft), paramètres de matériau, nom affiché (String Table `ST_Text_Cosmetics`).
- L'enregistrer dans la liste des skins disponibles (Asset Manager : type primaire `BeanSkin`).

## 4. Vérifier
- PIE à 2 clients : chaque client choisit un skin différent, et chacun voit le bon skin sur l'autre.
- Chapeau : il tombe et se ramasse en gardant le bon mesh.
- Pas de statistique liée au skin (§10).
- Chantier : testé éditeur / multi-machines, captures.
