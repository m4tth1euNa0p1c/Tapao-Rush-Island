# 05 — Contre-argumentaire A : camp « produit pour développeurs, mécanique visible, ambition »

Je défends 01 (Littoral) et 02 (Highwater), j'attaque 03 (Surcote) et 04 (Tidemark). Faits du dépôt : `docs/audit/01_REPO_MAP.md` (MAP).

## 1. Les affirmations fragiles

### 03_market.md — « le vrai client est le bureau d'études littoral »

**F1. Le client n°1 est un verdict posé avant la preuve.** Le « verdict » ouvre le cahier ; la preuve arrive en semaine 6 (8 entretiens, couperet à 3 intentions chiffrées), après cinq semaines de développement orienté PDF et rapport. Le cahier admet lui-même : « non vérifié : budgets réels des ateliers PAPI, part sous-traitée au privé ». Et sa raison d'échec n°2 dit pourquoi la niche est vide : les ateliers sont faits par des labos sur fonds publics (LittoSIM, LittoWag). Une niche vide parce que non solvable n'est pas une niche, c'est un désert. Circulaire : on choisit le BE parce qu'il paie, puis on découvrira en S6 s'il paie.

**F2. Les chiffres B2B sont inventés, les chiffres Fab sont mal lus.** « Année 3 : 5 à 10 clients BE, 80–150 k€/an » n'a aucune source, pas même « recherche non relue ». Le « pilote 6–10 k€ » comprend « 3 jours d'adaptation + atelier » : c'est du service facturé en jours, pas un produit ; le comparer au revenu produit de Tapao (14 k$, audit 85) compare un chèque de consultant à une licence. Inversement, « 24 M$ / 20 000 vendeurs ≈ 1 200 $ » est une moyenne sur 420 000 fiches, dont le cahier reconnaît qu'elle est « très inégalement répartie » : on ne déduit rien d'une moyenne qu'on dit elle-même non représentative.

