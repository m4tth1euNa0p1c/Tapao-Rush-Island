# Feature : Bandages de soin

- **Statut** : review (code compilé fichier par fichier, sans édition de liens ; build complet, action éditeur « bandages », tests et PIE à faire par le lead)
- **Owner** : ue-gameplay-programmer (T-340), art du kit par ue-blender-artist (contrat `art/export/Props/Bandage/manifest.json`), intégration par le lead
- **Date** : 2026-10-11
- **Cahier des charges** : lot L5. Sections : §4 (santé du joueur, 100 PV), §5 (objets de l'inventaire), §7 (boutique du marchand), §9 (réplication, serveur autoritaire), §21–22 (contrats). Aucun ID de recette du cahier ne couvre le soin : demande du propriétaire du 2026-10-11. IDs propres : BAN-01 à BAN-10 (§5). Recette du cahier touchée : §12 n°10 (réseau dégradé : l'application suit l'horloge du serveur).
- **Chantier** : T-340.

## 1. Intention de gameplay
Demande du propriétaire : des bandages de soin, objet consommable en pile ; F ou clic pour l'appliquer, une animation d'application en 1re et 3e personne, le soin jusqu'au maximum à la fin, interrompu par un coup ou un sprint ; en boutique, en ramassage et dans l'inventaire de départ de la carte de test L_Test_Flat.

Le joueur blessé sort un rouleau de bande de lin (touche de son emplacement). Il appuie sur **F** ou fait un **clic gauche** : le bean lève l'avant-bras gauche et y enroule la bande trois fois, serre et tapote (environ 3 s). Au moment où la bande est nouée, sa santé remonte au maximum et un bandage de la pile est consommé. Pendant l'application il peut marcher ; un coup reçu, un sprint, un saut, l'eau profonde ou le fait de ranger l'objet interrompent le geste : rien n'est consommé, rien n'est soigné. C'est le choix du risque : se soigner à l'abri, pas au milieu d'une mêlée.

## 2. Règles et valeurs réglables
Toutes les valeurs sont dans `DA_BandageRules` (`UBandageRules`, `Source/monke_island/Items/Bandage/BandageRules.h`). Les durées et les points d'accroche viennent du manifeste du kit art (le script éditeur les copie) ; le reste vient du code.

| Règle | Valeur | Champ |
|---|---|---|
| Durée de l'application (le clip est étiré dessus) | 3,0 s (manifeste : `clip.seconds`) | `ApplySeconds`, `ClipSeconds` |
| La bande commence à tourner | 0,4 s du clip (événement `WrapStart`) | `WrapStartSeconds` |
| Le soin tombe | 2,73 s du clip (événement `BandageDone`) | `HealEventSeconds` |
| Soin | jusqu'au maximum (sinon `HealAmount`, 50) | `bHealToMax`, `HealAmount` |
| À pleine santé | refusé (« Santé au maximum ») | `bUsableAtFullHealth` |
| Immobile pendant l'application | non (marcher est permis) | `bHoldStillWhileApplying` |
| Coupé par un coup | oui, dès 0,5 PV perdu d'un coup | `bInterruptOnHit`, `MinHitDamage` |
| Coupé par un sprint | oui ; option : au-delà d'une vitesse de marche (0 = sans limite) | `bInterruptOnSprint`, `MaxMoveSpeedCmS` |
| Coupé par un saut ou une chute | oui | `bInterruptOnJump` |
| Coupé par l'eau | à la nage ou plus de 40 cm d'eau aux pieds (patauger dans le lagon est permis) | `bInterruptInWater`, `MaxWaterDepthCm` |
| Taille de pile | 5 par case | `StackSize` (copié dans `DA_Item_Bandage.MaxStack`) |
| Prix chez le marchand | 150 c | `ShopPrice` |
| Bandages au départ sur L_Test_Flat | 3 | `TestMapStartingCount` (copié dans `BP_TestGameMode.ExtraStartingItems`) |
| Touches | F, clic gauche | `UseKeys`, `InputPriority` (6) |
| Débit des demandes (SEC-03) | 4 d'un coup, puis 2 par seconde | `RequestBurst`, `RequestsPerSecond` |
| Bande visible sur l'avant-bras après le soin | 6 s | `WrappedLingerSeconds` |
| Textes | « F : appliquer un bandage (3) », « Bandage… », « Plaie bandée : santé au maximum », « Bandage interrompu : coup reçu »… | `PromptText`, `ApplyingText`, `HealedText`, `FullHealthText`, `UnableText`, `InterruptTexts` |

Les touches ne sont prises à l'interaction (F) et au coup de poing (clic) que lorsque le bandage peut être appliqué et que le viseur n'est pas sur un objet utilisable : F ramasse toujours ce qu'on vise.

## 3. Flux (serveur autoritaire)
1. **Client propriétaire** : la touche est pressée ; si l'aperçu local le permet, `ABandageItem::RequestApply` envoie `ServerApply` (RPC fiable, sans paramètre).
2. **Serveur** : limitation de débit (`FRequestRateLimiter`), puis `BandageMath::JudgeStart` sur ses propres faits : l'objet est dans une case de l'inventaire de son porteur (`NotCarried` sinon), en main, porteur vivant, debout, au sol, hors de l'eau profonde, ne courant pas, blessé. Un refus est journalisé et dit au joueur (RPC client), sans autre effet.
3. **Serveur** : l'état `FBandageApplyRep` (en cours, heure serveur du début, durée) est répliqué ; deux minuteries : le soin à `BandageDone`, la fin à `ApplySeconds`. La santé du porteur est écoutée (un coup coupe), et chaque image le serveur vérifie sprint, saut, eau, rangement (`BandageMath::CheckInterrupt`).
4. **Toutes les machines** : le clip `A_Bean_Bandage_Apply` joue sur tout le corps (`UBeanAnimDriverComponent::PlayFullBody`, un client arrivé en retard démarre où en est le serveur) ; le rouleau suit la main droite (os `hand_r`, socket `grip`), la bande enroulée apparaît sur `lowerarm_l` dès `WrapStart`.
5. **Première personne (propriétaire)** : un rouleau propre à la vue (`FirstPersonRoll`) et les bras 1re personne pilotés par `UBeanFirstPersonArmsComponent::SetPoseOverride` : avec les clés du manifeste (`first_person.keys`, format `FBeanArmPose`) quand elles existent, sinon un enroulement procédural (l'avant-bras gauche monte, le rouleau fait trois tours autour, l'avant-bras redescend).
6. **Serveur, à `BandageDone`** : `UIslanderHealthComponent::ApplyHeal` jusqu'au maximum, puis `UIslanderInventoryComponent::RemoveItem` retire un bandage de la pile (le dernier détruit l'acteur et termine le geste avec lui).
7. **HUD** : `STapaoHUD` lit `ABandageItem::GetHudLine` : l'invite, la jauge discrète de l'application, la raison d'un refus ou d'une interruption.

