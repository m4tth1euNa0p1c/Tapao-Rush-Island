# 14 — Méta-progression entre les parties
Domaine : B · Auditeur : designer rétention et méta-progression

## Verdict
Le jeu n'a qu'une partie close (§24.7) et aucun profil joueur : à la défaite, or, boutique, cale et manche disparaissent. Seul stockage existant : un slot « campagne » de quatre compteurs, effacé par NOUVELLE PARTIE. Risque commercial : tôt ou après des heures (calibration en cours, voir 05), une défaite ne laisse aucune trace, donc aucune raison de relancer demain.

## Constats

- **[Majeur] Aucun profil par joueur ; la progression appartient à l'hôte**
  - Constat : seuls or, stocks et améliorations de l'hôte sont sauvegardés ; aucune progression personnelle n'est définie. `SG_LocalProfile` n'est qu'un nom ; le slot existant est supprimé à chaque nouvelle partie.
  - Preuve : cahier §7 l.171, §9 l.200, §16 l.321, §21 l.426 ; `TapaoUISubsystem.cpp:587` ; T-115 = `SG_Run` et historique, aucun profil.
  - Recommandation : ADR « profil » avant T-115. Profil (par machine, versionné, jamais effacé) : stats cumulées, décorations, rang, apparence, tampons d'îles, historique. Partie (hôte) : or, boutique, cale, manche. En fin de manche, le serveur envoie un `FRunSummary` à chaque joueur, qui l'écrit chez lui.
  - Effort : M

- **[Majeur] Défaite sans trace : méta horizontale par jalons, pas de puissance**
  - Constat : une puissance persistante fausserait le quota, l'égalité du §10 et les équipages mixtes. Proposition : « avis de décoration » du Bureau, environ 25 jalons déterministes (première vente, manche 3, Perle intacte, trois zones sans mort) donnant cosmétiques, fanfares (emote trompette existante), 5 rangs d'ancienneté (panneau d'équipage, bilan). Chaque défaite tamponne « traître à la Couronne » (§24.2 l.534).
  - Preuve : §10 l.214 ; §24.6 ; supposé : le genre vit du social, pas de puissance persistante. Héritage mécanique ou boutique élargie : après l'accès anticipé, sur télémétrie.
  - Recommandation : `DA_Decoration` évaluée par le serveur sur `FRunSummary`, règles Standard sans seed seulement (§16 l.317 : durées personnalisées et seed, donc farm). Profil modifiable toléré pour le cosmétique (`security.md:30`), jamais pour un effet de jeu.
  - Effort : M

- **[Majeur] Quatre beans identiques ; skins reportés en L6**
  - Constat : tenue unique configurée sur le composant, non répliquée, aucune sous-classe de `APlayerState`, T-028 `a-faire` depuis le 3 oct. Supposé : l'identité visuelle est le premier plaisir d'un coop à quatre, sans coût d'équilibre.
  - Preuve : `BeanPuppetComponent.h:63-67` ; grep `PlayerState` (un seul fichier) ; board T-028.
  - Recommandation : tranche minimale en L4 : couleur de corps et loadout dans un `ACrownPlayerState` partagé avec T-113, choix sauvé au profil ; tenues et chapeaux en L6.
  - Effort : M

- **[Mineur] Bilan et historique non alimentables**
  - Constat : le bilan de défaite promet meilleur objet, ventes totales, morts ; `FRunSale` n'a que GUID, catégorie, valeur ; `FRunProgress` ne retient aucune vente individuelle.
  - Preuve : `RunTypes.h:54-69, 203-226` ; §24.2 étape 10.
  - Recommandation : avant T-114, `ItemTypeId` et `SourceFragmentId` dans `FRunSale` (avec l'`ItemId` de 05), `FRunHighlights` (meilleure vente, total brut, morts, manches) dans `FRunProgress`, test `RunSpec`.
  - Effort : S

- **[Mineur] Galion : ne persister que l'apparence**
  - Constat : hache, lanterne, emplacements (§7) restent dans la partie. Le profil s'installe dans la cabine du capitaine, seule pièce meublée : coffre de marin personnel (garde-robe, trophée du meilleur objet par partie, carte des îles), local.
  - Preuve : `galleon.md:23,45` ; §7 l.173.
  - Recommandation : `BP_SeaChest` et un écran TAPAO ; fanions et patine du galion après la V1.
  - Effort : M

- **[Mineur] Vingt fragments sans collection, tirage sans choix**
  - Constat : 20 fragments, 8 écrits (nom, désignation de la Couronne, rumeur). Un « Registre des Possessions » (tampons : visitée, contrat signé, rumeur confirmée), un bestiaire et un catalogue d'objets donnent un objectif long sans gros contenu neuf. Le tirage n'exclut que l'île précédente : on ne peut pas viser une île manquante.
  - Preuve : `fragments-tuamotu.md:79` ; `islands.md` §2 ; `ExpeditionIslandValidation.cpp:202`.
  - Recommandation : 20 tampons au profil ; le contrat propose 2 ou 3 îles dont une « inédite » (voir 10, 77).
  - Effort : M

## Manques
1. Spécification du profil : `SG_LocalProfile`, `FRunSummary`, PlayerState projet.
2. Catalogue cosmétique minimal (8 couleurs, 3 tenues, 3 chapeaux, 3 fanfares), garde-robe, décorations, rangs.
3. Registre des Possessions, bestiaire.
4. Succès Steam, statistiques, Auto-Cloud : aucun OnlineSubsystem (`Build.cs:48-51` commenté, `.uproject` sans Steam).
5. Manche finale et clauses de contrat pour le joueur confirmé (voir 19).

## Quick wins
1. Sortir les compteurs de `UTapaoSaveGame` dans un slot `TapaoProfile` que NOUVELLE PARTIE n'efface pas.
2. Ajouter `ItemTypeId`, `SourceFragmentId`, `FRunHighlights` avant T-114.
3. Écran de fin (`TapaoUISubsystem.cpp:493-495`) : manche atteinte, meilleure vente, décorations à la place des trois compteurs ; « Nouveau contrat » direct vers le galion (cahier : menu).
4. Fiche design des décorations et rangs, pour figer les données du profil avant T-115.

## Questions qui challengent l'équipe
1. Un équipage perd à la manche 10 après deux heures de jeu : qu'emporte chaque joueur ? Si « rien », qu'est-ce qui le ramène demain ?
2. Chaque ami possède son exemplaire : pourquoi jouerait-il chez l'hôte d'un autre si seul l'hôte progresse ?
3. Sans manche finale (quotas sans limite), quand a-t-on « fini » ? Rétention par le social et les contenus L8, ou par une carotte de puissance ?
