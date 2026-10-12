# Feature : barque physique, rames, surcharge et naufrage

- **Lot** : L2 (barque) · **Cellule** : Objets & barque · **Tâches** : T-037 à T-042
- **Cahier** : §4 (endurance, nage), §5 (capacité, sièges séparés), §6.4 (barque ; la navigation libre à la rame y est une extension), §6.5 (embarquer assis), recette n°5, n°6 et n°19.
- **Écarts voulus par le propriétaire** : `docs/adr/ADR-003-barque-physique.md`.
- **Asset** : `SM_Barque` (`docs/art/briefs/barque.md`).

## Intention
Une barque qu'on vit physiquement, de façon drôle et lisible :
- elle flotte et tangue, et s'enfonce quand on la charge ;
- on y grimpe en sautant depuis l'eau, puis on s'assoit et on prend les rames ;
- elle avance par coups de rame, lentement ;
- on peut l'échouer à moitié sur le sable : elle laisse une trace quand on la pose et quand on la repousse vers l'eau ;
- trop chargée, elle prend l'eau et coule, et on perd tout.

## Règles
1. **Flottaison.** La poussée vient de 10 pontons sous la coque. Elle est proportionnelle à l'immersion entre la quille et le plat-bord, avec un amortissement vertical et angulaire. La traînée est faible vers l'avant, forte sur le côté (effet de quille). Une houle procédurale légère fait tanguer la barque, même à l'arrêt.
2. **Charge.**
   - Chaque joueur dans la barque pèse `PersonMassKg` (60 kg), qu'il soit debout sur le plancher ou assis.
   - Un objet physique compte pour sa masse, qu'il soit posé ou arrimé.
   - Capacité : `MaxLoadKg` (500 kg).
   - Le poids des joueurs est appliqué comme une force à leur position : la barque gîte du côté où ils se trouvent.
3. **Surcharge.**
   - Au-delà de `MaxLoadKg`, l'eau entre dans la barque à `FloodRateKgPerS` + `FloodRatePerKgOver` × excès.
   - Sous la limite, l'eau ressort lentement (`DrainRateKgPerS`, on écope).
   - L'eau embarquée pèse et se voit : un plan d'eau monte dans la coque.
   - À `FloodCapacityKg`, la barque **coule** : flottaison coupée, les passagers sont éjectés, la cargaison part au fond. Elle est **perdue** et détruite après `SunkLifeSpan`.
