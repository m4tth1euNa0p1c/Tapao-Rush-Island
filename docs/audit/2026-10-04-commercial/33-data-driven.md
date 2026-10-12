# 33 — Valeurs de gameplay : DataAsset ou en dur
Domaine : D · Auditeur : ingénieur data

## Verdict
Le rangement est bon : 35 classes DataAsset (environ 800 propriétés), 18 `constexpr`. Mais personne ne sait quelles valeurs le jeu joue : elles vivent dans des `.uasset` binaires qui contredisent défauts C++, fiches et tests. Risque commercial : équilibrer à l'aveugle, avec des tests verts sur d'autres chiffres que ceux livrés.

## Constats

- **[Bloquant commercial] Les valeurs jouées ne sont écrites nulle part**
  - Constat : Unreal ne sauvegarde que l'écart au défaut C++ (`DA_RunRules` : 1,2 Ko, rien de surchargé). Décodage des `.uasset` : au moins 26 propriétés divergent des en-têtes, p. ex. `InitialFloodDelay` 300 (120), `RowForceN` 14 000 (3 400), `LootRules.Count` 5 000 (1 200), `MaxStamina` 160 en BP (100). Les audits 01, 07, 11, 15 : cas isolés.
  - Preuve : `ExpeditionClockSpec.cpp:45` attend 120 sur un objet par défaut ; 20 `NewObject<U…Rules>()` en test, aucun `DA_*` chargé ; `import_loot.py:164-166` réécrit `Count` 1 200 à chaque exécution.
  - Recommandation : export JSON versionné des valeurs effectives ; spec qui charge chaque `DA_*` et vérifie le cahier ; supprimer les défauts qui mentent.
  - Effort : M

- **[Majeur] Taxe 35 % et bonus 100 : défauts compilés, valeurs en double**
  - Constat : `URunRules::Economy` est vide (aucun `DA_EconomyRules`, cf. 05) : le snapshot garde ses défauts compilés. Chaque valeur de §24.6 existe dans une DA et dans `FRunRulesSnapshot`.
  - Preuve : `RunTypes.h:259,262` ; `RunSpec.cpp` (snapshot par défaut).
  - Recommandation : créer et lier `DA_EconomyRules` ; défauts du snapshot à zéro avec `ensure`.
  - Effort : S

- **[Majeur] Doublons et replis qui dérivent**
  - Constat : `ZonesCount` (plafonne horloge et HUD) ignore `ZoneCount` de l'île ; `MaxFragmentIdLength` et `HiddenDepth` existent en DA et en `constexpr`, le second couplé à `BushMinZ` sans contrôle ; 19 replis `Rules ? Rules->X : littéral`, deux déjà faux (`MaxClipRate` 1,6 contre 4,0 ; `CorpseDelay` 1,5 contre 2,5) ; mots-clés de maillages copiés dans 3–4 DA.
  - Preuve : `BoatStartRules.h:43`/`ExpeditionIslandValidation.h:23` ; `SiteClearing.cpp:12`/`VillageRules.h:166` ; `TikiCreature.cpp:382,590`.
  - Recommandation : une source par valeur ; repli = `ensure`.
  - Effort : S

- **[Majeur] Objets : valeur et masse dans 23 Blueprints, culling réseau dans chacun**
  - Constat : `BaseValue` et `MassKg` sont posés par BP (butin 5–40, totem 150, perle 1 500), `NetCullDistanceSquared` (80 m) dans chaque BP de butin. La source réelle est un dict Python que la fiche contredit (1–6 $). Ni identifiant ni catégorie, que `FRunSale` exige. Équilibrage : voir 05.
  - Preuve : `ValuableComponent.h` (défaut 100) ; `import_loot.py:27` ; `loot-common.md`.
  - Recommandation : `UItemDefinition` (Id, Category, Value, Mass, NetCull, poids) lue par les BP.
  - Effort : M

- **[Majeur] Ordre d'initialisation codé par des délais**
  - Constat : camps 1,0 s, galion 2,0 s, butin 2,5 s, gardiens 3,0 s avec plancher codé 2,1 s : un invariant d'ordre réparti dans 4 DA et du code. Un hôte lent le casserait sans erreur (supposé).
  - Preuve : `CampRules.h:180`, `LootRules.h:73`, `TikiRules.h:82`, `TikiDirector.cpp:140-144`.
  - Recommandation : événement `OnFragmentReady` et phases explicites.
  - Effort : M

- **[Majeur] Environ 350 littéraux de gameplay en dur (ARC-03)**
  - Constat : scan de 44 `.cpp` : seuils IA 60/70/80 cm ; le défenseur lâche prise à `8.0` s mais l'embusqué lit `AmbushIdleTime` ; élan vertical des coups 60, 80, 120, 150 à côté d'un `KnockbackSpeed` en DA ; tolérances serveur du sabre 0,12 et 0,15 s non réglables. Le kit joueur est sur composants (Movement 31 propriétés, Grab 16, Combat 12), pas en DA.
  - Preuve : `TikiVillager.cpp:233-256` ; `TikiWarrior.cpp:143` ; `CutlassWeapon.cpp:41-44`.
  - Recommandation : `DA_PlayerRules` ; `KnockbackLift`, `ArrivalTolerance` et tolérances réseau en règles.
  - Effort : M

- **[Majeur] Aucune validation de données**
  - Constat : `IsDataValid` n'existe que sur 2 des 35 classes (error-handling.md §1 l'exige). Ni min ≤ max, ni `HiddenDepth > |BushMinZ|`, ni grille d'inondation de 2,56 km contre la taille des fragments.
  - Preuve : `ExpeditionIslandDefinition.cpp:44`, `IslandCatalog.cpp:28`.
  - Recommandation : base `UMonkeRulesBase::Validate()` et spec de cohérence croisée.
  - Effort : M

- **[Majeur, supposé] DA chargées par chemin texte, sans règle de cuisson**
  - Constat : 7 DA chargées par chemin (bruits de pas, inondation, vagues, `DA_TapaoUISkin`…), plus `BP_Barque` et `BP_Shark` ; ni dossier toujours cuit ni réglage de packaging. Sans skin, les écrans TAPAO sont éteints. Jamais packagé : supposé.
  - Preuve : `FootstepSubsystem.cpp:28`, `TapaoUISubsystem.cpp:84`, `SharkActor.cpp:416` ; `DefaultGame.ini`.
  - Recommandation : `UDeveloperSettings` « règles du jeu » en `TSoftObjectPtr`, dossier toujours cuit, `ensure` au démarrage.
  - Effort : S

## Manques
1. Tableau d'équilibrage généré (valeur d'une île contre quota) et `DA_PlayerRules`.
2. Stats par archétype tiki (un seul bloc `Warrior*`) ; consommateur de `DifficultyStep`.
3. Empreinte des DA vérifiée entre hôte et clients : vent et ciel sont recalculés localement.

## Quick wins
1. Créer `DA_EconomyRules`, le lier, défauts du snapshot à zéro.
2. Exporter les valeurs effectives en JSON, commiter les 26 écarts.
3. Supprimer les doublons `MaxFragmentIdLength`, `HiddenDepth` et les 2 replis faux.
4. Dossier toujours cuit pour `Data` ; `import_loot.py` sans `count` ni `min_spacing`.

## Questions qui challengent l'équipe
1. Quand le cahier dit 120 s et le `.uasset` 300 s, qui fait foi, et où est-ce écrit ?
2. Si je change un défaut dans un en-tête, savez-vous quels assets changent de comportement ?
3. Comment saurez-vous que le premier build packagé joue les règles de l'éditeur ?
