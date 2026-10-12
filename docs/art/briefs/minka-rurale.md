MINKA RURALE — DESCRIPTION ET PROMPTS POUR CLAUDE CODE / BLENDER
Version 1 — 3 octobre 2026
Image associée : planche « RÉFÉRENCE 3D · BLENDER — MINKA RURALE ».

1. CONCEPT RETENU

Créer une maison rurale japonaise inspirée d'une minka / nōka, destinée à un jeu d'aventure. Interprétation artistique semi-réaliste : silhouettes lisibles, matériaux crédibles, usure légère et intérieur visitable.

La maison est de plain-pied, avec une grande toiture de chaume, une structure de poteaux et poutres en bois sombre, des remplissages d'enduit clair et un plancher surélevé sur des appuis en pierre. Une galerie extérieure en bois, l'engawa, longe la façade des pièces d'habitation et le côté droit.

L'entrée principale, située à gauche de la façade, débouche dans une bande de sol en terre battue, le doma. Depuis cette zone basse, une marche permet d'atteindre le plancher habitable. Deux pièces en tatamis, une pièce commune et une pièce de rangement composent le reste de l'intérieur.

Ambiance : modeste, accueillante, patinée et entretenue. Palette : brun sombre, miel, paille dorée, beige chaud, ivoire et touches vert olive. Aucun personnage n'est requis pour le rendu.

Les proportions et le plan ci-dessous sont des choix de conception pour cet asset de jeu. La planche générée sert de référence visuelle ; les paramètres écrits servent à résoudre ses éventuelles incohérences entre vues.

2. ÉLÉMENTS ARCHITECTURAUX À REPRÉSENTER

- Charpente : poteaux, poutres, traverses et assemblages de menuiserie visibles. Suggérer des tenons, mortaises et chevilles sur les assemblages principaux.
- Toit : chaume épais, faîtage simple et toiture à croupes et pignons d'inspiration irimoya. Géométrie distincte du bâtiment et facilement masquable. Les brins de chaume sont principalement représentés par la texture et les normales.
- Appuis : pierres irrégulières sous les pièces structurelles, avec un vide lisible sous le plancher.
- Doma / entrée : sol bas en terre compactée, porte coulissante en bois, seuil et marche vers les pièces surélevées.
- Tatamis : modules rectangulaires à ratio 2:1, tressage fin, bordures sombres, disposition cohérente.
- Shoji : cadres et treillis de bois fin avec panneaux de papier washi translucide.
- Fusuma : portes et cloisons coulissantes opaques, papier clair, encadrement sobre et petites prises.
- Engawa : galerie de planches, couverte par les débords de toiture, reliant façade et côté droit.
- Tokonoma : alcôve décorative légèrement surélevée dans la pièce arrière droite, avec un rouleau sobre et un petit vase.
- Irori : petit foyer carré encastré dans la pièce commune en bois ; cendres et bouilloire en accessoires facultatifs.

Points de vocabulaire à respecter :
Le sol de l'entrée est plus bas que le plancher intérieur. Pour les shoji, parler de papier washi translucide. Le tokonoma est une alcôve décorative. Les typologies et techniques varient selon les régions et les époques ; le présent asset associe des éléments sélectionnés pour la lisibilité du jeu.

3. PARAMÈTRES DE CONCEPTION POUR BLENDER

Unités de travail : mètres.
Orientation : façade principale vers le sud / -Y ; arrière vers +Y ; droite de la façade vers +X ; verticale +Z.
Origine du bâtiment : angle avant gauche du corps principal, au niveau du sol extérieur.

Paramètres initiaux proposés :
- Corps principal : 9,00 m en X et 7,20 m en Y.
- Trame de conception : 1,80 m ; sous-module 0,90 m.
- Sol du doma : Z = 0,00 m.
- Plancher habitable et engawa : Z = 0,45 m.
- Hauteur des ouvertures : environ 2,10 m au-dessus du sol concerné.
- Largeur libre des passages principaux : viser au moins 0,90 m ; baies doubles de 1,80 m.
- Engawa : largeur 1,20 m.
- Toiture : débords protégeant intégralement l'engawa ; hauteur totale indicative de 6 à 7 m à ajuster avec la pente et le volume du chaume.
- Épaisseur visuelle des poteaux : environ 0,18 à 0,24 m.
- Épaisseur visuelle du chaume : environ 0,35 à 0,50 m.

Ces valeurs sont des paramètres du modèle, pas des standards historiques. Les zones ci-dessous constituent une grille d'implantation ; réserver les épaisseurs des murs et poteaux dans cette grille sans empiéter sur les passages.

Implantation du corps principal :
A. Doma : X de 0,00 à 1,80 ; Y de 0,00 à 7,20. Entrée au sud. Petite zone de travail / cuisson au nord.
B. Pièce commune en planches : X de 1,80 à 5,40 ; Y de 0,00 à 3,60. Petit irori, accès au doma et à l'engawa.
C. Washitsu avant : X de 5,40 à 9,00 ; Y de 0,00 à 3,60. Trame indicative de huit tatamis, table basse et deux coussins.
D. Pièce arrière en planches : X de 1,80 à 5,40 ; Y de 3,60 à 7,20. Coffre, rangement et futon plié.
E. Washitsu arrière : X de 5,40 à 9,00 ; Y de 3,60 à 6,30. Trame indicative de six tatamis.
F. Alcôves arrière : X de 5,40 à 9,00 ; Y de 6,30 à 7,20. Tokonoma dans la moitié gauche, rangement fermé dans la moitié droite.

