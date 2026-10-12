# Feature : Campagne des dix premiers jours (paliers et contenu débloqué)

- **Statut** : en cours. Socle, boucle des jours (T-214), renforts et crabes codés, compilés, testés (526 tests) et vérifiés en PIE le 2026-10-07 ; restent le fort tiré au hasard dès le jour 6 et les fusils de la « 6e vague » (voir « État de T-214 »). **2026-10-09 (T-252)** : forts et temples à artefacts au jour 3, courbe de difficulté par jour — **code écrit, non compilé** (voir la dernière section).
- **Owner** : à attribuer (ue-producer)
- **Date** : 2026-10-07
- **Cahier des charges / lots** :
  - L4 : partie, quotas, contenu par jour ;
  - L5 : forts, soldats français, crabes géants, météo ;
  - L6 : cinématique d'arrivée.
- **Rapport avec l'ADR-007** : la boucle reste galion hub, jours et contrats. Ces paliers fixent les dix premiers jours. Il faut arbitrer s'ils remplacent la table des zones de l'ADR-007 (1 800 / 3 500 / 6 500 / 10 000 / 18 000).

## Demande (mots du propriétaire)
> « Dans la boucle du jeu, on commence dans le menu, on lance une partie ; si en solo : cinématique d'arrivée du galion, plan cinéma de son arrivée vers l'île, on pop sur le galion. Pour l'instant on commence simple : un village, des villages par terre, on doit récolter 5 500 pour le 3e jour ; pour l'instant sur les Tuamotu on trouve des villages, au moins des marae. Le 3e [jour], le premier temple artefact : parmi les 6 artefacts, un apparaît sur le socle ; dès que je le saisis, la météo change en 10 secondes, vent violent, pluie. Jusqu'au 6e jour on doit ramener 10 500. Le 6e jour les forts français apparaissent ; à l'intérieur, des beans français en full bleu foncé, en uniforme français colonial, armés du gun ; il me tire dessus ; il y en a 8 au moins positionnés à l'intérieur, équipés d'un gun 6 coups et d'un sabre ; dès qu'ils n'ont plus de munitions, ils m'attaquent. Génère un bean full bleu foncé, la tenue coloniale époque française, et récupère les animations du bean. Lors de ce 6e jour jusqu'au 10e jour : 20 500 ; entre-temps, ces jours-là, le temps sera élargi ; maintenant tous les bâtiments apparaissent, et le reste des temples artefacts aussi. Maintenant, entre ces 3 jours, des crabes géants peuvent sortir du sable et me charger pour m'attaquer ; quand je suis accroupi dans les buissons, ils ne me voient plus. »

## Déroulé
**Lancement (solo)** : menu → Jouer → Solo → cinématique d'arrivée du galion → le joueur apparaît sur le pont du galion. La cinématique est faite de plans de cinéma montrant le galion qui approche de l'île.

**Jours 1 à 3**
- Objectif : 5 500 à la fin du jour 3.
- Sur l'île (fragments des Tuamotu) : villages et marae, au moins.
- Danger : les tiki actuels.

**Jour 3**
- Le premier temple reliquaire apparaît. Un seul des 6 artefacts légendaires est posé sur son socle.
- Quand on saisit cet artefact, la météo tourne en 10 s : vent violent et pluie.

**Jours 4 à 6**
- Objectif : 10 500 à la fin du jour 6.

**Jour 6**
- Les forts français apparaissent. Au moins 8 soldats français sont postés à l'intérieur de chaque fort.
- Les soldats tirent avec un pistolet à 6 coups, puis attaquent au sabre une fois leurs munitions épuisées.

**Jours 6 à 10**
- Objectif : 20 500 à la fin du jour 10.
- Le temps de l'expédition est allongé.
- Tous les bâtiments apparaissent, ainsi que les autres temples reliquaires (les 5 artefacts restants).
- Des crabes géants sortent du sable et chargent. Un joueur accroupi dans un buisson est invisible pour eux.

