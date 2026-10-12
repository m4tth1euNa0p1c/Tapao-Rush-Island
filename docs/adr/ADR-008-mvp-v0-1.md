# ADR-008 : MVP v0.1 (accès anticipé), jalons J1–J6 et lot L9 « Sortie »

- **Statut** : proposée. À confirmer par le propriétaire : aucune des 10 décisions de l'audit T-117 n'a reçu de réponse écrite au 2026-10-10. Les choix ci-dessous sont des défauts proposés.
- **Date** : 2026-10-10. Tâche T-136, rédigée par `ue-producer`.
- **Sources** :
  - cahier v1.4 (§1, §2, §11 à §15, §23, §24) et `docs/production/backlog.md` ;
  - ADR-001 à ADR-007 et `docs/features/campagne-dix-jours.md` ;
  - audit T-117 : `docs/audit/2026-10-04-commercial/SYNTHESE.md`, rapports 89 (sortie), 91 (périmètre), 92 (roadmap) ;
  - tri du chantier du jour : notes « Tri 2026-10-10 » de `docs/production/board.md`. Le rapport a été rendu à la session principale, qui l'enregistre sous `docs/production/triage-2026-10-10.md`.

## Contexte

**Ce que le jeu fait aujourd'hui.** Le build v1.6 du 2026-10-09 compile, passe 681 specs Automation et tourne en package Development. Un joueur seul fait la campagne des 10 jours, vérifiée en build :
- menu TAPAO, arrivée en cinématique sur le pont du galion, barque amarrée ;
- île tirée parmi 20 fragments de Raraka : `FRG_Raraka_01` a sa carte, les 19 autres se jouent dans `Lvl_Raraka` ;
- villages et cérémonie du drapeau, marae, temples et artefacts, forts français et leur garnison, tiki, crabe géant ;
- armes : mousquet, pistolets, fusil à pompe, tromblon, carabine, grenade, sabre, torche ; marchand, caisse de transport ;
- paliers de quota (5 500, 10 500, 20 500), fin de jour animée, exécution au quota manqué.

**Ce qui manque pour vendre.**
- **Preuves** : aucun test sur deux machines (T-133). Aucune revue réseau ni sécurité signée. Performance jamais mesurée (T-091).
- **Dépôt** : le dernier commit date du 2026-10-06 (T-164).
- **Code hors `main`** : la sauvegarde sûre, le sermon et l'arrimage restent sur des branches (T-131).
- **Coop** : on ne peut rejoindre qu'avec une adresse IP. Pas de Steam, pas de lobby, pas de spectateur, `MaxPlayers` n'est pas posé.
- **Présentation** : pas de tutoriel ni d'anglais ; le nom n'est pas vérifié.

**Écarts au cahier installés par les demandes du propriétaire, jamais actés** :
- cérémonie du drapeau de 60 s au lieu du sermon de 20 s (E-08) ;
- cargaison physique libre au lieu de 12 emplacements et 150 kg (ADR-003 B-02) ;
- départ par un seul joueur à la barre, sans vote (§6.5, recette n°8) ;
- pas de taxe de 35 % dans la boucle ADR-007, la cale est comptée sans vente au Percepteur ;
- nageur épuisé tué aussitôt (T-068), alors que §4 prévoit une nage ralentie ;
- voix (L7) livrée avant la validation du noyau.

**Contraintes du cahier.** Aucune publication sans accord explicite du propriétaire (§1, §14). Aucune date ferme avant que L0 à L2 soient mesurés (§11) : les dates ci-dessous sont des cibles de planification, pas des engagements.

**Constats de l'audit.** Le monde a dépassé la boucle : 20 fragments et 104 villages, mais aucune partie jouée à deux machines (91). Il n'y a ni jalon ni lot de sortie (89, 92).

## Options envisagées

1. **MVP = partie en manches du cahier v1.3 (ADR-005)** : lobby, palan, Percepteur, vente, registre, zone sûre.
   - Pour : conforme à §24.
   - Contre : T-112 à T-116 sont presque entièrement à coder, et le propriétaire a réorienté la boucle depuis (ADR-007 puis campagne des 10 jours).
2. **MVP = la campagne des 10 jours, fermée et prouvée à quatre** (option retenue).
   - Pour : elle s'appuie sur ce qui est construit, joué et testé en build. La boucle a déjà un début, une fin (victoire au jour 10 ou exécution) et une sauvegarde.
   - Contre : il faut acter les écarts au cahier et prouver la coop.
