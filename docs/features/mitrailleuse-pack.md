# Mitrailleuse Mle 1918 : pack d'arme (T-272, modèle, rig, animations)

**Lot :** L5 (armes à feu ; extension du cahier §5, voir `armes-nouvelles.md`). Demande du propriétaire du 2026-10-10 : « crée et modélise une mitrailleuse à manivelle type année 1918 mitrailleuse lourde, modélise le rechargement, courir avec, les balles s'éjectent, le canon chauffe, ajouter le recul, le son, la lumière des explosions, rends la mitrailleuse ultra réaliste type Call of Duty ». Gameplay, sons, données : `mitrailleuse.md` (T-273).

**Statut (2026-10-10) :** pack **exporté** (`build/packs/bean-machinegun`, `check_pack.py` : CHECK OK, 63 fichiers, FBX en mètres) et **importé** dans l'éditeur ouvert par l'action `mgimport` : 58 assets (gun, bras FP, 52 clips, 4 statiques), 0 échec, échelle racine du bean identique au projet (100), taille 117,4 × 28,3 × 30,7 cm conforme au manifeste ; instances PBR sur tous les emplacements, masque de chaleur posé (EmissiveStrength 0), LOD générés, bras FP sur les `MI_Bean_*`. Rapport : `monke_island/Saved/T272/mgimport.json`. **Pas testé en PIE** (aucune partie lancée par ce pack) ; données de jeu : `mgsetup` (T-273).

## Reconstruire
```
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/machinegun_pack/build.py -- --product --previews --save
python scripts/props/machinegun_pack/check_pack.py
```
Options : `--clips`, `--views`, `--no-bake` (aperçu rapide), `--no-export`, `--model-only`, `--render FP:FireLoop:1,...`. Environ 20 min (cuisson GPU OptiX ~4 min, 26 clips, 63 fichiers). Aperçu du modèle seul : `preview_model.py` (EEVEE, sans cuisson : ni usure des arêtes ni occlusion, la vérité est le rendu Cycles des textures cuites).

| Fichier | Rôle |
|---|---|
| `scripts/props/machinegun_pack/machinegun_model.py` | géométrie, os, sockets, trajet de la bande, statiques |
| `mg_materials.py` | matériaux PBR procéduraux à cuire (acier bleui, laiton bruni, acier noir à suie et couleurs de revenu, noyer huilé, toile, chêne, fer, cuir, caoutchouc ; masque de chaleur) |
| `animation.py` | les 13 gestes FP/TP (manivelle, recul, convoyeur de bande, rechargement, surchauffe…) |
| `build.py`, `export_pack.py` | cuisson, clips, FBX, manifeste, rendus |
| `check_pack.py` | contrôle hors Blender / Unreal (fichiers, unités, événements du contrat, os, textures, marges de la bande) |
| `import_machinegun_ue.py` | action `mgimport` : import par étapes, masque de chaleur, LOD |