Toutes les valeurs vont en DataAsset (`DA_RunRules`, règles des sites et des créatures), jamais en dur : jours, quotas, durée, contenus par jour, délai de 10 s de la météo, nombre de soldats, munitions.

## Briques existantes à réutiliser
- **Partie et jours** : `URunSubsystem`, `FRunContractCalculator`, `DA_RunRules` (ADR-007). Galion hub : `L_Galleon`, `UGalleonStartSubsystem`.
- **Villages, marae, temple** :
  - `AVillageSpawner`, `ACampSpawner` (marae, temples : T-180) ;
  - `AMaraeSite` + `DA_TempleReliquaryRules` (quatre artefacts sur l'autel aujourd'hui) ;
  - les 6 artefacts de `/Game/MonkeIsland/Items/PolynesianArtifacts/`.
- **Météo** : `AWindStormActor` (coups de vent, tempête finale), `ADayNightCycleActor`. Saisir l'artefact d'un temple fait tourner la météo en 10 s (T-203) : vent violent, pluie, ciel sombre et sons, jusqu'à la fin de la journée. Réglages dans `DA_WeatherRules` ; détail dans `storm-flood.md`, section « Météo qui tourne ». Code compilé, pas encore vu en éditeur.
- **Fort français** : assets importés par Codex dans `/Game/MonkeIsland/Environment/FortFrench/` (T-193, T-198).
- **Bean** :
  - générateur `scripts/character/bean_soldier.py` (squelette `SKEL_Bean` partagé : les animations du bean servent telles quelles) ;
  - armes du bean (pistolet de pirate, mousquet, sabre) et leurs animations (maquettes `bean-*`).
- **Crabes** : `SK_CrabGuardian` (créature tiki gardienne), `ATikiDirector` (sortie des buissons). L'accroupi existe (`bIsCrouched`), ainsi que les buissons connus du directeur (`CacheBushes`).

## Questions ouvertes (à trancher avec le propriétaire)
1. Les objectifs 5 500, 10 500 et 20 500 sont-ils des totaux cumulés depuis le jour 1, ou chaque période a-t-elle son propre objectif ?
2. « Le temps sera élargi » : de combien la journée s'allonge-t-elle à partir du jour 6 ? Le déluge vient-il plus tard ?
3. « Tous les bâtiments » : les villages complets, les forts, les temples, les marae, tout à la fois ?
4. Que se passe-t-il après le jour 10, et en cas d'objectif manqué (ADR-007 : contrat échoué) ?
5. Multijoueur : la cinématique d'arrivée est-elle jouée aussi pour l'hôte et ses invités ?
6. Le « gun 6 coups » : un pistolet de l'époque à 6 coups (poivrière) ou le pistolet de pirate existant avec 6 balles ?

## Décisions provisoires (lead, 2026-10-07) et demandes suivantes du propriétaire
- **Paliers** : trois contrats de l'ADR-007 dans `DA_RunRules`.
  - Jours 1 à 3 : 5 500 à cumuler.
  - Jours 4 à 6 : 10 500.
  - Jours 7 à 10 : 20 500. Ce palier dure 4 jours, et ses journées sont allongées (déluge ×1,5 plus tard).
  - La table à cinq zones de l'ADR-007 n'est plus la valeur par défaut. Le jour de campagne est le nombre de jours joués + 1, quel que soit le contrat.
- **Jours d'ouverture** (`DA_RunRules.Campaign`, `RunCampaign.h`) :

  | Jour | Contenu |
  |---|---|
  | 1 | premier temple, avec un des 6 artefacts sur son socle ; crabes géants (demande du 2026-10-08 : « fais apparaître le temple artefact, les crabes, les forts dès le jour 1 » ; avant : temple au jour 3, crabes au 4, forts au 6) |
  | 3 | forts français sur la terre du fragment et les autres temples à artefacts (3 par fragment, un artefact chacun) — demande du 2026-10-09 : « ajoute au 3e jour les forts français sur Raraka et les temples artefacts » (les forts étaient au jour 1 depuis le 2026-10-08, les autres temples au jour 7) |
  | 7 | tous les bâtiments (camps de pêcheurs) |

  - Sans partie en cours (cartes de test, PIE direct), tout est ouvert comme avant.