3. **Pas de périmètre** : on continue au fil des demandes.
   - Contre : le contenu grossit sans preuve multi-machines, et aucune sortie n'est possible (91, 92).

## Décision proposée : option 2

### 1. Définition du MVP v0.1

> Quatre joueurs se retrouvent par Steam. Ils jouent la campagne des 10 jours de l'arrivée au verdict (victoire ou exécution), quittent puis reprennent leur partie, sans crash ni duplication, en français et en anglais.

Critères de sortie (go). Sauf mention contraire, la preuve exigée est « testé multi-machines » :
1. **Boucle** : les jours 1 à 10 s'enchaînent en solo et à quatre (fin de jour, voyage, Continuer). La recette de la campagne (T-116, réécrite) passe.
2. **Coop** : sessions Steam et invitation (L7a), quatre joueurs au plus, lobby à bord, mort et spectateur, nom et couleur par joueur.
3. **Données sûres** : sauvegarde atomique avec copie de secours (§17). Aucun équipement ni butin dupliqué ou perdu après un crash (UI10).
4. **Performance** : 60 images/s en médiane, 1080p en qualité « Moyen », sur la machine de référence, à quatre joueurs (§12). Paliers de qualité livrés.
5. **Exploitation** : version affichée, rapports de crash, PDB archivés, contenu de test et commandes de triche hors build.
6. **L9 « Sortie »** fermé (section 5).

### 2. Classement du périmètre

**Indispensable (MVP v0.1, accès anticipé)**

| Domaine | Contenu | Tickets |
|---|---|---|
| Boucle | campagne des 10 jours, fin du déluge et barre, écrans de jour et de contrat, lobby à bord, voyage d'île en île à plusieurs, sauvegarde sûre et Continuer, mort et spectateur, recette de la campagne | T-200, T-214, T-221, T-142, T-228, T-173, T-172, T-088, T-115 (avec T-057), T-135, T-116 |
| Coop | test à deux machines, sessions Steam (L7a), `MaxPlayers` 4, version, rapports de crash, nom et couleur par joueur, revue réseau des systèmes serveur | T-133, T-134, T-127, T-028 (réduit), revue à créer |
| Contenu (gelé tel quel) | 3 à 4 îles héros en cartes dédiées (`FRG_Raraka_01` + 2 ou 3), villages et cérémonie, marae et temples, forts, tiki, crabe géant, requin, armes v2, sabre, torche, marchand, caisse | T-145 (partiel), T-194, T-180, T-231, T-211, T-261, T-058, T-246 à T-250, T-177, T-258 |
| Prise en main | tutoriel minimal, HUD lisible, paramètres, remappage et navigation clavier (UI08), anglais, nage animée | à créer, T-235, T-216, T-073, T-138, T-017 |
| Qualité | mesure de performance et paliers, recettes Test Flat et campagne, revues réseau, sécurité et fiabilité | T-091, T-163, T-116, revues à créer |

**Après l'accès anticipé (mises à jour U1 à U4)**
- les fragments restants en cartes dédiées (T-145 au-delà des îles héros) ;
- l'horloge 06h → 22h et ses paliers horaires (T-175) ;
- les zones du galion de l'ADR-007 et la boutique d'outils (T-174) ;
- les skins modulaires complets (T-028) ;
- le ragdoll actif, s'il est choisi (T-027) ;
- la voix de proximité, si elle n'est pas prouvée à J2 (T-263) ;
- cyclone et pluie (suite de T-203), Rôdeur des Ronces (L8) ;
- contrats à plusieurs offres, manette et Steam Deck, succès ;
- écume du récif (T-014), mémoire des îles (T-128).

**À couper** (proposé)
- l'éditeur d'îles E1 (§20) avant la 1.0 ; huit joueurs, serveurs dédiés et consoles ;
- `Variant_*` et les Mannequins du build ;
- `Lvl_Raraka` du build, dès que les îles héros ont leur carte ;
- le vol au double Espace dans le build Shipping (T-021) ;
- le repositionnement scripté entre trois mouillages (§6.4, n°19) : la barque se mène à la rame (ADR-003 B-01) ;
- `L_Galleon` séparée et la zone sûre de l'ADR-005, remplacées par l'ADR-007.