4. **Monter à bord.** (F = touche d'interaction commune, T-033 : `ABoatActor` implémente `IIslanderInteractable`.)
   - Dans l'eau, Espace fait sauter hors de l'eau si l'endurance le permet (`JumpStaminaCost`), assez haut pour passer le plat-bord.
   - On peut marcher sur le plancher : la barque est une base mobile.
   - **F** près de la barque quand personne ne rame : on prend les rames. Le rameur s'assoit seul, au centre, sur le poste de nage (0,35 m à l'avant des dames de nage, face à la poupe, comme l'animation `A_Bean_Boat_Row`). Les autres joueurs ne s'assoient pas : ils restent debout sur la barque, physiquement (demande du propriétaire du 2026-10-03, `DA_BoatRules.bOnlyRowerSits`). F encore : on se lève, `StandUpLiftCm` au-dessus du poste, libre de marcher ou de sauter dehors.
   - Assommé sur le poste (culbute X, coup, pince du crabe) : le rameur en tombe. Le siège et les rames sont libérés, et il retrouve son mouvement en se relevant (T-182).
   - Un joueur assis nulle part n'est jamais accroché à une barque, et aucun siège ne lui laisse le mouvement coupé (garde du siège, T-182).
5. **Ramer.**
   - Seul le rameur commande, avec Z/W (avant), S (arrière), Q/A (bâbord) et D (droite).
   - Chaque coup de rame dure `StrokePeriod` (1,2 s) avec une poussée en cloche : la barque fait des à-coups puis glisse. Les rames s'animent en phase.
   - Vitesse plafonnée à `MaxRowSpeed` (1,6 m/s) : la barque est lente.
   - **Ramer ne coûte pas d'endurance** : assis, on récupère comme à l'arrêt.
6. **Nage et noyade.**
   - Nager coûte de l'endurance (`SwimStaminaCost` en mouvement, `SwimIdleStaminaCost` sur place).
   - À 0, le nageur s'enfonce (`DrowningBuoyancy`). Après `DrownGraceTime`, il perd `DrownDamagePerSecond` points de vie par seconde jusqu'à la mort.
   - Il est sauvé s'il regagne pied, la barque ou de l'endurance.
7. **Sable et échouage.**
   - Une quille qui touche le fond reçoit un frottement proportionnel à la part de la coque posée (`BeachedDragPerPoint`) : la barque s'arrête, mais les joueurs peuvent la pousser.
   - Tant qu'elle glisse sur le sol (> `TrailMinSpeed`), elle laisse une trace : un décal de sillon tous les `TrailSpacing`, qui s'efface après `TrailLifeSpan`. C'est vrai à l'échouage comme à la remise à l'eau.
   - La trace est cosmétique, calculée sur chaque machine.
8. **Réseau.**
   - Le serveur fait autorité sur la physique de la barque, la charge, l'eau, le naufrage, les sièges et les entrées de rame (RPC via le pawn possédé, `UBoatRiderComponent`).
   - Le mouvement de la barque est répliqué. L'eau embarquée et l'état coulé sont répliqués.
   - Chaque machine aligne le personnage sur le siège répliqué (`BoatSeat::Resolve`), quel que soit l'ordre d'arrivée du siège, de l'attache et des corrections de mouvement. Un client ne défait que ce qu'il a appliqué lui-même ; le serveur défait aussi ce qu'il trouve sans l'avoir appliqué.
   - **Mode de réplication physique** (T-295, ADR-003 B-07, revue R-05) : `PredictiveInterpolation`. La coque d'un client est menée par vitesse vers l'état du serveur, prolongé de la latence. Entre deux envois, chaque machine applique les mêmes forces : flottaison, rames, poids, et houle. La houle suit l'horloge du serveur (`GetServerWorldTimeSeconds`) et non plus le temps écoulé depuis le `BeginPlay` de chaque machine. La barque est envoyée `NetUpdateFrequency` fois par seconde (30, au lieu de 100 par défaut). `PhysicsReplicationMode` peut repasser à `Default` dans `DA_BoatRules` si l'essai à 150 ms le demande.
   - **Cargaison arrimée** (T-295) : un objet arrimé qui porte un `UNetRestComponent` (props, cadavres) est **collé à la coque sur chaque machine**. Son mouvement n'est plus envoyé ; l'objet reste éveillé, regardé 2 fois par seconde, sans octet tant que rien ne change. Chaque client le porte avec sa propre coque. Il continue de simuler : la saisie, le compte de la cargaison et le poids le trouvent comme avant. Dès qu'un joueur le prend, il est libéré partout. Au naufrage, la cargaison collée est libérée. Les autres objets (pickups d'armes, caisse) restent portés par le serveur seul, comme avant.
   - **Rames** (revue R-09) : la commande part en non fiable. Une commande inchangée repart tous les `RowInputResendInterval` (0,25 s) tant que les touches sont tenues : un paquet perdu ne laisse plus la barque immobile.
   - **Naufrage** (revue R-12) : le serveur éjecte les joueurs assis. Le client de chaque joueur distant lance aussi son propre pion (`ClientLaunch`), une fois sorti de son siège : le saut est prédit, pas corrigé. La vitesse vient de `SinkEjectUpSpeed`.
   - **Arrivée en cours** (revue R-19) : un client qui reçoit une barque déjà coulée n'affiche pas « La barque a coulé ».
9. **Apparition.**
   - Une barque est posée sur l'eau près du PlayerStart.
   - La commande console `monke.SpawnBoat` en fait apparaître une devant le joueur, sur l'eau (serveur).