## L'arme
Mitrailleuse fictive d'après les Vickers / Maxim de 1918, manivelle latérale de Gatling, **1,17 m** (bêches arrière → bouche du cache-flamme), 28 × 31 cm avec la caisse, ≈ 17 kg. Repère de l'arme : +X bouche, +Z haut, **-Y droite** (côté manivelle), axe du canon sur Z = 0, mètres (Unreal : un point (x, y, z) m → (x, -y, z) × 100 cm).
- Boîte de culasse en acier bleui, plaques latérales à cadres rivetés, fenêtres d'alimentation des deux côtés, fenêtre d'éjection dessous, plaque arrière boulonnée, deux **bêches de tir en noyer** et levier de détente, plaque signalétique en laiton, **hausse relevable** levée.
- **Manchon à eau cannelé en laiton bruni** (20 cannelures, trois bagues), bouchon de remplissage moleté et chaînette, **soupape de vapeur à clapet** (os `steam_cap`), bouchon de vidange, raccord et **tuyau de condensation** gainé de toile le long du flanc gauche, guidon à oreilles, **poignée de transport** en noyer, **poignée avant en noyer inclinée à 45°** vers la gauche (visible en vue subjective), écrou de presse-étoupe, canon à couleurs de revenu, **cache-flamme conique** creux à suie.
- **Manivelle** à droite : carter boulonné, moyeu à écrou carré, bras, poignée de noyer à embout d'acier (rayon 9 cm).
- **Capot d'alimentation** à charnière avant (os `feed_cover`), loquet ; **poignée d'armement** à gauche (os `cocking_handle`, course 6,6 cm).
- **Bande de toile** à barrettes de laiton (pas 17,5 mm), cartouches de 7,7 mm à bourrelet (étui laiton 56 mm, balle chemisée cuivre, amorce), entrant par la gauche depuis une **caisse en chêne cerclée de fer** (coins, clous, anse, moraillon, glissière d'accroche) ; la **bande vide** ressort à droite dans une **poche de toile**.
- **Bretelle** en cuir tendue le long du flanc droit du manchon (deux anneaux, boucle) ; **trépied replié** sous l'avant du manchon (os `tripod` : `HideBoneByName("tripod")` pour le retirer).
- **60 600 triangles**, 4 matériaux cuits : `M_MG_Body` (2048), `M_MG_Jacket` (2048, + `T_MG_Jacket_Heat` 1024 : masque de rougeoiement du canon et du cache-flamme, noir ailleurs), `M_MG_Belt` (1024), `M_MG_Box` (1024) ; BaseColor sRGB, Normal DirectX, ORM linéaire. LOD : générés dans Unreal par `mgimport` (squelettique : 3 niveaux ; ramassable : 100 / 50 / 22 %).

## Squelette (`SK_Bean_MachineGun_Skeleton`, 35 os + 16 sockets)
| Os | Rôle |
|---|---|
| `gun_root` | racine (boîte de culasse, crosse, bretelle, poche) ; `FirstPersonAlignBone` par défaut |
| `jacket` | manchon, canon, cache-flamme, poignées avant et de transport |
| `crank` | manivelle (rotation autour de Y de l'arme passant par l'axe) |
| `feed_cover`, `cocking_handle`, `steam_cap` | capot, poignée d'armement, clapet de vapeur |
| `ammo_box` | caisse (réduite à zéro entre `BoxOut` et la nouvelle caisse) |
| `tripod` | trépied replié (facultatif) |
| `belt_01..17` | poches garnies, de l'extraction (01) jusque dans la caisse (17) |
| `belt_out_01..10` | poches vides (01-04 : réserve du convoyeur, cachées hors tir) |

**Sockets** : `socket_grip_r` (poignée de manivelle au repos : `GripSocket`), `socket_grip_l` (poignée avant, main gauche FP), `socket_grip_l_tp` (poignée de transport, main gauche TP), `socket_muzzle` (bouche, axe +X), `socket_eject` + `socket_eject_dir` (fenêtre d'éjection et un point 5 cm sur l'axe de sortie : direction = `eject_dir - eject`, ≈ (0,10 ; 0,20 ; -0,97) en repère Unreal de l'arme), `socket_steam` (sortie du clapet), `socket_jacket` (milieu du manchon), `socket_hose` (bout du tuyau), `socket_feed`, `socket_belt_out`, `socket_box`, `socket_box_slot`, `socket_cocking`, `socket_cover_latch`, `socket_crank_handle`.

## Gestes (30 i/s, contrat avec T-273)
| Clip | Images | Événements |
|---|---|---|
| `Idle` / `Walk` / `Sprint` | 90 / 120 / 90, boucles | — (Sprint : arme portée en travers, lourde) |
| `Equip` / `Holster` | 54 / 48 | `DrawWeapon` 6, `Ready` 46 / `StowWeapon` 42 |
| `FireStart` | 17 | `CrankTick` 0, `Fire` 1, 9, 13 (la manivelle accélère : ¾ de tour) |
| `FireLoop` | 16, boucle | `CrankTick` 0, `Fire` 0, 4, 8, 12 (un tour = 4 coups = 450 coups/min à vitesse 1) |
| `FireEnd` | 10 | `CrankTick` 8 (arrêt sur le cran, sans coup) |
| `Reload` | 150 (5 s) | `CoverOpen` 16, `BeltOut` 34, `BoxOut` 64, `BoxIn` 92, `BeltIn` 116, `CoverClose` 126, `Charge` 136 |
| `Overheat` | 60 | `SteamVent` 6 |
| `Inspect` / `DryFire` / `Discard` | 180 / 14 / 40 | — / `DryClick` 4 / `Empty` 4, `Release` 24 |

- **Manivelle** : angle φ depuis le repos (poignée en haut à l'avant : la main droite passe au-dessus de la boîte, visible en vue subjective) ; un coup par quart de tour. `FireStart` 0 → 270°, `FireLoop` 270 → 630° (image 16 = image 0), `FireEnd` 270 → 360° = repos. `FireEnd` part donc de la fin d'un tour.
- **Recul** à chaque coup : 1,4 cm en arrière et 1,8° vers le haut en une image, retour en trois, dispersion de lacet et de roulis par coup, frémissement continu (périodique sur la boucle) ; les mains sont sur l'arme, les bras suivent ; en TP les épaules (`spine_02`) encaissent.
- **Bande** (chaîne d'os, pas de défilement d'UV : les cartouches sont de la géométrie) : chaque poche est posée chaque image sur le trajet de la bande ; un pas par coup, en deux images après chaque `Fire` ; sur un tour de `FireLoop` la bande avance de quatre pas et revient au bouclage du clip, où le moteur n'interpole pas (un retour en cours de clip ferait glisser toute la bande à l'envers entre deux images). Les poches garnies passées au-delà du point d'extraction (dans la boîte, sous le capot) et les poches vides de réserve avant lui sont réduites à zéro. Marges vérifiées : 2,3 cm sous la fente de la caisse, 3,3 cm dans la poche de toile.
- **Rechargement** (deux mains) : main droite → loquet, capot ouvert ; bande soulevée hors du couloir et lâchée (elle pend de la caisse) ; main gauche → caisse décrochée de la glissière et lâchée (`BoxOut` : l'os est réduit à zéro, le jeu fait tomber `SM_Ammo_MGBox` depuis `socket_box`) ; nouvelle caisse remontée et accrochée (`BoxIn`) ; amorce tirée de la fente jusqu'à la fenêtre, enfilée en trois poussées (`BeltIn`) ; main droite rabat le capot (`CoverClose`) ; main gauche tire la poignée d'armement (`Charge` : la première cartouche avance d'un pas).
- **Surchauffe** : clapet qui saute et claque (`SteamVent`), main gauche retirée brûlée, secouée, puis remise ; l'avant du canon s'affaisse pendant ce temps.
- **Vue subjective** : bras FP placés par la caméra du jeu (90°), sans étirement ; cadrage `fp-framing.json` (`HipGripLocation` (40, 28, -19) cm, `HipRotation` (6,9 ; -5,8 ; 17,9)°), pas de pose de visée. **Troisième personne** : arme en travers du ventre, main droite à la manivelle, **main gauche sur la poignée de transport** (la poignée avant est hors de portée des bras courts du bean), roulée pour que la caisse dégage le ventre ; rechargement arme redressée contre la poitrine. Étirement des bras TP jusqu'à ×1,40 ; au rechargement la main gauche reste à 3,7 cm de la caisse au pire.

## Statiques (`Static/`)
| Maillage | Rôle |
|---|---|
| `SM_Bean_MachineGun` | l'arme au sol (pivot en bas au centre, 5 coques UCX), 54 000 triangles |
| `SM_Ammo_MGBox` | caisse pleine ramassable (UCX), 21,5 × 10,4 × 14 cm |
| `SM_MG_Casing` | douille tirée de 7,7 mm (5,8 cm, couchée selon +X, bouche noircie, amorce percutée), 600 triangles, `M_MG_Casing` (512) |
| `SM_MG_BeltLink` | une poche vide de la bande avec sa barrette (4,5 cm), pour les maillons éjectés |

## Image d'inventaire
`scripts/ui/render_weapon_icons.py -- --only MachineGun,SM_Ammo_MGBox` (pose « long », d'après `SM_Bean_MachineGun.glb` du pack), puis `python scripts/ui/finish_weapon_icons.py` → `art/ui/icons/weapons/T_Icon_MachineGun.png`, `T_Icon_SM_Ammo_MGBox.png` ; posées dans Unreal par `mgsetup` (T-273) ou `weaponicons`.
