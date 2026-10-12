# Caisse de transport en bois (T-258)

| Champ | Valeur |
|---|---|
| Tâche | T-258, lot **L2** (récolte, portage, arrimage ; cahier §5 à §7). Demande du propriétaire du 2026-10-09 : « ajoute la caisse en bois […] charge et ajoute dans Flat Test map, fais le spawn sur le galion, cette boîte permettra de transporter objet en masse plus facilement de la terre à la barque ». |
| Recette visée | §12 n°1 (bilan correct) et n°6 (ranger puis vider : quantité et valeur inchangées), §24.9 RUN04 et RUN05 (ce qui est dans la barque ou à bord du galion est compté une fois). |
| Maquette | `caisse-transport` v0.4.0 (`validated`) du 2e PC : `OneDrive\maquette-blender\maquettes\caisse-transport`, en lecture seule. Copie de travail : `art/export/Props/LootCrate/`. |
| Code | `Source/monke_island/Props/LootCrate/` : `LootCrateActor`, `LootCrateRules` (DataAsset), `LootContainer` (interface et comptage), `LootCrateMath` (règles pures) ; retouches localisées dans `Interaction/IslanderGrabComponent`, `Boats/BoatActor`, `Galleon/GalleonActor` et `Galleon/GalleonRules`. |
| Tests | `monke_island.LootCrate` (15 tests) : défauts, capacité, masse, taille, vidage, couvercle et comptage des contenus. |

## Intention
Le loot commun est fait de petits objets (0,03 à 1,2 kg). Les ramener un par un jusqu'à la barque coûte beaucoup d'allers-retours. La caisse sert à les regrouper : on la remplit à terre, on la porte (seul, ou à deux si elle est lourde), on la pose dans la barque, et tout son contenu voyage et compte avec elle.

## Règles
- **Corps physique.** La caisse est une boîte libre de 80 × 45 × 30 cm qui simule la physique. On la soulève comme les autres objets lourds, avec le composant de saisie (physics handle, cahier §5).
  - Un joueur seul porte à pleine force jusqu'à `LiftMassKg` (25 kg). Au-delà, la caisse traîne, sauf si plusieurs joueurs la tiennent : leurs forces s'additionnent (saisie partagée, ADR-002 P-06).
  - Le porteur est ralenti par sa part de la masse.
- **Ranger.** En tenant un objet, viser la caisse puis appuyer sur F : l'objet entre dans la caisse.
  - Il disparaît : sans corps physique, sans collision, caché, attaché à la caisse.
  - Sa masse s'ajoute à celle de la caisse, et le couvercle s'ouvre puis se referme.
  - L'objet est refusé (F le lâche simplement) dans ces cas :
    - la caisse contient déjà `MaxItems` objets ;
    - l'objet ferait dépasser `MaxContentKg` ;
    - sa boîte ne tient pas dans l'intérieur (`InnerSizeCm`, dans n'importe quelle orientation) ;
    - c'est un joueur, une autre caisse, un acteur fixé à autre chose, ou un acteur tagué `NoGrab` ou `NoCrate` ;
    - `bOnlyValuables` est activé et l'objet n'a pas de `UValuableComponent`.
- **Porter.** Les mains vides, F (tap) sur la caisse la soulève. L'invite indique « Porter la caisse (N objets, M kg) », où M est la masse de la caisse et de son contenu. Maintenir F soulève aussi, comme pour tout objet physique.
- **Vider.** Accroupi, F (tap) sur une caisse non vide la vide ; l'invite devient « Vider la caisse (N objets) ».
  - Les objets ressortent en éventail devant la caisse, du côté du joueur : 70 cm, 22° entre deux objets, 6 par rang, puis un rang 30 cm plus loin.
  - Chacun est posé sur le sol trouvé sous sa place (sable, pont ou plancher de barque) et ne traverse jamais une paroi.
  - Ils redeviennent des objets libres, avec la même valeur.
- **Protection.** Un objet rangé ne subit aucun choc : il n'a plus de collision. La caisse, elle, n'a pas de valeur.
- **Destruction.** Si la caisse est détruite en jeu (chute hors du monde, par exemple), son contenu ressort à sa place. Il n'est jamais perdu caché.

