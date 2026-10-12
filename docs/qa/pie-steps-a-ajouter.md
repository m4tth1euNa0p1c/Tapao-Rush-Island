# Étapes de pilotage PIE à ajouter (`fort_land_pie_check.py`)

- **Pour** : le lead, qui tient `scripts/editor/fort_land_pie_check.py` (action PIE `fortcheck` de `scripts/editor/pie_session.py`, étapes lues dans `monke_island/Saved/T186/fortland-step.txt`, résultats ajoutés à `fortland-check.json`).
- **Pourquoi** : les recettes T-163, T-265, T-266, T-267 et la recette de partie T-116 (`docs/qa/`) ont besoin d'étapes que le script n'a pas au 2026-10-10. Chaque fichier `steps-T-xxx.txt` marque `[à ajouter : …]` la séquence qui en dépend.
- **Rédigé par** `ue-qa-tester` le 2026-10-10. Aucune de ces étapes n'est écrite ni essayée : ce document est une spécification.

## 0. Ce que le script fait aujourd'hui, et ses limites qui comptent pour les recettes

| Limite | Effet | Réponse proposée |
|---|---|---|
| Toutes les étapes d'une requête tournent dans le même rappel `register_slate_post_tick_callback` | Aucune attente possible entre deux étapes : « tirer puis capturer 2 s après » demande deux requêtes | étape `wait` (§1.1) |
| `shot` lance `HighResShot`, qui ne prend pas le HUD Slate | Le HUD (jour, quota, munitions, compteur) n'est pas sur l'image | utiliser `cmd shot showui` (fenêtre entière, HUD compris), déjà employé par le lead ; le dire dans l'aide du script |
| Une étape au nom inconnu est **ignorée sans trace** (pas de `else` dans la chaîne de `elif`) | Une séquence qui contient une étape pas encore écrite « passe » sans rien faire | §1.2 : `entry["unknown"]` |
| Le pion et le contrôleur sont ceux du **joueur 0 du premier monde PIE** (l'hôte en PIE à 2) | Rien ne peut être lu ni capturé chez le client | étape `as` (§1.3) |
| Aucune lecture du journal | La preuve « ligne de journal » doit être cherchée à la main après coup | étapes `logmark`, `logtail` (§1.4) |
| `god 1` envoie la commande console `god`, qui **bascule** | Deux `god 1` de suite rendent le joueur mortel à nouveau ; `god 0` ne fait rien | §1.5 |

## 1. Étapes transverses (toutes les recettes)

### 1.1 `wait <secondes>`
- **Effet** : arrête la séquence et reprend les étapes suivantes `secondes` plus tard, sans nouvelle requête.
- **Mise en œuvre proposée** :
  - `fort_land_pie_check.py` : à la rencontre de `wait`, écrit `monke_island/Saved/T186/fortland-pending.json` = `{"steps": [étapes restantes], "due": time.time() + s, "chain": <id de la requête>}`, ajoute l'entrée courante au journal, puis s'arrête.
  - `pie_session.py`, `poll` : si `fortland-pending.json` existe et que `due` est passé (et que le PIE tourne toujours), relance `fortcheck` en lui passant ces étapes au lieu de `fortland-step.txt`, puis efface le fichier. Une nouvelle requête efface un `pending` en attente.
  - Le journal porte `"chain"` et `"part"` (0, 1, 2…) pour relier les morceaux d'une même séquence.
- **Lit / écrit** : `fortland-pending.json` ; ajoute une entrée par morceau à `fortland-check.json`.
- **Garde-fou** : `wait` au plus 120 s ; au-delà, refus (`entry["error"]`).

### 1.2 Étape inconnue
- **Effet** : `entry.setdefault("unknown", []).append(step)` au lieu de l'ignorer, et `pie-result.json` garde `passed: true`, mais le rapport la montre.

### 1.3 `as <n>`
- **Effet** : les étapes suivantes de la séquence agissent dans le monde PIE n° `n` : 0 = serveur d'écoute (l'hôte), 1 = premier client, 2 et 3 = clients suivants.
- **Mise en œuvre proposée** : `ue.EditorLevelLibrary.get_pie_worlds(False)` (à vérifier dans l'API Python 5.8 ; repli : parcourir `ue.EditorLevelLibrary.get_all_level_actors()` n'aide pas, car il lit le monde de l'éditeur), puis `world`, `pawn`, `controller` recalculés pour ce monde.
- **Règles** :
  - dans un monde client, seules les étapes de lecture et `cmd` locales ont un sens (`shot`, `cmd shot showui`, `find`, `near`, `health`, `fpstate`, `runstate`, `clock`, `hud`) ; une commande `monke.*` marquée « server » ne fait rien chez un client ;
  - `entry["world"]` = `n`, le `NetMode` du monde et le nom du pion.
- **Sert à** : prouver la réplication (même jour, même quota, mêmes morceaux de corps, même horloge chez le client) et capturer la vue du client.

### 1.4 `logmark <texte>` et `logtail <motif> [n]`
- `logmark` : `ue.log("QA-MARK <texte>")`, pour borner un cas dans `monke_island/Saved/Logs/monke_island.log`. `entry["logmark"]` = texte et heure.
- `logtail` : relit les 2 derniers Mo du journal de l'éditeur (lecture partagée, le fichier reste ouvert par l'éditeur), garde les lignes **écrites après le dernier `QA-MARK`** qui contiennent `motif` (sous-chaîne, ou expression régulière si elle commence par `re:`), les `n` dernières (20 par défaut) dans `entry["logtail"][motif]`.
- **Sert à** : joindre la preuve « ligne de journal » à l'entrée JSON du cas (ex. `logtail "Run: day"`, `logtail LogMonkeBoat`, `logtail "reacts:"`).

