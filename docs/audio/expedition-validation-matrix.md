# Matrice de validation : audio d'expédition (T-234, lot L5)

Raccordement du catalogue T-233 (138 demandes, `docs/audio/expedition-audio.html`) aux événements du jeu. Pour chaque son raccordé, la matrice dit où l'entendre en PIE et ce qui le valide. Elle ne valide rien par elle-même : **aucune écoute n'a encore eu lieu**, et une ligne ne passe « validée » qu'avec la date, la carte, le mode (solo, hôte + client) et le nom de la personne qui a écouté.

- **Lot** : L5 (V1).
- **Cahier** : §8 (tonnerre et flashs limités, contrôle de volume), §18 (canaux Master, musique, effets, ambiance, dialogues, UI ; préécoute puis Annuler), §19 (bonus accompagné d'un son bref, alerte unique à 60/30/10 s, pas de spam après une extension).
- **Recette** : §12 n°13 à 15 (horloge, bonus de zone), UI07 (audio préécouté puis Annuler), RUN09 (quota atteint).

## État au 2026-10-10

| Élément | État | Preuve |
|---|---|---|
| C++ `Source/monke_island/Audio/` (5 fichiers) dans l'arbre principal | copié seul depuis `.worktrees/T-234` (aucune autre source du worktree), compilé fichier par fichier en cible éditeur | `monke_island/Saved/T234/ubt-singlefile-*.log`, `docs/audio/expedition-code-validation.json` |
| Build éditeur complet (classes chargées dans l'éditeur) | non fait (lead) | — |
| Import des 76 WAV, mixage, liaisons | **non fait** : l'éditeur partagé a planté au premier passage (voir « Incident ») | aucun asset sous `/Game/MonkeIsland/Audio/Expedition` |
| `DA_ExpeditionAudioRules` (table événement → sons) | après le build | — |
| Test `monke_island.Audio.Expedition.Transitions` | compilé, non exécuté | — |
| Écoute PIE, hôte + client, build packagé | non faits | — |

## Ordre pour le lead

1. Build éditeur complet : il charge `UExpeditionAudioRules` et `UExpeditionAudioSubsystem` (nouveaux en-têtes `UCLASS` / `USTRUCT`, pas de Live Coding).
2. `monke_island/Config/DefaultGame.ini`, section `[/Script/UnrealEd.ProjectPackagingSettings]` : ajouter `+DirectoriesToAlwaysCook=(Path="/Game/MonkeIsland/Audio/Expedition")`. Le subsystem charge ses règles par chemin ; un `PrimaryAssetLabel` ne cuirait rien ici (le moteur scanne les labels en éditeur seulement, `BaseGame.ini`).
3. Éditeur relancé avec la session (`scripts/editor/lead_session_t186.py`), sans PIE : action `expeditionaudio`, à rejouer jusqu'à `"complete": true` dans `monke_island/Saved/T234/import-report.json`. Chaque passage gèle l'éditeur moins de 20 s (imports limités à 15 s, reprise par empreinte SHA-256). Ne pas l'enchaîner juste après une action d'import FBX qui a échoué : redémarrer l'éditeur d'abord.
4. `scripts/ue.ps1 test -Filter monke_island.Audio.Expedition`.
5. Écoute selon les tableaux ci-dessous, puis `python scripts/audio/expedition_catalogue.py` (le catalogue reprend les preuves d'import).

## Où écouter

- **Test Flat** (`/Game/MonkeIsland/Maps/Test/L_Test_Flat`, `BP_TestGameMode`) : ni expédition, ni inondation, joueur invulnérable, ni acteur de vent, ni cycle jour/nuit, pas de marchand repéré dans la carte. On y trouve l'eau, le galion, la barque, les soldats du fort, les crabes, les caisses et objets.
- **Expédition** (hub galion, lever l'ancre, fragment) : horloge, tempête de la submersion, nuit, douleur et mort, quota, issue de la campagne, marchand.
- **Console** : `monke.Audio.Report` (règles chargées, nombre de cues, boucles actives, envois par événement) ; `monke.Audio.Play <Événement>` (audition locale d'un cue ponctuel ; les boucles ne passent pas par là). « submitted » ne prouve pas que le son est audible. Pour l'horloge : `monke.SetTime <secondes écoulées>` (serveur), `monke.SetHour 22` (nuit), `monke.Run.Money 500` (or), `monke.SpawnBoat`.

## A. Sons remplacés dans des systèmes existants (actifs dès l'import, sans le build)

Chaque son de remplacement reprend la classe (`SC_Effects`), l'atténuation, la concurrence et la priorité du son qu'il remplace : le système qui le joue se comporte comme avant, seul l'enregistrement change. Valeurs avant/après relues après sauvegarde : `import-report.json` ; copies des DataAssets avant écriture : `monke_island/Saved/T234/backups/`.

| # | Événement | Son | Déclencheur | Où, comment | Critère | Catalogue |
|---|---|---|---|---|---|---|
| A1 | Pas sur sable sec | `SW_Exp_SandStep_01..04` (4 pas isolés d'un enregistrement sur sable, CC0) au lieu de `SW_Step_SandDry_0N` (pas sur neige, substitut) | `UFootstepSubsystem`, surface SandDry (plus de 60 cm au-dessus de l'eau, ou mesh « Sand » / « Beach ») | Test Flat : marcher puis courir sur le sable sec, puis descendre vers l'eau | crissement de sable, plus de neige ; 4 variantes sans répétition audible ; niveau comparable aux pas sur bois et pierre ; passage net au sable humide (sons inchangés) près de l'eau | AUD-03-01 |
| A2 | Sabre dégainé | `SW_Exp_BladeDraw` au lieu de la synthèse `SW_Cutlass_Draw` | `DA_CutlassRules.DrawSound` | Test Flat : sortir le sabre, vue 1re et 3e personne | un son par geste, calé sur la sortie de la lame | AUD-04-01 |
| A3 | Sabre rengainé | `SW_Exp_BladeSheathe` au lieu de `SW_Cutlass_Sheathe` | `SheatheSound` | Test Flat : ranger le sabre | un son par geste, fin nette | AUD-04-02 |
| A4 | Parade | `SW_Exp_BladeParry` (choc métallique) au lieu de `SW_Cutlass_Parry` | `ParrySound` | Test Flat : parer le coup d'un soldat du fort | choc net, jamais superposé à `HitSound` (son du propriétaire) | AUD-04-05, AUD-04-07 |
| A5 | Soldat qui dégaine | `SW_Exp_BladeDraw` | `DA_FortSoldierRules.DrawSound` | Test Flat : approcher la garnison jusqu'à l'alerte | audible de près seulement (`ATT_Cutlass`), un son par soldat | AUD-06-20 |

## B. Subsystem T-234 (après le build et la table `DA_ExpeditionAudioRules`)

`UExpeditionAudioSubsystem` : présentation locale en 2D pour le joueur de cette machine ; il lit les états répliqués (horloge, quota, issue, vent, jour/nuit, galion, nage, santé, inventaire, boutique), sans RPC et sans toucher au gameplay. Absent sur serveur dédié et en commandlet.

### Ambiances (boucles en fondu de 0,8 s, curseur Ambiance)

| # | Événement | Son | Déclencheur | Où, comment | Critère | Catalogue |
|---|---|---|---|---|---|---|
| B1 | Eau contre la coque | `SW_Exp_HullWater` (boucle de 2,8 s) | à bord du galion, vivant, tête hors de l'eau | Test Flat : monter sur le pont, rester 1 min | clapot discret sous `SW_Music_Galleon` ; **aucune répétition audible** (boucle courte : point à écouter en premier) | AUD-01-01 |
| B2 | Craquements du galion | `WoodCreak_01..03`, un toutes les 9 à 19 s | à bord | Test Flat : pont puis cale, 1 min | espacés, jamais deux fois le même d'affilée ; pas de doublon gênant avec les craquements de `SW_Music_Galleon` | AUD-02-01, AUD-02-12 |
| B3 | Sous l'eau | `SW_Exp_Underwater` (bulles filtrées à 520 Hz, 3,9 s) | caméra à plus de 12 cm sous la surface | Test Flat : plonger, remonter | entre et sort en fondu ; coupé dès la remontée | AUD-01-11 |
| B4 | Vent léger | `SW_Exp_WindLight` (29,5 s) | dehors, tête hors de l'eau, vent sous 0,5 | Test Flat (sans acteur de vent : couche légère permanente) ; expédition par temps calme | discret ; coupé sous l'eau | AUD-07-01 |
| B5 | Vent fort | `SW_Exp_WindStrong` | vent au-dessus de 0,5 (rafale) | expédition, pendant une rafale | relais en fondu avec le vent léger, sans trou ni empilement | AUD-01-07 |
| B6 | Tempête | `SW_Exp_WindStorm` + `SW_Exp_Rain` | vent de la submersion (`IsDeadlineWind`) | expédition : `monke.SetTime 535` puis attendre l'échéance | vent violent et pluie ensemble ; arrêtés au changement de jour et au voyage | AUD-01-09 |
| B7 | Début de tempête | jingle `SW_Exp_Danger` | entrée dans la tempête, une fois | idem B6 | une seule fois par tempête | AUD-09-12 |
| B8 | Tonnerre | `SW_Exp_Thunder` | toutes les 18 à 36 s pendant la tempête | idem B6, 2 min | au plus un coup toutes les 18 s (§8 « tonnerre limité ») ; suit le curseur Ambiance | AUD-01-08 |
| B9 | Grillons | `SW_Exp_Crickets` (11 s) | nuit, dehors, ni à bord ni en barque, hors tempête | expédition : `monke.SetHour 22`, puis `monke.SetHour 6` | montent et s'éteignent en fondu ; muets à bord | AUD-07-04 |

### Joueur (curseur Effets, sauf mention)

| # | Événement | Son | Déclencheur | Où, comment | Critère | Catalogue |
|---|---|---|---|---|---|---|
| B10 | Entrée et sortie de nage | `SW_Exp_SwimEnter` / `SW_Exp_SwimExit` | le mode de déplacement passe en nage, puis en sort | Test Flat : entrer en eau profonde, ressortir | une gerbe par transition ; pas de rafale en oscillant à la limite (0,3 s mini) | AUD-03-09 |
| B11 | Ramassage | `SW_Exp_Gem_01/02` (curseur UI) | le nombre d'objets de l'inventaire augmente | Test Flat : prendre un objet dans une caisse | un son par gain ; silence en équipant, en réorganisant, au chargement de l'inventaire de départ ; pas de gemme juste après un achat (0,6 s) | AUD-03-10, AUD-08-01 |
| B12 | Prise d'un objet | `SW_Exp_WoodHit_01` | début de prise d'un objet physique | Test Flat : saisir une caisse ou un objet | un son à la prise, rien au lâcher | AUD-03-11 |
| B13 | Douleur | `SW_Villager_Hurt_01` (voix de villageois tiki, **provisoire**) | perte de vie non mortelle | expédition (joueur invulnérable sur Test Flat) | une voix par coup, 0,35 s mini entre deux | AUD-03-07 |
| B14 | Mort | `SW_Villager_Death_01` (**provisoire**) | mort | expédition | une fois, sans cri de douleur simultané | AUD-03-08 |
| B15 | Saut, réception | aucun son configuré : silence voulu | — | Test Flat : sauter | aucun double son à la réception (celle des pas existe déjà) | AUD-03-05 |

### Marchand (curseurs Musique et UI)

| # | Événement | Son | Déclencheur | Où, comment | Critère | Catalogue |
|---|---|---|---|---|---|---|
| B16 | Ouverture de la boutique | jingle `SW_Exp_Merchant` | demande d'ouverture faite par ce joueur | galion du hub | un jingle par ouverture ; rien chez les autres joueurs | AUD-10-09 |
| B17 | Achat | `SW_Exp_Coin_01..03` | résultat serveur « Bought » | `monke.Run.Money 500`, puis acheter | après la confirmation du serveur, jamais avant ; pas de gemme de ramassage en plus | AUD-06-09, AUD-06-10 |
| B18 | Achat refusé | jingle `SW_Exp_QuotaFailed` | tout autre résultat (or insuffisant, trop loin, offre invalide) | acheter sans or | un son par refus (même son pour toutes les raisons) | AUD-06-11 |

### Horloge et partie (§19)

| # | Événement | Son | Déclencheur | Où, comment | Critère | Catalogue, recette |
|---|---|---|---|---|---|---|
| B19 | Début d'expédition | jingle `SW_Exp_DayStart` | expédition commencée depuis moins de 2 s | lever l'ancre depuis le hub | une fois ; rien pour un joueur arrivé en retard | AUD-09-01, AUD-09-02, AUD-09-10 |
| B20 | Mi-parcours | `SW_Exp_Bell` | le temps restant passe sous la moitié du délai initial (270 s pour 540 s) | `monke.SetTime 260`, attendre 10 s | une seule fois, même si un bonus de zone fait remonter le temps au-dessus | AUD-09-03 ; recette n°14, 15 |
| B21 | Compte à rebours | `SW_UI_Click` | secondes 9, 8 … 1 | `monke.SetTime 528` | 9 tics ; rien à 10 s (alerte native), rien à 0 (son d'évacuation natif) ; aucun tic si un bonus fait remonter le temps | §19 ; AUD-09-05 |
| B22 | Alertes 60/30/10 s et bonus de zone (existants, `DA_HudTimerRules`) | `S_HudTimer_Alert`, `_Evacuation`, `_ZoneSecured` | seuils de l'horloge, validation de zone | même session que B20 et B21, puis une zone pacifiée | aucun son T-234 par-dessus ; une alerte par seuil ; bonus annoncé par un son bref | §19 ; recette n°13 à 15 |
| B23 | Quota atteint | jingle `SW_Exp_QuotaMet` | `IsQuotaMet` passe à vrai | atteindre le quota du jour | une fois ; pas rejoué à la réplication, à l'arrivée tardive ni au voyage | AUD-09-08, AUD-02-14 ; RUN09 |
| B24 | Victoire ou défaite de la campagne | jingles `SW_Exp_Victory` / `SW_Exp_Defeat` | l'issue de la campagne change | dernier contrat réussi, ou quota manqué (`monke.Run.Day 10`) | une fois, y compris quand le verdict tombe au choix de fin de journée, hors expédition | AUD-09-14, AUD-10-12, AUD-10-13 |

Remarque : un saut d'horloge qui franchit la moitié du délai (par exemple `monke.SetTime 528` depuis le début) déclenche aussi la cloche, une fois ; sauter par paliers pour des écoutes séparées.

## C. Contrôles transverses

| # | Contrôle | Critère |
|---|---|---|
| C1 | Curseurs audio (§18) | Ambiance à 0 : B1 à B9 muets. Musique à 0 : jingles muets (B7, B16, B18, B19, B23, B24). UI à 0 : pièces et gemmes muettes. Effets à 0 : pas, sabre, nage, prise, douleur, compte à rebours, cloche muets. UI07 : préécoute puis Annuler restaure les valeurs. |
| C2 | Voyage, changement de jour, pause, retour au menu | aucune boucle T-234 ne survit ; `monke.Audio.Report` sur le nouveau monde : `loops=0` tant qu'aucune condition d'ambiance n'est remplie |
| C3 | Hôte + client | chacun n'entend que sa propre présentation, une seule fois ; les sons 3D natifs (crabes, armes, barque, pas) restent spatialisés |
| C4 | Arrivée tardive | ni début d'expédition, ni tic, ni jingle de quota ou d'issue au premier échantillon |
| C5 | Budget de voix | `CON_Exp_Ambience` 8, `CON_Exp_Foley` 12, `CON_Exp_Music` 2 (le plus faible s'arrête) ; en tempête, `au.Debug.Sounds 1` ou `stat soundwaves` sans empilement |
| C6 | Plage dynamique Nuit / Standard / Large (§18) | hors T-234 |

## Décisions de liaison

Le script n'écrit un champ que s'il est vide ou contient encore un son provisoire listé ; sinon le champ figure dans `skipped` du rapport. Les champs ci-dessous ne sont jamais écrits ; leurs valeurs du moment sont relevées dans `left_alone`.

| Champ | Son en place | Décision | Raison |
|---|---|---|---|
| `DA_FootstepRules` SandDry | `SW_Step_SandDry_0N` (pas sur neige Kenney, substitut documenté dans `art/SOURCES.md`) | remplacé (A1) | vrai sable enregistré |
| `DA_CutlassRules` Draw, Sheathe, Parry | synthèses de première passe (`scripts/props/cutlass_sounds.py`) | remplacés (A2 à A4) | coup et impact du même lot déjà remplacés par les sons du propriétaire (T-259) |
| `DA_FortSoldierRules.DrawSound` | `SW_Cutlass_Draw` | remplacé (A5) | même lame que le joueur |
| `DA_FootstepRules` SandWet | `SW_Step_SandWet_01..04` | gardé | 4 variantes faites pour le sable humide, contre 3 pas mouillés génériques |
| `DA_BoatRules.WakeSound` | `SW_Boat_Wake` | gardé | boucle synthétique de 8 s sans raccord, volume selon la vitesse ; `HullWater` ne dure que 2,8 s |
| `DA_BoatRules.OarSplashSound` | `SW_Boat_OarSplash_01` | gardé | synthèse sur mesure (demande du 2026-10-07), cohérente avec la goutte de rame |
| `DA_ShoreAmbienceRules` Surf, Sea | `SW_Shore_Surf` (48 s), `SW_Shore_Sea` (32 s) | gardés | candidats CC0 de 2,1 s et 1,8 s : répétition audible |
| `DA_CutlassRules` Swing, FinisherSwing, Hit, Kill | sons du propriétaire (T-259) | jamais écrits | — |
| `DA_MusicRules` Galleon, Boat | pistes du propriétaire (T-259) | jamais écrits | — |
| Règles des armes v2 (`DA_PistolRules`…) | T-259 | non référencées par le script | — |
| `DA_GiantCrabRules` | sons dédiés du crabe boss (T-261) | jamais écrits | — |
| `DA_GiantCrabRules_Sand` | champs vides | laissés vides | même boss : à remplir par T-261 avec ses sons dédiés, pas avec le foley bois de T-234 |

## Limites connues

- `HullWater` (2,8 s), `Underwater` (3,9 s) et `Rain` (6,3 s) sont des boucles courtes : à écouter en premier. Si la répétition s'entend, baisser le cue, ou monter une boucle plus longue à partir de plusieurs prises (non fait).
- Tonnerre non synchronisé à des éclairs. Pas d'occlusion sous le pont : pluie et vent identiques dans la cale. Le vent ne tient pas compte des abris (`AWindStormActor::GetShelterFactor` existe, non utilisé).
- Achat refusé : un seul son, quelle que soit la raison.
- Douleur et mort du joueur : voix de villageois tiki provisoires ; voix françaises à enregistrer (`docs/audio/expedition-voix-fr.txt`).
- Jingles (pizzicati CC0) : couleur à confirmer à l'écoute ; les musiques déjà en place ne sont pas remplacées.
- Crabes du sable : `ClackSound`, `HitSound`, `SlamSound` et cues vides (T-261).
- Soldats du fort : `SwingSound` et `HitSound` restent les synthèses `SW_Cutlass_Swing` / `SW_Cutlass_Hit`, alors que le joueur a les sons du propriétaire (constat, hors T-234).
- Sur les 138 demandes du catalogue, la plupart attendent un déclencheur qui n'existe pas encore (canons, portes, coffres, ancre, voiles, vente, rareté, malédiction…). Un son préparé n'implémente pas la mécanique ; aucune demande n'est validée en jeu.

## Incident du 2026-10-10

L'éditeur partagé a planté à 02:01:51 (heure locale) : `EXCEPTION_ACCESS_VIOLATION` dans `libfbxsdk.dll`, appelé par AssetTools depuis Python, image moteur `[891]`, une seconde après le début du premier passage d'`expeditionaudio`. L'action précédente d'une autre session (`shreddemo`, imports FBX) avait échoué à 02:01:48 dans la même image, et un maillage squelettique était encore en construction à 02:01:48.469. La requête T-234 a donc été exécutée dans un tick Slate imbriqué, pendant la fin du traitement FBX. Aucun paquet T-234 n'a été sauvegardé et aucun rapport n'a été écrit. Garde proposée pour la session du lead : ne pas lancer d'action dans la même image que la précédente (`unreal.SystemLibrary.get_frame_count()`), refuser la ré-entrance, et redémarrer l'éditeur après l'échec d'un import FBX, avant tout autre import.
