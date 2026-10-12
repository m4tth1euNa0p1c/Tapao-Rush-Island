# Feature : personnage jouable (Islander)

- **Statut** : done (v1)
- **Owner** : ue-gameplay-programmer
- **Date** : 2026-10-02

## 1. Intention de gameplay
Un personnage à la troisième personne, aux proportions et à la physique d'un adulte réel, pour parcourir l'atoll de Raraka à pied et à la nage. Le joueur doit sentir l'échelle du lieu : la plage, le platier où l'on patauge, le lagon où l'on perd pied.

## 2. Règles et valeurs tunables
| Paramètre | Valeur | Justification réelle | Où |
|-----------|--------|----------------------|----|
| Taille / largeur | 1,80 m / capsule r = 34 cm | adulte moyen | `AIslanderCharacter` (capsule 34 × 90) |
| Masse | 75 kg | adulte moyen | `UIslanderMovementComponent::Mass` |
| Gravité | 9,8 m/s² (GravityScale 1) | Terre | idem |
| Jogging | 3,5 m/s | footing | `MaxWalkSpeed` 350 |
| Sprint (Shift / LB / L3) | 6 m/s | sprint d'un adulte en forme | `MaxSprintSpeed` 600 |
| Accélération / freinage | 12 / 16 m/s² | démarrages vifs, sans glisse | `MaxAcceleration`, `BrakingDecelerationWalking` |
| Saut | ≈ 0,55 m | détente debout (v²/2g, v = 3,3 m/s) | `JumpZVelocity` 330 |
| Contrôle en l'air | 20 % | peu d'inertie modifiable | `AirControl` 0,2 |
| Marche maxi / pente | 40 cm / 45° | marche de rocher, pente raide | `MaxStepHeight`, `WalkableFloorAngle` |
| Pataugeage | vitesse × 1 → × 0,4 entre 0 et 1,2 m d'eau | traînée de l'eau sur les jambes | `WadingMinSpeedFactor` |
| Début de nage | eau ≥ 1,20 m aux pieds (sortie à 1,05 m) | eau à la poitrine | `SwimStartDepth`, `SwimExitHysteresis` |
| Nage | 1,2 m/s (×1,5 en sprint) | crawl de loisir | `MaxSwimSpeed`, `SprintSwimSpeedMultiplier` |
| Flottabilité | 1,2 → tête à ≈ 30 cm hors de l'eau | flottabilité humaine légèrement positive | `Buoyancy` |
| Plongée | regarder vers le bas en nageant | — | `AIslanderCharacter::MoveInput` |
| Poussée d'objets physiques | proportionnelle à leur masse | — | `bPushForceScaledToMass` |
| Caméra | bras 3,5 m, épaule droite, FOV 80° | lisibilité troisième personne | `CameraBoom`, `FollowCamera` |

## 3. Critères d'acceptation
- [x] Given le joueur apparaît sur la plage de Motutapu, then il est au sol, debout, à l'échelle (centre de capsule à +0,90 m du sol).
- [x] Given le joueur apparaît dans le lagon profond, then il flotte à l'équilibre (centre à −0,60 m, tête hors de l'eau) sans couler ni rebondir.
- [x] Given le joueur nage, then le mode de mouvement reste `Swimming` (validé en PIE après le correctif `OnMovementModeChanged`).
- [ ] Given le joueur entre dans l'eau depuis la plage, then il ralentit progressivement puis nage au-delà de 1,2 m.
- [ ] Given le joueur sprinte, then il atteint 6 m/s sur le sable sec.

## 4. Design technique
- `UIslandWaterSubsystem` (WorldSubsystem, `Water/`) : niveau de la mer = Z de l'acteur tagué `IslandWaterSurface` (le plan d'eau). `SetSeaLevel()` déplace le plan d'eau et diffuse `OnSeaLevelChanged`. C'est la base de la future **montée des eaux** et de la **surcote cyclonique**.
- `UIslanderMovementComponent` (`Player/`) : sprint, pataugeage, nage pilotée par le niveau de la mer (`IsInWater`, `ImmersionDepth`, transitions dans `UpdateCharacterStateBeforeMovement`). `PhysSwimming` du moteur repasse en chute quand le volume physique n'est pas un volume d'eau ; `OnMovementModeChanged` corrige ce cas.
- `AIslanderCharacter` (`Player/`, abstract) : capsule réelle, caméra troisième personne, Enhanced Input (move, look, mouse look, jump, sprint), mapping contexts propres au personnage.
- Blueprints : `/Game/MonkeIsland/Player/Blueprints/BP_Islander` (SKM_Manny_Simple + ABP_Unarmed, IA du template + IA_Sprint / IMC_Horror) et `BP_IslandGameMode` (pawn BP_Islander, contrôleur BP_FirstPersonPlayerController). GameMode override de `Lvl_Raraka`.

## 5. Tâches
| # | Tâche | Agent | Statut |
|---|-------|-------|--------|
| T-016 | Personnage, mouvement, niveau de la mer, BP, GameMode | ue-gameplay-programmer | fait |
| T-017 | Animation de nage du bean : `A_Bean_Swim` / `A_Bean_TreadWater`, flottaison à la surface (fiche `character-bean-soldier.md` §7) | ue-character-rigger | review (compilé, import et PIE à faire par le lead) |
| — | Navigation : désactiver `bCanEverAffectNavigation` sur les tuiles de terrain (export de 280 k triangles par tuile) | ue-performance-engineer | à faire |

## 6. Tests
- Claviers : ZQSD (IMC_Islander_AZERTY) et WASD (IMC_Default) actifs ensemble, flèches, manette.
- PIE scripté via MCP (positions de départ forcées : plage, lagon profond) : lecture du mode de mouvement et de la position.
- Limite : le MCP ne simule pas d'entrées joueur ; sprint et pataugeage sont à valider manette ou clavier en main.

## 7. Journal
- 2026-10-02 : classes C++, build UBT OK (toolchain MSVC remise à jour), Blueprints, GameMode ; flottabilité validée en PIE (−60 cm) ; correctif du mode de nage compilé en Live Coding, test PIE en attente (éditeur partagé avec la session de végétation).
- 2026-10-03 : correctif validé en PIE (lagon : Swimming stable à −60 cm ; plage : Walking) ; IMC AZERTY ajouté.

## Mouvement plus fluide et inclinaison du corps (2026-10-03, demande de l'utilisateur)
- **Mouvement :**
  - `MaxAcceleration` 900 : un trot atteint en environ 0,4 s ;
  - freinage 1300, frottement au sol 6, frottement de freinage séparé 4 : petite glissade à l'arrêt ;
  - `RotationRate` 380°/s : un demi-tour en environ 0,5 s.
- **Inclinaison du pantin (`UBeanPuppetComponent::UpdateLean`) :**
  - le corps penche dans l'accélération mesurée (démarrage, freinage, virage) ;
  - angle `LeanFactor` × atan(a / g), au plus `MaxLeanDegrees` (14°) ;
  - ressort amorti (`LeanStiffness` 90, `LeanDamping` 13), légère oscillation à l'arrêt ;
  - au sol seulement ; jamais en 1re personne pour son propriétaire, car la caméra suit le pantin.
- **Preuve :** compilé (build éditeur), valeurs vérifiées sur `BP_IslanderBean` par MCP. Non testé en PIE.
