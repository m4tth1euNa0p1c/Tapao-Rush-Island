# 05 — Économie du butin face à la courbe de quota
Domaine : A · Auditeur : économiste de jeu

## Verdict
Formules saines et testées (RUN14 : net 453 exact), mais calibration dérivée : la valeur au sol a été multipliée par ~79 en quelques heures sans toucher la courbe, et une seule perle noire couvre chacune des cinq premières manches solo. Le risque est mal payé (marae sûr = jackpot, combat et villages = presque rien) et la taxe n'a presque aucun effet de jeu. Risque commercial : une manche 1 gagnée sans choisir, puis un mur de cale (vente, cale, boutique non codées : chiffres issus des DA, scripts, calculs).

## Constats

- **[Bloquant commercial] Inflation : ×79 de valeur au sol, courbe inchangée**
  - Constat : T-076 posait 300 objets à 2,9 $. Après « 5–40 $ » puis T-108 (espacement 10 m) : 4 110 objets à 16,5 $ de moyenne, ≈ 68 000 $ par fragment, soit 20× les cinq premiers quotas solo (3 350).
  - Preuve : board.md:83,115 ; loot-common.md:8–21 ; import_loot.py:26–33 ; calcul.
  - Recommandation : séparer décor ramassable (1–3 $, garde l'effet 10 m) et butin (10–40 $, ~400 par fragment) ; cible : la manche 1 absorbe 40–60 % du butin accessible en 3 min.
  - Effort : S

- **[Bloquant commercial] La perle noire (1 500 $) court-circuite la courbe, en zone sûre**
  - Constat : un marae vaut 1 900 $ (perle 1 500, 2 offrandes 120, 2 conques 80), deux par fragment, en zone calme de 15 m sans tiki. Une perle couvre les manches 1–5 solo, 1–3 à quatre. Avec la cale persistante (RUN11), épargner les seules perles (2 par manche) tient jusqu'à M14 solo, M10 à quatre.
  - Preuve : marae-des-perles.md:16–18 ; MaraeRules.h:77–83 ; calcul.
  - Recommandation : une perle par fragment, gardée, valeur ≈ 1× le quota de la manche (ou 500 $) ; en faire une commande bonus du contrat.
  - Effort : M

- **[Majeur] Risque/récompense inversé : combat et villages ne paient pas**
  - Constat : arme de guerrier 30/45/50 $ (2–4 kg) : 2–3 objets gratuits, 12–15 $/kg contre 71 $/kg au commun. Camp ≈ 105 $ (outils 8–20 $ + 2–4 communs), en zone calme. Villages : aucun butin, leur thème (richesses, bois, vivres) n'est lu par aucun code C++.
  - Preuve : characters-tiki.md:188 ; camp_materials.json ; VillageLayout.h:185 (rien sous Village/).
  - Recommandation : armes ×3 ; coffre de zone par village pacifié (150–300 $ selon le thème) ; camps ×2.
  - Effort : S

- **[Majeur] La cale (40 emplacements) plafonne l'économie, pas le butin**
  - Constat : quota par emplacement : 6 $ (M1), 22 $ (M4), 40 $ (M6) en solo ; 38,5 $ (M4), 53 $ (M5) à quatre. Le commun (16,5 $, 40 max) ne suffit plus dès M4. Une cale pleine de butin premium (2 marae + coffrets) plafonne à ≈ 5 000 $ : mur vers M9 à quatre, M12–13 en solo ; quatre joueurs butent plus tôt. La masse ne contraint plus : 71 $/kg contre 5 $/kg au cahier §5.
  - Preuve : RunRules.h:69–74 ; RunCalculator.cpp:40–46 ; calcul.
  - Recommandation : cale × (1 + 0,25 × (joueurs − 1)) comme le quota ; paliers d'emplacements en boutique.
  - Effort : S

- **[Majeur] Taxe décorative, boutique vide, trésorerie sans puits**
  - Constat : le quota se juge au brut, donc la taxe ne touche que la trésorerie ; son seul puits (boutique, 550 $) est vidé dès M2 (net cumulé à quota exact : 623). Hache (100) et lanterne (150) n'existent pas (torche et mousquet sont déjà gratuits), l'emplacement de cale n'a pas de prix, DA_EconomyRules est absent. Frais de mort plats (50 $), plancher à 0. Bonus de 100 $ : 40 % du quota en M1, 2,7 % en M10.
  - Preuve : RunCalculator.cpp:81–102 ; EconomyRules.cpp:7–9 ; Glob DA_EconomyRules (aucun).
  - Recommandation : rachat de déficit depuis la trésorerie (≤ 25 % du quota) ; paliers de cale et de barque à prix croissants ; bonus 20 % et frais 5 % du quota ; créer DA_EconomyRules.
  - Effort : M

## Manques
1. Journal des ventes par source ($/min, cale pleine).
2. `ItemId` et catégorie sur UValuableComponent, objets bois/nourriture : sans eux, les commandes du §24.4 sont impossibles.
3. Puits récurrents : munitions (23 balles par joueur), entretien.
4. Butin de village ; objet à deux porteurs (max 9 kg pour 25 kg de levage) ; indice de richesse par île.

## Quick wins
1. DA_LootRules : espacement ×3,5 ou Count 500 ; décor à 1–3 $.
2. Marae : une seule perle, à 500 $.
3. `BP_Drop_*` ×3 (90/135/150 $).
4. `ShopPrices` : +5 emplacements de cale (200 $) ; bonus et frais indexés dans DA_RunRules.
5. Commun cassable (seuil 3 m/s au lieu de 6) ; champ `Source` par objet.

## Questions qui challengent l'équipe
1. Du butin « tous les 10 m » : le joueur doit-il chercher ou ramasser ? S'il ramasse, que mesure le quota ?
2. Quelle durée pour une partie réussie ? Avec la cale persistante, les seules perles tiennent jusqu'à M10 (quatre) ou M14 (solo) : fin de partie voulue ?
3. Si la défaite se juge au brut, quelle décision change quand la Couronne prélève 35 % ?
