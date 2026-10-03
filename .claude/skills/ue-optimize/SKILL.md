---
name: ue-optimize
description: Passe d'optimisation mesurée sur une map ou un système monke_island — référence chiffrée, goulot (jeu, rendu, GPU, mémoire, réseau, chargement), un changement à la fois, nouvelle mesure et rapport. À utiliser pour « optimise », « c'est lent », « ça saccade », « on tient le budget ? », ou avant la gate PROD des lots L5 et L6.
argument-hint: "[/Game/chemin/map] [frame|gpu|game|memoire|reseau|chargement]"
---

# Optimisation mesurée

Cible : `$ARGUMENTS`. Par défaut, la map principale du lot en cours, cible `frame`.
Norme : `docs/conventions/performance.md`. Agent : `ue-performance-engineer`, avec `ue-memory-engineer` pour la mémoire et `ue-network-engineer` pour le réseau.

1. **Cadrer** :
   - scénario de mesure : celui du cahier §12 si la map le permet, sinon un point de vue fixe noté avec ses coordonnées ;
   - configuration : `-game` non cooké pour comparer, build packagé pour les chiffres de référence ;
   - budget visé, pris dans la norme §1.
2. **Référence** : 3 captures avec `/ue-profile <map> <frames>`, médianes consignées (image, jeu, rendu, GPU ; p95, p99, 1 % bas).
   - Mémoire : `/ue-memory-audit measure`.
   - Réseau : PIE à 2 joueurs ou plus avec `-trace=default,net -NetTrace=1`.
3. **Goulot** : le plus grand temps entre jeu, rendu, GPU et RHI. Le détailler :
   - côté CPU : Unreal Insights (`-trace=default`) ;
   - côté GPU : `ProfileGPU`, `stat gpu`, `stat Nanite` ;
   - côté streaming : `stat Streaming`.

   Lister les 3 premiers postes de coût, chiffres à l'appui.
4. **Boucle**, pour chaque hypothèse, de la plus rentable à la moins rentable :
   1. un seul changement, pris dans le catalogue de leviers (norme §3) ;
   2. nouvelle mesure, 3 captures ;
   3. on garde si le gain dépasse le bruit entre les captures, on annule sinon ;
   4. on note le résultat, même négatif.
5. **Non-régression** : `/ue-test`. Vérification visuelle (capture avant et après) si le rendu change : un gain qui dégrade l'image se négocie avec l'utilisateur.
6. **Rapport** dans `docs/perf/<date>-<sujet>.md` (modèle `docs/templates/perf-report.md`) :
   - chiffres avant et après ;
   - changements gardés et changements annulés ;
   - budget tenu ou non, et écart restant.

## Règles
- Pas d'optimisation sans chiffre avant et après. Pas de « devrait être plus rapide ».
- **Éditeur partagé** (`FACTORY.md` §5) :
  - annoncer toute opération de plus de 30 s (génération PCG, rebuild) à l'utilisateur et aux autres sessions ;
  - découper par acteur ou par secteur.
- **Hors périmètre** : un changement de code ou d'asset qui appartient à une autre cellule passe par une tâche au chantier, sauf accord de son responsable.
