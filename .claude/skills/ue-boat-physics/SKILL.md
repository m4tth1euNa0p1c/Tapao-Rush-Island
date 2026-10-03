---
name: ue-boat-physics
description: Barque physique de monke_island — flottaison, rames, charge et surcharge (eau, naufrage), échouage et traces dans le sable, sièges et rameur répliqués, saut hors de l'eau et noyade — implémentation, réglage en PIE et recette. À utiliser pour « la barque flotte mal », « règle la barque », « nouvelle embarcation », « teste la barque », « surcharge / naufrage ».
argument-hint: "<régler | tester | nouvelle-embarcation | bug> [détail]"
---

# Barque physique

Demande : $ARGUMENTS · Agent : `ue-boat-programmer` · Spec : `docs/features/boat-rowing.md` · ADR-003.

## 1. Où sont les choses
| Sujet | Où |
|---|---|
| Valeurs (masse, capacité 500 kg, 60 kg par joueur, rames, sable, traces) | `DA_BoatRules` (`UBoatRules`) : `/Game/MonkeIsland/Boat/Data/DA_BoatRules` |
| Formules pures (testées) | `Source/monke_island/Boats/BoatPhysics.h` · specs `monke_island.Boats.*` |
| Flottaison, rames, charge, eau, naufrage, sable, traces, animation | `ABoatActor` (`Boats/BoatActor.*`) · BP `BP_Barque` (`SM_Barque`, `SM_Barque_Oar`, `M_BilgeWater`, `M_SandTrail_Decal`) |
| F (interaction commune `IIslanderInteractable`, implémentée par `ABoatActor`) | `ABoatActor` |
| Sièges répliqués, ZQSD, HUD de la barque, alerte de noyade | `UBoatRiderComponent` (ajouté à chaque pawn par `UBoatSubsystem`) |
| Apparition | barque de départ près du premier joueur ; console `monke.SpawnBoat` (hôte) |
| Nage, saut hors de l'eau, noyade, endurance assis | `UIslanderMovementComponent` (catégories Swimming / Drowning / Stamina) |

## 2. Régler (un paramètre à la fois, en PIE)
1. **Immersion à vide** : environ 16 cm, soit `HullDepthCm × HullMassKg / FloatCapacityKg`.
   - Trop haute : baisser `FloatCapacityKg`.
   - Ça rebondit : monter `VerticalDampingPerPoint`.
2. **Stabilité.**
   - Elle chavire trop vite quand un joueur est sur le bord : `CenterOfMassOffset.Z` plus bas, ou `AngularDamping` plus haut.
   - Elle est trop raide : l'inverse.
3. **Vitesse.**
   - La vitesse de croisière vaut √(`RowForceN` × 100 × 0,5 × `StrokeDriveFraction` / `ForwardQuadDrag`) ; viser 1,2 à 1,6 m/s.
   - `MaxRowSpeed` coupe la poussée.
   - Virage : `TurnTorqueNm` ; si elle dérape, monter `LateralDrag`.
4. **Surcharge.** Le temps de naufrage vaut `FloodCapacityKg / (FloodRateKgPerS + FloodRatePerKgOver × excès)`. Viser 10 à 20 s pour 1 joueur de trop : le joueur doit avoir le temps de comprendre et d'alléger.
5. **Sable.**
   - `BeachedDragPerPoint` : elle doit rester échouée, mais deux joueurs doivent pouvoir la repousser.
   - Traces : `TrailSpacing`, `TrailDecalSize`, `TrailLifeSpan`.
6. **Nage et noyade.** `SwimStaminaCost` / `SwimIdleStaminaCost`, `DrownGraceTime`, `DrownDamagePerSecond`, `DrowningBuoyancy`, `WaterJumpZVelocity` (assez pour passer le plat-bord, à environ 1,8 m au-dessus des pieds du nageur).

Après chaque réglage, consigne la valeur et sa raison dans la spec (tableau des valeurs).

## 3. Recette (PIE, puis PIE à 2 joueurs, hôte et client)
1. **Barque vide** : elle flotte et tangue doucement. `monke.SpawnBoat` en fait apparaître une devant soi.
2. **Monter à bord** : nager jusqu'à la barque, Espace pour sauter dedans (coûte de l'endurance), marcher sur le plancher, F pour s'asseoir, F pour se lever.
3. **Ramer** : ZQSD par à-coups, rames animées, lent ; l'endurance remonte pendant ce temps.
4. **Charge** : ajouter des joueurs et des objets. Le HUD affiche `Barque : X / 500 kg`.
5. **Surcharge** : au-delà de 500 kg, l'alerte s'affiche, l'eau monte, puis le naufrage : passagers éjectés, barque détruite après `SunkLifeSpan`.
6. **Écoper** : repasser sous la limite avant le naufrage fait redescendre l'eau.
7. **Échouage** : ramer jusqu'à la plage ; la barque s'arrête à moitié dans le sable et laisse un sillon. La repousser à l'eau laisse un sillon aussi.
8. **Noyade** : nager jusqu'à 0 d'endurance ; le nageur s'enfonce, voit l'alerte, perd de la vie après 3 s, puis meurt ou s'en sort en regagnant un appui.
9. **Réseau** : à 2 joueurs, sièges, rameur, eau et naufrage sont identiques chez l'hôte et chez le client.

Preuves : captures (`CaptureViewport` puis `scripts/mcp_image.py`) et log (`Logmonke_island` : « sank »). Niveau de preuve écrit dans le chantier.

## 4. Pièges connus
- Le niveau de la mer vient de `UIslandWaterSubsystem`. On ne place jamais de volume d'eau pour la barque.
- **Pivot de `SM_Barque`** : à la flottaison (Z = 0), pas au sol.
- **Capsule d'un joueur assis** : sans `QueryOnly`, elle pousse la coque et la barque part en vrille.
- **Ancien plancher au mauvais endroit** : les meshes de la barque se réimportent en supprimant l'asset, puis en réimportant dans un autre appel (voir la mémoire du projet sur l'import par MCP).
- **PIE en cours** : il bloque les sauvegardes. Coordonner avec les autres sessions avant de fermer l'éditeur pour un build.
- **Nouvelles classes ou changement de header** : éditeur fermé, puis build UBT. Live Coding ne suffit pas.

## 5. Nouvelle embarcation
Créer un enfant de `ABoatActor` (BP) avec son mesh :
- sockets `Seat_*`, `Oarlock_L/R` et `UCX_` ;
- pivot à la flottaison.

Lui donner un `DA_BoatRules_<Nom>` avec `HullSize`, `KeelLocalZ`, `GunwaleLocalZ`, `HullDepthCm` et les capacités. Ajouter les cas dans les specs.
