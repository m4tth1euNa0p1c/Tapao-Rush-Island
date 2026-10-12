# Audit commercial — synthèse des 100 rapports (2026-10-04)

Demande du propriétaire : « optimiser un max la place, etc., pour que cela devienne un jeu commercial ; 100 agents qui challengent technique, design, logique, gameplay, marketing… pour améliorer l'idée et résoudre les trucs de jeu manquants ». Tâche T-117.

- **Méthode** : 100 auditeurs, un angle chacun (`LENSES.md`, domaines A à J), en lecture seule. Chacun a écrit un rapport `NN-slug.md` d'au plus 900 mots, au format du `BRIEF.md`. Ils distinguent ce qu'ils ont vu (fichier, ligne, chiffre) de ce qu'ils supposent.
- **Limites** : personne n'a joué. Toutes les durées, coûts et performances sont lus ou calculés, jamais mesurés en jeu. Les recherches web (marché, juridique) sont des extraits, à faire vérifier par un professionnel.
- **Constats dépassés depuis l'écriture** : certains rapports ont été écrits avant ces livraisons du 2026-10-04 :
  - les packages Development de la session Codex (T-120, T-124) ;
  - la première île en carte séparée `L_Frag_Raraka_01` (T-089) ;
  - le verrou de l'éditeur partagé (`scripts/editor_lock.py`) ;
  - le correctif « une défaite ne crédite rien » (T-109) ;
  - la partie de 10 min, avec le déluge à 9 min (cahier v1.4, T-126).
  
  Les chiffres « 120 s » ou « 300 s » d'avant la submersion sont donc périmés.

## Verdict

1. **Le jeu est un bac à sable riche, pas encore une partie vendable** (11, 91). Le monde est très avancé : atoll 1:1, 20 fragments, 104 villages, barque physique, galion, tiki, météo. Mais la boucle ne se ferme pas :
   - la barre, le palan, la vente et le quota ne sont pas codés ;
   - le sermon dort sur une branche ;
   - la coop n'a jamais été jouée sur deux machines, et aucun ami ne peut la rejoindre.
2. **Le seul avantage défendable est la fiction de la Couronne**, avec le Percepteur, le sermon, la défaite « traître » et la satire du Bureau (71, 74, 81). Elle est aujourd'hui invisible à l'écran, et le nom « Crownfall » est déjà pris (78). La barque qui coule et la montée des eaux ont trois quasi-jumeaux sur Steam en 2026 (81).
3. **Trois risques peuvent tuer le projet avant le marché** (93) :
   - perdre le dépôt : aucune copie hors machine, et un disque C: à 99 % ;
   - une affaire juridique ou de réputation : données de terrain sous licence restreinte dans un build diffusé, lieu réel habité, sacré pillé ;
   - une coop jamais prouvée.
4. **Trajectoire réaliste** (89, 92, 84) :

   | Jalon | Date visée |
   |---|---|
   | Manches jouables en solo | 2026-11-22 |
   | Sessions internet | 2027-01-08 |
   | Page Steam, après six portes fermées (nom, capsule, trailer, anglais, lexique, questionnaires) | 2027-01-18 |
   | Démo coop de deux manches au Steam Next Fest | 2027-06-14 (un seul Next Fest par jeu) |
   | Accès anticipé | 2027-08-17, au plus tard 2027-09-28 |

   Rien de commercial avant que quatre amis puissent lancer, rejoindre et finir une manche.
5. **Argent** (85, 90, 99) :
   - **Prix et revenus** : 7,99 $ en accès anticipé. Le développeur touche environ 38 % du prix affiché. Une sortie au plancher (7 000 wishlists) rapporte 5 à 8 k€ en semaine 1 : bâtir le plan sur ce plancher, pas sur la viralité.
   - **Budget** : une early access crédible coûte environ 17 k€ au plancher, 28 k€ en recommandé et 50 k€ en confortable.
   - **Métiers sans titulaire** : neuf métiers n'ont personne et ne se confient pas à des agents. Ce sont la DA, la musique, la voix, les VFX, le droit, la consultation culturelle, la communauté, la QA multi-machines et les playtests.

## À faire cette semaine (vague 0)

