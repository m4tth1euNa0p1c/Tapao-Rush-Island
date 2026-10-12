# 10 — Rejouabilité (50 h et plus)
Domaine : A · Auditeur : designer roguelite

## Verdict
Les fondations sont saines (règles en DataAsset, pipeline hors ligne reproductible), mais les manches sont conçues et pas codées (seul `FRunCalculator` existe), et la variété prévue est de position, pas de situation : 20 fragments = 20 points d'arrivée d'une même recette. Calculé : ~70 expéditions pour voir les 20 îles ; supposé : nouveauté épuisée vers 10–15 h. Risque commercial : la couche qui tiendrait 50 h (contrats, modificateurs, variantes) est reportée à L8 (cahier §11).

## Constats

- **[Bloquant commercial] Le contrat n'est qu'un quota**
  - Constat : une île se réduit à carte, mouillages et taille : ni gain, ni objectif, ni modificateur. Cahier contradictoire : île « tirée » (§24.2) ou « sélectionnée » (§16). Les leviers existent pourtant (DataAsset ou paramètre global : heure de départ, vent, houle, niveau d'eau, gardiens, zones) : aucun contrat ne les surcharge.
  - Preuve : ExpeditionIslandDefinition.h:41-79, cahier §24.2 et §16, RunTypes.h:35 (commandes sans producteur, voir 05).
  - Recommandation : `UContractDefinition` = île + objectif + 0 à 2 modificateurs + coefficient de gain ; 3 offres par zone sûre, dont une île inédite. Objectifs : 2 à 4 zones, objet-cible (`BP_TidalRelic` existe), commande, discrétion (aucun villageois tué). Modificateurs : nuit, brume, houle, marée basse, vent, requin, village en alerte. Un de plus par manche de M2 à M6, la difficulté plafonnant à M6 (voir 15, 19).
  - Effort : L (tranche 1 : M)

- **[Majeur] 20 fragments, une seule recette, aucune variante**
  - Constat : les 20 définitions pointent sur `Lvl_Raraka` (aucune `L_Frag_*`). Toutes : 3 zones, un village par thème, 90 m de rayon, villages gelés. `VariantSeeds` n'a aucun lecteur ; chaque générateur tire sa graine. Le cahier prévoyait des variantes de placement (§1).
  - Preuve : Content/MonkeIsland/Maps, setup_fragments.py:121, villages/fragments/index.json (calculé), ExpeditionIslandDefinition.h:71, CampSpawner.cpp:120, TikiDirector.cpp:951.
  - Recommandation : avant de produire 19 cartes, varier la recette par la graine de manche (voir 19) : 3 zones parmi 4 à 6 sites (hameaux promus), thèmes permutés, 2–3 points de départ (criques et mouillages de `fragments.json`, à valider), tables de contenu des marae et camps.
  - Effort : M

- **[Majeur] Le contenu généré est du décor**
  - Constat : le thème d'un village n'est lu par aucun code ; les 44 hameaux (254 maisons) n'ont ni drapeau, ni gardiens, ni butin propre ; les rumeurs n'ont aucune table de butin ; marae et camps n'ont qu'une disposition. Rien n'incite à préférer un lieu à un autre.
  - Preuve : VillageLayout.h:185, TikiDirector.cpp:618, villages-japonais.md:157, islands.md:68, MaraeRules.h:62-67, CampRules.h:106.
  - Recommandation : un rôle de jeu par lieu, tiré par contrat : richesses = coffre, bois = raccourci, vivres = refuge ; hameau = cache, embuscade ou marchand ; rumeur = indice vrai sur l'objet-cible.
  - Effort : M

- **[Majeur] Identités narratives détachées des fragments**
  - Constat : islands.md décrit 8 identités selon un profil « attendu », attribuées par numéro, pas par profil mesuré : « Les Pas-sur-l'Eau » (îlots) devient FRG_03, naturel de 42 ha ; les vrais archipels sont 02, 10, 19. Noms de villages générés ≠ écrits (FRG_01 : Uo/Take/Kogane, pas Takara/Mori/Kura).
  - Preuve : islands.md:85 et 248, fragments.json, villages/fragments/index.json.
  - Recommandation : rattacher chaque identité par profil mesuré (type, ha, couvert, côté d'arrivée), la traduire en modificateur (Houle : vent ; Fourrés : nuit ; épave : butin à marée basse).
  - Effort : M

- **[Majeur] Aucun budget de nouveauté**
  - Constat : 50 h ≈ 250 manches (12 min, supposé). Une situation distincte = une île : 20, chacune revue ~12 fois ; le tirage n'exclut que la précédente. Aucun test ne mesure la variété du contenu.
  - Preuve : simulation (68 expéditions pour voir les 20 îles), ExpeditionIslandValidation.h:47, fragments-tuamotu.md (« previous None »).
  - Recommandation : cible ≥ 150 situations distinctes (20 îles × 4 objectifs × 7 modificateurs, un seul par contrat = 560 théoriques), mémoire de 8 îles, test de couverture sur (île, objectif, modificateurs, mouillage).
  - Effort : S (test), le reste via les constats 1 et 2

## Manques
1. Contrats à 3 offres, objectifs, modificateurs, gain couplé (voir 12, 15, 77).
2. Variantes de marae, camp, village : tables de contenu d'abord, art ensuite (voir 63).
3. Rôle de jeu des hameaux, rumeurs vraies, secrets.
4. Registre par île, graine du jour (voir 14, 19).
5. Mesure de variété, télémétrie (voir 95).
6. Autres atolls par le pipeline (juridique : voir 73).

## Quick wins
1. Faire lire `?Seed=` (déjà parsé pour l'arrivée) aux spawners de camps, butin et totems, graine journalisée.
2. Renseigner `IndicativeDifficulty` des 20 îles depuis `fragments.json`.
3. Table identité → île par profil mesuré (script sur `fragments.json`).
4. `BP_TidalRelic` (existant) posé dans un hameau tiré par graine : premier objet-cible.
5. Test de couverture de variété (île × mouillage × heure de départ).

## Questions qui challengent l'équipe
1. À la 20e heure, qu'est-ce qui est différent (pas plus dur) ? Citez trois situations absentes de la 2e heure.
2. Le joueur choisit-il son contrat ou subit-il un tirage ? Sans choix, où est la décision entre deux manches ?
3. La rejouabilité peut-elle attendre L8 alors que la fidélisation se joue dès les 10 premières parties ? Sinon, quels 3 modificateurs et 2 objectifs entrent dans L4 ?
