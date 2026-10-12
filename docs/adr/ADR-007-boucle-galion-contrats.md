# ADR-007 : Boucle de jeu — galion hub, jours, contrats par zone et cupidité

- **Statut** : acceptée. Demande du propriétaire du 2026-10-06 (session monk-island-63) : « Voici la logique que je veux pour mon jeu ».
- **Remplace** l'ADR-006 (5 jours × 10 périodes, défaite au quota manqué). **Complète** l'ADR-005 : le galion reste le cœur de la partie, il devient hub, lobby et base. Le cahier des charges reste le contrat produit. Les écarts sont listés plus bas.

## Les trois boucles

1. **Session** : menu (Jouer → Solo / Créer une partie multijoueur / Rejoindre). L'hôte apparaît sur **son galion**, et jusqu'à 3 joueurs le rejoignent.
2. **Expédition** (un jour) : Débarquer → Explorer → Risquer → Collecter → Transporter → Extraire. Une même question doit revenir sans cesse : « on a encore le temps ? ».
3. **Progression** : jour suivant, puis contrat terminé, puis zone suivante, avec un quota plus gros, un danger plus fort et un meilleur butin.

## Décision

### Galion = lobby + hub + base
- **Zones physiques** :
  - cabine du capitaine : carte, choix de la destination, lancement ;
  - quartier marchand / coffre : achats, améliorations du navire ;
  - stockage : butin non vendu ;
  - table des contrats : quota, objectifs secondaires ;
  - pont : attente, départ, arrivée.
- **Départ** : l'hôte valide « PRÊTS POUR LE DÉPART », puis un court compte à rebours. La traversée se fait si possible sans écran de chargement apparent (vent, voiles, vagues, destination discrète).
- **Carte d'arrivée** : « JOUR 1 / RARAKA — SECTEUR NORD / QUOTA 0 / 1 800 ». Ensuite, on part en chaloupe.

### Une journée
- **Horloge du jour** :

  | Heure | Phase |
  |---|---|
  | 06h00 | arrivée |
  | 08h–14h | calme |
  | 14h–17h | risques en hausse |
  | 17h–19h | créatures plus nombreuses, marée |
  | 19h00 | nuit |
  | 21h00 | très dangereux |
  | 22h00 | le galion part |

  Elle s'appuie sur le temps réellement écoulé : le déluge existant (cahier §5, §6) devient « le départ du galion ».
- **Retour à bord** : on dépose le butin. Il n'est **pas vendu automatiquement**, et le joueur choisit :
  - vendre (argent immédiat) ;
  - stocker (pour le prochain quota) ;
  - étudier (lore) ;
  - utiliser (effets).
