# Usine agentique monke_island / Crownfall Expeditions

Charte d'organisation des agents Claude. Contrat produit : `docs/CAHIER_DES_CHARGES.md` (adoption : `docs/adr/ADR-001-cahier-des-charges-crownfall.md`). Ce fichier dit **qui** travaille, **où**, **dans quel ordre** et **comment le travail passe d'un environnement à l'autre**. Il ne change aucune règle du cahier.

## 1. Principe : des rôles finis, des instances illimitées
- **Rôle** = un fichier `.claude/agents/<role>.md` (29 rôles). Le catalogue reste petit et stable.
- **Instance** = un agent lancé sur **une** tâche du chantier : `<role>#<T-NNN>` (ex. `ue-gameplay-programmer#T-042`). Une tâche = une instance = un résultat vérifiable. Il peut y avoir autant d'instances que de tâches : 10, 1 000 ou 10 000 sur la durée du projet.
- **Cellule** = groupe de rôles qui possède un domaine du cahier, ses dossiers et ses assets (§3). Une cellule a un lead qui découpe et relit.
- **Vague** = lot d'instances lancées en parallèle par la session principale (skill `/factory wave`). La taille d'une vague est bornée par les **ressources exclusives** (§5), pas par le nombre d'agents.

L'échelle se gagne en découpant finement (tâches petites, fichiers disjoints, contrats publics définis avant le code), pas en lançant des agents sur le même périmètre.