## 4. Fichiers
- Code : `Source/monke_island/Items/Bandage/` (`BandageRules.*`, `BandageMath.h`, `BandageItem.*`) ; accroches : `UI/Tapao/STapaoHUD.cpp` (ligne et jauge), `Shop/ShopCatalog.cpp` (catégorie « consommable »), `TestMap/MonkeTestGameMode.*` (`ExtraStartingItems`).
- Tests : `Source/monke_island/Items/Tests/BandageSpec.cpp` (`monke_island.Items.Bandage.*`).
- Données (créées par `scripts/editor/bandages_ue.py`, pas encore exécuté) : `/Game/MonkeIsland/Items/Bandage/` (`DA_BandageRules`, `BP_BandageItem`, `DA_Item_Bandage`, meshes, clip), icône `/Game/MonkeIsland/UI/Icons/Bandage/T_Icon_Bandage`, offre dans `DA_ShopCatalog`, `BP_TestGameMode.ExtraStartingItems`, `Pickup_Bandage_A/B` sur L_Test_Flat.
- Console (hors Shipping) : `monke.Bandage.Hurt [PV]` retire des PV à tous les joueurs du serveur, mode dieu de la carte de test compris (jamais jusqu'à la mort), pour essayer un bandage.

## 5. Critères d'acceptation
| ID | Critère | Preuve visée |
|---|---|---|
| BAN-01 | Blessé, F ou clic avec le bandage en main lance l'application ; à la fin la santé est au maximum | spec `Item` « heals to the maximum… », PIE |
| BAN-02 | Une unité de la pile est consommée, une seule | spec `Item`, PIE (la case passe de 3 à 2) |
| BAN-03 | Un coup pendant l'application l'interrompt : ni soin ni consommation | spec `Item` « is cut by a blow », PIE |
| BAN-04 | Un sprint, un saut, l'eau profonde ou le rangement l'interrompent | spec `Interrupt`, `Item` « put away », PIE |
| BAN-05 | Refus à pleine santé, mort, à terre, en l'air, à la nage | spec `Start`, `Item` « full health » |
| BAN-06 | Refus pour un objet qui n'est dans aucune case de son porteur ; seule la connexion propriétaire peut envoyer la demande | spec `Item` « in no inventory » ; revue réseau |
| BAN-07 | Une rafale de demandes est refusée sans déconnexion | spec `Item` « rate limit » |
| BAN-08 | Animation en 3e personne vue par les autres joueurs, en 1re personne par le porteur | PIE hôte + client |
| BAN-09 | Le bandage est en vente (150 c), ramassable (L_Test_Flat), et 3 au départ sur L_Test_Flat | script éditeur, PIE |
| BAN-10 | La jauge de progression s'affiche pendant l'application, la raison d'une interruption ensuite | PIE |

## 6. Limites connues
- Le clip joue sur tout le corps : en marchant, les jambes suivent le clip (debout) et non la marche. Un calque haut du corps propre au bandage demanderait une accroche dans `UBeanAnimDriverComponent::UpperSamples` (hors du périmètre de T-340) ; `bHoldStillWhileApplying` immobilise le joueur si le rendu glisse trop.
- Le dernier bandage détruit l'acteur à `BandageDone` : la fin du geste (0,27 s, « serre, tapote ») et la bande enroulée qui reste sur l'avant-bras ne se voient pas pour ce dernier.
- `BP_TestGameMode` est en mode dieu (santé pleine) : sans `monke.Bandage.Hurt`, le bandage y est refusé (« Santé au maximum »).
- Les clés 1re personne du kit sont « à venir » dans le manifeste : l'enroulement procédural joue en attendant ; ses valeurs (`FirstPersonLeftForearm`, `FirstPersonWrapCenter`) sont à régler en PIE.
