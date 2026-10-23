# 06 — Contre-cahier B : réalisme commercial et survie contre « Littoral » et « Highwater »

Camp défendu : 03 (Surcote) et 04 (Tidemark). Faits cités depuis `docs/audit/01_REPO_MAP.md` (MAP), `docs/production/recette-2026-10-11.md` et `docs/production/board.md` lu aujourd'hui. Chiffre non sourcé = estimé.

Un fait d'abord, que les deux cahiers attaqués citent sans le regarder : au 11/10, 147 tickets en `review`, **aucun lot validé**. Aujourd'hui le tableau affiche 288 `fait`, dont **93 fermés « sans recette dédiée : ni test lancé ni preuve nouvelle »** (acceptation du propriétaire le 11/10), **0 ticket « testé multi-machines »**, 15 marqués « non testé multi-machines ». La CI ne compile pas le C++ (MAP §6). Le jeu vitrine n'est pas validé ; il a été déclaré validé.

## 1. Les affirmations fragiles

### 01_tech — « Littoral »

**a) Trois modules, un Program `LittoralCli`, un dépôt Python séparé, une spec `.lgrid` publique et un rejeu CI sur deux binaires en semaine 1.** Sur-ingénierie pour une personne : le dépôt a **un seul module** (MAP §1.1), aucune CI qui compile, et personne n'y a jamais produit de cible Program. Trois semaines de contrats internes avant qu'un utilisateur n'existe, et **aucun entretien client** en huit semaines : zéro go/no-go.

**b) Le point fixe réécrit `FFloodGrid`, puis « `L_Frag_Raraka_01` inonde comme avant (hash de référence figé) » en S4.** Contradiction interne : un solveur entier ne reproduit pas au hash près un solveur flottant, et il n'existe aucun hash de référence puisque la submersion n'a jamais été mesurée (`fragments-tuamotu.md:449`). La réécriture jette les 4 fichiers de specs `Flood/Tests/`, seule preuve que le solveur marche, pour maintenir deux solveurs. Le déterminisme entre deux compilations est un problème que le client n°1 n'a pas : il livre **un** binaire à ses joueurs.

**c) 179 € par siège, « 60 ventes ≈ 10 k€ », « le plugin vit si Tapao en est le client et la vitrine ».** Marché non prouvé : le segment est plafonné par Fluid Flux à 350 $ **avec** rendu spectaculaire (03 §8) ; un plugin invisible à 179 € dépasse ce qu'un studio de 2 à 15 personnes paie pour une brique qu'il croit savoir écrire. Et la vitrine n'existe pas : ni page Steam, ni session à deux PC, ni lot recetté (SYNTHESE, bloquant 3). Enfin le bake Landscape est présenté comme un déplacement alors que le dépôt n'a **jamais utilisé un Landscape** (`grep ALandscape = 0`, MAP §1.3).

### 02_design — « Highwater »

**a) « Ce que tu prends, la mer le prend » comme idée unique d'un plugin.** C'est une mécanique de **Tapao** (extraction → risque), pas une capacité qu'un studio tiers achète : son jeu a sa boucle. Contradiction avec « sans dépendance au gameplay de Tapao ». Et le critère S5, « dégager 200 cellules avance l'eau d'au moins 20 s (DataAsset) », est un réglage scripté, exactement ce que §4-d jure ne jamais faire. Le solveur n'a ni rugosité ni végétation (MAP §3.2) ; en 8 semaines ça devient un bouton, pas une physique.

**b) Le curseur de niveau de mer, accroche « comprise en huit secondes », défendable 12 mois.** Un seuil d'altitude sur une heightmap est un plan en Z avec un overlay : un dev le fait en une heure, Epic en une semaine. Le défendable, c'est la grille répliquée prouvée, rangée en S6 après le curseur et le GIF. Pire : sous le contrat de Tapao que le cahier garde (« déluge à 9 min »), la montée finale fait **650 cm en 60 s puis +10 cm/s** (`ExpeditionClock.cpp:101-116`). Tout le fragment est sous l'eau en une minute ; `TimeToFlood` varie de quelques secondes entre cellules. « Lire le risque sur le sol » n'a aucun sens dans le seul jeu qui l'utilise.

