# Fiche son : armes v2, sabre et pistes du galion et de la barque (T-259, lot L5)

Demandes du propriétaire du 2026-10-09 : « audio nouveau » pour les nouvelles armes, qualité « type Call of Duty » sur des armes à silex du XVIIIe siècle ; « check aussi les nouveaux audio dans le OneDrive maquette et ajoute-les, je veux les entendre dans Flat ».
**Statut : review.** Sons préparés et vérifiés hors Unreal, importés et câblés dans l'éditeur partagé (propriétés relues après sauvegarde). Rien n'a été écouté en PIE ; pistes du galion et de la barque en attente du build complet du lead.

## Armes à feu : événements des gestes
Les noms d'événements sont ceux des manifestes des packs (`Weapons/PirateWeaponClips.inl`, `Clips[].Events` des `DA_*Rules`). Le tir (`FireSound`) part à l'événement `FireEvent` (`Fire` pour les pistolets et le fusil à pompe, `Shot` pour le mousquet, la carabine et le tromblon). Tous les autres sons sont des `EventSounds` attachés à l'arme, avec `EventSoundAttenuation` = `ATT_Firearm_Handling`. Le rapport du script vérifie que chaque événement des clips a un son (hors `Fire`, `Shot`, `PanFlash`, `MuzzleFlash`, `Recoil`, `ChamberSlug`, et `DryClick` des pistolets, couvert par `HammerDrop`).

| Événement | Déclencheur | Asset | Variations | Atténuation | Priorité / concurrency | Source + licence |
|---|---|---|---|---|---|---|
| Tir du pistolet | `Fire` | `SC_Pistol_Fire` | 3 (`Random` sans répétition) | `ATT_Firearm_Shot` | 2 / `SCON_Firearm_Shot` (12) | kurt *Black Powder*, bruno.auzet, fennelliott + couches synthétiques (sub, écho), CC0 |
| Tir du pistolet double | `Fire` (chaque canon) | `SC_DoublePistol_Fire` | 3 | `ATT_Firearm_Shot` | 2 / 12 | kurt, mlsulli, fennelliott, CC0 |
| Tir du fusil à pompe | `Fire` | `SC_Shotgun_Fire_V2` | 3 | `ATT_Firearm_Shot` | 2 / 12 | kurt, bruno.auzet, rubberduck *bangs*, CC0 |
| Tir du mousquet | `Shot` | `SC_PirateMusket_Fire` | 3 | `ATT_Firearm_Shot` | 2 / 12 | bruno.auzet, mlsulli, fennelliott, kurt, Thimras, CC0 |
| Tir de la carabine | `Shot` | `SC_Rifle_Fire` | 3 | `ATT_Firearm_Shot` | 2 / 12 | craigsmith *Flintlock rifle fire*, kurt, bruno.auzet, CC0 |
| Tir du tromblon | `Shot` | `SC_Blunderbuss_Fire` | 3 | `ATT_Firearm_Shot` | 2 / 12 | fennelliott, bruno.auzet, Thimras, rubberduck, kurt + grêle de plombs synthétique, CC0 |
| Silex, bassinet (armes longues) | `Trigger` (125 ms avant `Shot`) | `SC_Longgun_Trigger` | 3 | Handling | 1 / `SCON_Firearm_Handling` (10) | N3pomuk (chien), ImmergoMedia (étincelles), craigsmith et fennelliott (éclair du bassinet), CC0 |
| Chien des pistolets (tir et tir à vide) | `HammerDrop` | `SC_Pistol_HammerDrop` | 3 | Handling | 1 / 10 | N3pomuk, ImmergoMedia, CC0 |
| Armer le chien (pistolets) | `CockHammer`, `HammerCocked` | `SC_Pistol_Cock` | 2 | Handling | 1 / 10 | N3pomuk, CC0 |
| Demi-armé, armé (armes longues) | `HalfCockStart`, `FullCockEnd` | `SW_Longgun_HalfCock`, `SW_Longgun_FullCock` | 1 | Handling | 1 / 10 | N3pomuk, CC0 |
| Amorçage, batterie | `PanPrime`, `FrizzenClose` | `SW_Longgun_PanPrime`, `SW_Longgun_FrizzenClose` | 1 | Handling | 1 / 10 | LFA *equipment clicks III* + grains synthétiques ; LFA + N3pomuk, CC0 |
| Crosse au sol, poudre, balle | `StockLowered`, `PowderCharge`, `BallAndPatch` | `SW_Longgun_StockGround`, `_PowderCharge`, `_BallPatch` | 1 | Handling | 1 / 10 | Kenney *Impact Sounds*, rubberduck (papier), Kenney *RPG Audio*, coulée synthétique, CC0 |
| Baguette | `RamrodDraw`, `RamrodTurn`, `RamrodStroke` (×2), `RamrodReturn` | `SW_Longgun_RamrodDraw`, `_RamrodTurn`, `SC_Longgun_RamrodStroke` (3), `SW_Longgun_RamrodReturn` | 1 / 3 | Handling | 1 / 10 | Kenney, LFA, artisticdude *Swishes* + frottement synthétique, CC0 |
| Plombs (tromblon), boîte à calepins (carabine) | `ShotPour`, `PatchBox` | `SW_Blunderbuss_ShotPour`, `SW_Rifle_PatchBox` | 1 | Handling | 1 / 10 | Kenney *RPG Audio* + grains synthétiques, CC0 |
| Rechargement des pistolets | `AssistLeft`, `PaperPickup`, `PaperTransfer`, `RamrodGesture`, `RamrodReturned` | `SW_Firearm_HandGrab`, `SW_Pistol_PaperPickup`, `_PaperTransfer`, `_Ramrod`, `_RamrodReturn` | 1 | Handling | 1 / 10 | Kenney, rubberduck (papier), N3pomuk, CC0 |
| Pompe | `PumpBackStart`/`PumpForwardStart`, `PumpBackEnd`, `PumpForwardEnd` | `SW_Shotgun_PumpSlide`, `SC_Shotgun_PumpBack` (3), `SC_Shotgun_PumpForward` (3) | 1 / 3 | Handling | 1 / 10 | kerozenne *ShotgunPump*, CC0 |
| Douille, cartouches | `EjectSpentShell`, `ShellPickup`, `InsertSlug` | `SW_Shotgun_ShellEject`, `_ShellPickup`, `SC_Shotgun_ShellInsert` (3) | 1 / 3 | Handling | 1 / 10 | zer0_sol *Shotgun Reload*, Kenney, LFA, CC0 |
| Tir à vide du fusil à pompe | `DryClick` | `SW_Shotgun_DryClick` | 1 | Handling | 1 / 10 | N3pomuk, LFA, CC0 |
| Sortir, prêt, ranger | `DrawWeapon`, `Ready`, `StowWeapon` | `SW_Firearm_Draw`, `_Ready`, `_Stow` | 1 | Handling | 1 / 10 | Kenney *RPG Audio*, CC0 |
| Arme vide jetée | `Empty`, `Release` (clip `Discard`) | `SW_Firearm_DiscardEmpty`, `SC_Firearm_DiscardRelease` (2) | 1 / 2 | Handling | 1 / 10 | N3pomuk, Kenney, artisticdude ; chute au sol incluse à +0,3 s, CC0 |

