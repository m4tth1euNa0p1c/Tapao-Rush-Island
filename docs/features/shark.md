# Requin du lagon (cahier §8, lot L5)

| Champ | Valeur |
|---|---|
| Lot / recette | L5 (« un requin », n°10) ; cahier §8 |
| Code | `Source/monke_island/Creatures/SharkActor.*`, `SharkMath.h`, `SharkRules.h` ; tests `monke_island.Shark.*` |
| Assets | `/Game/MonkeIsland/Characters/Shark/` : `BP_Shark`, `DA_SharkRules`, `SM_Shark_Body`, `SM_Shark_Tail`, `MI_Shark_*` |
| Modèle | `scripts/props/shark.py` (procédural, requin à pointes noires, 2,0 m) → `art/blender/props/shark.blend`, `art/export/Creatures/` |
| Niveau | `Shark_Lagoon` dans `Lvl_Raraka` (outliner `Raraka/Gameplay`), environ 50 m au nord du PlayerStart, dans le lagon |
| Débogage | `monke.SpawnShark` : un requin dans l'eau profonde près du joueur local |

## Comportement
États : patrouille → détection → poursuite ⇄ attaque → retour. Mort : sur le dos, il coule, puis disparaît.
- **Cible** : uniquement un joueur vivant **qui nage** (`IsSwimming`). Un joueur assis dans la barque ou debout dans l'eau peu profonde n'est pas une cible.
- **Détection** (12 m) : le requin ralentit et se tourne vers le nageur pendant `DetectionDelay`, ce qui rend le danger lisible ; puis la poursuite commence.
- **Attaque** : 25 dégâts, une morsure toutes les 2 s au plus, recul du nageur ; le corps se détend en avant à la morsure.
- **Fin de poursuite** propre (retour) quand le nageur sort de l'eau, meurt, s'éloigne au-delà de `LoseRadius`, quand la poursuite dure plus de `ChaseGiveUpTime` sans morsure, ou quand il se réfugie en eau trop peu profonde.
- **Navigation** sans navmesh : chaque pas est accepté seulement si la colonne d'eau (surface du `UIslandWaterSubsystem`, fond par trace) dépasse `MinWaterDepth`. L'inondation rend donc navigables les terres submergées.
- **Réseau** : serveur autoritaire, déplacement cinématique répliqué, état répliqué ; les clients animent seulement la queue (battement selon la vitesse) et la morsure (multicast).

Toutes les valeurs sont dans `DA_SharkRules` (cahier : détection 12 m, 25 dégâts, délai 2 s).

## Preuves (2026-10-03)
- Compilé : cible jeu dans le worktree `dev/T-058`, puis build éditeur de `main` à 0 erreur.
- Testé : `monke_island.Shark.*` 6/6 et `monke_island.Tiki.*` 5/5, en headless.
- Pas encore testé en PIE : poursuite d'un nageur et morsure. Le MCP ne simule pas les entrées du joueur : test à la main ou `monke.SpawnShark` en nageant.