| # | Action | Pourquoi | Rapports |
|---|---|---|---|
| 1 | `git bundle` et copie de `.git/lfs` hors de C: (2e disque ou 2e PC), puis suppression des worktrees fusionnés (≈ −34 Go, accord du propriétaire) | Une panne de disque efface le projet ; il reste 17 Go libres | 31, 39, 41, 92, 93 |
| 2 | Geler la diffusion du build investisseurs (T-120) ; écrire à la DAG (cocoteraies) et à la DAF (dérivés Maxar, licence par couche) | Les cocoteraies DAG sont réservées à la recherche et à l'éducation (≥ 22 431 des 43 057 cocotiers). Des masques dérivés de Maxar sont embarqués | 70, 73, 97 |
| 3 | Fusionner T-054, T-055 et T-057 (sermon, arrimage, sauvegarde) dans `main` et lancer leurs ≈ 69 specs | Le cœur du jeu n'a jamais été joué : `AddZoneBonus` n'a aucun appelant | 03, 11, 39, 89, 92 |
| 4 | Premier test hôte + client sur deux PC (package `build/rec-T124`, LAN) | Zéro preuve « testé multi-machines » | 58, 92, 94 |
| 5 | Trancher les décisions ci-dessous (nom, lieu réel, tiki et marae, MVP) | Elles conditionnent le logo, la page Steam, le trailer et le contenu | 72, 73, 78, 91 |

## Les bloquants commerciaux, regroupés

| # | Bloquant | En bref | Rapports | Effort |
|---|---|---|---|---|
| 1 | **La manche ne se termine pas** | Seul `FRunCalculator` existe. Barre, vote, palan, cale, voyage seamless, zone sûre, Percepteur, vente et défaite manquent (T-112 à T-116) | 01, 11, 19, 32, 54 | L |
| 2 | **La mécanique centrale est hors de `main`** | Sermon et pacification sur `dev/T-054`. En l'état, 20 s immobile sans enjeu : à prototyper en trois variantes | 03, 08 | M |
| 3 | **La coop n'est ni joignable ni prouvée** | Il manque : <br>• héberger ou rejoindre, `USessionSubsystem`, Steam et voix ; <br>• `PlayerState` et un plafond à 4 joueurs (le moteur en admet 16). <br>Aucun test à deux machines, seamless travel non configuré, barque et butin simulés par chaque machine | 12, 27, 32, 34, 52, 56, 58 | L |
| 4 | **Le projet n'est pas protégé** | Aucun remote, disque à 99 %, 21 worktrees, plusieurs agents (Claude et Codex) sur un seul éditeur. Le verrou existe depuis le 2026-10-04 | 31, 39, 93 | S |
| 5 | **Exploitation impossible** | Assets chargés par chemin texte (risque de cuisson), aucun crash reporting, pas de version affichée, PDB non archivés, exe non signé. Le journal du package T-124 contient un ensure et 2 234 avertissements | 37, 48, 94, 96 | M |
| 6 | **Performances jamais mesurées** | L'atoll entier est chargé pour environ 4 % de terre jouée. Ultra et Lumen matériel par défaut, aucun palier de qualité, hôte jamais mesuré à quatre. Le 60 FPS n'est pas démontré | 42, 44, 45, 47, 49, 50, 51 | L |
| 7 | **Pas d'identité** | Nom pris, quatre noms qui coexistent (« TAPAO : Rush Island » à l'écran), aucune DA écrite ni étalonnage. Quatre beans identiques, pas de Percepteur, aucun visuel de vente | 61, 68, 69, 71, 74, 76, 78 | M |
| 8 | **Juridique et réputation** | Licences de données non prouvées, lieu réel (réserve de biosphère UNESCO), marae pillables, « tiki » monstres, villageois tuables et découpables. Aucune consultation, aucune entité vendeuse, pas de compte Steamworks ni d'EULA | 70, 72, 73, 75, 80, 97 | M |
| 9 | **Un jeu muet et sans effets** | Zéro Niagara, zéro musique, zéro voix. Le butin n'a aucun retour, la tempête n'a ni pluie ni éclairs, le « ragdoll actif » n'est pas fait (T-027) | 24, 65, 66, 67 | L |
| 10 | **Premières minutes hostiles** | Pas de tutoriel, HUD sans quota ni cale. La mort est une expulsion : jusqu'à 10 min d'attente avec la partie de 10 min, sans spectateur. Menus aux deux tiers du §16, 4 actions sur 28 à la manette, accessibilité nulle | 09, 13, 22, 23, 25, 26, 28, 30 | M |
| 11 | **Économie et rejouabilité** | Butin multiplié par 79 sans retoucher la courbe, perle noire qui court-circuite le quota. Le contrat se réduit au quota, la boutique est vide, pas de méta-progression ni de fin. Variété « de position » : une île rejouée dans 90 % des séries de 10 manches | 02, 05, 07, 10, 14, 17, 19, 63, 77 | M |
| 12 | **Rien côté marché** | Pas de page Steam, pas d'anglais (le français ≈ 2 % des clients Steam), pas de plan de sortie, de télémétrie ni de communauté | 79, 82, 84, 87, 89, 90, 95 | M |

