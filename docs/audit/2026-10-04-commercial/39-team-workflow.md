# 39 — Workflow multi-sessions
Domaine : D · Auditeur : lead technique

## Verdict
Le workflow produit vite (88 commits en 22 h) mais livre mal : trois fonctions cœur dorment sur des branches, le tronc n'est pas l'arbre testé, et tout tient sur un disque à 98 %, sans remote. Risque commercial : perdre le projet, ou en découvrir trop tard l'état réel, le propriétaire étant seul testeur.

## Constats

- **[Bloquant commercial] Aucune sauvegarde, disque plein à 98 %**
  - Constat : dépôt local sans remote ni bundle (`.git/lfs` 3,7 Go) ; disque à 40 Go libres (−3 Go en 15 min : worktree T-118) ; `.worktrees` : 18 arbres, 45 Go, dont 14 fusionnés jamais supprimés (≈ 34 Go).
  - Preuve : `git remote -v` vide ; `df /c` ; `du .worktrees`.
  - Recommandation : bundle et copie LFS nocturnes vers un 2e disque ; `worktree.py gc` en gate INT ; remote privé ensuite.
  - Effort : S (copie, gc) / M (remote)

- **[Bloquant commercial] Intégration sans propriétaire : sermon, arrimage, sauvegarde (T-054/055/057) jamais fusionnés**
  - Constat : 6 910 lignes, ≈ 69 specs jamais lancées, ≈ 76 commits de retard. Fusion à blanc propre (0 conflit) : la cause est le processus (les 14 autres branches : fusion 7 min en médiane après leur dernier commit). Effets : 03, 11, 35.
  - Preuve : `git branch --no-merged main` ; `git merge-tree` (3 × exit 0).
  - Recommandation : une fenêtre INT aujourd'hui, via une branche `integration` (FACTORY.md:46, inexistante) ; `review` > 4 h sans fusion = `bloque` + cause.
  - Effort : S

- **[Majeur] Code non versionné, tronc jamais testé tel quel**
  - Constat : à 03:00, 232 fichiers modifiés et 34 non suivis, dont 27 fichiers C++ (≈ 2 700 lignes : Run, Foliage, Water/Wave*) ; Foliage dort depuis 20 h. Les 296 tests incluent `Run`, non suivi ; T-103 : « main seul ne compile pas ». 48 commits sur 70 se font dans l'arbre partagé, parfois faute d'accord du propriétaire.
  - Preuve : `git status` ; BRIEF.md (Run 9) ; journal.md:263 ; board.md T-102.
  - Recommandation : commit local ciblé permanent après build OK (push exclu), via `commit.py` (chemins explicites, `--only`, auteur = session) ; specs sur un worktree propre de HEAD avant fusion.
  - Effort : S

- **[Majeur] Éditeur partagé : 6 rapports de plantage en 23 h, sans exclusion mutuelle**
  - Constat : 4 violations d'accès, 1 ensure, 1 assert (commandlet) ; le dernier, 2 s après un Live Coding appliqué pendant un PIE (02:05). 34 commits de code sur 38 touchent un header : 11 % seulement relèvent du Live Coding. La règle « pas de Live Coding en PIE » n'existe que dans la mémoire locale, pas dans CLAUDE.md.
  - Preuve : `Saved/Crashes` (6 dossiers) ; journal.md:251 (PIE croisés).
  - Recommandation : `scripts/editor/lock.py` (bail : propriétaire, opération, TTL, `IsPIERunning`) avant PIE, tests, Live Coding ou fermeture ; deux fenêtres de build fixes par jour.
  - Effort : S

- **[Majeur] Map de 105 Mo : une version en 20 h, propriété non tenue**
  - Constat : `Lvl_Raraka.umap` n'a que 4 commits (105 Mo de LFS chacun), aucun entre 10-03 05:59 et 10-04 02:37 malgré T-064, T-075, T-095 ; gel annoncé de 30-60 s à la sauvegarde. 15 tâches la citent, aucune ne porte `owner=Lvl_Raraka` : `set --note` écrase la note, jeton compris (T-095 : journal seul).
  - Preuve : `git log -- Lvl_Raraka.umap` ; journal.md:250 ; chantier.py:125.
  - Recommandation : propriétaire en colonne dédiée ; L_Frag_* et Level Instances (T-089) avec External Actors ; commit de la carte en fin de session.
  - Effort : M

- **[Majeur] Gates sur le papier, goulot humain**
  - Constat : 74 tâches sur 118 en `review`, 22 `fait`, 0 `bloque` ; aucune tâche pour intégrateur, relecteur ou sécurité ; aucun jeton `revue-reseau=OK`. Les 31 specs sont `EditorContext` seul : éditeur fermé obligatoire (aucune n'utilise `GEditor`).
  - Preuve : board.md ; factory-gate/SKILL.md:22-24 ; grep `EAutomationTestFlags`.
  - Recommandation : `ClientContext` pour les specs de logique pure, sur la cible jeu des worktrees (supposé faisable) ; recette de 15 min tirée des notes « non testé » ; plafond de 15 `review`.
  - Effort : M

- **[Mineur] Pilotage fragile**
  - Constat : `chantier.py` réécrit `board.md` (58 Ko) sans verrou ni écriture atomique (0 divergence sur 298 lignes : théorique) ; un seul auteur Git.
  - Preuve : chantier.py:64-67 ; `git log --format=%an`.
  - Recommandation : verrou, auteur `monk-island-<id>`.
  - Effort : S

## Manques
- Remote privé et CI (build propre de HEAD, tags).
- Verrou outillé (éditeur, carte, Blueprints).
- Smoke PIE automatisé (hôte + client).

## Quick wins
1. `git bundle` et copie de `.git/lfs` sur un 2e disque.
2. Supprimer les 14 worktrees fusionnés : −34 Go.
3. Fusionner T-054/055/057 dans `integration`, compiler, lancer leurs ≈ 69 specs.
4. Commiter le code non suivi, par tâche.
5. CLAUDE.md : pas de Live Coding en PIE ; commits locaux ciblés autorisés.

## Questions qui challengent l'équipe
1. Qui est l'intégrateur ? Acceptez-vous un gel des nouvelles fonctions tant que `review` dépasse 15 tâches ?
2. Si C: meurt ce soir, que reste-t-il ? Quel disque ou remote payez-vous cette semaine ?
3. Un plantage toutes les 4 h : le 2e PC doit-il devenir banc de build et de test, plutôt que station Blender ?
