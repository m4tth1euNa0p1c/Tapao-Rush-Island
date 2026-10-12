# 96 — Crash reporting, support et patchs
Domaine : J · Auditeur : ingénieur live ops

## Verdict
Aucune chaîne d'exploitation n'existe : le package ne lance pas le Crash Reporter, qui n'a aucune adresse d'envoi, les PDB ne sont pas archivés et rien n'identifie la build. Presque tout se règle par configuration et scripts, avant le premier testeur externe. Risque commercial : une build publique qui plante ou abîme les sauvegardes sans visibilité ni retour arrière.

## Constats
- **[Bloquant commercial] Un crash joueur n'arrive nulle part**
  - Constat : le Crash Reporter n'est pas stagé (`IncludeCrashReporter=False`, `ue.ps1 package` sans `-CrashReporter`) et son binaire n'a aucune URL d'envoi : son dialogue renvoie au formulaire de bugs d'Epic. Les PDB (395 Mo par build) ne vivent que dans chaque dossier `build/` ; T-120 vient d'un arbre Git sale.
  - Preuve : vu : `T-124/game-validation.log:1145` (« Could not start crash report client »), `BaseGame.ini:110`, `ue.ps1:140-141`, `CrashReportCoreConfig.cpp:32-38`, chaînes de `CrashReportClient.exe`.
  - Recommandation : BugSplat (`DataRouterUrl`, `symbol-upload -v` : docs.bugsplat.com/integrations/game-development/unreal-engine) ou Sentry (docs.sentry.io/platforms/unreal/, Windows UE 5.2+) ; `ue.ps1 release <v>` : refuse un arbre sale, tague, archive PDB et `Releases/<v>`. Tarifs non vérifiés.
  - Effort : M
- **[Majeur] Build non identifiable, données personnelles exposées**
  - Constat : la version du menu est un texte manuel « v0.1 » ; le journal ne porte que le moteur (5.8.3). Le crash contient `MachineId`, `EpicAccountId` ; le journal `User: matth`, `Computer: OMEN`, des chemins `C:\Users\…` : contre `security.md §6`. En Shipping, ensures et 232 `UE_LOG` disparaissent.
  - Preuve : vu : `TapaoUISkin.h:127-129`, `game-validation.log:284-296,663-664`, `Build.h:200-212`, crash T-124.
  - Recommandation : `ProjectVersion` `0.x.y+<hash>` posée par script, affichée, journalisée, envoyée aux outils ; `SetGameData` (fragment, manche, rôle) et `SetDeploymentName` (dev, bêta, public) de `FGenericCrashContext` ; masquage, consentement (97) ; bêta en Development, ou `bUseLoggingInShipping` (`RequiresUniqueBuildEnvironment` : coût à mesurer).
  - Effort : S
- **[Majeur] Retour arrière : un binaire ancien réécrit une sauvegarde récente**
  - Constat : `UTapaoSaveGame.Version` n'est jamais lue ; le moteur renvoie à `ULocalPlayerSaveGame` pour les versions de jeu. Supposé : un binaire ancien ignore les champs inconnus puis les perd à la réécriture, et Auto-Cloud rapporte la sauvegarde récente.
  - Preuve : vu : `TapaoSaveGame.h:22`, `GameplayStatics.cpp:112-113,2473-2488`, `SaveGame.h:47`.
  - Recommandation : `SG_Campaign` et `SG_Run` dérivent de `ULocalPlayerSaveGame` (`GetSavedDataVersion`, `HandlePostLoad`) ; refus d'une version future (UI05) dès la première build publique ; migrations additives ; test « N lit N+1, réécrit ».
  - Effort : M
- **[Majeur] Hotfix : un conteneur de 1,18 Go, ni `release` ni `patch`**
  - Constat : tout le contenu tient dans un `.ucas`, pour 76 DataAssets de 8 Mo. Le flux Epic (`-createreleaseversion`, `-basedonreleaseversion -generatepatch` : unité = un `.uasset`) n'est pas outillé ; le delta Steam d'un `.ucas` recuit n'est pas mesuré. La somme réseau est constante (audits 27, 56) : un hotfix de DA laisse joindre avec d'autres règles.
  - Preuve : vu : `monke_island-Windows.ucas` (1,18 Go), `ue.ps1:138-145`, `NetworkVersion.cpp:241-244`, dev.epicgames.com/documentation/en-us/unreal-engine/how-to-create-a-patch-in-unreal-engine (IoStore non confirmé).
  - Recommandation : essayer un patch d'un DA (< 5 Mo) ; avant la 1.0, un `tuning.json` non-UFS (`DirectoriesToAlwaysStageAsNonUFS`) lu au démarrage, borné, son hash dans `GetLocalNetworkVersionOverride` ; mesurer le delta sur branche privée.
  - Effort : M
- **[Majeur] Branches Steam et support : rien de planifié**
  - Constat : aucune tâche au chantier, aucune adresse, FAQ ni gabarit de rapport. Doc Steam : branche par défaut publiée à la main, branches bêta à mot de passe optionnel ; retour arrière non explicité (supposé : « Set Build Live » d'un ancien build).
  - Preuve : vu : grep `board.md`, `backlog.md`, `FACTORY.md` ; partner.steamgames.com/doc/store/application/branches.
  - Recommandation : branches `default`, `beta`, `interne` ; ID du dernier build sain dans `journal.md` ; e-mail, Discord, bouton « Signaler » en jeu (version, fragment, journal zippé) ; FAQ : erreur 4551 (Smart App Control), DX12, VRAM.
  - Effort : S
- **[Majeur] Astreinte solo : sévérités, runbook, routine absents**
  - Constat : 8 dossiers UECC en 25 h en développement ; 85 tâches `review` attendent un seul humain.
  - Preuve : vu : `Saved/Crashes`, `board.md`.
  - Recommandation : S1 (crash au lancement, sauvegarde perdue) : build sain < 1 h ; S2 (≥ 2 % des sessions) : hotfix ≤ 72 h (audit 89) ; deux triages de 20 min par jour, 14 jours ; routine d'agent : top 5 des crashs → `BUG-NNN`.
  - Effort : S

## Manques
1. Crash : stagé, adresse, symboles par version (lot L9, audit 89).
2. Version affichée, résumé technique (§18).
3. `ue.ps1 release`/`patch`, archive `Releases/<v>`.
4. Page de support, FAQ, runbook d'incident.

## Quick wins
1. BugSplat ou Sentry gratuit, `IncludeCrashReporter=True`, `-CrashReporter` : crash volontaire sur le package, pile lisible (1 j).
2. `ue.ps1 release <v>` : tag, `ProjectVersion`, build, PDB hors dépôt, ligne `journal.md`, version et `SetGameData` au démarrage (1 j).
3. `docs/operations/runbook.md` : sévérités, retour au build sain, modèle d'annonce, FAQ (0,5 j).
4. Filtre retirant `User:`, `Computer:` et chemins d'un journal avant envoi (1 h).

## Questions qui challengent l'équipe
1. Samedi 23 h, un crash au lancement touche 30 % des joueurs : qui remet le dernier build sain en ligne, en combien de minutes, l'avez-vous répété ?
2. Que contient un rapport de crash (EpicAccountId, MachineId, chemins) et l'envoyez-vous sans consentement ?
3. Un correctif de valeur doit-il coûter 1,9 Go ? Quel mécanisme figerez-vous avant la 1.0 : il ne pourra plus s'ajouter après ?
