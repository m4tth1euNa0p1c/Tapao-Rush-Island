# ADR-001 : Le cahier des charges Crownfall Expeditions devient le contrat produit

- **Statut** : acceptée (adoption) — écarts §3 à arbitrer par le propriétaire
- **Date** : 2026-10-03

## Contexte
Le propriétaire a fourni `docs/CAHIER_DES_CHARGES.md` (Crownfall Expeditions v1.2) comme cahier des charges officiel à suivre par toute l'équipe d'agents. Il a été écrit pour un « futur dépôt » (`Crownfall.uproject`, `Docs/`, `Source/Crownfall/`), alors que le projet existant est `monke_island` (UE 5.8, atoll réel de Raraka, personnage 3e personne `BP_Islander`). Le cahier lui-même impose : « Ne remplace pas un projet existant ».

## Décision
1. `docs/CAHIER_DES_CHARGES.md` est la **référence produit** : boucle de jeu, règles, valeurs Standard, lots L0–L8 + E1, recette (§12, §23), organisation des agents (§13), définition de fini (§15). En cas de conflit entre une spec `docs/features/`, une habitude de l'équipe et le cahier, le cahier prime — sauf écart listé en §3 ci-dessous, tant qu'il n'est pas tranché.
2. On **garde le projet existant** et on transpose l'arborescence :

| Cahier | monke_island |
|---|---|
| `Crownfall.uproject` | `monke_island/monke_island.uproject` (UE 5.8, version exacte notée dans `docs/production/journal.md`) |
| `Docs/` | `docs/` (`Docs/CAHIER_DES_CHARGES.md` → `docs/CAHIER_DES_CHARGES.md`) |
| `Source/Crownfall/` + modules Player, Interaction, Items, Boats, Expedition, World, Economy, AI, UI, Save | module unique `Source/monke_island/`, un sous-dossier par domaine (`Expedition/`, `Items/`, `Boats/`…) |
| `Content/Crownfall/` | `/Game/MonkeIsland/` (convention `docs/conventions/asset-naming.md`) |
| `Tools/` | `scripts/` |
| Préfixe `BP_Crownfall*` | conservé tel quel pour les classes nommées par le cahier (§21) |

3. Les **statuts** du cahier (§13) sont mappés sur ceux du chantier, sans changer `scripts/chantier.py` :

| Cahier | Chantier | Condition |
|---|---|---|
| À faire | `a-faire` | |
| En cours | `en-cours` | |
| À vérifier | `review` | code écrit / compilé, recette pas encore passée |
| Validé | `fait` | **uniquement** avec preuve : build, test nommé, capture PIE, rapport de recette |
| Bloqué | `bloque` | |

   Toute note de fin indique le niveau de preuve : `compilé` / `testé éditeur` / `testé build` / `testé multi-machines`.

4. L'existant est **réutilisé** quand il couvre une exigence : `UIslandWaterSubsystem` (niveau d'eau global) → base de `BP_TideManager` (§6.1) ; `UIslandMovementComponent` (nage, sprint) → déplacement et endurance (§4) ; pipeline carto Raraka → référence visuelle et données d'eau/bathymétrie pour le requin (§8).

## 3. Écarts à arbitrer par le propriétaire
| # | Sujet | Cahier | Existant | Défaut appliqué en attendant |
|---|---|---|---|---|
| E-01 | Caméra | 1re personne, bras locaux, corps visible des autres | `BP_Islander` 3e personne | Suivre le cahier (1re personne) pour le personnage d'expédition ; `BP_Islander` reste jouable dans `Lvl_Raraka` |
| E-02 | Monde | Archipel **fictif**, îles faites à la main | Atoll **réel** de Raraka à l'échelle 1:1 (≈ 400 km²) | **Tranché le 2026-10-03 par ADR-004** : fragments réels de Raraka séparés par la mer, une map par expédition (`L_Frag_Raraka_NN`), contenu fictif |
| E-03 | Fiction | Couronne, colons, drapeau britannique, factions insulaires | Village réel de Motutapu | Aucun lieu, village ou habitant réel de Raraka dans la fiction Crownfall ; factions insulaires fictives |
| E-04 | Nom | Crownfall Expeditions (provisoire) | monke_island | Nom du jeu affiché = Crownfall Expeditions ; identifiants techniques monke_island inchangés |
| E-05 | Réseau | Listen server 2–4 joueurs, serveur autoritaire dès L1 | Code solo | Réplication conçue dès L1 (pas de rattrapage après coup) |
| E-06 | Jour/nuit | Crépuscule à 120 s, nuit à 240 s (§5) | `DA_DayNightRules` accéléré à la demande de l'utilisateur (2026-10-03) : crépuscule 50 s, nuit 75 s, boucle 3 min | Garder les valeurs du cahier en défaut C++ ; régler le DataAsset au playtest (T-043) |
| E-07 | Horloge | 120 s avant la submersion (Standard, §6.1) ; l'eau s'arrête à la fin de la submersion | `DA_ExpeditionRules` : 300 s initiales, demande du propriétaire du 2026-10-03 ; après la submersion la mer continue de monter (`PostSubmersionRiseCmPerSecond` 10 cm/s, plafond +15 m) jusqu'à la noyade des nageurs, la barque flotte | Défaut C++ inchangé à 120 s ; valeur du DataAsset à revoir au playtest (T-101) |

## Conséquences
- Sprints et features sont rattachés à un **lot** (L0…L8, E1) et citent les IDs de recette (§12 n°1–21, UI01–ED04, NET01) qu'ils couvrent.
- Les lots s'enchaînent dans l'ordre des dépendances : L0 → L1 → L2 → L3 → L4, puis L5–L6 ; pas de L7/L8/E1 avant validation du noyau.
- Les tâches carto/décor Raraka en cours (T-012 à T-019) ne sont plus prioritaires ; elles restent au backlog en P2.
- Décisions réversibles courantes : prises par les agents et consignées en ADR (`docs/adr/ADR-NNN-*.md`). Arbitrage du propriétaire seulement pour changement de vision, coût externe ou blocage sans repli (§14).
