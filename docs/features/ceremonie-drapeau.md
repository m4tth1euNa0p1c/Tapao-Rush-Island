# Feature : Cérémonie du drapeau et villageois des maisons

- **Statut** : in-progress (code compilé en cible jeu, specs écrites ; build éditeur, données et PIE à faire)
- **Owner** : lead
- **Date** : 2026-10-07
- **Cahier des charges** : lot L3. Sections : §6.2 « Pacifier une zone » (drapeau, rituel immobile, interruption, bonus) et §6.1 (bonus de zone une fois). La recette n'est pas encore passée.
- **Chantier** : T-194 (cérémonie), T-195 (villageois des maisons). Remplace T-191 (cercle rouge).

## 1. Intention de gameplay
Au centre de chaque village, un petit tas de pierres. Debout près de lui, le joueur appuie sur **F**. Le mât sort des pierres et le drapeau britannique y monte pendant une minute, au son de sa trompette. Pendant ce temps, les villageois quittent leur routine et viennent former un cercle autour de lui. Ceux des maisons apportent la Tidal Relic de leur maison. Quand le drapeau est en haut, ils déposent leur relique autour du joueur, l'acclament et rentrent chez eux. Le village est pacifié et la porte du sanctuaire s'ouvre sur son artefact légendaire.

Chaque maison a un habitant qui vit sa routine à l'intérieur. Si le joueur entre, il le suit et le surveille de loin. Si le joueur le frappe ou sort un objet de la maison, il crie, panique et s'enfuit.

