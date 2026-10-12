# BUG-122 : la barque est emportée par le vent

- **Sévérité** : majeur
- **Statut** : fixed — validation du joueur à effectuer dans le build corrigé
- **Date** : 2026-10-04
- **Chantier** : T-122

## Reproduction

Lancer une expédition, quitter la zone calme du galion en barque et attendre une rafale ou la tempête. Le propriétaire constate que la barque est emportée.

**Attendu** : aucune poussée directe du vent sur la coque ; flottaison, houle, rames et charge conservées.

## Cause racine

`AWindStormActor::RefreshPushedBodies` inclut les corps physiques proches des joueurs. `ApplyForces` leur applique une pression proportionnelle à la surface exposée et au carré du vent relatif. La coque de `ABoatActor` entre dans cette liste et n'avait pas le tag `WindSheltered` déjà reconnu par la tempête.

## Correctif

- `ABoatActor` reçoit `WindSheltered` dans le constructeur et à `BeginPlay`, pour couvrir aussi les anciens Blueprints et instances qui remplacent le tableau de tags natif.
- `GetWindAccelerationOn` respecte la même exclusion que l'application effective des forces.
- Aucun réglage de masse, de flottaison, de houle, de rames ou de vent global n'est changé.
- Les modifications locales préexistantes qui protègent le galion sont conservées.

## Non-régression

`monke_island.Storm.BoatWind` crée un monde physique et un vent fort : la coque doit rester immobile tandis qu'un objet libre voisin est poussé. Les tags de la barque sont effacés avant `BeginPlay` pour reproduire une surcharge par un Blueprint. Un second test contrôle l'accélération annoncée pour les objets abrités.

Avant correction, les deux tests échouent : vitesse, déplacement et accélération de la barque non nuls ; accélération annoncée d'un objet abrité non nulle. Preuve : `build/T-122/red-report.json`.

Le worktree `dev/T-122` part de la version Git, qui manque de changements locaux existants (`SetForceThirdPerson`, `bExhaustionKillsInWater`) nécessaires au build. Le correctif seul a été reporté par patch dans l'arbre principal complet, sans écraser ces changements. Sa compilation et ses tests y sont vérifiés.

Après correction : compilation éditeur réussie et suite Automation complète à 311/311 tests réussis, dont les deux tests `BoatWind`. Trois avertissements attendus proviennent des tests existants de refus de données économiques invalides. Preuves : `build/T-122/green-build.log`, `green-tests.log`, `green-report.json` et `test-started.txt`.

BuildCookRun terminé avec succès (code 0) : `build/T-122/package.log`. Livraison corrigée : `build/rec-T122/Windows/monke_island.exe`, avec tout son dossier `Windows`. Correctif isolé conservé dans le commit `9a80b28` de `dev/T-122` ; le package utilise l'arbre principal avec ses changements locaux.

Le jeu corrigé a été ouvert automatiquement pour le propriétaire. `build/T-122/game-validation.log` confirme le menu, le chargement de Raraka et le début d'expédition. La validation visuelle du comportement de la barque par le joueur reste à effectuer.
