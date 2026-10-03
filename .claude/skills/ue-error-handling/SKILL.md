---
name: ue-error-handling
description: Gestion des erreurs et robustesse de monke_island selon docs/conventions/error-handling.md — `audit [dossier]` (contrôles ERR-01 à ERR-09 : check sur données externes, échecs avalés, async sans échec ni délai, interface verrouillée, écritures non atomiques, logs, messages joueur, tests d'échec) ; `apply <domaine>` (catégorie LogMonke<Domaine>, énumération d'erreurs, types de résultat, messages en String Table, tests d'échec) ; `flow <flux §22>` (dérouler chaque étape d'un flux en cas d'échec). À utiliser pour un nouveau système faillible (sauvegarde, réseau, chargement), en REC, ou pour « ça plante quand… », « l'écran reste bloqué ».
argument-hint: "audit [dossier] | apply <domaine> | flow <nouvelle-campagne|sermon|extraction|reglage-video|test-editeur>"
---

# Gestion des erreurs

Mode : `$ARGUMENTS`. Par défaut : `audit` sur les fichiers de la tâche, sinon sur `monke_island/Source/monke_island/`.
Norme : `docs/conventions/error-handling.md`. Agent : `ue-reliability-engineer`.

## `audit [dossier]`
1. Recherches ciblées, puis **confirmation de chaque cas dans le code** :

   | Contrôle | Quoi chercher |
   |----------|---------------|
   | ERR-01 | `check(` / `checkf(` sur une valeur lue d'un fichier, d'un RPC ou d'une saisie |
   | ERR-02 | `Cast<`, `GetOwner()`, `GetWorld()`, `GetController()`, `FindComponentByClass`, `LoadObject` déréférencés sans test |
   | ERR-03 | `bool` ou `TOptional` ignorés ; `else` absent après une opération faillible ; `return;` sans log |
   | ERR-04 | callbacks asynchrones (`AsyncSaveGameToSlot`, `RequestAsyncLoad`, sessions) sans branche d'échec, d'annulation ou de délai |
   | ERR-05 | widget ou bouton verrouillé avant une opération et jamais déverrouillé sur la branche d'échec ; état partiel (faux emplacement, double crédit) |
   | ERR-06 | écriture directe sur le fichier final sans fichier temporaire ni copie valide gardée |
   | ERR-07 | `Error` pour des cas normaux, logs dans `Tick`, logs sans identifiant de corrélation |
   | ERR-08 | message joueur absent, technique, ou `FText` en dur hors String Table |
   | ERR-09 | chemin d'échec sans test `monke_island.<Domaine>.Errors.*` |

2. Rapport dans `docs/audits/<date>-erreurs-<sujet>.md` (modèle `docs/templates/audit-report.md`). Pour une tâche en REC, ajouter dans sa note `revue-fiabilite=OK`, ou la liste des écarts.

## `apply <domaine>` (dans les fichiers autorisés par la tâche)
1. **Catégorie de log** `LogMonke<Domaine>` : `DECLARE_LOG_CATEGORY_EXTERN` dans `<Domaine>/<Domaine>Log.h`, `DEFINE_LOG_CATEGORY` dans le `.cpp`.
2. **Énumération** `E<Domaine>Error` (`UENUM(BlueprintType)`), alignée sur les cas du cahier. Fonctions faillibles en `TValueOrError<T, E<Domaine>Error>` ou `TOptional`, marquées `[[nodiscard]]`.
3. **Messages joueur** : une entrée de String Table par erreur (avec `ue-narrative-designer` pour le texte), affichée par `WBP_ErrorDialog` ou par une notification (`ue-ui-developer`).
4. **Tests** : un test par erreur attendue dans `monke_island.<Domaine>.Errors.*`. `AddExpectedError` déclare les erreurs volontaires.
5. `/ue-build`, puis `/ue-test`.

## `flow <flux>` (cahier §22)
Pour chaque étape du flux (par exemple, nouvelle campagne : validation du formulaire → verrou du bouton → création transactionnelle → chargement du hub → état prêt), écrire :
- ce qui peut échouer ;
- qui le détecte ;
- l'erreur renvoyée ;
- le message au joueur ;
- l'état restauré ;
- le déverrouillage de l'interface ;
- le test qui le prouve.

Le résultat va dans le rapport d'audit, ou dans la section 4 de la spec de la feature.

## Règles
- Pas d'exception C++, pas de `try` / `catch`.
- La sauvegarde appartient à `ue-systems-programmer`, les sessions à `ue-network-engineer`, les écrans d'erreur à `ue-ui-developer`. Hors des fichiers autorisés, les corrections deviennent des tâches au chantier.
