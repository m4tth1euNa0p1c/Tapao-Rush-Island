---
name: ue-architecture
description: Architecture du code monke_island selon docs/conventions/architecture.md — `doc` crée ou met à jour docs/architecture.md (vues C4, graphe des dépendances entre dossiers, flux du §22, propriétaires) ; `review [dossier]` vérifie les contrôles ARC-01 à ARC-10 et produit un rapport d'audit ; `adr <sujet>` rédige une décision d'architecture. À utiliser avant une feature structurante, en fin de lot, ou pour « l'architecture tient ? ».
argument-hint: "doc | review [dossier] | adr <sujet>"
---

# Architecture

Mode : `$ARGUMENTS`. Par défaut : `review` sur `monke_island/Source/monke_island/`.
Norme : `docs/conventions/architecture.md`. Agent : `ue-architect`.

## `doc` : document d'architecture (cahier §13)
1. Inventorier le code :
   - dossiers de `Source/monke_island/` ;
   - classes principales (`UCLASS`, `USTRUCT`, `UINTERFACE`) ;
   - subsystems et composants ;
   - modules du `Build.cs` et plugins du `.uproject`.
2. Construire le graphe des dépendances entre dossiers à partir des `#include "<Dossier>/…"`, puis signaler les cycles et les dépendances qui remontent les couches.
3. Écrire ou mettre à jour `docs/architecture.md` :

   | Section | Contenu |
   |---------|---------|
   | 1. Contexte (C4 niveau 1) | joueurs, hôte, clients, fichiers locaux, services externes (L7) |
   | 2. Conteneurs (C4 niveau 2) | processus hôte (serveur et client), processus client, sauvegardes `SG_*`, projets d'île |
   | 3. Composants (C4 niveau 3) | un tableau par domaine : classes, rôle, état répliqué, propriétaire (cellule) |
   | 4. Dépendances | graphe Mermaid (`flowchart`) entre dossiers, couches |
   | 5. Flux de référence | les flux du §22 en séquences Mermaid, avec ce que le serveur valide à chaque étape |
   | 6. Décisions | liste des ADR et de leur effet |
   | 7. Écarts | ce qui viole la norme, avec l'ADR ou la tâche corrective |

4. N'inventer aucune classe : ce qui n'existe pas encore est marqué « prévu (catalogue §21) ».

## `review [dossier]` : revue d'architecture
1. Pour chaque contrôle ARC-01 à ARC-10 de la norme, chercher les cas (`Grep` : includes croisés, `GetAllActorsOfClass`, `bCanEverTick = true`, `static` non const, valeurs en dur, `LoadSynchronous`, logique dans `UI/`…), puis **confirmer chaque cas en relisant le code**.
2. Vérifier que chaque règle du cahier implémentée a son test `monke_island.<Domaine>.*` (ARC-09).
3. Rédiger le rapport `docs/audits/<date>-architecture-<sujet>.md` (modèle `docs/templates/audit-report.md`), avec un verdict et des tâches correctives au chantier pour chaque CRITIQUE ou MAJEUR.
4. Si l'architecture a changé, enchaîner sur `doc`.

## `adr <sujet>` : décision d'architecture
1. Prendre le prochain numéro libre dans `docs/adr/`, et partir du modèle `docs/templates/adr.md`.
2. Contenu :
   - contexte ;
   - options comparées : au moins 2, chacune évaluée sur les attributs ISO/IEC 25010 de la norme §1 ;
   - décision et conséquences ;
   - normes concernées.
3. Statut : `proposée`, si elle touche à la vision ou à un coût externe (arbitrage de l'utilisateur, cahier §14), sinon `acceptée`. Mettre à jour `docs/architecture.md`, section 6.

## Règles
- Lecture et rédaction seulement : ce skill ne modifie pas le code. Les corrections passent par des tâches.
- Chaque constat cite le fichier et la ligne, ainsi que l'ID du contrôle.