## Valeurs (DataAsset `DA_BoatRules`, classe `UBoatRules`)
| Valeur | Défaut | Note |
|---|---|---|
| HullMassKg | 220 | barque en bois de 5,6 m |
| PersonMassKg | 60 | demandé par le propriétaire |
| MaxLoadKg | 500 | demandé par le propriétaire (le cahier dit 150 kg : ADR-003) |
| FloatCapacityKg | 1100 | poussée à l'immersion totale (quille → plat-bord) |
| HullDepthCm | 80 | quille → plat-bord |
| FloodRateKgPerS / FloodRatePerKgOver / DrainRateKgPerS | 15 / 0,6 / 6 | |
| FloodCapacityKg | 450 | eau embarquée qui fait couler |
| RowForceN / StrokePeriod / MaxRowSpeed | 1800 / 1,2 s / 160 cm/s | |
| TurnTorque | 2600 N·m | |
| BeachedDragPerPoint | 0,35 | fraction de vitesse retirée par seconde et par point posé |
| TrailSpacing / TrailLifeSpan / TrailMinSpeed | 25 cm / 90 s / 8 cm/s | |
| SunkLifeSpan | 20 s | |
| SinkEjectUpSpeed | 300 cm/s | éjection des passagers assis au naufrage ; était en dur jusqu'à T-295 |
| StandUpLiftCm | 35 cm | hauteur gagnée en se levant du poste (F, pont du galion, naufrage) ; était en dur jusqu'à T-182 |
| PhysicsReplicationMode | PredictiveInterpolation | T-295, ADR-003 B-07 ; `Default` en repli |
| NetUpdateFrequency | 30 | envois par seconde de la barque (100 par défaut dans le moteur) |
| RowInputResendInterval (`UBoatRiderComponent`) | 0,25 s | commande de rame tenue renvoyée (R-09) |

Noyade (composant de mouvement du joueur) : `SwimIdleStaminaCost` 2/s, `DrowningBuoyancy` 0,85, `DrownGraceTime` 3 s, `DrownDamagePerSecond` 12. Saut hors de l'eau : `WaterJumpZVelocity` 520 cm/s.

## Départ en barque (T-093, lot L3, ADR-004)
Au début d'une expédition, les joueurs commencent assis dans la barque, sur l'eau, la proue vers l'île.