**c) Client n°1 = « studio indé coop de 2 à 8 personnes qui montre le plugin dans son trailer », 150 à 400 ventes.** Aucune donnée Fab, **aucun entretien** en 8 semaines ; le critère de S4 est « un GIF qu'on a envie d'envoyer ». Les trois quasi-jumeaux de l'audit 81 prouvent l'inverse de la thèse : ils ont sorti leur jeu avec une île qui coule uniformément, donc sans ce produit. Et « 1 158 tests passent » est une lecture fautive : MAP compte ≈ 1 158 `It(`, rapporte 1 085 tests annoncés par le README, et **aucun** test de réplication.

## 2. Ce que je concède

**À 01_tech :**
1. **Le bake unique à l'édition**, qui remplace les 16 000 rayons par image (`FloodRules.h:132-140`) et les abris reconnus par **nom de mesh** (`WindRules.h:120-124`) : c'est le vrai refactor, 04 le reprend tel quel.
2. **`credits[]` obligatoire, refusé par la CI sans licence**, et hash des sources dans l'asset : la seule parade outillée à la contamination Maxar/DAG que mon camp décrit sans l'outiller.
3. **« Le climat s'importe, la géométrie se déduit »** et `propose_` au lieu de `design_` : à adopter mot pour mot.
4. Le **hash par bloc 32×32** pour savoir *où* ça diverge : si le multi est en V1, c'est l'outil de diagnostic qui manque à 04.

**À 02_design :**
1. **`TimeToFlood` et `FloodOrder` par point, en Blueprint, comme API vendue** : première ligne de la page Fab. 04 l'a (`TimeToWet`) mais ne la met pas en avant.
2. **Une capture publiable de 10 s chaque semaine dès S4**, arrêt sinon : discipline de survie que mon camp, occupé à chiffrer, n'impose pas.
3. **Une seule menace, archétypes jamais toponymes, zéro code B2B en V1** : 03 doit reculer, le bureau d'études est une piste d'année 2.

## 3. Position finale

1. **Client n°1** : le dev solo ou duo d'un coop UE sur Fab à 59 $, go/no-go sur 5 entretiens en semaine 1 ; le bureau d'études est un appel de semaine 6, et Tapao un premier utilisateur, pas une vitrine tant qu'il n'a pas de build prouvé à deux PC.
2. **Multijoueur** : V1, parce que sans preuve à deux PC le produit est un plan en Z ; mais c'est la **porte de semaine 4**, avant tout curseur, lint ou MCP, et si elle échoue le plugin reste interne.
3. **Packs de lieux réels** : jamais vendus ; un script MIT exécuté par l'utilisateur sous sa responsabilité, démo fictive.
4. **MCP** : supprimé du produit, au plus un commandlet de lint ; une règle déterministe s'explique par une info-bulle.
5. **Menaces** : l'eau seule, sur le solveur existant déplacé sans réécriture ; le reste attend des ventes.

## Résumé

- Les deux cahiers posent un plan de quinze personnes sur un développeur seul et une vitrine dont 93 tickets ont été fermés sans test et 0 prouvé à deux PC.
- 01 réécrit un solveur qui marche, sans client, à 179 € au-dessus du segment ; 02 vend une mécanique de Tapao et un curseur copiable, sous un contrat qui noie tout en 60 s.
- Je garde de 01 le bake unique, les crédits vérifiés par la CI, le hash par bloc, « climat importé » ; de 02 l'API `TimeToFlood`/`FloodOrder`, la capture hebdomadaire, « une menace, archétypes, zéro B2B ».
- Position : dev coop à 59 $ avec go/no-go semaine 1, multi en V1 comme porte de semaine 4, aucun pack vendu, MCP supprimé, l'eau seule.
- Défaut commun des deux cahiers : pas un seul entretien client en huit semaines.
