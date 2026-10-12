# 72 — Sensibilité culturelle (Pacifique et Polynésie)
Domaine : H · Auditeur : consultante culturelle (Pacifique)

## Verdict
Le dossier narratif est plus prudent que le jeu : noms traduits, marae jamais moqué, Motutapu exclu (`islands.md:25-61`, ADR-004:20). Mais le code écrit prend des lieux sacrés pour butin, fait des « tiki » des monstres et laisse découper des villageois, trois motifs voisins de cas ayant valu retraits et critiques. Risque commercial : une capture lue au premier degré avant que la satire ne s'entende, sans trace d'un regard polynésien.

## Constats
Sources web : résumés de recherche uniquement.

- **[Majeur] Le sacré devient butin vendu à la Couronne**
  - Constat : chaque fragment compte 2 marae pillables (perle 1 500 $, offrandes, conques) et jusqu'à 8 camps de pêcheurs aux outils volables ; 30 totems tiki (150 $) sont semés par partie. La « Perle du Gouverneur » ne couvre que la perle ; le reste heurte l'esprit de `islands.md:67` (« ne promet jamais de piller les habitants ou leurs lieux sacrés »).
  - Preuve : `marae-des-perles.md:16-22`, `MaraeRules.h:83`, `characters-tiki.md:150`, `board.md:103` ; marae = lieux vivants, https://whc.unesco.org/en/list/1529
  - Recommandation : `NoGrab` (`IslanderGrabComponent.cpp:90`) sur le contenu du marae, devenu refuge (`islands.md:60`) ; valeur reportée sur des caches de la Couronne (épaves, coffre du Percepteur) ; totems remplacés par des bornes de possession ; arbitrer avec 05.
  - Effort : M

- **[Majeur] Le polynésien est monstre, victime ou butin**
  - Constat : les insulaires sont des créatures-coco « tiki », masquées et rugissantes à l'attaque, en pagne et terrifiées dans la fuite ; maisons, torii et sanctuaires sont japonais (prudence d'ADR-001 E-03), ce qui réserve les signes autochtones à ces rôles. Pour un poète du Pacifique (Met Museum), un tiki porte le mana d'une divinité : le méconnaître suscite « mélancolie et colère ». Daybreak a annulé un « Maui » (H1Z1) à tatouages et masque maori.
  - Preuve : `characters-tiki.md:22-26`, `art/SOURCES.md:35,116-124` ; https://metmuseum.org/perspectives/articles/2023/5/tiki-manifesto ; https://teaonews.co.nz/2019/05/02/us-game-developer-cancels-release-of-maui-character
  - Recommandation : avant le bake (`characters-tiki.md:81`), remplacer masques et nom par des « Gardiens » naturels (coco, corail, cendre) ; « tiki » réservé à un décor inoffensif.
  - Effort : M

- **[Majeur] Villageois frappés, tués, découpés**
  - Constat : le villageois, apeuré, peut être tué et découpé ; le meurtre déclenche une vengeance, pas un refus. Pocket God (2009), titré « simulateur de torture polynésien ? », fut jugé raciste par l'International Pacific Women's Information Network.
  - Preuve : `TikiVillager.cpp:395-427`, `characters-tiki.md:109-136` ; https://www.pocketgamer.com/pocket-god/pocket-god-a-polynesian-torture-simulator/
  - Recommandation : villageois non lésables (ils esquivent, repoussent), découpe réservée aux ennemis.
  - Effort : S

- **[Majeur] Satire déclarée, non jouée**
  - Constat : l'ironie vit dans des textes ; les verbes sont ceux d'un raid récompensé. Le sermon (20 s, drapeau) est une victoire sans texte ni voix. Il évoque la mission chrétienne, lisible comme moquerie de la foi (supposé).
  - Preuve : cahier §6.2:108-112 ; aucun « sermon » dans `Source/` ; `islands.md:40`
  - Recommandation : « Lecture de l'Édit de possession » à une plage vide (écho du Requerimiento, 1513) : « Les habitants, absents, sont réputés consentants. » Le Percepteur tamponne le drapeau « provisoire » ; la Couronne perd à chaque scène clé.
  - Effort : M

- **[Majeur] Lieu réel : exclusion logique, atoll qui coule**
  - Constat : le flux réel charge encore `Lvl_Raraka` 1:1 ; le butin commun n'évite Motutapu que sur 120 m ; l'inondation était centrée sur le village. Supposé : map entière livrée. « Île condamnée, mer qui monte » épouse un atoll des Tuamotu (1–2 m d'altitude) : « des étrangers pillent avant la noyade ».
  - Preuve : BRIEF (flux), ADR-004:20-22, `loot-common.md:33`, `storm-flood.md:90` ; https://www.mvariety.com/?p=124481
  - Recommandation : ni « Raraka » ni « Tuamotu » au marketing ni dans les assets livrés (hors crédits Etalab) ; Motutapu absent des maps cuites (vérifier avec 43/48) ; villageois évacués en pirogue, jamais noyés.
  - Effort : M

- **[Majeur] Aucune consultation, consignes contradictoires**
  - Constat : aucune trace de conseiller ni de budget ; agent narratif et gabarit de quête exigent des toponymes réels, contre ADR-001 E-03 ; bible absente. Moana a consulté (Oceanic Story Trust), mais a retiré un costume Maui comparé au blackface et reste critiquée : consulter réduit le risque sans l'annuler.
  - Preuve : `ue-narrative-designer.md:7-12`, `narrative-quest.md:4`, `islands.md:13` ; https://www.wptv.com/news/national/moana-a-disney-hit-but-portrayal-irks-some-in-the-pacific
  - Recommandation : 2 conseillers payés (un pa'umotu, un Pacifique), revue avant trailer, bake et Steam (contact : Direction de la culture et du patrimoine, https://www.service-public.pf/art/wp-content/uploads/sites/55/2024/03/Hiroa-N°195-Mars-web.pdf) ; 1 à 2 % du net (supposé) à une structure locale choisie avec eux ; crédits Etalab, remerciements consentis ; skins sans tatouage ni masque.
  - Effort : M

## Manques
1. Rapport de consultation polynésien.
2. Bible narrative, « lignes rouges » d'une page.
3. Faction insulaire non hostile et agissante (cahier §1:11).

## Quick wins
1. `NoGrab` sur le contenu du marae.
2. Villageois non lésables.
3. « tiki » banni des textes, de Steam, du trailer.
4. Corriger `ue-narrative-designer.md`, `narrative-quest.md`.
5. Trois Édits de possession à la place du sermon.

## Questions qui challengent l'équipe
1. Vos maisons sont japonaises ; masque, pagne, marae et totem, eux, sont monstre, victime ou butin : voulu ?
2. Quelle scène montre la Couronne perdant ? Sinon, la satire est dans vos notes.
3. Qui a montré le marae pillé à un Polynésien, et retarderiez-vous la capsule pour l'entendre ?
