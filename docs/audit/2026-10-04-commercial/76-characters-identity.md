# 76 — Identité des personnages et cosmétiques
Domaine : H · Auditeur : character designer

## Verdict
Un seul bean existe (une tenue, un chapeau, une couleur, aucun nom), rien dans le code ne le fait varier par joueur (T-028 `a-faire`). Le Percepteur n'a ni fiche ni modèle, et la 1re personne montre à peine le bean à son propriétaire. Risque : quatre amis qui ne se reconnaissent pas, donc ni attachement ni cosmétique à gagner ou vendre.

## Constats

- **[Bloquant commercial] De dos, quatre beans sont indiscernables**
  - Constat : en coop on voit surtout des dos (supposé). Seules deux surfaces se recolorent : la tête bleue (`M_Bean_Skin`, mains et bras FP compris) et le bleu marine partagé (`M_Bean_Navy` : col, revers, parements, épaulettes, cocarde). Deux paramètres rendent donc chaque joueur unique, sans nouveau mesh.
  - Preuve : `bean_soldier.py:338,378,390,424,482,494,550`, `M_Bean_Master.uasset` (paramètre `BaseColor`), `art/_previews/bean_soldier_dos.png`, `BeanPuppetComponent.h:61-67`, aucun PlayerState (voir 14, 18).
  - Recommandation : trois canaux par distance (seuils supposés). Forme du chapeau (> 20 m), couleur de tête et parements (≤ 20 m), plaque de nom (≤ 12 m, modèle `Creatures/MobHealthBarComponent.h:41`). Couleur unique par équipage, attribuée par le serveur, palette type Okabe-Ito sans vert ni rouge, doublée d'un glyphe 1–4 (cahier §18 l.360).
  - Effort : M

- **[Majeur] Catalogue : une planche, zéro variante**
  - Constat : le cahier vise « pilote + 3 variantes ». Seule `sprite/player-1.png` existe, aucune variante dans `maquettes/`, aucun code `Character/Cosmetics/`. 8 couleurs × 6 parements = 48 looks en pure donnée. La fiche prévoit un « indigène fictif » : déguisement culturel sur un atoll réel, à retirer (voir 72).
  - Preuve : cahier §10 l.214, fiche l.65, board T-028.
  - Recommandation : `DA_BeanSkin` d'abord, puis 3 chapeaux (tricorne, bicorne, bonnet) et 3 tenues de corps de la Couronne (aumônier pour le sermon, commis, chirurgien), morphologie identique (§10).
  - Effort : L

- **[Majeur] Chapeau : quatre tricornes noirs identiques au sol**
  - Constat : T-123 (worktree, en cours) laisse le chapeau au sol jusqu'à F par son porteur, sans marque visible de propriétaire.
  - Preuve : board T-123, `BeanDroppedHat.cpp:63` (worktree).
  - Recommandation : cocarde (`Hat_Cockade`) teintée du joueur, nom au survol.
  - Effort : S

- **[Majeur] Le Percepteur, vu à chaque manche, n'a aucune conception**
  - Constat : « visage de la satire » qui estime, taxe et signe. 0 occurrence dans `Source/` et `docs/features/`, ni planche ni maquette au 2e PC (45 dossiers), T-114 `a-faire`. Il est l'entonnoir naturel de l'identité : il lit noms et matricules.
  - Preuve : cahier §24.1-24.2, grep `Percepteur`.
  - Recommandation : fiche `CHR_Bean_Collector`. Silhouette inverse du soldat (noir, gris, perruque poudrée haute, registre, tampon), même `SKEL_Bean` (30 clips), répliques selon 20. Trancher « frapper le Percepteur ».
  - Effort : M

- **[Majeur] Attachement : un soldat jetable sans histoire**
  - Constat : mort définitive, 50 couronnes par recrue : le bean est consommable et rien ne le marque. Sans bouche ni clignement (yeux en meshes), il n'exprime rien.
  - Preuve : board T-104, cahier §24.4 l.557, fiche §1.
  - Recommandation : matricule persistant « Cpl. Léa III » (chiffre romain +1 par mort, nom au registre), décorations de 14 en médailles sur le revers, grade aux épaulettes (déjà modélisées), clignement et yeux écarquillés.
  - Effort : M

- **[Majeur] 76,6 k triangles par bean : le catalogue multiplie un coût hors budget**
  - Constat : corps 14,2 k, tenue 50,0 k, chapeau 12,4 k. Budget §10 : 30–60 k. 4 beans ≈ 306 k. Recette « quatre beans affichés » non cochée.
  - Preuve : `maquettes/bean-soldier/build-report.json` (Blender, UE non mesuré), cahier §10 l.220, fiche l.112.
  - Recommandation : budget par pièce (tenue ≤ 25 k, chapeau ≤ 5 k), LOD imposés, mesure à 4 beans avant la 2e tenue.
  - Effort : M

- **[Majeur] Cosmétiques : valeur perçue faible, profil et droits absents**
  - Constat : 1re personne par défaut, chapeau masqué pour soi : l'acheteur ne se voit presque jamais. Sans profil local, validation serveur ni droits (OnlineSubsystem commenté), rien ne se gagne ni ne se vend.
  - Preuve : `IslanderCharacter.h:94,185`, `BeanPuppetComponent.h:121-133`, `monke_island.Build.cs:48-51`, voir 14.
  - Recommandation : couleur de tête aux mains FP (gratuit), miroir en cabine (voir 20). Cosmétiques gagnés en jeu à l'accès anticipé, puis après la 1.0 un pack payant de tenues des corps de la Couronne (DLC, sans hasard). Jamais de loot box ni de costume culturel réel.
  - Effort : L

## Manques
1. PlayerState (nom, couleur unique, glyphe, loadout répliqué), plaque de nom.
2. Fiche, planche et modèle du Percepteur.
3. Catalogue minimal, vestiaire, budget et LOD des pièces de skin.

## Quick wins
1. Vérifier `Used with Skeletal Mesh` sur `M_Bean_Master` (parent des 12 `MI_Bean_*`) : sinon le build packagé remplace `MI_Bean_SleeveRed` par le matériau par défaut (`docs/releases/v1-rec-investisseurs.md:41`).
2. Teinte provisoire de la tête par `PlayerId` (supposé répliqué nativement) dans `SpawnSkinParts()`.
3. Fiche d'une page du Percepteur, avec la décision sur le sabre.

## Questions qui challengent l'équipe
1. *Soldats jetables* (50 couronnes la recrue) ou *personnage auquel on tient* (cosmétiques) : lequel gagne ?
2. Un joueur frappe le Percepteur au sabre : *rien* tue votre meilleur gag, *la frégate* est votre moment le plus partageable. Qui l'écrit ?
3. Un seul humain dessine les planches : combien de tenues en six mois, et cela justifie-t-il profil, validation serveur et vestiaire ?
