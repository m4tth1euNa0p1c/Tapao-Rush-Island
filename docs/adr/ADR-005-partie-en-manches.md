# ADR-005 : Partie en manches autour du galion

- **Statut** : acceptée. Proposée le 2026-10-04 ; le propriétaire a répondu « Go » le même jour sans modifier les points ci-dessous. Elle met en œuvre le cahier v1.3, §3 et §24.
- **Demande** : le propriétaire décrit le parcours suivant.
  - Menu, lancer une partie, arriver sur le galion. En multijoueur, on attend les autres puis on lance.
  - Après un chargement, on se retrouve sur le galion. On prend la barque jusqu'à l'île, on y joue, on ramène les objets en barque.
  - De retour sur le galion, on termine la partie à la barre. Après un chargement, on arrive en zone sûre (océan seul, avec le galion), où l'on vend les objets.
  - Quota atteint : manche suivante. Sinon, on perd et on meurt.

## Contexte : le code au 2026-10-04

| Brique | État |
|---|---|
| Menu | `L_MainMenu`, interface TAPAO (monk-island-19), carte par défaut du jeu. JOUER ouvre `L_Boot` |
| Lancement | `L_Boot` + `AExpeditionBootGameMode` : tirage du fragment, attente des clients, `ServerTravel` vers `Lvl_Raraka?Island=FRG_Raraka_NN` (T-094, T-095) |
| Arrivée | `UBoatStartSubsystem` pose la barque au point d'arrivée du fragment (20 `AExpeditionArrivalPoint`) et y assoit les joueurs (T-093) |
| Expédition | `AExpeditionGameState` : phases Preparing / Exploring / Submerging / Resolved, horloge commune, submersion (T-097, T-098, T-101, T-103) |
| Mort | Mort définitive (T-104) : équipage entier mort, puis `RestartMap` (`L_Boot`) après 8 s |
| Contenu du fragment | Villages (`AVillageSpawner`, T-102), camps et marae (`ACampSpawner`), tiki : ils suivent le fragment de l'URL |
| Économie | `FEconomyCalculator` : quotas de bois et de nourriture, taxe de 35 %, bonus, boutique, résolution idempotente. Testé, pas branché au jeu |
| Objets | `APhysicsPropActor` + `UValuableComponent` (valeur qui baisse aux chocs). Armes de mobs lâchées (T-105) |
| Barque | Physique et rames (ADR-003), `CargoVolume` qui compte la masse, parois anti-chute (monk-island-19) |
| **Absent** | Galion, cale, palan, barre, zone sûre, vente, état de partie qui survit aux changements de map, sauvegarde de partie |

## Décision

1. **Trois lieux.**
   - `L_MainMenu` : inchangé.
   - **`L_Galleon`** (nouveau) : le galion en pleine mer. Il sert de lobby (manche 0) puis de zone sûre (manches suivantes), avec le cotre du Percepteur. Il **remplace `L_Boot`**, dont le tirage du fragment et l'attente des clients passent dans la phase Lobby.
   - **La map d'expédition** : `Lvl_Raraka` en intérim (T-095), puis `L_Frag_*` (ADR-004). Le galion y est posé au mouillage du fragment.
2. **Un seul galion.** `AGalleonActor` (C++) et `BP_Galleon` :
   - maillage, d'abord un blockout puis la maquette du 2e PC ;
   - pont praticable ;
   - volumes `AboardVolume` (joueurs) et `CargoVolume` (objets : cale et pont) ;
   - échelle de coupée ;
   - point d'amarrage de la barque ;
   - emplacements de rangement de la cale ;
   - prises pour la barre, le palan et la Balance ;
   - points d'apparition sur le pont.

   Le galion est cinématique : pas de flottaison simulée en V1, balancement cosmétique.
3. **L'état de partie survit aux voyages.**
   - `URunSubsystem` (`UGameInstanceSubsystem`) fait foi sur l'hôte. Il garde un `FRunState` : `RunId`, numéro de manche, `RoundId`, seed, contrat (fragment, quota, commandes), trésorerie, manifeste de cale, morts de la manche, fragments récents, partie close ou non.
   - Les clients en voient la partie utile par un **`URunStateComponent` répliqué**, ajouté au GameState de chaque map.
   - Le composant évite de changer l'héritage de `AExpeditionGameState` (propriétaire : monk-island-aa).