## 2. Environnements : DEV → REC → INT → PROD
| Env | Où | Qui | Entrée | Sortie (gate) | Statut chantier |
|-----|----|-----|--------|----------------|-----------------|
| **DEV** | code, docs, scripts : worktree `.worktrees/T-NNN`, branche `dev/T-NNN` (`python scripts/worktree.py new T-NNN`) ; assets : arbre principal + éditeur partagé | instance du rôle assigné | tâche `a-faire` avec fiche §13 complète (lot, dépendances, fichiers autorisés, contrat, recette, preuve) | `python scripts/worktree.py build T-NNN` à 0 erreur (UBT du worktree, éditeur ouvert ou non), tests du domaine verts, auto-contrôle conventions, commits sur `dev/T-NNN` | `en-cours` → `review` + note `env=rec ; branche=dev/T-NNN` |
| **REC** (recette) | même branche, éditeur partagé ou headless | `ue-qa-tester` (tests + recette du cahier), `ue-code-reviewer` (lecture seule), `ue-network-engineer` si code serveur, `ue-security-engineer` si RPC / fichiers / sauvegarde / dépendance, `ue-reliability-engineer` si opération faillible (fichier, async, réseau, flux §22) | `review` + preuves DEV | critères d'acceptation + IDs de recette du cahier passés, review sans CRITIQUE/MAJEUR, revues réseau / sécurité / fiabilité signées quand requises | `review` + note `env=int` (ou retour `en-cours` avec la liste des écarts) |
| **INT** (intégration) | `main` dans l'arbre principal (`python scripts/worktree.py merge T-NNN`) | `ue-integration-lead` (+ `ue-editor-integrator` pour les assets, `ue-build-engineer` si le build casse) | tâches passées en REC | fusion sans conflit, build complet 0 erreur, **toute** la suite `monke_island.*` verte, PIE solo puis hôte + client sans erreur ni ensure | `fait` + note `env=int niveau=testé éditeur` |
| **PROD** | `main` + tag `L?-vN` | `ue-integration-lead`, `ue-build-engineer` (`/ue-package`), `ue-qa-tester`, `ue-performance-engineer`, `ue-memory-engineer`, `ue-architect` (revue d'architecture), `ue-security-engineer` (avant tout build distribué) | lot complet en INT | recette du **lot** (§11/§12/§23) passée sur build Windows packagé hors éditeur, release checklist, perf et mémoire mesurées contre les budgets, revue d'architecture sans CRITIQUE/MAJEUR ouvert, problèmes connus listés | lot validé dans `docs/production/journal.md`, tag |

Règles :
- Pas de saut d'environnement. Un retour arrière renvoie à DEV avec les écarts nommés.
- Le niveau de preuve est toujours écrit : `compilé` / `testé éditeur` / `testé build` / `testé multi-machines`.
- `fait` = sorti de INT. Un lot est « Validé » seulement après PROD.
- **Mode worktree (actif depuis le 2026-10-03, dépôt Git + LFS local, commit `47b1bdb`)** :
  - **Worktree léger** : `scripts/worktree.py new T-NNN` crée `.worktrees/T-NNN` sur `dev/T-NNN`, en sparse-checkout code + docs (`Source/`, `Config/`, `.uproject`, `docs/`, `scripts/`, `.claude/`), sans `Content/` ni téléchargement LFS : quelques secondes et quelques Mo. `--full` seulement si la tâche doit lire des assets.
  - **Compilation sans fermer l'éditeur** : `worktree.py build T-NNN` compile le module avec l'`Intermediate/` du worktree. Tant que l'éditeur est ouvert, UBT refuse la cible `monke_islandEditor`, même dans un worktree : le mutex Live Coding est global au moteur installé. On compile donc la **cible jeu** `monke_island` (environ 40 s à froid). Elle vérifie aussi ce qui casserait un build packagé (code `WITH_EDITORONLY_DATA`, collisions de noms en build unity). La preuve `compilé` de DEV ne réquisitionne plus l'éditeur partagé ; seule la reconstruction éditeur de l'arbre principal après fusion (INT) le demande.
  - **Assets** : `.uasset` et `.umap` restent dans l'arbre principal, modifiés via l'éditeur partagé (ressource exclusive §5), avec le jeton `owner=<asset>` dans la note du chantier. `git lfs lock` exige un serveur LFS distant : le dépôt est local, donc pas de verrou Git tant qu'il n'y a pas de remote.
  - **Commits** : chaque instance commite **ses** fichiers sur sa branche, préfixe de domaine et ID de tâche (`gameplay: … (T-NNN)`). Sur `main`, on ne commite qu'après un build OK, et seulement ses propres fichiers : jamais de `git add -A` sur le travail des autres sessions.
  - **Fusion** (gate INT) : `worktree.py merge T-NNN` fait un `--no-ff` dans `main`, sur un arbre principal propre pour les fichiers touchés. Puis build complet et tests ; `worktree.py remove T-NNN` après `fait`.
  - **Repli mono-arbre** (petite correction urgente, ou tâche purement éditeur) : travail direct dans l'arbre principal, fichiers disjoints, commit ciblé des fichiers de la tâche. Les règles de gate ne changent pas.

## 3. Cellules (alignées sur le cahier §9, §13, §21)
| Cellule | Lead | Rôles | Possède (code `Source/monke_island/…`, contenu `/Game/MonkeIsland/…`) | Cahier | Lots |
|---------|------|-------|-------------|--------|------|
| **Direction** | `ue-producer` | `ue-architect`, `ue-game-designer`, `ue-narrative-designer` | `docs/` (backlog, sprints, specs, ADR) | §1–3, §13–15 | tous |
| **Noyau expédition** | `ue-architect` | `ue-gameplay-programmer`, `ue-network-engineer` | `Expedition/`, `Player/`, `Interaction/` ; GameMode/GameState/PlayerState/Controller | §3, §4, §6, §9, §22 | L0–L3 |
| **Objets & barque** | `ue-gameplay-programmer` | `ue-boat-programmer` (flottaison, rames, surcharge, sable, noyade), `ue-network-engineer`, `ue-editor-integrator` | `Items/`, `Boats/` ; `BP_Barque`, `DA_BoatRules` ; `BP_CarryableResource`, `BP_Rowboat`, `BP_MooringPoint`, `BP_BoatPlacementManager` | §5, §6.4, §6.5 | L1–L2 |
| **Réseau & physique** | `ue-network-engineer` | `ue-physics-animator`, `ue-code-reviewer` | revue de tout code serveur ; `Session/` (L7) | §9, §5 (réplication physique), §12 n°10 | L1–L7 |
| **Monde & dangers** | `ue-level-designer` | `ue-ai-programmer`, `ue-environment-artist`, `ue-tech-artist`, `ue-gis-cartographer` | `World/`, `AI/` ; maps `L_Island_*`, `BP_TideManager`, `BP_DayNightManager`, `BP_EncounterDirector`, ennemis, requin | §6.1–6.3, §8 | L0, L3, L5, L8 |
| **Systèmes : économie, sauvegarde, paramètres** | `ue-systems-programmer` | `ue-game-designer` | `Economy/`, `Save/`, `Settings/` ; `SG_*`, `DA_ExpeditionRules`, `DA_Item` | §7, §17, §18 | L4–L5 |
| **UI** | `ue-ui-developer` | `ue-narrative-designer` (textes), `ue-audio-designer` (sons UI) | `UI/` ; tous les `WBP_*`, `L_MainMenu` | §16, §18, §19 | L0, L4–L6 |
| **Personnages** | `ue-character-artist` | `ue-character-rigger`, `ue-physics-animator`, `ue-cosmetics-programmer` | `Characters/`, `Cosmetics/` ; `SK_*`, `ABP_ExpeditionCharacter`, `DA_*Skin_*` | §10 (+ ADR-002) | L1, L6 |
| **Art & audio** | `ue-tech-artist` | `ue-blender-artist`, `ue-environment-artist`, `ue-audio-designer` | `Art/`, `Audio/` ; matériaux, Niagara, props, MetaSounds | §1, §8, §19 | tous |
| **Hub & éditeur d'îles** | `ue-level-designer` | `ue-gameplay-programmer`, `ue-ui-developer`, `ue-systems-programmer` | `Hub/`, `IslandEditor/` ; `L_GalleonHub`, `L_EditorSandbox` | §16, §20 | L4, E1 |
| **Qualité & livraison** | `ue-integration-lead` | `ue-qa-tester`, `ue-code-reviewer`, `ue-build-engineer`, `ue-performance-engineer` | `Tests/`, packaging, branches `integration`/`main` | §12, §15, §23 | tous |
| **Qualité technique** (transverse) | `ue-architect` | `ue-performance-engineer`, `ue-memory-engineer`, `ue-security-engineer`, `ue-reliability-engineer`, `ue-code-reviewer` | normes `docs/conventions/`, `docs/architecture.md`, `docs/audits/`, `docs/perf/`, `docs/security/` ; revues transverses (architecture, perf, mémoire, sécurité, erreurs) ; le code des domaines reste à leur cellule (corrections via tâches) | §9, §12, §13, §17, §21, §22 | tous |
| **Tidemark** (plugin) | `tidemark-lead` | `tidemark-solver-programmer`, `tidemark-terrain-programmer`, `tidemark-network-engineer`, `tidemark-editor-programmer`, `tidemark-render-programmer`, `tidemark-qa`, `tidemark-release-engineer`, `tidemark-tapao-integrator`, `tidemark-mcp-developer` (V1.1) | `monke_island/Plugins/Tidemark/` (zéro include du jeu), `docs/produit/tidemark/`, `scripts/tidemark/` ; `Source/monke_island/Flood/` et `Water/` seulement par `tidemark-tapao-integrator` en S7 | `docs/produit/CAHIER_DES_CHARGES_TIDEMARK.md` (§10 plan, portes S2/S4/S5) | TM-01 → TM-43 |

Un dossier ou un asset hors du périmètre de la cellule demande l'accord du lead propriétaire (noté dans la fiche de tâche).

## 4. Matrice lot → cellules
| Lot | Cellules actives | Recette de sortie |
|-----|------------------|-------------------|
| L0 | Direction, Qualité (build, version moteur), Monde (map grise), UI (menu minimal), Noyau (squelette GameMode/State) | projet ouvre et compile ; cartes de démarrage |
| L1 | Noyau, Objets, Réseau, Personnages (pilote placeholder) | n°2, n°3 |
| L2 | Objets & barque (dont barque physique, ADR-003), Réseau, UI (cargaison) | n°5, n°6, n°19 + recette de `docs/features/boat-rowing.md` |
| L3 | Noyau, Monde & dangers, Réseau, UI (zone, timer), Art (eau, drapeau) | n°7, n°13–18 |
| L4 | Systèmes, Hub, UI | n°1, n°9, n°20, UI01–UI05, UI10 |
| L5 | Monde (nuit, requin), Systèmes (paramètres), UI (HUD, pause), Art & audio, Qualité (perf 4 joueurs) | n°10, n°11, n°21, UI06–UI09, UI11–UI12 |
| L6 | Qualité & livraison, Personnages | build hors éditeur, recette §10 |
| L7 | Réseau (sessions, VoIP) | NET01 |
| E1 | Hub & éditeur d'îles, UI, Systèmes | ED01–ED04 |
| L8+ | selon proposition chiffrée | recette propre à chaque lot |

## 5. Ressources exclusives et concurrence
| Ressource | Capacité | Règle |
|-----------|----------|-------|
| Éditeur Unreal + MCP `unreal` | 1 | une seule instance éditeur à la fois (jeton `owner=editor` dans la note du chantier) ; PIE bloque les sauvegardes |
| Build UBT de l'arbre principal (éditeur fermé) | 1 | seulement en INT, après fusion ; fermer l'éditeur = coordination avec la tâche qui le tient |
| Build UBT d'un worktree | N | `worktree.py build T-NNN` (cible jeu tant que l'éditeur est ouvert) ; les builds se sérialisent seuls (`-WaitMutex`) |
| MCP `blender` | 1 | une instance Blender à la fois |
| Une map `.umap` ou un Blueprint | 1 propriétaire | `owner=<asset>` dans la note du chantier (`git lfs lock` seulement le jour où un remote LFS existe) |
| Code C++ | N | un worktree par instance ; les conflits se règlent à la fusion (INT), donc des fichiers qui se recouvrent restent possibles mais coûtent une résolution |
| Docs, specs, scripts Python, données | N | parallèle si fichiers disjoints |

