# Correspondance des SHA (réécriture du 2026-10-12)

L'historique a été réécrit le 12/10/2026 avant l'envoi sur GitHub : auteur unique Matthieu Barraque, sans ligne de co-auteur ni de session Claude.
Le contenu des commits est identique ; seuls les identifiants ont changé. Les documents du chantier citent les anciens SHA courts : cette table donne le nouveau.

| Date | Ancien | Nouveau | Sujet |
|---|---|---|---|
| 2026-10-03 05:25 | `47b1bdb` | `5afd4bc` | build: dépôt initial monke_island (état du 2026-10-03) |
| 2026-10-03 05:26 | `63fe8c6` | `5a96bf4` | build: outil worktree de l'usine (dev/T-NNN, sparse code + docs) |
| 2026-10-03 05:27 | `3dfca17` | `8f9c3dc` | build: worktree merge ne bloque que sur les fichiers modifiés des deux côtés |
| 2026-10-03 05:27 | `98b6c15` | `967aca8` | build: usine en mode worktree (FACTORY, /factory, /factory-gate, /ue-vcs) |
| 2026-10-03 05:30 | `40a2d45` | `0fa80b1` | build: worktree build compile la cible jeu par défaut (verrou Live Coding) |
| 2026-10-03 05:30 | `462aeed` | `df813c1` | build: FACTORY — compilation DEV en cible jeu tant que l'éditeur est ouvert (mutex Live Coding) |
| 2026-10-03 05:30 | `4f9a3e0` | `bd5e8b4` | gameplay: grille spatiale des buissons pour les tiki (T-051) |
| 2026-10-03 05:31 | `3435c1c` | `f0937a3` | build: verrous d'assets par owner= au chantier (git lfs lock impossible sans remote) |
| 2026-10-03 05:31 | `6225d1f` | `0fbcff6` | fix: cible jeu — accesseur de lumière hors éditeur, tag de vent en double (unity) |
| 2026-10-03 05:32 | `eed0466` | `6f377de` | build: .uasset/.umap non verrouillables (LFS les mettait en lecture seule, l'éditeur ne sauvegardait plus) |
| 2026-10-03 05:33 | `3efc971` | `8fa5311` | merge: T-051 dans main (fin de REC) |
| 2026-10-03 05:35 | `18336b8` | `f5fd356` | build: worktree réutilise les règles UBT autorisées de l'arbre principal |
| 2026-10-03 05:36 | `c0241cc` | `c150979` | systems: bilan économique d'expédition (T-056) |
| 2026-10-03 05:38 | `ab5b209` | `0890a99` | gameplay: requin V1 du cahier §8 (T-058) |
| 2026-10-03 05:39 | `01f3481` | `8b98ede` | art: requin à pointes noires procédural, corps + queue (T-058) |
| 2026-10-03 05:39 | `2a17de9` | `7e56d2b` | merge: T-058 dans main (fin de REC) |
| 2026-10-03 05:39 | `962deb8` | `120ba7e` | art: sorties du requin (blend, FBX, aperçu) (T-058) |
| 2026-10-03 05:40 | `d95949a` | `99211cc` | gameplay: zones pacifiables et sermon (T-054) |
| 2026-10-03 05:41 | `efcde0e` | `cb40aa4` | gameplay: arrimage de la cargaison (T-055) |
| 2026-10-03 05:42 | `f96c7e8` | `c510734` | gameplay: horloge d'expédition (T-053) |
| 2026-10-03 05:43 | `86771b1` | `f62bcf5` | content: requin (BP_Shark, DA_SharkRules, MI, meshes) placé dans le lagon ; weapon_socket des tiki (T-058, T-046) |
| 2026-10-03 05:44 | `23c2d44` | `0baca56` | docs: fiche requin (T-058) |
| 2026-10-03 05:46 | `3400d1c` | `7d82131` | systems: sous-système de sauvegarde (T-057) |
| 2026-10-03 05:50 | `f9962ed` | `9ceece8` | systems: corrections de recette (T-056) |
| 2026-10-03 05:51 | `8af02a4` | `6f9ae6b` | gameplay: corrections de recette (T-053) |
| 2026-10-03 05:53 | `01ecf50` | `07c4e82` | gameplay: corrections de recette (T-055) |
| 2026-10-03 05:53 | `8e7d358` | `6874be9` | gameplay: porter une charge fatigue même en marchant (T-061) |
| 2026-10-03 05:54 | `45ae1ad` | `124faa0` | merge: T-061 dans main (fin de REC) |
| 2026-10-03 05:55 | `d1a9035` | `d9a4781` | gameplay: corrections de recette (T-054) |
| 2026-10-03 05:56 | `1a8e1b1` | `7f8d872` | systems: corrections de recette (T-057) |
| 2026-10-03 05:56 | `8ebffaf` | `927dbce` | art: statuette Relique des Marées (T-060) |
| 2026-10-03 05:57 | `1736626` | `2f3accf` | merge: T-056 dans main (fin de REC) |
| 2026-10-03 05:57 | `381db99` | `639f734` | merge: T-053 dans main (fin de REC) |
| 2026-10-03 05:59 | `8d3ea07` | `ee2cec4` | content: Relique des Marées (SM_TidalRelic, MI_Relic_*, BP_TidalRelic 10 kg) posée près du départ (T-061, T-060) |
| 2026-10-03 06:11 | `5281260` | `57e9f15` | gameplay: vent beaucoup plus faible, au sol seulement ; noix des seuls cocotiers à couronne (T-049, T-050) |
| 2026-10-03 06:14 | `78432f1` | `6f0f512` | fix: barque et ragdoll éjectés au moindre contact (T-063) |
| 2026-10-03 07:21 | `d24cda9` | `757afe4` | gameplay: rythme jour/nuit, compteur de jours, coups de vent aléatoires, escalade jusqu'au jour 3 et abri (T-067) |
| 2026-10-03 07:22 | `147a80b` | `15f271f` | merge: T-067 dans main (fin de REC) |
| 2026-10-03 07:26 | `d595f0e` | `83a010d` | content: DA_DayNightRules (jour 12 min / nuit 6 min) et DA_WindRules (ultra 4, +0,35 par niveau) (T-067) |
| 2026-10-03 19:14 | `d1976a0` | `94e6c91` | content: barque plus rapide à la rame, 2,6 -> 4,5 m/s à vide (DA_BoatRules : RowForceN 5000, MaxRowSpeed 450, ForwardQuadDrag 0,6, TurnTorqu |
| 2026-10-03 19:44 | `f36b73a` | `d3b12ea` | audio: bruits de pas par sol (sable sec/mouillé, eau, litière, corail, pierre, bois) pour joueurs et créatures + chute des noix sur le sable |
| 2026-10-03 19:53 | `6aeeb58` | `aef37e3` | ui: menu des réglages (cahier §18) — Audio (6 volumes par SoundClass + mix), Affichage, Souris, Confort ; Appliquer / Annuler / Restaurer ;  |
| 2026-10-03 19:54 | `bd785e7` | `88f4b43` | audio: les pas et les noix réimportés gardent la SoundClass Effets (T-073) |
| 2026-10-03 20:14 | `03bac90` | `8ec270d` | merge: T-073 dans main (fin de REC) |
| 2026-10-03 20:23 | `0adf6d2` | `f7bd1d6` | ui: menu des réglages intégré — SlateCore lié, SoundClass par canal (48 sons en Effets, 2 en Ambiance), SM_UserVolumes, DA_SettingsRules (T- |
| 2026-10-03 20:24 | `124460f` | `f9568ba` | audio: pas et chocs de noix dans la SoundClass Effets (T-073) |
| 2026-10-03 22:12 | `2a03c50` | `b2a6513` | docs: fragments des Tuamotu — ADR-004, spec, aperçu des candidats (T-081) |
| 2026-10-03 22:17 | `1ae6792` | `f1efde1` | narrative: noms et identités des 8 fragments, String Table ST_Islands (T-092) |
| 2026-10-03 22:24 | `b778346` | `74f8559` | gameplay: îles d'expédition — définitions de fragments, catalogue, subsystem et Asset Manager (T-087) |
| 2026-10-03 22:28 | `7bc54d6` | `c55ef76` | gameplay: île aléatoire au lancement — tirage seedé sans répétition, voyage ?Island=, carte de boot (T-094) |
| 2026-10-03 22:32 | `9636dcb` | `86c24f7` | boats: départ en barque au point d'arrivée du fragment (T-093) |
| 2026-10-03 22:40 | `11fb437` | `299e09d` | ui: compteur avant inondation du HUD — temps serveur, évacuation, +2:00, alertes uniques 60/30/10 s (T-097) |
| 2026-10-03 22:49 | `bec8ae0` | `dec93e8` | map: fragments de Raraka coupes par la mer, memoire FRG_Raraka_NN, arrivee de barque et controle (T-082) |
| 2026-10-03 22:50 | `4373ec7` | `96efcad` | Merge branch 'dev/T-093' into dev/T-098 |
| 2026-10-03 22:50 | `70d1430` | `f6e6b70` | Merge branch 'dev/T-087' into dev/T-098 |
| 2026-10-03 22:50 | `8121d33` | `ac63696` | Merge branch 'dev/T-097' into dev/T-098 |
| 2026-10-03 22:50 | `83d86e0` | `764816c` | Merge branch 'dev/T-092' into dev/T-098 |
| 2026-10-03 22:50 | `8e8ad6b` | `b1ffc39` | Merge branch 'dev/T-082' into dev/T-098 |
| 2026-10-03 23:01 | `db8df34` | `9784fbf` | gameplay: une seule horloge pour l'eau et la tempête finale (T-098) |
| 2026-10-03 23:01 | `e8bc8d3` | `1b6ff1d` | villages: villages japonais de Raraka — minka rurale, kit, générateur par chunks avec mémoire, PCG, PNJ, routine, lumières, ambiance, portes |
| 2026-10-03 23:03 | `dbaa430` | `e172dd4` | tiki: villages, alarme, cadavres, découpe, embuscades, totems, corps mous, voix, fondus, villageois cachés chez eux (T-064, T-072, T-077) |
| 2026-10-03 23:03 | `e884b4d` | `91ad341` | merge: fragments des Tuamotu dans main — îles + tirage (T-087, T-094), départ en barque (T-093), compteur d'inondation (T-097), fragments (T |
| 2026-10-03 23:05 | `641605f` | `2ad91c6` | tiki: IsInBoat renommé IsRagdollInBoat dans TikiRagdollCorpse (doublon en build unity avec TikiCorpse) |
| 2026-10-03 23:20 | `6162f11` | `a34faa4` | gameplay: barque plus rapide (force 8000, vitesse max 750, traînée 0,4) |
| 2026-10-03 23:38 | `da32e04` | `47587aa` | expedition: départ en barque sur une île aléatoire — L_Boot, catalogue de 20 fragments, points d'arrivée, horloge d'expédition, compteur (T- |
| 2026-10-03 23:39 | `c5fe989` | `aef413c` | gameplay: barque encore plus rapide (force 14000, vitesse max 1100, traînée 0,3) |
| 2026-10-03 23:53 | `6e56a0f` | `0507bf5` | perf: le vent ne pousse que les objets physiques proches des joueurs, liste rafraîchie 1 fois/s, nettoyage du cache d'abri hors de la boucle |
| 2026-10-03 23:53 | `fb62dd9` | `c0404b9` | merge: T-100 dans main (fin de REC) |
| 2026-10-03 23:56 | `7129489` | `f8dae1e` | gameplay: vent moins fort (rafales 0,3 +0,15 par palier, tempête finale 1,8, poussée réduite) (T-100) |
| 2026-10-04 00:08 | `4de3c9f` | `b3b2c4f` | gameplay: la mer continue de monter après la submersion jusqu'à la noyade (10 cm/s, plafond +15 m), message barque, écart E-07 (T-101) |
| 2026-10-04 00:08 | `652e5bb` | `d22fda9` | gameplay: 5 min avant l'inondation (E-07, T-101) |
| 2026-10-04 00:08 | `938c3bf` | `b12da73` | merge: T-101 dans main (fin de REC) |
| 2026-10-04 00:26 | `5f1f5b5` | `3093378` | gameplay: mort définitive en expédition, équipe morte = fin de partie puis retour à L_Boot (nouveau tirage, départ en barque) (T-104) |
| 2026-10-04 00:26 | `650ad67` | `f9663c4` | merge: T-104 dans main (fin de REC) |
| 2026-10-04 00:33 | `bf2739f` | `9351573` | gameplay: l'eau monte uniquement sur le fragment joué — grille locale recentrée sur l'arrivée de la barque (2,56 km), parcelles noyées en un |
| 2026-10-04 01:03 | `4a2ebc9` | `8f7d6d9` | gameplay: la fin de partie renvoie à L_Boot (RestartMap), pas au menu principal (T-104) |
| 2026-10-04 01:42 | `a4f3a71` | `b26c29b` | docs: cahier des charges v1.3 — partie en manches autour du galion (§3, §24), ADR-005 acceptée |
| 2026-10-04 01:42 | `cc76f20` | `892566f` | villages + tiki: villages des fragments joués (T-102), armes lâchées par les mobs (T-105), barres de vie masquées |
| 2026-10-04 01:50 | `dcf9d60` | `1709ba6` | gameplay: retour automatique après la fin de partie à 30 s, pour laisser servir l'écran NAUFRAGÉ (T-104) |
| 2026-10-04 02:19 | `dc2ed0a` | `f900583` | map: horizon des fragments — silhouettes sable + canopée du reste de l'atoll, build_horizon.py, recette M_Horizon_Atoll (T-084) |
| 2026-10-04 02:28 | `be16042` | `9d09472` | map: terrain des fragments de Raraka (build_fragment.py), hoa synthétiques, marge en pente douce, mouillage du galion et contrôles (T-083) |
| 2026-10-04 02:29 | `255d717` | `90fa0e9` | Merge branch 'dev/T-084' |
| 2026-10-04 02:29 | `a1a49f5` | `8f99139` | Merge branch 'dev/T-083' |
| 2026-10-04 02:34 | `1a8c8e0` | `0d62671` | gameplay: armes du joueur — sabre (slot 2, sons, prise en main), torche (slot 3, visible seulement en main), mousquet ; inventaire refusé as |
| 2026-10-04 02:34 | `31dc03a` | `1162add` | anim: bibliothèque d'animations du bean (1re et 3e personne, rame, torche, sabre, trompette) et des mobs (T-078, T-079) |
| 2026-10-04 02:35 | `a32a7d9` | `528b49a` | monde: campements de pêcheurs, Marae des Perles et butin commun par fragment — 8 camps et 2 marae par zone, butin tous les 10 m, armes des m |
| 2026-10-04 02:36 | `306ab9a` | `9a6613a` | barque: rameur seul assis au centre du banc, mains et rames synchronisées, objets arrimés à la coque une fois posés, pas de barque dans le m |
| 2026-10-04 02:36 | `547f07d` | `324cb2e` | emote: trompette de héraut sur la touche V (fanfare synthétisée, 4 clips) |
| 2026-10-04 02:36 | `c3ac3f6` | `ec72957` | monde: heure de départ aléatoire entre 6 h 30 et 18 h (DA_DayNightRules) |
| 2026-10-04 02:37 | `48947f6` | `e13231d` | galion: AGalleonActor cinématique ancré au large de l'arrivée, équipage sur le pont, barque amarrée, échelle de coupée, suit la montée local |
| 2026-10-04 02:37 | `9f67999` | `ca88699` | ui: interface TAPAO — écran-titre, menu principal, confirmations, chargement, pause, Naufragé / Île conquise, sauvegarde de campagne, HUD jo |
| 2026-10-04 02:37 | `be0ba2d` | `fc97a3f` | map: Lvl_Raraka — spawners des campements, marae et galion ; chantier à jour |
| 2026-10-04 02:43 | `740eaaf` | `2f47c60` | fix: mousquet — variables locales renommées (masquage de globales en build unity) |
| 2026-10-04 02:53 | `99687d2` | `2fc08d0` | interaction: objet tenu fantôme contre les corps physiques, cible bornée devant les obstacles lourds (barque), visée basse limitée (T-065, T |
| 2026-10-04 03:00 | `9926d4e` | `4ae795f` | merge: T-118 dans main (fin de REC) |
| 2026-10-04 03:00 | `d7c644c` | `70dfb5c` | gameplay: la barque ne se saisit plus (tag NoGrab) ; une noix qui tombe traverse la coque au lieu de pousser la barque (T-118) |
| 2026-10-04 04:10 | `9a80b28` | `a1f7982` | boat: exclude hull from storm wind forces (T-122) |
| 2026-10-04 04:36 | `871027d` | `6b20df1` | ui: restore mouse capture when island loading ends (T-124) |
| 2026-10-04 04:37 | `807b3e0` | `1b5e2f2` | galion: zone sans vent autour du navire (WindCalmRadius, sphère WindCalmZone lue par AWindStormActor) |
| 2026-10-04 04:38 | `eb02418` | `2c78d32` | test: use a focusable pause widget in input regression (T-124) |
| 2026-10-04 04:40 | `8508a75` | `222781b` | vent: zones calmes WindCalmZone (galion, par 19) et acteurs WindSheltered non poussés (barques) dans AWindStormActor |
| 2026-10-04 04:42 | `86f3f50` | `edd8e5b` | galion: mouillage lu dans les données de l'île (rôle Galleon, repli sur la recherche), lampe de cabine ; fiche à jour (T-110, T-111) |
| 2026-10-04 04:47 | `9319578` | `ed0ebc0` | fix: keep dropped hats until explicit F interaction |
| 2026-10-04 05:12 | `fbde5da` | `8c7727f` | fragments: première île jouable L_Frag_Raraka_01 — terrain Nanite (14 tuiles), eau, horizon, végétation filtrée, LI_ExpeditionCore, mouillag |
| 2026-10-04 05:16 | `f57c615` | `0625e55` | galion: catalogue des îles chargé en lecture directe d'une carte, recherche de repli côté large ; capture du mouillage et de la cabine éclai |
| 2026-10-04 05:19 | `383d923` | `07e2226` | outils: verrou de l'éditeur partagé entre sessions Claude et Codex (scripts/editor_lock.py, vérifié par ue.ps1 build/test/editor/clean/packa |
| 2026-10-04 05:28 | `1fc50e9` | `17a93d3` | integration: fusion des zones et du sermon T-054 (T-131) |
| 2026-10-04 05:28 | `77def72` | `565d681` | integration: fusion de l'arrimage T-055 (T-131) |
| 2026-10-04 05:28 | `afaa860` | `1e415d2` | integration: fusion de la sauvegarde T-057 (T-131) |
| 2026-10-04 05:33 | `6c12f05` | `84e8358` | gameplay: mémorise les îles réellement jouées (T-128) |
| 2026-10-04 05:35 | `2702853` | `fa3b836` | docs: ADR-006 — un jour par île, quota tous les 5 jours, 10 périodes de difficulté croissante, record au menu (demande du propriétaire) |
| 2026-10-04 05:37 | `eea515e` | `7d85767` | expedition: rôle de mouillage Galleon (ADR-005, mouillage du galion lu par AGalleonSpawner) |
| 2026-10-04 05:43 | `22f95f4` | `16ced25` | integration: rétablir les prérequis de compilation du tronc (T-131) |
| 2026-10-04 05:43 | `31b747a` | `d1d45bb` | mesure: structure les morts et les journaux locaux (T-129) |
| 2026-10-04 05:43 | `9a26fc3` | `3a1d48b` | gameplay: raccorder la pacification à l'horloge v1.4 et couvrir ses refus (T-131) |
| 2026-10-04 05:43 | `eb1d46c` | `97e03bf` | integration: reprendre le mouillage Galleon versionné pendant T-131 |
| 2026-10-04 05:44 | `5f738fc` | `3589835` | merge: T-142 dans main (fin de REC) |
| 2026-10-04 05:44 | `8ef0950` | `778d291` | gameplay: fin du déluge (ADR-006) — mort des joueurs hors du galion, survivants vers l'île suivante, équipage mort = défaite (T-142) |
| 2026-10-04 05:44 | `9fdc09f` | `6d941a2` | interaction: l'invite de saisie ignore aussi les acteurs NoGrab (complément de T-118, hunk écrit par une autre session) |
| 2026-10-04 05:48 | `dd89dea` | `d92d98d` | monde: beaucoup plus de marae — 8 par fragment, emplacements tous les 45 m (1468 sur l'atoll, 11 à 135 par fragment), écart minimal 50 m |
| 2026-10-04 05:50 | `33e1740` | `57f77ad` | docs: prépare les brouillons légaux et les preuves de licences (T-130) |
| 2026-10-04 05:51 | `a018676` | `a4ce36e` | tests: corrige la spec et consigne le blocage de compilation (T-129) |
| 2026-10-04 05:51 | `cc121a6` | `2392b25` | docs: archiver les preuves et la reprise d'intégration T-131 |
| 2026-10-04 05:52 | `a9ec0eb` | `a292580` | docs: préciser la réservation éditeur suivant T-131 |
| 2026-10-04 05:53 | `7eddd45` | `9add1b7` | ui: TAPAO — ligne « Jour N / Période P » au chargement, « ÎLE CONQUISE ! » pour les survivants du jour (ADR-006) ; rideau gardé jusqu'au con |
| 2026-10-04 05:53 | `eeedf46` | `654ad4e` | outils: ue.ps1 — appels au verrou sans NativeCommandError en PowerShell 5.1 (signalé par aa) |
| 2026-10-04 05:55 | `d0a72ef` | `000f63b` | config: aucun message de debug à l'écran en jeu (bEnableOnScreenDebugMessages=False), demande du propriétaire |
| 2026-10-04 06:00 | `29d8239` | `7c2f549` | exploitation: versionne les builds et prépare les symboles et crashs (T-127) |
| 2026-10-04 06:00 | `eac5a42` | `3ff117e` | docs: précise les raccords de recette des causes de mort (T-129) |
| 2026-10-04 06:01 | `1158bf7` | `30297ae` | docs: transmet les résultats de la mission Codex 78 |
| 2026-10-04 06:08 | `66106a8` | `b0580ba` | gameplay: corrections de recette T-141 — mer montée autour du fragment sans marche au bord de la grille (fondu, spawn sec), une seule simula |
| 2026-10-04 06:08 | `fce5a9f` | `5a51173` | monde: 2 marae garantis près de l'arrivée (30 à 400 m des joueurs), 12 par fragment par défaut, dégagement de 14 m ; plus de palmiers ni de  |
| 2026-10-04 06:11 | `3ae878d` | `cf7304f` | gameplay: expédition de 10 min — déluge à 9 min (InitialFloodDelay 540 s, réglé par 78, T-126) ; valeurs T-104 / T-142 inchangées |
| 2026-10-04 06:11 | `af8a5d8` | `9e45761` | chantier: prochaine étape des fragments (T-091 mesure, fragments 02 à 20) |
| 2026-10-04 06:11 | `e3faa8a` | `0627bbe` | package: titre « Taopao : Rush Island », maps et dossiers à cuire (dont L_Frag_Raraka_01, section T-120), ue.ps1 package sans recompiler l'é |
| 2026-10-04 06:12 | `ba93f53` | `2771fe8` | marae: palmiers et buissons retirés du modèle, 12 marae par fragment, dégagement 14 m (T-144) ; chantier de fin de journée |
| 2026-10-04 06:12 | `c9b03a4` | `6d9a38b` | chantier: état de fin de journée de la session 19 (prochaines étapes par tâche) |
| 2026-10-04 12:51 | `fb06c7a` | `5d0df2b` | finitions: places debout de la barque hors de la vue du rameur, sons du compteur d'inondation générés par script (alerte, évacuation, zone s |
| 2026-10-06 12:08 | `2cf9598` | `c88d0d6` | arena: AArenaZone — les mobs hostiles qui commencent dans le cercle n'en sortent jamais (replacés au bord par le serveur, 10×/s), joueur et  |
| 2026-10-12 03:22 | `0b0d57a` | `4ca3f5d` | campagne: dix jours, contrats et cupidité, horloge d'expédition, mort, spectateur et exécution |
| 2026-10-12 03:22 | `1ec8fd3` | `086d382` | usine: agents, skills, chantier, ADR, audits et recettes (travail du 06/10 au 12/10) |
| 2026-10-12 03:22 | `7cfb80b` | `ab71853` | socle: config du projet, module C++ (GameMode, contrôleur, tests, outils) et contenu du template |
| 2026-10-12 03:23 | `2690d0b` | `524cd7f` | objets: butin et valeur, caisse de transport, inventaire, capture (cage, menottes), bandages, boutique du marchand |
| 2026-10-12 03:23 | `42c435d` | `387f8d6` | galion: mouillage, échelle, départ à la barre et vote ; barque, places et charge |
| 2026-10-12 03:23 | `45818d1` | `5302c08` | joueur: bean soldat, mouvement et endurance, saisie physique, furtivité, voix de proximité, emotes |
| 2026-10-12 03:23 | `8320733` | `3754049` | armes: mousquet v2, pistolets, fusil à pompe, tromblon, carabine, fusil à verrou, mitraillette, mitrailleuse, baïonnette, grenades, sabre |
| 2026-10-12 03:23 | `e5fd710` | `710c303` | carte: Lvl_Raraka, fragments FRG_Raraka_01 à 20, données de référence et scripts carto |
| 2026-10-12 03:23 | `f7ed636` | `0416bcf` | monde: villages japonais, cérémonie du drapeau, marae, temples, forts, camps, jour et nuit, vent, tempête et déluge |
| 2026-10-12 03:23 | `f9fd825` | `7091aff` | créatures: Veilleurs façon Outlast et tactiques, villageois, crabe géant v2, garnison française, requin |
| 2026-10-12 03:24 | `376b395` | `6fa9537` | docs: specs des features, bible et noms d'îles |
| 2026-10-12 03:24 | `e8e25bf` | `7d4878e` | art: sources Blender, exports, aperçus, textures et HDRI CC0, planches de référence |
| 2026-10-12 03:24 | `eb671a7` | `0605c39` | interface: UI TAPAO (menus, HUD, fin de jour), musique, sons, réglages |