**À trancher par le propriétaire**
- **Sensibilités (T-139)** :
  - marae pillables (perle de 1 500 $, offrandes) ;
  - villageois tuables et découpables ;
  - « tiki » (le nom public « Veilleurs » est proposé par la bible T-137) et « Raraka » dans les textes publics.
- **Écarts au cahier** :
  - sermon de 20 s ou cérémonie de 60 s (E-08), et bonus de +120 s ;
  - arrimage à emplacements ou cargaison libre (B-02) ;
  - vote de départ (§6.5) ;
  - taxe de 35 % (§7) ;
  - mort immédiate du nageur épuisé (T-068 contre §4).

### 3. Gel du monde (proposé)

Jusqu'à J1, on n'ajoute aucune île, aucun bâtiment, aucune créature, aucune arme ni aucune mécanique. Le travail se limite à :
- corriger ;
- fermer la boucle, la coop et la sauvegarde ;
- mesurer la performance et produire les preuves.

Une nouvelle demande du propriétaire en remplace une autre du même jalon. Chaque lot garde au plus 15 tickets `review`. Chaque ticket porte son lot (L0 à L9) et son jalon (J0 à J6).

### 4. Jalons (cibles indicatives, à confirmer)

| Jalon | Cible | Limite | Contenu | Preuve exigée | Si manqué |
|---|---|---|---|---|---|
| J0 Tronc propre | 2026-10-17 | 2026-10-24 | décisions de cette ADR et du tri ; commits par lots (T-164) ; copie hors machine confirmée et worktrees fusionnés retirés (T-132) ; sort de T-131 tranché | `git log` propre, kit OneDrive vérifié | pas de nouveau code |
| J1 Tranche verticale | 2026-11-30 | 2026-12-14 | jours 1 à 3 en solo et à deux sur deux machines (LAN) ; sauvegarde sûre et Continuer ; performance mesurée (T-091) ; revue réseau sans MAJEUR ; recette des jours 1 à 3 passée ; 10 jours d'affilée sans crash en solo | testé multi-machines et testé build | gel prolongé, rien de public |
| J2 Playtest Steam privé | 2027-03-15 | 2027-04-26 | Steam (L7a, NET01), lobby à quatre, spectateur, anglais, tutoriel ; 8 testeurs externes, au moins 90 % de sessions sans crash | testé multi-machines, sur des réseaux distincts | démo repoussée |
| J3 Démo publique | Next Fest de juin 2027 : soumission vers le 2027-05-10, fête vers le 14–21 juin (dates Valve à revérifier) | fixe | démo des jours 1 à 3, page Steam, portes L9 REL01 à REL05 fermées | testé build par des joueurs externes | pas de Next Fest |
| J4 Accès anticipé | 2027-08-17 | 2027-09-28 | MVP v0.1 complet (§1), L9 complet ; quatre joueurs 60 min sans désynchronisation ; moins de 2 % de sessions avec crash | testé multi-machines, accord écrit du propriétaire | la date recule, jamais le critère |
| J5 Mises à jour | U1 à EA + 1 mois, U2 à + 3, U3 à + 5, U4 à + 7 | — | liste « Après l'accès anticipé » ; hotfix en 72 h au plus (crash, duplication, perte de sauvegarde) | Automation, smoke test packagé, deux machines | contenu reporté |
| J6 Version 1.0 | EA + 9 à 12 mois (mai à septembre 2028) | — | fin de partie, 8 à 10 îles, avis au moins à 80 % positifs | build public | reste en accès anticipé |

Le Next Fest de février 2027 est abandonné. Il demande la page et la démo vers le 10 janvier, avec J1 et J2 déjà tenus. Les dates de Valve sont celles de l'audit 89, à revérifier sur Steamworks.

### 5. Lot L9 « Sortie »

Nouveau lot, après L6 et L7a, avec sa recette :