- **Artefacts** :
  - `DA_TempleReliquaryRules.ArtifactPool` contient les 6 artefacts.
  - Chacun garde son socle de la maquette ; un socle dont l'artefact n'a pas été distribué reste vide.
  - Les deux perles (2,3 cm) sont affichées trois fois plus grosses, pour qu'on les voie et qu'on puisse les saisir.
- **Test** : `monke.Run.Day N` place la partie au jour N (contrats précédents réussis). Le contenu de ce jour apparaît sur l'île suivante.
- **Sanctuaires des villages** : ils gardent leur artefact légendaire dès le jour 1. À trancher : faut-il le retirer avant le jour 7 ?
- **Demandes suivantes du propriétaire** (2026-10-07), regroupées dans T-214 :
  - un exécutable stable avec la partie sauvegardée ;
  - un compteur de jours jusqu'au 10e ;
  - en fin de jour : décompte final et départ du bateau au loin, puis animation d'arrivée sur une nouvelle zone ;
  - le crabe sort du sable à partir de 5:30 de la journée, à 50 m du joueur, uniquement sur le sable ;
  - un renfort de mobs dès le jour 2 ;
  - des fusils pour les mobs des villages à partir de la 6e vague ;
  - le fort tiré au hasard dès le jour 6.
- **Bean français et garnison** : maquette du 2e PC importée. 7 soldats dans le fort de Test Flat. Voir `garnison-fort-francais.md` (T-213).

## État de T-214 (lead, 2026-10-07)
Compilé, 526 tests verts, vérifié en PIE sur `L_Frag_Raraka_01` (captures en rafale, journaux).

| Demande | État | Où |
|---|---|---|
| Compteur de jours jusqu'au 10e | fait : « Jour N / 10 » et le quota en haut à gauche ; l'écran de chargement dit la même chose (« Jour 2/10 — Villages et marae — Quota 0 / 1800 $ ») | `STapaoHUD::GetDayText`, `MakeDayLine` |
| Fin de jour : décompte final et départ du bateau au loin | fait. Au déluge fini à bord, la vue quitte le joueur pour une caméra qui regarde le galion partir. Le décompte « Départ dans N » s'affiche en haut au centre et le bilan du jour à gauche. Le jeu passe au noir puis lance le voyage. En campagne, il n'y a plus de panneau « Île conquise ! » : l'île est comptée dans la sauvegarde. | `UDayTransitionSubsystem`, `DA_DayTransitionRules` |
| Arrivée sur une nouvelle zone, début du jour | fait. L'île suivante est tirée au hasard. Après l'écran de chargement, la caméra arrive du large vers le galion au mouillage, le titre « Jour N » s'affiche, puis un fondu bref passe aux yeux du joueur sur le pont. La caméra ne traverse jamais le gréement. | idem |
| Renfort de mobs dès le jour 2 | fait : danger ×1,5 à partir du jour 2, inscrit au journal (« Run: day begins (campaign day 2, danger x1.50) ») | `RunCampaign::MobScale`, `URunSubsystem::GetDangerScale` |
| Crabe géant à 5:30 dès le jour 4, à 50 m, sur le sable | fait : vérifié en PIE (crabe sorti du sable à 42 m du joueur, puis chasse) | `UGiantCrabEmergenceSubsystem`, `DA_GiantCrabEmergenceRules` |
| Exécutable stable avec la partie sauvegardée | build Development empaqueté (voir le journal du chantier) ; la partie et le profil sont dans `monke_island/Saved/SaveGames` à côté de l'exécutable | `scripts/ue.ps1 package` |
| Fort tiré au hasard dès le jour 6 | remplacé le 2026-10-08 : l'île du fort `FRG_Raraka_21` est retirée du tirage. Le fort est généré sur la terre de l'île tirée, dès le jour 1. Voir `forts-atoll.md` | `UFortSiteSubsystem`, `AFortSite`, `DA_FortSiteRules` |
| Fusils pour les mobs des villages « au bout de la 6e vague » | **abandonné par le propriétaire** (2026-10-07 : « enlève l'idée des mobs avec fusil, laisse-les avec leurs armes ») : `DA_TikiRules.Musket.ArmedShare` = 0, code conservé mais inactif | `ATikiWarrior::SetArmed` |

Outils de test : `monke.EndDeluge` (hors Shipping) termine le déluge tout de suite ; `monke.Run.Day N` ; `monke.CrabEmerge`.

Défaut trouvé au test du build empaqueté et corrigé le 2026-10-07 : la première place de l'équipage sur le pont tombait dans l'étal du marchand. La vue devenait brune environ 1 s toutes les 7 s, au rythme du tangage du galion. Désormais, `AGalleonActor` glisse le marchand le long du pont jusqu'à laisser une demi-place de marge autour des 4 places (journal : « slid 90 cm clear of the crew's places »). Le correctif est dans l'exécutable livré ; l'éditeur doit être recompilé, éditeur fermé, pour l'avoir aussi.

