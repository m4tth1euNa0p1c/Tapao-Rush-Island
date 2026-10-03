# Cycle jour / nuit

Lot L5 (nuit), tâche T-043. Cahier §5 : « Le jour/nuit dépend du temps réellement écoulé, pas du timer restant ; la pacification ne rembobine pas la météo. Crépuscule à 120 s, nuit à 240 s par défaut. » §8 : la nuit doit être inquiétante, mais les itinéraires doivent rester perceptibles.

## Règles
- L'horloge avance avec le temps de jeu réel. Le serveur la possède et la réplique (`FDayNightClock`) ; chaque machine en calcule l'éclairage.
- Elle suit une courbe de clés (temps écoulé → heure solaire) définie dans `DA_DayNightRules`. Le 2026-10-03, l'utilisateur a demandé un temps plus rapide (« le temps passe pas assez vite ») : le DataAsset en jeu boucle en 3 min. Les valeurs du cahier restent les valeurs par défaut en C++ ; écart à arbitrer dans ADR-001.

  | Moment | Cahier (défaut C++) | `DA_DayNightRules` (en jeu) | Heure solaire |
  |---|---|---|---|
  | plein jour | 0 s | 0 s | 9 h |
  | crépuscule, soleil à environ −2° | 120 s | 50 s | 18 h 15 |
  | nuit | 240 s | 75 s | 19 h 30 |
  | aube | 480 s | 150 s | 5 h 30 |
  | boucle | 600 s | 180 s | 9 h |

- Le soleil suit la vraie course solaire de Raraka : latitude −16,18°, jour de l'année 276. Repère de la carte : X = est, Y = sud.
- La nuit est éclairée par une lune pleine, froide (9 500 K), toujours au moins à 30° au-dessus de l'horizon. On assombrit l'exposition (−1,5 EV) et le brouillard.
- Les phases Jour, Crépuscule, Nuit et Aube se déduisent de l'élévation du soleil (±6°). Le délégué `OnPhaseChanged` servira à la lanterne, à l'audio et au requin.

## Code
- `Source/monke_island/DayNight/` : `ADayNightCycleActor` (un seul par niveau), `UDayNightRules`, `DayNightMath` (fonctions pures), tests `monke_island.DayNight.Clock`.
- Si l'acteur ne reçoit aucune référence, il retrouve seul le `DirectionalLight` soleil (atmosphère, index 0), le `SkyLight` et l'`ExponentialHeightFog`.
- Commandes de débogage (hors Shipping), côté serveur : `monke.SetHour 18.5` et `monke.TimeSpeed 10`.

## Critères d'acceptation
- [ ] À 120 s le soleil est à l'horizon ; à 240 s il fait nuit (tests de l'horloge).
- [ ] La nuit reste lisible : silhouettes et rivage visibles (PIE, capture).
- [ ] L'hôte et le client voient la même heure, à ±1 s près (PIE hôte + client).
