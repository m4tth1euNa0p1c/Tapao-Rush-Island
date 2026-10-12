# 48 — Build, cook et packaging
Domaine : E · Auditeur : ingénieur release

## Verdict
Aucun package n'existe, et le cook actuel livrerait un jeu muet : sept assets vitaux (menu, barque, règles de mer, pas, ressac, vent, réglages) sont chargés par chemin texte, donc non cuits, sans trace en Shipping. Le correctif, vérifié dans les sources du cooker 5.8, est surtout de la configuration : une journée. Risque commercial : le découvrir chez un testeur, sur un exécutable non signé que Smart App Control peut bloquer.

## Constats
- **[Bloquant commercial] Sept assets chargés par chemin texte échappent au cook**
  - Constat : le cooker suit références dures, souples et chemins d'ini, pas les chaînes C++. Zéro référence à `DA_TapaoUISkin` (+58,7 Mo d'UI), `BP_Barque`, `DA_WaveRules`, `DA_FootstepRules`, `DA_ShoreAmbienceRules`, `DA_WindLeavesRules`, `DA_SettingsRules` : menu absent, barque C++ nue, mer plate, sons muets. Corrige 33 et 37 : `DA_BoatStartRules`, `DA_HudTimerRules` (ini), `DA_FloodRules`, `BP_Shark` sont cuits.
  - Preuve : vu : octets de 2 011 paquets ; `SoftObjectPath.cpp:693` ; `TapaoUISubsystem.h:62-63` ; `BoatSubsystem.cpp:102-106` ; jamais cuit, non éprouvé.
  - Recommandation : `+DirectoriesToAlwaysCook` (UI/Tapao, Boat, Settings, Player/Footsteps, Environment/Shore, Water, Wind), puis contrôle dans `Saved/Cooked` : une spec d'éditeur passerait.
  - Effort : S
- **[Bloquant commercial] Aucune carte déclarée au packaging**
  - Constat : pas de section `ProjectPackagingSettings`. `L_MainMenu` est cuite (carte par défaut), `Lvl_Raraka` par les `DA_Island_*`, `L_Boot` par le seul `RestartMap` de `DA_ExpeditionRules`. Le cook reste sélectif grâce aux 21 DA AlwaysCook ; sinon `Cook.CookAllByDefault` cuirait tout `/Game`. ADR-004/005 ajoutent `L_Galleon`, `L_Transit`, `L_Frag_*`, retirent `L_Boot`.
  - Preuve : `CookOnTheFlyServer.cpp:257,8948` ; cahier §11 L6, §21 l.433 (« une map chargée dynamiquement doit être présente »).
  - Recommandation : type primaire `Map` (AlwaysCook, `/Game/MonkeIsland/Maps`, récursif), méthode conseillée par le cooker (`:8544`), plus `+MapsToCook` des trois cartes actuelles.
  - Effort : S
- **[Majeur] Le plugin MCP expérimental est dans la cible Game**
  - Constat : `ModelContextProtocol` (modules Runtime, NoRedist) et `MCPClientToolset` n'ont pas de `TargetAllowList`, contrairement aux 11 autres outils. Confirme 31.
  - Preuve : `monke_island.uproject:34-37,108-111` ; vu : reçu `Binaries/Win64/monke_island.target`, les deux plugins dans `BuildPlugins`, 235 plugins actifs.
  - Recommandation : `TargetAllowList: ["Editor"]` ; archive sans `ModelContextProtocol*` ni `UnrealEditor*` ; élaguer ensuite les plugins inutiles.
  - Effort : S
- **[Majeur] `ue.ps1 package` s'écarte de l'éditeur ; Shipping jamais compilé**
  - Constat : sans `-nodebuginfo`, UAT copie les `.pdb` (377 Mo en Development) ; sans `-iostore`, il produit des `.pak` classiques. Ni garde « éditeur ouvert », ni contrôle post-cook, ni log Shipping. `Intermediate` ne contient que `Development` ; collisions de build unity déjà vues (31). Sains : tests sous `WITH_AUTOMATION_TESTS`, commandes sous `!UE_BUILD_SHIPPING`.
  - Preuve : `ue.ps1:138-145` ; `TurnkeySupportModule.cpp:488-499,574-577` ; `CopyBuildToStagingDirectory.Automation.cs:3043` ; `security.md:79`.
  - Recommandation : ajouter `-nodebuginfo -iostore`, archiver `build/symbols/<version>`, vérifier le package par script ; `ue.ps1 build -Config Shipping` avant le cook.
  - Effort : S