4. **Voyages.**
   - `ServerTravel` en mode seamless entre `L_Galleon` et la map d'expédition, avec une carte de transition vide `L_Transit`.
   - `APlayerState::CopyProperties` garde l'identité et l'état prêt.
   - URL d'expédition : `?Island=FRG_xx&Round=n` ; le parseur `ParseFragmentOption` existe déjà.
   - Le seamless travel est limité en PIE : la recette se fait en `-game` (comme TAPAO).
   - Repli accepté au prototype : un voyage non seamless, avec reconnexion des clients. L'état de partie restant chez l'hôte, rien n'est perdu.
5. **Cale entre les maps.**
   - Au départ, le serveur fige ce qui est dans le `CargoVolume` du galion, plus ce que tiennent les joueurs à bord.
   - Chaque objet devient un `FHoldEntry` : classe en référence souple, valeur courante, masse, `ItemId` stable. Les objets sont détruits et le manifeste écrit dans `URunSubsystem`.
   - En zone sûre, le galion les fait réapparaître sur les emplacements de la cale, physique endormie jusqu'à la prise.
   - Les invendus restent dans le manifeste d'une manche à l'autre.
6. **Barre et votes.**
   - `UHelmComponent` : action à maintenir 2 s, requête serveur par le PlayerController.
   - `UDepartureVote` applique les règles de §6.5 et §24.2 : tous à bord ou majorité, préavis de 5 ou 10 s, annulation possible.
   - À l'échéance : instantané de ce qui est à bord, mort des absents, voyage.
   - Le même composant sert au lobby, où lever l'ancre lance la manche.
   - La fin de submersion déclenche le départ automatique.
7. **Palan.**
   - `UHoistComponent` vérifie que la barque est amarrée au galion et immobile. Il faut pour cela un nouvel état `ABoatActor::IsMooredTo()` et une zone d'amarrage à contrainte douce.
   - Il range les objets du `CargoVolume` de la barque dans la cale (déplacement et animation du filet). Chaque objet est compté une fois.
8. **Vente et registre.**
   - `ARoyalAppraiserActor` réunit la Balance et la table du Percepteur.
   - Un objet posé sur la Balance est estimé à sa valeur courante. « Vendre » envoie une requête serveur, idempotente par `ItemId`, et alimente `FRunLedger`.
   - « Signer » ouvre un vote, puis `FRunCalculator::ResolveRound(rules, ledger, deaths)` tranche :
     - **succès** : crédit, sauvegarde, boutique, contrat suivant ;
     - **défaite** : scène, bilan, partie close, retour au menu.
   - `FRunCalculator` reprend `FEconomyCalculator::ComputeTax`. Il ajoute la courbe de quota, l'échelle selon les joueurs, les frais et les commandes. Ce sont des fonctions pures, couvertes par des tests Automation (RUN14).
9. **Mort.**
   - T-104 évolue : un équipage entier mort renvoie le galion seul en zone sûre après 8 s, cale conservée, au lieu de `RestartMap`.
   - La défaite n'arrive qu'à la signature.
   - Les morts réapparaissent à bord en zone sûre et leurs frais sont facturés.
10. **Sauvegarde.**
    - `SG_Run` passe par `SaveSubsystem` (§21), en écriture atomique. Elle est écrite à la signature réussie et à chaque achat.
    - Le marqueur `ExpeditionInProgress` (§17) est posé au lever d'ancre.
    - Une partie perdue est close et archivée.
11. **Interface** (style TAPAO de monk-island-19) : panneau d'équipage et contrat (lobby), registre, boutique, bilan de défaite. Les widgets observent `URunStateComponent` et ne calculent rien.
12. **Données.** `DA_RunRules` porte les valeurs de §24.6. L'estimation lit `UValuableComponent`. La difficulté par manche est lue par `TikiDirector`, les embuscades et les camps.

## Restructuration de l'existant

