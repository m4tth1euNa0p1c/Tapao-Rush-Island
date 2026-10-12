# 68 — Direction artistique de l'UI TAPAO
Domaine : G · Auditeur : DA UI

## Verdict
Kit TAPAO soigné techniquement (rendus cohérents, 115 sprites, polices OFL), mais habillé pour « TAPAO : Rush Island » (naufragés, bois et corde) : ni Couronne, ni satire, ni Polynésie. Le HUD a déjà quitté la maquette (jauges de 7 px) quand menu et écrans restent massifs : deux dialectes. Risque commercial : la première image porte l'ancien nom, et le look est interchangeable sur Steam.

## Constats

- **[Bloquant commercial] La première image porte l'ancien nom**
  - Constat : le logo (« TAPAO » en bois, bannière « RUSH ISLAND ») ouvre titre, menu et chargement ; version, crédits et confirmation « Quitter » répètent ce nom ; « Crownfall » est absent du DA.
  - Preuve : STapaoScreens.cpp:161,352,405, TapaoUISubsystem.cpp:638, import_tapao_ui.py:39,194.
  - Recommandation : logo simple (couronne + vague), lisible à 120 px en capsule ; version lue du build.
  - Effort : M

- **[Majeur] Charte « naufragés », pas « Couronne »**
  - Constat : bois flotté, corde, laiton (UI_TEMPLATE.md:3). Sur 85 sprites : 0 couronne, sceau, tampon, ancre ni drapeau. Monnaie : pièce « T », « $ » au HUD, « couronnes » au cahier. Fins : « NAUFRAGÉ ! », « ÎLE CONQUISE ! », contre « Contrat rompu… traître à la Couronne » (§24.2). Le bleu du bean et du galion manque à l'UI. Polynésie : audit 72 d'abord.
  - Preuve : TapaoUISubsystem.cpp:511-517, STapaoHUD.cpp:148, character-bean-soldier.md:50, galleon_pie.png.
  - Recommandation : « documents de la Couronne » (parchemin, ruban marine, cire rouge, laiton) ; bois réservé au bord.
  - Effort : L

- **[Majeur] Texte cuit dans 22 sprites**
  - Constat : 13 boutons, 3 onglets et 6 bannières portent leur libellé français ; l'anglais couvre 5 boutons, non importés. Les blancs existent ; seul `btn_wide_blank` (confirmation) est utilisé.
  - Preuve : ui_manifest.json (`label` ×22), import_tapao_ui.py:88.
  - Recommandation : blancs + texte vivant (LOCTEXT, police Titre).
  - Effort : M

- **[Majeur] Cadres 9-slice : épaisseur probablement doublée, écrans non testés**
  - Constat : la skin suppose 70 px (2 000 px × 0,07 × 0,5) ; le moteur calcule marge × taille réelle de la texture : 140 px (lu, non vu). La pause garderait 280 px de parchemin pour des boutons de 360 px. Pause, confirmation, crédits, fin : non testés en jeu.
  - Preuve : ElementBatcher.cpp:857-860 (moteur), TapaoStyle.cpp:31-34, STapaoScreens.cpp:292, board T-106.
  - Recommandation : marges 0,035 / 0,05 ; captures 720p, 1080p, 1440p.
  - Effort : S

- **[Majeur] Typographie et couleurs : pas de système**
  - Constat : Pirata One sert une fois (« CRÉDITS ») ; Alegreya SC (petites capitales) compose aussi les phrases, pénibles à lire, inadaptées aux sous-titres. 6 tailles en dur, 4 couleurs, aucun jeton d'alerte. L'or fait 1,0:1 sur parchemin (encre : 11,4:1) : aucun accent lisible sur le papier. Crème sur le sable du fond : 2,5:1 (échantillon ; indice de campagne : contour de 1 px).
  - Preuve : STapaoScreens.cpp:171,228,478, TapaoUISkin.h:84-109, contrastes calculés sur les PNG.
  - Recommandation : ajouter Alegreya (bas de casse, OFL) ; styles et jetons (encre rouge, marine, alerte) dans le DA, lus aussi par Minuteur et Réglages.
  - Effort : M

- **[Majeur] Menu saturé, fond contraire au cahier**
  - Constat : logo 344 px + 6 boutons de 94 px ≈ 1 000 px sur 1 080 (calcul) ; les 7 entrées du §16 ≈ 1 100 px. Fond = photo floue d'une minka en plein jour sur une carte quasi vide ; le cahier veut un « port au crépuscule avec galion en arrière-plan, animé légèrement ».
  - Preuve : STapaoScreens.cpp:27,138-168, L_MainMenu.umap (49 Ko), cahier §16.
  - Recommandation : colonne à gauche, boutons de 64 px ; scène 3D réelle, caméra lente.
  - Effort : M

- **[Majeur] Animation et son d'UI quasi absents**
  - Constat : deux éléments animés (« Appuyez sur une touche », minuteur) ; ni transition, ni bannière qui se déroule, ni compteur défilant ; chargement factice ; 2 sons de bouton. Signer le registre et la frégate, moments satiriques, n'ont aucun support UI.
  - Preuve : `FCurveSequence` seulement dans SFloodTimer, STapaoScreens.cpp:379,412-417.
  - Recommandation : fondu 0,15 s, tampon (échelle, secousse, son), compteur défilant ; option réduction du mouvement.
  - Effort : M

## Manques
1. Écrans « documents » du Percepteur (contrat, registre, reçu, boutique ; T-114) : les captures les plus vendeuses.
2. 11 pictogrammes (couronne, sceau, tampon, ancre, drapeau, palan, galion, requin, vent, mort, prêt) ; les icônes HUD mêlent aplat et brillant, et passent de 220 à 20 px sans mipmaps (supposé : scintillement).

## Quick wins
1. Remplacer « TAPAO : Rush Island » par le nom retenu et la version du build.
2. Fins de partie : `ribbon_blank` + « CONTRAT ROMPU » / « QUOTA ATTEINT ».
3. Marges du parchemin, puis 4 captures.
4. Fond du menu : galion au crépuscule.

## Questions qui challengent l'équipe
1. Quelle fiction l'UI vend-elle : naufragés en rush (kit) ou satire de la bureaucratie royale (cahier) ?
2. Qui tranche la DA : le 2e PC (Blender) ou ce PC (Slate) ? Le HUD a déjà quitté la maquette ; UI_TEMPLATE.md décrit encore les plaques.
3. Sans le logo, reconnaît-on Crownfall sur une capture du HUD ?
