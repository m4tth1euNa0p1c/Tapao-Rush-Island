# Backlog monke_island

Priorités : **P0** en cours / indispensable · **P1** prochain · **P2** plus tard.
**Contrat produit : `docs/CAHIER_DES_CHARGES.md`** (Crownfall Expeditions). Le backlog suit ses lots, dans l'ordre ; un lot est validé quand sa recette passe (§11, §12, §23). Adoption et écarts : `docs/adr/ADR-001-cahier-des-charges-crownfall.md`.
Chaque épic passe par `/ue-feature` (spec → design → implémentation → tests → review) ; le suivi au jour le jour est dans le chantier (`/chantier`).

## P0 — Prototype central (L0 → L4)
| Lot | Livrable | Critère de passage |
|-----|----------|--------------------|
| **L0** | Version moteur notée, architecture, map grise, commandes build, menu minimal + cartes de démarrage (L_MainMenu, L_GalleonHub, L_Island_Prototype) | Le projet ouvre et compile ; dépendances et limites documentées |
| **L1** | Déplacement 1re personne, interaction (200 cm, ligne de vue, validation serveur), prise/dépôt ; réplication dès ici | Deux joueurs prennent des objets distincts ; même objet jamais dupliqué (recette n°2, n°3) |
| **L2** | Barque, sièges, arrimage, capacité 12 / 150 kg, repositionnement entre 3 mouillages | Stock partagé sans duplication ; déplacement annoncé, cargaison conservée (n°5, n°6, n°19) |
| **L3** | Ennemi mêlée, 3 zones pacifiables (drapeau + sermon 20 s), 120 s + 120 s/zone, submersion 60 s, extraction | Bonus unique partagé, eau qui ne baisse jamais, boucle solo puis hôte + client (n°7, n°13–18) |
| **L4** | Hub, quotas, taxe 35 %, boutique, sauvegarde ; Nouvelle partie / Charger / Continuer | Exemple économique exact, relance conserve la progression, bilan non rejoué (n°1, n°9, n°20, UI01–UI05, UI10) |

### Fragments des Tuamotu (ADR-004, spec `docs/features/fragments-tuamotu.md`, T-081 → T-092)
Chaque expédition se joue sur un morceau réel de Raraka, séparé du reste par la mer (hoa, lagon), avec 3 villages, un marae et les silhouettes de l'atoll à l'horizon. L3 : pipeline + `L_Frag_Raraka_01` (remplace `L_Island_Prototype`) · L4 : choix au hub et voyage · L8 : fragments 2 à N.

### Partie en manches autour du galion (cahier v1.3 §24, ADR-005 acceptée le 2026-10-04)
Parcours : menu → galion (lobby) → île (barque, palan) → départ à la barre → zone sûre (vente, quota) → manche suivante ou défaite. L3 (complément) : galion au mouillage, palan, barre, départ automatique · L4 : `L_Galleon`, manches, vente, registre, quota éliminatoire, boutique, sauvegarde de partie. Tâches du chantier (ADR-005, ordre de mise en œuvre) :
1. Règles et calculs purs (`DA_RunRules`, `FRunCalculator`, tests RUN14) — ue-systems-programmer
2. Galion en blockout (`BP_Galleon` : pont, volumes, échelle, amarrage) ; maquette au 2e PC — ue-boat-programmer, ue-level-designer
3. `UGalleonStartSubsystem` à la place de BoatStart : galion au mouillage, barque au flanc, joueurs sur le pont — ue-boat-programmer
4. Barre, vote de départ, palan, abandon, départ automatique en fin de submersion — ue-gameplay-programmer (revue réseau)
5. `L_Galleon`, `ARunGameMode`, `URunSubsystem`, voyages, manifeste de cale ; suppression de `L_Boot` — ue-systems-programmer
6. Balance, registre, signature, défaite, contrat suivant, écrans TAPAO — ue-gameplay-programmer, ue-ui-developer
7. Sauvegarde de partie, Continuer, historique — ue-systems-programmer
8. Recette RUN01–RUN14 ; mise en scène provisoire (voiles, traversée, frégate) — ue-qa-tester, ue-tech-artist

## P1 — V1 coop (L5–L6)
- **L5** : 4 joueurs, endurance, nuit, lanterne, requin ; HUD, paramètres, pause (n°10, n°11, n°21, UI06–UI09, UI11–UI12)
- **L6** : build Windows V1 hors éditeur, intégration des personnages du propriétaire (§10), navigation et packaging vérifiés

## P2 — Après le noyau
- **L7** : sessions internet privées et VoIP (NET01)
- **E1** : éditeur d'îles intégré (ED01–ED04), après L6
- **L8+** : îles variées, événements, factions, catastrophes, upgrades avancés — chacun proposé avec recette et coût

## P2 — Map de Raraka (référence visuelle et technique, cf. écart E-02 d'ADR-001)
- Terrain réel de l'atoll : masques officiels (trait de côte + atlas des récifs IRD), bathymétrie, meshes Nanite, import UE (T-003 → T-006)
- Matériau terrain atoll et eau du lagon réaliste (T-007, T-008)
- Calque des parcelles cadastrales (T-009)

## P2 — Mécaniques futures (demandées par l'utilisateur le 2026-10-02 ; montée des eaux et requin désormais couverts par L3 et L5)
| Épic | Idée | Contraintes dès maintenant |
|------|------|-----------------------------|
| **Tempête / cyclone** | événement météo dynamique : vent, pluie, houle, ciel, visibilité | ciel, nuages et vent pilotables par paramètres (pas figés dans le niveau) |
| **Montée des eaux** | submersion progressive ou surcote cyclonique ; les motu ne culminent qu'à 2–4 m | niveau d'eau = paramètre global (plan d'eau à Z = 0 réglable) ; hauteurs réelles du terrain conservées |
| **Attaques de requins** | danger quand on nage : lagon, passe, océan (comportements différents selon la zone) | zones lagon / passe / océan et bathymétrie du pipeline carto réutilisables par l'IA |