**Règles (serveur uniquement, `UBoatStartSubsystem`)**
1. Une frame après le `BeginPlay` du monde (le niveau d'eau et les acteurs sont prêts), le serveur choisit l'arrivée dans cet ordre :
   1. le **transform injecté** par `SetArrivalOverride(Transform, FragmentId)` : mouillage `Start` de la définition d'île (T-087/T-094) ; Boats/ ne dépend pas d'Expedition/ ;
   2. un acteur **`AExpeditionArrivalPoint`** dont le `FragmentId` vaut l'option d'URL `?Island=<FragmentId>` (contrat avec T-094) ; tirage seedé s'il y en a plusieurs ;
   3. sinon, un `AExpeditionArrivalPoint` au hasard (`bRandomArrivalWhenIslandUnknown`) ; graine : `?Seed=N`, puis `ArrivalSeed`, puis l'horloge (elle est écrite dans le log) ;
   4. sinon, rien. Le départ se fait sur un PlayerStart, comme avant, avec un log clair et aucun crash.
2. Sécurité : s'il y a moins de `MinWaterDepthCm` d'eau sous le point, on recule le point vers le large, par pas de `SafetySearchStepCm`, jusqu'à `SafetySearchMaxCm`. Au-delà, pas de barque.
3. La barque (`BoatClass`, référence souple, sinon `BP_Barque`) apparaît au repos. Son pivot, à la flottaison, est posé `SpawnHeightAboveWaterCm` au-dessus de la surface donnée par `UIslandWaterSubsystem`. Elle n'a que le lacet du point : la proue suit la flèche (+X).
4. Chaque contrôleur de joueur embarque **une seule fois**, dès que son pawn existe (solo, hôte, clients, et ceux qui rejoignent en cours si `bBoardLateJoiners`) :
   - il prend le premier siège libre de `SeatOrder` ; le premier assis prend les rames, et ZQSD fonctionne tout de suite (`ABoatActor::SitCharacter`, même chemin que F) ;
   - quand tous les sièges sont pris, il est posé debout sur un `StandingSpots`, et la barque porte son poids (`PersonMassKg`) ;
   - sa vue est tournée vers la proue (`bFaceBow`).
5. Le respawn après la mort n'est **pas** modifié : un contrôleur déjà embarqué repasse par le GameMode et un PlayerStart. Le GameMode Blueprint reste tel quel.
6. Réplication : la place assise passe par `UBoatRiderComponent` (`SeatedBoat` / `SeatIndex` répliqués, `OnRep_Seat` attache le pawn chez le client). La barque se réplique comme toute barque.

**Valeurs (`DA_BoatStartRules`, classe `UBoatStartRules`)** : bEnabled (vrai), BoatClass (vide = BP_Barque), bRandomArrivalWhenIslandUnknown (vrai), ArrivalSeed (0 = nouvelle graine), MaxFragmentIdLength (64), SpawnHeightAboveWaterCm (5), MinWaterDepthCm (120 : tirant d'eau 30 cm chargée, plus une marge), SafetySearchStepCm / SafetySearchMaxCm (100 / 5000), SeatOrder (0, 1, 2, 3), StandingSpots (repère de la coque, donc ×0,75 : (85, ±45, 100) à côté du rameur côté proue, (172, ±25, 100) entre le banc avant et le pont avant ; aucune entre le rameur et sa caméra, T-146), bBoardLateJoiners (vrai), bFaceBow (vrai).
Tant que l'asset n'existe pas, ce sont les valeurs par défaut de la classe qui s'appliquent. Pour brancher l'asset, renseigner `StartRulesAsset` dans `Config/DefaultGame.ini`, section `[/Script/monke_island.BoatStartSubsystem]` (la ligne y est en commentaire).

**Critères**
- [ ] Map avec un point `FRG_X`, lancée avec `?Island=FRG_X` : barque au point, joueur 1 aux rames, ZQSD actif.
- [ ] Sans option : un point au hasard, le même pour la même `?Seed=`.
- [ ] Map sans point ni override : départ sur un PlayerStart, aucune barque de départ supplémentaire en dehors de la barque d'essai existante.
- [ ] 5 joueurs pour 4 sièges : 4 assis et 1 debout dans la barque. Elle flotte.
- [ ] Mort puis respawn : sur un PlayerStart, pas dans la barque.
- [ ] Hôte + client : le client se voit assis au bon siège (PIE à 2, T-095).
- [ ] Specs `monke_island.Boats.Start.*` vertes.

## Critères d'acceptation
- [ ] Barque vide : elle flotte avec environ 16 cm d'immersion, de façon stable, avec un léger tangage.
- [ ] 1 joueur debout sur un bord : la barque gîte de ce côté.
- [ ] Depuis l'eau : Espace sort du nageur de l'eau et il retombe dans la barque, plancher compris.
- [ ] F près d'un siège : le joueur est assis. Le premier assis rame ; ZQSD fait avancer et tourner, par à-coups, à 1,6 m/s au maximum. L'endurance remonte pendant ce temps.
- [ ] Charge 8 × 60 kg = 480 kg : elle flotte. Avec un objet de 40 kg de plus : l'eau monte, la barque coule, les passagers sont éjectés, elle est détruite après 20 s.
- [ ] Sous la limite avant le naufrage : l'eau ressort.
- [ ] Ramer jusqu'à la plage : la barque s'échoue et laisse un sillon. La repousser à l'eau laisse aussi un sillon.
- [ ] Nageur à 0 d'endurance : il s'enfonce, perd de la vie après 3 s et meurt s'il ne regagne pas un appui.
- [ ] Hôte + client : sièges, rames, eau embarquée et naufrage sont cohérents (preuve `testé éditeur` en PIE 2 joueurs).
- [ ] Hôte + client à 100 puis 200 ms et 3 % de perte (T-295) : la barque du client tangue en phase avec celle de l'hôte, sans à-coups de correction visibles (`p.NetShowCorrections 1`). Les 12 objets arrimés restent en place chez le client. Moins de 5 Ko/s pour la barque chargée (`stat net`). Z tenu fait toujours avancer.
- [ ] Specs `monke_island.Boats.*` et `monke_island.Player.Movement` vertes.

## Bug T-182 : joueur entraîné par la barque, qui ne peut plus bouger
**Signalement.** Propriétaire, 2026-10-06 21:20 : « quand le bateau se déplace, le joueur est entraîné et ne peut plus bouger ». Indice relevé à 21:40 en PIE `L_Frag_Raraka_01` : `CharacterMovement` en `MOVE_None`, capsule attachée à la coque en (41,7 ; 0 ; 127,2).

**Lecture de l'indice.**
- Cette position est exactement celle du poste du rameur :
  - `RowerSeatLocation` vaut (41,7 ; 0 ; 7,2) ;
  - on y ajoute la demi-hauteur de capsule de l'époque, 90 cm, divisée par l'échelle 0,75 de la coque : 7,2 + 120 = 127,2.
- C'est donc l'état d'un rameur assis par `UBoatRiderComponent`, pas un reste du galion. En solo, `PlaceOnDeck` (`TeleportCharacter`) libère bien le siège.
- Le joueur peut être assis aux rames à deux moments :
  - au départ, pendant les 2 à 6 s où `BoatStart` le met aux rames, avant que le galion n'ancre (`SpawnDelay` 2 s, plus l'attente du catalogue des îles) ;
  - plus tard, s'il reprend les rames avec F.
- La capture de 21:42 (`Saved/T149/captures/ingame_editor.png`) le montre en vue à la première personne, au-dessus d'un village de l'île. Il n'était donc plus assis, puisque la vue passe à la troisième personne sur le siège.

**Cause trouvée dans le code.**
1. **Assommé sur son siège, le rameur restait assis et accroché à la barque.** C'est le cas de la culbute X, d'un coup de mob ou du lancer du crabe.
   - Pendant le ragdoll, la capsule restait attachée au poste du rameur en `MOVE_None` : c'est l'état de l'indice.
   - Au relevé, `TryRecoverFromRagdoll` posait la capsule à l'endroit du corps, toujours attachée à la coque (même dans l'eau), et la passait en `MOVE_Falling`.
   - Le siège était toujours tenu : le contexte de rame restait actif et ZQSD ramait au lieu de marcher. Le joueur était emporté par la barque à chacun de ses mouvements. Seul F (« Se lever ») le libérait.
