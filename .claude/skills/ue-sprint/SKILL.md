---
name: ue-sprint
description: Planifie un sprint de l'équipe monke_island à partir du cahier des charges (lots L0–L8, recette), du backlog et du tableau de chantier — objectifs, tâches assignées aux agents, dépendances, critères de fin — puis crée les tâches dans le chantier. À utiliser pour « planifie la suite », « prochain sprint », « organise le travail ».
argument-hint: "[objectif du sprint]"
---

# Sprint

Objectif proposé : $ARGUMENTS

Le contrat est `docs/CAHIER_DES_CHARGES.md` (adoption et écarts : `docs/adr/ADR-001-cahier-des-charges-crownfall.md`). Un sprint fait avancer **un lot** du cahier ; il ne saute pas de lot.

1. État : `python scripts/chantier.py --env`, `docs/production/backlog.md`, specs `docs/features/*.md` (statuts), bugs ouverts, ADR.
2. Lot visé : le premier lot non validé dans l'ordre L0 → L1 → L2 → L3 → L4 → L5 → L6 (puis L7, E1, L8+ seulement si le noyau L1–L4 est validé). Relis dans le cahier : la ligne du lot (§11), ses ajouts (§23), la recette qu'il doit passer (§12 n°1–21, UI01–UI12, ED01–ED04, NET01) et les contrats concernés (§9, §21, §22).
3. Lance l'agent `ue-producer` avec l'objectif, le lot, l'état et la durée (défaut : 1 semaine) : il propose le sprint.
4. Montre le plan à l'utilisateur (lot, objectif, 5 à 12 tâches, agent, dépendances, ce qui tourne en parallèle, recette visée) et attends son accord. Si un écart d'ADR-001 encore ouvert (E-01…E-05) touche le sprint, pose la question avec le plan.
5. Une fois validé :
   - crée `docs/production/sprints/sprint-NN.md` depuis `docs/templates/sprint.md` ;
   - crée chaque tâche : `python scripts/chantier.py add "[Lx][<Cellule>] <titre>" --agent <agent> --feature <slug> --note "env=dev ; fini = <critère> ; recette=<IDs> ; après T-NNN ; owner=<ressource exclusive ou ->"`.
6. Résume : numéro du sprint, lot, IDs créés, première vague ; le lancement se fait par `/factory wave sprint-NN`, le passage d'environnement par `/factory-gate`.

Organisation (cellules, environnements DEV → REC → INT → PROD, ressources exclusives, taille des vagues) : `.claude/factory/FACTORY.md`.

## Fiche de tâche (cahier §13)
Chaque tâche porte : ID chantier, objectif, lot, dépendances, fichiers/assets autorisés (propriétaire unique pour une map ou un Blueprint), contrat public (classe, interface, événement du §21), critères de recette, preuve attendue. Le sprint-NN.md contient la fiche complète ; la note du chantier en garde le résumé.

## Règles
- Une tâche = un agent = un résultat vérifiable.
- Validé = `fait` avec preuve ; code écrit ou compilé sans recette = `review` (« À vérifier »).
- La fin de sprint indique, pour chaque tâche, le niveau de preuve : compilé / testé éditeur / testé build / testé multi-machines.
- Les dépendances vont dans la note (`après T-004`) et l'ordre de lancement.
- Pas plus de 3 tâches `en-cours` par agent. Deux agents ne touchent jamais la même map ou le même Blueprint en même temps.
- Le lot est validé seulement quand sa recette passe ; noter le résultat dans `docs/production/journal.md`.
