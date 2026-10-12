# V1 — build Windows de recette pour présentation investisseurs

Date : 4 octobre 2026. Chantier : T-120. Configuration : Win64 Development, Unreal Engine 5.8.3. Statut : package produit et démarrage testé ; recette visuelle et fonctionnelle complète à valider.

## Dernière livraison corrigée — T-124

Le build du 4 octobre 2026 rend automatiquement le contrôle de la souris à la fin du chargement de l'île. Il conserve le correctif du vent sur la barque. Lancement : `build/rec-T124/Windows/monke_island.exe`, avec tout son dossier `Windows`.

- Compilation éditeur réussie ; 315/315 tests Automation réussis, dont `UI.TapaoInput` 4/4. Preuves : `build/T-124/green-report.json`, `green-tests.log` et `green-started.txt`.
- BuildCookRun réussi en 3 min 34 s, code 0 ; éditeur déjà compilé réutilisé, attente du verrou de compilation. Preuve : `build/T-124/package.log`.
- Jeu ouvert automatiquement. Le journal `build/T-124/game-validation.log` confirme le menu, Raraka (`FRG_Raraka_12`) et le passage de `NoCapture` à `CapturePermanently` après chargement. La validation de la sensation de contrôle reste au joueur.
- Correctif isolé dans `871027d` et `eb02418` (`dev/T-124`), reporté dans l'arbre partagé avec ses changements locaux.
- SHA-256 du binaire `monke_island/Binaries/Win64/monke_island.exe` : `22419133C7BE7983453FD24D4A03FC6F993F02462553B31BDB0D4C79ECC9DD62`.

## Livraison précédente — T-122

Le build corrigé du 4 octobre 2026 exclut la barque des forces du vent tout en conservant sa flottaison et les rames. Lancement : `build/rec-T122/Windows/monke_island.exe` ; conserver tout le dossier `Windows`.

- Compilation éditeur réussie et 311/311 tests Automation réussis, dont deux tests de non-régression du vent sur la barque.
- BuildCookRun réussi, code 0, durée 1 min 56 s. Preuves : `build/T-122/green-report.json` et `build/T-122/package.log`.
- Jeu ouvert automatiquement ; `build/T-122/game-validation.log` confirme le menu, le chargement de `Lvl_Raraka` et le début d'expédition avec `FRG_Raraka_08`. Recette visuelle et validation du joueur encore requises.
- Correctif `9a80b28` (`dev/T-122`) reporté dans l'arbre principal avec ses modifications locales préexistantes.
- SHA-256 du binaire `monke_island/Binaries/Win64/monke_island.exe` : `DD7BBE424FACB08230E55D4052219B16BAAADDC0A83A2AE05BA6DDA3C4657315`.

## Livraison initiale — T-120

- Lancement : `build/package/Windows/monke_island.exe`.
- Conserver tout le dossier `Windows` avec l'exécutable ; taille mesurée : 1,89 Gio, symboles Development inclus.
- Source : `main`, base `9926d4e`, avec les modifications locales présentes lors du build. Ce package n'est pas une version issue d'un arbre Git propre.
- SHA-256 du binaire `monke_island/Binaries/Win64/monke_island.exe` : `F585F7F36B4BE3020BD1FB23B461F71F4A5091E5013BF3EF428972F2521F59F7`.

## Vérifications réalisées

- [x] Assets ouverts sauvegardés par MCP, puis fermeture propre de l'éditeur.
- [x] Compilation éditeur : `Result: Succeeded`.
- [x] Suite Automation : 309 tests réussis, 0 échec, 0 non exécuté. Trois tests de refus de données invalides produisent chacun un avertissement attendu.
- [x] BuildCookRun : `BUILD SUCCESSFUL`, code 0, durée 3 min 50 s.
- [x] Manifest de livraison : trois cartes actuelles, menu TAPAO, barque et sept assets de règles/présentation explicitement inclus.
- [x] Exécutable démarré hors éditeur ; logs confirmant `L_MainMenu`, `L_Boot?listen`, sélection de `FRG_Raraka_10`, puis chargement de `Lvl_Raraka` et début d'expédition.
- [ ] Contrôle visuel automatisé : helper Computer Use indisponible après reprise et réinitialisation (`native pipe ... os error 2`). Les logs attestent le chargement, pas la qualité visuelle ni les interactions.
- [ ] Quitter proprement, jouer une manche complète et effectuer la recette hôte/client : non vérifiés. Le jeu est laissé ouvert pour le propriétaire.

Preuves : `build/rec-investisseurs/editor-build-final.log`, `automation-report.json`, `package.log`, `smoke-menu.log`. Le dernier log continue d'être alimenté par le jeu ouvert.

## Corrections de packaging

`DefaultGame.ini` déclare les cartes actuelles et les dossiers chargés par chemin natif dans les éléments à cuire. Les plugins `ModelContextProtocol` et `MCPClientToolset` sont limités à la cible Editor.

## Points à vérifier avant la présentation

- Le matériau `MI_Bean_SleeveRed` n'a pas l'usage `SkeletalMesh` : le moteur annonce son remplacement par le matériau par défaut sur le costume Redcoat.
- Le log contient des avertissements de placement physique (`FindTeleportSpot`) et un dépassement de file de travail des ombres virtuelles (`VSM ... Queue overflow`), susceptible d'affecter les performances. Aucun budget de performance n'a été mesuré dans ce build.
- La boucle complète des manches, vente et quotas autour du galion reste un chantier produit (T-112 à T-116). Ce build de recette ne vaut pas validation de la V1 complète ni de sa coopération multi-machines.

Pour la démonstration, vérifier manuellement le costume, le départ, la barque, le HUD et la fluidité sur la machine de présentation avant la séance.
