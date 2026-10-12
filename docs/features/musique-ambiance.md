# Feature : Musique d'ambiance (hawaïenne, nuit, Couronne) et pistes du galion et de la barque

- **Statut** : review — codé, compilé, tests `monke_island.Music` verts ; vérifié en PIE par les journaux (2026-10-07), écoute par le propriétaire à faire. T-259 (2026-10-09) : pistes `Galleon` et `Boat` codées et compilées en fichier seul, sons importés ; `DA_MusicRules` à câbler après le build complet (action `owneraudio`), non testé en PIE
- **Owner** : lead (rôle `ue-audio-designer`)
- **Date** : 2026-10-07
- **Cahier des charges** : lot L5 (ambiance) — tâches T-220, T-259

## 1. Intention
Trois morceaux, choisis par le propriétaire (sources `OneDrive\maquette-blender\audio`, lecture seule) :
- la chanson hawaïenne accompagne le jeu par défaut (menus, barque, galion, île de jour) ;
- à la tombée de la nuit sur l'île, elle baisse jusqu'au silence et la chanson de nuit prend le relais ;
- pendant la levée du drapeau d'un village, la chanson britannique joue une minute ;
- T-259 (sons du 2026-10-08, « je veux les entendre dans Flat ») : à bord du galion, les craquements du navire (`galion_song`) ; à une place de la barque, assis ou debout, les coups de rame (`barque_song`). L'analyse a montré deux ambiances, pas des chansons : elles passent par la classe `SC_Ambience` (curseur « ambiance »).

## 2. Règles et valeurs (`DA_MusicRules`, `/Game/MonkeIsland/Audio/Music`)
| Paramètre | Valeur | Rôle |
|---|---|---|
| `Hawaiian` / `Night` / `Crown` | `SW_Music_Hawaiian` (8 min), `SW_Music_Night` (4 min), `SW_Music_Crown` (75,8 s) | morceaux en boucle, classe `SC_Music` |
| `Galleon` / `Boat` | `SW_Music_Galleon` (64,9 s), `SW_Music_Boat` (42,1 s) | ambiances en boucle sans raccord audible, classe `SC_Ambience` (T-259) |
| `GalleonVolume` / `BoatVolume` | 0,8 / 0,7 | volumes pleins (T-259) |
| `HawaiianVolume` / `NightVolume` / `CrownVolume` | 0,55 / 0,6 / 0,75 | volumes pleins |
| `FadeSeconds` / `NightFadeSeconds` | 2,5 s / 6 s | fondus enchaînés |
| `DaylightSilent` / `DaylightFull` | 0,15 / 0,55 | lumière du jour où la chanson hawaïenne se tait / est pleine (crépuscule) |
| `CeremonyRadius` | 4 000 cm | distance du drapeau qui se lève |
| `CrownSeconds` | 60 s | durée de la chanson britannique |
| `CheckInterval` | 0,25 s | lecture du monde |

Choix du morceau (`MusicMath::PickTrack`, `MusicMath::PickPlace`) :
1. à une place d'une barque (`UBoatRiderComponent::IsSeated`, places debout comprises) : piste de la barque, sur toutes les cartes ;
2. à bord du galion (`AGalleonActor::IsActorAboard`) : piste du galion, sur toutes les cartes (la barque passe d'abord : une barque le long du bord peut être dans le volume du galion) ;
3. ailleurs, en expédition, rien tant que le joueur n'a pas posé le pied sur l'île (nage vers l'île, scène d'arrivée) ;
4. drapeau en train de monter à moins de 40 m, pendant la première minute : chanson britannique ;
5. nuit (phase `Night` du cycle jour/nuit) sur l'île : chanson de nuit ;
6. sinon, chanson hawaïenne (menus, cartes sans expédition, île de jour). Elle ne baisse avec la lumière que sur l'île.

## 3. Critères d'acceptation
- [x] La musique démarre au premier monde de jeu (journal `Music: DA_MusicRules ready`, `Music: SW_Music_Hawaiian starts`).
- [x] La nuit sur l'île : `Music: Night (night 1, sailing 0, flag 0)` puis `SW_Music_Night starts` (PIE, `monke.SetHour 22.5`).
- [x] Retour à bord la nuit : `Music: Hawaiian (night 1, sailing 1, flag 0)`, au volume plein (correctif du 2026-10-07).
- [ ] Chanson britannique pendant la trompette d'un drapeau : non vérifiée en jeu (règle couverte par `monke_island.Music.Rules`).
- [ ] T-259 : à bord du galion de Test Flat, journal `Music: Galleon (… galleon 1 …)` puis `SW_Music_Galleon starts` ; assis dans une barque, `Music: Boat (… boat 1 …)` puis `SW_Music_Boat starts` ; en descendant, retour à la chanson hawaïenne (après le build complet et l'action `owneraudio`).
- [ ] Écoute par le propriétaire (volumes, fondus).

## 4. Design technique
- `UMusicSubsystem` (`Music/`) : sous-système d'instance de jeu, joue en local et jamais sur un serveur dédié. Il lit le monde 4 fois par seconde. Les composants audio 2D persistent d'une carte à l'autre, et un morceau silencieux est mis en pause plutôt qu'arrêté.
- Le chemin `MusicRulesPath` se règle dans `DefaultGame.ini`. Le dossier `/Game/MonkeIsland/Audio/Music` est toujours cuisiné, car le DataAsset n'est référencé par aucune carte.
- Conversion : `scripts/audio/convert_music.py` (module `aud` de Blender, MP3 vers WAV 44,1 kHz) ; les WAV restent hors du dépôt (`art/audio/music/*.wav`, `.gitignore`).
- Import et DataAsset : `scripts/audio/music_setup_ue.py` (action `music`).
- T-259 : `scripts/audio/convert_owner_sounds.py` (Blender `aud`, décodage en flottant) puis `owner_sounds_prepare.py` (48 kHz mono ; galion : lit de 64,9 s recomposé de morceaux du fichier de 5 s, coupés entre les coups, fin raccordée au début ; barque : boucle coupée au milieu des silences entre deux coups de rame) → `art/audio/owner/` ; import et câblage : `owner_sounds_setup_ue.py` (action `owneraudio`, idempotente : les propriétés `Galleon`, `Boat`, `GalleonVolume`, `BoatVolume` sont notées « en attente » tant que l'éditeur n'a pas été recompilé).
- Origine et licence des morceaux : à confirmer par le propriétaire avant toute publication.

## 5. Tests
- `monke_island.Music.Rules` (choix du morceau, volume selon la lumière, fondus ; T-259 : galion et barque de jour comme de nuit, avec ou sans expédition, priorité de la barque sur le galion).
