# Modèle de consignes d'un agent de vague

À copier dans le prompt d'un agent lancé pendant que l'éditeur est ouvert et que le propriétaire joue. Remplacer les
`<…>`. Ce modèle reprend ce qui a marché pendant les vagues du 2026-10-11 : agents en parallèle, intégration par le lead.

```
Tâche chantier **T-<NNN>** (lot L<n>) — tu es <rôle> ; le lead (session principale) intègre, fait le build complet
et vérifie en PIE. Passe T-<NNN> `en-cours` au début et `review` à la fin avec une note honnête
(`PYTHONIOENCODING=utf-8 python scripts/chantier.py set T-<NNN> review --note "…"` depuis la racine du dépôt ;
niveau de preuve réel : tu t'arrêtes à « compilé »). Notes en français, code en anglais, style Epic, valeurs en
DataAsset, serveur autoritaire. Lis CLAUDE.md et docs/conventions/*.md.

## Demande du propriétaire (date), mot pour mot
« … »

## Ce qu'il faut obtenir
<résultat attendu, cadrage décidé par le lead, valeurs de départ, ce qui est hors périmètre>

## Contrat avec les autres agents de la vague
<noms de fichiers, chemins d'export, noms de gestes / sockets / événements, API ajoutées par le lead>

## Contraintes impératives (éditeur partagé)
- L'éditeur Unreal est ouvert et le propriétaire y joue : pas de MCP unreal, aucune PIE, ne ferme pas l'éditeur, pas
  de verrou `scripts/editor_lock.py`, pas de Live Coding.
- Compilation fichier par fichier sans édition de liens : `bash scripts/factory/build_editor.sh --single <fichier.cpp>`.
  Liste les headers réfléchis modifiés (le lead fera le build complet).
- Fichiers autorisés : <dossiers>. Interdits : <dossiers des autres agents>. Un changement indispensable ailleurs :
  décris-le dans ton rapport sans le faire.
- Ce qui se fait dans l'éditeur (imports, DataAssets, niveaux) : un script `scripts/editor/<nom>_ue.py` (docstring,
  idempotent, rapport JSON dans `monke_island/Saved/T<NNN>/`), sans l'exécuter : le lead l'ajoute à `ACTIONS` de
  `scripts/editor/lead_session_t186.py` et le lance.
- Blender en arrière-plan seulement (`blender -b`), fermé après chaque tâche ; OneDrive\maquette-blender en lecture seule.
- Malwarebytes peut verrouiller un fichier juste écrit : réessaie. Ne committe rien.

## Rapport final (pour le lead)
Fichiers, headers réfléchis, compilation OK ou non, script éditeur et prérequis, étapes de vérification PIE précises
(solo, puis hôte + client), ce qui reste incertain.
```

## Intégration par le lead
1. Quand les agents ont tous rendu : demander au propriétaire de fermer l'éditeur (~10 min), prendre le verrou.
2. `MONKE_LOCK_OWNER=<session> bash scripts/factory/integrate_wave.sh "monke_island" <action1> <action2> …` :
   build complet, suite de tests, relance de l'éditeur avec les sessions lead et PIE, actions éditeur de la vague.
3. Vérifications en jeu : `StartPIE` (MCP) puis `bash scripts/factory/pie_step.sh "claim; …"`, captures avec
   `bash scripts/factory/shot.sh "<étapes>" <nom>` (fichiers dans `build/factory/shots/`).
4. Tableau : `scripts/chantier.py set …` avec le niveau de preuve ; roadmap : `python scripts/production/roadmap_page.py <out.html>`.
