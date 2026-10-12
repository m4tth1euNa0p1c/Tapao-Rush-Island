# ADR-003 — Barque physique : écarts au cahier des charges

- **Statut** : proposé. Demande explicite du propriétaire (2026-10-03), à confirmer à la recette L2.
- **Contexte** : le propriétaire veut une barque qu'on mène soi-même :
  - flottaison, rames ;
  - poids des joueurs et capacité en kilos ;
  - naufrage par surcharge ;
  - échouage sur le sable avec une trace ;
  - noyade à court d'endurance.

  Le cahier (§4, §5, §6.4, §6.5) décrit une barque plus abstraite.
- Les valeurs vont toutes dans `DA_BoatRules` et dans le composant de mouvement. Rien n'est en dur. Spec : `docs/features/boat-rowing.md`.

| ID | Cahier | Demande | Décision proposée |
|----|--------|---------|-------------------|
| B-01 | §6.4 : déplacement scripté de 15 s entre 3 mouillages ; « navigation physique libre à rames = extension » | navigation libre à la rame, dès maintenant | **Adopter en parallèle.** La barque physique est le support. Le repositionnement entre mouillages (vote, 15 s, trajectoire) reste à faire : il pourra piloter la même barque (autopilote) sans la remplacer. |
| B-02 | §5 : capacité 12 emplacements / 150 kg ; joueurs sur des sièges séparés | 60 kg par joueur, 500 kg maximum, joueurs compris | **Adopter.** `MaxLoadKg = 500` et `PersonMassKg = 60`. Les 12 emplacements d'arrimage restent pour le comptage des quotas, sans limite de masse propre. À arbitrer : garder ou non une limite de 150 kg de cargaison arrimée en plus des 500 kg totaux. |
| B-03 | §12 n°5 : « barque pleine : refus clair, aucun objet perdu par l'interface » | la surcharge remplit la barque d'eau, qui coule et se perd | **Adopter, en distinguant deux cas.** L'arrimage par l'interface refusera toujours au-delà de la capacité (n°5 tient). La surcharge physique (objets jetés, joueurs montés) fait couler la barque. C'est un risque de jeu visible : eau qui monte, préavis, écopage sous la limite. |
| B-04 | §4 : nage qui coûte de l'endurance ; « nage lente, pas de mort instantanée » | à court d'endurance dans l'eau, on se noie | **Adopter.** La mort n'est pas instantanée : on s'enfonce, puis délai de 3 s, puis 12 PV/s, soit environ 11 s au total. On est sauvé si on regagne un appui. |
| B-05 | §6.5 : embarquer dans un siège libre, sans objet tenu | monter dans la barque, s'asseoir, prendre les rames ; on peut aussi rester debout | **Adopter.** Rester debout est possible mais **ne compte pas comme embarqué** pour l'extraction (§6.5 tient). F assoit le joueur ; le premier assis rame. |
| B-06 | aucune règle | traces de sable à l'échouage et à la remise à l'eau | **Adopter**, en effet cosmétique local, sans réplication. |
| B-07 | §5, §9 : serveur autoritaire, physique des objets libres répliquée | revue réseau T-268 (R-05) : barque sans mode de réplication choisi, houle locale, cargaison arrimée téléportée par le serveur seul | **Adopter (2026-10-11, T-295, `ue-network-engineer`), à confirmer en PIE.** Le serveur simule. Les clients suivent en `PredictiveInterpolation`, à 30 envois par seconde (`DA_BoatRules`). Chaque machine applique les mêmes forces entre deux envois, et la houle suit l'horloge du serveur. La cargaison arrimée est collée à la coque sur chaque machine (`UNetRestComponent`) et n'est plus envoyée. Repli : `PhysicsReplicationMode = Default`. Si la barque ne tient pas à 150 ms, le client la rend cinématique. |

**Conséquences.**
- La recette L2 ajoute des critères physiques (spec).
- La revue réseau (`ue-network-engineer`) porte sur la physique répliquée de la barque, sur les joueurs debout sur une base mobile et sur les RPC de rame.
- B-07 (T-295) : le mode et la cargaison collée sont `compilé` (2026-10-11). Ils ne sont pas testés en réseau. La signature attend l'essai n°4 de `docs/audit/revue-reseau-T268.md` : client rameur et hôte debout, puis l'inverse, à 100 puis 200 ms et 3 % de perte. On compte les corrections avec `p.NetShowCorrections 1` et le débit avec `stat net`.
- La nouvelle compétence de l'usine : rôle `ue-boat-programmer` et skill `/ue-boat-physics`.