## 2. Règles et valeurs réglables
| Paramètre | Valeur | Où |
|---|---|---|
| Durée de la montée | 60 s (demande ; le cahier §6.2 dit 20 s : écart à arbitrer dans l'ADR-001) | `DA_FlagCeremonyRules.RitualSeconds` |
| Zone où F lance la cérémonie | rayon 4,5 m, ± 3 m en hauteur | `Radius`, `HalfHeight` |
| Immobilité de l'officiant | 10 cm (cahier §6.2) | `MoveTolerance` |
| Un coup reçu interrompt | oui (cahier) | `bInterruptOnDamage` |
| Bonus de zone à la fin | oui, une fois par zone (`AExpeditionGameState::AddZoneBonus`) | `bGrantZoneBonus` |
| Ouverture des maisons scellées (sanctuaire) | oui | `bOpenSealedHouses` + `DA_ShrineKit.bSealedUntilFlag` |
| Cercle des villageois / anneau des reliques | 3,8 m / 1,5 m autour de l'officiant | `GatherRadius`, `OfferRadius` |
| Écart au tas de pierres | 1,2 m | `CairnClearance` |
| Vitesse pour rejoindre la cérémonie | 300 cm/s | `GatherSpeed` |
| Acclamations avant de rentrer | 8 s | `CelebrateSeconds` |
| Fanfare pendant la montée | au début, puis toutes les 15 s | `DA_TrumpetEmoteRules.RitualFanfareInterval` |
| Habitants par maison | 1 (minka), 0 (sanctuaire) | `UVillageHouseKit.Residents` |
| Distance de surveillance | 1,7 m (recule sous 1,1 m) | `DA_TikiRules.WatchDistance`, `WatchMinDistance` |
| Vol : objet hors de la maison | marge 40 cm, voleur cherché à 8 m | `TheftMargin`, `ThiefSearchRadius` |
| Panique | 12 s, un cri toutes les 2,2 s | `PanicSeconds`, `ScreamInterval` |
| Relique portée | devant le villageois (38, 0, −18 cm) | `RelicCarryOffset` |

## 3. Critères d'acceptation
- [ ] Debout près du tas de pierres d'un village, le joueur voit « F Hisser les couleurs ». F lance la cérémonie.
- [ ] La trompette joue en boucle et la barre en haut de l'écran affiche le décompte de 1:00. Le drapeau monte du bas au haut du mât.
- [ ] Si le joueur bouge, reçoit un coup ou meurt, la cérémonie s'interrompt (« Cérémonie interrompue »). Le drapeau redescend et le mât reste planté. Une nouvelle cérémonie repart de zéro.
- [ ] Au bout d'une minute, le message « Village pacifié : le sanctuaire s'ouvre » s'affiche et les portes du sanctuaire s'ouvrent pour de bon.
- [ ] Les villageois du village viennent en cercle autour du joueur, en contournant l'anneau sans le traverser. Ceux des maisons tiennent leur Tidal Relic.
- [ ] À la fin, ils déposent leur relique autour du joueur, acclament, puis rentrent chez eux. Si la cérémonie est interrompue, ils rapportent leur relique à sa place.
- [ ] Chaque minka a un habitant qui va d'un point de sa routine à l'autre sans traverser de mur.
- [ ] Un joueur qui entre est suivi à environ 1,7 m, sans être poussé. Le villageois passe par les portes quand il ne le voit plus.
- [ ] Un objet de la maison sorti par le joueur, ou un coup, déclenche un cri, une panique et une fuite. L'habitant sort par la porte, ou se réfugie au fond de la maison si le joueur bloque la sortie. Il revient plus tard.
- [ ] En multijoueur, le client voit le drapeau, la barre, la trompette, les reliques portées et les portes, comme l'hôte.

## 4. Design technique
- `Ceremony/AFlagCeremonySite` est répliqué et toujours pertinent.
  - Ses composants : tas de pierres (`SM_Temple_Debris_StonePile`, collision), mât (`SM_FlagCeremony_Pole`, UCX) et drapeau (`SM_FlagCeremony_Flag`).
  - L'état répliqué (`FFlagCeremonyState`) contient la phase, l'heure serveur de départ, l'officiant, le mât planté et le compteur de fins. Chaque machine calcule la hauteur du drapeau avec l'horloge serveur.
  - Le serveur valide le départ (zone, vivant, debout, hors de l'eau) et vérifie l'officiant toutes les 0,1 s.
  - La touche : `IMC_FlagCeremony` (F → `IA_FlagCeremony`, priorité 10) est ajoutée seulement quand la cérémonie peut partir. La requête passe par `UTrumpetEmoteComponent::ServerRequestCeremony`, un composant possédé par le pion.
- `UTrumpetEmoteComponent` ajoute une phase `Ritual`.
  - `StartRitual` / `StopRitual` côté serveur.
  - `RitualSeconds` et `RitualStopCount` sont répliqués.
  - L'animation `A_Bean_Trumpet_Play_Loop` passe par `UBeanAnimDriverComponent`.
- `AVillageSpawner`
  - pose un site sur la place de chaque village pacifiable (`DA_VillageRules.FlagSiteClass` / `FlagCeremonyRules`) ;
  - tient un registre des maisons à kit (`FVillageHouseInfo` : kit, transform, porte, portail, relique, objets) ;
  - `OpenSealedHouses` déverrouille les portes scellées. Pour cela, `UVillageDoorsComponent` réplique `UnsealedVillages`.
- `ATikiDirector`
  - fait apparaître les habitants à l'intérieur quand un joueur approche du village ;
  - vérifie les vols chaque seconde ;
  - gère le rassemblement : places attribuées dans l'ordre angulaire d'arrivée (`StartGathering`, `FinishGathering`).
- `ATikiVillager` ajoute les états `HomeWalk`, `HomeActivity`, `Watch`, `Panic`, `ReturnHome` et `Ceremony` (étapes `ETikiCeremonyStep`).
  - Ses chemins suivent les points de passage du kit (`VillageHouseNav`), puis la porte et le portail de la parcelle. Dehors, il fait le tour de l'anneau (`FlagCeremonyMath::RingApproach`).
  - `CarriedRelic` est répliqué : la relique est attachée devant lui, sans physique ni collision.
- Données des minka (`scripts/props/minka_nav.py`) : 17 points de passage dans la doma et les cinq pièces, reliés par les baies (pierre de marche, fusuma). On y trouve aussi le nœud de porte, le nœud devant le tokonoma (relique), le point devant la porte et le volume intérieur.
- Assets : `scripts/props/flag_ceremony.py` (Blender) puis `flag_ceremony_ue.py` (action `ceremony`) ; `house_nav_ue.py` (action `housenav`).
- Risques :
  - les villageois ne passent pas par un navmesh ; dehors, ils vont en ligne droite et passent les points où ils bloquent ;
  - 16 habitants par village en plus des villageois de la place ;
  - les reliques déposées restent physiques et peuvent être ramassées.

## 5. Tâches
| # | Tâche | Agent | Statut |
|---|-------|-------|--------|
| T-194 | Site de cérémonie, trompette, HUD, portes du sanctuaire, assets | lead | en-cours |
| T-195 | Habitants des maisons, surveillance, vol, rassemblement, reliques | lead | en-cours |

## 6. Tests
- Automation : `monke_island.Ceremony.Flag` (montée, zone, immobilité, anneau, approche par l'extérieur) et `monke_island.Village.HouseNav` (nœud proche, chemin le plus court, inatteignable, refuge, intérieur). Compilés en cible jeu, pas encore lancés.
- PIE à faire sur L_Test_Flat (Uo-mura) :
  - F près des pierres, montée complète, interruption en bougeant ;
  - sanctuaire ouvert ;
  - vol d'un objet de minka ;
  - coup à un habitant ;
  - puis hôte + client.

## 7. Journal
- 2026-10-07 (soir) : les hameaux entre les zones (« petits villages ») n'avaient ni tas de pierres ni drapeau : `AVillageSpawner::SpawnFlagSites` ne posait un site que sur les villages-zones (`bZone`). `DA_VillageRules.bFlagSitesInHamlets` (vrai) en pose aussi un sur la place de chaque hameau. Mêmes règles, même récompense, mais toujours pas de gardiens (`bZone` reste faux). Testé éditeur : « 6 flag sites on the squares of FRG_Raraka_01, 3 of them in hamlets », tas de pierres sur la place de F01_H1, cérémonie lancée (« 12 villagers come to the flag of F01_H1 »). Build v1.1.
- 2026-10-07 : demandes du propriétaire (cercle rouge → tas de pierres, F, drapeau sur une minute, barre de progression, un PNJ par maison, surveillance, panique, cercle autour du joueur, reliques déposées au centre, sanctuaire ouvert à la fin). C++ écrit et compilé (cible jeu) ; assets Blender du mât et du drapeau exportés (`art/export/Props/FlagCeremony/preview.png`).