- **Fin du jour** : écran « JOUR N TERMINÉ » (valeur récupérée, blessures, objets perdus, artefacts rares, bonus d'exploration, quota X / Y).
- **Entre deux jours** : phase courte sur le galion (acheter, vendre, réparer, équiper, regarder le prochain secteur), puis « JOUR SUIVANT ».

### Contrats et zones (valeurs dans `DA_RunRules`, jamais en dur)
- **Un contrat = une zone, en 3 jours au plus** (`DaysPerContract`). La valeur rapportée s'additionne d'un jour à l'autre.
- **Zones par défaut** :

  | Zone | Danger | Quota |
  |---|---|---|
  | 1 Lagons extérieurs | faible | 1 800 |
  | 2 Village abandonné | moyen | 3 500 |
  | 3 Marae intérieur | élevé | 6 500 |
  | 4 Atoll frappé par une tempête | très élevé | 10 000 |
  | 5 Île interdite | extrême | 18 000 |

- **Ce qui augmente d'une zone à l'autre**, et pas seulement les PV : spawns (`SpawnScale`), valeur du butin (`LootValueScale`), départ du galion plus tôt (`FloodDelayScale`). Plus tard : objets plus lourds, météo, malédictions, cartes plus grandes.
- **Quota atteint au dernier jour** : « CONTRAT TERMINÉ — NOUVELLE DESTINATION DISPONIBLE ». L'excédent passe dans la trésorerie (`ExcessPayoutFraction`).
- **Cupidité (§16)** : si le quota est atteint **avant** le dernier jour, l'équipage choisit de **quitter la région** (contrat terminé) ou de **tenter un jour de plus**. Ce jour supplémentaire rapporte plus (`GreedLootScale` ×1,5) et il est plus dangereux (`GreedDangerScale` ×1,4).
- **Échec sans tout effacer (§14)** :
  - équipage entier mort : le galion repart, le butin resté sur l'île est perdu, le jour est consommé, frais médicaux par mort ;
  - quota manqué au dernier jour : « CONTRAT ÉCHOUÉ », une partie de la trésorerie est perdue (`FailedTreasuryLoss` 50 %), retour à une zone moins rentable (`FailedZoneDrop` 1). **Les améliorations du galion sont gardées**, et ce n'est pas une fin de partie.
- **Victoire** : le contrat de la dernière zone est terminé. Le record est conservé, puis une nouvelle partie peut commencer.

### Progression
- **Outils du joueur** : l'argent achète des **outils qui ouvrent des possibilités**, pas des +% :
  - lampe (nuit) ;
  - corde (trous) ;
  - pelle (butin enterré) ;
  - détecteur (artefacts) ;
  - chariot (objets lourds) ;
  - appareil photo ;
  - sac étanche ;
  - harpon.
- **Boutique de départ** : torche améliorée 120, sac renforcé 250, corde 80, kit médical 150, harpon 300, lampe longue portée 400, détecteur 600, chariot 800.
- **Améliorations du galion** : cale agrandie, atelier, infirmerie, cartographie, treuil, nouvelle chaloupe, sonar, cabine du collectionneur. Elles sont visibles dans le hub.

## Ce qui est fait (2026-10-06, session 63)
- **`Run/RunContract.h/.cpp`** : `FRunContractCalculator` (pur) gère les jours, le quota cumulé, la cupidité, l'échec et la victoire, ainsi que les multiplicateurs de danger, de butin et de délai.
- **`DA_RunRules`** (`URunRules`) : les 5 zones et les champs `Contract`.
- **`URunSubsystem`** (GameInstance) garde la partie d'une île à l'autre :
  - début de jour : `AExpeditionGameState::BeginPlay` ;
  - fin de jour : valeur de la cale du galion, morts, équipage mort ;
  - commandes console `monke.Run.Status / Stay / Leave / New`.
- **`URunStateComponent`** est sur le game state de l'expédition, répliqué : zone, jour, quota, valeur, cupidité, résumé du jour.
- **Branchements** :
  - spawns tiki × danger de la zone ;
  - valeur du butin × zone ;
  - délai avant le déluge × zone ;
  - HUD « Jour N/3 · Quota X / Y » et ligne de chargement.
- **Tests** : `monke_island.Run.Contract`.

### Suite du 2026-10-06 (session 63), compilé en cible jeu
- **Menu** : JOUER / CONTINUER / NOUVELLE PARTIE ouvrent un écran de choix :
  - SOLO (sans serveur) ;
  - CRÉER UNE PARTIE (`listen`, 4 joueurs au plus) ;
  - REJOINDRE (adresse IP de l'hôte, port 7777).
  NOUVELLE PARTIE remet à zéro les jours et la trésorerie (`URunSubsystem::StartNewRun`).
- **Marchand bean sur le pont du galion** :
  - placement : `AShopkeeperActor`, apparu par `AGalleonActor` (`UGalleonRules::ShopkeeperClass`) ;
  - interaction : F ouvre la boutique TAPAO (`STapaoShopScreen`) ;
  - catalogue `DA_ShopCatalog` : mousquet 400, pistolet 250, pompe 600, torche 120 ;
  - achat validé par le serveur : portée, trésorerie, place dans l'inventaire.
- **Munitions** : 15 par arme à feu. Après la dernière, l'arme quitte l'inventaire (`UFirearmRules::bDiscardWhenEmpty`).
- **Stockage** :
  - l'inventaire de chaque joueur, munitions restantes comprises, est gardé d'une île à l'autre (`FRunLoadout`) ;
  - une arme de mob y garde sa valeur de butin, multiplicateur de sa zone d'origine compris et pertes comprises (`FRunLoadoutItem::BaseValue` / `Value`, -1 = pas de valeur propre ; T-314). Le format de sauvegarde passe à 2 : une partie de format 1 se lit sans valeur, comme avant, puis se réécrit en format 2 ;
  - il est rendu une seule fois par île (R-41, T-311) : un pion du même joueur sur la même île (retour après départ, pion rejoué) repart avec le kit de base ; un joueur qui quitte la partie garde ce qu'il portait en partant ;
  - les morts repartent avec le kit de base ;
  - la partie est sauvegardée sur disque (slot `Run`, `URunSaveGame`) à chaque fin de jour, choix et achat, par `MonkeSaveSlot` (T-302) : fichier contrôlé avant lecture (taille, enveloppe, somme de contrôle, classe), écriture transactionnelle avec dernière copie valide (`Run.sav.bak`), fichier endommagé ou d'une version plus récente renommé à part, message au joueur sous le menu ;
  - seule la machine qui tient la partie (solo, hôte) lit et écrit ce slot ; un client affiche la partie de l'hôte (`URunStateComponent`, R-03, T-293) ;
  - trésorerie de départ : 600 (`StartingTreasury`).
- **Fin de jour** :
  - l'écran « JOUR N TERMINÉ » (`STapaoDayEnd`) s'affiche sur toutes les machines ;
  - la question de cupidité a des boutons chez l'hôte ; les autres joueurs voient « le capitaine décide » ;
  - le voyage est rallongé de `GreedChoiceSeconds` (15 s), puis avancé à 3 s après la réponse.

## Écarts à trancher ou à faire
| Sujet | Où on en est | Tâche |
|---|---|---|
| Galion hub au lancement (spawn sur le pont, lobby, attente des joueurs) | aujourd'hui : menu → `L_Boot` → île en barque, galion au mouillage | T-113 (refonte) |
| Zones du galion (cabine, marchand, stockage, table des contrats) et choix vendre / stocker / étudier / utiliser | rien | nouvelles tâches |
| Question de cupidité : vote de tout l'équipage, à la table des contrats | boutons chez l'hôte (fait) | UI |
| Écran « JOUR N TERMINÉ » | fait (`STapaoDayEnd`) | — |
| Horloge 06h → 22h affichée, paliers de danger horaires | jour/nuit et déluge existent en secondes écoulées | DayNight + HUD |
| Écran de fin de partie à la mort de l'équipage | T-173 (2026-10-10) : « JOUR N PERDU », le galion repart vers l'île suivante ; avis du Bureau CONTRAT TERMINÉ / ÉCHOUÉ ; code compilé fichier par fichier, non testé en jeu (`docs/features/fin-de-jour-contrats.md`) | T-173 (review) |
| Zones ↔ fragments (Lagons = quels fragments ?) | tirage aléatoire de tous les fragments | catalogue d'îles |
| Sauvegarde de la partie et record | en mémoire seulement (perdue à la fermeture) | T-115 |
| Boutique, outils, améliorations | rien | nouvelles tâches |
