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
   - **F** près de la barque quand personne ne rame : on prend les rames. Le rameur s'assoit seul, au centre, sur le poste de nage (0,35 m à l'avant des dames de nage, face à la poupe, comme l'animation `A_Bean_Boat_Row`). Les autres joueurs ne s'assoient pas : ils restent debout sur la barque, physiquement (demande du propriétaire du 2026-10-03, `DA_BoatRules.bOnlyRowerSits`). F encore : on se lève et on descend sur le côté.
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
- [ ] Specs `monke_island.Boats.*` et `monke_island.Player.Movement` vertes.

## Hors périmètre
- Animation du personnage assis ou ramant : le personnage n'a pas encore de squelette dédié (T-025, T-026). Seules les rames et l'eau sont animées.
- Vagues réelles de l'océan.
- Dégâts de coque.
- Arrimage par emplacements (§5 et `Slot_*`) : tâche L2 séparée.