- **[Majeur] Identité, version et signature absentes**
  - Constat : copyright par défaut du moteur (« Fill out your copyright… »), société vide, version 1.0.0.0, ni icône ni splash. La somme réseau = nom + version : deux builds différents se connectent (supposé : désynchronisation). Smart App Control a bloqué ici des DLL neuves (erreur 4551), désactivé depuis.
  - Preuve : `DefaultGame.ini:10-11` ; `BaseGame.ini:74-81` ; `NetworkVersion.cpp:241-254` ; `security.md:78`.
  - Recommandation : renseigner `GeneralProjectSettings`, incrémenter la version par script, vérifier SAC sur le 2e PC, prévoir un certificat avant l'accès anticipé.
  - Effort : M
- **[Majeur] Taille, temps et place du premier cook**
  - Constat : 1 278 Mo, 2 011 assets, 88 % atteignables (textures 530 Mo, meshes 188, DataTables 168, carte 110 ; Mannequins du template 94). Supposé : build 1,3–2 Go, premier cook 30–90 min, puis 10–25 min. D3D11 SM5, ray tracing et Substrate alourdissent. Disque : 39 Go libres, `.worktrees` 46 Go.
  - Preuve : `Content/` mesuré ; `DefaultEngine.ini:32,36,50`.
  - Recommandation : retirer D3D11 SM5 (Nanite et Lumen exigent SM6), trancher le ray tracing, purger les worktrees, chronométrer dans `journal.md`.
  - Effort : S

## Manques
1. **Checklist du premier build Shipping** (absente : `release-checklist.md` ignore cook, symboles, signature).
   1. Éditeur et pairs arrêtés, 15 Go libres ; configuration ci-dessus, `CulturesToStage=fr,en`.
   2. `ue.ps1 build -Config Shipping` : zéro erreur, unity compris.
   3. `package -Config Development` d'abord (journaux lisibles) : zéro `Error:`, lire les `Warning` `LogCook`.
   4. Contrôle : `Saved/Cooked/Windows/monke_island/Content/MonkeIsland` contient cartes, `DA_TapaoUISkin`, `BP_Barque`, DA de règles, `Data/Islands`.
   5. Smoke packagé `-log` : menu, JOUER, barque, vagues, sons, réglages d'affichage, écran de chargement MoviePlayer, PCG non régénéré.
   6. `package -Config Shipping` : ni `.pdb`, ni MCP ; console inerte ; taille et durée relevées.
   7. Symboles archivés, version incrémentée, `journal.md` « testé build ».
   8. Signature, puis test sur le 2e PC.
2. Vérificateur de package et procédure d'installation (cahier §11 L6).

## Quick wins
1. Section `ProjectPackagingSettings` (Map AlwaysCook, MapsToCook, DirectoriesToAlwaysCook) : 1 h.
2. `TargetAllowList: ["Editor"]` sur les deux plugins MCP : 5 min.
3. `ue.ps1 package` : `-nodebuginfo -iostore`, garde éditeur : 1 h.
4. `ue.ps1 build -Config Shipping`, corriger : 0,5 j.
5. Société, titre, copyright, version, icône : 1 h.

## Questions qui challengent l'équipe
1. `Lvl_Raraka` (110 Mo, 816 000 instances) part-il tel quel en accès anticipé, et quelle taille de téléchargement acceptez-vous ?
2. Qui signe les binaires, avec quel certificat, avant quelle date ?
3. Le nom `monke_island` (exe, `Saved/`) est-il définitif ? Renommer après la sortie orpheline sauvegardes et réglages (35).