### Suite du 2026-10-07 (soir)
- **Quota manqué au jour jalon** : exécution de l'équipage sur une planche par un peloton français, puis la campagne est perdue. Voir `docs/features/execution-equipage.md`.
- **Cinématiques qui ralentissent** (« le jeu lag lors des cinématiques »). Mesuré au CSV profiler en PIE : le GPU reste vers 11 ms. Le premier ralentissement vient du démarrage de l'île (villages, villageois, navigation, physique) : jusqu'à 90 ms par image la première seconde, encore 34 ms quand l'arrivée commençait.
  - L'arrivée attend désormais au noir que les images descendent sous `ArrivalSettleFrameMs` (25 ms), entre `ArrivalSettleMinSeconds` (1,5 s) et `ArrivalSettleMaxSeconds` (6 s). Journal : « DayTransition: the island settled after … ».
  - Les 5 meshes non-Nanite du galion (voiles, gréement, drapeau, barre, gouvernail) passent en Nanite. Ils causaient le débordement « [VSM] Non-Nanite Marking Job Queue overflow » pendant les plans du galion.
  - La coque reste non-Nanite : son verre de cabine est translucide.
  - Les captures en rafale des tests ajoutent elles-mêmes des saccades de 170 ms ; elles ne sont pas dans le jeu.

### Nuit et buissons (demande du 2026-10-07 au soir)
> « à partir du 3e jour la nuit tombe plus tôt, plus de mobs sortent des bush le jour, le 2e jour »
- **Nuit plus tôt dès le jour 3** (`DA_RunRules.Campaign.EarlyNightFromDay` = 3). L'expédition démarre entre 13 h et 15 h 30 (`EarlyNightStartHours`), donc la nuit (18 h 30) tombe 2,5 à 5 min après l'arrivée. Les jours 1 et 2 gardent le tirage de `DA_DayNightRules` (6 h 30 → 18 h), qui fait tomber la nuit entre 0,5 et 12 min après l'arrivée. Journal : « the game starts at … h …, the night falls early (campaign) ».
- **Plus de tikis sortent des buissons le jour, dès le jour 2** (`DayBushMobsFromDay` = 2, `DayBushMobScale` = 2,5). Le facteur de jour de `DA_TikiRules` (`DaySpawnScale`, quatre fois moins que la nuit) est multiplié par 2,5. Cela vaut pour les embuscades et les défenseurs des villages, et s'ajoute au renfort de 1,5 du jour 2.
- Rien ne change sur les cartes de test sans campagne (jour 0).
- Tests : `monke_island.Run.Campaign` (« Night and day »).