## Arrimage et bilan
- **Barque.**
  - **Charge** : la caisse pèse dans `ABoatActor::GetLoadKg` avec son contenu, car sa masse physique vaut caisse vide + contenu. La surcharge et la voie d'eau la prennent donc en compte.
  - **Transport** : posée dans la coque, elle est arrimée comme tout objet au repos (`KeepCargoAboard`) et son contenu voyage avec elle.
  - **Comptage** : `ABoatActor::GetCargoAboard` liste les objets libres à bord. Elle descend dans les caisses (`LootContainers::AppendWithContents`), sans compter deux fois les objets tenus par un joueur.
- **Galion et bilan.** `AGalleonActor::GetCargoAboard` descend aussi dans les caisses. `GetCargoValue`, et donc `AExpeditionGameState::ComputeDayValue` (valeur rapportée du jour) et la valeur de cargaison du HUD Tapao, comptent les objets de valeur rangés dans une caisse posée à bord.
  - Aucun double comptage : un objet rangé n'a plus de collision, l'overlap de la cargaison ne le voit donc pas directement.
- **Palan (RUN04, s'il est codé plus tard).** Pour ne rien oublier, il doit utiliser `GetCargoAboard` de la barque, ou hisser la caisse entière.

## Apparition
- **Galion, à chaque expédition.** Le serveur pose `UGalleonRules::LootCrateClass` sur le pont, près du marchand. Il le fait juste après le marchand, dans `AGalleonActor::ApplyRules`.
  - **Classe** : par défaut la classe C++ nue (`/Script/monke_island.LootCrateActor`). Elle charge `DA_LootCrateRules` par chemin s'il existe (`DefaultRules`, sinon les valeurs par défaut de la classe) et ses meshes par les chemins des règles, comme le marchand. Elle ne dépend donc pas du Blueprint ; `BP_LootCrate` peut la remplacer dans `DA_GalleonRules`.
  - **Point visé** : `DeckSpawnSocket` + `LootCrateOffset` (90, 230) cm dans le repère de la coque, soit à tribord du marchand, loin de l'échelle de coupée. La caisse est tournée de `LootCrateYaw` (90°, sa longueur dans l'axe de la quille).
  - **Place libre** : elle prend la première place libre sur des anneaux de `LootCrateSearchStep` (25 cm), jusqu'à `LootCrateSearchRings` (6) anneaux. Une place convient si :
    - le pont de l'équipage est dessous, à ±`LootCrateMaxDeckStep` (25 cm), donc pas sur un gaillard ;
    - rien ne gêne : pavois, hiloire d'écoutille, marchand, mât ;
    - elle reste à l'écart des 4 places de l'équipage.
  - **Pose** : la caisse est posée à 3 cm du pont et s'y stabilise.
  - Le journal donne sa position, ou prévient si aucune place libre n'a été trouvée.
- **`L_Test_Flat`.** Le script `cratemap` pose une caisse `LootCrate_Test` près du premier PlayerStart. Il y ajoute quatre objets de loot commun (`LootCrate_TestLoot_*`) pour l'essayer tout de suite, dans le dossier `LootCrateTest` avec le tag `T258_LootCrate`.

