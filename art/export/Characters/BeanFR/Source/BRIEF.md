# Beans français : ennemis en uniforme (fort des Tuamotu)

## Demande
- « Réalise les Beans français, montre images des Beans en uniforme colonial. »
- Changement de périmètre : 7 skins inspirés de `ref/ref_uniformes.jpg` (lithographie de 1845, 6 fantassins, dans l'ordre de la planche), plus un 7e skin libre : un officier colonial. Tous portent mousquet ET sabre, et ce sont des ENNEMIS qui attaquent le joueur.
- Retour sur le preview : peau beige teinté marron, pas de disques blancs, yeux ronds rouges, moustache « française » et cheveux noirs, avec les mêmes caractéristiques que le Bean. Le Bean britannique de comparaison reste inchangé.

## Base commune (inchangée par rapport au player)
- Corps, mains, pieds, proportions (1,05 m sans coiffure) et squelette `SKEL_Bean` : ils viennent de `scripts/character/bean_soldier.py`, réutilisé **sans modification** via `bean_kit.bean()`. La pondération par distance aux os est la même, donc toutes les animations `A_Bean_*` s'appliquent.
- Visage français : peau `MAT-BeanFR_Skin` (beige caramel #B89470), pas de visière, yeux ronds rouges `MAT-BeanFR_Eye_Red` (légèrement émissifs). Moustache en guidon et cheveux `MAT-BeanFR_Hair` : mèches sur les côtés et la nuque, plus un catogan avec nœud et des boucles « ailes de pigeon » sous les tricornes.
- Les cols droits sont échancrés devant pour dégager la moustache.

## Les 7 skins (ordre de la planche)
| # | id / nom | Inspiration | Points clés |
|---|---|---|---|
| 1 | fr1 `CFM_Fusilier` | Compagnies franches de la Marine, 1730-1760 | justaucorps gris-blanc à parements bleus en botte, poches en travers, veste/culotte/bas bleus, sacoche de cuir brun en bandoulière, tricorne bordé d'or |
| 2 | fr2 `Fusilier_1767` | habit bleu à revers verts | revers, col, parements et veste verts, culotte verte, guêtres blanches, baudrier et ceinturon blancs, tricorne noir |
| 3 | fr3 `Fusilier_Bicorne` | vers 1786-1791 | revers bleus passepoilés de rouge, col droit et parements rouges, buffleteries croisées, guêtres noires montantes, bicorne en bataille à plumet rouge |
| 4 | fr4 `Chasseur_Shako` | vers 1806 | habit bleu ouvert sur gilet vert, culotte verte, épaulettes rouges, shako à plaque dorée et pompon rouge |
| 5 | fr5 `Grenadier` | vers 1806 | bonnet à poil (fourrure procédurale) à plumet rouge, pantalon bleu, épaulettes, col et parements rouges |
| 6 | fr6 `Sergent` | vers 1812 | shako à plumet et cordon rouge, pantalon à bande rouge, 2 galons d'argent au-dessus du parement |
| 7 | fr7 `Officier_Marine` | officier des troupes de la Marine, 1750-1770 (choix libre) | habit bleu de roi, parements et retroussis rouges, devants galonnés d'or, boutons dorés, épaulette d'or, hausse-col doré à médaillon d'émail bleu, écharpe de soie blanche, veste et culotte de lin blanc (adaptées au climat des Tuamotu), bottes, tricorne galonné d'or |

**Cocarde blanche** (royauté) sur toutes les coiffures, y compris les tenues inspirées de la fin du siècle : c'est un choix assumé, la planche de 1845 montrant des uniformes de plusieurs époques. Les couleurs sont sobres (laines teintes, avec une légère usure procédurale au rendu, retirée des exports).

## Armes et sockets
- Le mousquet est celui de `bean-musket` (`musket_redesign.py`). Dans cette scène, la petite couronne britannique du platine est retirée et l'ancre est gardée (emblème aussi de la Marine royale). Une baïonnette à douille neuve (`SM_Bean_Bayonet`, lame de 0,215 m, pivot à la bouche) est fixée au socket `muzzle`.
- Le sabre et le fourreau viennent de `bean-cutlass` et `bean-scabbard`. Ils reprennent les sockets de `bean-anim-library/attach_sabre.json` (pelvis/scabbard, pelvis/sabre_sheathed, hand_r/sabre_grip).
- Mousquet en main : il est calculé pour chaque pose (grip_r au centre de la mitaine droite, canon vers la mitaine gauche). Dans les rigs source, le mousquet a son propre os animé, donc la relation main-mousquet varie selon le clip.
- Nouveau socket `spine_02/musket_back` : le mousquet passe dans le dos quand le sabre est tiré.

## Animations ennemies
- Réutilisées telles quelles (même squelette) : Idle, Walk, Run, Musket_Hold/Aim/Fire/Reload/Lower, Sabre_Draw/Idle/Slash_R/Slash_L/Thrust/Finisher/Block/Sheathe, Hit_React, Death.
- Nouvelles (`exports/anims/A_BeanFR_*.fbx`), composées à partir des FBX livrés :
  - `Idle_Guard` : port d'armes avec respiration ;
  - `Patrol` : jambes de la marche et haut du corps au port d'armes, à 0,49 m/s ;
  - `Bayonet_Thrust` : depuis la visée, on arme, on se fend, on revient ;
  - `Sabre_Combo` : Slash_R puis Slash_L puis Thrust, avec des fondus de 3 images.
- Manifeste et note Unreal : `ue/BeanFR_EnemyManifest.json` et `ue/BEAN_FR_ENEMY.md`.

## Rendus
`renders/preview.png` (les 7 skins dans l'ordre de la planche, puis le Bean joueur), `turnaround.png` (face, profil, dos), `buste_officier.png`, `combat.png` (tir, baïonnette, sabre contre le Bean joueur), `viewport.png` (Workbench, équivalent headless de la vue 3D).
