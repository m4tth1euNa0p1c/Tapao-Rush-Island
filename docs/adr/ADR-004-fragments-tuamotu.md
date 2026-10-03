# ADR-004 : Les îles d'expédition sont des fragments réels de Raraka, séparés par la mer

- **Statut** : acceptée. Choix du propriétaire du 2026-10-03, qui tranche l'écart E-02 d'ADR-001.
- **Spec** : `docs/features/fragments-tuamotu.md`.

## Contexte
- `Lvl_Raraka` reprend tout l'atoll à l'échelle 1:1 : 29 × 26 km, 352 motu, 51 tuiles, plus de 800 000 instances de végétation. C'est trop gros pour une expédition de 2 à 8 minutes (cahier §6.1). Le joueur s'y noie, et la machine aussi.
- Le cahier (§2) demande un archipel fictif d'îles faites à la main. ADR-001 (E-02) prévoyait en attendant une île fictive, `L_Island_Prototype`.
- Le propriétaire veut au contraire des **morceaux des Tuamotu** : chacun contient un village, un marae et d'autres surfaces (cocoteraie, fourré, plage, platier, hoa, lagon). Ils ne doivent **pas être coupés salement** : chaque morceau doit être séparé du reste par la mer.

## Décision
1. **Un fragment par map.** Une expédition se joue sur un seul fragment, dans sa propre map `L_Frag_<Ile>_<NN>`. L'équipe y arrive depuis le hub galion par `ServerTravel`. Il n'y a pas de streaming de plusieurs fragments dans une même partie.
2. **Formes réelles de Raraka.** On garde le trait de côte, l'atlas des récifs et la bathymétrie estimée par le pipeline carto existant, mais le contenu est fictif : villages japonais, marae, tiki. Les autres atolls viendront plus tard avec le même pipeline (`/map-geodata`).
3. **Coupures par la mer uniquement.** Une frontière de fragment ne traverse jamais la terre. Elle passe :
   - soit par un **hoa naturel** ou par le lagon, là où il y a déjà de l'eau entre deux motu ;
   - soit, pour les grands motu, par un **hoa synthétique** creusé au point le plus étroit, avec une plage de chaque côté et une pente d'au plus 7° (même règle que T-068).

   Au-delà du fragment, le fond descend en pente douce vers le lagon profond ou l'océan. On ne voit ni mur ni falaise.
4. **Horizon.** Le reste de l'anneau de l'atoll reste visible au loin (au-delà de 1,5 km), sous forme de silhouettes simplifiées : bande de sable et canopée, sans collision ni gameplay.
5. **Motutapu exclu.** Aucun fragment ne contient le village réel ni ses abords. Cela prolonge E-03 d'ADR-001.
6. **Catalogue figé.** Les fragments sont choisis hors ligne par un script, revus, puis **gelés** avec un identifiant stable (`FRG_Raraka_NN`). Les variantes viennent du placement (seed), pas d'une génération de terrain au lancement. C'est conforme au cahier : îles faites à la main et variantes de placement avant toute génération procédurale.
7. `Lvl_Raraka` reste la **map source** : référence, réglages, captures. On n'y joue plus d'expédition.

## Conséquences
- `L_Island_Prototype` (L0/L3) devient le premier fragment : `L_Frag_Raraka_01`.
- Les systèmes communs (eau et marée, ciel et jour/nuit, tempête, horloge d'expédition, directeur d'événements) vivent dans une Level Instance partagée, `LI_ExpeditionCore`. Chaque fragment ne porte que son terrain, sa végétation et son contenu.
- Chaque fragment est décrit par un DataAsset `UExpeditionIslandDefinition` (map en référence souple, nombre de zones, mouillages, crédits des données). Le hub les choisit dans un catalogue.
- Un fragment hérite des tâches carto déjà faites : terrain, masques, cocotiers, végétation, villages et marae sont filtrés et recadrés, pas refaits.
- **Lots.** Le pipeline et le premier fragment jouable relèvent de L3 (première île, trois zones). La sélection au hub relève de L4. Les fragments 2 à N sont des « îles variées » (L8 du cahier) : on les produit après la validation du noyau, même si le pipeline les rend peu coûteux.
- **Données.** Te Fenua / DAF, Licence Ouverte Etalab 2.0, et atlas IRD : sources citées dans les crédits du jeu. L'imagerie Maxar n'est jamais embarquée.