| Existant | Devient |
|---|---|
| `L_Boot` + `AExpeditionBootGameMode` (T-095) | Supprimé. Le tirage (`PickFragmentId`, sans redite) et l'attente des clients passent dans la phase Lobby de `L_Galleon` |
| JOUER → `L_Boot` | JOUER → `L_Galleon?listen` en multijoueur, `L_Galleon` en solo |
| `UBoatStartSubsystem` (T-093) : barque au point d'arrivée, joueurs aux rames | `UGalleonStartSubsystem`. Le point d'arrivée est décalé vers le large jusqu'à 4 m de fond ; le galion y est ancré, la barque amarrée au flanc, les joueurs posés sur le pont. Les 20 `AExpeditionArrivalPoint` restent la source |
| `RestartMap` de `AExpeditionGameState` (T-104) | Retour en zone sûre quand l'équipage est perdu ; défaite seulement à la signature |
| Départ depuis la barque (§6.5, jamais codé) | Départ à la barre (`UHelmComponent`, `UDepartureVote`) |
| `FEconomyCalculator` (quotas bois / nourriture) | `FRunCalculator` (quota en valeur, commandes, frais). `FEconomyCalculator` garde la taxe et la boutique |
| `ABoatActor` (`CargoVolume`, parois) | Plus un état « amarrée au galion » et une zone d'amarrage, pour le palan |
| `AVillageSpawner`, `TikiDirector`, camps, marae | Inchangés, ils suivent le fragment de l'URL. Ils lisent la difficulté de la manche |

## Ordre de mise en œuvre

Tâches créées au chantier le 2026-10-04.

1. **Règles et calculs purs** : `DA_RunRules`, `FRunState`, `FRunCalculator` et leurs tests, dont RUN14. *ue-systems-programmer*
2. **Galion en blockout** : `BP_Galleon` en primitives (pont, volumes, échelle, amarrage), pour avancer sans attendre la maquette. La maquette est commandée au 2e PC. *ue-boat-programmer, ue-level-designer, ue-blender-artist*
3. **`UGalleonStartSubsystem`** dans la map d'expédition, à la place de BoatStart : horloge après 10 s de préparation. *ue-boat-programmer*
4. **Barre, vote de départ, palan, abandon des absents, départ automatique** en fin de submersion. *ue-gameplay-programmer, revue ue-network-engineer*
5. **`L_Galleon`, `ARunGameMode`** (Lobby / Vente), `URunSubsystem`, voyages, manifeste de cale. Suppression de `L_Boot`. *ue-systems-programmer, ue-level-designer*
6. **Balance, registre, signature, défaite, contrat suivant** et leurs écrans TAPAO. *ue-gameplay-programmer, ue-ui-developer avec monk-island-19*
7. **Sauvegarde de partie**, Continuer, historique. *ue-systems-programmer*
8. **Recette RUN01–RUN14** en solo, puis hôte + client, puis 4 joueurs. Mise en scène provisoire (voiles, traversée, frégate). *ue-qa-tester, ue-tech-artist*

Les étapes 1 et 2 sont indépendantes et peuvent partir en parallèle. Les étapes 3 et 4 donnent la boucle île ↔ galion jouable sans zone sûre : départ, puis retour au lobby.

## Risques

- **Seamless travel en PIE** : support limité. La recette se fait en `-game` ; le repli non seamless est prévu (décision 4).
- **Physique** : les objets qui réapparaissent dans la cale s'empilent. On les range sur des emplacements, physique endormie.
- **Barque contre galion** : une barque physique contre un galion statique. On prévoit une zone d'amarrage à contrainte douce ; les parois de cargaison de la barque existent déjà.
- **Performance** : le galion est un gros acteur ajouté à chaque fragment, à mesurer avec T-091.
- **Propriété des fichiers** : `AExpeditionGameState` (aa), BoatStart et `ABoatActor` (barque, monk-island-19), TAPAO (monk-island-19), `Lvl_Raraka` partagée. Chaque tâche nomme son propriétaire au chantier.

## Points validés par le propriétaire (« Go » du 2026-10-04)

1. **Quota en valeur vendue**, au lieu de quantités de bois et de nourriture, qui deviennent des commandes facultatives.
2. **Morts en zone sûre** : ils réapparaissent à bord contre 50 couronnes de frais de recrutement.
3. **Équipage entier mort** : le galion rentre seul avec sa cale, pas de défaite immédiate. La vente décide.
4. **Invendus** gardés en cale pour une manche suivante.
5. **Lobby et zone sûre** = le même lieu, le galion en pleine mer.
6. **Défaite** : scène satirique (frégate, « déclaré traître à la Couronne »), sans gore.
7. **Courbe de quota** : 250, 400, 610, 880, 1 210… multipliée par 1 + 0,25 par joueur au-delà du premier.