| ID | Vérification | Résultat attendu |
|---|---|---|
| REL01 | Nom et identité | nom libre vérifié (INPI, EUIPO, USPTO), le même au menu, dans `ProjectName`, dans `Game.Title` et sur la page |
| REL02 | Licences des données | DAG (cocoteraies), DAF et dérivés Maxar, IRD, Te Fenua : autorisation écrite, ou couche retirée du build ; inventaire de T-130 à jour |
| REL03 | Licences des contenus | musiques (T-220), sons (T-233, T-234), polices et textures : licence prouvée ; crédits complets (§23) |
| REL04 | Juridique | EULA, politique de confidentialité et mentions relues par un juriste ; entité vendeuse ; compte Steamworks |
| REL05 | Steam | AppID, page, capsules, bande-annonce, questionnaire de contenu (classification) |
| REL06 | Build Shipping | version affichée, rapports de crash, PDB archivés, exécutable signé ; plugins MCP, cartes de test et commandes `monke.*` absents |
| REL07 | Performance | paliers de qualité, premier lancement en « Moyen », 60 images/s en médiane en 1080p sur la machine de référence (§12) |
| REL08 | Langues | français et anglais complets, aucun texte tronqué (UI12) |
| REL09 | Support | procédure de hotfix en 72 h, notes de version, canal de retour des joueurs |
| REL10 | Accord et revues | accord écrit du propriétaire avant toute publication (§1, §14) ; revues d'architecture, de sécurité et de mémoire sans CRITIQUE ni MAJEUR |

### 6. Décisions du propriétaire à confirmer

Questions fermées. La liste complète (Q1 à Q30), avec les questions d'exploitation (commits, worktrees, test à deux PC), est dans le rapport du tri du 2026-10-10 (`docs/production/triage-2026-10-10.md`, enregistré par la session principale).

| # | Question | Défaut proposé | Débloque |
|---|---|---|---|
| D1 | Adoptez-vous cette ADR : MVP v0.1 = campagne des 10 jours à quatre par Steam ? | oui | tout le plan |
| D2 | Gelez-vous le monde jusqu'à J1 (§3) ? | oui | T-145, T-174, T-175 |
| D3 | « Taopao : Rush Island » est-il le nom de travail à vérifier avant toute page publique ? | oui | REL01 |
| D4 | Visez-vous le Next Fest de juin 2027 plutôt que celui de février ? | juin | J3 |
| D5 | Lance-t-on les sessions Steam (L7a) juste après J1, avant L6 ? | oui | T-134 |
| D6 | L'anglais est-il livré avec la démo ? | oui | T-138 |
| D7 | Le jeu sort-il en premium (environ 7,99 $) en accès anticipé, sans microtransaction ? | oui | REL04, REL05 |
| D8 | Créez-vous l'entité vendeuse et le compte Steamworks avant le 2026-12-01 ? | oui | REL04 |
| D9 | La cérémonie du drapeau (60 s) remplace-t-elle définitivement le sermon de 20 s (E-08) ? | oui | T-054, T-131 |
| D10 | Garde-t-on le bonus de +120 s par zone pacifiée (une journée dépasse alors 10 min) ? | non | T-126, `DA_ExpeditionRules` |
| D11 | La cargaison physique libre remplace-t-elle l'arrimage à 12 emplacements et 150 kg (B-02) ? | oui | T-055, T-131 |
| D12 | Le départ reste-t-il décidé par un seul joueur à la barre, sans vote d'équipage (§6.5) ? | oui | T-112 |
| D13 | Abandonne-t-on la taxe de 35 % dans la campagne ? | oui | T-056 |
| D14 | Ragdoll passif assumé (oui) ou ragdoll actif façon Fall Guys (non) ? | oui | T-027 |

## Conséquences

- **Si l'ADR est acceptée** :
  - créer au chantier les tickets L9 (REL01 à REL10), « tutoriel minimal », « paliers de qualité », « revue réseau » et « revue sécurité » ;
  - étiqueter les tickets ouverts avec leur jalon ;
  - remplacer dans le backlog la liste « Partie en manches » (ADR-005, étapes 4 à 8) par le MVP v0.1 ;
  - reporter dans ADR-001 les écarts tranchés (E-08 confirmé, nouveaux E-09 et suivants).
- **Cahier** : une version 1.5 consignerait la campagne des 10 jours et le lot L9. Cette version ne vaut qu'avec l'accord du propriétaire.
- **ADR-005** : les étapes 4 à 8 sont remplacées par l'ADR-007 et la campagne. Seuls `FRunCalculator` (T-109) et le galion (T-110, T-111) restent en service.
- **Usine** : les portes de `FACTORY.md` ne changent pas. Un ticket ne passe `fait` qu'avec une preuve, et un jalon n'est tenu qu'au niveau de preuve indiqué dans le tableau des jalons.
