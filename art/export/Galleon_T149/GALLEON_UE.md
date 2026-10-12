# Galion anglais : intégration Unreal (flottaison)

Le galion est à l'échelle de Bean (1,05 m) : coque de 22 m, largeur 6 m, tirant d'eau 1,7 m, mâture jusqu'à 17,6 m. Le pont principal est à 1,6 m au-dessus de l'eau, avec un pavois de 0,7 m. Le gaillard d'arrière est à 2,9 m ; dessous, la cabine du capitaine a 1,3 m sous plafond. La dunette est à 4,0 m.

**L'origine du modèle est au centre de la ligne de flottaison** (z = 0 = surface de l'eau), la proue vers l'avant.

## Fichiers (`exports/`)
| Asset | Contenu | Pivot |
|---|---|---|
| `SM_Galleon` | Coque, ponts, châteaux, cabine meublée, mâts, vergues, proue, ancres, échelle, bossoirs, canons, cloche, lanternes, décor de pont. ~173 collisions UCX explicites (coque en 7 tranches, dalles des 4 ponts + entrepont avant + cabine, pavois, flancs des châteaux, tableau, cloisons, balustrades des rebords, garde-corps des hunes, escaliers en rampes, mâts, cabestans, hiloires, mobilier de cabine, sacres de pont, beaupré, tonneaux, caisses, pompes, poteaux de lanternes) et ~100 sockets. | ligne d'eau |
| `SM_Galleon_Sails` | Voiles carrées + voile latine (matériau toile, double face) | ligne d'eau |
| `SM_Galleon_Rigging` | Haubans, enfléchures, étais, bras. Sans collision. | ligne d'eau |
| `SM_Galleon_Flag` | Pavillon de Saint-Georges, à animer avec un WPO de vent | pied de hampe (socket `flag`) |
| `SM_Galleon_Wheel` | Barre à roue, qui tourne autour de l'axe avant du navire (Roll = Y local) | axe de la roue (socket `helm`) |
| `SM_Galleon_Rudder` | Gouvernail (Yaw ±35° autour de son Z local = étambot, suit la barre) | étambot (socket `rudder_hinge`) |

Les positions des pivots dans SM_Galleon (en m), les sockets et les pontons sont dans `../galleon-report.json`.
Les trois pièces mobiles sont exportées dans leur repère local et les sockets `flag`, `helm`, `rudder_hinge` portent la
rotation correspondante (`socket_rots_deg` : identité pour le pavillon et la roue, X −10,49° pour le gouvernail =
inclinaison de l'étambot) : **attacher `SM_Galleon_X` au `SOCKET_X` sans offset** reproduit la pose du navire.

**Matériaux** : `SM_Galleon` a ~31 slots `M_Galleon_<Nom>` (liste dans `galleon-report.json > exports.SM_Galleon.material_names`).
Les variantes de ton par planche sont fusionnées à l'export : le ton (0,8 … 1,2) est dans la vertex color `Tone`
(linéaire, RGB = ton / 1,2). Dans le matériau UE : `BaseColor × VertexColor × 1,2` (ou ignorer la vertex color pour un bois uniforme).

## Sockets
| Socket | Usage |
|---|---|
| `helm` | Barre : E pour prendre la barre (caméra et contrôles du navire) |
| `boarding_ladder` | Pied de l'échelle d'abordage (tribord) : monter à bord depuis l'eau ou la barque |
| `barque_dock` | Amarrage de la barque le long du bord (position de la barque quand on accoste) |
| `barque_davit` / `barque_davit_fore` | Têtes des deux bossoirs bâbord (crochets des palans 0,55 m dessous) |
| `barque_hang` | Barque hissée : attacher `SM_Barque` (origine = flottaison, proue vers l'avant du navire, bâbord vers l'extérieur) sans offset : centrée entre les bossoirs, 0,9 m sous les crochets, quille au-dessus de la lisse de dunette |
| `cargo_hatch` | Cale : déposer les objets ramenés de la barque (inventaire du navire) |
| `captain_cabin` | Cabine : coffre de stockage, carte du monde, point de sauvegarde |
| `bean_spawn` | Apparition de Bean à bord |
| `anchor_port` / `anchor_starboard` | Ancres : jeter l'ancre (le navire s'arrête) |
| `flag`, `bell`, `rudder_hinge` | Pavillon, cloche (son), gouvernail |

## Flottaison (Water plugin)
1. Active les plugins **Water** et **Buoyancy**. Le niveau a un `WaterBodyOcean`, ou un `WaterBodyLake` pour le lagon.
2. **BP_Galleon** (Actor ou Pawn) :
   - Racine : `SM_Galleon`, avec *Simulate Physics* activé, *Mass (kg) Override* = **42 000**, *Center Of Mass Offset* = (0 ; 0,6 ; -0,4) m, soit (0 ; -60 ; -40) cm dans Unreal (l'axe Y est inversé). *Linear Damping* 0,4, *Angular Damping* 1,2.
   - Enfants : `SM_Galleon_Sails`, `SM_Galleon_Rigging` et `SM_Galleon_Flag` (*No Collision*), `SM_Galleon_Wheel` et `SM_Galleon_Rudder` à leur pivot.
   - Composant **Buoyancy** avec les 10 pontons de `galleon-report.json > buoyancy.pontoons`. Positions en m ×100, avec Y inversé ; rayon 80 à 100 cm.
   - Le pont doit rester à environ 1,6 m au-dessus de l'eau à l'équilibre. S'il s'enfonce, augmente le rayon des pontons ; s'il gîte, baisse le centre de masse.
3. **Bean à bord** : les ponts ont des collisions UCX marchables. Pour que Bean ne glisse pas quand le navire tangue, active *Character Movement > Impart Base Velocity* sur le personnage. Utilise un *Movement Base* : le pont est la base dès que Bean marche dessus.
4. **Navigation** (à la barre) : applique une force vers l'avant proportionnelle aux voiles déployées (0 / ½ / pleine), et un couple de lacet proportionnel à l'angle du gouvernail. Fais tourner `SM_Galleon_Wheel` (Roll = angle de barre × 4) et `SM_Galleon_Rudder` (Yaw = angle de barre).
5. **Barque** : quand la barque s'approche à moins de 3 m de `barque_dock`, propose « E : accoster ». Les objets de la barque passent dans l'inventaire du navire (`cargo_hatch`). La barque peut être hissée et attachée à `barque_davit`.

## Régénérer
```
uv run factory/tools/maquette.py build galion-anglais --headless --force
uv run factory/tools/make_sheet.py galleon
```