Engawa en L :
- Bande de façade : X de 1,80 à 9,00 ; Y de -1,20 à 0,00.
- Bande latérale : X de 9,00 à 10,20 ; Y de 0,00 à 7,20.
- Jonction d'angle : X de 9,00 à 10,20 ; Y de -1,20 à 0,00.
- Laisser libre au niveau du sol l'approche de l'entrée du doma, devant X de 0,00 à 1,80.

Circulation :
- Relier le doma à la pièce commune par une ouverture et une marche.
- Relier les quatre pièces par des cloisons coulissantes avec passages effectifs.
- Placer les shoji entre les pièces habitables et l'engawa.
- Conserver une façade et un côté cohérents avec la vue intérieure.
- Si les tatamis ne s'ajustent pas après déduction des épaisseurs, adapter leur module au volume utile en gardant le ratio 2:1. Ne pas les superposer.

4. PROMPT À COPIER DANS CLAUDE CODE

Tu es un artiste technique 3D spécialisé dans Blender et les environnements de jeu. Crée l'asset « Minka_Rurale » à partir de la planche jointe et des spécifications de ce fichier.

OBJECTIF
Produire une véritable maison 3D avec extérieur et intérieur visitable, matériaux et éléments mobiles séparés. Reprendre l'apparence de la planche et suivre les proportions, niveaux et zones écrites ci-dessus.

MÉTHODE
1. Examine l'image et identifie les volumes, matériaux, ouvertures et pièces. Vérifie la version de Blender et les moyens disponibles pour la piloter.
2. Construis un blockout paramétrique en mètres : corps principal, doma, plancher surélevé, pièces, engawa, structure et toiture.
3. Vérifie les passages et l'alignement des baies avant d'ajouter les détails.
4. Modélise le bois, les enduits, les panneaux, le chaume et les accessoires. Utilise des variations discrètes, des biseaux modérés et des matériaux réutilisables.
5. Sépare la toiture pour permettre une vue intérieure et les futures fonctions d'occultation.
6. Sépare chaque panneau coulissant et définis ses positions fermée et ouverte. Assure-toi qu'il peut coulisser sans traverser un poteau.
7. Prépare des rendus cohérents de l'extérieur, des façades et de l'intérieur.
8. Sauvegarde le projet et les exports effectivement obtenus ; signale les étapes qui ne peuvent pas être exécutées dans l'environnement disponible.

ORGANISATION DE SCÈNE
Collections proposées :
Minka_Structure
Minka_Toiture
Minka_Murs
Minka_Planchers
Minka_Engawa
Minka_Shoji
Minka_Fusuma
Minka_Tatamis
Minka_Mobilier
Minka_Collisions
Minka_Presentation

Objets distincts pour la toiture, les panneaux coulissants, les tatamis principaux et les meubles. Origines utiles au déplacement des panneaux. Éviter un unique mesh réunissant tout le bâtiment.

MATÉRIAUX
Bois sombre structurel ; bois plus clair pour les planchers ; enduit terre/chaux ivoire ; chaume ; tatami ; papier translucide des shoji ; panneaux opaques des fusuma ; pierre ; terre battue.
Pour le chaume et les fibres, privilégier les textures et les normales afin de contenir la complexité.
Adapter les matériaux aux formats d'export : les nœuds procéduraux Blender peuvent nécessiter une cuisson en textures. Conserver les originaux éditables dans le .blend.

LIVRABLES DEMANDÉS À CLAUDE CODE
- Minka_Rurale.blend, scène organisée et ressources nécessaires.
- Script Python Blender reproductible, si l'asset est généré par script.
- Minka_Rurale.glb avec matériaux et textures compatibles.
- FBX séparé si le pipeline Unreal du projet l'exige.
- Rendus : trois-quarts extérieur, façade, profil droit, dessus sans toit et intérieur.
- Note brève indiquant dimensions finales, nombre de triangles, textures, éléments mobiles et limites d'export constatées.

VÉRIFICATIONS UTILES
- Le même bâtiment apparaît dans toutes les vues.
- L'entrée basse, la marche et le plancher surélevé sont visibles.
- Les pièces communiquent ; aucun passage n'est condamné par un mesh invisible.
- Shoji et fusuma sont visuellement distincts et coulissent correctement.
- Le tokonoma reste une alcôve décorative.
- Les normales et les matériaux fonctionnent dans Blender et dans l'export testé.
- Les collisions simplifiées respectent les portes, la galerie et les sols.
- Le masque de toiture expose bien toutes les pièces pour l'inspection.
- Contrôler l'échelle lors de l'import dans Unreal, où les distances sont habituellement exprimées en centimètres.

