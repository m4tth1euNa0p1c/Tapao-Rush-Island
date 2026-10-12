# 69 — Capsule Steam, key art et visuels de vente
Domaine : G · Auditeur : DA marketing

## Verdict
Aucun visuel de vente n'existe : zéro capsule, zéro key art, zéro capture aux normes Steam, et le seul logo porte un autre nom que le cahier. Le bean et le décor donnent pourtant une image forte (chaloupe rouge, lagon doré). Risque commercial : une page Steam faite de captures d'éditeur.

## Constats

- **[Bloquant commercial] Aucune capture utilisable**
  - Constat : Steam exige 5 captures, 1920×1080 minimum, 16:9, gameplay seul (pas de concept art), dont 4 tous publics. Les 4 captures du dépôt font 1280×720 (éditeur visible) ou 840×615, l'une masquée par un chapeau noir. Les 8 planches `sprite/` sont des références.
  - Preuve : en-têtes PNG (vu), partner.steamgames.com/doc/store/assets/standard.
  - Recommandation : capturer le build `build/rec-T122` en 3840×2160, HUD de debug coupé.
  - Effort : M

- **[Bloquant commercial] Un logo, trois noms, illisible en petit**
  - Constat : le seul logo (`logo.png`) dit « TAPAO : Rush Island », le cahier « Crownfall Expeditions », le projet « First Person Template ». Test fait : à 184×69 le sous-titre est illisible, à 120×45 le lettrage bois n'est qu'une tache ; son ratio 1,44:1 ne remplit que la moitié du small (2,65:1).
  - Preuve : ui_manifest.json:3, DefaultGame.ini:3, cahier l.1.
  - Recommandation : nom unique (voir 78), puis logotype horizontal ≥ 3:1, 1 à 2 mots, aplats sans texture, contraste ≥ 4,5:1.
  - Effort : M

- **[Majeur] 11 formats Steam, rien de planifié**
  - Constat : header 920×430, small 462×174, main 1232×706, vertical 748×896, library 600×900 et 920×430, hero/bannière 3840×1240 (sans texte, zone sûre 860×380), logo 1280 px, icônes 184×184 et 256/512, fond 1438×810. Aucun lot ne les porte.
  - Preuve : partner.steamgames.com/doc/store/assets/libraryassets, cahier l.26, grep « key art », « page Steam » : 0.
  - Recommandation : master 6000×4000 en calques, sujet dans le rectangle central 44 % × 48 % pour survivre aux 8 recadrages, brief `docs/art/briefs/key-art.md`.
  - Effort : M

- **[Majeur] Moments : six montrables, trois absents**
  - Constat : montrables : chaloupe au soleil bas (T-095), village japonais (T-102), galion et barque (`galleon_pie.png`), marae (T-075), tiki tranchés au sabre (T-072) ; barque surchargée qui coule (T-039) jamais capturée, retour visuel pauvre (rapport 24). Absents : sermon (hors `main`, T-054), tempête sans pluie, vente (T-114). Le « bean ridicule » n'a que le ragdoll sur impact, le ragdoll actif reste à faire (T-027).
  - Preuve : storm-flood.md:104, board.md:34,61,121.
  - Recommandation : capsule : bean, chaloupe, horizon (cadrage T-095). Vertical : bean devant un masque tiki. Hero : galion au crépuscule. Key art : chaloupe surchargée, relique, île qui sombre. Village : captures seulement, lecture confuse (supposé).
  - Effort : M

- **[Majeur] Lisibilité à petite taille : aucune règle**
  - Constat : le cadrage T-095 recadré à 184×69 se réduit à une tache rouge et noire, visière et visage disparaissent.
  - Preuve : test d'échelle fait ici, doc Steam standard.
  - Recommandation : un sujet ≥ 40 % de la hauteur, trois teintes (rouge #C21F2A, noir, turquoise, sans vert-rouge), logo ≥ 55 % de la hauteur, zéro HUD ni texte, contrôle en gris à 120×45, hero : sujet hors de l'angle du logo.
  - Effort : S

- **[Majeur] Pas de chaîne de capture**
  - Constat : 0 Level Sequence, 0 CineCamera, 0 Niagara dans `Content/MonkeIsland`, pas de mode photo (rapport 18). Le build de recette n'a aucun contrôle visuel, son costume Redcoat perd son matériau (`MI_Bean_SleeveRed`).
  - Preuve : v1-rec-investisseurs.md:30,41.
  - Recommandation : corriger le matériau, puis acteur de capture (caméras fixes, HUD coupé) et Movie Render Queue.
  - Effort : M

- **[Majeur] Contenu à cadrer**
  - Constat : capsules « PG-13 », 4 captures tous publics : ni découpe ni cadavres (T-072) dans le key art. Planches = images générées (cahier §10), références seulement ; Steam veut l'IA du contenu livré déclarée (marketing non cité, déclarer par prudence). Le bean évoque Fall Guys (supposé), tricorne, tiki-coco et galion le différencient.
  - Preuve : partner.steamgames.com/doc/store/assets/rules, partner.steamgames.com/doc/gettingstarted/contentsurvey.
  - Recommandation : interdits dans le brief, décision IA avant la page.
  - Effort : S

- **[Mineur] Icône et splash absents**
  - Constat : `Build/Windows` ne contient que `FileOpenOrder`, aucun `.ico` dans le dépôt.
  - Preuve : listing `monke_island/Build/Windows`, partner.steamgames.com/doc/store/assets/community.
  - Recommandation : logomark = tête du bean (tricorne, visière), lisible à 16 px, en .ico, JPG, PNG.
  - Effort : S

## Manques
1. Nom unique, logotype horizontal, logomark.
2. Master key art en calques, 8 recadrages.
3. 8 captures 4K propres, dont 2 avec le HUD TAPAO.
4. Drapeau, pluie de tempête, Percepteur visibles en jeu.

## Quick wins
1. Cinq captures 3840×2160 depuis le build T-122 : chaloupe, village, galion, tiki, marae.
2. Corriger `MI_Bean_SleeveRed` avant.
3. Brief `key-art.md` : palette Couronne (#1E2848, #C21F2A, #C99A3E), interdits, formats.
4. Rendu Blender du key art sur le 2e PC (FBX `art/export/`, atoll archivé).
5. Icône du bean et `Application.ico`.

## Questions qui challengent l'équipe
1. Quel nom sur la page Steam, et qui tranche avant le logo ?
2. Le sermon, votre signature, n'est pas dans `main` : que montre la capsule en attendant ?
3. Le cahier promet l'« inquiétant », vos beans et tiki sont des jouets : comédie ou peur ?
