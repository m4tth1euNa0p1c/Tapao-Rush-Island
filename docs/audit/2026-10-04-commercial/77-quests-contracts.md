# 77 — Contrats et missions : variété et narration émergente
Domaine : H · Auditeur : quest designer

## Verdict
Un contrat se réduit à une île tirée et un quota ; son seul vestige codé, `FRunOrder`, n'a ni producteur ni objet identifiable. La couche qui fait des histoires tient en DataAssets et en calculs purs, comme `FRunCalculator`. Risque commercial : des manches interchangeables dès la troisième (supposé), et `FRunState` (T-113, T-115) figé sans place pour elle.

## Constats

- **[Bloquant commercial] Le contrat n'a pas de forme de données**
  - Constat : `FRunOrder` = catégorie, quantité, bonus ; `FRunSale` = GUID, catégorie, valeur ; aucun objet n'a de type.
  - Preuve : RunTypes.h:35-69 ; ValuableComponent.h:25-57 ; grep `FRunState|UContract` = 0 ; T-113, T-115 `a-faire`.
  - Recommandation : avant T-113, `UContractDefinition` (client, objectifs, modificateurs, rumeur, poids, manches, drapeaux) et `FContractCalculator` pur (offres, paiement, idempotent par `RoundId`) avec spec ; `ItemTypeId` (FName) sur les objets ; chiffres en DA.
  - Effort : M

- **[Majeur] Une prime nette n'achète pas la survie**
  - Constat : le verdict ne lit que le brut ; bonus et commandes sont nets, sans puits de trésorerie (05, 19) : l'offre risquée de 02 (quota ×1,3, prime ×2) est dominée.
  - Preuve : RunCalculator.cpp:81-99 ; bonus = 2,7 % du quota en M10 (05).
  - Recommandation : payer le risque en objet-cible valant 25 % du quota de la manche, posé à l'apparition (au brut, jamais fixe : voir la perle, 05). Prime-trophée : commande `Drop.ShellHammer ×2` (armes lâchées, T-105), plafonnée, prix de l'arme inchangé.
  - Effort : S

- **[Majeur] Aucun choix entre deux manches, et les thèmes ne commandent rien**
  - Constat : §24.2 tire l'île, §16 la fait « sélectionner » ; §24 prime. Quota identique pour toutes les offres : changent client, risque, objet-cible. `Theme` (richesses, bois, vivres) n'a aucun lecteur : une commande de bois devrait désigner la zone à pacifier d'abord (décision de l'horloge).
  - Preuve : cahier l.323, 483, 503 ; VillageLayout.h:185.
  - Recommandation : trois cartes (10) à la table de la cabine (20) : Routine (4 bois, 3 vivres, village nommé, sans modificateur), Spéciale (objectif, un modificateur), Commission (objectif, deux modificateurs, puis la malédiction) ; une île inédite au moins ; `DrawOffers(RoundSeed, historique)` pur ; vote ; amender §24.2 par ADR.
  - Effort : M

- **[Majeur] Modificateurs : des leviers sans porte d'entrée**
  - Constat : 28 `*Rules.h`, chaque directeur lit les siens ; `DifficultyForRound` sans appelant hors tests.
  - Preuve : TikiDirector.h:146 ; DayNightCycleActor.h:73 ; ExpeditionRules.h:25-37.
  - Recommandation : `UModifierDefinition` (effets {clé FName, opération, valeur}, exclusions) lu via un `URoundModifierSet` serveur, sans muter les DA partagées. Nuit, brume, grain (tempête avancée) d'abord, chacun télégraphié (§8 l.183). Une malédiction est un lot satirique (« formulaire 14-B »), jamais imputé aux lieux sacrés (islands.md §1.7).
  - Effort : M

- **[Majeur] Objectifs : trois types sur l'existant, un piège culturel**
  - Constat : `BP_TidalRelic` est un tiki de basalte à spirales koru (sacré, supposé), que islands.md §1.7 exclut du butin promis ; 10 en fait la première cible. Les 44 hameaux n'ont aucun rôle ; villageois sans NavMesh ; pas de carte tactique (12).
  - Preuve : tidal_relic.py:1-5 ; marae-des-perles.md:22 ; villages-japonais.md:128,157 ; TikiVillager.h:11-33.
  - Recommandation : relique = objet de la Couronne (sextant de l'amiral) dans une cabane de camp (CampRules.h:134-146) ; le tiki devient une restitution à l'autel. Témoin = villageois accroupi (clip `Cower`) d'un hameau proche d'un mouillage, qui rejoint la barque en ligne droite (états de `ATikiVillager` à étendre), jamais vendu : il donne une piste. Carte = document invendable qui précise la rumeur suivante.
  - Effort : L (tranche 1 : M)

- **[Majeur] Rumeurs et mémoire : du texte, pas d'histoire**
  - Constat : 8 rumeurs écrites sur 20, aucun lecteur ; §1.7 les veut validées contre la table de loot, intenable à la main. Rien ne relie la manche n à n+1.
  - Preuve : islands.md:66-68 ; grep Source `Rumor` = 0 ; RunTypes.h:203-226.
  - Recommandation : générer la rumeur depuis l'objectif placé : {objet} × {mésaventure de la Couronne} × {lieu-type}, ~50 fragments (≤ 130 caractères). `FRunState` garde drapeaux (témoin, carte, arriérés à +25 %) et une chronique dite par le Percepteur (« Manche 4 : sextant rapporté, abîmé de 60 %. »). Dossiers de trois contrats avec Pembury, Fenwick, Pettifer : l'objet de l'île A pointe vers B.
  - Effort : M

## Manques
1. L4 : trois contrats, nuit/brume/grain, objet de la Couronne, commandes, prime-trophée, table-carte.
2. V1 : témoin, carte, marée basse, village en alerte, dossiers, chronique.
3. L8 : île maudite, restitution, escorte (IA de suivi).
4. Assets : objets de la Couronne (2e PC), ~30 répliques du Percepteur (20), `WBP_ContractBoard`.

## Quick wins
1. `ItemTypeId` (`UValuableComponent`, `FRunSale`, `BP_Drop_*`), `OrderId`, `Label`.
2. Fonction pure `ProduceOrders(thèmes, manche)` avec spec.
3. Objet-cible à 25 % du quota via `InitializeValue` (32).
4. Écrire ~50 fragments de rumeur et de chronique en CSV.
5. Retirer `BP_TidalRelic` des cibles ; nommer l'objet de la Couronne.

## Questions qui challengent l'équipe
1. Après trois manches, quelle histoire un équipage raconte-t-il que vous n'avez pas écrite ?
2. Si la carte Commission rapporte plus et que trois joueurs la veulent, que fait le quatrième ?
3. La cible de la blague est-elle la Couronne ou les insulaires ? Le tiki de basalte : pillé ou restitué ?
