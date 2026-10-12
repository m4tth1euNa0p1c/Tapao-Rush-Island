# Feature : Capture de prisonniers (cage, menottes en acier, vente 10 000 c)

- **Statut** : review (code compilé fichier par fichier ; build complet, action éditeur « capture », tests et PIE à faire par le lead)
- **Owner** : ue-gameplay-programmer (T-336), art T-337 (ue-blender-artist), intégration par le lead
- **Date** : 2026-10-11
- **Cahier des charges** : lot L5. Sections : §4 (interaction à portée revérifiée par le serveur), §5 (objets et cargaison), §7 (monnaie commune de l'équipage), §9 (réplication, serveur autoritaire), §21–22 (contrats). Aucun ID de recette du cahier ne couvre cette mécanique : demande du propriétaire du 2026-10-11. Recette du cahier touchée : §12 n°2 (deux demandes simultanées, un seul gagnant), n°10 (réseau dégradé). IDs propres : CAP-01 à CAP-12 (§3).
- **Chantier** : T-336 (code, données, script), T-337 (cage, menottes, chaîne, clips).

## 1. Intention de gameplay
Demande du propriétaire, mot pour mot : « pouvoir poser cage pendant 2 minutes sur place, animation avec F sur plage avec nouvelle item : cage ; crée nouvelle item, menotte en acier, permettant de menotter un pnj, de le ramener dans cage et le vendre, une fois dans cage cela rapporte 10 000 c ».

Le joueur affaiblit un soldat français ou un Veilleur au combat. Avec les **menottes en acier** en main, il maintient **F** devant lui : le bean lui passe les fers, et le captif le suit au bout d'une chaîne. Sur la plage, avec la **cage** en main, **F** la pose : le bean la dépose, l'ouvre, et elle reste **2 minutes**. Le joueur amène son prisonnier devant la cage et appuie sur **F** sur la cage : la porte s'ouvre, le prisonnier entre, la porte se ferme, **+10 000 c** vont au trésor de l'équipage et tout le monde en est averti. Quelques secondes plus tard, le prisonnier est emmené.

## 2. Règles et valeurs réglables
Toutes les valeurs sont dans `DA_CaptureRules` (`UCaptureRules`, `Source/monke_island/Capture/CaptureRules.h`). Les valeurs ci-dessous sont celles du code ; le script éditeur reprend du manifeste de T-337 les durées, les points d'accroche et les tailles.

### 2.1 Qui peut être menotté
| Règle | Valeur | Champ |
|---|---|---|
| Classes capturables | `AFortSoldier` (prisonniers contre rançon), `ATikiCreature` et sous-classes (Veilleurs : prime ; villageois `ATikiVillager`, PNJ des maisons) | `CapturableClasses` |
| Jamais | `AGiantCrab` (boss), et aucun pion tenu par un joueur | `NeverCapturableClasses` |
| Maîtrisé | santé ≤ 35 % **ou** à terre (une jambe arrachée, il rampe) | `SubduedHealthFraction`, `bDownIsSubdued` |
| Portée | 200 cm des yeux au corps de la cible, en vue ; la cible à moins de 40° du viseur | `CuffReachCm`, `CuffAimAngleDeg` |
| Geste | F maintenu 1,5 s ; relâché avant, rien ne se passe | `CuffHoldSeconds` |
| Tolérance serveur | +60 cm de portée (latence) | `ServerReachToleranceCm` |

**Villageois capturables** (décision du propriétaire, 2026-10-11) : les PNJ du village (`ATikiVillager`, créatures tiki fictives) se menottent et se vendent comme les autres, sous le nom « villageois ». Le cadrage de départ les excluait (T-139 : le village de Motutapu est réel et la vente d'insulaires capturés rappelle les raids négriers de 1862-1863 dans les Tuamotu) ; la règle reste dans les données (`NeverCapturableClasses`) pour revenir en arrière sans code.

### 2.2 Menottes en acier (`AHandcuffsItem`, `DA_Item_Handcuffs`)
- Ouvertes dans la main droite (`SM_Steel_Handcuffs_Open`, socket `grip`).
- La touche F ne leur est donnée que si une cible valable est devant le joueur : sinon F garde son rôle (ramasser, s'asseoir, la cage).
- Une invite sous le viseur dit ce qui est possible : « Maintenir F : menotter le soldat », ou pourquoi pas (« Pas encore maîtrisé : santé 62 % (il faut 35 % ou moins, ou à terre) »), puis la barre du maintien.
- **Réutilisables** : une paire tient un seul captif. Elle redevient libre quand ce captif est vendu, s'enfuit ou disparaît.
- Un joueur ne mène qu'un prisonnier à la fois.

### 2.3 Prisonnier (`ACapturePrisoner` + `UCaptiveComponent`)
| Règle | Valeur | Champ |
|---|---|---|
| Suit son geôlier à | 180 cm | `FollowDistanceCm` |
| Longueur de chaîne (au-delà : tiré à pleine vitesse) | 350 cm | `ChainLengthCm` |
| Allure | la marche (150 cm/s) ou celle du geôlier, jusqu'à 650 cm/s chaîne tendue ; 70 cm/s s'il rampe | `FollowWalkSpeed`, `FollowMaxSpeed`, `CrawlFollowSpeed` |
| Lâché si | le geôlier meurt, nage, quitte la partie, ou s'éloigne à plus de 12 m | `ReleaseDistanceCm` |
| Eau | il n'entre jamais dans plus de 50 cm d'eau : il attend au bord | `MaxWadeDepthCm` |
| Lâché | il reste menotté, immobile ; n'importe quel joueur reprend sa chaîne avec F (« Prendre la chaîne du prisonnier ») ; il s'enfuit après 90 s | `AbandonedEscapeSeconds` |
| Invulnérable | oui : aucune balle perdue ne coûte la prime | `bPrisonerInvulnerable` |
| Clips | `A_BeanFR_Captive_Idle` / `_Walk` (T-337) pour le squelette des soldats ; les Veilleurs gardent leurs propres clips, mains ramenées devant le ventre par une pose procédurale (IK à deux os) | `BeanCaptiveIdle`, `BeanCaptiveWalk`, `BoundHands*` |
| Menottes fermées | `SM_Steel_Handcuffs` sur `hand_r` (transformation du manifeste T-337) | `CuffBones`, `CuffsOnBone` |
| Chaîne | `SM_Chain_Link` répété tous les 3,2 cm (110 maillons au plus 120), de `chain_anchor` des menottes au milieu de la mitaine gauche du geôlier ; en première personne, elle entre dans le champ de vision en bas à gauche | `ChainLink*`, `JailerChain*`, `FirstPersonChainOffset` |

**Choix d'architecture (écart au cadrage)** : le cadrage prévoyait un composant ajouté au soldat ou au Veilleur, et des points d'accroche dans `FortSoldier` et `TikiCreature`. À la capture, le mob est **remplacé** par un `ACapturePrisoner` à sa place, avec son apparence : maillage, matériaux, taille, os perdus (chapeau, membres) et clips. Le `UCaptiveComponent` (état répliqué, geôlier, chaîne) vit sur ce prisonnier. Trois raisons :
1. Le fort (`AFortGarrisonZone::CheckSoldiers`) et l'arène de `L_Test_Flat` (`AArenaZone::CheckMembers`, 10 fois par seconde) **replacent de force** leurs soldats et mobs à l'intérieur. Un captif resté soldat serait téléporté en arrière dès la sortie du fort ou du cercle.
2. `Creatures/Hostility` fait de tout soldat ou Veilleur vivant une cible pour les autres camps : un captif resté soldat serait tué en route par les Veilleurs.
3. Zéro ligne dans `FortSoldier.*`, `TikiCreature.*`, les zones et l'IA, qui sont gros et sensibles (garnison, démembrement, T-332 et T-328 viennent d'y être livrés).

En contrepartie, le soldat ou le Veilleur capturé quitte le jeu pour de bon : pas de butin, pas d'arme lâchée, et le fort le compte comme disparu. Un prisonnier lâché ne redevient jamais combattant ; il s'enfuit, c'est-à-dire qu'il disparaît.

### 2.4 Cage (`ACageItem` → `ACaptureCage`, `DA_Item_Cage`)
| Règle | Valeur | Champ |
|---|---|---|
| Durée de vie | 120 s, compte à rebours au-dessus de la cage | `CageLifetimeSeconds`, `bShowCageTimer` |
| Retour | dans l'inventaire de celui qui l'a posée (son pion actuel), sinon pliée au sol à sa place | `bReturnCageToPlacer`, `bDropCageWhenNoRoom` |
| Posée | 150 cm devant le joueur, porte tournée vers lui | `PlaceDistanceCm`, `CageYawOffsetDeg` |
| Geste | `A_Bean_Cage_Place` 1,5 s : la caisse pliée touche le sable à 0,4 s, la cage s'ouvre à 0,933 s (`CagePlaced`) ; touches de déplacement au repos pendant le geste | `PlaceGestureSeconds`, `PlaceCrateDownSeconds`, `PlaceEventSeconds`, `bHoldStillDuringGestures` |
| Place | une boîte de 120 × 160 × 190 cm libre au-dessus du sol (murs, arbres, rochers, barque, corps) ; aucune autre cage à moins de 4 m | `CageHalfExtentCm`, `ClearanceLiftCm`, `MinCageSpacingCm` |
| Vente | prisonnier mené à moins de 3 m, F sur la cage (vente automatique possible en réglant un rayon) | `SellRadiusCm`, `AutoSellRadiusCm` (0) |
| Prix | 10 000 c au trésor de l'équipage | `SalePrice` |
| Emmené après | 4 s dans la cage fermée | `TakenAwaySeconds` |
| Porte | +100° vers l'extérieur en 0,6 s, sur le socket `door_hinge` | `DoorOpenYawDeg`, `DoorSwingSeconds`, sockets `door_hinge`, `prisoner`, `entry` |

**La plage** : il n'existe ni matériau physique « sable », ni type de surface dans le projet. Une plage est donc définie comme un **sol de terrain** (objet `WorldStatic`, jamais une barque, un pont, une caisse ni un corps) qui remplit quatre conditions :
- il est entre **0 et 300 cm au-dessus du niveau de la mer** (`UIslandWaterSubsystem::GetSeaLevel`, vagues exclues ; la montée des eaux déplace la plage) ;
- il n'est **pas plus pentu que 20°** ;
- **la mer est à moins de 30 m** : parmi 24 sondes, sur 8 directions et 3 anneaux de 10, 20 et 30 m autour de la place, au moins un fond est trouvé 10 cm ou plus sous le niveau de la mer ;
- il y a la place pour la cage.

Sur `L_Test_Flat`, le plat central n'a pas de mer à 30 m : la plage est à l'ouest (x ≤ −4900, mer à −50). Les valeurs sont dans la catégorie « Beach » (`MinHeightAboveSeaCm`, `MaxHeightAboveSeaCm`, `MaxShoreDistanceCm`, `ShoreMinDepthCm`, `ShoreProbeDirections`, `ShoreProbeRings`, `MaxSlopeDeg`).

### 2.5 Argent : trésor de l'équipage
La vente appelle `URunSubsystem::AddMoney(World, SalePrice)` sur le serveur. C'est le chemin de `TrySpend`, que prend le marchand : la trésorerie de la partie (`FRunContractState::Treasury`), poussée vers `URunStateComponent` sur une carte d'expédition, et lue directement chez l'hôte sur `L_Test_Flat`. La vente est sauvegardée avec la partie. Le message d'équipe donne le montant et le trésor après la vente : « X a mis un soldat en cage : +10 000 c au trésor (trésor : 10 600 c) ».

**Vérifié dans le code** : le compteur « 150c » en bas à droite de la carte de test n'est **pas** le trésor. C'est la valeur de la cargaison posée à bord du galion (`AGalleonActor::GetCargoValue`, `STapaoHUD::Refresh`), qui devient la valeur du jour, donc le quota, à la fin de la journée. Après une vente, ce compteur ne bouge pas : le trésor se lit dans la boutique (« Trésorerie de l'équipage : N couronnes »), dans le message de vente, ou avec `monke.Run.Status`.

Si le propriétaire veut voir la prime dans ce compteur, ou la compter dans le quota, il y a deux suites possibles, à arbitrer : déposer une « bourse de la prime » à bord du galion (un objet de valeur 10 000 c), ou ajouter une ligne « Trésor » au HUD.

### 2.6 Touches, HUD, messages
- **F** pour tout. Les objets n'ajoutent leur contexte d'entrée (créé à l'exécution depuis `UseKeys`, priorité 6, touche consommée) que lorsque leur action est possible et que le viseur n'est pas sur un objet utilisable. Sinon, F sert à l'interaction habituelle : ramasser, s'asseoir, vendre sur la cage, reprendre une chaîne.
- **Invite** : une ligne sous le viseur dans le HUD TAPAO (`STapaoHUD`), lue chez `UCaptureSubsystem::GetHudLine`. Elle affiche, dans l'ordre :
  1. un message de la capture pendant 4 s : vente (6 s), capture, prisonnier lâché ou enfui, cage posée ou repartie, refus du serveur ;
  2. sinon, l'invite de l'objet en main, avec la barre du geste ;
  3. sinon, le rappel du prisonnier mené.
  Les invites de la cage et du prisonnier lâché passent par `IIslanderInteractable`, donc par l'invite « F » habituelle d'`AIslanderHUD`.
- **Textes** : en `FText` dans `DA_CaptureRules` (catégorie « Texts », localisables) : invites, messages, noms des captifs, motifs de refus.

## 3. Critères d'acceptation (IDs CAP)
- [ ] **CAP-01** Solo, `L_Test_Flat` : la cage et les menottes sont dans l'inventaire de départ (cellules libres de la barre) et posées au sol près des autres ramassages (groupes A et B).
- [ ] **CAP-02** Cage en main sur le plat central : « La cage se pose sur la plage, près de la mer », F ramasse ou utilise normalement. Sur la plage ouest : « F : poser la cage (2 min) ». F joue le geste : la caisse touche le sable, puis la cage s'ouvre à 1,5 m. L'objet quitte l'inventaire et le compte à rebours part de 2:00.
- [ ] **CAP-03** Une seconde cage à moins de 4 m, une pente ou un palmier dans la boîte : refus clair, aucun geste.
- [ ] **CAP-04** Menottes en main devant un soldat en pleine santé : invite « Pas encore maîtrisé : santé 100 % … ». Soldat à 30 % : « Maintenir F : menotter le soldat ». Relâché avant 1,5 s : rien. Maintenu : le geste joue, le soldat devient prisonnier menotté (fers aux poignets, clip captif) et suit à 1,8 m, chaîne entre ses menottes et la main gauche du joueur.
- [ ] **CAP-05** Un villageois maîtrisé se menotte et se vend (« villageois ») ; le crabe géant jamais (« Celui-là ne se met pas aux fers »).
- [ ] **CAP-06** Le prisonnier sort du cercle de l'arène et du fort sans être replacé. Les Veilleurs et les soldats ne l'attaquent pas.
- [ ] **CAP-07** Prisonnier mené à moins de 3 m de la cage, F sur la cage : la porte s'ouvre, il entre, la porte se ferme, +10 000 c au trésor (boutique, `monke.Run.Status`), message à tout l'équipage. Il disparaît 4 s plus tard, les menottes redeviennent libres.
- [ ] **CAP-08** Le geôlier nage, meurt ou court à plus de 12 m : le prisonnier reste menotté sur place, son ancien geôlier est averti ; un autre joueur reprend sa chaîne avec F ; sans geôlier pendant 90 s, il s'enfuit.
- [ ] **CAP-09** À 2:00, la cage disparaît et revient dans l'inventaire de celui qui l'a posée, ou pliée au sol s'il n'a plus de place.
- [ ] **CAP-10** Hôte + client : chacun voit les gestes, la cage, sa porte, son compte à rebours, le prisonnier, sa chaîne et les messages. Deux joueurs qui menottent la même cible : un seul gagnant (§12 n°2). Deux joueurs sur la même chaîne lâchée : un seul la prend.
- [ ] **CAP-11** Réseau dégradé (100 à 200 ms, 1 à 3 % de pertes, §12 n°10) : pas de double vente, pas de prisonnier fantôme, pas de menottes bloquées « en usage ».
- [ ] **CAP-12** Le marchand vend la Cage (1 500 c) et les Menottes en acier (800 c) dans « Outils et objets », avec leurs icônes.

## 4. Design technique
- **Dossier** `Source/monke_island/Capture/` (domaine Capture, catégorie de log `LogMonkeCapture`). Les dépendances vont toutes dans un seul sens : Capture → Fort, Creatures (dans `CaptureTargets.cpp` seulement), Player, Weapons (objet d'inventaire), Interaction, Water, Run, Character/Bean. Aucun domaine ne dépend de Capture, sauf le HUD (`UI/Tapao/STapaoHUD.cpp`, en lecture seule).
- **Classes**
  | Classe | Rôle |
  |---|---|
  | `UCaptureRules` | DataAsset `DA_CaptureRules` : toutes les valeurs et tous les textes |
  | `CaptureMath` | règles pures (maîtrise, motifs de refus, plage, sondes de la mer, allure, lâcher, chaîne, durées, horloge), testées |
  | `CaptureTargets` | seul endroit qui connaît `AFortSoldier` et `ATikiCreature` : genre, « à terre », copie de l'apparence et des clips |
  | `UCaptureSubsystem` | registre des cages et des prisonniers (aucune recherche d'acteurs), réservation d'une cible par une paire de menottes, ligne de HUD et messages locaux |
  | `ACaptureItem` | base des deux objets : maillage tenu sur `hand_r` après la pose du pantin, contexte d'entrée créé à l'exécution, gestes répliqués, maintien des touches de déplacement, limite de requêtes, messages au porteur |
  | `ACageItem` | lieu de la cage (aperçu chez le propriétaire, recalcul par le serveur), geste, pose au moment `CagePlaced`, retrait de l'inventaire |
  | `AHandcuffsItem` | recherche de cible, maintien, contrôle final, capture, état « en usage » répliqué |
  | `ACaptureCage` | cage posée : durée, porte, vente, paiement, emport, retour de l'objet, compte à rebours |
  | `ACapturePrisoner` | pion du captif : apparence, pose, menottes fermées, interaction « prendre la chaîne », fuite |
  | `UCaptiveComponent` | état répliqué (mené, lâché, entre dans la cage, en cage), geôlier, pas (navigation ou ligne droite, jamais dans l'eau profonde), contrôles du lâcher, chaîne cosmétique |
  | `UCapturePrisonerAnimInstance` | instance d'animation de la bibliothèque du bean, plus les mains liées (IK à deux os dans l'espace composant) |
- **Réseau**
  - Requêtes client : `ACageItem::ServerPlaceCage()` (sans paramètre : le serveur cherche lui-même la place), `AHandcuffsItem::ServerBeginCuff(ACharacter*)` et `ServerCancelCuff()`. La vente et la chaîne reprise passent par `IIslanderInteractable` (`UIslanderGrabComponent::ServerInteract` : portée et vue revérifiées, limite de requêtes).
  - Chaque requête est limitée (6 d'un coup, puis 3 par seconde ; le relâchement n'est jamais limité), puis entièrement revérifiée par le serveur. Les refus sont journalisés (`Warning`) et envoyés au seul joueur (`ClientNotice`), sans déconnexion.
  - Répliqué : le geste (`FCaptureGestureRep`, heure serveur, rejoué en retard là où le serveur en est), `bDeployed`, `Prisoner` des menottes ; sur le prisonnier, `Look` et `Rules` (une fois), `State` et `Jailer` ; sur la cage, `Rules`, `ExpireServerTime` et `PlacerName` (une fois), `bDoorOpen`, `Occupant` et `bGoing` (cage toujours pertinente).
  - Multicasts fiables : la vente et le lâcher. La fuite et le départ de la cage laissent 1 s au message avant que l'acteur ne disparaisse.
  - Un seul gagnant : une cible n'est réservée qu'à une paire de menottes à la fois (`ClaimTarget`), une vente n'est payée qu'une fois par prisonnier (état `Caged`), et la première chaîne reprise l'emporte.
- **Bean (points d'accroche)**
  - `UBeanAnimLibrary` : `CagePlace` et `Cuff`.
  - `UBeanAnimDriverComponent` : `PlayFullBody(clip, vitesse, départ)` et `StopFullBody(clip)`. Les gestes sont étirés sur leur durée de jeu ; celui des menottes l'est sur le maintien, pour que son clic tombe quand le serveur ferme les fers.
  - Le tag d'acteur `BodyArmsInFirstPerson` montre les bras du corps en première personne quand un objet de capture est en main.
- **Tick** : objets de capture seulement en main ou pendant un geste ; porte de la cage seulement quand elle bouge ; le prisonnier à chaque image (sa pose, ses pas, la chaîne), comme les mobs qu'il remplace (quelques acteurs au plus).
- **Éditeur** : `scripts/editor/capture_ue.py` (action « capture » de la session du lead, rapport `monke_island/Saved/T336/capture.json`, essai à blanc `--check`) :
  - importe le kit de T-337 (maillages avec leurs UCX et leurs sockets ramenés à l'échelle 1, textures, `MI_Capture_*`, icônes, clips) ;
  - crée `DA_CaptureRules`, `BP_CaptureCage`, `BP_CageItem`, `BP_Handcuffs`, `DA_Item_Cage`, `DA_Item_Handcuffs` ;
  - branche les clips du bean ;
  - complète l'inventaire de départ de `BP_IslanderBean`, la boutique et les ramassages de `L_Test_Flat`.

## 5. Tâches
| # | Tâche | Agent | Statut |
|---|-------|-------|--------|
| T-336 | Code Capture/, points d'accroche bean et HUD, tests, script éditeur, spec | ue-gameplay-programmer | review |
| T-337 | Cage, porte, caisse pliée, menottes (ouvertes, fermées), maillon, textures, icônes, 5 clips, manifestes | ue-blender-artist | livré (status « built ») |
| — | Build complet, action « capture », tests `monke_island.Capture`, PIE solo puis hôte + client (CAP-01 à CAP-12) | lead | à faire |

## 6. Tests
- Automation spec `monke_island.Capture` (`Capture/Tests/CaptureSpec.cpp`). Elle couvre :
  - les valeurs du propriétaire (2 min, 10 000 c, F, 35 %, 2 m, 1,5 s) ;
  - les classes capturables : soldats, Veilleurs et villageois ; jamais le crabe géant ni un joueur ;
  - un texte pour chaque refus ;
  - la maîtrise (seuil, à terre, nombre invalide) et l'ordre des refus du menottage ;
  - la visée ;
  - la plage (hauteur, pente, fond marin, sondes, ordre des refus) ;
  - l'allure (attente, pas du geôlier, chaîne tendue, borne) et les motifs de lâcher ;
  - la chaîne (affaissement, longueur conservée, points, nombre de maillons) ;
  - les gestes (vitesse étirée, avancement) ;
  - la cage (temps restant, horloge « 1:42 », porte) et un prix jamais négatif.
- Niveau de preuve au 2026-10-11 : **compilé** (fichiers seuls avec UBT `-SingleFile`, session Live Coding active, sans édition de liens). Le build complet, les tests, l'action éditeur, le PIE et le réseau n'ont pas encore été passés.

## 7. Limites connues et points à arbitrer
- Les transformations « unreal_guess » du manifeste de T-337 (caisse et menottes en main, menottes du captif, bout de la laisse, caisse au sol) sont à vérifier en jeu. Les réglages se font dans `DA_CaptureRules`.
- **Veilleurs** : pas de clip captif. Leurs mains liées viennent d'une pose procédurale sur leurs propres clips ; les fers sont réglés pour le squelette du bean.
- Un captif sans jambe rampe, à 70 cm/s : son geôlier doit l'attendre.
- Le prisonnier ne monte ni dans la barque ni sur le galion (lâché à 12 m).
- `A_Bean_Lead_Walk` (marche du geôlier, chaîne en main) n'est pas branché : il faudrait que le pilote d'animation du bean connaisse la laisse.
- Le compteur « c » du HUD est la cargaison du galion, pas le trésor (§2.5).

## 8. Journal
- 2026-10-11 : demande du propriétaire ; cadrage du lead (soldats et Veilleurs, jamais les villageois, maîtrise, F maintenu, chaîne, cage de 2 min sur la plage, +10 000 c au trésor, inventaire de départ, `L_Test_Flat`, boutique). Code, script, tests et spec écrits (T-336). Kit livré par T-337.
- 2026-10-11 : décision du propriétaire « utilise les pnj pour la cage » : villageois capturables (`ECaptiveKind::Villager`, `VillagerName`), compilé en fichier seul.
