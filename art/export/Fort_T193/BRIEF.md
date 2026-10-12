# Fort français : brief et sources

## Demande
« crée maintenant un grand bâtiment, un fort français avec les remparts, le port, les tours de garde, les bâtiments
intérieurs, des cages, une prison, un fort habitation, une prison, une salle des coffres avec des lingots d'or, des
pierres précieuses, des sacs de perles, crée une ambiance ultra réaliste, utilise échelle Bean, utilise modèle Tuamotu
île géologie pour modélisation »

## Avertissement historique (fiction assumée)
- La présence française aux Tuamotu date du **XIXe siècle** (protectorat, puis annexion). Un fort royal français du
  XVIIe–XVIIIe sur un atoll est une **fiction** du jeu.
- Aucun fort réel n'est copié : tracé générique de « quadrilatère bastionné » à flancs droits, sans orillons ni
  demi-lunes.

## Échelle Bean (règles appliquées)
| Règle | Valeur | Justification |
|---|---|---|
| Échelle du monde | x0,58 | Bean 1,05 / humain 1,80, comme `galion-anglais` (22 m de coque) |
| Contremarche | 16,5 cm | règle `marae_kit` / `temple-reliquaire` |
| Chemin de ronde | Z 2,97 = 18 marches, 3,0 m de large | canon (1,7 m) + passage 1,3 m |
| Créneau (allège) | 0,66 au-dessus du chemin | l'œil de Bean (~0,78, supposé) passe au-dessus |
| Merlon | 1,32 | Bean (1,05) entièrement à couvert |
| Embrasure à canon | genouillère 0,28 | axe du tube du canon du galion à 0,39 ; bloquée par un UCX invisible |
| Garde-corps intérieur | 0,495 | on ne tombe pas dans la cour, on la voit |
| Portes | 1,0 x 1,8 (porte charretière 2,2 x 2,64) | Bean 0,55 + marges capsule/caméra |
| Étage | 2,64 entre planchers | 16 marches |
| Rampe à canons | 1:5 | marchable dans Unreal, on y pousse un canon |
| Cachots | 1,8 x 3,4 sous 2,31 | Bean tient debout et se retourne |
| Cage au sol | 1,32 libres | Bean tient debout |
| Table du trésor | 0,66 | Bean voit l'intérieur des coupelles |

## Géologie de l'atoll (modèle Tuamotu)
- Atoll bas, sans relief volcanique : le motu culmine à environ +1,6 m au-dessus du lagon, avec un bourrelet de tempête
  de blocs de corail côté océan. Coupe nord → sud : front récifal et crête (surf), platier (feo) à fleur d'eau,
  plage océane de débris coralliens et beachrock, cocoteraie, fort, esplanade, quai, lagon turquoise. Une passe
  profonde se trouve à l'est (voir `params.site.profile`).
- Matériaux du récif : moellons de corail mort et de conglomérat (feo), pierre de taille corallienne, dalles de
  beachrock, chaux de corail (mortier, enduit blanc).
- Matériaux importés : briques et pierres de lest (arrivées comme lest des navires), tuiles, chêne, ferronnerie.
- Bois locaux : cocotier et tamanu (*Calophyllum*).
- Les choix liés à la nappe : il n'y a pas de cave profonde (une lentille d'eau saumâtre se trouve à environ 1,6 m). La salle
  des coffres est donc une **casemate** prise dans le remblai du bastion SO, plancher à -0,99. Le fossé est **sec**,
  avec des flaques saumâtres. La citerne est alimentée par l'eau de pluie des toits.
- Assets réutilisés : `art/archives/environnements/atoll_tuamotu` (couches de sol, végétation, blocs de corail, eau,
  ciels).

## Sources
| Source | Utilisation | Statut |
|---|---|---|
| Bénard, planches de pavillons (1762), via Flag Heritage Foundation : https://www.flagheritagefoundation.org/download/signa_mundi/flag_charts_of_past_times__tableaux_de_drapeau_des_temps_passes/Benard-1762-pl-17.pdf | pavillon blanc des vaisseaux du roi ; pavillon royal blanc semé de fleurs de lys d'or avec les armes de France | **consulté par extrait de recherche**, planche non relue en détail |
| Article sur les pavillons de la marine marchande en Nouvelle-France : https://heraldicscienceheraldique.com/les-pavillons-de-la-marine-marchande-en-nouvelle-france.html | ordonnance de 1689 : pavillon marchand bleu à croix blanche (non retenu) | extrait de recherche, non relu |
| Encyclopaedia Britannica, 1re édition (1771), article fortification : https://jimclifford.ca/early_encyclopedia_britannica/articles/1771/eb_1st_1771_006749.html | parapet de 6 à 7,5 pieds au-dessus du terre-plein ; banquette environ 4 pieds sous le sommet du parapet (x0,58 → merlon 1,32, créneau 0,66) | extrait de recherche, ordre de grandeur |
| Réseau des sites majeurs de Vauban, glossaire : https://sites-vauban.org/en/resources/glossary/bastion | vocabulaire du bastion (faces, flancs, gorge) | extrait de recherche |
| Musée du Génie (Angers), fiche fortification : https://www.musee-du-genie-angers.fr/fpdb/12783240-fichfortpol.pdf | vocabulaire | **non relu** |
| Magasins à poudre « type Vauban » : voûte épaisse, contreforts, évents en chicane, mur d'isolement, ferrures en cuivre ou bronze | magasin à poudre | **de mémoire, non vérifié** dans cette session |
| Lingot coulé (environ 25 x 8,5 x 4,5 cm), louis d'or (environ 24 mm) | butin | **ordres de grandeur non vérifiés** |
| Géologie des atolls (motu, platier, beachrock, feo, lentille de Ghyben-Herzberg, micro-marée) | site | connaissances générales + archive `atoll_tuamotu`, **non vérifié** avec une source dans cette session |

## Volontairement fictif ou simplifié
- **Nom du fort** : à définir (proposition : « Fort Saint-Louis-des-Perles »).
- **Frontispice** : armes de France (écu à trois fleurs de lys, couronne simplifiée) en bas-relief, sans devise ni date.
- **Pavillon** : blanc semé de fleurs de lys d'or, **sans** les armoiries complètes (variante : blanc uni). Pas de
  tricolore, qui serait anachronique.
- **Butin** : poinçons et frappes des pièces et lingots illisibles et génériques, sans légende ni armoiries réelles.
- **Galion** : le galion anglais du projet, au mouillage. Un navire français reste à prévoir si le scénario le demande.

## Contraintes techniques
- Machine avec environ 7 Go de RAM libres et un dossier OneDrive. Le build se fait **par lots en sous-processus Blender**
  (`params.build`, `params.lots`, `FORT_BUILD`). La scène est assemblée en liant les `modules/<lot>.blend`.
- Textures : 2K au maximum, 1K pour les petits objets. Pas de bake Cycles. Rendu EEVEE à 32 échantillons, 48 pour les
  intérieurs.
- Chaque module est exporté en FBX et GLB, avec ses UCX_ et SOCKET_. Les portes, la herse, le pont-levis, la grue et
  les cages sont des SM séparés et animables.