### 1.5 `god 0|1`
- **Effet** : rend l'état demandé, quel qu'il soit avant. Lire l'état réel (`pawn.can_be_damaged` et le drapeau du cheat `god`) ; n'envoyer `god` que s'il faut basculer. `entry["god"]` = état final.
- Rappel : sur `L_Test_Flat`, le mode de jeu de test fait apparaître le joueur invulnérable ; l'étape `mortal` existante le rend mortel.

### 1.6 `players`
- **Effet** : pour chaque `PlayerState` du monde serveur : nom, pion, position (m), santé, mort, à bord du galion ou non, assis dans une barque ou non. `entry["players"]`.
- **Sert à** : RUN05, RUN06, RUN07, T-265 (joueur resté à terre) sans dépendre de la vue.

### 1.7 `runstate` et `clock`
- `runstate` : l'état de la partie, lu côté serveur sur `URunSubsystem` (`ue.SubsystemBlueprintLibrary.get_game_instance_subsystem(pawn, ue.RunSubsystem)`) ou, à défaut, sur le `URunStateComponent` du GameState : jour de campagne, jour du contrat, palier, quota, valeur du contrat, trésorerie, cupidité en attente, jour de cupidité, issue. Si ces valeurs ne sont pas lisibles en Python (pas de `BlueprintPure`), repli : `cmd monke.Run.Status` puis `logtail "Run:" 3`.
- `clock` : `AExpeditionGameState` : temps serveur écoulé, échéance de montée, fin de submersion, phase, zones pacifiées. `entry["clock"]`.
- **Sert à** : RUN09, RUN10, RUN14, T-265 (submersion, fin du déluge, jours débloqués).

