# Normes de gestion des erreurs, des journaux et de la robustesse

**Références** :
- Epic Coding Standard : assertions, pas d'exceptions C++ (désactivées dans Unreal) ;
- documentation Epic *Asserts*, *Logging* et *Structured Logging* ;
- C++ Core Guidelines, section E, adaptée : on signale l'échec par la valeur de retour, pas par une exception ;
- pratique SRE : taux de sessions sans crash ;
- cahier §16 (erreurs pour « Rejoindre »), §17 (sauvegardes), §21 (`SaveSubsystem`, `WBP_ErrorDialog`), §22 (flux).

## 1. Classer l'erreur, puis choisir le mécanisme
| Nature | Exemple | Mécanisme |
|--------|---------|-----------|
| Invariant interne, impossible si le code est juste | état interne incohérent, index interne hors bornes | `check` / `checkf`, `checkNoEntry()`. Actifs en développement, retirés en Shipping. |
| Bug probable mais récupérable | composant absent d'un Blueprint, asset non assigné | `ensureMsgf(Cond, TEXT("…"))` + repli : sortie anticipée ou valeur par défaut. Un ensure se signale une fois par session, sans arrêter le jeu. |
| Appel avec effet de bord à garder en Shipping | `verify(Init())` | `verify` / `verifyf` |
| Erreur attendue, venue de l'extérieur | fichier absent ou corrompu, version incompatible, réseau perdu, saisie invalide, requête refusée par le serveur | valeur de retour typée, log `Warning`, message au joueur (§3) |
| Donnée d'éditeur invalide | DataAsset hors bornes, référence manquante | `IsDataValid(FDataValidationContext&)` : bloque la sauvegarde ou le cook, pas le joueur |

**Règle absolue** : jamais de `check` sur une donnée venue du disque, du réseau ou d'un joueur. Un fichier malformé ou un paquet hostile ne doit jamais planter le jeu (`security.md`).

## 2. Fonctions faillibles
- **Valeur de retour typée** : `TValueOrError<T, E>` (`Templates/ValueOrError.h`, avec `MakeValue` / `MakeError`), `TOptional<T>`, ou `bool` avec un paramètre de sortie d'erreur. Marquer `[[nodiscard]]` toute fonction dont l'appelant doit traiter l'échec.
- **Une énumération d'erreurs par domaine** (`ESaveError`, `EJoinError`, `EPurchaseError`…), alignée sur les messages du cahier. Par exemple, pour Rejoindre (§16) :
  - session introuvable ;
  - session complète ;
  - version incompatible ;
  - partie déjà en expédition ;
  - connexion expirée ;
  - invitation non valide.
- **Asynchrone** : toute opération rend explicitement l'un de trois résultats, succès, échec avec motif, ou annulation (§21). Elle a un délai maximal (chargement : 60 s au §16) et n'est jamais attendue en bloquant le thread de jeu.
- **Transactions** : écrire dans un fichier temporaire, valider, puis remplacer (§17). En cas d'échec, revenir à l'état précédent et déverrouiller l'interface (§22 : « erreur exploitable et déverrouillage »). Les écritures sont sérialisées.
- **Idempotence** : rejouer une requête déjà résolue renvoie le résultat existant (`ExpeditionId`, `(ExpeditionId, ZoneId)`).

## 3. Côté joueur
- **Message** court, actionnable et en français, tiré d'une String Table. Il s'affiche dans `WBP_ErrorDialog` ou une notification, jamais comme un code brut seul.
- **Pas d'impasse** : chaque échec ramène à un état stable (menu, hub, formulaire prérempli). Un bouton n'est jamais sans effet ; une entrée indisponible est désactivée avec une explication (§16).
- **Sauvegarde** :
  - indicateur « Sauvegarde… / Sauvegardé / Échec » (§17) ;
  - si le fichier est corrompu, proposer la dernière copie valide, sans jamais l'écraser en silence ;
  - une campagne corrompue n'empêche pas d'en charger une autre.

