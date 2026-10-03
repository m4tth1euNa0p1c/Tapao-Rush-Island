# Normes d'architecture

S'applique à tout le code C++ et aux Blueprints de monke_island / Crownfall Expeditions. Le cahier des charges prime (§9, §21, §22) ; un écart à ces normes passe par un ADR.

**Références** :
- Epic Coding Standard et documentation *Gameplay Framework* ;
- ISO/IEC 25010 (modèle de qualité logicielle) ;
- modèle C4 (vues contexte, conteneurs, composants) ;
- ADR au format de Michael Nygard ;
- C++ Core Guidelines (interfaces, propriété des ressources).

## 1. Attributs de qualité (ISO/IEC 25010), par ordre de priorité
1. **Exactitude fonctionnelle et intégrité réseau** : serveur autoritaire, résolutions idempotentes par `ExpeditionId`.
2. **Fiabilité** : sauvegardes sûres, reprise après crash, aucune impasse d'interface (`error-handling.md`).
3. **Efficacité** : budgets de `performance.md` et `memory.md`.
4. **Maintenabilité et testabilité** : règles en fonctions pures testées, domaines découplés.
5. **Sécurité** : `security.md`.

En cas de conflit entre deux attributs, l'arbitrage est écrit dans un ADR.

## 2. Structure du code
- **Un seul module UBT**, `monke_island`. Pas de nouveau module ni plugin sans besoin démontré (cahier §9), et toujours par ADR.
- **Un dossier par domaine** dans `Source/monke_island/` :
  - domaines du cahier : `Player/`, `Interaction/`, `Items/`, `Boats/`, `Expedition/`, `World/`, `Economy/`, `AI/`, `UI/`, `Save/` ;
  - domaines existants : `Water/`, `Character/`, `Settings/`, `Session/`, `Hub/`, `IslandEditor/`, `Cosmetics/`.
- **Inclusion** : `#include "<Domaine>/X.h"`.
- **Couches** : une dépendance ne va que vers le bas.

| Couche | Contenu | Peut dépendre de |
|--------|---------|------------------|
| 1. Socle (`Core/`) | interfaces (`Interactable`, `Damageable`, `Carryable`), types et énumérations partagés, catégories de log, définitions de DataAssets | moteur uniquement |
| 2. Domaines | règles, état, services de chaque domaine | socle, et interfaces ou événements d'un autre domaine |
| 3. Présentation (`UI/`, FX, audio) | widgets, HUD, retours visuels et sonores | socle et domaines, en lecture seule (événements, propriétés répliquées) |

**Interdits** :
- inclure un en-tête `UI/` depuis un domaine ;
- des cycles d'inclusion entre domaines ;
- inclure la classe concrète d'un autre domaine quand une interface, un délégué ou un subsystem suffit ;
- utiliser du code `Variant_*` (template) dans le code Crownfall sans ADR.

## 3. Où placer quoi (Gameplay Framework)
| Élément | Responsabilité |
|---------|----------------|
| GameMode | Règles et transitions décidées par le serveur. N'existe que sur le serveur. |
| GameState | État partagé répliqué : phase, échéance, zones, quotas, barque. |
| PlayerState | État répliqué propre à chaque joueur : identité, vivant ou mort, prêt, embarqué. |
| PlayerController | Entrées, interface locale, requêtes vers le serveur. |
| Pawn / Character | Incarnation : déplacement, animation. |
| Components | Comportements réutilisables : santé, endurance, interaction, portage. |
| Subsystems | Services sans acteur (Save, Settings, UIFlow, Water), selon la portée : GameInstance, World ou LocalPlayer. |
| DataAssets / DataTables | Toutes les valeurs de gameplay. Jamais de valeur en dur. |
| Blueprints | Assemblage, références d'assets, réglages. Chaque classe du catalogue §21 qui porte des règles hérite d'une base C++. Pas de logique dans le Level Blueprint. |
| Widgets | Observent des états et envoient des requêtes. Ne calculent jamais une règle ni une récompense (§21). |

## 4. Règles de conception
- **Composition plutôt qu'héritage** : au plus 3 niveaux d'héritage sous la classe moteur. Pour le couplage faible, utiliser des interfaces `UINTERFACE` et des délégués `On<Quelque chose>Changed`.
- **Identifiants stables** : `FGuid`, `FName` ou `FPrimaryAssetId`. Jamais un nom saisi par le joueur comme clé ou chemin de fichier (§17).
- **Pas d'état global mutable** : ni variables statiques mutables, ni singletons « maison ». Utiliser des subsystems. `GetAllActorsOfClass`, `FindObject` et les recherches par tag sont interdits dans un chemin appelé à chaque frame : tenir un registre dans un subsystem.
- **Tick désactivé par défaut** : préférer les timers et les événements. Un tick doit être justifié et mesuré.
- **Réseau** : la réplication se décide dès la conception, avec `ue-network-engineer`. Les requêtes client passent par un acteur possédé, et les résolutions sont idempotentes.
- **Références d'assets** : en souple (`TSoftObjectPtr`) entre domaines et vers le contenu lourd (`memory.md`).
- **Asynchronisme** : tout ce qui est long (sauvegarde, chargement, réseau) est asynchrone et rend succès, échec ou annulation (`error-handling.md`).
- **Testabilité** : une règle du cahier vit dans une fonction pure, appelable sans monde, et elle est couverte par une Automation Spec `monke_island.<Domaine>.*`.

## 5. Documentation obligatoire
- **`docs/architecture.md`** (exigé par le cahier §13) :
  - vues C4 de niveau 1 à 3 : contexte, conteneurs (hôte, client, sauvegardes locales), composants par domaine ;
  - graphe des dépendances entre dossiers ;
  - flux de référence du §22 ;
  - propriétaires (cellules de `.claude/factory/FACTORY.md`).

  Ce document est mis à jour par `/ue-architecture doc` après chaque ADR structurant.
- **Un ADR** (`docs/adr/ADR-NNN-<sujet>.md`, modèle `docs/templates/adr.md`) pour chacun de ces sujets :
  - nouveau module ou plugin ;
  - système maison contre GAS ;
  - schéma de sauvegarde ;
  - modèle de réplication ;
  - dépendance externe ;
  - écart à une norme de `docs/conventions/`.
- **Contrats publics** (interfaces, événements, DataAssets, signatures des RPC) : écrits dans la spec avant le code (`FACTORY.md` §1).

## 6. Contrôles de la revue d'architecture (`/ue-architecture review`)
| ID | Contrôle | Sévérité par défaut |
|----|----------|---------------------|
| ARC-01 | Dépendance interdite entre couches, ou cycle d'inclusion | MAJEUR |
| ARC-02 | Règle du cahier implémentée dans un widget, un Level Blueprint ou côté client | CRITIQUE |
| ARC-03 | Valeur de gameplay en dur | MAJEUR |
| ARC-04 | État global mutable, singleton maison | MAJEUR |
| ARC-05 | Recherche d'acteurs (`GetAllActorsOfClass`, `FindObject`) dans un chemin appelé à chaque frame | MAJEUR |
| ARC-06 | Tick activé sans justification | MINEUR |
| ARC-07 | Classe « dieu » : plus de 1 000 lignes, ou plus de 25 méthodes publiques | MINEUR |
| ARC-08 | Référence dure vers le contenu lourd d'un autre domaine | MAJEUR |
| ARC-09 | Règle sans test automatisé | MAJEUR |
| ARC-10 | Choix structurant sans ADR | MINEUR |