### 1.8 `water [x y]`
- **Effet** : niveau de la mer (`UIslandWaterSubsystem.get_sea_level`), surface et profondeur d'eau au pion ou au point donné (`get_water_surface_at`, `get_water_depth_at`, fonctions `BlueprintPure` existantes). Sous-système de monde : `ue.SubsystemBlueprintLibrary.get_world_subsystem(pawn, ue.IslandWaterSubsystem)`. Valeurs en cm. `entry["water"]`.
- **Sert à** : T-265 (montée des eaux, galion qui monte, barque hors grille), T-266 (barque échouée), T-163 (torche éteinte dans l'eau, bassin du requin).

### 1.9 `track <ClasseOuNom> [n]`
- **Effet** : position (m), vitesse linéaire (m/s) et angulaire (°/s) du composant racine des `n` acteurs les plus proches dont la classe ou le nom contient le motif (simulés ou non). `entry["track"]`.
- **Sert à** : barque projetée ou non (T-059, T-063, T-065), objet tenu qui tournoie (T-066), objets restés à bord (T-107), cadavre qui saute (T-151), morceaux de démembrement qui s'envolent (T-155).

### 1.10 `place <ClasseOuNom> <x> <y> <z> [yaw]`
- **Effet** : `set_actor_location_and_rotation` (téléportation) de l'acteur le plus proche du motif, physique réveillée, vitesse remise à zéro. `entry["place"]`.
- **Sert à** : poser la barque sur le sable (T-065), des objets dans la barque (T-107), un cadavre dans l'eau (T-068).

### 1.11 `summon <chemin de classe> [n]`
- **Effet** : fait apparaître `n` objets devant le joueur. Mise en œuvre la plus simple : la commande de triche `summon <chemin>_C` du `CheatManager` (hors Shipping), déjà joignable par `cmd` si les triches sont actives dans la partie ; sinon `ue.GameplayStatics.begin_deferred_actor_spawn_from_class` + `finish_spawning_actor` (disponibilité en Python 5.8 à vérifier).
- **Sert à** : objets de valeur à charger (barque pleine, quota atteint en RUN09, cupidité en T-267).

### 1.12 `tohull <x> <y> <z> <yaw> <pitch>`
- **Effet** : comme `tofort`, mais dans le repère de la coque du premier `AGalleonActor` (cm de la coque divisés par 100 : X en travers, Y le long, proue en +Y, Z depuis la flottaison ; relevés de `docs/features/execution-equipage.md`).
- **Sert à** : gouvernail (« Terminer la journée »), porte du gaillard d'avant (T-197), pont, échelle.

### 1.13 `sweep <distance m> [yaw]`
- **Effet** : balayage de la capsule du pion (`ue.SystemLibrary.capsule_trace_single`, rayon et demi-hauteur de sa capsule) depuis sa position, sur `distance`, dans la direction de sa vue (ou `yaw`). `entry["sweep"]` = premier acteur et composant touchés, distance, normale.
- **Sert à** : prouver sans clavier qu'une porte ou une cloison bloque (T-197), qu'un tronc de cocotier bloque et qu'un fourré se traverse (T-036).

### 1.14 `walk <avant> <droite> <secondes> [sprint]`
- **Effet** : pendant `secondes`, à chaque image, `pawn.add_movement_input` dans le repère de la vue (avant, droite entre −1 et 1) ; `sprint` active la course si la fonction est exposée (à vérifier) ; position de départ et d'arrivée, distance parcourue, vitesse moyenne. Demande le rappel par image de `wait` (§1.1).
- **Sert à** : marcher contre la porte (T-197), traverser un fourré (T-036), capturer la démarche en 3e personne (T-024, avec `camera`), dérive au vent hors abri et derrière une maison (T-067).

### 1.15 `crouch 1|0` et `jump`
- **Effet** : `pawn.crouch()` / `pawn.un_crouch()` et `pawn.jump()` (fonctions `BlueprintCallable` d'`ACharacter`). `entry["crouch"]` = `is_crouched` après l'appel ; `entry["jump"]` = endurance avant et après si elle est lisible.
- **Sert à** : furtivité devant le crabe et les mobs (T-207, T-262), coût d'endurance du saut (T-031).

### 1.16 `camera <x> <y> <z> <yaw> <pitch> | off`
- **Effet** : fait apparaître (ou déplace) un `CameraActor` de test et `controller.set_view_target_with_blend(camera, 0)` ; `off` rend la vue au pion. Le jeu reste en 1re personne pour le joueur : seule la vue change.
- **Sert à** : capturer le bean en 3e personne en marche et en course (T-024, recette §10), une scène de loin (démembrement, lancer du crabe).

### 1.17 `fx [motif]`
- **Effet** : liste les `NiagaraComponent` actifs du monde à moins de 30 m du joueur (asset, propriétaire, position, âge si lisible). `entry["fx"]`.
- **Sert à** : prouver l'apparition du sang (T-153, T-162) et de la pluie (T-203) en plus de la capture.

### 1.18 `light <ClasseOuNom> [échantillons] [intervalle]`
- **Effet** : intensité (cd), rayon d'atténuation et visibilité des `PointLightComponent` de l'acteur le plus proche ; avec `échantillons`, relevés successifs (demande `wait`). `entry["light"]`.
- **Sert à** : torche vacillante (9 cd / 9,5 m, levée 11 cd / 12,5 m), éteinte dans l'eau (T-099).
