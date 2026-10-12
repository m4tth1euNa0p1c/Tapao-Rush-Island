# 35 — Système de sauvegarde
Domaine : D · Auditeur : ingénieur sauvegarde

## Verdict
La sauvegarde active (quatre compteurs) n'est ni atomique, ni vérifiée, ni versionnée; la version robuste (T-057) dort sur une branche et vise le hub v1.2, pas les manches de l'ADR-005. Le butin d'une expédition réussie, les migrations et Steam Cloud ne sont pas traités. Risque commercial : avis « partie perdue » dès la sortie.

## Constats

- **[Majeur] Chemin actif non atomique; le robuste dort sur une branche**
  - Constat : `SaveGameToSlot` écrit sur le fichier final, sans somme de contrôle; le chargement ignore l'erreur d'archive (objet partiel possible). `dev/T-057` (3 213 lignes : CRC32, fichier temporaire, `.bak`, 23 specs jamais lancées) n'est pas dans main; son `SG_Campaign` est le hub v1.2 (or, réserve, équipement).
  - Preuve : `TapaoUISubsystem.cpp:654-694`; moteur `SaveGameSystem.h:151`, `GameplayStatics.cpp:2457-2489`; `git merge-base --is-ancestor dev/T-057 main` : non.
  - Recommandation : fusionner T-057 en INT, lancer `Save.*`, brancher le menu dessus (UI05, voir 23); T-115 en tire `SG_Run`.
  - Effort : M

- **[Majeur] Marqueur armé pendant la vente : un crash détruit un butin réussi**
  - Constat : posé au lever d'ancre, `ExpeditionInProgress` n'a pas de fin explicite : §17 dit « à la résolution », soit la signature (supposé), alors que la cale rapportée n'existe qu'en mémoire hôte. Crash, départ de l'hôte ou chargement raté de la map de 110 Mo (supposé) : « rentrée vide, morts facturés ». En manche 1, cale vide : défaite certaine, partie close.
  - Preuve : cahier §17 l.335, §24.2 étape 7, §24.7; ADR-005 §5, §10; audit 09.
  - Recommandation : au figeage de la cale, une transaction écrit cale rapportée, phase Vente et morts, puis vide le marqueur. La manche ne se rejoue pas; seule la vente reprend, idempotente par `ItemId`.
  - Effort : M

- **[Majeur] État à sauvegarder incomplet; les types de T-109 s'écriraient vides**
  - Constat : `FRunState` (ADR-005 §3, absent du code) omet achats de boutique (§7 l.171 les prévoit), équipement par joueur, effectif au lancement (frais, échelle du quota) et phase. Aucune propriété de `RunTypes.h` n'a `SaveGame` : l'archive de T-057 les ignorerait, trésorerie comprise, sans erreur.
  - Preuve : `RunTypes.h:203-226`; `SaveSubsystem.cpp:935-938`; moteur `Property.cpp:1052`.
  - Recommandation : fiche `save-run.md` (champs v1) avant T-114; drapeaux `SaveGame`; spec d'aller-retour (RUN12).
  - Effort : S

- **[Majeur] Versionnage : pas de copie d'avant migration, références par chemin**
  - Constat : `.bak` tourne à chaque écriture : la copie d'avant migration disparaît à la 2e sauvegarde (§16 en exige une). Le moteur écrit `/Script/monke_island.TapaoSaveGame` en en-tête : renommer le module (nom provisoire) orpheline ces fichiers; T-057 évite ce piège. `FHoldEntry` (prévu, ADR-005 §5) référence sa classe par chemin : objet renommé, objet perdu.
  - Preuve : `SaveFileFormat.cpp:336`; `GameplayStatics.cpp:140-148`.
  - Recommandation : garder `<id>.v<N>.bak` avant la première écriture migrée; `ItemTypeId` (FName d'un catalogue DA), objet inconnu valorisé à sa valeur sauvée; fixtures `.sav` N-1 en test.
  - Effort : M

- **[Majeur] Steam Cloud : dossier lié au nom du projet, rien de configuré**
  - Constat : aucun plugin Steam (inutile pour Auto-Cloud, supposé). En Shipping, `Saved/` est sous `%LOCALAPPDATA%\<nom du .uproject>`; en Development, dans le dossier d'installation. Renommer le projet orphelinerait sauvegardes, réglages et motifs Cloud.
  - Preuve : `Build.cs:48-51`; moteur `App.cpp:218-219`, `Paths.cpp:451-470`.
  - Recommandation : figer `UE_SAVED_DIR_OVERRIDE` avant toute bêta; Auto-Cloud racine `WinAppDataLocal`, `*.sav` seuls (ni `.tmp`, `.bak`, ni `GameUserSettings.ini`), historique ≤ 50 fichiers; test sur 2 PC.
  - Effort : S

- **[Mineur] Écriture sans reprise sous verrou; copie cliente interdite**
  - Constat : `Move` avec remplacement supprime puis déplace; T-057 compense en relisant trois candidats (correct) mais ne réessaie jamais : un verrou passager de Defender ou Steam (supposé) refuserait un départ. `NotHost` bloque aussi le profil et la copie de partie voulus par 14 et 27.
  - Preuve : `FileManagerGeneric.cpp:299-323`; `SaveFileFormat.cpp:336,347,359`; `SaveSubsystem.cpp:460-469`.
  - Recommandation : 3 essais de 100 ms par `Move`; droits par domaine (partie : hôte; profil, réglages : partout).
  - Effort : S

- **[Mineur] Réglages et autosave : sans version ni confirmation**
  - Constat : `UMonkeUserSettings` n'a que `bSaved`; `Apply` enregistre aussitôt, sans retour automatique à 15 s (§18), et l'INI est écrit directement en jeu. L'autosave de 30 s tourne en expédition, contre §24.7.
  - Preuve : `SettingsSubsystem.cpp:167-182,236`; `ConfigCacheIni.cpp:529-557`; `TapaoUISubsystem.h:75`.
  - Recommandation : `SettingsVersion`, confirmation vidéo 15 s, spec INI tronqué; supprimer le minuteur, écrire sur événements.
  - Effort : S

## Manques
1. `SG_LocalProfile` (voir 14).
2. Écran Charger, indicateur « Sauvegarde… / Échec », `WBP_ErrorDialog` : rien dans main.
3. Test d'injection de panne (processus tué en écriture), fixtures N-1, Auto-Cloud vérifié sur deux PC.

## Quick wins
1. Compiler `dev/T-057` en cible jeu, lancer ses 23 specs.
2. `GetSave` : mettre un fichier illisible de côté (`.corrupt`) au lieu de l'écraser; griser CONTINUER.
3. Retirer l'autosave en île; remplacer « La partie est sauvegardée » (`TapaoUISubsystem.cpp:604`) par « quitter en expédition = expédition perdue ».
4. `SaveGame` sur `RunTypes.h` + spec d'aller-retour; `UE_SAVED_DIR_OVERRIDE` dans les `Target.cs`.

## Questions qui challengent l'équipe
1. Un crash de l'hôte pendant la vente doit-il coûter tout le butin, donc la partie en manche 1 ? Sinon, quand le marqueur est-il vidé ?
2. Jusqu'à quelle version l'accès anticipé garantit-il de charger une partie ?
3. Qui fige le nom du jeu, du module et du dossier de sauvegarde avant la première bêta ?