## 4. Journaux
- **Catégories** : `Logmonke_island` par défaut. Un domaine critique ou bavard déclare la sienne :
  - nom `LogMonke<Domaine>` (`LogMonkeSave`, `LogMonkeExpedition`, `LogMonkeNet`…) ;
  - `DECLARE_LOG_CATEGORY_EXTERN` dans un `.h` du domaine, `DEFINE_LOG_CATEGORY` dans son `.cpp`.
- **Niveaux** :

  | Niveau | Usage |
  |--------|-------|
  | `Fatal` | interdit dans le code du projet (on utilise `check`) |
  | `Error` | une fonctionnalité a échoué ; une action est nécessaire |
  | `Warning` | anomalie récupérée, requête refusée |
  | `Display` / `Log` | jalons : début et fin d'expédition, sauvegarde, connexion |
  | `Verbose` / `VeryVerbose` | détail de débogage, désactivé par défaut |

  Jamais de log à chaque image au niveau `Log` ou au-dessus.
- **Contenu** : ce qui se passe, sur quoi (`GetNameSafe(Obj)`), les valeurs utiles et un identifiant de corrélation (`ExpeditionId`, `ZoneId`, `SlotId`). Pour les champs structurés, utiliser `UE_LOGFMT` (`Logging/StructuredLog.h`). Aucune donnée personnelle (`security.md`).
- **Shipping** : les logs y sont retirés à la compilation. Ce qui doit rester disponible en Shipping (le résumé technique du §18) passe par un mécanisme dédié.

## 5. Crashs et diagnostic
- Le Crash Reporter est actif en développement. Les symboles (`.pdb`) sont archivés pour chaque build, et `docs/production/journal.md` relie la version au jeu de symboles.
- Le marqueur `ExpeditionInProgress` permet de reprendre au hub après un crash, sans aucun loot crédité deux fois (§17).
- **Gate INT** (`FACTORY.md` §2) : aucun `Error`, aucun ensure et aucun crash en PIE, ni en solo ni en hôte + client.
- Un bug passe par `/ue-bugfix` : rapport, cause racine, test de non-régression.

## 6. Tests des chemins d'échec
- Chaque erreur attendue a son test, dans `monke_island.<Domaine>.Errors.*`. Exemples :
  - fichier tronqué ou aléatoire ;
  - version future ;
  - écriture impossible ;
  - requête refusée ;
  - double résolution ;
  - délai dépassé ;
  - session pleine.
- Un test qui déclenche volontairement une erreur la déclare : `AddExpectedError(TEXT("motif"), EAutomationExpectedErrorFlags::Contains, 1)`, ou `AddExpectedMessage`.

## 7. Contrôles de l'audit (`/ue-error-handling`)
| ID | Contrôle | Sévérité par défaut |
|----|----------|---------------------|
| ERR-01 | `check` sur une donnée externe (disque, réseau, joueur) | CRITIQUE |
| ERR-02 | Résultat d'un `Cast`, d'un `GetOwner`, `GetWorld`, `Load` ou `Find` utilisé sans vérification | MAJEUR |
| ERR-03 | Opération faillible qui échoue en silence (ni retour, ni log, ni message) | MAJEUR |
| ERR-04 | Opération asynchrone sans branche d'échec ou d'annulation, ou sans délai maximal | MAJEUR |
| ERR-05 | Échec qui laisse l'interface verrouillée ou un état partiel (faux emplacement, double crédit) | CRITIQUE |
| ERR-06 | Écriture de fichier non atomique, ou qui écrase sans garder de copie valide | CRITIQUE |
| ERR-07 | Log mal calibré : `Error` pour un cas normal, log à chaque image, aucun identifiant de corrélation | MINEUR |
| ERR-08 | Message d'erreur joueur absent, technique ou codé en dur hors String Table | MINEUR |
| ERR-09 | Chemin d'échec sans test | MAJEUR |
