# Personnage pilote — Bean « Soldat britannique »

| Champ | Valeur |
|---|---|
| Identifiant | `CHR_Bean_Soldier` (pilote §10 ; trois variantes cosmétiques ensuite) |
| Lot / tâches | L6 « intégration personnages » — T-022 à T-029 |
| Référence | `sprite/player-1.png` (planche : face, profil, dos, 3/4, accessoire) |
| Source Blender | `scripts/character/bean_soldier.py` → `art/blender/characters/bean_soldier.blend` |
| Exports | `art/export/Characters/BeanSoldier/` : `SK_Bean_Body.fbx`, `SK_Bean_Outfit_Redcoat.fbx`, `SM_Bean_Hat_Tricorne.fbx` |
| Rendus de contrôle | `art/_previews/bean_soldier_{face,profil,dos,trois_quarts,turnaround,pose_test}.png` |
| Cible UE | `/Game/MonkeIsland/Characters/Bean/` (`Meshes`, `Materials`, `Physics`, `Anims`, `Data`, `Blueprints`) |
| Statut | modèle pilote et rig de base **faits dans Blender** (rendus) ; rien n'est encore importé dans UE |

## 1. Analyse de la planche
**Lecture des vues.** Les vues face, profil, dos et 3/4 montrent bien la même tenue. Le tricorne est aussi présenté seul, comme accessoire. Écarts avec la règle du §10 : les bras sont le long du corps et non en pose A, et le fond est blanc. C'est acceptable pour un pilote. Pour les variantes, demander des planches en pose A.

**Structure du corps.** Un seul volume en forme de haricot (*bean*), sans cou : la tête et le tronc ne font qu'un.
- Visière ovale crème en haut de la face, avec deux yeux noirs en pilule verticale. Pas de bouche, pas de nez.
  Depuis le 2026-10-07 (demande du propriétaire, « enlève le rond blanc »), le bean joueur n'a plus de visière : ses yeux et sa moustache blonde sont posés sur la peau beige (`build_body(visor=False)`). Le marchand garde la sienne. La moustache a été baissée de 1,5 cm le même jour.
- Moufles bleues sans doigts, avec un pouce en bouton.
- Jambes très courtes, cachées par la culotte et les bottes.
- Silhouette lisible de loin : masse rouge, chapeau noir, croix blanche.

**Proportions mesurées sur la vue de face** (en fraction de la hauteur du corps sans chapeau) :

| Repère | Fraction | Valeur à 1,05 m |
|---|---|---|
| Haut de la tête | 1,00 | 1,05 m |
| Visière (centre / hauteur) | 0,77 / 0,20 | 0,805 m / 0,21 m |
| Col | 0,64–0,68 | 0,665–0,718 m |
| Ceinturon | 0,32–0,36 | 0,335–0,382 m |
| Bas de la veste | 0,16 | 0,17 m |
| Haut des bottes | 0,14 | 0,145 m |
| Largeur du corps | 0,38 | 0,40 m (profondeur 0,36 m) |
| Envergure du bord du tricorne | 0,63 | 0,66 m |

**Tenue, du dessous vers le dessus :**
- culotte crème ;
- bottes brun foncé à boucle laiton ;
- gilet crème, visible entre les revers et sous le ceinturon ;
- habit rouge à basques avec fente au dos, ouvert sous le ceinturon, retroussis crème ;
- revers bleu marine avec deux rangées de 4 boutons laiton ;
- col bleu marine ;
- parements bleu marine à 2 boutons ;
- baudriers croisés crème, en X devant et derrière ;
- ceinturon en cuir à boucle laiton ;
- giberne en cuir sur la hanche arrière droite, avec rabat et bouton.

**Tricorne** en feutre noir, galon beige, cocarde bleu marine à bouton laiton sur le côté gauche.

**Palette** (sRGB) : peau `#4FA3E0`, visière `#F2E9DA`, yeux `#141418`, habit `#C21F2A`, bleu marine `#1E2848`, crème `#ECE2C8`, laiton `#C99A3E`, cuir `#4B2D1D`, bottes `#2A1B13`, feutre `#1B1B1E`, galon `#C8AD78`.