## Tâches
Voir le chantier (`docs/production/board.md`), tâches ajoutées le 2026-10-07 avec le préfixe « Campagne 10 jours ».

### Forts sur l'atoll, temple et crabes dès le jour 1 (2026-10-08)
> « fais en sorte que les forts français spawn sur la map de Raraka Tuamotu et pas sur une île personnalisée, fais apparaître le temple artefact, les crabes, les forts dès le jour 1 » — « crée une map et génère procéduralement »
- **Forts** : générés à chaque expédition sur un emplacement du fragment tiré. Voir `forts-atoll.md` : carte des emplacements, fort terrestre, générateur.
- **Jours** : temple, crabes et forts au jour 1 (`DA_RunRules.Campaign`). Les forts repassent au jour 3 le 2026-10-09 (section suivante).
- **Temple** : il manquait sur les fragments 02 à 20, joués sur `Lvl_Raraka`, car leur camp spawner n'avait pas de règles de temple. C'est corrigé.

### Forts et temples à artefacts au jour 3, difficulté par jour (2026-10-09, T-252)
> « ajoute au 3e jour les forts français sur Raraka et les temples artefacts ; mets "tout devient plus dur" ; ajoute tout »

**Statut : code écrit, non compilé** (lot L5 ; le lead compile et intègre). Preuves à apporter : compilation, `monke_island.Run.Campaign` (« Difficulty by day »), `monke_island.Run.Contract`, `monke_island.Fort.Garrison`, `monke_island.Creatures.CrabEmergence`, `monke_island.Player.Health`, puis PIE (voir « À vérifier »).

**Jours d'ouverture** (`DA_RunRules.Campaign`, `RunCampaign.h`) : `FortsFromDay` = 3 et `AllArtifactsFromDay` = 3 ; le premier temple reste au jour 1 (`TemplesFromDay`), les crabes aussi (`GiantCrabsFromDay`), tous les bâtiments au jour 7. Jours 1-2 : un temple par fragment ; dès le jour 3 : `TemplesPerFragmentAll` (3) temples, un artefact chacun, et les forts.

**Courbe de difficulté** (`DA_RunRules.DifficultyByDay`, une ligne `FRunDayDifficulty` par jour, `Run/RunDifficulty.h`) : la dernière ligne vaut pour les jours suivants ; le jour 0 (carte de test, PIE direct sans partie) joue les règles telles quelles ; une table vide aussi. Lecture : `RunDifficulty::Of(Table, Jour)` (pure), `URunRules::GetDayDifficulty(Jour)`, `URunSubsystem::GetDayDifficulty()` et `URunSubsystem::DifficultyOf(WorldContext)` (neutre sans partie). Journal au début du jour : « Run: day begins (campaign day N, danger xK; difficulty of the day: mobs x…, health x…, … ) ».

