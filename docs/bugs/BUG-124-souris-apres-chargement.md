# BUG-124 : souris bloquée à l'arrivée en partie

- **Sévérité** : majeur
- **Statut** : corrigé — validation visuelle du joueur en attente
- **Date** : 2026-10-04
- **Chantier** : T-124 ; interface TAPAO, recette UI01/UI06

## Reproduction

Depuis le menu principal, lancer une expédition et attendre la disparition du chargement. La souris ne fait pas tourner la caméra. Ouvrir puis fermer Pause débloque le contrôle.

**Attendu** : la souris contrôle immédiatement la caméra lorsque le joueur apparaît, sans clic ni passage par Pause.

## Cause

Le menu règle le viewport en `FInputModeUIOnly`, qui ignore les entrées de jeu et désactive la capture de la souris. Les voyages de carte conservent ce viewport. `HideCurtain` retirait le rideau de chargement sans rétablir `FInputModeGameOnly`. Fermer Pause appelle déjà `RefocusScreen`, qui rétablit correctement ce mode et la capture relative de la souris.

## Correctif

À la fin du chargement d'une île, le rideau rend le contrôle via `RefocusScreen`. Si le contrôleur local n'est pas encore disponible, le rideau attend son arrivée. Un nouveau voyage annule cette attente. Les menus Pause, confirmation, fin de partie et paramètres gardent la priorité lorsqu'ils sont ouverts ; le retour au menu principal ne capture pas la souris.

Périmètre : `UI/Tapao/TapaoUISubsystem.*` et `UI/Tests/TapaoInputSpec.cpp`, développés dans `.worktrees/T-124`. La base Git du worktree manque de dépendances locales déjà présentes dans l'arbre principal ; seul le patch de cette tâche y est reporté pour compilation et tests.

## Vérifications

`monke_island.UI.TapaoInput` utilise un contrôleur local et un viewport Unreal/Slate isolés. Il vérifie les entrées du jeu, le mode de capture et la demande de mouvement relatif après le chargement, puis Pause, retour au menu et arrivée retardée du contrôleur. Il n'initialise pas les sous-systèmes de sauvegarde/paramètres et ne crée pas de fenêtre native.

Compilation éditeur réussie. Suite complète : **315/315 tests réussis**, dont `TapaoInput` **4/4**, trois avertissements attendus des tests Economy. Preuves fraîches : `build/T-124/green-build.log`, `green-tests.log`, `green-report.json`, `green-started.txt` ; rapport contrôlé avec filtre, minimum et date de début.

Build Win64 Development réussi : `build/rec-T124/Windows/monke_island.exe`. Le jeu est ouvert automatiquement ; son journal `build/T-124/game-validation.log` confirme le menu et le départ vers la partie. Validation visuelle du joueur : déplacer la souris dès l'apparition du personnage, ouvrir/fermer Pause puis revenir au menu.

Le premier packaging a rencontré l'éditeur rouvert par une autre session (Live Coding), puis un build concurrent. La livraison réutilise l'éditeur déjà compilé et testé (`-skipbuildeditor`) et attend le verrou UBT pour compiler la cible jeu (`-ubtargs=-WaitMutex`). Aucune fermeture forcée de l'éditeur. Packaging réussi en 3 min 34 s, preuve `build/T-124/package.log`.

Vérification dans le build : arrivée sur `Lvl_Raraka` avec `FRG_Raraka_12`, puis journal du 4 octobre à 02:51:20 UTC : `MouseLockMode` passe de `DoNotLock` à `LockOnCapture`, `MouseCaptureMode` passe de `NoCapture` à `CapturePermanently`. Cela confirme la restauration du mode dans le package ; le ressenti de la caméra reste une validation visuelle du joueur.

Avant correction, les tests d'arrivée immédiate et retardée échouent sur les entrées/capture ; les cas Pause et menu passent (`build/T-124/red-report.json`). Après correction, les deux cas d'arrivée passent. Le premier passage complet signale une erreur du montage de test Pause : le `SBox` de remplacement ne reçoit pas le focus, contrairement au vrai `STapaoPause`. Le montage utilise désormais un `SButton` focalisable, sans changer les assertions (`fixture-report.json` conserve ce passage).

Commits isolés : `871027d` (correctif et tests), `eb02418` (montage de test focalisable), branche `dev/T-124`. Les autres modifications locales sont conservées. ARC/MEM : réutilisation de la gestion existante des écrans, aucun tick permanent ajouté ; rappel de fin de chargement annulé au voyage et à la désinitialisation. Réseau : uniquement le contrôle local, sans changement de RPC.
