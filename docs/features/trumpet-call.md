# Feature : Appel de trompette (capacité sur A)

- **Statut** : review (code compilé fichier par fichier ; build complet, action éditeur, tests et PIE à faire par le lead)
- **Owner** : ue-gameplay-programmer (T-333), intégration par le lead
- **Date** : 2026-10-11
- **Cahier des charges** : lot L5. Sections : §4 (joueur : endurance, nage, portage), §9 (réplication, serveur autoritaire). Aucun ID de recette du cahier ne couvre cette capacité : demande du propriétaire du 2026-10-11.
- **Chantier** : T-333. Remplace la fanfare gratuite de la touche V (emote sans recharge).

## 1. Intention de gameplay
Le joueur appuie sur **A** : la caméra passe derrière le bean, qui sort sa trompette de héraut et sonne une vraie sonnerie militaire, « Assembly » de l'US Army (domaine public). L'appel dure 10 s. Pendant ce temps, la barre d'endurance remonte jusqu'au plein et le bean peut continuer de marcher, courir ou porter. Les autres joueurs voient la trompette et entendent la sonnerie de loin. Ensuite, la capacité se recharge pendant une minute.

En bas à gauche, l'icône du sort ressemble à celles de League of Legends. On y voit la trompette sur fond pourpre, un cadre doré et la touche **A** dans un petit cartouche. Pendant l'appel, un contour clair se vide. Pendant la recharge, un voile sombre tourne dans le sens horaire avec les secondes restantes au centre. Un éclair signale que le sort est prêt. L'icône est assombrie quand l'appel est impossible (dans l'eau, assis dans la barque, à terre).

## 2. Règles et valeurs réglables
Toutes les valeurs sont dans `DA_TrumpetEmoteRules` (`UTrumpetEmoteRules`).

| Paramètre | Valeur | Champ |
|---|---|---|
| Touche | A (clavier AZERTY du propriétaire ; ZQSD déplacent) | `CallKey`, mappée à `CallAction` (IA_TrumpetCall) dans `MappingContext` (IMC_TrumpetEmote), priorité `MappingPriority` = 1 |
| Durée totale de l'appel | 10 s : sortie 0,792 s, boucle de jeu 8,416 s, rangement 0,792 s | `CallSeconds`, `EquipSeconds`, `UnequipSeconds` |
| Recharge | 60 s, comptées depuis le début de l'appel (il reste 50 s quand la trompette est rangée) | `CallCooldown` |
| Endurance rendue | une barre pleine (1 × `MaxStamina`), répartie sur toute la boucle de jeu (environ 11,9/s), en plus de la récupération normale du bean | `CallStaminaFraction`, `CallStaminaSeconds` (0 = toute la boucle) |
| Sonnerie | `SW_Trumpet_Call`, 8,4 s, jouée une fois au début de la boucle | `CallSound` |
| Fondu de la sonnerie si l'appel est rompu | 0,3 s | `CallSoundFadeSeconds` |
| Portée du son | celle de la trompette (`ATT_Trumpet`, 60 m) | `SoundAttenuation` |
| Icône | `T_Icon_TrumpetCall` | `CallIcon` |
| Limite de requêtes (appel et cérémonie) | 4 d'un coup, puis 2 par seconde | `RequestBurst`, `RequestsPerSecond` |

**Conditions de départ** : comme l'ancienne emote et la cérémonie. Le bean doit être vivant, à pied, hors de l'eau, pas en ragdoll, pas assis dans la barque. Rien d'autre ne doit jouer de la trompette et la recharge doit être finie.

**Pendant l'appel** :
- l'objet en main est rangé, puis repris à la fin (la trompette se tient à deux mains) ;
- la vue passe en troisième personne, comme pendant la cérémonie, puis revient quand la trompette se range ;
- **la marche reste libre** : on peut marcher, courir, sauter et s'accroupir.

Choix de la marche libre : immobile, le bean récupère déjà 20 points d'endurance par seconde. Sa barre se remplit en 6 s environ, sans trompette. Un appel qui le figerait n'apporterait donc rien. En marchant, en courant ou en portant une charge (qui bloque la récupération), l'appel change vraiment la donne. La cérémonie, elle, garde son immobilité : elle en a besoin.

**Ce qui rompt l'appel** (le serveur vérifie à chaque image de l'appel) : le bean passe à terre (roulade X, coup qui assomme), meurt, entre dans l'eau, s'assoit dans la barque ou prend un objet en main (touche 1 à 6, ramassage qui équipe). La trompette se range et la sonnerie s'éteint en fondu. L'endurance cesse de remonter. La recharge, elle, continue.

Une cérémonie du drapeau lancée avec F remplace l'appel en cours. Un coup reçu qui n'assomme pas ne rompt pas l'appel.