## 2. Échelle
La consigne est « la taille d'un Minion à côté d'un humain ». Un Minion mesure environ 1,05 m à côté d'un adulte de 1,80 m, soit un rapport de 0,58.
- **Corps : 1,05 m. Avec le tricorne : 1,10 m.**
- Un cocotier adulte de la map fait 15 à 25 m, soit environ 15 à 20 fois le bean. Un cocotier jeune (`SM_CoconutPalm_Young_*`) lui arrive à hauteur d'épaule ou de tête.
- Capsule UE proposée : rayon 24 cm, demi-hauteur 53 cm. La valeur ira dans `DA_BeanPhysics` (taille de capsule, échelle de vitesse), jamais en dur.
- Écart avec le §10 (« taille humaine cohérente avec la capsule ») : décision du propriétaire, consignée dans l'ADR-002.

## 3. Modularité des skins (façon R.E.P.O.)
Toutes les pièces partagent un même squelette `SKEL_Bean` ; on change de skin en remplaçant des pièces.

| Emplacement | Contenu pilote | Variantes prévues |
|---|---|---|
| Corps | `SK_Bean_Body` + `MI_Bean_Skin` (couleur) | toute couleur, motifs (paramètre de matériau) |
| Tenue | `SK_Bean_Outfit_Redcoat` (Leader Pose sur le corps) | marin, officier, indigène fictif… |
| Chapeau | `SM_Bean_Hat_Tricorne` sur le socket `hat_socket` (os `hat`) | bicorne, bonnet, sans chapeau |
| Visage | yeux / visière (matériau) | expressions par texture (hors V1) |

- Données : `UBeanSkinDefinition` (PrimaryDataAsset `DA_BeanSkin_*`), contenant le slot, le mesh et les paramètres de matériau.
- Le choix est fait dans le lobby du galion (§lobby), répliqué par le `PlayerState`, et appliqué sur tous les clients.
- Les skins sont purement cosmétiques : mêmes statistiques pour tous en V1 (§10).

## 4. Squelette `SKEL_Bean`
`root` → `pelvis` → `spine_01` → `spine_02` → `head` → `hat`. Sous `spine_02` : `upperarm_l/r` → `lowerarm_l/r` → `hand_l/r`. Sous `pelvis` : `thigh_l/r` → `calf_l/r` → `foot_l/r`, et `coattail_l/r`.
- Gauche = +X dans Blender ; le personnage regarde −Y dans Blender, donc +Y dans UE, comme le mannequin.
- Sockets à créer dans UE : `hand_r_socket`, `hand_l_socket`, `back_socket` (`spine_02`), `hat_socket` (`hat`).
- Poids : calculés par distance aux os, limités à certains os selon la pièce (bras, jambes, tronc, chapeau 100 % `hat`). C'est déterministe et ne connaît pas les échecs de « bone heat » sur des vêtements en couches.
- Test de pose (`bean_soldier_pose_test.png`) : visière, gilet, baudriers et chapeau suivent bien.

**Défauts connus, pour T-025 :**
- le col reste rigide quand la tête penche ;
- la jonction épaule/manche s'ouvre quand le bras est levé ;
- les axes des os ne sont pas encore normalisés pour le retargeting.

## 5. Physique et démarche (façon Fall Guys)
Principe : un **ragdoll actif**. L'animation donne une pose cible, et la physique la suit plus ou moins fermement, ce qui produit le côté mou et dandinant.