2. **La libération dépendait de ce que chaque machine avait appliqué, et rien ne la revérifiait.**
   - Un client qui recevait le siège avant que sa barque ne connaisse ses sièges sortait sans rien faire et ne réessayait jamais.
   - `TeleportCharacter` ne libérait qu'un joueur que son composant disait assis. Un joueur attaché sans siège serait resté collé à la barque, et `MoorBoat` l'aurait emporté.

**Correction (2026-10-10).**
- **Règle pure** `BoatSeat::Resolve` (`Boats/BoatSeat.h`, catégorie de log `LogMonkeBoat`). Assis : attaché à la coque de sa barque, mouvement coupé. Assis nulle part : jamais attaché à une barque, et aucun siège ne coupe le mouvement.
- **Application de la règle.** `UBoatRiderComponent::ReconcileSeat` l'applique à chaque changement de siège (`SetSeat`, `OnRep_Seat`). Le tick la revérifie sur chaque machine : c'est la garde du siège.
- **Rameur assommé.** Le serveur libère le siège (`StandCharacter`, les rames passent au suivant). La capsule quitte aussitôt la coque. Le mouvement et la collision `QueryAndPhysics` reviennent quand le ragdoll se relève.
- **Joueur assis nulle part, encore tenu.** La garde le libère : détaché, collision rendue, `MOVE_Falling` s'il était en `MOVE_None`. Elle écrit un avertissement `LogMonkeBoat` (« Seat guard: … freed »).
  - Le `MOVE_None` posé par un autre système (pince du crabe, crash en vol) n'est pas touché.
  - Un client n'annule jamais une attache répliquée par le serveur : c'est la garde du serveur qui la traite.
