# 17 — Outils et objets
Domaine : B · Auditeur : designer objets et outils

## Verdict
Sabre, mousquet et torche sont solides techniquement (serveur-autoritaire, DataAssets) mais offerts gratuitement à tous, à peine essayés, et sans lien avec le vrai travail du jeu : porter du butin. La boutique n'a aucune marchandise : la progression entre manches est vide. Risque commercial : aucun « build » d'équipement ni objet à moment viral.

## Constats
- **[Bloquant commercial] Boutique vide ; la hache n'a plus de raison d'être**
  - Constat : hache et lanterne ne sont que deux prix (voir 05, 12). Bois facultatif, arbres en instances PCG, rien de cassable : la hache ne sert à rien ; la torche fait déjà lanterne.
  - Preuve : `EconomyRules.cpp:7-8` ; cahier §24.4.
  - Recommandation : abandonner la hache ; vendre sac à butin, lanterne mains libres, gaffe, longue-vue, baril de poudre.
  - Effort : M

- **[Majeur] Porter le butin : aucun outil, un objet à la fois**
  - Constat : butin de 5–40 $ (quota 250) porté à la main, 25 kg par joueur ; ramassage en inventaire prévu mais non fait ; aucun outil pour repêcher un objet emporté par l'eau (T-072). Le HUD donne valeur et masse à 2 m seulement (un scan à distance aurait un sens). Grappin inutile : relief de 2–4 m.
  - Preuve : `loot-common.md:25` ; `IslanderGrabComponent.h:48` ; `IslanderHUD.cpp:157` ; `backlog.md:48`.
  - Recommandation : sac à butin (objets ≤ 1 kg en données : moins de physique répliquée, supposé), gaffe, civière à deux.
  - Effort : M

- **[Majeur] Équipement gratuit, identique, inchangeable**
  - Constat : chaque bean démarre avec mousquet, sabre, torche (voir 08) ; aucune API d'ajout ou de dépôt, donc les armes lâchées par les tiki ne se manient pas ; slot 4 vide, mort sans perte (supposé).
  - Preuve : `IslanderInventoryComponent.cpp:60-87` ; board T-099 ; `characters-tiki.md:188`.
  - Recommandation : 4 slots = arme, lumière, outil, consommable, achetés ou trouvés, perdus à la mort.
  - Effort : M

- **[Majeur] Mousquet et sabre : aucune gestion de ressource**
  - Constat : le mousquet tue en un coup à 80 m (10 000 dégâts, demande du 2026-10-03 ; la fiche, reprise par le rapport 16, dit 70). 23 balles jamais recomplétées. Fil du sabre sans enjeu (−20 % sous 50 %). Poire à poudre, silex, baril, pierre à aiguiser : modélisés (2e PC), non intégrés.
  - Preuve : `MusketRules.h:112` ; `import_musket_reload_sounds.py:80-81` ; `musket.md:13` ; `maquettes/bean-powder-horn`.
  - Recommandation : poudre limitée, à recompléter au baril ; silex et fil réparables.
  - Effort : M

- **[Majeur] Torche : inutile, incombinable, incohérente au portage**
  - Constat : nuit éclairée par une pleine lune ; aucune IA ne réagit à la lumière ; 2,4 s avant la flamme, exclut le sabre, 22 dégâts contre 80 PV, 300 s sans recharge, vent sans effet. Seule du trio utilisable en portant un objet : lumière gratuite, clic gauche en conflit (supposé). Aucun test.
  - Preuve : `day-night-cycle.md:18` ; `TorchItem.cpp:109-122` ; `CutlassWeapon.cpp:157` ; `TorchRules.h:112`.
  - Recommandation : main gauche cumulable avec le sabre ; rechargeable aux feux ; vent l'éteint ; refus en portage.
  - Effort : M

- **[Mineur] Trompette : touche V en double, ignorée de l'IA**
  - Constat : V est aussi le changement de vue (supposé) ; fanfare à 60 m sans effet sur l'IA ; icône HUD sans slot ; ni tâche, ni fiche, ni test (rôle : voir 18).
  - Preuve : `import_trumpet.py:189,211` ; `musket.md:24` ; `STapaoHUD.cpp:49`.
  - Recommandation : autre touche ; ping de ralliement.
  - Effort : S

- **[Mineur] Noix et poissons : bornes de soin, pas des objets**
  - Constat : consommés sur place (F), ni portables ni partageables ; la même noix tombe des palmes en danger. Aucun stock, aucun moment de coop.
  - Preuve : `consumable-coconut.md:13-21` ; `ConsumablePickupActor.h:19`.
  - Recommandation : un consommable portable et donnable dans le slot 4.
  - Effort : S

- **[Majeur] Peu validé, code hors dépôt**
  - Constat : un PIE du propriétaire a tué au mousquet et coupé au sabre ; aucune trace d'essai pour torche, noix, trompette. `Weapons/`, `Emotes/`, `Camps/` ne sont pas versionnés.
  - Preuve : board T-072, T-099 ; `git ls-files` : 0.
  - Recommandation : versionner maintenant ; 30 min de playtest manuel.
  - Effort : S

## Manques
1. Sac à butin (ramassage en inventaire).
2. Lanterne mains libres, posable sur barque et galion.
3. Gaffe ou harpon : repêcher le butin, repousser le requin.
4. Baril de poudre piégé, poire, pierre à aiguiser (maquettes prêtes).
5. Longue-vue, boussole, ping (voir 12) ; lorgnon du Percepteur (scan de valeur).
6. Civière ou brouette pour charges partagées.

## Quick wins
1. Versionner `Weapons/`, `Emotes/`, `Camps/`.
2. Torche : refuser l'usage si `IsHolding()` ; trompette sur une touche libre.
3. Corriger `musket.md` (70 → 10 000) ; écrire les fiches torche et trompette.
4. Figer cinq articles de boutique dans `DA_EconomyRules` avant T-114.

## Questions qui challengent l'équipe
1. Qu'achète-t-on pour 150 $ si chaque bean démarre avec tout, sans perte à la mort ?
2. Jeu de porteurs ou de tueurs ? Les trois outils livrés sont des armes.
3. Quel objet fera rire un streamer ? Le baril de poudre est modélisé : pourquoi pas dans le jeu ?