| Élément | Implémentation UE | Valeurs dans `DA_BeanPhysics` |
|---|---|---|
| Physics Asset `PA_Bean` | capsules : pelvis, spine_01, spine_02, tête (grosse), bras, mains, cuisses, pieds ; chapeau à part | masses (corps total ≈ 25 kg), amortissements |
| Wobble permanent | `UPhysicalAnimationComponent` sur `spine_01` et en dessous, profil « Wobbly » (raideur faible sur le haut du corps et les bras, forte sur les jambes) | `OrientationStrength`, `AngularVelocityStrength`, `PositionStrength`, poids du blend physique |
| Démarche dandinée | AnimBP `ABP_Bean` : balancement latéral du bassin, roulis du corps selon la vitesse, inclinaison avant à l'accélération, petits pas rapides | amplitude et fréquence du dandinement, inclinaison max |
| Ragdoll à l'impact | au-delà d'un seuil d'impulsion ou de chute → simulation complète (`SetSimulatePhysics` sous `pelvis`) ; capsule figée | seuil d'impact, durée minimale au sol |
| Relevé | quand le corps est presque immobile : snapshot de pose, puis blend vers le montage « se relever » face au sol ou au ciel | vitesse de repos, durée du blend |
| Chapeau physique | corps rigide propre attaché au socket par une contrainte cône (oscillation). Il se détache au-delà d'un seuil, devient un objet ramassable et revient au socket au ramassage. | angle du cône, raideur, seuil de détachement |
| Basques | os `coattail_l/r` en corps simulés, contraints et amortis (vêtements simulés : écart §10, voir ADR-002) | angle max, amortissement |

**Réseau.**
- Le serveur reste autoritaire sur la capsule et sur l'état (debout, ragdoll, relevé), répliqué par un enum.
- Le wobble est cosmétique et local à chaque client.
- En ragdoll, le serveur réplique la position du pelvis, et les clients rattrapent la position par un blend.
- Le chapeau détaché est un acteur répliqué.

**Caméra.** Le cahier impose la 1re personne (E-01, ADR-001). Le bean entier, wobble compris, est vu par les autres joueurs. Localement, on ne montre que les moufles et les bras, dans une vue dédiée. Arbitrage dans l'ADR-002.

## 6. Recette (§10 et propre au bean)
- [ ] Import sans erreur, bonne échelle (1,05 m), pieds au sol, avant sur +X dans le Blueprint.
- [ ] Aucune déformation majeure aux épaules, coudes et genoux sur repos, marche, course, saut, nage, portage et mort.
- [ ] Wobble visible à la marche, sans tremblement à l'arrêt (le corps se stabilise en moins d'une seconde).
- [ ] Ragdoll déclenché par un choc au-delà du seuil, puis relevé automatique ; jamais bloqué sous le terrain.
- [ ] Chapeau : tombe après un coup/ragdoll, reste au sol après le relevé, après plus de 5 s et au-delà de 30 m du joueur ; aucun ramassage au contact. S'approcher, viser et appuyer sur **F — Remettre le chapeau** pour le remettre. Refus à distance, derrière un obstacle, pendant le ragdoll, après la mort ou pour un autre joueur ; réplication vérifiée en hôte + client.
- [ ] Quatre beans affichés simultanément, avec quatre skins différents, et coût mesuré (`/ue-profile`).
- [ ] Corps masqué dans la caméra 1re personne sans disparaître pour les autres ; ombres correctes.

### Chapeau au sol — T-123 (demande du 2026-10-04)

Le ramassage explicite remplace le retour automatique de T-034. `UBeanPuppetComponent`
cache le chapeau porté et crée un `ABeanDroppedHat` physique répliqué, sans durée de vie.
La touche F passe par l'interaction existante ; le serveur vérifie le propriétaire,
le délai de ramassage, la distance réelle au chapeau, la visibilité et l'état du joueur.
Seul ce ramassage détruit l'objet au sol et réaffiche le chapeau porté. Un chapeau
détruit ou éloigné n'est jamais recréé automatiquement ; le départ du personnage
nettoie son objet. `HatReturnDelay` et `HatLostDistance` sont conservés uniquement
pour compatibilité des anciens assets et ne commandent plus aucun retour.

Tests de non-régression : `monke_island.Character.BeanHat` (monde physique et
interaction réels ; ils ne constituent pas une recette réseau hôte/client).

## 7. Nage — T-017 (MVP, ADR-008)