Taille de vague recommandée : 10 à 15 instances code et docs en worktrees, plus 1 tâche éditeur et 1 tâche Blender dans l'arbre principal. Les builds de worktree se sérialisent : au-delà de 15 compilations simultanées, la vague attend surtout UBT. En repli mono-arbre : 3 à 6 instances. Au-delà, enchaîner les vagues. Pas plus de 3 tâches `en-cours` par rôle et par cellule sans accord du lead.

## 6. Cycle d'une instance
1. La session principale crée le worktree (`python scripts/worktree.py new T-NNN`), puis lance l'agent avec : ID de tâche, env (`dev`), chemin `.worktrees/T-NNN`, fiche §13, propriétaire des ressources exclusives. Pas d'`isolation: "worktree"` de l'outil Agent : il ferait un checkout complet de `Content/` et du LFS (environ 3 Go par agent).
2. L'instance passe `en-cours`, travaille **uniquement** sur ses fichiers autorisés **dans son worktree**, compile avec `worktree.py build T-NNN`, commite sur `dev/T-NNN` et produit ses preuves.
3. Elle termine en `review` (`env=rec`) ou `bloque` (avec ce qu'il faut pour débloquer). Jamais `fait` directement depuis DEV.
4. `/factory-gate rec|int|prod <T-NNN|lot>` fait passer les gates.

## 6 bis. Vague rapide dans l'arbre principal (pratique du 2026-10-11)
Quand l'éditeur est ouvert et que le propriétaire y joue, la vague tourne dans l'arbre principal, sans worktree :
- **Consignes** : `.claude/factory/brief-template.md`. Chaque agent a ses dossiers à lui (aucun fichier partagé sans contrat écrit dans les consignes) et compile fichier par fichier (`scripts/factory/build_editor.sh --single`). Ce qui se fait dans l'éditeur est un script `scripts/editor/*_ue.py` que le lead lance.
- **Intégration en une commande** : `scripts/factory/integrate_wave.sh` (build complet, suite de tests, relance de l'éditeur, actions éditeur de la vague). Le lead ne ferme l'éditeur qu'après accord du propriétaire, verrou pris.
- **Vérification en jeu** : `scripts/factory/pie_step.sh` (première étape `claim`) et `shot.sh`. Les étapes refusent toute partie que le lead n'a pas lancée lui-même.
- **Goulot connu** : l'éditeur unique (PIE, imports, builds) est partagé avec le propriétaire. Regrouper les builds (un seul par vague), réserver des créneaux sans partie (la nuit) aux sessions de test, et écrire les recettes répétitives en tests Automation qui tournent sans interface (`ue.ps1 test`).
- **Roadmap** : `python scripts/production/roadmap_page.py <out.html>` après chaque vague, republiée à la même adresse.

## 7. Conventions de chantier pour l'usine
Titre : `[Lx][Cellule] <résultat attendu>`. Note : `env=dev|rec|int|prod ; owner=<ressource> ; recette=<IDs> ; après T-NNN ; preuve=<niveau>`. Le chantier (`scripts/chantier.py`) reste la source de vérité ; aucun autre tableau.

## 8. Roster (2026-10-03)
**Agents (29)** — Direction : `ue-producer`, `ue-architect`, `ue-game-designer`, `ue-narrative-designer`. Code : `ue-gameplay-programmer`, `ue-boat-programmer`, `ue-network-engineer`, `ue-systems-programmer`, `ue-ai-programmer`, `ue-ui-developer`, `ue-physics-animator`, `ue-cosmetics-programmer`. Monde et art : `ue-level-designer`, `ue-environment-artist`, `ue-tech-artist`, `ue-blender-artist`, `ue-character-artist`, `ue-character-rigger`, `ue-audio-designer`, `ue-gis-cartographer`. Éditeur : `ue-editor-integrator`. Qualité et livraison : `ue-integration-lead`, `ue-qa-tester`, `ue-code-reviewer`, `ue-build-engineer`, `ue-performance-engineer`. Qualité technique : `ue-memory-engineer`, `ue-security-engineer`, `ue-reliability-engineer` (avec `ue-architect` et `ue-performance-engineer`).

**Skills (35)** — Usine : `/factory`, `/factory-gate`, `/chantier`, `/ue-sprint`. Pipelines : `/ue-feature`, `/character-pipeline`, `/ue-boat-physics`. Code : `/ue-new-class`, `/ue-build`, `/ue-test`, `/ue-review`, `/ue-bugfix`, `/ue-ai-behavior`, `/ue-ui-widget`. Éditeur et contenu : `/ue-editor`, `/ue-import`, `/ue-material`, `/ue-level-blockout`, `/ue-pcg-scatter`, `/blender-asset`, `/character-model`, `/character-rig`, `/character-physics`, `/character-skin`, `/map-geodata`, `/map-terrain`. Qualité et livraison : `/ue-playtest`, `/ue-profile`, `/ue-package`, `/ue-vcs`. Qualité technique : `/ue-architecture`, `/ue-optimize`, `/ue-memory-audit`, `/ue-security-review`, `/ue-error-handling` (normes dans `docs/conventions/`).
