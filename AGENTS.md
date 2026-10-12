# Codex — monke_island / Crownfall Expeditions

Échanges en français, code et identifiants en anglais. Lire `CLAUDE.md` pour le
contexte technique ; le contrat produit est `docs/CAHIER_DES_CHARGES.md` (dont §24
pour la partie en manches), complété par les ADR acceptées. Les audits décrivent
un instantané : vérifier leurs constats dans le code courant.

## Repères

- Projet UE 5.8 : `monke_island/monke_island.uproject`.
- Module C++ : `monke_island/Source/monke_island/<Domaine>/`.
- Assets propres au jeu : `/Game/MonkeIsland/`. Préserver le contenu du template.
- Tâches et preuves : `docs/production/board.md` et `journal.md`, gérés uniquement
  avec `scripts/chantier.py`. Pas de deuxième backlog Codex.
- Organisation existante : `.claude/factory/FACTORY.md`. Lire les conventions du
  domaine dans `docs/conventions/` et la fiche `docs/features/` concernée.
- Guide des outils et analyse initiale : `docs/production/codex.md`.

## Travailler sur ce dépôt partagé

Avant d'écrire, consulter `python scripts/monke.py status` et la tâche concernée
(`python scripts/monke.py task T-NNN`). Préserver les modifications des autres
sessions. Pour le C++, utiliser les worktrees de `scripts/worktree.py` ; les
petits ajouts docs/outillage sur des fichiers disjoints peuvent rester dans l'arbre
principal. Les branches existantes peuvent être anciennes : comparer leur base.

Une instance possède une tâche et un périmètre explicite. Pour une délégation,
transmettre ID, racine/worktree, fichiers autorisés, critères et ressources
exclusives. Utiliser les profils `.codex/agents/*.toml` disponibles ; si le moteur
d'agents ne permet pas de sélectionner ces profils, lire le fichier pertinent et
transmettre ses instructions au sous-agent. Les profils sont persistants, pas des
processus tournant en permanence. Le parent seul coordonne les écritures du
chantier lorsque plusieurs agents travaillent simultanément.

Les rôles Codex se rattachent aux rôles existants du chantier (table dans le guide).
N'augmenter ni les droits ni la concurrence de la session pour charger un profil.
Ne pas appliquer les paramètres de modèle Claude aux agents Codex.

## Verrou de l'éditeur partagé (obligatoire)

Quatre sessions Claude et Codex partagent un seul éditeur Unreal. Avant de **fermer l'éditeur, compiler
l'arbre principal, lancer la suite headless, relancer l'éditeur, faire un gros import / PCG / PIE de test** :

```powershell
python scripts/editor_lock.py status                                   # libre ?
python scripts/editor_lock.py acquire --owner codex --reason "build T-NNN" --minutes 15
python scripts/editor_lock.py close-editor --owner codex               # fermeture propre, verrou exigé
# ... build / tests / relance (scripts/ue.ps1 vérifie le verrou ; MONKE_LOCK_OWNER=codex) ...
python scripts/editor_lock.py release --owner codex
```

Si le verrou est tenu par une autre session, **attendre** : ne jamais fermer l'éditeur ni arrêter un PIE
qui n'est pas le sien, ne jamais utiliser `--force` sans accord de l'utilisateur. Les sessions Claude ne
reçoivent pas les messages de Codex : annoncer chaque fenêtre dans la note de sa tâche
(`python scripts/chantier.py set T-NNN en-cours --note "fenêtre éditeur …"`) en plus du verrou.
Le build d'un worktree en cible jeu (`scripts/worktree.py build T-NNN`) ne demande pas le verrou.

## Ressources Unreal

L'éditeur, le build principal et chaque `.umap`/Blueprint ont un propriétaire
unique. Vérifier les notes `owner=` du chantier avant usage. Ne pas fermer
l'éditeur ou lancer un second packaging détenu par une autre tâche.
Le MCP Unreal est à `http://127.0.0.1:8000/mcp` : sa configuration ne prouve pas
qu'il fonctionne. Avant appel, découvrir les vrais outils/toolsets disponibles ;
ne pas inventer de méthode MCP. Lire `.claude/skills/ue-editor/SKILL.md` pour les
particularités locales, en adaptant les noms d'outils à la session.

Les maquettes du second PC arrivent dans `C:/Users/matth/OneDrive/maquette-blender` :
source en lecture seule, attendre la fin de synchronisation avant import.
Pas d'import ou de génération massive comme effet secondaire d'un diagnostic.

## Vérifications

```powershell
python scripts/monke.py doctor
python scripts/monke.py status --json
python scripts/monke.py task T-113
python -m unittest discover -s scripts/tests -p "test_monke.py" -v
```

Pour le jeu, réutiliser `scripts/ue.ps1` et `scripts/worktree.py`. Live Coding
convient aux corps de fonctions ; les changements de headers réfléchis demandent
une compilation éditeur, après coordination. `worktree.py build T-NNN` valide la
cible jeu sans remplacer la validation de la cible éditeur.

Après une exécution Automation, contrôler le `index.json` avec
`python scripts/monke.py report --filter monke_island --min-tests <nombre-attendu>`.
Pour rattacher le fichier au run, fournir aussi `--since <date-ISO-avec-fuseau>`
enregistrée avant le lancement. Le minimum dépend de la suite demandée, pas du
nombre historique indiqué dans un document. Archiver les preuves nécessaires à
la tâche : un rapport existant n'établit pas que HEAD a été testé.

Garder les niveaux de preuve séparés : code écrit, compilé, testé éditeur, testé
build, testé multi-machines. Une spec de calcul ne valide pas un RPC ou un voyage.
Respecter DEV → REC → INT → PROD ; ne pas passer une tâche gameplay à `fait` sur
la seule présence de code ou d'un ancien rapport vert.
