# 62 — Lisibilité et silhouettes en végétation dense
Domaine : G · Auditeur : DA gameplay

## Verdict
Les modèles respectent le cahier (silhouettes lisibles, §1), mais la lisibilité en jeu n'est ni conçue ni capturée. Dans un fourré couvrant 85 % du sol, ni tiki, ni butin de 10 cm, ni équipier n'a contour, éclat ou repère. Risque commercial : un jeu injuste (menaces invisibles), sans plaisir de trouvaille.

## Constats

- **[Bloquant commercial] Le butin commun est invisible**
  - Constat : 4 110 objets de 9–30 cm, teintes terre, parmi fougères de 50 cm et noix brunes. Ni contour, ni éclat, ni son. Invite à 2 m seulement, par rayon sans épaisseur. Un peigne à 5 m couvre ~19×2 px en 1080p (calcul, FOV 90). Armes lâchées (30–50 $) sans repère ; la perle, sur autel à torches, se lit (supposé). Non vérifié en jeu.
  - Preuve : loot-report.json (OneDrive), board.md:115, catalogue_atoll.md:68, IslanderGrabComponent.cpp:157, loot-common.md:49, marae-des-perles.md:15, grep Source et Content=0 CustomDepth.
  - Recommandation : contour stencil ≤ 8 m teinté par valeur, éclat ≤ 15 m, tintement ≤ 6 m, balayage sphérique, repère de 20 s sur l'arme lâchée.
  - Effort : M

- **[Majeur] Ennemis cachés par construction, sans signe hostile**
  - Constat : le naupaka (2,1–3,7 m, un adulte de 1,80 m y disparaît) engloutit des tiki de 0,95–1,30 m. L'embusqué naît à 60 cm du buisson, sans test de champ de vision, et le frémissement du 16 se noie dans la brise permanente. Le Coco brun se fond dans la litière (rendu Blender), seuls Crabe et Veilleur ont un signe fort. À 80 m un tiki fait 12 px (calcul), indiscernable du villageois (supposé) : frapper celui-ci fait surgir 4 tiki.
  - Preuve : README raraka:65-67, TikiDirector.cpp:788/1135, board.md:69, characters-tiki.md:109, tiki_lineup.png.
  - Recommandation : yeux émissifs sur les 3 guerriers, préavis de 1,2 s en son et gerbe de feuilles, pas d'apparition en vue sans préavis, bandeau blanc sur le villageois.
  - Effort : M

- **[Majeur] Barres de vie : fuite possible, puis suppression totale**
  - Constat : le widget, en espace écran, ignore le décor : seul un buisson à moins de 2 m le masque. Depuis le 2026-10-04, `MaxDistance=0` les coupe toutes : plus de retour de vie hors chiffres du sabre.
  - Preuve : MobHealthBarComponent.cpp:40/137-146, characters-tiki.md:237, 04, 22.
  - Recommandation : règle écrite. (1) Rien sur un ennemi non vu (trace caméra→tête, proxy buisson). (2) Barre 3 s après un coup du joueur, jamais sur un neutre. (3) Retour d'abord sur le corps (flash, boiterie, masque fissuré). (4) Allié toujours visible.
  - Effort : S

- **[Majeur] Interactifs sans affordance à distance**
  - Constat : seul indice monde, le point de visée passe de 5 à 9 px et dore, à ≤ 2 m. Seuls barque (lisible), échelle et noix existent ; drapeau, socle, palan et Balance n'ont aucune affordance définie, et TAPAO n'a aucun pictogramme monde. Depuis la mer (T-095/01-02) le rivage est un mur de palmes : rien n'indique les 3 zones.
  - Preuve : IslanderHUD.cpp:167-178, STapaoHUD.cpp:115-219, ADR-005:63-68, islands.md:93.
  - Recommandation : grammaire à 4 rôles (objectif, interactif, butin, hostile) : teinte, forme, pictogramme, son, distances 100/15/6/2 m. Mât de 30 m au-dessus des palmes (14–23 m), fumée de village, boussole (12). Liseré sur tout interactif à 6 m.
  - Effort : M

- **[Majeur] Coéquipiers : quatre clones, rouge sur rouge**
  - Constat : la silhouette se lit bien sur le vert mais se fond dans la coque rouge (T-095/01) ; en fourré, rien ne localise un équipier à 30 m. T-095/03 : chaque vue est bouchée par l'autre bean (supposé : caméra sans anti-clip). Le rouge, déjà celui des beans, ne peut pas signifier le danger.
  - Preuve : character-bean-soldier.md:21, grep BeanSkin=0, captures T-095, 18.
  - Recommandation : contour d'allié à travers le décor, couleur du joueur, au-delà de 8 m. Équipier estompé à moins de 40 cm de la caméra.
  - Effort : M

- **[Majeur] Nuit, preuve et norme absentes**
  - Constat : lune pleine à −1,5 EV, critère nuit lisible non coché. Les 4 captures sont de jour, aucune en fourré ni avec hostile, butin ou drapeau, alors qu'une expédition sur deux voit la nuit (01). Aucune norme ni tâche, alors que L5 exige danger lisible.
  - Preuve : day-night-cycle.md:18/28, cahier §11:235, docs/production/captures, board.md.
  - Recommandation : carte `LV_Readability` (acteurs types, 3 décors, 4 lumières), captures auto, niveaux de gris, deutéranopie, contraste ≥ 3:1, norme `docs/conventions/readability.md`.
  - Effort : M

## Manques
1. Contours stencil, teinte réservée par rôle.
2. Révélation du butin (éclat, tintement, lorgnon : 17).
3. Signes hostiles, blessures lisibles sur le corps.

## Quick wins
1. Invite 0 kg pour 10 butins sur 12 : `%.0f` → 2 décimales (IslanderHUD.cpp:157).
2. Balayage sphérique de 8 cm (IslanderGrabComponent.cpp:157).
3. Réticule à 3 états (hostile, neutre, interactif).
4. 12 captures de référence (fourré, village, jour, nuit) avant tout réglage.

## Questions qui challengent l'équipe
1. Quelle part des 4 110 butins un novice trouve-t-il en cinq minutes de fourré, nuit comprise ? Sous la moitié, la récolte est une loterie.
2. Un tiki né à 60 cm d'un buisson, hors de vue et sans préavis, respecte-t-il le cahier §8 (signal préalable) ?
3. Qui possède la lisibilité, sans tâche ni norme, et quelle teinte signifie quoi, le rouge étant celui des beans ?