5. PROMPT EXACT DE GÉNÉRATION DE LA PLANCHE IMAGE

Use case: stylized-concept.
Create a beautiful landscape 3:2 3D architectural MODEL REFERENCE SHEET of a traditional Japanese rural MINKA farmhouse, for a semi-realistic stylized adventure game and subsequent modeling in Blender with Claude Code. This is an artistic building-design reference with an illustrative roofless interior view, not a dimensioned engineering drawing.

SUBJECT: one modest single-storey rectangular rural timber house, warm and lived-in, substantial dark weathered wooden posts and beams, warm off-white earth-plaster infill panels, aged wood baseboards, natural irregular stone footings. A large steep thick brown-gold THATCH roof with a restrained traditional hip-and-gable silhouette, deep eaves and a simple bound ridge; no tile roof, no pagoda, no temple ornaments, no bright red torii. Keep the same roof shape and timber structure in every exterior view. Exterior proportions approximately nine units wide to seven units deep. Raised wooden dwelling floor and a narrow L-shaped ENG AWA wooden veranda across the front of the raised living area and along the right side. The left-front entrance accesses a visibly LOWER compacted-earth DOMA floor; make the threshold and step up to the dwelling understandable. Main entrance a simple wooden sliding door at front-left. The front-right rooms open onto the veranda through light timber-grid translucent off-white SHOJI sliding panels, partly open to reveal the interior. Internal FUSUMA partitions are opaque cream panels with simple dark wooden borders. Separate functional panels, no glass curtain walls. Modest age and craftsmanship, fine but readable wood grain, bundled thatch texture, linen-like tatami weave, restrained slight moss on stone bases. Stylized semi-realism with tactile PBR materials and gently softened edges, not a cartoon toy.

CONSISTENT INTERIOR DESIGN in the roofless view: display the front facade toward the bottom of the panel and the rear toward the top. A narrow continuous lower earth-floor service strip occupies the LEFT fifth of the rectangular building, with front-left entry and a small rustic cooking/work area at its rear. The remaining raised wooden dwelling block is divided into four rooms in a two-by-two arrangement: front-left raised wood-floored communal room with a small square sunken IRORI hearth; front-right a rectangular tatami sitting room with a low wooden table and two cushions; rear-left a simple wood-floored storage/sleeping room with a folded futon and a wooden chest; rear-right a second smaller tatami room with a TOKONOMA display alcove at its rear wall, one plain hanging scroll and a tiny flower vase, plus a closed cupboard beside the alcove. The tokonoma is a shallow raised recess, not a passage. Clearly visible sliding partitions connect the rooms. The L-shaped exterior veranda follows the bottom and right edges of the raised living block. Leave the lower left entrance accessible from ground level without crossing the raised veranda. The roofless view shows readable floor levels, timber posts, doors and partitions; keep proportions faithful to the exterior design. Furniture is sparse, no people. No modern appliances or plumbing.

PAGE COMPOSITION: clean white studio background with subtle contact shadows, high-end rendered reference board. Large elevated FRONT THREE-QUARTER exterior hero view at upper left, showing front entry and right veranda. At upper right, two smaller clear orthographic exterior elevations, FRONT and RIGHT SIDE. At lower left, a large straight-down ROOFLESS TOP VIEW showing the whole interior arrangement without roof or ceiling occlusion; keep wall heights cut low enough to read all rooms. At lower right, a close three-quarter interior cutaway view of the tatami room, open shoji, veranda and tokonoma, corresponding to the same house, not a different building. A small unobtrusive pair of isolated shoji and fusuma panels may be placed nearby only if space permits. Carefully aligned layout, generous spacing, no overlapping or cropped buildings, every angle depicts one consistent asset. Consistent neutral daylight studio illumination, sharp focus, no scene background or landscape.

TEXT: upper-left header exactly 'RÉFÉRENCE 3D · BLENDER', subtitle 'MINKA RURALE'. Small clean dark-gray sans-serif captions at their respective views: '3/4 EXTÉRIEUR', 'FAÇADE', 'PROFIL', 'DESSUS · TOIT RETIRÉ', 'INTÉRIEUR'. A few discreet interior labels only where clearly useful: 'DOMA', 'TATAMIS', 'ENGAWA', 'TOKONOMA'. No numerical dimensions, no dense annotations, no watermark. Aim for a handsome coherent semirealistic Japanese farmhouse model sheet, with the main architecture and all requested interior elements visually understandable.

6. APPUI DOCUMENTAIRE

Le choix d'une maison vernaculaire à toit de chaume est notamment inspiré des maisons conservées par le musée municipal Nihon Minka-en. Le musée décrit également la distinction entre les zones de sol en terre et les planchers surélevés. Le plan et les dimensions du présent asset sont une création pour le jeu.

Musée Nihon Minka-en, présentation officielle :
https://www.nihonminkaen.jp/leaflet_english.html

Musée Nihon Minka-en, FAQ officielle — accès aux espaces intérieurs :
https://www.nihonminkaen.jp/faq_english.html

Références consultées le 3 octobre 2026.

