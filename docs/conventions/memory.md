# Normes mémoire

Couvre la durée de vie des objets, les allocations, le chargement des assets et les budgets.

**Références** :
- documentation Epic : *Unreal Object Handling* et ramasse-miettes, *Memory Insights*, *Low-Level Memory Tracker*, *Asset Management* ;
- Epic Coding Standard (pointeurs intelligents) ;
- C++ Core Guidelines, section R (gestion des ressources), adaptée aux UObject.

## 1. Durée de vie des UObject (ramasse-miettes)
| Cas | Règle |
|-----|-------|
| Membre qui possède ou garde un UObject | `UPROPERTY()` + `TObjectPtr<T>`. Sans `UPROPERTY`, l'objet peut être ramassé, ce qui donne un crash ou un comportement indéfini. |
| Référence non possédante qui peut disparaître | `TWeakObjectPtr<T>`, vérifiée (`IsValid()` / `Get()`) à chaque usage. |
| UObject tenu par une classe non-UObject | `FGCObject` avec `AddReferencedObjects` + `GetReferencerName`, ou `TStrongObjectPtr<T>` |
| Acteur ou composant qui peut être détruit | Tester avec `IsValid(Obj)` (couvre aussi les objets en attente de destruction), pas avec `!= nullptr`. Effacer les références à la destruction (cahier §22). |
| Création | `NewObject`, `SpawnActor`, `CreateDefaultSubobject`, jamais `new` sur un UObject. Hors UObject : `TUniquePtr` / `TSharedPtr` via `MakeUnique` / `MakeShared`, jamais de `new` / `delete` à la main. |
| Racine | `AddToRoot()` seulement avec un `RemoveFromRoot()` symétrique et commenté. Jamais de UObject dans une variable statique. |

**Délégués, timers et lambdas** :
- capturer `this` uniquement par une liaison faible : `CreateWeakLambda(this, …)`, `AddUObject`, `FTimerDelegate::CreateUObject` ;
- tout retirer dans `EndPlay` ou `Deinitialize` : `RemoveAll(this)`, `GetWorldTimerManager().ClearAllTimersForObject(this)`.

## 2. Allocations et conteneurs
- **Aucune allocation dans un chemin appelé à chaque image** :
  - `Reserve` ;
  - `TInlineAllocator<N>` pour les petits tableaux ;
  - `Reset()` plutôt que `Empty()` quand on réutilise un conteneur ;
  - `TArrayView` ou `TConstArrayView` pour passer une vue sans copier.
- **Chaînes** : pas de `FString` temporaire ni de `Printf` à chaque image. `FName` pour les identifiants, `FText` pour ce qui s'affiche.
- **Volumes bornés** :
  - données répliquées, sauvegardes et projets d'île (cahier §20 : 500 éléments, 30 ennemis, 5 zones) ;
  - tableaux venus du réseau ou du disque, dont la taille est vérifiée avant l'allocation (`security.md`).

## 3. Assets et chargement
- **Coût d'une référence dure** : elle charge l'asset avec son référent. Entre domaines et pour le contenu lourd (meshes, textures, sons, niveaux), utiliser `TSoftObjectPtr` ou `TSoftClassPtr`, chargés en asynchrone par `UAssetManager::GetStreamableManager().RequestAsyncLoad`.
- **Chargement synchrone** : pas de `LoadObject` ni de `LoadSynchronous` pendant le jeu, à cause des saccades. Seule exception : l'écran de chargement d'un niveau.
- **Primary Assets** : items, îles, contrats et skins sont des `UPrimaryDataAsset` gérés par l'Asset Manager (chargement par bundles).
- **Contrôle** dans l'éditeur avec *Reference Viewer* et *Size Map* : un Blueprint de gameplay ne doit pas entraîner un domaine entier.

## 4. Budgets
Ce sont des propositions, à valider au L0 sur la machine de référence.

| Poste | Budget |
|-------|--------|
| RAM du processus (build packagé, scénario §12 à 4 joueurs) | ≤ 8 Go |
| VRAM en 1080p, qualité haute | ≤ 6 Go (cible : GPU de 8 Go) |
| Pool de streaming des textures | réglé pour qu'aucun avertissement *Texture streaming pool over budget* n'apparaisse dans le scénario de référence |
| Fuite | aucune : RAM stable à ± 2 % après 3 cycles hub → expédition → hub |
| Texture de prop | 2K au maximum ; 4K seulement par ADR (terrain, objets héros) |
| Personnage (§10) | 30 000 à 60 000 triangles en LOD0, textures 2K, 2 à 4 matériaux |

## 5. Mesurer
- **Instantané** : `memreport -full` en console, ou `-ExecCmds="memreport -full"` au lancement. Le rapport va dans `Saved/Profiling/MemReports/`.
- **Par catégorie** : LLM (Low-Level Memory Tracker), en lançant avec `-llm` puis `stat LLM` ou `stat LLMFULL`. Ajouter `-llmcsv` pour une courbe dans le temps.
- **Allocations** : Memory Insights, en lançant avec `-trace=default,memory` (canaux memtag, memalloc, callstack et module), puis ouvrir le `.utrace` dans Unreal Insights.
- **Objets** : `obj list class=<Classe>` pour les compter. En développement seulement, `gc.CollectGarbageEveryFrame 1` force le ramasse-miettes pour faire sortir les erreurs de durée de vie.
- **Fuites** : comparer deux `memreport`, ou deux points LLM, après N cycles identiques.
- **Où mesurer** : les chiffres de référence viennent d'un build packagé (Development ou Test). Les chiffres de l'éditeur ne sont qu'indicatifs.

## 6. Contrôles de l'audit mémoire (`/ue-memory-audit`)
| ID | Contrôle | Sévérité par défaut |
|----|----------|---------------------|
| MEM-01 | Pointeur membre vers un UObject sans `UPROPERTY` (ni `FGCObject` ni `TStrongObjectPtr`) | CRITIQUE |
| MEM-02 | Pointeur brut gardé d'une image à l'autre vers un objet qui peut être détruit, au lieu d'un `TWeakObjectPtr` | MAJEUR |
| MEM-03 | Délégué, timer ou lambda qui capture `this` sans liaison faible, ou qui n'est pas retiré en `EndPlay` | MAJEUR |
| MEM-04 | `AddToRoot` sans `RemoveFromRoot`, ou UObject statique | MAJEUR |
| MEM-05 | Allocation ou chaîne temporaire dans un chemin appelé à chaque image | MINEUR |
| MEM-06 | Chargement synchrone en jeu | MAJEUR |
| MEM-07 | Référence dure vers du contenu lourd entre domaines | MAJEUR |
| MEM-08 | Taille non bornée venue du réseau, du disque ou d'un joueur | MAJEUR |
| MEM-09 | Budget dépassé ou fuite mesurée | MAJEUR |