## Grenade
| Événement | Déclencheur | Asset | Variations | Atténuation | Priorité / concurrency | Source + licence |
|---|---|---|---|---|---|---|
| Mèche allumée | `Light` | `SW_Grenade_Light` (0,95 s, s'éteint avant le lancer) | 1 | Handling | 1 / 10 | squareal *Match Sizzle*, craigsmith *Fuse Burning*, themightyglider, CC0 |
| Lancer | `Fire` | `SW_Grenade_Throw` | 1 | Handling | 1 / 10 | artisticdude, Kenney, CC0 |
| Sac, grenade suivante, sac vide, sac jeté | `BagReach`, `HammerCocked`, `DryClick`/`Empty`, `Release` | `SW_Grenade_BagReach`, `_Take`, `_EmptyBag`, `_BagToss` | 1 | Handling | 1 / 10 | Kenney, artisticdude, CC0 |
| Rebond | `BounceSound` (volume selon la vitesse d'impact, au plus un toutes les 0,15 s) | `SC_Grenade_Bounce` | 3 | `ATT_Grenade_Bounce` | 1,5 / 10 | Kenney *Impact Sounds*, CC0 |
| Explosion | `ExplosionSound` | `SC_Grenade_Explosion` | 3 | `ATT_Grenade_Explosion` | 3 / `SCON_Grenade_Explosion` (6) | unfa *Grenade Explosion*, Kinoton, qubodup, Thimras, rubberduck (gravats) + sub, pluie de sable et écho synthétiques, CC0 |

## Sabre (sons du propriétaire, OneDrive `maquette-blender/audio`)
| Événement | Asset | Traitement | Source |
|---|---|---|---|
| `SwingSound` (coups du combo, estoc) | `SW_Cutlass_Owner_Swing` | début coupé : le pic tombe à 0,20 s (lames actives 0,14 à 0,32 s) | `epe_slash_1_song.mp3` |
| `FinisherSwingSound` | `SW_Cutlass_Owner_FinisherSwing` | 0,195 s de silence ajoutés : le pic tombe à 0,53 s (lame active 0,49 à 0,62 s, coup à 0,57 s) | `epe_slash_2_song.mp3` |
| `HitSound` et `KillSound` | `SW_Cutlass_Owner_Hit` | commence 25 ms avant l'impact (le souffle est déjà dans le coup), fin à -45 dB | `epe_enemie_toucher.mp3` |
Les anciens `SW_Cutlass_*` restent dans le projet ; `ATT_Cutlass` et `SoundPitchJitter` (±8 %) inchangés.

## Ambiances : galion et barque (pistes de `UMusicSubsystem`)
- `SW_Music_Galleon` (`galion_song.mp3`, 5 s) : craquements de coque, coups sourds, grondement grave. Une boucle de 5 s s'entendrait : lit de 64,9 s fait uniquement de cet enregistrement (34 morceaux de 1,4 à 3 s coupés entre les coups, ordre tiré au hasard avec une graine fixe, fondus à puissance constante, fin raccordée au début).
- `SW_Music_Boat` (`barque_song.mp3`, 49 s) : coups de rame dans l'eau, un toutes les 2,45 s. Boucle de 42,1 s coupée au milieu de deux silences entre les coups : le rythme continue au raccord.
- Joués en 2D, classe `SC_Ambience` (curseur « ambiance »), en boucle, mis en pause hors de leur lieu. Choix du morceau : barque d'abord (assis ou debout à une place), puis galion, sur toutes les cartes (Test Flat comprise) ; ailleurs rien ne change (`MusicMath::PickTrack`, spec `monke_island.Music.Rules`).

## Mixage
- Classes : armes et sabre `SC_Effects` (curseur « effets ») ; galion et barque `SC_Ambience`.
- Niveaux des fichiers : tirs et explosion à -1 dBFS crête, limités jusqu'à une intensité cible des 100 ms les plus fortes (tromblon -7, mousquet et carabine -8,5, fusil à pompe -9, pistolet double -10, pistolet -11, explosion -6 dB) ; silex et chiens -5 à -8 dBFS ; rechargements -10 à -15 ; manipulations -14 à -18. Les volumes des `EventSounds` (0,5 à 1) font le reste.
- Atténuations (`/Game/MonkeIsland/Weapons/Audio`) : `ATT_Firearm_Shot` plein jusqu'à 15 m puis décroissance naturelle jusqu'à 300 m, aigus coupés avec la distance (20 kHz à 20 m, 2,5 kHz à 300 m) ; `ATT_Firearm_Handling` plein jusqu'à 1,5 m, nul à 20 m ; `ATT_Grenade_Explosion` plein jusqu'à 20 m, jusqu'à 400 m, 1,8 kHz au loin ; `ATT_Grenade_Bounce` 2 m / 35 m. Toutes passent en 2D entre 1,5 m et 0,5 m (le porteur entend son arme au centre).
- Concurrence : 12 tirs, 10 sons de manipulation, 6 explosions ; au-delà, le plus lointain puis le plus ancien s'arrête.

## Fichiers et scripts
- Sources CC0 : `scripts/audio/weapons_v2_sources.json` → `weapons_v2_fetch.py` (licence vérifiée sur chaque page, empreintes dans `art/audio_src/weapons_v2/inventory.json`, hors Git).
- Conception : `scripts/audio/weapons_v2_prepare.py` → `art/audio/weapons/v2/*.wav` (79 WAV, 48 kHz PCM16 mono) + `manifest.json` (sources, instants, traitements, mesures de chaque fichier).
- Sons du propriétaire : `convert_owner_sounds.py` (Blender `aud`) → `owner_sounds_prepare.py` → `art/audio/owner/*.wav` + `manifest.json`.
- Unreal : actions `weaponsaudio` (`weapons_audio_setup_ue.py`) et `owneraudio` (`owner_sounds_setup_ue.py`) de la session du lead ; rapports dans `monke_island/Saved/T259/`.
- `setup_pirate_weapons.py` réécrit `FireSound`, `EventSounds` et `ExplosionSound` avec les sons provisoires du mousquet à chaque passage : relancer `weaponsaudio` après lui.

## Champs C++ manquants (hors périmètre audio)
- Ramassage de munitions (`AAmmoPickup` / `UAmmoDefinition`) : aucun son ; `SW_Ammo_Pickup` est importé, prêt à brancher.
- Mèche de la grenade en vol (`UGrenadeRules` / `AGrenadeProjectile`) : aucun son en boucle ; `SW_Grenade_FuseLoop` (3 s, raccord sans clic) est importé, prêt à brancher.
- Arme jetée ou lâchée qui touche le sol, arme ramassée (`AInventoryPickup`) : aucun son ; la chute est pour l'instant incluse dans `SC_Firearm_DiscardRelease` (+0,3 s).

## Écoute à faire (PIE, Test Flat)
Tir de chaque arme (et à 30, 100, 250 m pour l'atténuation), rechargements complets, tir à vide, jet de l'arme vide, grenade (allumage, lancer, rebonds, explosion de près et de loin), sabre (combo, coup chargé, touche, mise à mort), galion (monter à bord) et barque (s'asseoir).