| Jour | Mobs (`MobCountScale`) | Santé mobs/soldats (`MobHealthScale`) | Dégâts sur les joueurs (`MobDamageScale`) | Soldats / fort (`SoldiersPerFort`) | Précision soldats (`SoldierHitChanceScale`) | Forts (`FortsPerExpedition`) | Santé crabe (`CrabHealthScale`) | Sortie du crabe (`CrabEmergenceMinute`) | Déluge (`FloodDelayScale`, sur 540 s) | Nuit (`NightShare`) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | ×0,8 | ×0,9 | ×0,9 | — | ×1 | — (pas de fort) | ×0,9 | règles (5:30) | ×1 → 9:00 | règle de la campagne (tirage 6 h 30-18 h) |
| 2 | ×1 | ×1 | ×1 | — | ×1 | — | ×1 | règles (5:30) | ×1 → 9:00 | règle de la campagne |
| 3 | ×1,1 | ×1,1 | ×1,1 | 7 (la garnison du niveau) | ×1 | 1 | ×1,1 | 5:30 | ×0,95 → 8:33 | règle de la campagne (nuit tôt : départ 13 h-15 h 30) |
| 4 | ×1,25 | ×1,2 | ×1,2 | 8 | ×1,1 | 1 | ×1,2 | 5:30 | ×0,9 → 8:06 | règle de la campagne |
| 5 | ×1,25 | ×1,2 | ×1,2 | 8 | ×1,1 | 1 | ×1,2 | 5:15 | ×0,9 → 8:06 | règle de la campagne |
| 6 | ×1,4 | ×1,35 | ×1,3 | 9 | ×1,2 | 2 | ×1,35 | 5:00 | ×0,85 → 7:39 | 60 % de l'expédition de nuit |
| 7 | ×1,6 | ×1,5 | ×1,4 | 10 | ×1,25 | 2 | ×1,5 | 5:00 | ×0,85 → 7:39 | 65 % |
| 8 | ×1,8 | ×1,7 | ×1,5 | 11 | ×1,3 | 2 | ×1,7 | 4:30 | ×0,8 → 7:12 | 70 % |
| 9 | ×1,8 | ×1,7 | ×1,5 | 11 | ×1,3 | 3 | ×1,7 | 4:30 | ×0,8 → 7:12 | 70 % |
| 10 | ×2 | ×2 | ×1,6 | 12 | ×1,4 | 3 | ×2 | 4:00 | ×0,78 → 7:01 | 80 % |