## Place, flux et optimisation

### Disque du poste de développement (41, 39, 38)

| Levier | Gain estimé |
|---|---|
| Supprimer les worktrees fusionnés (`worktree.py remove`) | ≈ −33,6 Go |
| `.vs`, Autosaves, `avant_pente`, puis `git lfs prune` | ≈ −4,8 Go |
| LFS : 3,64 Go stockés pour 2,25 utiles ; 44 Mo de JSON recopiés trois fois | ≈ −1,4 Go |
| 48 DataTables de végétation (génération PCG) | −173 Mo, aussi sur le build |

### Build livré (41, 48, 70)

- **Taille estimée** : 0,9 à 1,5 Gio. Elle n'a jamais été chiffrée sur les packages T-120 et T-124 : à mesurer.
- **Exécutable** : 319 Mo, car les plugins MCP sont dans la cible Game. Mettre `TargetAllowList: ["Editor"]` sur les plugins MCP.
- **Symboles** : sortir les PDB (377 Mo) du build et les archiver par version.
- **Contenu à retirer du cook** :
  - le template résiduel (148 Mo, `Variant_*`, Mannequins) ;
  - `Lvl_Raraka` (atoll entier, village réel) dès que les cartes `L_Frag_*` le remplacent ;
  - les DataTables de génération.
- **Textures** : UI non compressée, doublons, formats NPOT et 2K sur de petits objets.

### Mémoire, processeur et rendu (42, 44, 45, 49, 51)