- **Client en retard.** Il applique le siège dès que sa barque connaît ses sièges.
- **Galion.** `AGalleonActor::TeleportCharacter` (pont au départ, échelle) appelle `UBoatRiderComponent::LeaveBoat` : le siège est libéré et la capsule quitte toute barque.
- **Barque.** `ABoatActor::SitCharacter` refuse un corps en ragdoll ou mort. `StandCharacter` décroche aussi un joueur pendu à sa coque sans y être assis.
- **Valeur.** La hauteur gagnée en se levant passe dans `DA_BoatRules.StandUpLiftCm` (35 cm, avant en dur).
- **Commande de contrôle (hors Shipping).** `monke.BoatSeatGuardCheck` met le joueur local dans l'état du bug : accroché à la barque la plus proche, `MOVE_None`, sans siège. La garde doit le libérer à l'image suivante.
- **Spec.** `monke_island.Boats.Seat` (`Boats/Tests/BoatSeatSpec.cpp`) : table de décision de la règle et séquence BoatStart → `PlaceOnDeck` → `MoorBoat` → F deux fois.

**Statut honnête.**
- `compilé` : UBT, fichier par fichier, le 2026-10-10. Fichiers : `BoatRiderComponent.cpp`, `BoatActor.cpp`, `BoatSubsystem.cpp`, `GalleonActor.cpp` et `BoatSeatSpec.cpp`.
- Non fait : spec non exécutée, PIE non fait, multijoueur non testé.
- En-têtes réfléchis modifiés (`UBoatRules`, `UBoatRiderComponent`) : il faut un build complet, éditeur fermé. Live Coding ne suffit pas.
- Revue réseau `ue-network-engineer` obligatoire en REC.
- **Écarts restants.**
  - Au départ, le joueur reste 2 à 6 s aux rames avant le galion : ZQSD rame, même sous l'écran noir d'arrivée. C'est voulu par `BoatStart`, mais l'ADR-005 met l'équipage sur le pont. La décision revient au propriétaire.
  - Un client assis peut trembler : les corrections de position du serveur visent une capsule attachée. Non traité.

**Statut honnête T-295 (2026-10-11, `ue-network-engineer`).**
- Réalisé : mode de réplication physique, houle sur l'horloge du serveur, cargaison collée à la coque sur chaque machine (`UNetRestComponent`), fréquence, rames renvoyées (R-09), éjection prédite (R-12), message d'arrivée en cours (R-19).
- Niveau de preuve : `compilé` (UBT, fichier par fichier, éditeur ouvert, 2026-10-11). En-têtes réfléchis modifiés (`UBoatRules`, `UBoatRiderComponent`) : il faut un build complet, éditeur fermé.
- Non fait : PIE, essai n°4 de `docs/audit/revue-reseau-T268.md`. Protocole : client rameur, hôte debout, puis l'inverse ; PktLag 100 puis 200, PktLoss 3 ; 12 objets arrimés ; `stat net` ; `p.NetShowCorrections 1` ; `LogMonkeNetRest Verbose`.

## Hors périmètre
- Animation du personnage assis ou ramant : le personnage n'a pas encore de squelette dédié (T-025, T-026). Seules les rames et l'eau sont animées.
- Vagues réelles de l'océan.
- Dégâts de coque.
- Arrimage par emplacements (§5 et `Slot_*`) : tâche L2 séparée.