**F3. Contradiction interne sur la crédibilité.** Le BE achète « d'où vient ce chiffre ? ». Le produit lui répond avec un relief Copernicus à 2–4 m d'erreur pour 3 m de relief (sa propre raison d'échec n°1), et la parade est « premiers pilotes sur des côtes métropolitaines Litto3D ». Donc : le client n°1 n'est pas dans le Pacifique, le pack Tuamotu ne lui sert pas, et la « trace par cellule » affichera une incertitude plus grande que la valeur. Pire : il vise les marchés PAPI/PPRL (réglementaires) avec une mention non désactivable « ni prévision ni document réglementaire ». Le BE achète un outil qui dit à l'élu qu'il ne faut pas s'en servir pour ce qu'il a été acheté. Enfin, « multijoueur V2 » retire la seule propriété que les plugins GPU ne peuvent pas copier ; le dépôt l'a déjà en architecture (MAP §3.2, horloge répliquée, simulation locale).

### 04_devil.md — « Tidemark, 59 $, zéro donnée, pas de MCP »

**F1. « Zéro octet de lieu réel » jette le seul actif que le dépôt possède.** MAP §4 : pipeline carto Python, trait de côte Etalab, 352 motu, 20 fragments catalogués, 43 057 palmiers placés. Tout ce qui est propre (côte Te Fenua) est jeté avec ce qui est sale (Maxar, DAG). Le cahier conclut « le relief vertical ne le sera pas » tout en citant lui-même Litto3D Etalab sur Moorea, Bora Bora et Tahiti : « aucun produit Tuamotu » n'est pas « aucun lieu réel possible ». Et il contredit la contrainte du propriétaire (lieux précis) sans la négocier, par interdiction totale (« jamais, V1 et V2 »). Un plugin sans donnée est exactement ce qu'un concurrent copie : le cahier le dit (« l'algorithme est de 2007 », « Epic peut le faire, on cesse de vendre »).

**F2. Prix et go/no-go circulaires.** Le prix est fixé à 59 $ parce que le produit est « d'appoint », puis le produit est jugé « d'appoint » parce que 40 ventes à 59 $ font 2 k$. Le couperet de semaine 1 est « 3 devs sur 5 diraient “je paierais 59 $” » : un échantillon de cinq, une question hypothétique, zéro valeur statistique, mais une décision d'arrêt. Le « ⚠ 50 % personne n'en veut » sort du chapeau. Et un produit qu'on annonce « pas une entreprise » ne justifie pas huit semaines d'une seule personne pendant que « L1–L4 reste prioritaire » : le plan viole sa propre règle « un seul produit à la fois ».

**F3. Les bloquants juridiques sont gonflés.** L'argument ODbL (« une grille calculée depuis OSM est une base dérivée, share-alike ») est présenté comme coup fatal au volet données, alors que MAP §7 établit qu'OSM n'est pas utilisé dans le dépôt et que le cahier l'exclut lui-même en §9.2 : un problème qu'on résout en une ligne ne tue pas un pilier. Sur le MCP, « une règle déterministe s'explique par une info-bulle » oublie que `lint_level` interroge 1,2 M cellules et des chemins entre points nommés, pas une cellule sous la souris ; et le plugin MCP officiel est déjà activé dans le `.uproject` (MAP §1.1) : exposer un commandlet coûte une semaine (01 S7), pas « un produit ».

## 2. Ce que je concède

**À 03.** (a) La chaîne de justification par cellule, avec source et incertitude, est la vraie différence avec un plugin d'eau ; 01 l'a (`rule_id@version`, `grid_hash`), elle doit afficher l'incertitude dès V1, pas en option B2B. (b) Parler à des clients avant la semaine 8 : 01 ne confronte un dev externe qu'en S8 ; une semaine d'entretiens (devs coop UE, pas BE) doit entrer en S2–S3. (c) Fab seul ne paie pas un salaire ; le format ouvert (`.lgrid` + lecteur Python) doit survivre sans Unreal pour ouvrir le B2B plus tard.

**À 04.** (a) Le relief vertical d'un atoll n'est pas honorable avec les sources libres : le pack Atoll est « inspiré », sa forme en plan est réelle, son relief est déclaré estimé, et il n'est jamais vendu comme preuve (02 §11.1 le dit déjà). (b) Purge Maxar/DAG **avant** tout branchement du plugin, sinon il hérite de la contamination (MAP §7 : bloquant). (c) L'ordre : d'abord extraire `Flood/` en plugin parce que Tapao en a besoin (`Flood → ExpeditionGameState`, 16 000 rayons/image) ; 01 S4 le fait, mais le go/no-go écrit doit exister en S1, avec plus de cinq personnes. (d) La CI ne compile rien (MAP §6) : la spec de déterminisme doit tourner en CI dès S1.

## 3. Position finale sur les cinq désaccords

1. **Client n°1** : le studio UE coop de 2 à 15 personnes sur Fab, qui veut la même eau chez l'hôte et les clients ; le bureau d'études est un client n°2 de V2, servi par la traçabilité déjà dans le format, pas par une ligne de code en V1.
2. **Multijoueur** : V1, prouvé sur deux PC en S5–S6, parce que c'est la seule propriété qu'aucun plugin GPU ne copie et la raison d'achat du client n°1.
3. **Packs de lieux réels** : vitrine unique en V1 (atoll « inspiré des Tuamotu », côte Etalab, relief marqué estimé, toponymes renommés) ; pilier interdit tant qu'un LiDAR Etalab ne couvre pas le lieu ; « jamais » rejeté, car il jette le pipeline carto et la contrainte du propriétaire.
4. **MCP** : outil gratuit secondaire qui exécute le CLI et renvoie `rule_id` + `grid_hash` (01 §6.4), ni accroche ni supprimé ; l'accroche est le curseur de niveau de mer et le lint, sans IA.
5. **Menaces en V1** : une seule, l'eau, avec trois lectures (quand, où, à cause de quoi) et les éditions de descripteurs par le gameplay ; vent, érosion, passes persistantes en V2.