- **Ce qui se cumule avec `MobCountScale`** (toujours là, dans le DA) : le renfort de la campagne `MobSurplusScale` ×1,5 dès le jour 2 (demande du 2026-10-07) et les buissons de jour `DayBushMobScale` ×2,5 dès le jour 2 ; le jour de gourmandise ×1,4 (ADR-007). Total mobs de la campagne (hors buissons de jour et gourmandise) : jour 1 ×0,8 ; jour 2 ×1,5 ; jour 3 ×1,65 ; jours 4-5 ×1,9 ; jour 6 ×2,1 ; jour 7 ×2,4 ; jours 8-9 ×2,7 ; jour 10 ×3. Si c'est trop, baisser `MobSurplusScale` à 1 dans `DA_RunRules` : la courbe porte alors seule la progression.
- **Les paliers de zone sont neutres** : `Zones[].SpawnScale` (1 / 1,3 / 1,7) et `FloodDelayScale` (1 / 1 / 1,5) passent à 1 dans les valeurs par défaut (`URunRules()`) et dans le DA (script ci-dessous) ; la courbe par jour les remplace, plus finement. Les quotas (5 500 / 10 500 / 20 500), les durées (3 / 3 / 4 jours) et `LootValueScale` (1 / 1,5 / 2,2) ne changent pas. Le « temps élargi » des jours 7-10 (déluge ×1,5, décision provisoire du 2026-10-07) est donc abandonné : « tout devient plus dur » prime, le déluge vient plus tôt (7 min le jour 10). À confirmer avec le propriétaire.
- **Où chaque valeur s'applique** (serveur) :
  - `MobCountScale` : `URunSubsystem::GetDangerScale()` (zone × gourmandise × renfort × jour), lu par `ATikiDirector` (`SpawnScaleOf` : embuscades des buissons, nuit, défenseurs et gardiens des villages, plafonds par joueur et par zone de buissons).
  - `MobHealthScale` : `ATikiDirector::SpawnWarriorFromBush` (guerriers seulement, pas les villageois) et `AFortGarrisonZone` (chaque soldat pris dans la garnison), via `UIslanderHealthComponent::ScaleMaxHealth` (santé max × facteur, puis `ResetHealth`), après le `BeginPlay` de la créature qui pose sa santé de base.
  - `MobDamageScale` : `UIslanderHealthComponent::HandleTakeAnyDamage` — seulement quand le propriétaire est le pion d'un joueur et que le coup vient d'un pion que personne ne contrôle (mob, soldat, crabe, ou leur arme / projectile) et pas d'un PlayerController ; les chutes, les douves, les coups entre joueurs et entre créatures restent tels quels (`ScaleMobDamage`, `IsMobCauser`, pures).
  - `SoldiersPerFort`, `SoldierHitChanceScale` : `AFortGarrisonZone` — `GarrisonSettleSeconds` (1 s) après son début, la garnison est comptée et complétée aux postes de patrouille (clones des soldats placés, uniformes à tour de rôle : classe, `Rules`, `Kind`, `Skin`, `HatMesh`, `HeadMesh`) ou réduite (les derniers vus partent). La précision est exposée par `GetHitChanceScale()` ; **le branchement dans `AFortSoldier::Shoot` reste au lead** (`FortSoldier.cpp` est réservé) : voir `forts-atoll.md`.
  - `FortsPerExpedition` : `UFortSiteSubsystem::BuildForts` (`RunDifficulty::CountOr(jour, DA_FortSiteRules.FortsPerExpedition)`), limité aux emplacements du fragment (certains n'en ont qu'un ou deux : journal « day N asks K forts, FRG has M site(s) »).
  - `CrabHealthScale`, `CrabEmergenceMinute` : `UGiantCrabEmergenceSubsystem` (`GiantCrabEmergenceMath::DayTimes` décale toutes les sorties de `DA_GiantCrabEmergenceRules.EmergeSeconds` pour que la première tombe à la minute du jour ; santé × facteur après l'apparition, aussi pour `monke.CrabEmerge`).
  - `FloodDelayScale` : `URunSubsystem::GetFloodDelayScale()` (zone × jour), déjà lu par `AExpeditionGameState::StartExpedition` (`InitialFloodDelay` × facteur).
  - `NightShare` : `URunSubsystem::GetEarlyNightStartHours` — quand la part est > 0, l'heure de départ est `NightFallHour` (18 h 30, `DA_RunRules`) − (1 − part) × `ExpeditionSolarHours` (8,5 h solaires pour 540 s) × facteur du déluge ; sinon la règle de la nuit tôt (`EarlyNightStartHours`) comme avant. `ADayNightCycleActor` ne change pas.
- **Script éditeur** (à lancer par le lead, verrou de l'éditeur, pas de PIE) : `scripts/editor/owner_batch_1009_ue.py`, partie `runrules` : écrit dans `DA_RunRules` les jours de déblocage, les zones neutres et les dix lignes de la courbe (le DA peut garder des valeurs enregistrées avant le changement des défauts C++ : tout est posé explicitement, avant / après dans `Saved/T186/ownerbatch1009.json`).
- **Tests écrits (non exécutés)** : `Run/Tests/RunCampaignSpec.cpp` (jours de déblocage, « Difficulty by day » : neutre au jour 0 et sans table, ligne du jour et dernière ligne, planchers, courbe par défaut monotone et valeurs nommées, zones neutres, heure de départ pour la part de nuit), `Run/Tests/RunContractSpec.cpp` (paliers neutres), `Fort/Tests/FortGarrisonSpec.cpp` (effectif à ajouter / retirer, poste d'un soldat ajouté), `Creatures/Tests/GiantCrabEmergenceSpec.cpp` (sorties décalées), `Player/Tests/IslanderHealthSpec.cpp` (dégâts des mobs sur les joueurs).
- **À vérifier en PIE** (`monke.Run.Day N` puis l'île suivante) : jour 2 → pas de fort, un temple ; jour 3 → fort, 3 temples, garnison de 7 (« garrison of 7, the day wants 7 »), journal « difficulty of the day » ; jour 6 → 2 forts si le fragment a 2 emplacements, 9 soldats (« 2 soldier(s) added at the posts »), crabe à 5:00, départ vers 15 h 36 ; jour 10 → 12 soldats, crabe à 4:00, déluge à ~7:01, un coup de crabe (25) fait 40 ; carte de test (`L_Test_Flat`) inchangée (jour 0).