- **Charger une île, pas l'atoll** : T-089 l'a fait pour FRG_01, il reste 19 fragments. Retirer aussi la collision complexe sous −5 m.
- **Végétation** : le Naupaka pèse 73 % des triangles. Mettre Nanite `KeepPercentTriangles` à 25 % et mesurer.
- **Objets physiques** : réduire à 40 cadavres au plus (contre 80) et à 1 500 butins au plus.
- **IA tiki** : la mettre en sommeil loin des joueurs. Le vent ne doit plus recréer son rendu à chaque image.
- **Lumières** : au plus 8 lampes à ombres par fragment (66 aujourd'hui) ; soleil mis à jour à 4 Hz plutôt que 20 Hz.
- **Rendu** :
  - premier lancement en « Moyen », sans flou de mouvement ;
  - paliers de qualité (`DefaultScalability.ini`) ;
  - Lumen matériel réservé à « Épique » ;
  - une machine de référence et une mesure packagée (T-091).

### Réseau (34, 46, 53)

- Mettre `SetNetUpdateFrequency` à 20–30 Hz sur le butin, les tiki et la barque (100 Hz par défaut), et activer la fréquence adaptative.
- La cargaison arrimée et le combustible de la torche sont répliqués à chaque image : répliquer seulement l'heure de fin ou le changement.
- Régler `MaxInternetClientRate`, et renvoyer périodiquement l'entrée des rames (un paquet perdu la bloque).

### Flux et temps morts (21, 47, 54)

- **Aujourd'hui** : 26,7 s de JOUER à la prise en main, pour 1,6 s de chargement réel (PIE), et ≈ 47 s du lancement au premier geste. Relancer après une mort coûte au moins 28 s.
- **Leviers** :
  - plancher de rideau de 6,5 s appliqué deux fois ;
  - détour par `L_Boot` : 7,2 s ;
  - solo avec `?listen` : 7 s, et un port ouvert ;
  - splash ;
  - pas de préchargement ni de PSO embarqués.
- **Cible** : ≤ 15 s de JOUER au contrôle, et une spec qui échoue si les planchers cumulés dépassent 4 s.
- **Effets de la partie de 10 min** (T-126) :
  - le mort attend jusqu'à 10 min : spectateur ou rappel sur le pont indispensables (28) ;
  - une seule rafale de vent avant le déluge ;
  - la tempête tombe en plein jour dans 47 à 81 % des parties, selon l'heure de départ aléatoire (64). Caler le ciel sur l'horloge d'expédition ou annoncer la tempête 25 à 30 s avant (15, 64).

## Le MVP vendable (89, 91)

**Quatre amis invités par Steam, un galion, 3 à 4 îles, 5 manches, avec tutoriel, sauvegarde et anglais.** Geler le monde (fragments, villages, décor) tant que RUN10 n'est pas joué sur deux machines.

| Priorité | Contenu |
|---|---|
| **P0 (avant tout test externe)** | • Boucle fermée (T-112 à T-116) et sermon jouable (T-054). <br>• Sessions Steam et invitation (L7a), plafond à 4. <br>• `PlayerState` : noms, couleurs, morts. <br>• Spectateur et bilan de manche. <br>• Percepteur et trois scènes Couronne (vente, édit, frégate). <br>• Paliers de qualité, crash reporting et version. <br>• Nom libre, anglais, tutoriel minimal. |
| **P1 (avant l'accès anticipé)** | • Communication : voix de proximité, pings, emotes. <br>• Ambiance : musique adaptative, Niagara (mer, tempête, découpe en sève). <br>• Économie : contrats à trois offres, boutique, méta horizontale (profil, cosmétiques gagnés). <br>• Mesure : télémétrie locale avec consentement. <br>• Plateformes : manette et Deck « Playable ». |
| **À reporter ou couper** | Éditeur d'îles E1, 8 joueurs, factions, cyclone et tsunami, marae pillable, villageois létaux, `Lvl_Raraka` et `Variant_*` dans le build |

## Quick wins (moins d'un jour chacun)

| # | Action | Rapports |
|---|---|---|
| 1 | `MaxPlayers=4` (`[/Script/Engine.GameSession]`) et vol (double Espace) interdit en Shipping | 27, 40, 57, 59 |
| 2 | `TargetAllowList: ["Editor"]` sur les plugins MCP, `DirectoriesToAlwaysCook` et `MapsToCook` à vérifier sur le package | 31, 37, 41, 48 |
| 3 | Premier lancement en « Moyen », `r.DefaultFeature.MotionBlur=0`, lissage souris coupé | 23, 25, 26, 45 |
| 4 | « NAUFRAGÉ » seulement si toute l'équipe est morte, spectateur brut (suivre un vivant), `EDeathCause` | 09, 12, 28 |
| 5 | Trois sons du compteur (`DA_HudTimerRules`), son et chiffre flottant sur `OnValueChanged`, bannière à chaque phase | 13, 15, 24 |
| 6 | Noms au-dessus des équipiers, teinte de tête par joueur | 08, 18, 76 |
| 7 | Astuces de chargement réécrites en « Circulaires du Bureau » | 13, 20, 74 |
| 8 | Remplacer « TAPAO : Rush Island » (logo, `Game.Title`, `ProjectName`) par le nom de travail, et la version au menu | 61, 68, 71, 87, 96 |
| 9 | Contenu des marae en `NoGrab`, villageois non létaux et non découpables, aucun rouge organique | 72, 80, 91 |
| 10 | Le poing n'affecte plus un équipier ; « Recommencer » est confirmé et grisé si des équipiers vivent | 54, 57 |
| 11 | JOUER en solo sans `?listen` : −7 s et aucun port ouvert | 21, 40 |
| 12 | `AimedFallChance` passe de 0,5 à 0,15 ; `bExhaustionKillsInWater` passe à `false` | 09, 15 |
| 13 | `SetNetUpdateFrequency` 20–30 Hz et `net.UseAdaptiveNetUpdateFrequency=1` | 34, 46 |
| 14 | Vérifier « Used with Skeletal Mesh » sur `M_Bean_Master` (manche rouge perdue dans le package) | 69, 76, 86 |
| 15 | Mémoire des 8 dernières îles dans le tirage : 9,9 îles distinctes sur 10 manches au lieu de 8,3 | 63 |
| 16 | Exporter les valeurs effectives des DataAssets en JSON versionné (les valeurs jouées divergent des défauts C++, des fiches et des tests) | 33 |

## Décisions qui reviennent au propriétaire

1. **Nom** : « Crownfall » est pris (un jeu Steam de 2017 et un événement Dota 2 de 2024). Candidats à vérifier : « Drowned Assets » et « Sinking Fund », par recherche INPI, EUIPO et USPTO (78).
2. **Lieu réel ou archipel fictif** (écart E-02) : faut-il retirer « Raraka » et « Motutapu » des textes publics et l'atoll du build (72, 73, 97) ?
3. **Sacré et violence** : marae pillés, « tiki » monstres (« Veilleurs » ?), villageois tuables. Garder, renommer ou couper (72, 75, 80) ? Faut-il payer deux conseillers polynésiens avant le trailer (72) ?
4. **Ragdoll** : actif façon Fall Guys (T-027), ou passif assumé (67) ?
5. **Périmètre** : geler le monde jusqu'à RUN10 et adopter un ADR « MVP v0.1 » (91) ?
6. **Ordre des lots** : sessions Steam et invitation avant la V1 (L7a avant L6), et voix avant l'accès anticipé (56, 83, 88) ?
7. **Bonus de pacification** : garder +120 s par zone (une partie dépasse alors 10 min) ou passer à 0 (T-126) ?
8. **Modèle économique** : premium en accès anticipé à 7,99 $, sans microtransaction, avec des cosmétiques gagnés (85) ?
9. **Structure juridique** (SAS ou micro-entreprise) et compte Steamworks (100 $, 21 jours de délai) (97, 98).
10. **Langue** : anglais livré avec la démo, le français restant la langue d'écriture (79, 82) ?

## Plan proposé, par vagues

| Vague | Horizon | Contenu |
|---|---|---|
| 0 | Cette semaine | Sauvegarde hors machine, purge du disque, gel du build diffusé et courriers de licence, fusion T-054/055/057, premier test à deux PC, décisions |
| 1 | Fin novembre 2026 | Boucle des manches (T-112 à T-116), sermon, tutoriel minimal, spectateur, bilan, paliers de qualité, mesure sur machine de référence (T-091) |
| 2 | Janvier 2027 | Sessions Steam et invitation (L7a), identité des joueurs, anglais, crash reporting et version, playtests entre amis (portes G1 à G4, rapport 94) |
| 3 | Juin 2027 (démo) | `STYLE.md` et étalonnage, Percepteur et scènes Couronne, musique, Niagara, contrats et boutique, 3 à 4 îles héros, page Steam en anglais, capsule et trailer (69, 84, 86) |
| 4 | Août à septembre 2027 (accès anticipé) | Voix de proximité, manette et Deck, télémétrie avec consentement, Discord public, volet légal complet |

## Tâches créées au chantier (2026-10-04, source « audit T-117 »)

À la demande du propriétaire, l'exécution est confiée à Codex pour économiser les tokens Claude. Deux sessions `codex exec` sont lancées à 05:27 par monk-island-78 : la première porte T-131, la seconde T-128, T-129, T-130 et T-127.

| Tâche | Contenu | Porteur | Rapports |
|---|---|---|---|
| T-127 | Exploitation : version, crash reporting, PDB, plugins MCP en éditeur seulement, `MaxPlayers=4` | Codex | 27, 37, 48, 96 |
| T-128 | Tirage des îles : mémoire des 8 dernières | Codex | 10, 63 |
| T-129 | Journaux structurés : `EDeathCause`, build et hash des règles, `log_events.py` | Codex | 28, 95 |
| T-130 | Brouillons légaux et inventaire des licences | Codex | 70, 73, 97 |
| T-131 | Intégration de T-054, T-055 et T-057 dans `main` | Codex | 03, 11, 39 |
| T-132 | Sauvegarde hors machine et purge des worktrees fusionnés | lead, **accord du propriétaire** | 39, 41, 93 |
| T-133 | Premier test hôte + client sur deux machines | lead + propriétaire (2e PC) | 58, 94 |
| T-134 | Spike Steam (L7a) | ue-network-engineer, **accord du propriétaire** (Build.cs) | 56, 98 |
| T-135 | Mort et spectateur | ue-ui-developer (avec 19) | 09, 28 |
| T-136 | ADR « MVP v0.1 », jalons, lot L9 | ue-producer | 89, 91, 92 |
| T-137 | Bible d'une page, ton, Percepteur | ue-narrative-designer | 71, 74, 75 |
| T-138 | Localisation et anglais | ue-ui-developer (avec 19) | 79 |
| T-139 | Sensibilités culturelles | ue-game-designer, **décision du propriétaire** | 72, 80 |

Existantes à tenir : T-091 (mesure de performance), T-112 à T-116 (boucle des manches).

## Suite

- **Audit récurrent** : relancer chaque semaine les 10 à 20 angles touchés par les livraisons de la semaine, en lecture seule. Les rapports sont datés, et la synthèse indique ce qui est résolu.
- **Rapports à lire en premier** : 11 (écart au cahier), 91 (MVP), 93 (risques), 100 (avocat du diable), 89 et 92 (sortie et roadmap), 72 et 73 (sensibilités et lieu réel), 41 (place).