## Réseau (serveur autoritaire, cahier §9)
- **Contenu.** `Contents` (objet + masse) est répliqué. Seul le serveur range (`ServerStoreHeld`, après vérification de la portée depuis les yeux, de la ligne de vue sans l'objet tenu et de `CanStoreItem`) ou vide (`Interact` vérifié par le composant de saisie).
  - Un client ne choisit ni ce qui entre ni ce qui sort.
  - Les autres joueurs qui tenaient l'objet le lâchent.
- **Côté client.** La caisse est un acteur physique répliqué (mouvement et physique). Pour les objets rangés :
  - caché et physique coupée sont répliqués par le moteur (`bHidden`, `bRepPhysics`, attachement) ;
  - la collision ne l'est pas : `OnRep_Contents` la coupe sur chaque client.
  - Un objet qui ressort ne redevient visible et solide qu'une fois détaché de la caisse par sa propre réplication (2 s au plus), pour ne pas pousser la caisse de l'intérieur.
- **Pertinence.** Un objet caché sans collision n'est plus pertinent pour les clients. Après quelques secondes, son canal se ferme : un objet apparu en jeu est détruit chez le client, puis recréé quand la caisse est vidée. Le compte et la masse restent exacts côté client, car ils viennent de `Contents`.
- **Couvercle.** `MulticastLidPulse` (non fiable, cosmétique) ouvre puis referme le couvercle sur chaque poste et joue le son facultatif. La caisse ne « tick » que pendant ce mouvement.
- **Repos réseau (T-316).** La caisse porte un `UNetRestComponent`, comme les props (`docs/features/loot-common.md`) :
  - 30 envois par seconde en mouvement au lieu de 100, aucun une fois endormie (dormance) ;
  - posée sur le pont du galion, elle y est collée sur chaque machine après 0,75 s d'immobilité, avec ce qu'elle contient ; dans la barque, la barque la colle à sa coque comme sa cargaison ; un joueur qui la soulève la libère partout ;
  - chaque changement de contenu la réveille d'abord : rangement, vidage, objet disparu de l'intérieur. Un client dont le canal dormait reçoit le contenu tout de suite, et le multicast du couvercle trouve un canal ouvert ;
  - l'objet rangé et les objets qui ressortent sont réveillés aussi.
  - Spec `monke_island.LootCrate` (`Network`) : une caisse endormie se réveille au rangement et au vidage.

## Valeurs (`DA_LootCrateRules`, jamais dans le code)
| Valeur | Défaut | Note |
|---|---|---|
| `MaxItems` / `MaxContentKg` | 12 / 60 kg | demande du lead (la maquette conseillait 25 kg et 30 cases) |
| `EmptyMassKg` | 8 kg | la maquette calcule 12,7 kg de bois et 4 kg de couvercle |
| `InnerSizeCm` | 74,4 × 39,4 × 26,6 | intérieur de la maquette |
| `BodyHalfSizeCm` | 40 × 22,5 × 15 | boîte physique (extérieur des planches) |
| `LidHingeCm`, `LidOpenAngleDeg` | (0, 22,5, 30), 105° | charnière sur l'arête haute de la face avant (+Y, côté opposé aux cordes) |
| `LidOpenSeconds` / `LidHoldSeconds` / `LidCloseSeconds` | 0,2 / 0,35 / 0,3 s | |
| `bEmptyWhenCrouched` | vrai | accroupi + F = vider |
| `EmptyDistanceCm`, `EmptySpreadDeg`, `EmptyPerRow`, `EmptyRowStepCm`, `EmptyDropHeightCm` | 70, 22°, 6, 30, 3 | |
| `StoreSound`, `EmptySound`, `SoundAttenuation` | vides | sons facultatifs (cellule audio) |
| `CarryPrompt`, `EmptyPrompt`, `StorePrompt` | textes FR avec pluriel ICU | à passer en String Table avec l'UI (§16) |

Placement sur le galion (`DA_GalleonRules`) : `LootCrateClass`, `LootCrateOffset`, `LootCrateYaw`, `LootCrateSearchStep`, `LootCrateSearchRings`, `LootCrateMaxDeckStep`, `LootCrateClearanceCm`.

## Assets
| Asset | Source | Note |
|---|---|---|
| `/Game/MonkeIsland/Props/LootCrate/Meshes/SM_LootCrate` | `SM_LootCrate.fbx` + `_LOD1` / `_LOD2` | 3 LOD (tailles d'écran 1 / 0,5 / 0,2), 5 UCX creuses conservées, 36 sockets (`Lid`, `LootZone`, `Slot_00..29`, `Grab_L/R`, `Carry`, `BoatSlot`), pivot au milieu du fond, 80,9 × 49,2 × 30,1 cm |
| `/Game/MonkeIsland/Props/LootCrate/Meshes/SM_LootCrate_Lid` | `SM_LootCrate_Lid.fbx` + LOD | 3 LOD, 2 UCX, socket `Grab`, pivot sous le plateau |
| `Textures/T_LootCrate_BaseColor`, `_Normal`, `_ORM` | atlas 2048 de la maquette | normale DirectX (vert gardé), ORM linéaire (masques) |
| `Materials/MI_LootCrate` | parent `M_Character_Baked` | BaseColor, ORM, Normal, émissif 0 |
| `DA_LootCrateRules`, `BP_LootCrate` | script `cratemap` | à créer après le build (voir plus bas) |

- **Scripts.**
  - `scripts/props/import_loot_crate_ue.py` (action `crateimport`) importe les assets. Il remet en place les sockets que l'import des LOD retire (le moteur supprime les sockets « créés à l'import » absents du fichier LOD) et écrit son rapport dans `monke_island/Saved/T258/loot-crate-import.json`.
  - `scripts/editor/loot_crate_test_map_ue.py` (action `cratemap`) crée le DataAsset et le Blueprint, puis place la caisse sur la carte de test. Il s'arrête net, sans rien toucher, si les classes C++ ne sont pas encore compilées.
- **Non intégré.** Les 5 animations de portage de Bean (`A_Bean_Crate_*`), la petite caisse à anse (`SM_LootCrate_Small`) et le portage « au ventre » de la maquette (sockets `Carry`, `Grab_L/R`). La caisse se porte comme tout objet physique, flottante devant les yeux.

## Statut (honnête)
- [x] Assets importés dans l'éditeur partagé le 2026-10-09 (`crateimport` passé ; rapport `monke_island/Saved/T258/loot-crate-import.json`).
- [x] C++ compilé le 2026-10-09 fichier par fichier, sans édition de liens : 0 erreur, aucun warning. Fichiers compilés :
  - `LootContainer.cpp`, `LootCrateActor.cpp`, `Tests/LootCrateSpec.cpp` ;
  - `IslanderGrabComponent.cpp`, `BoatActor.cpp`, `GalleonActor.cpp` ;
  - le code généré par UHT pour les classes touchées (RPC, OnRep, réplication).
- [x] `cratemap` lancé avant le build : il refuse proprement (« classes not in this editor yet »), sans rien modifier.
- [ ] Build complet, puis `cratemap` (DataAsset, Blueprint, carte de test) : à lancer par le lead.
- [ ] `monke_island.LootCrate` exécuté.
- [ ] Testé en PIE : rien de ce qui précède n'a été joué.
- [ ] Testé hôte + client.
- [x] T-316 (repos réseau, réveil à chaque changement de contenu) compilé le 2026-10-11, fichier par fichier, avec le code généré ; spec `monke_island.LootCrate` (`Network`) non exécutée ; PIE non fait.

## Reste à faire
- [ ] **Cuisson.** La classe nue charge `DA_LootCrateRules` et les meshes par chemin. Avant un build packagé, ajouter `+DirectoriesToAlwaysCook=(Path="/Game/MonkeIsland/Props/LootCrate")` dans `Config/DefaultGame.ini`. Aujourd'hui, ces assets ne sont cuits que parce que `BP_LootCrate` est posé sur `L_Test_Flat`.
- [ ] **HUD.** Afficher l'invite de rangement en tenant un objet. Aujourd'hui, `Player/IslanderHUD.cpp` affiche « Lâcher ». Il faut y appeler `UIslanderGrabComponent::FindStoreTarget` puis `ILootContainer::GetStorePrompt` (« Ranger dans la caisse (3/12) »).
- [ ] Recette en PIE :
  - remplir à 12, puis tenter un 13e objet (refusé) ;
  - porter seul à vide, puis à deux une fois pleine ;
  - poser la caisse dans la barque : charge, arrimage, puis traversée ;
  - mesurer la valeur du jour avec la caisse sur le pont du galion ;
  - vider accroupi ;
  - vérifier la caisse du galion à chaque expédition, sur les deux cartes d'expédition (FRG_01 et FRG_02..20).
- [ ] Hôte + client : contenu, masse et couvercle identiques ; objet qui ressort sans à-coup.
- [ ] Sons (rangement, vidage, couvercle) par la cellule audio.
- [ ] Animations de portage et de chargement dans la barque de la maquette (AnimBP de Bean).