**Clips** (provisoires, recette §10) : `A_Bean_Swim` (brasse en boucle, 1,2 s) et `A_Bean_TreadWater` (nage sur place, 1,6 s : jambes en pédalage, mains qui godillent). 30 i/s, racine fixe, sur `SK_Bean_Body_Skeleton` comme tous les `A_Bean_*`, dans `/Game/MonkeIsland/Characters/Bean/Animations/Library`.
- Générateur : `scripts/character/bean_swim_clips.py` (Blender en `-b`, ouvre `bean_soldier.blend` sans jamais l'enregistrer). Le tronc et les jambes sont posés par angles, les bras par IK à deux os vers une trajectoire du poignet.
- Export : réglages de la bibliothèque, soit armature « Armature », `FBX_SCALE_NONE`, −Y avant / Z haut, os déformants. Le nœud `Armature` à l'échelle 100 se replie dans l'os `root`, comme pour `A_Bean_Idle.fbx` (structure comparée nœud par nœud).
- Sorties : `art/export/Characters/Bean/Anims/A_Bean_Swim.fbx`, `A_Bean_TreadWater.fbx` et le rapport `bean_swim_clips.json`. Aperçus : `art/_previews/bean_swim_brasse.png` (4 phases) et `bean_swim_surplace.png`.
- Import : action `beanswim` (`scripts/character/bean_swim_import_ue.py`), un FBX à la fois. Le script range les clips dans `DA_BeanAnimLibrary` (`Swim`, `TreadWater`) ; rapport dans `monke_island/Saved/T-017/beanswim-import.json`.

**Ligne d'eau.** Les clips sont faits pour une surface à 60 cm au-dessus de la racine (`SwimRootDepth`). Le générateur contrôle chaque image :

| Clip | Yeux | Moustache | Tricorne |
|---|---|---|---|
| `A_Bean_Swim` | ≥ 9,5 cm | ≥ 8 cm | ≥ 29 cm |
| `A_Bean_TreadWater` | ≥ 11,8 cm | ≥ 9,7 cm | ≥ 32 cm |

Sur les deux clips, la caméra 1re personne reste devant le visage.

**En jeu** (`UBeanAnimDriverComponent`, état `Swim`) :
- **Entrée dans l'eau.** Le bean prend la pose d'eau dans deux cas :
  - il nage (`MOVE_Swimming`) ;
  - l'eau aux pieds de la capsule atteint `SwimPoseMinDepth` (75 cm), qu'il marche ou qu'il tombe.

  Il en sort 10 cm plus bas. Un saut hors de l'eau (montée de plus de 300 cm/s) passe au saut.
- **Pourquoi la profondeur et pas le mode seul.** La capsule du bean mesure 110 cm, mais `SwimStartDepth` reste à 120 cm (valeur d'adulte). Elle flotte donc au-dessus de son seuil et oscille entre nage et chute vers 120 cm. C'est une lecture du code, à confirmer en PIE.
- **Choix du clip.**
  - Sur place jusqu'à `TreadMaxSpeed` (20 cm/s), brasse complète dès `SwimFullSpeed` (70 cm/s), fondu entre les deux.
  - Cadence = vitesse / `SwimStrokeSpeed` (120 cm/s), bornée entre 0,7 et 1,6.
  - Entrée et sortie de l'eau en `SwimBlendTime` (0,35 s).
  - Dans l'eau, la couche haut du corps (sabre, torche, trompette) s'efface et les one-shots corps entier sont coupés.
- **Flottaison.** `UBeanPuppetComponent::SetWaterStance` place la racine à `SwimRootDepth` sous la surface, vagues et inondation comprises. Elle monte au plus de `SwimMaxLift` (75 cm) au-dessus des pieds de la capsule ; plus bas, en plongée, le bean suit la capsule. Sans clip de nage, l'ancien `SwimLift` (60 cm en nage) s'applique.
- **Réglages** dans `DA_BeanAnimLibrary`, catégorie *Swim*.
- **Spec** : `monke_island.Bean.Swim`.

**Défauts connus :**
- pas de clip de plongée : en plongée, le corps garde la brasse ;
- `SwimStartDepth` (120 cm) reste celui d'un adulte : le bean marche dans 75 à 120 cm d'eau en pose de nage. Proposition : environ 75 cm sur `BP_IslanderBean`, décision de gameplay non prise.