## 3. Critères d'acceptation
- [ ] En solo, A lance l'appel. La vue passe derrière le bean, la trompette sort et la sonnerie « Assembly » joue environ 8,4 s. La trompette se range 10 s après l'appui. L'objet qui était en main revient.
- [ ] Endurance vidée en courant, puis A : la barre remonte pendant l'appel et atteint le plein en marchant ou à l'arrêt.
- [ ] Pendant l'appel, ZQSD déplacent le bean, Shift le fait courir, Espace sauter.
- [ ] Pendant la recharge, A ne fait rien. L'icône montre le voile qui tourne et les secondes (50 à la fin de l'appel, puis jusqu'à 0). Un éclair signale qu'elle est prête, 60 s après l'appui.
- [ ] Dans l'eau ou assis dans la barque, l'icône est assombrie et A ne fait rien.
- [ ] Une touche d'objet ou une roulade pendant l'appel range la trompette et éteint la sonnerie.
- [ ] V ne lance plus de fanfare (il ne sert plus qu'à la vue et au micro).
- [ ] En hôte + client : le client lance l'appel et l'hôte voit et entend la trompette, et réciproquement. L'endurance du client remonte sans à-coups ni corrections de mouvement visibles. L'icône du client affiche sa propre recharge.
- [ ] L'icône est cachée pendant les scènes d'arrivée et de départ, une fois mort, et sans pion.

## 4. Design technique
- **`UTrumpetEmoteComponent`** (`Source/monke_island/Emotes/`), sur `BP_IslanderBean`.
  - Le client propriétaire appelle `RequestCall`. Côté serveur, `ServerRequestCall` passe la limite de requêtes, puis vérifie l'état, la recharge et qu'aucune autre partie de trompette ne joue.
  - L'état répliqué tient en une seule propriété, `FTrumpetPlay PlayState` (identifiant, nature, longueur de boucle, heure serveur de départ, instant de coupure). Chaque machine joue la partie calée sur l'heure serveur. Un pion qui devient pertinent en cours de route rejoint la partie là où elle en est, ou l'ignore si elle est finie.
  - `CallStartTime` (heure serveur du dernier appel) n'est répliquée qu'au propriétaire (`COND_OwnerOnly`) : elle sert à la recharge et à l'icône.
- **Endurance** : elle est simulée à la fois par le serveur et par le client propriétaire (`UIslanderMovementComponent::TickComponent`). L'appel la rend donc des deux côtés avec la même fonction cumulée, `TrumpetCallMath::StaminaGiven`, appliquée via `RestoreStamina`. Le total rendu est identique sur les deux machines ; seul un décalage de l'ordre de la demi-latence subsiste.
- **Cérémonie** : `StartRitual` / `StopRitual` et `IsRitualLocked` restent inchangés pour `AFlagCeremonySite` et `AIslanderCharacter`. La fanfare V et `PlayFanfare` sont supprimées.
- **HUD** : `STapaoSpellSlot` (Slate, `UI/Tapao/`) est un emplacement de sort générique. Il dessine le voile avec des sommets personnalisés et le contour avec des lignes. `STapaoHUD` le place à droite des jauges, en bas à gauche (46 px, à 62 px du bas). Il le lit sur le pion local, rafraîchi deux fois par seconde.
- **Logique pure** : `Emotes/TrumpetCallMath.h` (durée, recharge, endurance) et `UI/Tapao/TapaoSpellMath.h` (balayage du carré).
- **Éditeur** : `scripts/editor/trumpet_call_ue.py` (action « trumpetcall »). Il importe la sonnerie et l'icône, crée IA_TrumpetCall, mappe A dans IMC_TrumpetEmote à la place de V et renseigne le DataAsset.
- **Assets** : la sonnerie vient de `scripts/audio/trumpet_call_prepare.py`, l'icône de `scripts/ui/trumpet_call_icon.py`. Les crédits sont dans `art/SOURCES.md`.
- **Conflit de touche** : `IMC_Default` mappe A à `IA_Move` (gauche, pour un clavier QWERTY). IA_TrumpetCall consomme sa touche et son contexte a une priorité plus haute (1 contre 0), donc A ne fait plus que l'appel. Un joueur QWERTY garde Q (`IMC_Islander_AZERTY`) et les flèches pour aller à gauche. Un mappage selon la disposition du clavier reste à arbitrer.

## 5. Tâches
| # | Tâche | Agent | Statut |
|---|-------|-------|--------|
| T-333 | Capacité, sonnerie, icône, HUD, tests, script éditeur | ue-gameplay-programmer | review |
| — | Build complet éditeur fermé, action « trumpetcall », tests, PIE solo puis hôte + client | lead | à faire |

## 6. Tests
- Automation spec `monke_island.Emotes.TrumpetCall` (`Emotes/Tests/TrumpetCallSpec.cpp`) : valeurs du DataAsset, durée, recharge et refus pendant la recharge, endurance rendue (totale, à mi-boucle, sur une durée plus courte, appel rompu), balayage de l'icône.
- Vérifications à faire en PIE, en solo puis en hôte + client : voir les critères du §3.
- Niveau de preuve au 2026-10-11 : **compilé** (fichiers seuls avec `-SingleFile`, sans édition de liens). Ni le build complet, ni les tests, ni l'éditeur, ni le réseau n'ont encore été passés.

## 7. Journal
- 2026-10-11 : demande du propriétaire (« A : emote trompette, sonnerie d'armée libre de droits, rend l'endurance, 10 s, recharge 1 min, icône en bas à gauche comme dans LoL »). Code, sonnerie « Assembly » (U.S. Army, PD-USGov-Military), icône, script éditeur, spec et tests écrits (T-333).
